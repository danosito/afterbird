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
  // Kiwi-style: let the Chrome Web Store detail page load normally and drive
  // installs through its native "Add to Chrome" button (which calls
  // chrome.webstorePrivate.beginInstallWithManifest3 — see
  // chrome/browser/extensions/desktop_android/webstore_private/). The
  // throttle used to intercept the URL and show a custom dialog; that wrapper
  // UX is retired as of v1.6. We keep the throttle registered so raw
  // redirect-to-.crx chains still reach the download layer.
  if (url.DomainIs("chromewebstore.google.com") ||
      url.DomainIs("chrome.google.com")) {
    return PROCEED;
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
