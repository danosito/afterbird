// Copyright 2026 The Afterbird Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "url/gurl.h"

// Generated header defines JNI_AppMenuBridge_* function dispatch.
#include "chrome/browser/extensions/android/jni_headers/AppMenuBridge_jni.h"

namespace extensions {

namespace {

// Separators that mirror AppMenuBridge.java constants.
constexpr char kFieldSeparator[] = "\x1e";
constexpr char kRecordSeparator[] = "\x1f";

// Returns the popup URL for the extension's browser action, or empty string
// if the extension has no action or no default popup.
std::string GetPopupUrlForExtension(const Extension* extension) {
  // Read the "default_popup" from the "browser_action" manifest key.
  // For Manifest V2 extensions (which is what Kiwi-style loading targets),
  // this is the popup HTML loaded when the toolbar icon is clicked.
  const base::Value::Dict* browser_action =
      extension->manifest()->available_values().FindDict("browser_action");
  if (browser_action) {
    const std::string* popup = browser_action->FindString("default_popup");
    if (popup && !popup->empty()) {
      GURL popup_url = extension->GetResourceURL(*popup);
      if (popup_url.is_valid()) return popup_url.spec();
    }
  }

  // Fall back to action (MV3) or page_action.
  const base::Value::Dict* action =
      extension->manifest()->available_values().FindDict("action");
  if (action) {
    const std::string* popup = action->FindString("default_popup");
    if (popup && !popup->empty()) {
      GURL popup_url = extension->GetResourceURL(*popup);
      if (popup_url.is_valid()) return popup_url.spec();
    }
  }

  return std::string();
}

}  // namespace

// @JniType("std::string") return from Java → return std::string directly.
static std::string JNI_AppMenuBridge_GetRunningExtensions(
    JNIEnv* env,
    Profile* profile,
    content::WebContents* web_contents) {
  std::string result;
  if (!profile) {
    return result;
  }

  ExtensionRegistry* registry = ExtensionRegistry::Get(profile);
  if (!registry) {
    return result;
  }

  const ExtensionSet& enabled = registry->enabled_extensions();
  bool first = true;
  for (const auto& extension : enabled) {
    // Skip component/theme/app extensions that shouldn't appear in the menu.
    if (extension->is_theme() || extension->is_hosted_app() ||
        extension->location() ==
            extensions::mojom::ManifestLocation::kComponent) {
      continue;
    }

    if (!first) result += kRecordSeparator;
    first = false;

    // name
    result += extension->name();
    result += kFieldSeparator;
    // id
    result += extension->id();
    result += kFieldSeparator;
    // popup URL (may be empty)
    result += GetPopupUrlForExtension(extension.get());
    result += kFieldSeparator;
    // icon (base64) - omitted for now; Java uses a generic puzzle-piece
    // fallback when this is empty. See TODO below.
    result += "";
    result += kFieldSeparator;
    // active/inactive state (in incognito) - always "active" for now
    result += "active";
  }

  return result;
}

static void JNI_AppMenuBridge_CallExtension(
    JNIEnv* env,
    Profile* profile,
    content::WebContents* web_contents,
    std::string& extension_id) {
  // Placeholder: without full browser_action / ExtensionActionManager
  // infrastructure (desktop-only), there's no direct way to invoke an
  // extension's action from Java here. Kiwi did this via ExtensionActionAPI::
  // DispatchExtensionActionClicked. Desktop-android builds don't compile that.
  //
  // Callers should prefer opening the popup URL directly via
  // TabCreator.createNewTab(popupUrl).
  (void)extension_id;
  (void)profile;
  (void)web_contents;
}

static void JNI_AppMenuBridge_GrantExtensionActiveTab(
    JNIEnv* env,
    Profile* profile,
    content::WebContents* web_contents,
    std::string& extension_id) {
  // Placeholder: granting activeTab permission requires ActiveTabPermission
  // which depends on ExtensionService. Not available in desktop-android
  // builds yet. Extensions will still work for content-scripts and DNR which
  // don't require activeTab.
  (void)extension_id;
  (void)profile;
  (void)web_contents;
}

}  // namespace extensions
