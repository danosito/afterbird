// Copyright 2026 The Afterbird Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Thin, best-effort stubs for chrome.* extension functions that real-world
// extensions (uBlock Origin, Dark Reader, etc.) call defensively on startup.
// Each stub produces a plausible empty / success response so the JS-side
// binding promise resolves instead of throwing "Access to extension API
// denied." — which in MV3 service workers often crashes the worker before
// the real setup code runs.
//
// These are NOT replacements for the real APIs. They're specifically:
//
//   * `chrome.permissions.getAll`  — returns { permissions: [...], origins: [...] }
//     reconstructed from the extension's manifest permissions. Enough for
//     extensions that feature-detect via "does this permission exist in my
//     active set?" — they'll find what they declared.
//
//   * `chrome.permissions.contains` — true iff the requested {permissions,
//     origins} subset appears in the declared manifest permissions.
//
//   * `chrome.commands.getAll`     — returns the manifest `commands` dict
//     transcribed into the shape the extension API expects. No shortcut
//     routing; the returned `shortcut` field is empty because desktop-android
//     has no global keyboard accelerator surface.
//
//   * `chrome.notifications.create` — logs + returns the caller-supplied id
//     (or generates one). A real notification is NOT shown — we don't have
//     the NotificationDisplayService wiring on desktop-android yet.
//
// Everything else stays unregistered (the original "Unknown Extension API"
// error is still logged by function_dispatcher for them).

#ifndef CHROME_BROWSER_EXTENSIONS_DESKTOP_ANDROID_DESKTOP_ANDROID_STUB_APIS_H_
#define CHROME_BROWSER_EXTENSIONS_DESKTOP_ANDROID_DESKTOP_ANDROID_STUB_APIS_H_

#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"
#include "extensions/browser/extension_function_registry.h"

namespace extensions {

// Registers the stub functions below with the provided registry.
void RegisterDesktopAndroidStubApiFunctions(
    ExtensionFunctionRegistry* registry);

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

class DesktopAndroidCommandsGetAllFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("commands.getAll", COMMANDS_GETALL)
  DesktopAndroidCommandsGetAllFunction() = default;

 protected:
  ~DesktopAndroidCommandsGetAllFunction() override = default;
  ResponseAction Run() override;
};

class DesktopAndroidNotificationsCreateFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("notifications.create", NOTIFICATIONS_CREATE)
  DesktopAndroidNotificationsCreateFunction() = default;

 protected:
  ~DesktopAndroidNotificationsCreateFunction() override = default;
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_DESKTOP_ANDROID_DESKTOP_ANDROID_STUB_APIS_H_
