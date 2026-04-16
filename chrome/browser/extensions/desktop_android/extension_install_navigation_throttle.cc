// Copyright 2026 The Afterbird Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/desktop_android/extension_install_navigation_throttle.h"

#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/uuid.h"
#include "chrome/browser/extensions/desktop_android/extension_installer.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"
#include "net/base/filename_util.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace extensions {

namespace {

// Max payload accepted via SimpleURLLoader. Real CRX files are a few MB;
// cap at 128 MiB so a malicious server can't blow up disk.
constexpr size_t kMaxDownloadBytes = 128u * 1024 * 1024;

// Self-owned helper that outlives the NavigationThrottle. The throttle dies
// the moment it returns CANCEL_AND_IGNORE, so the SimpleURLLoader + installer
// need to live somewhere else. We park them here and `delete this;` once the
// installer callback fires.
class CrxDownloadInstaller {
 public:
  CrxDownloadInstaller(content::BrowserContext* context,
                       const GURL& url,
                       const base::FilePath& cache_path)
      : browser_context_(context->GetWeakPtr()),
        cache_path_(cache_path),
        url_(url) {}
  CrxDownloadInstaller(const CrxDownloadInstaller&) = delete;
  CrxDownloadInstaller& operator=(const CrxDownloadInstaller&) = delete;
  ~CrxDownloadInstaller() = default;

  // Kicks off the download; self-deletes when the install callback runs.
  void Start() {
    if (!browser_context_) {
      delete this;
      return;
    }
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
              "Downloads a .crx / .user.js file the user navigated to, so "
              "Afterbird can install it through its extension installer "
              "instead of prompting an 'open with' dialog that Android "
              "can't satisfy."
            trigger:
              "User navigated to a URL ending in .crx or .user.js, or a URL "
              "whose response carries Content-Type: "
              "application/x-chrome-extension."
            data: "HTTP GET with no cookies, requesting the extension package."
            destination: WEBSITE
          }
          policy {
            cookies_allowed: NO
            setting: "No user setting."
            policy_exception_justification: "Not yet implemented."
          })");

    loader_ = network::SimpleURLLoader::Create(std::move(request), annotation);
    loader_->SetAllowHttpErrorResults(false);

    auto factory = browser_context_->GetDefaultStoragePartition()
                       ->GetURLLoaderFactoryForBrowserProcess();
    loader_->DownloadToFile(
        factory.get(),
        base::BindOnce(&CrxDownloadInstaller::OnDownloaded,
                       base::Unretained(this)),
        cache_path_, kMaxDownloadBytes);
  }

 private:
  void OnDownloaded(base::FilePath path) {
    if (path.empty() || !browser_context_) {
      LOG(WARNING) << "[Afterbird] crx download failed from " << url_
                   << " err=" << (loader_ ? loader_->NetError() : -1);
      delete this;
      return;
    }
    installer_ = std::make_unique<DesktopAndroidExtensionInstaller>(
        browser_context_.get());
    installer_->InstallFromFile(
        path,
        base::BindOnce(&CrxDownloadInstaller::OnInstalled,
                       base::Unretained(this)));
  }

  void OnInstalled(scoped_refptr<const Extension> ext,
                   const std::string& error) {
    if (ext) {
      LOG(INFO) << "[Afterbird] crx install succeeded: " << ext->id() << " "
                << ext->name();
    } else {
      LOG(WARNING) << "[Afterbird] crx install failed from " << url_ << ": "
                   << error;
    }
    delete this;
  }

  base::WeakPtr<content::BrowserContext> browser_context_;
  base::FilePath cache_path_;
  GURL url_;
  std::unique_ptr<network::SimpleURLLoader> loader_;
  std::unique_ptr<DesktopAndroidExtensionInstaller> installer_;
};

// Filename-extension match for candidate extension URLs.
bool LooksLikeExtensionUrl(const GURL& url) {
  if (!url.is_valid() || !url.has_host()) {
    return false;
  }
  const std::string path = base::ToLowerASCII(url.path());
  return base::EndsWith(path, ".crx") || base::EndsWith(path, ".user.js");
}

// Response MIME match. Servers serving .crx under a generic path still set
// Content-Type correctly. See Extension::kMimeType.
bool LooksLikeExtensionMime(const std::string& mime_type) {
  return mime_type == "application/x-chrome-extension" ||
         mime_type == "application/x-chromium-extension";
}

// Prefer the Content-Disposition filename; fall back to the URL's basename.
base::FilePath SuggestedFilename(const GURL& url,
                                 const std::string& content_disposition) {
  std::u16string suggested = net::GetSuggestedFilename(
      url, content_disposition, /*referrer_charset=*/std::string(),
      /*suggested_name=*/std::string(),
      /*mime_type=*/std::string(),
      /*default_file_name=*/"picked.crx");
#if defined(OS_WIN)
  return base::FilePath(suggested);
#else
  return base::FilePath(base::UTF16ToUTF8(suggested));
#endif
}

// Best-effort scratch dir under the browser_context cache. We reuse the same
// afterbird_picker folder the Java picker streams into — keeps cleanup in one
// place.
base::FilePath MakeScratchPath(content::BrowserContext* context,
                               const base::FilePath& filename) {
  base::FilePath base_cache;
  if (!base::PathService::Get(base::DIR_CACHE, &base_cache)) {
    base_cache = context->GetPath().Append(FILE_PATH_LITERAL("Cache"));
  }
  const base::FilePath dir =
      base_cache.Append(FILE_PATH_LITERAL("afterbird_picker"));
  base::CreateDirectory(dir);
  const std::string uniq = base::Uuid::GenerateRandomV4().AsLowercaseString();
  base::FilePath name = filename.empty() ? base::FilePath("picked.crx")
                                          : filename.BaseName();
  return dir.Append(uniq + "_" + name.AsUTF8Unsafe());
}

}  // namespace

// static
std::unique_ptr<ExtensionInstallNavigationThrottle>
ExtensionInstallNavigationThrottle::MaybeCreate(
    content::NavigationHandle* handle) {
  if (!handle || !handle->IsInMainFrame()) {
    return nullptr;
  }
  // Bail on non-http(s) schemes. We don't want to touch chrome://, about:, etc.
  if (!handle->GetURL().SchemeIsHTTPOrHTTPS()) {
    return nullptr;
  }
  return std::make_unique<ExtensionInstallNavigationThrottle>(handle);
}

ExtensionInstallNavigationThrottle::ExtensionInstallNavigationThrottle(
    content::NavigationHandle* handle)
    : content::NavigationThrottle(handle) {}

ExtensionInstallNavigationThrottle::~ExtensionInstallNavigationThrottle() =
    default;

content::NavigationThrottle::ThrottleCheckResult
ExtensionInstallNavigationThrottle::WillStartRequest() {
  const GURL& url = navigation_handle()->GetURL();
  if (!LooksLikeExtensionUrl(url)) {
    return PROCEED;
  }
  LOG(INFO) << "[Afterbird] crx-url intercept, starting download: " << url;
  StartDownload(url);
  return CANCEL_AND_IGNORE;
}

content::NavigationThrottle::ThrottleCheckResult
ExtensionInstallNavigationThrottle::WillRedirectRequest() {
  // Evaluate the redirect target too — some store CDNs return an HTTP 302
  // from a non-suspicious URL to the actual .crx.
  return WillStartRequest();
}

content::NavigationThrottle::ThrottleCheckResult
ExtensionInstallNavigationThrottle::WillProcessResponse() {
  // MIME-based fallback: if the URL didn't end in .crx but the server says
  // it's an extension, still divert.
  if (const net::HttpResponseHeaders* headers =
          navigation_handle()->GetResponseHeaders()) {
    std::string mime;
    headers->GetMimeType(&mime);
    if (LooksLikeExtensionMime(mime)) {
      const GURL& url = navigation_handle()->GetURL();
      LOG(INFO) << "[Afterbird] crx-mime intercept, starting download: " << url
                << " mime=" << mime;
      StartDownload(url);
      return CANCEL_AND_IGNORE;
    }
  }
  return PROCEED;
}

const char* ExtensionInstallNavigationThrottle::GetNameForLogging() {
  return "ExtensionInstallNavigationThrottle";
}

void ExtensionInstallNavigationThrottle::StartDownload(const GURL& url) {
  content::WebContents* web_contents = navigation_handle()->GetWebContents();
  if (!web_contents) {
    return;
  }
  content::BrowserContext* context = web_contents->GetBrowserContext();
  if (!context) {
    return;
  }
  const base::FilePath cache_path =
      MakeScratchPath(context, base::FilePath(url.ExtractFileName()));
  // CrxDownloadInstaller self-deletes once the install callback runs; this
  // lets the download and install outlive the NavigationThrottle, which is
  // torn down as soon as we return CANCEL_AND_IGNORE.
  (new CrxDownloadInstaller(context, url, cache_path))->Start();
}

}  // namespace extensions
