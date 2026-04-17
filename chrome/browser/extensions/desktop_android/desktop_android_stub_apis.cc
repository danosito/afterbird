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

}  // namespace

// ----------------------------------------------------------------------------

ExtensionFunction::ResponseAction
DesktopAndroidPermissionsGetAllFunction::Run() {
  const Extension* ext = extension();
  if (!ext) {
    // Still produce a valid-shaped response — extensions treat `undefined`
    // the same as "no permissions" and proceed, which is what we want.
    base::Value::Dict empty;
    empty.Set("permissions", base::Value::List());
    empty.Set("origins", base::Value::List());
    base::Value::List args;
    args.Append(std::move(empty));
    return RespondNow(ArgumentList(std::move(args)));
  }
  base::Value::List args;
  args.Append(BuildPermissionsDict(*ext));
  return RespondNow(ArgumentList(std::move(args)));
}

// ----------------------------------------------------------------------------

ExtensionFunction::ResponseAction
DesktopAndroidPermissionsContainsFunction::Run() {
  // Arg 0 is a Permissions dict: { permissions?: [...], origins?: [...] }.
  // We report "contains=true" iff every requested entry appears in what the
  // manifest declared. Anything we can't resolve → false (conservative).
  if (args().empty() || !args()[0].is_dict()) {
    base::Value::List out;
    out.Append(false);
    return RespondNow(ArgumentList(std::move(out)));
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
  base::Value::List out;
  out.Append(ok);
  return RespondNow(ArgumentList(std::move(out)));
}

// ----------------------------------------------------------------------------

ExtensionFunction::ResponseAction DesktopAndroidCommandsGetAllFunction::Run() {
  // Upstream shape is an array of Command dicts:
  //   { name, description, shortcut, global }
  // We return whatever manifest `commands` dict declared, with `shortcut` left
  // blank because desktop-android has no in-browser accelerator surface to
  // wire them into.
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
  base::Value::List args;
  args.Append(std::move(out));
  return RespondNow(ArgumentList(std::move(args)));
}

// ----------------------------------------------------------------------------

ExtensionFunction::ResponseAction
DesktopAndroidNotificationsCreateFunction::Run() {
  // API: notifications.create(optional string id, NotificationOptions opts,
  //                           optional callback(string id))
  // The argument list here is [id?, options] depending on whether the caller
  // supplied an id. We only care about echoing back an id — the JS bindings
  // layer has already normalised the invocation.
  std::string id;
  if (!args().empty() && args()[0].is_string()) {
    id = args()[0].GetString();
  }
  if (id.empty()) {
    // Unique-ish synthetic id. Format mirrors upstream enough that code
    // keying off the returned id to cancel later still works via our stub
    // clear/update handlers (future patch).
    id = base::StringPrintf(
        "stub-%lld",
        static_cast<long long>(
            base::Time::Now().InMillisecondsSinceUnixEpoch()));
  }
  LOG(INFO) << "[afterbird] notifications.create stub: id=" << id
            << " (no visible toast — NotificationDisplayService not wired on "
               "desktop-android)";
  base::Value::List args_out;
  args_out.Append(id);
  return RespondNow(ArgumentList(std::move(args_out)));
}

// ----------------------------------------------------------------------------

void RegisterDesktopAndroidStubApiFunctions(
    ExtensionFunctionRegistry* registry) {
  registry->RegisterFunction<DesktopAndroidPermissionsGetAllFunction>();
  registry->RegisterFunction<DesktopAndroidPermissionsContainsFunction>();
  registry->RegisterFunction<DesktopAndroidCommandsGetAllFunction>();
  registry->RegisterFunction<DesktopAndroidNotificationsCreateFunction>();
}

}  // namespace extensions
