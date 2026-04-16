// Copyright 2026 The Afterbird Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_DESKTOP_ANDROID_EXTENSION_INSTALL_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_EXTENSIONS_DESKTOP_ANDROID_EXTENSION_INSTALL_NAVIGATION_THROTTLE_H_

#include <memory>

#include "content/public/browser/navigation_throttle.h"
#include "url/gurl.h"

namespace extensions {

// Intercepts navigations that look like extension packages (URLs ending in
// `.crx` or `.user.js`, or responses served as `application/x-chrome-extension`).
// When matched, the navigation is cancelled silently and a helper
// (CrxDownloadInstaller, private to the .cc) fetches the resource and hands
// the cached path to DesktopAndroidExtensionInstaller.
class ExtensionInstallNavigationThrottle : public content::NavigationThrottle {
 public:
  // Returns a throttle for `handle` only when the request is a main-frame
  // navigation to a potentially-extension URL. Returns nullptr otherwise so
  // the MaybeAddThrottle path in ChromeContentBrowserClient can drop it.
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
  ThrottleCheckResult WillProcessResponse() override;
  const char* GetNameForLogging() override;

 private:
  // Kicks off the out-of-band download/install. Does NOT block the navigation
  // beyond this call — the helper is self-owned.
  void StartDownload(const GURL& url);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_DESKTOP_ANDROID_EXTENSION_INSTALL_NAVIGATION_THROTTLE_H_
