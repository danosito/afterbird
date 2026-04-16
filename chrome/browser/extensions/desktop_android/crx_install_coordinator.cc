// Copyright 2026 The Afterbird Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/desktop_android/crx_install_coordinator.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "base/uuid.h"
#include "base/values.h"
#include "chrome/browser/extensions/android/extension_install_confirm_bridge.h"
#include "chrome/browser/extensions/desktop_android/extension_installer.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace extensions {

namespace {

// Max payload accepted via SimpleURLLoader. Real CRX files are a few MB;
// cap at 128 MiB so a malicious server can't blow up disk.
constexpr size_t kMaxDownloadBytes = 128u * 1024 * 1024;

// Scratch dir under the browser_context cache. Shared with the Java picker
// flow — each file is UUID-prefixed so there are no collisions.
base::FilePath MakeFetchPath(content::BrowserContext* context,
                             const GURL& url) {
  base::FilePath base_cache;
  if (!base::PathService::Get(base::DIR_CACHE, &base_cache)) {
    base_cache = context->GetPath().Append(FILE_PATH_LITERAL("Cache"));
  }
  const base::FilePath dir =
      base_cache.Append(FILE_PATH_LITERAL("afterbird_picker"));
  base::CreateDirectory(dir);
  const std::string uniq = base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::string filename = url.ExtractFileName();
  if (filename.empty()) {
    filename = "picked.crx";
  }
  return dir.Append(uniq + "_" + filename);
}

// Extracts the manifest permissions list (MV3 `permissions` +
// `host_permissions`, or MV2 combined `permissions` list) and returns a
// comma-separated list of raw strings. Empty if none.
//
// No PermissionMessageProvider humanisation — that machinery lives in
// //extensions/common/permissions and is not compiled on desktop-android.
std::string JoinRawPermissions(const Extension& extension) {
  std::vector<std::string> out;
  const Manifest* manifest = extension.manifest();
  if (!manifest) {
    return std::string();
  }

  auto append_strings_from = [&out](const base::Value* value) {
    if (!value || !value->is_list()) {
      return;
    }
    for (const base::Value& entry : value->GetList()) {
      if (entry.is_string()) {
        out.push_back(entry.GetString());
      } else if (entry.is_dict()) {
        // MV2 sometimes nests {"fileSystem": ["write"]} dicts; include the
        // first key so the user sees *something*.
        for (const auto pair : entry.GetDict()) {
          out.push_back(pair.first);
          break;
        }
      }
    }
  };

  append_strings_from(manifest->FindPath("permissions"));
  append_strings_from(manifest->FindPath("host_permissions"));
  append_strings_from(manifest->FindPath("optional_permissions"));
  // De-duplicate while preserving order.
  std::vector<std::string> dedup;
  for (const std::string& s : out) {
    if (std::find(dedup.begin(), dedup.end(), s) == dedup.end()) {
      dedup.push_back(s);
    }
  }
  return base::JoinString(dedup, ", ");
}

}  // namespace

// static
void CrxInstallCoordinator::StartFromDownload(
    content::BrowserContext* browser_context,
    content::WebContents* web_contents,
    const GURL& url) {
  if (!browser_context) {
    return;
  }
  // Self-owned.
  auto* owned = new CrxInstallCoordinator(browser_context, web_contents, url,
                                          url.host());
  owned->Start();
}

// static
void CrxInstallCoordinator::StartFromWebstore(
    content::BrowserContext* browser_context,
    content::WebContents* web_contents,
    const GURL& crx_url,
    const std::string& source_label) {
  if (!browser_context) {
    return;
  }
  auto* owned = new CrxInstallCoordinator(browser_context, web_contents,
                                          crx_url, source_label);
  owned->Start();
}

CrxInstallCoordinator::CrxInstallCoordinator(
    content::BrowserContext* browser_context,
    content::WebContents* web_contents,
    const GURL& url,
    const std::string& source_label)
    : content::WebContentsObserver(web_contents),
      browser_context_(browser_context->GetWeakPtr()),
      url_(url),
      source_label_(source_label) {}

CrxInstallCoordinator::~CrxInstallCoordinator() = default;

void CrxInstallCoordinator::WebContentsDestroyed() {
  // Anchoring the dialog on a dead WebContents is impossible, but the
  // install itself can still finish; we just lose toast anchoring. Drop the
  // observer but keep running — if we're past AWAIT_CONFIRM the install
  // will complete without touching the tab.
  Observe(nullptr);
}

void CrxInstallCoordinator::Start() {
  if (!browser_context_) {
    delete this;
    return;
  }
  fetched_crx_path_ = MakeFetchPath(browser_context_.get(), url_);

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = url_;
  request->method = "GET";
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  const net::NetworkTrafficAnnotationTag annotation =
      net::DefineNetworkTrafficAnnotation("afterbird_extension_crx_fetch",
                                          R"(
        semantics {
          sender: "Afterbird Extensions"
          description:
            "Downloads a Chrome extension package (.crx) that the user has "
            "asked to install, so Afterbird can extract and register it "
            "through its own installer instead of saving to Downloads/ or "
            "prompting an 'open with' dialog that Android can't satisfy."
          trigger:
            "User navigated to a Chrome Web Store extension detail page, or "
            "initiated a download whose response MIME or URL indicates a "
            "Chrome extension package."
          data: "HTTP GET with no cookies, requesting the extension package."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: NO
          setting: "No user setting; gated on an install-confirmation dialog."
          policy_exception_justification: "Not yet implemented."
        })");

  loader_ = network::SimpleURLLoader::Create(std::move(request), annotation);
  loader_->SetAllowHttpErrorResults(false);

  auto factory = browser_context_->GetDefaultStoragePartition()
                     ->GetURLLoaderFactoryForBrowserProcess();
  loader_->DownloadToFile(
      factory.get(),
      base::BindOnce(&CrxInstallCoordinator::OnDownloaded,
                     weak_factory_.GetWeakPtr()),
      fetched_crx_path_, kMaxDownloadBytes);
}

void CrxInstallCoordinator::OnDownloaded(base::FilePath path) {
  if (path.empty() || !browser_context_) {
    const int net_err = loader_ ? loader_->NetError() : -1;
    LOG(WARNING) << "[Afterbird] crx download failed from " << url_
                 << " err=" << net_err;
    FailWith("Could not download extension");
    return;
  }
  fetched_crx_path_ = path;
  installer_ = std::make_unique<DesktopAndroidExtensionInstaller>(
      browser_context_.get());
  installer_->PrepareFromFile(
      path, base::BindOnce(&CrxInstallCoordinator::OnPrepared,
                           weak_factory_.GetWeakPtr()));
}

void CrxInstallCoordinator::OnPrepared(
    std::unique_ptr<DesktopAndroidExtensionInstaller::PreparedInstall> prepared) {
  if (!prepared) {
    FailWith("Extension package is corrupted");
    return;
  }
  if (!prepared->error.empty() || !prepared->extension) {
    LOG(WARNING) << "[Afterbird] unpack failed: " << prepared->error;
    installer_->DiscardPrepared(std::move(prepared));
    FailWith("Extension package is corrupted");
    return;
  }
  prepared_ = std::move(prepared);

  // S3: ask the user.
  if (!web_contents()) {
    // Lost the tab between fetch and confirm. Silent cancel.
    CancelSilently("web_contents_destroyed_before_confirm");
    return;
  }

  const std::string name = prepared_->extension->name();
  const std::string version = prepared_->extension->VersionString();
  const std::string permissions = JoinRawPermissions(*prepared_->extension);

  ExtensionInstallConfirmCallback::Show(
      web_contents(), name, version, permissions, source_label_,
      base::BindOnce(&CrxInstallCoordinator::OnConfirmDecision,
                     weak_factory_.GetWeakPtr()));
}

void CrxInstallCoordinator::OnConfirmDecision(bool confirmed) {
  if (!confirmed) {
    // User tapped Cancel / dismissed / back-press.
    if (installer_ && prepared_) {
      installer_->DiscardPrepared(std::move(prepared_));
    }
    // Also clean up the fetched CRX.
    if (!fetched_crx_path_.empty()) {
      base::ThreadPool::PostTask(
          FROM_HERE, {base::MayBlock()},
          base::BindOnce(base::IgnoreResult(&base::DeleteFile),
                         fetched_crx_path_));
    }
    ShowToast("Install cancelled");
    delete this;
    return;
  }

  if (!installer_ || !prepared_) {
    FailWith("Install state was lost");
    return;
  }

  // Remember whether the extension was already installed — used to pick a
  // "Installed" vs "Replaced" toast. Read it BEFORE commit, since commit
  // adds the entry to the registry.
  const std::string ext_id = prepared_->extension->id();
  const bool already_installed = IsAlreadyInstalled(ext_id);

  installer_->CommitPrepared(
      std::move(prepared_),
      base::BindOnce(&CrxInstallCoordinator::OnCommitted,
                     weak_factory_.GetWeakPtr(), already_installed));
}

void CrxInstallCoordinator::OnCommitted(
    bool was_already_installed,
    scoped_refptr<const Extension> extension,
    const std::string& error) {
  // Also clean up the originally fetched CRX file — the staging dir is kept
  // by the installer on success (promoted) or deleted on failure.
  if (!fetched_crx_path_.empty()) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(base::IgnoreResult(&base::DeleteFile),
                       fetched_crx_path_));
  }

  if (!extension) {
    LOG(WARNING) << "[Afterbird] commit failed: " << error;
    ShowToast("Install failed: " + error);
    delete this;
    return;
  }
  SucceedWith(extension, was_already_installed);
}

void CrxInstallCoordinator::FailWith(const std::string& reason) {
  LOG(WARNING) << "[Afterbird] crx install failed: " << reason
               << " url=" << url_;
  if (!fetched_crx_path_.empty()) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(base::IgnoreResult(&base::DeleteFile),
                       fetched_crx_path_));
  }
  if (installer_ && prepared_) {
    installer_->DiscardPrepared(std::move(prepared_));
  }
  ShowToast(reason);
  delete this;
}

void CrxInstallCoordinator::CancelSilently(const char* reason) {
  LOG(INFO) << "[Afterbird] crx install silently cancelled: " << reason
            << " url=" << url_;
  if (!fetched_crx_path_.empty()) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(base::IgnoreResult(&base::DeleteFile),
                       fetched_crx_path_));
  }
  if (installer_ && prepared_) {
    installer_->DiscardPrepared(std::move(prepared_));
  }
  delete this;
}

void CrxInstallCoordinator::SucceedWith(
    scoped_refptr<const Extension> extension,
    bool replaced) {
  LOG(INFO) << "[Afterbird] crx install succeeded: " << extension->id() << " "
            << extension->name();
  ShowToast(replaced ? "Extension replaced" : "Extension installed");
  delete this;
}

void CrxInstallCoordinator::ShowToast(const std::string& message) {
  // Best-effort: log to INFO so it's visible in `adb logcat`. A full
  // Activity-anchored toast would require another JNI hop; v1.2 scope keeps
  // it to a log line. ModalDialogManager will already have dismissed the
  // confirm dialog by this point.
  LOG(INFO) << "[Afterbird] install toast: " << message;
}

bool CrxInstallCoordinator::IsAlreadyInstalled(
    const std::string& extension_id) const {
  if (!browser_context_) {
    return false;
  }
  auto* registry = ExtensionRegistry::Get(browser_context_.get());
  if (!registry) {
    return false;
  }
  return registry->GetInstalledExtension(extension_id) != nullptr;
}

}  // namespace extensions
