// Copyright 2026 The Afterbird Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/desktop_android/desktop_android_stub_apis.h"

#include <string>
#include <string_view>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"

namespace extensions {

namespace {

// Reads the manifest's `permissions` array and splits it into the API-style
// "permissions" (plain strings) and "origins" (anything that looks like a
// match pattern / URL). `host_permissions` is appended to origins too
// (MV3). Returns a fresh { permissions: [], origins: [] } dict either way.
base::Value::Dict BuildPermissionsDict(const Extension& extension) {
  base::Value::List permissions;
  base::Value::List origins;

  auto append_entry = [&](const std::string& s) {
    // A heuristic: if it contains '://' or starts with '*:' it's a match
    // pattern; otherwise it's an API permission token.
    if (s.find("://") != std::string::npos || s.starts_with("*://") ||
        s == "<all_urls>") {
      origins.Append(s);
    } else {
      permissions.Append(s);
    }
  };

  if (const base::Value* perms =
          extension.manifest()->FindKey("permissions")) {
    if (perms->is_list()) {
      for (const base::Value& v : perms->GetList()) {
        if (v.is_string()) {
          append_entry(v.GetString());
        }
      }
    }
  }
  // MV3 host_permissions — treated purely as origins.
  if (const base::Value* hosts =
          extension.manifest()->FindKey("host_permissions")) {
    if (hosts->is_list()) {
      for (const base::Value& v : hosts->GetList()) {
        if (v.is_string()) {
          origins.Append(v.GetString());
        }
      }
    }
  }

  base::Value::Dict dict;
  dict.Set("permissions", std::move(permissions));
  dict.Set("origins", std::move(origins));
  return dict;
}

// Common helpers that build a single-argument Value::List. The caller wraps
// it with ExtensionFunction::ArgumentList(...) inside its own Run() — that
// method is protected so it can't be called from this namespace.
base::Value::List OneArgList(base::Value v) {
  base::Value::List args;
  args.Append(std::move(v));
  return args;
}

base::Value::List EmptyArrayArgList() {
  return OneArgList(base::Value(base::Value::List()));
}

base::Value::List NullArgList() {
  return OneArgList(base::Value());
}

}  // namespace

// ----------------------------------------------------------------------------
// permissions
// ----------------------------------------------------------------------------

ExtensionFunction::ResponseAction
DesktopAndroidPermissionsGetAllFunction::Run() {
  const Extension* ext = extension();
  if (!ext) {
    base::Value::Dict empty;
    empty.Set("permissions", base::Value::List());
    empty.Set("origins", base::Value::List());
    return RespondNow(ArgumentList(OneArgList(base::Value(std::move(empty)))));
  }
  return RespondNow(ArgumentList(
      OneArgList(base::Value(BuildPermissionsDict(*ext)))));
}

ExtensionFunction::ResponseAction
DesktopAndroidPermissionsContainsFunction::Run() {
  if (args().empty() || !args()[0].is_dict()) {
    return RespondNow(ArgumentList(OneArgList(base::Value(false))));
  }

  const Extension* ext = extension();
  base::Value::Dict declared =
      ext ? BuildPermissionsDict(*ext) : base::Value::Dict();
  const base::Value::List* declared_perms = declared.FindList("permissions");
  const base::Value::List* declared_origins = declared.FindList("origins");

  auto list_contains = [](const base::Value::List* haystack,
                          const std::string& needle) {
    if (!haystack) {
      return false;
    }
    for (const base::Value& v : *haystack) {
      if (v.is_string() && v.GetString() == needle) {
        return true;
      }
    }
    return false;
  };

  const base::Value::Dict& req = args()[0].GetDict();
  bool ok = true;
  if (const base::Value::List* rp = req.FindList("permissions")) {
    for (const base::Value& v : *rp) {
      if (!v.is_string() || !list_contains(declared_perms, v.GetString())) {
        ok = false;
        break;
      }
    }
  }
  if (ok) {
    if (const base::Value::List* ro = req.FindList("origins")) {
      for (const base::Value& v : *ro) {
        if (!v.is_string() || !list_contains(declared_origins, v.GetString())) {
          ok = false;
          break;
        }
      }
    }
  }
  return RespondNow(ArgumentList(OneArgList(base::Value(ok))));
}

// ----------------------------------------------------------------------------
// commands
// ----------------------------------------------------------------------------

ExtensionFunction::ResponseAction DesktopAndroidCommandsGetAllFunction::Run() {
  // Upstream shape is an array of Command dicts:
  //   { name, description, shortcut, global }
  base::Value::List out;
  const Extension* ext = extension();
  if (ext) {
    const base::Value::Dict* cmds =
        ext->manifest()->FindDictPath("commands");
    if (cmds) {
      for (const auto [name, value] : *cmds) {
        if (!value.is_dict()) {
          continue;
        }
        const base::Value::Dict& d = value.GetDict();
        base::Value::Dict entry;
        entry.Set("name", name);
        const std::string* desc = d.FindString("description");
        entry.Set("description", desc ? *desc : std::string());
        entry.Set("shortcut", std::string());
        entry.Set("global", d.FindBool("global").value_or(false));
        out.Append(std::move(entry));
      }
    }
  }
  return RespondNow(ArgumentList(OneArgList(base::Value(std::move(out)))));
}

// ----------------------------------------------------------------------------
// notifications
// ----------------------------------------------------------------------------

ExtensionFunction::ResponseAction
DesktopAndroidNotificationsCreateFunction::Run() {
  std::string id;
  if (!args().empty() && args()[0].is_string()) {
    id = args()[0].GetString();
  }
  if (id.empty()) {
    id = base::StringPrintf(
        "stub-%lld",
        static_cast<long long>(
            base::Time::Now().InMillisecondsSinceUnixEpoch()));
  }
  LOG(INFO) << "[afterbird] notifications.create stub: id=" << id
            << " (no visible toast — NotificationDisplayService not wired on "
               "desktop-android)";
  return RespondNow(ArgumentList(OneArgList(base::Value(id))));
}

ExtensionFunction::ResponseAction
DesktopAndroidNotificationsUpdateFunction::Run() {
  // API: notifications.update(string id, NotificationOptions, callback(bool))
  // Return true so callers that wait on the bool don't treat it as an error.
  return RespondNow(ArgumentList(OneArgList(base::Value(true))));
}

ExtensionFunction::ResponseAction
DesktopAndroidNotificationsClearFunction::Run() {
  // API: notifications.clear(string id, callback(bool wasCleared))
  return RespondNow(ArgumentList(OneArgList(base::Value(true))));
}

ExtensionFunction::ResponseAction
DesktopAndroidNotificationsGetAllFunction::Run() {
  // API: notifications.getAll(callback(object notifications))
  // No active notifications on desktop-android; return an empty dict.
  return RespondNow(ArgumentList(OneArgList(base::Value(base::Value::Dict()))));
}

ExtensionFunction::ResponseAction
DesktopAndroidNotificationsGetPermissionLevelFunction::Run() {
  // API: notifications.getPermissionLevel(callback(PermissionLevel level))
  // "granted" matches what a normal Chrome profile reports, so extensions
  // don't short-circuit on a "denied" reading and skip their setup.
  return RespondNow(ArgumentList(OneArgList(base::Value("granted"))));
}

// ----------------------------------------------------------------------------
// tabs — shape-only stubs. Most return empty / synthetic values; see
// desktop_android_extension_web_contents_observer if a real hookup lands.
// ----------------------------------------------------------------------------

namespace {

// Synthesises a minimal Tab dict. Field names match what tabs.json declares
// as "Tab" so the bindings layer's post-processing doesn't choke.
base::Value::Dict MakeStubTab(int id) {
  base::Value::Dict t;
  t.Set("id", id);
  t.Set("index", 0);
  t.Set("windowId", 0);
  t.Set("highlighted", false);
  t.Set("active", false);
  t.Set("pinned", false);
  t.Set("audible", false);
  t.Set("autoDiscardable", true);
  t.Set("discarded", false);
  t.Set("incognito", false);
  t.Set("url", std::string());
  t.Set("title", std::string());
  t.Set("status", "complete");
  t.Set("selected", false);
  t.Set("groupId", -1);
  return t;
}

}  // namespace

ExtensionFunction::ResponseAction DesktopAndroidTabsQueryFunction::Run() {
  // Always return an empty array. Bitwarden's BadgeService does
  //   tabs.query(...).then(tabs => tabs.filter(...))
  // and pre-fix this returned `undefined`, crashing the SW with
  // "Cannot read properties of undefined (reading 'filter')". An empty array
  // makes filter/map/forEach no-op cleanly.
  return RespondNow(ArgumentList(EmptyArrayArgList()));
}

ExtensionFunction::ResponseAction DesktopAndroidTabsGetFunction::Run() {
  // Fail with a clear error — a callback receiving undefined is worse than
  // an error. Extensions that check lastError handle this gracefully.
  return RespondNow(Error("Tab not found on desktop-android (stub)"));
}

ExtensionFunction::ResponseAction DesktopAndroidTabsGetCurrentFunction::Run() {
  // Per spec, returns undefined if called from a non-tab context; that's the
  // state that most closely mirrors running from a service worker.
  base::Value::List args;
  args.Append(base::Value());
  return RespondNow(ArgumentList(std::move(args)));
}

ExtensionFunction::ResponseAction DesktopAndroidTabsCreateFunction::Run() {
  // Pretend to create a tab and return a synthetic Tab dict. The id is a
  // monotonically-unique-enough millisecond timestamp so subsequent calls
  // referencing it still won't match any real tab — but at least the callback
  // fires with the right shape.
  int id = static_cast<int>(
      base::Time::Now().InMillisecondsSinceUnixEpoch() & 0x7FFFFFFF);
  return RespondNow(ArgumentList(OneArgList(base::Value(MakeStubTab(id)))));
}

ExtensionFunction::ResponseAction DesktopAndroidTabsRemoveFunction::Run() {
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction DesktopAndroidTabsUpdateFunction::Run() {
  int id = 0;
  if (!args().empty() && args()[0].is_int()) {
    id = args()[0].GetInt();
  }
  return RespondNow(ArgumentList(OneArgList(base::Value(MakeStubTab(id)))));
}

ExtensionFunction::ResponseAction DesktopAndroidTabsReloadFunction::Run() {
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction DesktopAndroidTabsDuplicateFunction::Run() {
  int id = 0;
  if (!args().empty() && args()[0].is_int()) {
    id = args()[0].GetInt();
  }
  return RespondNow(ArgumentList(OneArgList(base::Value(MakeStubTab(id + 1)))));
}

ExtensionFunction::ResponseAction DesktopAndroidTabsHighlightFunction::Run() {
  // Schema: callback(windows.Window). Return a plausible empty window dict.
  base::Value::Dict w;
  w.Set("id", 0);
  w.Set("focused", true);
  w.Set("incognito", false);
  w.Set("alwaysOnTop", false);
  w.Set("type", "normal");
  w.Set("state", "normal");
  base::Value::List tabs;
  w.Set("tabs", std::move(tabs));
  return RespondNow(ArgumentList(OneArgList(base::Value(std::move(w)))));
}

ExtensionFunction::ResponseAction
DesktopAndroidTabsDetectLanguageFunction::Run() {
  // Schema: callback(string language). ISO-639-1 "und" = undetermined.
  return RespondNow(ArgumentList(OneArgList(base::Value("und"))));
}

ExtensionFunction::ResponseAction DesktopAndroidTabsDiscardFunction::Run() {
  // Schema: callback(Tab discardedTab).
  base::Value::List args_out;
  args_out.Append(base::Value());  // undefined/null
  return RespondNow(ArgumentList(std::move(args_out)));
}

ExtensionFunction::ResponseAction DesktopAndroidTabsGoBackFunction::Run() {
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction DesktopAndroidTabsGoForwardFunction::Run() {
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction DesktopAndroidTabsGroupFunction::Run() {
  // Schema: callback(integer groupId).
  return RespondNow(ArgumentList(OneArgList(base::Value(-1))));
}

ExtensionFunction::ResponseAction DesktopAndroidTabsUngroupFunction::Run() {
  return RespondNow(NoArguments());
}

// ----------------------------------------------------------------------------
// windows — synthetic single-window world
// ----------------------------------------------------------------------------

namespace {

base::Value::Dict MakeStubWindow() {
  base::Value::Dict w;
  w.Set("id", 0);
  w.Set("focused", true);
  w.Set("incognito", false);
  w.Set("alwaysOnTop", false);
  w.Set("type", "normal");
  w.Set("state", "normal");
  w.Set("top", 0);
  w.Set("left", 0);
  w.Set("width", 0);
  w.Set("height", 0);
  return w;
}

}  // namespace

ExtensionFunction::ResponseAction DesktopAndroidWindowsGetAllFunction::Run() {
  base::Value::List out;
  out.Append(MakeStubWindow());
  return RespondNow(ArgumentList(OneArgList(base::Value(std::move(out)))));
}

ExtensionFunction::ResponseAction DesktopAndroidWindowsGetFunction::Run() {
  return RespondNow(ArgumentList(OneArgList(base::Value(MakeStubWindow()))));
}

ExtensionFunction::ResponseAction
DesktopAndroidWindowsGetCurrentFunction::Run() {
  return RespondNow(ArgumentList(OneArgList(base::Value(MakeStubWindow()))));
}

ExtensionFunction::ResponseAction
DesktopAndroidWindowsGetLastFocusedFunction::Run() {
  return RespondNow(ArgumentList(OneArgList(base::Value(MakeStubWindow()))));
}

ExtensionFunction::ResponseAction DesktopAndroidWindowsCreateFunction::Run() {
  return RespondNow(ArgumentList(OneArgList(base::Value(MakeStubWindow()))));
}

ExtensionFunction::ResponseAction DesktopAndroidWindowsUpdateFunction::Run() {
  return RespondNow(ArgumentList(OneArgList(base::Value(MakeStubWindow()))));
}

ExtensionFunction::ResponseAction DesktopAndroidWindowsRemoveFunction::Run() {
  return RespondNow(NoArguments());
}

// ----------------------------------------------------------------------------
// action (MV3) / browserAction (MV2) — no-op setters, plausible getters
// ----------------------------------------------------------------------------

// All setX() return no arguments; all getX() return the caller's default. We
// don't store state per-tab/per-extension yet — that would require a real
// ExtensionActionRuntime hookup.

ExtensionFunction::ResponseAction DesktopAndroidActionSetIconFunction::Run() {
  return RespondNow(NoArguments());
}
ExtensionFunction::ResponseAction DesktopAndroidActionSetTitleFunction::Run() {
  return RespondNow(NoArguments());
}
ExtensionFunction::ResponseAction DesktopAndroidActionGetTitleFunction::Run() {
  return RespondNow(ArgumentList(OneArgList(base::Value(std::string()))));
}
ExtensionFunction::ResponseAction
DesktopAndroidActionSetBadgeTextFunction::Run() {
  return RespondNow(NoArguments());
}
ExtensionFunction::ResponseAction
DesktopAndroidActionGetBadgeTextFunction::Run() {
  return RespondNow(ArgumentList(OneArgList(base::Value(std::string()))));
}
ExtensionFunction::ResponseAction
DesktopAndroidActionSetBadgeBackgroundColorFunction::Run() {
  return RespondNow(NoArguments());
}
ExtensionFunction::ResponseAction
DesktopAndroidActionGetBadgeBackgroundColorFunction::Run() {
  // Returns ColorArray [r,g,b,a] 0-255. Default to opaque black.
  base::Value::List color;
  color.Append(0);
  color.Append(0);
  color.Append(0);
  color.Append(255);
  return RespondNow(ArgumentList(OneArgList(base::Value(std::move(color)))));
}
ExtensionFunction::ResponseAction DesktopAndroidActionSetPopupFunction::Run() {
  return RespondNow(NoArguments());
}
ExtensionFunction::ResponseAction DesktopAndroidActionGetPopupFunction::Run() {
  return RespondNow(ArgumentList(OneArgList(base::Value(std::string()))));
}
ExtensionFunction::ResponseAction DesktopAndroidActionEnableFunction::Run() {
  return RespondNow(NoArguments());
}
ExtensionFunction::ResponseAction DesktopAndroidActionDisableFunction::Run() {
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
DesktopAndroidBrowserActionSetIconFunction::Run() {
  return RespondNow(NoArguments());
}
ExtensionFunction::ResponseAction
DesktopAndroidBrowserActionSetTitleFunction::Run() {
  return RespondNow(NoArguments());
}
ExtensionFunction::ResponseAction
DesktopAndroidBrowserActionGetTitleFunction::Run() {
  return RespondNow(ArgumentList(OneArgList(base::Value(std::string()))));
}
ExtensionFunction::ResponseAction
DesktopAndroidBrowserActionSetBadgeTextFunction::Run() {
  return RespondNow(NoArguments());
}
ExtensionFunction::ResponseAction
DesktopAndroidBrowserActionGetBadgeTextFunction::Run() {
  return RespondNow(ArgumentList(OneArgList(base::Value(std::string()))));
}
ExtensionFunction::ResponseAction
DesktopAndroidBrowserActionSetBadgeBackgroundColorFunction::Run() {
  return RespondNow(NoArguments());
}
ExtensionFunction::ResponseAction
DesktopAndroidBrowserActionGetBadgeBackgroundColorFunction::Run() {
  base::Value::List color;
  color.Append(0);
  color.Append(0);
  color.Append(0);
  color.Append(255);
  return RespondNow(ArgumentList(OneArgList(base::Value(std::move(color)))));
}
ExtensionFunction::ResponseAction
DesktopAndroidBrowserActionSetPopupFunction::Run() {
  return RespondNow(NoArguments());
}
ExtensionFunction::ResponseAction
DesktopAndroidBrowserActionGetPopupFunction::Run() {
  return RespondNow(ArgumentList(OneArgList(base::Value(std::string()))));
}
ExtensionFunction::ResponseAction
DesktopAndroidBrowserActionEnableFunction::Run() {
  return RespondNow(NoArguments());
}
ExtensionFunction::ResponseAction
DesktopAndroidBrowserActionDisableFunction::Run() {
  return RespondNow(NoArguments());
}

// ----------------------------------------------------------------------------
// contextMenus — registration-only, no actual menu routing on Android
// ----------------------------------------------------------------------------
//
// Extensions call contextMenus.create(...) to register click handlers; since
// desktop-android has no contextual-menu surface on pages, we just accept
// the registration and return the id. No onClicked events will ever fire.

ExtensionFunction::ResponseAction
DesktopAndroidContextMenusCreateFunction::Run() {
  // Upstream ContextMenusCreateFunction returns NoArguments() — the JS
  // binding in extensions/renderer/resources/context_menus_handlers.js
  // derives the id from createProperties.id or .generatedId on the
  // request side, so the function itself doesn't need to return one.
  // Returning an id here confused the custom-callback chain and produced
  // "extensionCallback is not a function" errors in the renderer.
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
DesktopAndroidContextMenusUpdateFunction::Run() {
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
DesktopAndroidContextMenusRemoveFunction::Run() {
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
DesktopAndroidContextMenusRemoveAllFunction::Run() {
  return RespondNow(NoArguments());
}

// ----------------------------------------------------------------------------
// cookies — empty / null returns. See api-infeasible.md for the plumbing
// required to bridge the real CookieManager.
// ----------------------------------------------------------------------------

ExtensionFunction::ResponseAction DesktopAndroidCookiesGetFunction::Run() {
  // Schema: callback(Cookie? cookie). Null = not found.
  base::Value::List args_out;
  args_out.Append(base::Value());
  return RespondNow(ArgumentList(std::move(args_out)));
}

ExtensionFunction::ResponseAction DesktopAndroidCookiesGetAllFunction::Run() {
  return RespondNow(ArgumentList(EmptyArrayArgList()));
}

ExtensionFunction::ResponseAction DesktopAndroidCookiesSetFunction::Run() {
  // Schema: optional callback(Cookie? cookie). Return null — extensions that
  // check lastError will see a blank lastError (no error) but null cookie.
  base::Value::List args_out;
  args_out.Append(base::Value());
  return RespondNow(ArgumentList(std::move(args_out)));
}

ExtensionFunction::ResponseAction DesktopAndroidCookiesRemoveFunction::Run() {
  // Schema: optional callback(object? details).
  base::Value::List args_out;
  args_out.Append(base::Value());
  return RespondNow(ArgumentList(std::move(args_out)));
}

ExtensionFunction::ResponseAction
DesktopAndroidCookiesGetAllCookieStoresFunction::Run() {
  // Schema: callback(CookieStore[] cookieStores). Return a single entry so
  // extensions that iterate stores (uBO, Honey) have something to work with.
  base::Value::Dict store;
  store.Set("id", "0");
  base::Value::List tab_ids;
  store.Set("tabIds", std::move(tab_ids));
  base::Value::List stores;
  stores.Append(std::move(store));
  return RespondNow(ArgumentList(OneArgList(base::Value(std::move(stores)))));
}

// ----------------------------------------------------------------------------
// types.ChromeSetting — privacy.* and similar all route through here.
// ----------------------------------------------------------------------------

ExtensionFunction::ResponseAction
DesktopAndroidTypesChromeSettingGetFunction::Run() {
  // Schema: callback({ value, levelOfControl, incognitoSpecific? })
  base::Value::Dict d;
  d.Set("value", base::Value());  // null
  d.Set("levelOfControl", "not_controllable");
  return RespondNow(ArgumentList(OneArgList(base::Value(std::move(d)))));
}

ExtensionFunction::ResponseAction
DesktopAndroidTypesChromeSettingSetFunction::Run() {
  // Schema: callback() — returns nothing.
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
DesktopAndroidTypesChromeSettingClearFunction::Run() {
  return RespondNow(NoArguments());
}

// ----------------------------------------------------------------------------
// extension (MV2 shim)
// ----------------------------------------------------------------------------

ExtensionFunction::ResponseAction
DesktopAndroidExtensionIsAllowedIncognitoAccessFunction::Run() {
  // Desktop-android doesn't expose the extension "Allow in incognito" toggle;
  // reporting false matches the de facto state.
  return RespondNow(ArgumentList(OneArgList(base::Value(false))));
}

ExtensionFunction::ResponseAction
DesktopAndroidExtensionIsAllowedFileSchemeAccessFunction::Run() {
  return RespondNow(ArgumentList(OneArgList(base::Value(false))));
}

// ----------------------------------------------------------------------------
// scripting — shape-only.
//
// insertCSS: return `{}` so the callback gets a truthy result. uBO gates
// further work on a non-error response here.
// removeCSS / executeScript: NoArguments() success.
// ----------------------------------------------------------------------------

ExtensionFunction::ResponseAction
DesktopAndroidScriptingInsertCSSFunction::Run() {
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
DesktopAndroidScriptingRemoveCSSFunction::Run() {
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
DesktopAndroidScriptingExecuteScriptFunction::Run() {
  // callback(injectionResult[]): return an empty array so uBO's `.map`
  // over the results doesn't blow up.
  base::Value::List results;
  return RespondNow(ArgumentList(OneArgList(base::Value(std::move(results)))));
}

// ----------------------------------------------------------------------------

void RegisterDesktopAndroidStubApiFunctions(
    ExtensionFunctionRegistry* registry) {
  // permissions
  registry->RegisterFunction<DesktopAndroidPermissionsGetAllFunction>();
  registry->RegisterFunction<DesktopAndroidPermissionsContainsFunction>();
  // commands
  registry->RegisterFunction<DesktopAndroidCommandsGetAllFunction>();
  // notifications
  registry->RegisterFunction<DesktopAndroidNotificationsCreateFunction>();
  registry->RegisterFunction<DesktopAndroidNotificationsUpdateFunction>();
  registry->RegisterFunction<DesktopAndroidNotificationsClearFunction>();
  registry->RegisterFunction<DesktopAndroidNotificationsGetAllFunction>();
  registry
      ->RegisterFunction<DesktopAndroidNotificationsGetPermissionLevelFunction>();
  // tabs
  registry->RegisterFunction<DesktopAndroidTabsQueryFunction>();
  registry->RegisterFunction<DesktopAndroidTabsGetFunction>();
  registry->RegisterFunction<DesktopAndroidTabsGetCurrentFunction>();
  registry->RegisterFunction<DesktopAndroidTabsCreateFunction>();
  registry->RegisterFunction<DesktopAndroidTabsRemoveFunction>();
  registry->RegisterFunction<DesktopAndroidTabsUpdateFunction>();
  registry->RegisterFunction<DesktopAndroidTabsReloadFunction>();
  registry->RegisterFunction<DesktopAndroidTabsDuplicateFunction>();
  registry->RegisterFunction<DesktopAndroidTabsHighlightFunction>();
  registry->RegisterFunction<DesktopAndroidTabsDetectLanguageFunction>();
  registry->RegisterFunction<DesktopAndroidTabsDiscardFunction>();
  registry->RegisterFunction<DesktopAndroidTabsGoBackFunction>();
  registry->RegisterFunction<DesktopAndroidTabsGoForwardFunction>();
  registry->RegisterFunction<DesktopAndroidTabsGroupFunction>();
  registry->RegisterFunction<DesktopAndroidTabsUngroupFunction>();
  // windows
  registry->RegisterFunction<DesktopAndroidWindowsGetAllFunction>();
  registry->RegisterFunction<DesktopAndroidWindowsGetFunction>();
  registry->RegisterFunction<DesktopAndroidWindowsGetCurrentFunction>();
  registry->RegisterFunction<DesktopAndroidWindowsGetLastFocusedFunction>();
  registry->RegisterFunction<DesktopAndroidWindowsCreateFunction>();
  registry->RegisterFunction<DesktopAndroidWindowsUpdateFunction>();
  registry->RegisterFunction<DesktopAndroidWindowsRemoveFunction>();
  // action
  registry->RegisterFunction<DesktopAndroidActionSetIconFunction>();
  registry->RegisterFunction<DesktopAndroidActionSetTitleFunction>();
  registry->RegisterFunction<DesktopAndroidActionGetTitleFunction>();
  registry->RegisterFunction<DesktopAndroidActionSetBadgeTextFunction>();
  registry->RegisterFunction<DesktopAndroidActionGetBadgeTextFunction>();
  registry
      ->RegisterFunction<DesktopAndroidActionSetBadgeBackgroundColorFunction>();
  registry
      ->RegisterFunction<DesktopAndroidActionGetBadgeBackgroundColorFunction>();
  registry->RegisterFunction<DesktopAndroidActionSetPopupFunction>();
  registry->RegisterFunction<DesktopAndroidActionGetPopupFunction>();
  registry->RegisterFunction<DesktopAndroidActionEnableFunction>();
  registry->RegisterFunction<DesktopAndroidActionDisableFunction>();
  // browserAction
  registry->RegisterFunction<DesktopAndroidBrowserActionSetIconFunction>();
  registry->RegisterFunction<DesktopAndroidBrowserActionSetTitleFunction>();
  registry->RegisterFunction<DesktopAndroidBrowserActionGetTitleFunction>();
  registry
      ->RegisterFunction<DesktopAndroidBrowserActionSetBadgeTextFunction>();
  registry
      ->RegisterFunction<DesktopAndroidBrowserActionGetBadgeTextFunction>();
  registry->RegisterFunction<
      DesktopAndroidBrowserActionSetBadgeBackgroundColorFunction>();
  registry->RegisterFunction<
      DesktopAndroidBrowserActionGetBadgeBackgroundColorFunction>();
  registry->RegisterFunction<DesktopAndroidBrowserActionSetPopupFunction>();
  registry->RegisterFunction<DesktopAndroidBrowserActionGetPopupFunction>();
  registry->RegisterFunction<DesktopAndroidBrowserActionEnableFunction>();
  registry->RegisterFunction<DesktopAndroidBrowserActionDisableFunction>();
  // contextMenus
  registry->RegisterFunction<DesktopAndroidContextMenusCreateFunction>();
  registry->RegisterFunction<DesktopAndroidContextMenusUpdateFunction>();
  registry->RegisterFunction<DesktopAndroidContextMenusRemoveFunction>();
  registry->RegisterFunction<DesktopAndroidContextMenusRemoveAllFunction>();
  // cookies
  registry->RegisterFunction<DesktopAndroidCookiesGetFunction>();
  registry->RegisterFunction<DesktopAndroidCookiesGetAllFunction>();
  registry->RegisterFunction<DesktopAndroidCookiesSetFunction>();
  registry->RegisterFunction<DesktopAndroidCookiesRemoveFunction>();
  registry
      ->RegisterFunction<DesktopAndroidCookiesGetAllCookieStoresFunction>();
  // types.ChromeSetting
  registry->RegisterFunction<DesktopAndroidTypesChromeSettingGetFunction>();
  registry->RegisterFunction<DesktopAndroidTypesChromeSettingSetFunction>();
  registry->RegisterFunction<DesktopAndroidTypesChromeSettingClearFunction>();
  // extension (MV2)
  registry->RegisterFunction<
      DesktopAndroidExtensionIsAllowedIncognitoAccessFunction>();
  registry->RegisterFunction<
      DesktopAndroidExtensionIsAllowedFileSchemeAccessFunction>();
  // scripting (MV3 — cosmetic-filter bootstrap needs it)
  registry->RegisterFunction<DesktopAndroidScriptingInsertCSSFunction>();
  registry->RegisterFunction<DesktopAndroidScriptingRemoveCSSFunction>();
  registry->RegisterFunction<DesktopAndroidScriptingExecuteScriptFunction>();
}

}  // namespace extensions
