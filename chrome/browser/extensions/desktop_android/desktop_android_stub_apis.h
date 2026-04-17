// Copyright 2026 The Afterbird Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Thin, best-effort stubs for chrome.* extension functions that real-world
// extensions (uBlock Origin, Dark Reader, Bitwarden, Honey, etc.) call
// defensively on startup. Each stub produces a plausible empty / success
// response so the JS-side binding promise resolves instead of throwing
// "Access to extension API denied." — which in MV3 service workers often
// crashes the worker before the real setup code runs.
//
// These are NOT replacements for the real APIs. Categories below:
//
//   * Shape-only stubs that return a valid-shape default (`[]`, `{}`, a
//     synthetic id, `true`/`false`) so `.then(x => x.filter(...))` and similar
//     don't throw. This is why Bitwarden's BadgeService stops crashing on
//     SW wake once `tabs.query` returns `[]` instead of `undefined`.
//
//   * Manifest echoes: `permissions.getAll`, `commands.getAll` read the
//     extension's manifest and transcribe declared entries into the expected
//     API response shape.
//
//   * Logging stubs: `notifications.create` returns a synthetic id; no
//     actual toast is surfaced since NotificationDisplayService isn't wired
//     on desktop-android.
//
// If an extension needs a *real* implementation, it belongs in a dedicated
// file (see desktop_android_developer_private.cc for an example). This file
// should stay "one class per stub, self-contained, no side effects".

#ifndef CHROME_BROWSER_EXTENSIONS_DESKTOP_ANDROID_DESKTOP_ANDROID_STUB_APIS_H_
#define CHROME_BROWSER_EXTENSIONS_DESKTOP_ANDROID_DESKTOP_ANDROID_STUB_APIS_H_

#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"
#include "extensions/browser/extension_function_registry.h"

namespace extensions {

// Registers the stub functions below with the provided registry.
void RegisterDesktopAndroidStubApiFunctions(
    ExtensionFunctionRegistry* registry);

// ---- permissions ----------------------------------------------------------

class DesktopAndroidPermissionsGetAllFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("permissions.getAll", PERMISSIONS_GETALL)
  DesktopAndroidPermissionsGetAllFunction() = default;

 protected:
  ~DesktopAndroidPermissionsGetAllFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidPermissionsContainsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("permissions.contains", PERMISSIONS_CONTAINS)
  DesktopAndroidPermissionsContainsFunction() = default;

 protected:
  ~DesktopAndroidPermissionsContainsFunction() override = default;
  ResponseAction Run() override;
};

// ---- commands -------------------------------------------------------------

class DesktopAndroidCommandsGetAllFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("commands.getAll", COMMANDS_GETALL)
  DesktopAndroidCommandsGetAllFunction() = default;

 protected:
  ~DesktopAndroidCommandsGetAllFunction() override = default;
  ResponseAction Run() override;
};

// ---- notifications --------------------------------------------------------

class DesktopAndroidNotificationsCreateFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("notifications.create", NOTIFICATIONS_CREATE)
  DesktopAndroidNotificationsCreateFunction() = default;

 protected:
  ~DesktopAndroidNotificationsCreateFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidNotificationsUpdateFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("notifications.update", NOTIFICATIONS_UPDATE)
  DesktopAndroidNotificationsUpdateFunction() = default;

 protected:
  ~DesktopAndroidNotificationsUpdateFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidNotificationsClearFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("notifications.clear", NOTIFICATIONS_CLEAR)
  DesktopAndroidNotificationsClearFunction() = default;

 protected:
  ~DesktopAndroidNotificationsClearFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidNotificationsGetAllFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("notifications.getAll", NOTIFICATIONS_GET_ALL)
  DesktopAndroidNotificationsGetAllFunction() = default;

 protected:
  ~DesktopAndroidNotificationsGetAllFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidNotificationsGetPermissionLevelFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("notifications.getPermissionLevel",
                             NOTIFICATIONS_GETPERMISSIONLEVEL)
  DesktopAndroidNotificationsGetPermissionLevelFunction() = default;

 protected:
  ~DesktopAndroidNotificationsGetPermissionLevelFunction() override = default;
  ResponseAction Run() override;
};

// ---- tabs (shape-only, no TabAndroid plumbing yet) ------------------------

class DesktopAndroidTabsQueryFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabs.query", TABS_QUERY)
  DesktopAndroidTabsQueryFunction() = default;

 protected:
  ~DesktopAndroidTabsQueryFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidTabsGetFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabs.get", TABS_GET)
  DesktopAndroidTabsGetFunction() = default;

 protected:
  ~DesktopAndroidTabsGetFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidTabsGetCurrentFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabs.getCurrent", TABS_GETCURRENT)
  DesktopAndroidTabsGetCurrentFunction() = default;

 protected:
  ~DesktopAndroidTabsGetCurrentFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidTabsCreateFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabs.create", TABS_CREATE)
  DesktopAndroidTabsCreateFunction() = default;

 protected:
  ~DesktopAndroidTabsCreateFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidTabsRemoveFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabs.remove", TABS_REMOVE)
  DesktopAndroidTabsRemoveFunction() = default;

 protected:
  ~DesktopAndroidTabsRemoveFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidTabsUpdateFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabs.update", TABS_UPDATE)
  DesktopAndroidTabsUpdateFunction() = default;

 protected:
  ~DesktopAndroidTabsUpdateFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidTabsReloadFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabs.reload", TABS_RELOAD)
  DesktopAndroidTabsReloadFunction() = default;

 protected:
  ~DesktopAndroidTabsReloadFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidTabsDuplicateFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabs.duplicate", TABS_DUPLICATE)
  DesktopAndroidTabsDuplicateFunction() = default;

 protected:
  ~DesktopAndroidTabsDuplicateFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidTabsHighlightFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabs.highlight", TABS_HIGHLIGHT)
  DesktopAndroidTabsHighlightFunction() = default;

 protected:
  ~DesktopAndroidTabsHighlightFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidTabsDetectLanguageFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabs.detectLanguage", TABS_DETECTLANGUAGE)
  DesktopAndroidTabsDetectLanguageFunction() = default;

 protected:
  ~DesktopAndroidTabsDetectLanguageFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidTabsDiscardFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabs.discard", TABS_DISCARD)
  DesktopAndroidTabsDiscardFunction() = default;

 protected:
  ~DesktopAndroidTabsDiscardFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidTabsGoBackFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabs.goBack", TABS_GOBACK)
  DesktopAndroidTabsGoBackFunction() = default;

 protected:
  ~DesktopAndroidTabsGoBackFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidTabsGoForwardFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabs.goForward", TABS_GOFORWARD)
  DesktopAndroidTabsGoForwardFunction() = default;

 protected:
  ~DesktopAndroidTabsGoForwardFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidTabsGroupFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabs.group", TABS_GROUP)
  DesktopAndroidTabsGroupFunction() = default;

 protected:
  ~DesktopAndroidTabsGroupFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidTabsUngroupFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabs.ungroup", TABS_UNGROUP)
  DesktopAndroidTabsUngroupFunction() = default;

 protected:
  ~DesktopAndroidTabsUngroupFunction() override = default;
  ResponseAction Run() override;
};

// ---- windows --------------------------------------------------------------

class DesktopAndroidWindowsGetAllFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("windows.getAll", WINDOWS_GETALL)
  DesktopAndroidWindowsGetAllFunction() = default;

 protected:
  ~DesktopAndroidWindowsGetAllFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidWindowsGetFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("windows.get", WINDOWS_GET)
  DesktopAndroidWindowsGetFunction() = default;

 protected:
  ~DesktopAndroidWindowsGetFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidWindowsGetCurrentFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("windows.getCurrent", WINDOWS_GETCURRENT)
  DesktopAndroidWindowsGetCurrentFunction() = default;

 protected:
  ~DesktopAndroidWindowsGetCurrentFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidWindowsGetLastFocusedFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("windows.getLastFocused", WINDOWS_GETLASTFOCUSED)
  DesktopAndroidWindowsGetLastFocusedFunction() = default;

 protected:
  ~DesktopAndroidWindowsGetLastFocusedFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidWindowsCreateFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("windows.create", WINDOWS_CREATE)
  DesktopAndroidWindowsCreateFunction() = default;

 protected:
  ~DesktopAndroidWindowsCreateFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidWindowsUpdateFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("windows.update", WINDOWS_UPDATE)
  DesktopAndroidWindowsUpdateFunction() = default;

 protected:
  ~DesktopAndroidWindowsUpdateFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidWindowsRemoveFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("windows.remove", WINDOWS_REMOVE)
  DesktopAndroidWindowsRemoveFunction() = default;

 protected:
  ~DesktopAndroidWindowsRemoveFunction() override = default;
  ResponseAction Run() override;
};

// ---- action (MV3) ---------------------------------------------------------

class DesktopAndroidActionSetIconFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("action.setIcon", ACTION_SETICON)
  DesktopAndroidActionSetIconFunction() = default;

 protected:
  ~DesktopAndroidActionSetIconFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidActionSetTitleFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("action.setTitle", ACTION_SETTITLE)
  DesktopAndroidActionSetTitleFunction() = default;

 protected:
  ~DesktopAndroidActionSetTitleFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidActionGetTitleFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("action.getTitle", ACTION_GETTITLE)
  DesktopAndroidActionGetTitleFunction() = default;

 protected:
  ~DesktopAndroidActionGetTitleFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidActionSetBadgeTextFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("action.setBadgeText", ACTION_SETBADGETEXT)
  DesktopAndroidActionSetBadgeTextFunction() = default;

 protected:
  ~DesktopAndroidActionSetBadgeTextFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidActionGetBadgeTextFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("action.getBadgeText", ACTION_GETBADGETEXT)
  DesktopAndroidActionGetBadgeTextFunction() = default;

 protected:
  ~DesktopAndroidActionGetBadgeTextFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidActionSetBadgeBackgroundColorFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("action.setBadgeBackgroundColor",
                             ACTION_SETBADGEBACKGROUNDCOLOR)
  DesktopAndroidActionSetBadgeBackgroundColorFunction() = default;

 protected:
  ~DesktopAndroidActionSetBadgeBackgroundColorFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidActionGetBadgeBackgroundColorFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("action.getBadgeBackgroundColor",
                             ACTION_GETBADGEBACKGROUNDCOLOR)
  DesktopAndroidActionGetBadgeBackgroundColorFunction() = default;

 protected:
  ~DesktopAndroidActionGetBadgeBackgroundColorFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidActionSetPopupFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("action.setPopup", ACTION_SETPOPUP)
  DesktopAndroidActionSetPopupFunction() = default;

 protected:
  ~DesktopAndroidActionSetPopupFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidActionGetPopupFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("action.getPopup", ACTION_GETPOPUP)
  DesktopAndroidActionGetPopupFunction() = default;

 protected:
  ~DesktopAndroidActionGetPopupFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidActionEnableFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("action.enable", ACTION_ENABLE)
  DesktopAndroidActionEnableFunction() = default;

 protected:
  ~DesktopAndroidActionEnableFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidActionDisableFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("action.disable", ACTION_DISABLE)
  DesktopAndroidActionDisableFunction() = default;

 protected:
  ~DesktopAndroidActionDisableFunction() override = default;
  ResponseAction Run() override;
};

// ---- browserAction (MV2) --------------------------------------------------

class DesktopAndroidBrowserActionSetIconFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.setIcon", BROWSERACTION_SETICON)
  DesktopAndroidBrowserActionSetIconFunction() = default;

 protected:
  ~DesktopAndroidBrowserActionSetIconFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidBrowserActionSetTitleFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.setTitle", BROWSERACTION_SETTITLE)
  DesktopAndroidBrowserActionSetTitleFunction() = default;

 protected:
  ~DesktopAndroidBrowserActionSetTitleFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidBrowserActionGetTitleFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.getTitle", BROWSERACTION_GETTITLE)
  DesktopAndroidBrowserActionGetTitleFunction() = default;

 protected:
  ~DesktopAndroidBrowserActionGetTitleFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidBrowserActionSetBadgeTextFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.setBadgeText",
                             BROWSERACTION_SETBADGETEXT)
  DesktopAndroidBrowserActionSetBadgeTextFunction() = default;

 protected:
  ~DesktopAndroidBrowserActionSetBadgeTextFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidBrowserActionGetBadgeTextFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.getBadgeText",
                             BROWSERACTION_GETBADGETEXT)
  DesktopAndroidBrowserActionGetBadgeTextFunction() = default;

 protected:
  ~DesktopAndroidBrowserActionGetBadgeTextFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidBrowserActionSetBadgeBackgroundColorFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.setBadgeBackgroundColor",
                             BROWSERACTION_SETBADGEBACKGROUNDCOLOR)
  DesktopAndroidBrowserActionSetBadgeBackgroundColorFunction() = default;

 protected:
  ~DesktopAndroidBrowserActionSetBadgeBackgroundColorFunction() override =
      default;
  ResponseAction Run() override;
};

class DesktopAndroidBrowserActionGetBadgeBackgroundColorFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.getBadgeBackgroundColor",
                             BROWSERACTION_GETBADGEBACKGROUNDCOLOR)
  DesktopAndroidBrowserActionGetBadgeBackgroundColorFunction() = default;

 protected:
  ~DesktopAndroidBrowserActionGetBadgeBackgroundColorFunction() override =
      default;
  ResponseAction Run() override;
};

class DesktopAndroidBrowserActionSetPopupFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.setPopup", BROWSERACTION_SETPOPUP)
  DesktopAndroidBrowserActionSetPopupFunction() = default;

 protected:
  ~DesktopAndroidBrowserActionSetPopupFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidBrowserActionGetPopupFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.getPopup", BROWSERACTION_GETPOPUP)
  DesktopAndroidBrowserActionGetPopupFunction() = default;

 protected:
  ~DesktopAndroidBrowserActionGetPopupFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidBrowserActionEnableFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.enable", BROWSERACTION_ENABLE)
  DesktopAndroidBrowserActionEnableFunction() = default;

 protected:
  ~DesktopAndroidBrowserActionEnableFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidBrowserActionDisableFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.disable", BROWSERACTION_DISABLE)
  DesktopAndroidBrowserActionDisableFunction() = default;

 protected:
  ~DesktopAndroidBrowserActionDisableFunction() override = default;
  ResponseAction Run() override;
};

// ---- contextMenus ---------------------------------------------------------

class DesktopAndroidContextMenusCreateFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contextMenus.create", CONTEXTMENUS_CREATE)
  DesktopAndroidContextMenusCreateFunction() = default;

 protected:
  ~DesktopAndroidContextMenusCreateFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidContextMenusUpdateFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contextMenus.update", CONTEXTMENUS_UPDATE)
  DesktopAndroidContextMenusUpdateFunction() = default;

 protected:
  ~DesktopAndroidContextMenusUpdateFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidContextMenusRemoveFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contextMenus.remove", CONTEXTMENUS_REMOVE)
  DesktopAndroidContextMenusRemoveFunction() = default;

 protected:
  ~DesktopAndroidContextMenusRemoveFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidContextMenusRemoveAllFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contextMenus.removeAll", CONTEXTMENUS_REMOVEALL)
  DesktopAndroidContextMenusRemoveAllFunction() = default;

 protected:
  ~DesktopAndroidContextMenusRemoveAllFunction() override = default;
  ResponseAction Run() override;
};

// ---- cookies --------------------------------------------------------------

class DesktopAndroidCookiesGetFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("cookies.get", COOKIES_GET)
  DesktopAndroidCookiesGetFunction() = default;

 protected:
  ~DesktopAndroidCookiesGetFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidCookiesGetAllFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("cookies.getAll", COOKIES_GETALL)
  DesktopAndroidCookiesGetAllFunction() = default;

 protected:
  ~DesktopAndroidCookiesGetAllFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidCookiesSetFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("cookies.set", COOKIES_SET)
  DesktopAndroidCookiesSetFunction() = default;

 protected:
  ~DesktopAndroidCookiesSetFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidCookiesRemoveFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("cookies.remove", COOKIES_REMOVE)
  DesktopAndroidCookiesRemoveFunction() = default;

 protected:
  ~DesktopAndroidCookiesRemoveFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidCookiesGetAllCookieStoresFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("cookies.getAllCookieStores",
                             COOKIES_GETALLCOOKIESTORES)
  DesktopAndroidCookiesGetAllCookieStoresFunction() = default;

 protected:
  ~DesktopAndroidCookiesGetAllCookieStoresFunction() override = default;
  ResponseAction Run() override;
};

// ---- types.ChromeSetting (privacy, etc. rely on it) -----------------------
//
// Extensions call `chrome.privacy.network.networkPredictionEnabled.set({...})`
// etc. The generated JS bindings dispatch to `types.ChromeSetting.set`.
// We stub the three accessors so those chains don't reject.

class DesktopAndroidTypesChromeSettingGetFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("types.ChromeSetting.get", TYPES_CHROMESETTING_GET)
  DesktopAndroidTypesChromeSettingGetFunction() = default;

 protected:
  ~DesktopAndroidTypesChromeSettingGetFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidTypesChromeSettingSetFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("types.ChromeSetting.set", TYPES_CHROMESETTING_SET)
  DesktopAndroidTypesChromeSettingSetFunction() = default;

 protected:
  ~DesktopAndroidTypesChromeSettingSetFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidTypesChromeSettingClearFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("types.ChromeSetting.clear",
                             TYPES_CHROMESETTING_CLEAR)
  DesktopAndroidTypesChromeSettingClearFunction() = default;

 protected:
  ~DesktopAndroidTypesChromeSettingClearFunction() override = default;
  ResponseAction Run() override;
};

// ---- extension (MV2 shim) -------------------------------------------------

class DesktopAndroidExtensionIsAllowedIncognitoAccessFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("extension.isAllowedIncognitoAccess",
                             EXTENSION_ISALLOWEDINCOGNITOACCESS)
  DesktopAndroidExtensionIsAllowedIncognitoAccessFunction() = default;

 protected:
  ~DesktopAndroidExtensionIsAllowedIncognitoAccessFunction() override =
      default;
  ResponseAction Run() override;
};

class DesktopAndroidExtensionIsAllowedFileSchemeAccessFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("extension.isAllowedFileSchemeAccess",
                             EXTENSION_ISALLOWEDFILESCHEMEACCESS)
  DesktopAndroidExtensionIsAllowedFileSchemeAccessFunction() = default;

 protected:
  ~DesktopAndroidExtensionIsAllowedFileSchemeAccessFunction() override =
      default;
  ResponseAction Run() override;
};

// ---- scripting (shape-only) -----------------------------------------------
//
// uBlock Origin calls insertCSS during its cosmetic-filter bootstrap; if it
// errors the bootstrap short-circuits before the network-filter engine
// compiles. Returning NoArguments() lets uBO proceed. Cosmetic filtering
// still won't apply because no CSS is injected, but network-filter blocking
// via webRequest begins to flow.

class DesktopAndroidScriptingInsertCSSFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("scripting.insertCSS", SCRIPTING_INSERTCSS)
  DesktopAndroidScriptingInsertCSSFunction() = default;

 protected:
  ~DesktopAndroidScriptingInsertCSSFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidScriptingRemoveCSSFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("scripting.removeCSS", SCRIPTING_REMOVECSS)
  DesktopAndroidScriptingRemoveCSSFunction() = default;

 protected:
  ~DesktopAndroidScriptingRemoveCSSFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidScriptingExecuteScriptFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("scripting.executeScript", SCRIPTING_EXECUTESCRIPT)
  DesktopAndroidScriptingExecuteScriptFunction() = default;

 protected:
  ~DesktopAndroidScriptingExecuteScriptFunction() override = default;
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_DESKTOP_ANDROID_DESKTOP_ANDROID_STUB_APIS_H_
