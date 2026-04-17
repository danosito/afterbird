// Copyright 2026 The Afterbird Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/desktop_android/extension_install_navigation_throttle.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/browser/extensions/desktop_android/crx_install_coordinator.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace extensions {

namespace {

// Recognises Chrome Web Store detail pages. Returns the 32-character
// extension id on match, empty string otherwise. Supports both the current
// host (chromewebstore.google.com) and the legacy one still seen in shared
// links (chrome.google.com/webstore).
std::string ExtractWebstoreExtensionId(const GURL& url) {
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return std::string();
  }
  const std::string host = url.host();
  const std::string path = url.path();
  std::string id_candidate;
  if (host == "chromewebstore.google.com") {
    // /detail/<slug>/<id> or /detail/<id>
    std::vector<std::string_view> parts = base::SplitStringPiece(
        path, "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (parts.size() >= 2 && parts[0] == "detail") {
      id_candidate = std::string(parts.back());
    }
  } else if (host == "chrome.google.com" &&
             base::StartsWith(path, "/webstore/detail/",
                              base::CompareCase::SENSITIVE)) {
    std::vector<std::string_view> parts = base::SplitStringPiece(
        path, "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    // /webstore/detail/<slug>/<id> or /webstore/detail/<id>
    if (parts.size() >= 3) {
      id_candidate = std::string(parts.back());
    }
  }
  if (id_candidate.size() != 32) {
    return std::string();
  }
  // Extension IDs are lowercase a-p (32 chars of the 16-letter alphabet
  // used by ExtensionId::kAlphabet). Be lenient: just require [a-p].
  for (char c : id_candidate) {
    if (c < 'a' || c > 'p') {
      return std::string();
    }
  }
  return id_candidate;
}

// Builds the Chrome Web Store "get CRX" URL. This is the same endpoint the
// omaha updater hits for autoupdates; it returns a 302 to a googleusercontent
// CRX blob for any valid public extension ID.
GURL BuildWebstoreCrxUrl(const std::string& extension_id) {
  return GURL(
      "https://clients2.google.com/service/update2/crx?response=redirect"
      "&prodversion=128.0&acceptformat=crx2,crx3&x=id%3D" +
      extension_id + "%26installsource%3Dondemand%26uc");
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
  const std::string webstore_id = ExtractWebstoreExtensionId(url);
  if (!webstore_id.empty()) {
    const GURL crx_url = BuildWebstoreCrxUrl(webstore_id);
    LOG(INFO) << "[Afterbird] webstore intercept id=" << webstore_id
              << " crx=" << crx_url;
    HandOffToCoordinator(crx_url);
    return CANCEL_AND_IGNORE;
  }
  // Temporary: log CWS navigations we *didn't* intercept so we can see
  // whether the URL layout changed, the host is different, or the throttle
  // simply didn't run. Drop this once we're confident.
  if (url.DomainIs("chromewebstore.google.com") ||
      url.DomainIs("chrome.google.com")) {
    LOG(INFO) << "[Afterbird] throttle saw store URL but didn't match: " << url;
  }
  return PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
ExtensionInstallNavigationThrottle::WillRedirectRequest() {
  // Evaluate the redirect target too — some store CDNs return an HTTP 302
  // from a non-suspicious URL to the actual detail page.
  return WillStartRequest();
}

const char* ExtensionInstallNavigationThrottle::GetNameForLogging() {
  return "ExtensionInstallNavigationThrottle";
}

void ExtensionInstallNavigationThrottle::HandOffToCoordinator(
    const GURL& crx_url) {
  content::WebContents* web_contents = navigation_handle()->GetWebContents();
  if (!web_contents) {
    return;
  }
  content::BrowserContext* context = web_contents->GetBrowserContext();
  if (!context) {
    return;
  }
  // CrxInstallCoordinator self-owns; it will outlive this throttle
  // (the throttle is torn down as soon as we return CANCEL_AND_IGNORE).
  CrxInstallCoordinator::StartFromWebstore(context, web_contents, crx_url,
                                           "Chrome Web Store");
}

}  // namespace extensions
