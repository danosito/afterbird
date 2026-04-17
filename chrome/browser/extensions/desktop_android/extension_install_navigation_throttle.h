// Copyright 2026 The Afterbird Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_DESKTOP_ANDROID_EXTENSION_INSTALL_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_EXTENSIONS_DESKTOP_ANDROID_EXTENSION_INSTALL_NAVIGATION_THROTTLE_H_

#include <memory>

#include "content/public/browser/navigation_throttle.h"
#include "url/gurl.h"

namespace extensions {

// Intercepts main-frame navigations to Chrome Web Store detail pages
// (`chromewebstore.google.com/detail/...` or
// `chrome.google.com/webstore/detail/...`) and, on match, hands the
// synthesised clients2.google.com/service/update2/crx URL to
// CrxInstallCoordinator, which runs the v1.2 fetch → confirm → install
// pipeline.
//
// v1.1 also matched raw `.crx` / `.user.js` URLs and responses with MIME
// `application/x-chrome-extension`. v1.2 moves that logic to the
// ChromeDownloadManagerDelegate::InterceptDownloadIfApplicable hook, so
// this throttle only deals with webstore store-detail URLs now. No download
// intent, no dialog — just a URL shape match.
class ExtensionInstallNavigationThrottle : public content::NavigationThrottle {
 public:
  // Returns a throttle for `handle` only when the request is a main-frame
  // http(s) navigation. Returns nullptr otherwise so the MaybeAddThrottle
  // path in ChromeContentBrowserClient can drop it.
  static std::unique_ptr<ExtensionInstallNavigationThrottle> MaybeCreate(
      content::NavigationHandle* handle);

  explicit ExtensionInstallNavigationThrottle(
      content::NavigationHandle* handle);
  ExtensionInstallNavigationThrottle(
      const ExtensionInstallNavigationThrottle&) = delete;
  ExtensionInstallNavigationThrottle& operator=(
      const ExtensionInstallNavigationThrottle&) = delete;
  ~ExtensionInstallNavigationThrottle() override;

  // content::NavigationThrottle:
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  const char* GetNameForLogging() override;

 private:
  // Cancels the navigation and hands off to the install coordinator. Safe
  // to call once per throttle lifetime.
  void HandOffToCoordinator(const GURL& crx_url);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_DESKTOP_ANDROID_EXTENSION_INSTALL_NAVIGATION_THROTTLE_H_
