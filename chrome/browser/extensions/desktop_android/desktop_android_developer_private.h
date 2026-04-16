// Copyright 2026 The Afterbird Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Minimal implementation of the developerPrivate API for the
// desktop-android-extensions build. The upstream implementation at
// //chrome/browser/extensions/api/developer_private/ is ~3000 lines and
// pulls in ExtensionService, DevTools, SafetyHub, ManifestV2 deprecation
// infrastructure, safe browsing, etc. — none of which are compiled with
// enable_extensions=false.
//
// We only implement the handlers that chrome://extensions actually needs to
// render a usable list view:
//   * developerPrivate.getProfileConfiguration
//   * developerPrivate.getExtensionsInfo
//   * developerPrivate.getItemsInfo
//   * developerPrivate.updateProfileConfiguration (for the Developer mode
//     toggle)
//   * developerPrivate.reload / enable / allowIncognito (no-op/best-effort)
//
// Everything else reports "not implemented" and JS falls back to the page's
// empty-state UI.

#ifndef CHROME_BROWSER_EXTENSIONS_DESKTOP_ANDROID_DESKTOP_ANDROID_DEVELOPER_PRIVATE_H_
#define CHROME_BROWSER_EXTENSIONS_DESKTOP_ANDROID_DESKTOP_ANDROID_DEVELOPER_PRIVATE_H_

#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"

namespace extensions {

class ExtensionFunctionRegistry;

// Registers the developerPrivate handlers below with the provided registry.
// Call this once from the BrowserClient's RegisterExtensionFunctions().
void RegisterDesktopAndroidDeveloperPrivateFunctions(
    ExtensionFunctionRegistry* registry);

// ----------------------------------------------------------------------------

class DesktopAndroidDeveloperPrivateGetProfileConfigurationFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.getProfileConfiguration",
                             DEVELOPER_PRIVATE_GETPROFILECONFIGURATION)
  DesktopAndroidDeveloperPrivateGetProfileConfigurationFunction();

 protected:
  ~DesktopAndroidDeveloperPrivateGetProfileConfigurationFunction() override;
  ResponseAction Run() override;
};

class DesktopAndroidDeveloperPrivateUpdateProfileConfigurationFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.updateProfileConfiguration",
                             DEVELOPER_PRIVATE_UPDATEPROFILECONFIGURATION)
  DesktopAndroidDeveloperPrivateUpdateProfileConfigurationFunction();

 protected:
  ~DesktopAndroidDeveloperPrivateUpdateProfileConfigurationFunction() override;
  ResponseAction Run() override;
};

class DesktopAndroidDeveloperPrivateGetExtensionsInfoFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.getExtensionsInfo",
                             DEVELOPER_PRIVATE_GETEXTENSIONSINFO)
  DesktopAndroidDeveloperPrivateGetExtensionsInfoFunction();

 protected:
  ~DesktopAndroidDeveloperPrivateGetExtensionsInfoFunction() override;
  ResponseAction Run() override;
};

class DesktopAndroidDeveloperPrivateGetItemsInfoFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.getItemsInfo",
                             DEVELOPER_PRIVATE_GETITEMSINFO)
  DesktopAndroidDeveloperPrivateGetItemsInfoFunction();

 protected:
  ~DesktopAndroidDeveloperPrivateGetItemsInfoFunction() override;
  ResponseAction Run() override;
};

class DesktopAndroidDeveloperPrivateGetExtensionInfoFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.getExtensionInfo",
                             DEVELOPER_PRIVATE_GETEXTENSIONINFO)
  DesktopAndroidDeveloperPrivateGetExtensionInfoFunction();

 protected:
  ~DesktopAndroidDeveloperPrivateGetExtensionInfoFunction() override;
  ResponseAction Run() override;
};

// Simple stubs that always succeed with an empty response, so that the JS
// can optimistically call them without throwing. Backend side-effects are
// not implemented.
class DesktopAndroidDeveloperPrivateNoOpFunction : public ExtensionFunction {
 protected:
  ~DesktopAndroidDeveloperPrivateNoOpFunction() override = default;
  ResponseAction Run() override;
};

#define AFTERBIRD_DEVELOPER_PRIVATE_NOOP(ClassName, api_name, histogram_value) \
  class ClassName : public DesktopAndroidDeveloperPrivateNoOpFunction {       \
   public:                                                                    \
    DECLARE_EXTENSION_FUNCTION(api_name, histogram_value)                     \
  }

AFTERBIRD_DEVELOPER_PRIVATE_NOOP(
    DesktopAndroidDeveloperPrivateAutoUpdateFunction,
    "developerPrivate.autoUpdate",
    DEVELOPER_PRIVATE_AUTOUPDATE);
AFTERBIRD_DEVELOPER_PRIVATE_NOOP(
    DesktopAndroidDeveloperPrivateReloadFunction,
    "developerPrivate.reload",
    DEVELOPER_PRIVATE_RELOAD);
AFTERBIRD_DEVELOPER_PRIVATE_NOOP(
    DesktopAndroidDeveloperPrivateDeleteExtensionErrorsFunction,
    "developerPrivate.deleteExtensionErrors",
    DEVELOPER_PRIVATE_DELETEEXTENSIONERRORS);
AFTERBIRD_DEVELOPER_PRIVATE_NOOP(
    DesktopAndroidDeveloperPrivateGetUserSiteSettingsFunction,
    "developerPrivate.getUserSiteSettings",
    DEVELOPER_PRIVATE_GETUSERSITESETTINGS);
AFTERBIRD_DEVELOPER_PRIVATE_NOOP(
    DesktopAndroidDeveloperPrivateGetUserAndExtensionSitesByEtldFunction,
    "developerPrivate.getUserAndExtensionSitesByEtld",
    DEVELOPER_PRIVATE_GETUSERANDEXTENSIONSITESBYETLD);
AFTERBIRD_DEVELOPER_PRIVATE_NOOP(
    DesktopAndroidDeveloperPrivateGetMatchingExtensionsForSiteFunction,
    "developerPrivate.getMatchingExtensionsForSite",
    DEVELOPER_PRIVATE_GETMATCHINGEXTENSIONSFORSITE);

#undef AFTERBIRD_DEVELOPER_PRIVATE_NOOP

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_DESKTOP_ANDROID_DESKTOP_ANDROID_DEVELOPER_PRIVATE_H_
