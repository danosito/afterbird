// Copyright 2026 The Afterbird Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/desktop_android/extension_installer.h"

#include <algorithm>
#include <array>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/extensions/desktop_android/desktop_android_extension_system.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/extension.h"
#include "extensions/common/file_util.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "third_party/zlib/google/zip.h"

namespace extensions {

namespace {

// `Cr24` in ASCII.
constexpr std::array<uint8_t, 4> kCrxMagic = {0x43, 0x72, 0x32, 0x34};

bool HasSuffix(const base::FilePath& path, base::FilePath::StringPieceType s) {
  return base::EndsWith(path.value(), s,
                        base::CompareCase::INSENSITIVE_ASCII);
}

// Returns true if `path` names a directory containing a manifest.json.
bool IsUnpackedExtensionDir(const base::FilePath& path) {
  return base::DirectoryExists(path) &&
         base::PathExists(path.AppendASCII("manifest.json"));
}

// Reads the first 4 bytes of `path` into `out`. Returns false if the file
// couldn't be opened or is too short.
bool ReadMagic(const base::FilePath& path, std::array<uint8_t, 4>* out) {
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    return false;
  }
  return file.ReadAndCheck(0, *out);
}

// Returns a per-installation unique dir under `parent` named
// "afterbird_install_<timestamp>.<pid>". Caller owns + cleans up.
base::FilePath MakeStagingDir(const base::FilePath& parent) {
  base::FilePath staging;
  base::CreateTemporaryDirInDir(parent, FILE_PATH_LITERAL("afterbird_install_"),
                                &staging);
  return staging;
}

// Reads a CRX3 header length from a file. CRX3 format:
//   bytes  0- 3: "Cr24"
//   bytes  4- 7: version (little-endian uint32 == 3)
//   bytes  8-11: header length in bytes (little-endian uint32)
//   bytes 12- :  signed header proto
//   bytes (12 + header_len)-: zip body
// Returns the offset at which the zip body begins, or -1 on failure.
int64_t ReadCrxBodyOffset(const base::FilePath& path) {
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    return -1;
  }
  std::array<uint8_t, 12> head;
  if (!file.ReadAndCheck(0, head)) {
    return -1;
  }
  if (!std::equal(head.begin(), head.begin() + 4, kCrxMagic.begin())) {
    return -1;
  }
  // head[4..7] = version (ignored beyond existence check).
  const uint32_t header_size = static_cast<uint32_t>(head[8]) |
                               (static_cast<uint32_t>(head[9]) << 8) |
                               (static_cast<uint32_t>(head[10]) << 16) |
                               (static_cast<uint32_t>(head[11]) << 24);
  // Guard against absurd header sizes — a sane CRX3 header is < 16 MiB.
  if (header_size == 0 || header_size > 16u * 1024 * 1024) {
    return -1;
  }
  return 12 + header_size;
}

}  // namespace

struct ExtensionInstaller::UnpackedResult {
  base::FilePath unpacked_root;
  std::string error;
};

ExtensionInstaller::ExtensionInstaller(content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

ExtensionInstaller::~ExtensionInstaller() = default;

void ExtensionInstaller::InstallFromFile(const base::FilePath& file_path,
                                         Callback cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const base::FilePath install_root =
      browser_context_->GetPath().AppendASCII("Extensions");
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ExtensionInstaller::UnpackOnBlockingThread, file_path,
                     install_root),
      base::BindOnce(&ExtensionInstaller::OnUnpacked,
                     weak_factory_.GetWeakPtr(), std::move(cb)));
}

// static
ExtensionInstaller::UnpackedResult
ExtensionInstaller::UnpackOnBlockingThread(const base::FilePath& src,
                                           const base::FilePath& dest_root) {
  UnpackedResult result;
  if (!base::PathExists(src)) {
    result.error = "Source path does not exist";
    return result;
  }
  if (!base::CreateDirectory(dest_root)) {
    result.error = "Could not create Extensions directory";
    return result;
  }

  base::FilePath staging = MakeStagingDir(dest_root);
  if (staging.empty()) {
    result.error = "Could not create staging directory";
    return result;
  }

  // Case 1: already-unpacked directory. Copy into staging so the user's copy
  // isn't our live install dir.
  if (IsUnpackedExtensionDir(src)) {
    if (!base::CopyDirectory(src, staging, /*recursive=*/true)) {
      result.error = "Could not copy unpacked extension";
      base::DeletePathRecursively(staging);
      return result;
    }
    // base::CopyDirectory copies src as a child of staging, so the real root
    // is staging/<src-basename>.
    result.unpacked_root = staging.Append(src.BaseName());
    return result;
  }

  // Case 2: .crx — strip the header, then unzip the body.
  std::array<uint8_t, 4> magic{};
  bool has_magic = ReadMagic(src, &magic);
  const bool is_crx =
      has_magic && std::equal(magic.begin(), magic.end(), kCrxMagic.begin());
  const bool is_zip = has_magic && magic[0] == 'P' && magic[1] == 'K';

  if (is_crx) {
    const int64_t body_offset = ReadCrxBodyOffset(src);
    if (body_offset < 0) {
      result.error = "Malformed CRX header";
      base::DeletePathRecursively(staging);
      return result;
    }
    // Slice out the zip portion into a scratch file, then unzip. zlib's Unzip
    // takes either a path or a fd, but the simplest is a scratch .zip.
    base::FilePath scratch_zip;
    if (!base::CreateTemporaryFileInDir(staging, &scratch_zip)) {
      result.error = "Could not create scratch zip";
      base::DeletePathRecursively(staging);
      return result;
    }
    base::File in(src, base::File::FLAG_OPEN | base::File::FLAG_READ);
    base::File out(scratch_zip,
                   base::File::FLAG_OPEN | base::File::FLAG_WRITE);
    if (!in.IsValid() || !out.IsValid()) {
      result.error = "I/O open failed while splitting CRX";
      base::DeletePathRecursively(staging);
      return result;
    }
    constexpr size_t kChunk = 64 * 1024;
    std::vector<uint8_t> buf(kChunk);
    int64_t offset = body_offset;
    while (true) {
      int read = in.Read(offset, reinterpret_cast<char*>(buf.data()), kChunk);
      if (read < 0) {
        result.error = "CRX body read failed";
        base::DeletePathRecursively(staging);
        return result;
      }
      if (read == 0) {
        break;
      }
      int written = out.WriteAtCurrentPos(reinterpret_cast<char*>(buf.data()),
                                          read);
      if (written != read) {
        result.error = "CRX body write failed";
        base::DeletePathRecursively(staging);
        return result;
      }
      offset += read;
    }
    out.Close();
    in.Close();

    if (!zip::Unzip(scratch_zip, staging)) {
      result.error = "CRX body is not a valid zip";
      base::DeletePathRecursively(staging);
      return result;
    }
    base::DeleteFile(scratch_zip);
    result.unpacked_root = staging;
    return result;
  }

  // Case 3: plain .zip. Accept by magic or suffix — extensions often ship
  // with the .zip extension for convenience.
  if (is_zip || HasSuffix(src, FILE_PATH_LITERAL(".zip"))) {
    if (!zip::Unzip(src, staging)) {
      result.error = "Zip extraction failed";
      base::DeletePathRecursively(staging);
      return result;
    }
    // Some zips wrap the extension in a single top-level folder; detect that
    // and descend once so manifest.json is at the real root.
    base::FileEnumerator entries(staging, /*recursive=*/false,
                                 base::FileEnumerator::FILES |
                                     base::FileEnumerator::DIRECTORIES);
    base::FilePath first = entries.Next();
    base::FilePath second = entries.Next();
    if (!first.empty() && second.empty() && base::DirectoryExists(first) &&
        base::PathExists(first.AppendASCII("manifest.json"))) {
      result.unpacked_root = first;
    } else {
      result.unpacked_root = staging;
    }
    return result;
  }

  result.error = "Unsupported file type (expected .zip, .crx, or a directory)";
  base::DeletePathRecursively(staging);
  return result;
}

void ExtensionInstaller::OnUnpacked(Callback cb, UnpackedResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!result.error.empty() || result.unpacked_root.empty()) {
    std::move(cb).Run(nullptr, result.error);
    return;
  }
  std::string load_error;
  scoped_refptr<const Extension> extension = file_util::LoadExtension(
      result.unpacked_root, mojom::ManifestLocation::kUnpacked,
      Extension::NO_FLAGS, &load_error);
  if (!extension) {
    LOG(WARNING) << "[Afterbird] LoadExtension failed: " << load_error;
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(base::IgnoreResult(&base::DeletePathRecursively),
                       result.unpacked_root));
    std::move(cb).Run(nullptr, load_error);
    return;
  }

  auto* system = static_cast<DesktopAndroidExtensionSystem*>(
      ExtensionSystem::Get(browser_context_));
  if (!system) {
    std::move(cb).Run(nullptr, "ExtensionSystem unavailable");
    return;
  }
  std::string add_error;
  if (!system->AddExtension(scoped_refptr<Extension>(
                                const_cast<Extension*>(extension.get())),
                            add_error)) {
    LOG(WARNING) << "[Afterbird] AddExtension failed: " << add_error;
    std::move(cb).Run(nullptr, add_error);
    return;
  }
  LOG(INFO) << "[Afterbird] Installed extension from " << result.unpacked_root;
  std::move(cb).Run(extension, std::string());
}

}  // namespace extensions
