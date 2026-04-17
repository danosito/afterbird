// Copyright 2026 The Afterbird Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Minimal webstorePrivate implementation for desktop-android. The CWS
// detail page's Install button invokes beginInstallWithManifest3, which
// hands the extension id to CrxInstallCoordinator::StartFromWebstore —
// the same pipeline the deleted navigation-throttle used. We implement
// only the three functions the live CWS detail page calls.

#ifndef CHROME_BROWSER_EXTENSIONS_DESKTOP_ANDROID_WEBSTORE_PRIVATE_WEBSTORE_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_DESKTOP_ANDROID_WEBSTORE_PRIVATE_WEBSTORE_PRIVATE_API_H_

#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"
#include "extensions/browser/extension_function_registry.h"

namespace extensions {

// Registers the three webstorePrivate handlers below with the given registry.
// Call once from the BrowserClient's RegisterExtensionFunctions.
void RegisterDesktopAndroidWebstorePrivateFunctions(
    ExtensionFunctionRegistry* registry);

class WebstorePrivateGetWebGLStatusFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webstorePrivate.getWebGLStatus",
                             WEBSTOREPRIVATE_GETWEBGLSTATUS)
  WebstorePrivateGetWebGLStatusFunction();

 protected:
  ~WebstorePrivateGetWebGLStatusFunction() override;
  ResponseAction Run() override;
};

// Takes a Details dict {id: string, manifest: string, iconUrl?: string, ...}.
// Validates origin + extension id, hands off to CrxInstallCoordinator,
// responds with the CWS result_code enum the page expects.
class WebstorePrivateBeginInstallWithManifest3Function
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webstorePrivate.beginInstallWithManifest3",
                             WEBSTOREPRIVATE_BEGININSTALLWITHMANIFEST3)
  WebstorePrivateBeginInstallWithManifest3Function();

 protected:
  ~WebstorePrivateBeginInstallWithManifest3Function() override;
  ResponseAction Run() override;
};

// Pure no-op success. The CWS page uses completeInstall to drive its own
// spinner state; we have nothing to commit on our side because
// CrxInstallCoordinator completes asynchronously.
class WebstorePrivateCompleteInstallFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webstorePrivate.completeInstall",
                             WEBSTOREPRIVATE_COMPLETEINSTALL)
  WebstorePrivateCompleteInstallFunction();

 protected:
  ~WebstorePrivateCompleteInstallFunction() override;
  ResponseAction Run() override;
};

// Returns the full `132.0.6834.83` version string; CWS uses it to decide
// which extension variant to serve. Previously unregistered → the page
// would log "Unknown Extension API" and fall back to a no-op, leaving
// the Install button in an indeterminate state on some extensions.
class WebstorePrivateGetFullChromeVersionFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webstorePrivate.getFullChromeVersion",
                             WEBSTOREPRIVATE_GETFULLCHROMEVERSION)
  WebstorePrivateGetFullChromeVersionFunction();

 protected:
  ~WebstorePrivateGetFullChromeVersionFunction() override;
  ResponseAction Run() override;
};

// Returns MV2 deprecation phase info. We're on MV3-soft-deprecation, so
// answer with the "inactive" state — MV2 extensions still install and run.
class WebstorePrivateGetMV2DeprecationStatusFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webstorePrivate.getMV2DeprecationStatus",
                             WEBSTOREPRIVATE_GETMV2DEPRECATIONSTATUS)
  WebstorePrivateGetMV2DeprecationStatusFunction();

 protected:
  ~WebstorePrivateGetMV2DeprecationStatusFunction() override;
  ResponseAction Run() override;
};

// Returns an empty referrer chain. CWS uses this for spam/abuse signals;
// we have no SafeBrowsing integration on desktop-android so empty is fine.
class WebstorePrivateGetReferrerChainFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webstorePrivate.getReferrerChain",
                             WEBSTOREPRIVATE_GETREFERRERCHAIN)
  WebstorePrivateGetReferrerChainFunction();

 protected:
  ~WebstorePrivateGetReferrerChainFunction() override;
  ResponseAction Run() override;
};

// Returns whether the current window is incognito. Drives the "open in
// incognito" affordance on the CWS detail page.
class WebstorePrivateIsInIncognitoModeFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webstorePrivate.isInIncognitoMode",
                             WEBSTOREPRIVATE_ISININCOGNITOMODEFUNCTION)
  WebstorePrivateIsInIncognitoModeFunction();

 protected:
  ~WebstorePrivateIsInIncognitoModeFunction() override;
  ResponseAction Run() override;
};

// Returns the install state for a given extension id: "installable",
// "installed", "enabled", etc. CWS uses it to swap between the "Add to
// Chrome" and "Remove from Chrome" button text.
class WebstorePrivateGetExtensionStatusFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webstorePrivate.getExtensionStatus",
                             WEBSTOREPRIVATE_GETEXTENSIONSTATUS)
  WebstorePrivateGetExtensionStatusFunction();

 protected:
  ~WebstorePrivateGetExtensionStatusFunction() override;
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_DESKTOP_ANDROID_WEBSTORE_PRIVATE_WEBSTORE_PRIVATE_API_H_
