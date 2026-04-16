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

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"
#include "extensions/browser/extension_function_registry.h"

namespace extensions {
class DesktopAndroidExtensionInstaller;
class Extension;
}  // namespace extensions

namespace extensions {

// Registers the developerPrivate handlers below with the provided registry.
// Call this once from the BrowserClient's RegisterExtensionFunctions().
void RegisterDesktopAndroidDeveloperPrivateFunctions(
    ExtensionFunctionRegistry* registry);

// ----------------------------------------------------------------------------

class DesktopAndroidDeveloperPrivateGetProfileConfigurationFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.getProfileConfiguration",
                             DEVELOPERPRIVATE_GETPROFILECONFIGURATION)
  DesktopAndroidDeveloperPrivateGetProfileConfigurationFunction();

 protected:
  ~DesktopAndroidDeveloperPrivateGetProfileConfigurationFunction() override;
  ResponseAction Run() override;
};

class DesktopAndroidDeveloperPrivateUpdateProfileConfigurationFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.updateProfileConfiguration",
                             DEVELOPERPRIVATE_UPDATEPROFILECONFIGURATION)
  DesktopAndroidDeveloperPrivateUpdateProfileConfigurationFunction();

 protected:
  ~DesktopAndroidDeveloperPrivateUpdateProfileConfigurationFunction() override;
  ResponseAction Run() override;
};

class DesktopAndroidDeveloperPrivateGetExtensionsInfoFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.getExtensionsInfo",
                             DEVELOPERPRIVATE_GETEXTENSIONSINFO)
  DesktopAndroidDeveloperPrivateGetExtensionsInfoFunction();

 protected:
  ~DesktopAndroidDeveloperPrivateGetExtensionsInfoFunction() override;
  ResponseAction Run() override;
};

class DesktopAndroidDeveloperPrivateGetExtensionInfoFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.getExtensionInfo",
                             DEVELOPERPRIVATE_GETEXTENSIONINFO)
  DesktopAndroidDeveloperPrivateGetExtensionInfoFunction();

 protected:
  ~DesktopAndroidDeveloperPrivateGetExtensionInfoFunction() override;
  ResponseAction Run() override;
};

// developerPrivate.updateExtensionConfiguration — { extensionId, ...flags }.
// Only the `userMayModify → disableReason` subset is honoured; most flags are
// no-ops on desktop-android.
class DesktopAndroidDeveloperPrivateUpdateExtensionConfigurationFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.updateExtensionConfiguration",
                             DEVELOPERPRIVATE_UPDATEEXTENSIONCONFIGURATION)
  DesktopAndroidDeveloperPrivateUpdateExtensionConfigurationFunction();

 protected:
  ~DesktopAndroidDeveloperPrivateUpdateExtensionConfigurationFunction()
      override;
  ResponseAction Run() override;
};

// developerPrivate.loadUnpacked — opens the Android file picker, streams the
// selection into the app cache, and runs DesktopAndroidExtensionInstaller.
// Responds asynchronously after either the user cancels, the install fails,
// or the install succeeds. Takes a LoadUnpackedOptions dict but we ignore
// failQuietly / populateError / retryGuid / useDraggedPath.
class DesktopAndroidDeveloperPrivateLoadUnpackedFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.loadUnpacked",
                             DEVELOPERPRIVATE_LOADUNPACKED)
  DesktopAndroidDeveloperPrivateLoadUnpackedFunction();

 protected:
  ~DesktopAndroidDeveloperPrivateLoadUnpackedFunction() override;
  ResponseAction Run() override;

 private:
  void OnFilePicked(const base::FilePath& path);
  void OnInstalled(scoped_refptr<const Extension> extension,
                   const std::string& error);

  std::unique_ptr<DesktopAndroidExtensionInstaller> installer_;
};

// chrome://extensions' toolbar calls these for Remove and toggle. Their
// official names (management.uninstall / management.setEnabled) live in the
// `management` API namespace which we don't compile; service.ts is patched
// to route through developerPrivate instead.
class DesktopAndroidDeveloperPrivateRemoveMultipleExtensionsFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.removeMultipleExtensions",
                             DEVELOPERPRIVATE_REMOVEMULTIPLEEXTENSIONS)
  DesktopAndroidDeveloperPrivateRemoveMultipleExtensionsFunction();

 protected:
  ~DesktopAndroidDeveloperPrivateRemoveMultipleExtensionsFunction() override;
  ResponseAction Run() override;
};

class DesktopAndroidDeveloperPrivateReloadFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.reload",
                             DEVELOPERPRIVATE_RELOAD)
  DesktopAndroidDeveloperPrivateReloadFunction();

 protected:
  ~DesktopAndroidDeveloperPrivateReloadFunction() override;
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

// chromium-style lints require ref-counted classes to declare an explicit
// protected/private destructor, so the NOOP macro expands to a full class with
// =default dtor rather than the one-liner pattern.
#define AFTERBIRD_DEVELOPERPRIVATE_NOOP(ClassName, api_name, histogram_value) \
  class ClassName : public DesktopAndroidDeveloperPrivateNoOpFunction {       \
   public:                                                                    \
    DECLARE_EXTENSION_FUNCTION(api_name, histogram_value)                     \
                                                                              \
   protected:                                                                 \
    ~ClassName() override = default;                                          \
  }

AFTERBIRD_DEVELOPERPRIVATE_NOOP(
    DesktopAndroidDeveloperPrivateAutoUpdateFunction,
    "developerPrivate.autoUpdate",
    DEVELOPERPRIVATE_AUTOUPDATE);
AFTERBIRD_DEVELOPERPRIVATE_NOOP(
    DesktopAndroidDeveloperPrivateDeleteExtensionErrorsFunction,
    "developerPrivate.deleteExtensionErrors",
    DEVELOPERPRIVATE_DELETEEXTENSIONERRORS);
AFTERBIRD_DEVELOPERPRIVATE_NOOP(
    DesktopAndroidDeveloperPrivateGetUserSiteSettingsFunction,
    "developerPrivate.getUserSiteSettings",
    DEVELOPERPRIVATE_GETUSERSITESETTINGS);
AFTERBIRD_DEVELOPERPRIVATE_NOOP(
    DesktopAndroidDeveloperPrivateGetUserAndExtensionSitesByEtldFunction,
    "developerPrivate.getUserAndExtensionSitesByEtld",
    DEVELOPERPRIVATE_GETUSERANDEXTENSIONSITESBYETLD);
AFTERBIRD_DEVELOPERPRIVATE_NOOP(
    DesktopAndroidDeveloperPrivateGetMatchingExtensionsForSiteFunction,
    "developerPrivate.getMatchingExtensionsForSite",
    DEVELOPERPRIVATE_GETMATCHINGEXTENSIONSFORSITE);

#undef AFTERBIRD_DEVELOPERPRIVATE_NOOP

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_DESKTOP_ANDROID_DESKTOP_ANDROID_DEVELOPER_PRIVATE_H_
