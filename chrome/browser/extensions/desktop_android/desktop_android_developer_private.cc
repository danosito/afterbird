// Copyright 2026 The Afterbird Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/desktop_android/desktop_android_developer_private.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "content/public/browser/web_contents.h"
#include "chrome/browser/extensions/android/extension_install_bridge.h"
#include "chrome/browser/extensions/desktop_android/desktop_android_extension_system.h"
#include "chrome/browser/extensions/desktop_android/extension_installer.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "url/gurl.h"

namespace extensions {

namespace {

// These strings are what the chrome://extensions JS enums resolve to. They
// must match the schema enum names in developer_private.idl.
constexpr char kLocationFromStore[] = "FROM_STORE";
constexpr char kLocationUnpacked[] = "UNPACKED";
constexpr char kLocationThirdParty[] = "THIRD_PARTY";
constexpr char kLocationInstalledByDefault[] = "INSTALLED_BY_DEFAULT";
constexpr char kLocationUnknown[] = "UNKNOWN";

constexpr char kStateEnabled[] = "ENABLED";
constexpr char kStateDisabled[] = "DISABLED";
constexpr char kStateTerminated[] = "TERMINATED";
constexpr char kStateBlocklisted[] = "BLOCKLISTED";

constexpr char kTypeExtension[] = "EXTENSION";
constexpr char kTypeHostedApp[] = "HOSTED_APP";
constexpr char kTypeLegacyPackagedApp[] = "LEGACY_PACKAGED_APP";
constexpr char kTypePlatformApp[] = "PLATFORM_APP";
constexpr char kTypeTheme[] = "THEME";
constexpr char kTypeLoginScreenExtension[] = "LOGIN_SCREEN_EXTENSION";

const char* LocationToString(mojom::ManifestLocation location) {
  switch (location) {
    case mojom::ManifestLocation::kInternal:
      return kLocationFromStore;
    case mojom::ManifestLocation::kUnpacked:
    case mojom::ManifestLocation::kCommandLine:
      return kLocationUnpacked;
    case mojom::ManifestLocation::kExternalComponent:
    case mojom::ManifestLocation::kExternalPolicy:
    case mojom::ManifestLocation::kExternalPolicyDownload:
    case mojom::ManifestLocation::kExternalPref:
    case mojom::ManifestLocation::kExternalPrefDownload:
    case mojom::ManifestLocation::kExternalRegistry:
    case mojom::ManifestLocation::kComponent:
      return kLocationInstalledByDefault;
    case mojom::ManifestLocation::kInvalidLocation:
      return kLocationUnknown;
  }
  return kLocationUnknown;
}

const char* TypeToString(Manifest::Type type) {
  switch (type) {
    case Manifest::TYPE_EXTENSION:
      return kTypeExtension;
    case Manifest::TYPE_HOSTED_APP:
      return kTypeHostedApp;
    case Manifest::TYPE_LEGACY_PACKAGED_APP:
      return kTypeLegacyPackagedApp;
    case Manifest::TYPE_PLATFORM_APP:
      return kTypePlatformApp;
    case Manifest::TYPE_THEME:
      return kTypeTheme;
    case Manifest::TYPE_LOGIN_SCREEN_EXTENSION:
      return kTypeLoginScreenExtension;
    default:
      return kTypeExtension;
  }
}

// Builds a minimal ExtensionInfo dict matching the subset of fields Polymer
// reads in item.ts / detail_view.ts on chrome://extensions.
base::Value::Dict BuildExtensionInfo(const Extension& extension,
                                     const char* state) {
  base::Value::Dict info;
  info.Set("id", extension.id());
  info.Set("name", extension.name());
  info.Set("shortName", extension.short_name());
  info.Set("version", extension.GetVersionForDisplay());
  info.Set("description", extension.description());
  info.Set("state", state);
  info.Set("location", LocationToString(extension.location()));
  info.Set("type", TypeToString(extension.GetType()));

  info.Set("userMayModify", !Manifest::IsComponentLocation(extension.location()));
  info.Set("mustRemainInstalled", Manifest::IsComponentLocation(extension.location()));
  info.Set("mustRemainInstalledReason", "");
  info.Set("isCommandRegistrationHandledByChrome", false);
  info.Set("canUploadAsAccountExtension", false);

  // Icons — build a chrome-extension:// icon URL for the default size.
  const ExtensionIconSet& icon_set = IconsInfo::GetIcons(&extension);
  std::string default_icon_url;
  base::Value::List icons;
  for (const auto& icon : icon_set.map()) {
    base::Value::Dict entry;
    entry.Set("size", icon.first);
    const std::string url = "chrome://extension-icon/" + extension.id() + "/" +
                            base::NumberToString(icon.first) + "/1";
    entry.Set("url", url);
    if (default_icon_url.empty()) {
      default_icon_url = url;
    }
    icons.Append(std::move(entry));
  }
  if (default_icon_url.empty()) {
    default_icon_url = "chrome://extension-icon/" + extension.id() + "/48/1";
  }
  info.Set("iconUrl", default_icon_url);
  info.Set("icons", std::move(icons));

  // Views are always empty — we don't track live pages for extensions on
  // desktop-android (there is no options page popup etc.).
  info.Set("views", base::Value::List());

  // Empty permission lists — the JS renders "No permissions" gracefully.
  base::Value::Dict permissions;
  permissions.Set("simplePermissions", base::Value::List());
  permissions.Set("canAccessSiteData", false);
  info.Set("permissions", std::move(permissions));
  info.Set("dependentExtensions", base::Value::List());
  info.Set("installWarnings", base::Value::List());
  info.Set("manifestErrors", base::Value::List());
  info.Set("runtimeErrors", base::Value::List());
  // Afterbird: `runtimeWarnings` is NOT optional on the UI side —
  // `ExtensionsItemElement.hasSevereWarnings_()` reads `.length`
  // unconditionally, so missing it throws and kills the Lit render →
  // the item row collapses to height=0 and the list looks empty.
  info.Set("runtimeWarnings", base::Value::List());

  // Disable reasons dictionary (all false when enabled).
  base::Value::Dict disable_reasons;
  disable_reasons.Set("corruptInstall", false);
  disable_reasons.Set("custodianApprovalRequired", false);
  disable_reasons.Set("custodianApprovalRequiredForInstallation", false);
  disable_reasons.Set("blockedByPolicy", false);
  disable_reasons.Set("reloading", false);
  disable_reasons.Set("suspiciousInstall", false);
  disable_reasons.Set("updateRequired", false);
  disable_reasons.Set("publishedInStoreRequired", false);
  disable_reasons.Set("unsupportedDeveloperExtension", false);
  disable_reasons.Set("unsupportedManifestVersion", false);
  info.Set("disableReasons", std::move(disable_reasons));

  // safetyCheckText, safetyCheckWarningReason — leave blank; the UI shows
  // no warning when these are absent.
  info.Set("safetyCheckText", base::Value::Dict());

  // Incognito access — not supported on desktop-android.
  info.Set("incognitoAccess", base::Value::Dict());

  // File access — optional; empty dict works.
  info.Set("fileAccess", base::Value::Dict());

  // Policy-controlled fields.
  info.Set("controlledInfo", base::Value());

  // Site access controls — desktop-android doesn't expose these yet.
  base::Value::Dict site_access;
  site_access.Set("hostAccess", "ON_CLICK");
  site_access.Set("hasAllHosts", false);
  site_access.Set("specificSiteControls", base::Value::List());
  info.Set("siteAccess", std::move(site_access));
  info.Set("runOnAllUrls", base::Value::Dict());
  info.Set("showAccessRequestsInToolbar", base::Value::Dict());
  info.Set("pinnedToToolbar", base::Value::Dict());

  // Size on disk: not computed here.
  info.Set("size", "");

  // Path (for unpacked extensions).
  info.Set("path", extension.path().AsUTF8Unsafe());
  info.Set("prettifiedPath", extension.path().AsUTF8Unsafe());

  // Manifest version.
  info.Set("manifestVersion", extension.manifest_version());

  // Web store URL and update URL.
  info.Set("webStoreUrl", "https://chrome.google.com/webstore/detail/" +
                              extension.id());
  info.Set("updateUrl", std::string());

  info.Set("mv2DeprecationNoticeAcknowledged", false);
  info.Set("acknowledgeSafetyCheckWarningReason", "");

  return info;
}

base::Value::List BuildAllExtensionsInfo(content::BrowserContext* context) {
  base::Value::List list;
  auto* registry = ExtensionRegistry::Get(context);
  if (!registry) {
    return list;
  }
  for (const auto& ext : registry->enabled_extensions()) {
    list.Append(BuildExtensionInfo(*ext, kStateEnabled));
  }
  for (const auto& ext : registry->disabled_extensions()) {
    list.Append(BuildExtensionInfo(*ext, kStateDisabled));
  }
  for (const auto& ext : registry->terminated_extensions()) {
    list.Append(BuildExtensionInfo(*ext, kStateTerminated));
  }
  for (const auto& ext : registry->blocklisted_extensions()) {
    list.Append(BuildExtensionInfo(*ext, kStateBlocklisted));
  }
  return list;
}

base::Value::Dict BuildProfileInfo(content::BrowserContext* context) {
  base::Value::Dict cfg;
  PrefService* prefs = ExtensionPrefs::Get(context)->pref_service();
  const bool dev_mode =
      prefs && prefs->GetBoolean(prefs::kExtensionsUIDeveloperMode);
  cfg.Set("inDeveloperMode", dev_mode);
  cfg.Set("canLoadUnpacked", true);
  cfg.Set("isDeveloperModeControlledByPolicy", false);
  cfg.Set("isIncognitoAvailable", false);
  cfg.Set("isChildAccount", false);
  cfg.Set("isMv2DeprecationNoticeDismissed", true);
  cfg.Set("isMv2DeprecationWarningDismissed", true);
  cfg.Set("isMv2DeprecationDisabledDismissed", true);
  cfg.Set("isMv2DeprecationUnsupportedDismissed", true);
  return cfg;
}

}  // namespace

// ----------------------------------------------------------------------------

DesktopAndroidDeveloperPrivateGetProfileConfigurationFunction::
    DesktopAndroidDeveloperPrivateGetProfileConfigurationFunction() = default;
DesktopAndroidDeveloperPrivateGetProfileConfigurationFunction::
    ~DesktopAndroidDeveloperPrivateGetProfileConfigurationFunction() = default;

ExtensionFunction::ResponseAction
DesktopAndroidDeveloperPrivateGetProfileConfigurationFunction::Run() {
  base::Value::List args;
  args.Append(BuildProfileInfo(browser_context()));
  return RespondNow(ArgumentList(std::move(args)));
}

// ----------------------------------------------------------------------------

DesktopAndroidDeveloperPrivateUpdateProfileConfigurationFunction::
    DesktopAndroidDeveloperPrivateUpdateProfileConfigurationFunction() =
        default;
DesktopAndroidDeveloperPrivateUpdateProfileConfigurationFunction::
    ~DesktopAndroidDeveloperPrivateUpdateProfileConfigurationFunction() =
        default;

ExtensionFunction::ResponseAction
DesktopAndroidDeveloperPrivateUpdateProfileConfigurationFunction::Run() {
  // args()[0] is a ProfileConfigurationUpdate dict; the only field we honour
  // is inDeveloperMode. Everything else is silently ignored.
  if (!args().empty() && args()[0].is_dict()) {
    const base::Value::Dict& update = args()[0].GetDict();
    if (std::optional<bool> dev_mode = update.FindBool("inDeveloperMode")) {
      PrefService* prefs =
          ExtensionPrefs::Get(browser_context())->pref_service();
      if (prefs) {
        prefs->SetBoolean(prefs::kExtensionsUIDeveloperMode, *dev_mode);
      }
    }
  }
  return RespondNow(NoArguments());
}

// ----------------------------------------------------------------------------

DesktopAndroidDeveloperPrivateGetExtensionsInfoFunction::
    DesktopAndroidDeveloperPrivateGetExtensionsInfoFunction() = default;
DesktopAndroidDeveloperPrivateGetExtensionsInfoFunction::
    ~DesktopAndroidDeveloperPrivateGetExtensionsInfoFunction() = default;

ExtensionFunction::ResponseAction
DesktopAndroidDeveloperPrivateGetExtensionsInfoFunction::Run() {
  base::Value::List args;
  args.Append(BuildAllExtensionsInfo(browser_context()));
  return RespondNow(ArgumentList(std::move(args)));
}

// ----------------------------------------------------------------------------

DesktopAndroidDeveloperPrivateGetExtensionInfoFunction::
    DesktopAndroidDeveloperPrivateGetExtensionInfoFunction() = default;
DesktopAndroidDeveloperPrivateGetExtensionInfoFunction::
    ~DesktopAndroidDeveloperPrivateGetExtensionInfoFunction() = default;

ExtensionFunction::ResponseAction
DesktopAndroidDeveloperPrivateGetExtensionInfoFunction::Run() {
  if (args().empty() || !args()[0].is_string()) {
    return RespondNow(Error("Extension id is required"));
  }
  const std::string& id = args()[0].GetString();
  auto* registry = ExtensionRegistry::Get(browser_context());
  if (!registry) {
    return RespondNow(Error("No ExtensionRegistry"));
  }
  const Extension* ext = registry->GetInstalledExtension(id);
  if (!ext) {
    return RespondNow(Error("No such extension: " + id));
  }
  const char* state = registry->enabled_extensions().Contains(id)
                          ? kStateEnabled
                          : kStateDisabled;
  base::Value::List args_out;
  args_out.Append(BuildExtensionInfo(*ext, state));
  return RespondNow(ArgumentList(std::move(args_out)));
}

// ----------------------------------------------------------------------------

ExtensionFunction::ResponseAction
DesktopAndroidDeveloperPrivateNoOpFunction::Run() {
  return RespondNow(NoArguments());
}

// ----------------------------------------------------------------------------
// Extension management: updateExtensionConfiguration (toggle),
// removeMultipleExtensions (uninstall), reload.
//
// These delegate to ExtensionRegistrar owned by DesktopAndroidExtensionSystem.
// ExtensionRegistrar lives in extensions/browser and is not gated on
// ENABLE_EXTENSIONS, so it links cleanly in our desktop-android build.
// ----------------------------------------------------------------------------

namespace {

ExtensionRegistrar* GetRegistrar(content::BrowserContext* context) {
  auto* system = static_cast<DesktopAndroidExtensionSystem*>(
      ExtensionSystem::Get(context));
  return system ? system->extension_registrar() : nullptr;
}

}  // namespace

DesktopAndroidDeveloperPrivateUpdateExtensionConfigurationFunction::
    DesktopAndroidDeveloperPrivateUpdateExtensionConfigurationFunction() =
        default;
DesktopAndroidDeveloperPrivateUpdateExtensionConfigurationFunction::
    ~DesktopAndroidDeveloperPrivateUpdateExtensionConfigurationFunction() =
        default;

ExtensionFunction::ResponseAction
DesktopAndroidDeveloperPrivateUpdateExtensionConfigurationFunction::Run() {
  if (args().empty() || !args()[0].is_dict()) {
    return RespondNow(Error("Expected a config dict"));
  }
  const base::Value::Dict& update = args()[0].GetDict();
  const std::string* id = update.FindString("extensionId");
  if (!id || id->empty()) {
    return RespondNow(Error("extensionId is required"));
  }
  ExtensionRegistrar* registrar = GetRegistrar(browser_context());
  if (!registrar) {
    return RespondNow(Error("ExtensionRegistrar unavailable"));
  }

  if (std::optional<bool> enabled = update.FindBool("isEnabled")) {
    if (*enabled) {
      registrar->EnableExtension(*id);
    } else {
      registrar->DisableExtension(
          *id, /*disable_reasons=*/disable_reason::DISABLE_USER_ACTION);
    }
  }

  // Other flags (allowIncognito, fileAccess, hostAccess, showAccessRequests,
  // pinnedToToolbar, collectsErrors, ...) are silently accepted as no-ops —
  // desktop-android doesn't support them and the JS expects the promise to
  // resolve regardless.

  return RespondNow(NoArguments());
}

// ----------------------------------------------------------------------------

DesktopAndroidDeveloperPrivateRemoveMultipleExtensionsFunction::
    DesktopAndroidDeveloperPrivateRemoveMultipleExtensionsFunction() = default;
DesktopAndroidDeveloperPrivateRemoveMultipleExtensionsFunction::
    ~DesktopAndroidDeveloperPrivateRemoveMultipleExtensionsFunction() =
        default;

ExtensionFunction::ResponseAction
DesktopAndroidDeveloperPrivateRemoveMultipleExtensionsFunction::Run() {
  if (args().empty() || !args()[0].is_list()) {
    return RespondNow(Error("Expected an array of extension ids"));
  }
  ExtensionRegistrar* registrar = GetRegistrar(browser_context());
  if (!registrar) {
    return RespondNow(Error("ExtensionRegistrar unavailable"));
  }
  // Collect the ids first — registrar->RemoveExtension() may invalidate the
  // Extension* we'd otherwise need to ask about its manifest location.
  std::vector<ExtensionId> ids;
  for (const base::Value& id_val : args()[0].GetList()) {
    if (id_val.is_string()) {
      ids.push_back(id_val.GetString());
    }
  }
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context());
  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context());
  for (const ExtensionId& id : ids) {
    // Capture the location before we remove the extension from the registry
    // (afterwards GetInstalledExtension returns nullptr).
    mojom::ManifestLocation location = mojom::ManifestLocation::kUnpacked;
    if (const Extension* ext = registry->GetInstalledExtension(id)) {
      location = ext->location();
    }
    registrar->RemoveExtension(id, UnloadedExtensionReason::UNINSTALL);
    if (prefs) {
      prefs->OnExtensionUninstalled(id, location,
                                    /*external_uninstall=*/false);
    }
  }
  return RespondNow(NoArguments());
}

// ----------------------------------------------------------------------------

DesktopAndroidDeveloperPrivateReloadFunction::
    DesktopAndroidDeveloperPrivateReloadFunction() = default;
DesktopAndroidDeveloperPrivateReloadFunction::
    ~DesktopAndroidDeveloperPrivateReloadFunction() = default;

ExtensionFunction::ResponseAction
DesktopAndroidDeveloperPrivateReloadFunction::Run() {
  if (args().empty() || !args()[0].is_dict()) {
    return RespondNow(Error("Expected a reload options dict"));
  }
  const base::Value::Dict& opts = args()[0].GetDict();
  const std::string* id = opts.FindString("extensionId");
  if (!id || id->empty()) {
    return RespondNow(Error("extensionId is required"));
  }
  ExtensionRegistrar* registrar = GetRegistrar(browser_context());
  if (!registrar) {
    return RespondNow(Error("ExtensionRegistrar unavailable"));
  }
  registrar->ReloadExtension(*id, ExtensionRegistrar::LoadErrorBehavior::kNoisy);
  return RespondNow(NoArguments());
}

// ----------------------------------------------------------------------------
// developerPrivate.loadUnpacked — async file-picker driven install.
//
// chrome://extensions calls this when the user taps "Load unpacked". The
// upstream handler pops a native directory picker and installs on success;
// our Android picker can pick a .zip/.crx or a tree uri (treated as a
// directory after extraction). The function stays alive across the
// async picker roundtrip via AddRef / Release inside ExtensionFunction.

DesktopAndroidDeveloperPrivateLoadUnpackedFunction::
    DesktopAndroidDeveloperPrivateLoadUnpackedFunction() = default;
DesktopAndroidDeveloperPrivateLoadUnpackedFunction::
    ~DesktopAndroidDeveloperPrivateLoadUnpackedFunction() = default;

ExtensionFunction::ResponseAction
DesktopAndroidDeveloperPrivateLoadUnpackedFunction::Run() {
  content::WebContents* web_contents = GetSenderWebContents();
  if (!web_contents) {
    return RespondNow(Error("Cannot show file picker — no WebContents"));
  }
  installer_ =
      std::make_unique<DesktopAndroidExtensionInstaller>(browser_context());
  ExtensionInstallCallback::Show(
      web_contents,
      base::BindOnce(
          &DesktopAndroidDeveloperPrivateLoadUnpackedFunction::OnFilePicked,
          base::WrapRefCounted(this)));
  return RespondLater();
}

void DesktopAndroidDeveloperPrivateLoadUnpackedFunction::OnFilePicked(
    const base::FilePath& path) {
  if (path.empty()) {
    Respond(Error("File selection was canceled."));
    return;
  }
  installer_->InstallFromFile(
      path,
      base::BindOnce(
          &DesktopAndroidDeveloperPrivateLoadUnpackedFunction::OnInstalled,
          base::WrapRefCounted(this)));
}

void DesktopAndroidDeveloperPrivateLoadUnpackedFunction::OnInstalled(
    scoped_refptr<const Extension> extension,
    const std::string& error) {
  if (!extension) {
    // chrome://extensions expects a LoadError object on failure, but the
    // simple text error path triggers the same toast + empty-state UI —
    // good enough for v0.6 and keeps us out of LoadError's ~20-field dict.
    Respond(Error(error.empty() ? "Extension failed to load" : error));
    return;
  }
  Respond(NoArguments());
}

// ----------------------------------------------------------------------------

void RegisterDesktopAndroidDeveloperPrivateFunctions(
    ExtensionFunctionRegistry* registry) {
  registry->RegisterFunction<
      DesktopAndroidDeveloperPrivateGetProfileConfigurationFunction>();
  registry->RegisterFunction<
      DesktopAndroidDeveloperPrivateUpdateProfileConfigurationFunction>();
  registry->RegisterFunction<
      DesktopAndroidDeveloperPrivateGetExtensionsInfoFunction>();
  registry->RegisterFunction<
      DesktopAndroidDeveloperPrivateGetExtensionInfoFunction>();

  // Extension management (v0.6 phase 1).
  registry->RegisterFunction<
      DesktopAndroidDeveloperPrivateUpdateExtensionConfigurationFunction>();
  registry->RegisterFunction<
      DesktopAndroidDeveloperPrivateRemoveMultipleExtensionsFunction>();
  registry->RegisterFunction<DesktopAndroidDeveloperPrivateReloadFunction>();
  registry->RegisterFunction<
      DesktopAndroidDeveloperPrivateLoadUnpackedFunction>();

  // Stub functions.
  registry->RegisterFunction<DesktopAndroidDeveloperPrivateAutoUpdateFunction>();
  registry->RegisterFunction<
      DesktopAndroidDeveloperPrivateDeleteExtensionErrorsFunction>();
  registry->RegisterFunction<
      DesktopAndroidDeveloperPrivateGetUserSiteSettingsFunction>();
  registry->RegisterFunction<
      DesktopAndroidDeveloperPrivateGetUserAndExtensionSitesByEtldFunction>();
  registry->RegisterFunction<
      DesktopAndroidDeveloperPrivateGetMatchingExtensionsForSiteFunction>();
}

}  // namespace extensions
