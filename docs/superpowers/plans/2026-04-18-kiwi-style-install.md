# Kiwi-style install flow Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rework Afterbird's extension install flow so the CWS detail page renders normally and its native "Install" button triggers the existing `CrxInstallCoordinator` via a scoped `webstorePrivate` shim, while adding a "+ (from store)" discovery button to `chrome://extensions`.

**Architecture:** Three surgical changes: (1) delete the navigation-throttle that hides CWS detail pages, (2) add a 3-function `webstorePrivate` shim that calls into the existing `CrxInstallCoordinator::StartFromWebstore` on `beginInstallWithManifest3`, (3) add a Lit-button in the `chrome://extensions` toolbar that opens the CWS homepage. Download-intercept for raw `.crx` / `.user.js` URLs is untouched; the confirm-dialog pipeline is untouched.

**Tech Stack:** C++ (`chrome/browser/extensions/desktop_android/...`), `BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)` guards, TypeScript / Lit (`chrome/browser/resources/extensions/toolbar.*`), `chrome_content_browser_client.cc` hook removal, GN BUILD files.

**Spec:** `docs/superpowers/specs/2026-04-18-kiwi-style-install-design.md`
**Diagnostic:** `docs/superpowers/diagnostics/2026-04-17-kiwi-install-flow.md`
**Branch:** `feature/heavy-ext-and-install`
**Device under test:** `R5CTA2MHJFA` (always `adb -s R5CTA2MHJFA ...`)

---

## File structure (decision)

New C++ files, all under `chrome/browser/extensions/desktop_android/webstore_private/`:

- `webstore_url_util.h` / `webstore_url_util.cc` — helpers previously living in the throttle: `ExtractWebstoreExtensionId(const GURL&)`, `BuildWebstoreCrxUrl(const std::string& extension_id)`, plus new `IsWebstoreOrigin(const GURL&)`.
- `webstore_private_api.h` / `webstore_private_api.cc` — the three ExtensionFunction subclasses: `WebstorePrivateGetWebGLStatusFunction`, `WebstorePrivateBeginInstallWithManifest3Function`, `WebstorePrivateCompleteInstallFunction`. Plus a `RegisterDesktopAndroidWebstorePrivateFunctions(registry)` entry point mirroring the developer_private pattern.

Deleted:
- `chrome/browser/extensions/desktop_android/extension_install_navigation_throttle.cc`
- `chrome/browser/extensions/desktop_android/extension_install_navigation_throttle.h`

Modified:
- `chrome/browser/BUILD.gn` — swap the two throttle lines for the four webstore_private lines.
- `chrome/browser/chrome_content_browser_client.cc` — delete the throttle include + `MaybeAddThrottle` block.
- `chrome/browser/extensions/desktop_android/desktop_android_extensions_browser_client.cc` — call the new `Register…WebstorePrivateFunctions`.
- `chrome/browser/resources/extensions/toolbar.ts` — add `onLoadFromStoreClick_()` method.
- `chrome/browser/resources/extensions/toolbar.html.ts` — add the `+ (from store)` `cr-button`.
- `chrome/browser/ui/webui/extensions/extensions_ui.cc` — register the `toolbarLoadFromStore` string (via `AddString` fallback since desktop-android has no IDS_ for it).

Test-and-smoke-only (no commits to these paths):
- `adb -s R5CTA2MHJFA ...` on the physical device.
- `/tmp/kiwi_ksi-smoke-*.png` screenshots written to `/tmp/` and not checked in.

---

## Task 1: Create `webstore_url_util` extracted from the throttle

**Files:**
- Create: `chrome/browser/extensions/desktop_android/webstore_private/webstore_url_util.h`
- Create: `chrome/browser/extensions/desktop_android/webstore_private/webstore_url_util.cc`

- [ ] **Step 1: Write the header**

Write `chrome/browser/extensions/desktop_android/webstore_private/webstore_url_util.h`:

```cpp
// Copyright 2026 The Afterbird Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_DESKTOP_ANDROID_WEBSTORE_PRIVATE_WEBSTORE_URL_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_DESKTOP_ANDROID_WEBSTORE_PRIVATE_WEBSTORE_URL_UTIL_H_

#include <string>

class GURL;

namespace extensions::desktop_android {

// Returns the 32-character [a-p] extension id parsed from a Chrome Web Store
// detail URL, or the empty string if `url` is not a recognised CWS detail
// URL. Accepts both the current host (chromewebstore.google.com) and the
// legacy host (chrome.google.com/webstore).
std::string ExtractWebstoreExtensionId(const GURL& url);

// Builds the clients2.google.com update endpoint URL that returns a CRX
// redirect for the given extension id.
GURL BuildWebstoreCrxUrl(const std::string& extension_id);

// Returns true iff `url` is on the current or legacy Chrome Web Store origin.
// Used by the webstorePrivate shim to gate the API to the CWS origin.
bool IsWebstoreOrigin(const GURL& url);

}  // namespace extensions::desktop_android

#endif  // CHROME_BROWSER_EXTENSIONS_DESKTOP_ANDROID_WEBSTORE_PRIVATE_WEBSTORE_URL_UTIL_H_
```

- [ ] **Step 2: Write the implementation**

Write `chrome/browser/extensions/desktop_android/webstore_private/webstore_url_util.cc`:

```cpp
// Copyright 2026 The Afterbird Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/desktop_android/webstore_private/webstore_url_util.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "url/gurl.h"

namespace extensions::desktop_android {

std::string ExtractWebstoreExtensionId(const GURL& url) {
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return std::string();
  }
  const std::string host = url.host();
  const std::string path = url.path();
  std::string id_candidate;
  if (host == "chromewebstore.google.com") {
    std::vector<std::string_view> parts = base::SplitStringPiece(
        path, "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (parts.size() >= 2 && parts[0] == "detail") {
      id_candidate = std::string(parts.back());
    }
  } else if (host == "chrome.google.com" &&
             base::StartsWith(path, "/webstore/detail/",
                              base::CompareCase::SENSITIVE)) {
    std::vector<std::string_view> parts = base::SplitStringPiece(
        path, "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (parts.size() >= 3) {
      id_candidate = std::string(parts.back());
    }
  }
  if (id_candidate.size() != 32) {
    return std::string();
  }
  for (char c : id_candidate) {
    if (c < 'a' || c > 'p') {
      return std::string();
    }
  }
  return id_candidate;
}

GURL BuildWebstoreCrxUrl(const std::string& extension_id) {
  return GURL(
      "https://clients2.google.com/service/update2/crx?response=redirect"
      "&prodversion=128.0&acceptformat=crx2,crx3&x=id%3D" +
      extension_id + "%26installsource%3Dondemand%26uc");
}

bool IsWebstoreOrigin(const GURL& url) {
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return false;
  }
  const std::string host = url.host();
  return host == "chromewebstore.google.com" || host == "chrome.google.com";
}

}  // namespace extensions::desktop_android
```

- [ ] **Step 3: Wire into BUILD.gn so it compiles**

Edit `chrome/browser/BUILD.gn` around line 7842. Find the three-file block:

```gn
      # Afterbird v1.2: orchestrates fetch → unpack → confirm → install,
      # replacing the v1.1 silent-install CrxDownloadInstaller. Owns the
      # SimpleURLLoader, staging dir, and the JNI confirm-dialog bridge.
      "extensions/desktop_android/crx_install_coordinator.cc",
      "extensions/desktop_android/crx_install_coordinator.h",
```

Add **after** that block (before the closing `]`):

```gn
      # Afterbird v1.4: webstorePrivate shim + URL helpers. The detail
      # page's own Install button calls into beginInstallWithManifest3,
      # which hands off to CrxInstallCoordinator.
      "extensions/desktop_android/webstore_private/webstore_url_util.cc",
      "extensions/desktop_android/webstore_private/webstore_url_util.h",
```

- [ ] **Step 4: Build**

Run on the serv build box (assume the user uses `ssh serv 'cd ~/afterbird && autoninja -C out/android chrome_public_apk'` or the local equivalent; if the plan executor is running locally, `autoninja -C out/android chrome_public_apk`):

Expected: build succeeds. The file is compiled but nobody calls it yet — the compile is a pure no-op check. If the build fails with an unused-symbols warning, ignore — `static` and anonymous-namespace aren't used here and the linker is fine with unreferenced public symbols.

- [ ] **Step 5: Commit**

```bash
git add chrome/browser/extensions/desktop_android/webstore_private/webstore_url_util.{cc,h} chrome/browser/BUILD.gn
git commit -m "feat(extensions): extract webstore URL helpers into reusable util

Moves ExtractWebstoreExtensionId / BuildWebstoreCrxUrl out of the
navigation-throttle's anonymous namespace into a shared util header
so the upcoming webstorePrivate shim can use them. Adds IsWebstoreOrigin
for the origin gate."
```

---

## Task 2: Scaffold the webstorePrivate header

**Files:**
- Create: `chrome/browser/extensions/desktop_android/webstore_private/webstore_private_api.h`

- [ ] **Step 1: Write the header**

Write `chrome/browser/extensions/desktop_android/webstore_private/webstore_private_api.h`:

```cpp
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

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_DESKTOP_ANDROID_WEBSTORE_PRIVATE_WEBSTORE_PRIVATE_API_H_
```

- [ ] **Step 2: Commit**

```bash
git add chrome/browser/extensions/desktop_android/webstore_private/webstore_private_api.h
git commit -m "feat(extensions): webstorePrivate shim header for desktop-android

Three ExtensionFunction subclasses for the functions CWS's detail page
actually calls: getWebGLStatus, beginInstallWithManifest3,
completeInstall. Follows the DesktopAndroidDeveloperPrivate* pattern."
```

---

## Task 3: Implement `WebstorePrivateGetWebGLStatusFunction` (trivial)

**Files:**
- Create: `chrome/browser/extensions/desktop_android/webstore_private/webstore_private_api.cc`
- Modify: `chrome/browser/BUILD.gn`

- [ ] **Step 1: Write the initial .cc with only getWebGLStatus**

Write `chrome/browser/extensions/desktop_android/webstore_private/webstore_private_api.cc`:

```cpp
// Copyright 2026 The Afterbird Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/desktop_android/webstore_private/webstore_private_api.h"

#include <memory>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/extensions/desktop_android/crx_install_coordinator.h"
#include "chrome/browser/extensions/desktop_android/webstore_private/webstore_url_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace extensions {

void RegisterDesktopAndroidWebstorePrivateFunctions(
    ExtensionFunctionRegistry* registry) {
  registry->RegisterFunction<WebstorePrivateGetWebGLStatusFunction>();
  registry->RegisterFunction<WebstorePrivateBeginInstallWithManifest3Function>();
  registry->RegisterFunction<WebstorePrivateCompleteInstallFunction>();
}

// -----------------------------------------------------------------------------
// getWebGLStatus — returns a fixed "webgl_allowed". Matches upstream for
// desktop platforms where WebGL is available.

WebstorePrivateGetWebGLStatusFunction::
    WebstorePrivateGetWebGLStatusFunction() = default;
WebstorePrivateGetWebGLStatusFunction::
    ~WebstorePrivateGetWebGLStatusFunction() = default;

ExtensionFunction::ResponseAction
WebstorePrivateGetWebGLStatusFunction::Run() {
  base::Value::List result;
  result.Append("webgl_allowed");
  return RespondNow(ArgumentList(std::move(result)));
}

// -----------------------------------------------------------------------------
// beginInstallWithManifest3 — not yet implemented; returns an error so the
// CWS page doesn't hang. Task 4 fills this in.

WebstorePrivateBeginInstallWithManifest3Function::
    WebstorePrivateBeginInstallWithManifest3Function() = default;
WebstorePrivateBeginInstallWithManifest3Function::
    ~WebstorePrivateBeginInstallWithManifest3Function() = default;

ExtensionFunction::ResponseAction
WebstorePrivateBeginInstallWithManifest3Function::Run() {
  return RespondNow(Error("not_implemented_yet"));
}

// -----------------------------------------------------------------------------
// completeInstall — no-op. Our install pipeline is already async-committed.

WebstorePrivateCompleteInstallFunction::
    WebstorePrivateCompleteInstallFunction() = default;
WebstorePrivateCompleteInstallFunction::
    ~WebstorePrivateCompleteInstallFunction() = default;

ExtensionFunction::ResponseAction
WebstorePrivateCompleteInstallFunction::Run() {
  return RespondNow(NoArguments());
}

}  // namespace extensions
```

- [ ] **Step 2: Wire the .cc into BUILD.gn**

Edit `chrome/browser/BUILD.gn`, right after the url_util entries added in Task 1:

```gn
      "extensions/desktop_android/webstore_private/webstore_url_util.cc",
      "extensions/desktop_android/webstore_private/webstore_url_util.h",
      "extensions/desktop_android/webstore_private/webstore_private_api.cc",
      "extensions/desktop_android/webstore_private/webstore_private_api.h",
```

- [ ] **Step 3: Register in the BrowserClient**

Edit `chrome/browser/extensions/desktop_android/desktop_android_extensions_browser_client.cc`:

Change the include block (around line 11) from:

```cpp
#include "chrome/browser/extensions/desktop_android/desktop_android_developer_private.h"
```

to:

```cpp
#include "chrome/browser/extensions/desktop_android/desktop_android_developer_private.h"
#include "chrome/browser/extensions/desktop_android/webstore_private/webstore_private_api.h"
```

Change `AfterbirdChromeExtensionsBrowserAPIProvider::RegisterExtensionFunctions` (around line 96) from:

```cpp
  void RegisterExtensionFunctions(
      ExtensionFunctionRegistry* registry) override {
    RegisterDesktopAndroidDeveloperPrivateFunctions(registry);
  }
```

to:

```cpp
  void RegisterExtensionFunctions(
      ExtensionFunctionRegistry* registry) override {
    RegisterDesktopAndroidDeveloperPrivateFunctions(registry);
    RegisterDesktopAndroidWebstorePrivateFunctions(registry);
  }
```

- [ ] **Step 4: Build**

Run: `autoninja -C out/android chrome_public_apk`

Expected: success. Three new classes compile, registry link works.

- [ ] **Step 5: Commit**

```bash
git add chrome/browser/extensions/desktop_android/webstore_private/webstore_private_api.cc chrome/browser/BUILD.gn chrome/browser/extensions/desktop_android/desktop_android_extensions_browser_client.cc
git commit -m "feat(extensions): register webstorePrivate shim (stub bodies)

Wires the three ExtensionFunction classes through the API registry so
the extension function dispatcher routes webstorePrivate calls to us.
getWebGLStatus returns the fixed 'webgl_allowed' string; the other two
return a not-implemented error until task 4 fills them in."
```

---

## Task 4: Implement `beginInstallWithManifest3` — origin gate + id extraction + coordinator handoff

**Files:**
- Modify: `chrome/browser/extensions/desktop_android/webstore_private/webstore_private_api.cc`

- [ ] **Step 1: Rewrite the beginInstallWithManifest3 body**

Replace the stub body in `webstore_private_api.cc`:

```cpp
ExtensionFunction::ResponseAction
WebstorePrivateBeginInstallWithManifest3Function::Run() {
  // Origin gate: the function is only exposed to the CWS origin upstream.
  // We replicate that check here.
  if (!desktop_android::IsWebstoreOrigin(source_url())) {
    LOG(WARNING) << "[Afterbird] webstorePrivate called from non-store origin: "
                 << source_url();
    return RespondNow(Error("unknown_extension"));
  }
  return RespondNow(Error("not_implemented_yet"));
}
```

- [ ] **Step 2: Build and smoke the gate**

`autoninja -C out/android chrome_public_apk && \
  ssh serv 'cd ~/afterbird && ./tools/install_local.sh'` (or whatever the
repo's existing deploy script is — check `tools/` and README if unfamiliar;
worst case: `adb -s R5CTA2MHJFA install -r out/android/apks/ChromePublic.apk`).

On device, open `chrome://extensions` and in DevTools console paste:

```js
chrome.webstorePrivate.beginInstallWithManifest3({id: "a".repeat(32)}, () => {
  console.log(chrome.runtime.lastError?.message);
});
```

Expected: `"unknown_extension"` (chrome://extensions is not a CWS origin).

- [ ] **Step 3: Extract the extension id from params and forward to coordinator**

Now replace the body with the full implementation:

```cpp
ExtensionFunction::ResponseAction
WebstorePrivateBeginInstallWithManifest3Function::Run() {
  // Origin gate.
  if (!desktop_android::IsWebstoreOrigin(source_url())) {
    LOG(WARNING) << "[Afterbird] webstorePrivate called from non-store origin: "
                 << source_url();
    return RespondNow(Error("unknown_extension"));
  }
  // Params: the first argument is a Details dict with at minimum `id`.
  if (args().empty() || !args()[0].is_dict()) {
    return RespondNow(Error("invalid_arguments"));
  }
  const std::string* id = args()[0].GetDict().FindString("id");
  if (!id || id->size() != 32) {
    return RespondNow(Error("invalid_id"));
  }
  // Id shape check: lowercase a-p only.
  for (char c : *id) {
    if (c < 'a' || c > 'p') {
      return RespondNow(Error("invalid_id"));
    }
  }
  // Hand off to the coordinator.
  content::WebContents* web_contents = GetSenderWebContents();
  content::BrowserContext* context = browser_context();
  if (!web_contents || !context) {
    return RespondNow(Error("no_web_contents"));
  }
  const GURL crx_url = desktop_android::BuildWebstoreCrxUrl(*id);
  LOG(INFO) << "[Afterbird] webstorePrivate install id=" << *id
            << " crx=" << crx_url;
  CrxInstallCoordinator::StartFromWebstore(context, web_contents, crx_url,
                                           "Chrome Web Store");
  // The CWS page expects a result enum string. Returning an empty string
  // for the success_code slot matches upstream's "" default.
  base::Value::List result;
  result.Append("");  // result_code
  return RespondNow(ArgumentList(std::move(result)));
}
```

- [ ] **Step 4: Build**

Run: `autoninja -C out/android chrome_public_apk`

Expected: success. If `GetSenderWebContents()` isn't visible in the
`ExtensionFunction` base from our override path, the compiler will say so
— `#include "content/public/browser/web_contents.h"` is already at the top
and the method is on `ExtensionFunction`. If unavailable, fall back to
`content::WebContents::FromRenderFrameHost(render_frame_host())`.

- [ ] **Step 5: Commit**

```bash
git add chrome/browser/extensions/desktop_android/webstore_private/webstore_private_api.cc
git commit -m "feat(extensions): wire beginInstallWithManifest3 to CrxInstallCoordinator

Validates origin is CWS and id is 32-char [a-p], builds the update2/crx
URL via the shared util, hands off to the existing coordinator. The
confirm dialog + install pipeline is unchanged — only the trigger moves
from navigation-throttle to webstorePrivate extension-function call."
```

---

## Task 5: Delete the navigation-throttle

**Files:**
- Delete: `chrome/browser/extensions/desktop_android/extension_install_navigation_throttle.cc`
- Delete: `chrome/browser/extensions/desktop_android/extension_install_navigation_throttle.h`
- Modify: `chrome/browser/BUILD.gn`
- Modify: `chrome/browser/chrome_content_browser_client.cc`

- [ ] **Step 1: Remove from BUILD.gn**

Edit `chrome/browser/BUILD.gn`. Delete these four lines (around line 7837-7838):

```gn
      # Afterbird v1.1: NavigationThrottle that turns direct .crx /
      # .user.js navigations into an install.
      "extensions/desktop_android/extension_install_navigation_throttle.cc",
      "extensions/desktop_android/extension_install_navigation_throttle.h",
```

Rewrite the adjacent v1.2 comment to reflect the new reality; change:

```gn
      # Afterbird v1.2: orchestrates fetch → unpack → confirm → install,
      # replacing the v1.1 silent-install CrxDownloadInstaller. Owns the
      # SimpleURLLoader, staging dir, and the JNI confirm-dialog bridge.
      "extensions/desktop_android/crx_install_coordinator.cc",
      "extensions/desktop_android/crx_install_coordinator.h",
```

to:

```gn
      # Afterbird v1.2 / v1.4: orchestrates fetch → unpack → confirm →
      # install. Triggered by ShouldInterceptDownload (raw .crx URL) and
      # webstorePrivate.beginInstallWithManifest3 (CWS detail page).
      "extensions/desktop_android/crx_install_coordinator.cc",
      "extensions/desktop_android/crx_install_coordinator.h",
```

- [ ] **Step 2: Remove the include and the throttle registration in chrome_content_browser_client.cc**

Edit `chrome/browser/chrome_content_browser_client.cc`.

At line ~701, delete the include block:

```cpp
#if BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
// Afterbird: throttle that turns .crx / .user.js navigations into an
// install instead of a download / open-with prompt.
#include "chrome/browser/extensions/desktop_android/extension_install_navigation_throttle.h"
#endif
```

At line ~5474, delete the registration block:

```cpp
#if BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
  // Afterbird: intercept .crx / .user.js navigations so clicking an
  // extension link on any page installs through our pipeline instead of
  // prompting an "open-with" dialog that Android can't satisfy.
  MaybeAddThrottle(
      extensions::ExtensionInstallNavigationThrottle::MaybeCreate(handle),
      &throttles);
#endif
```

- [ ] **Step 3: Delete the throttle source files**

```bash
git rm chrome/browser/extensions/desktop_android/extension_install_navigation_throttle.cc chrome/browser/extensions/desktop_android/extension_install_navigation_throttle.h
```

- [ ] **Step 4: Build**

Run: `autoninja -C out/android chrome_public_apk`

Expected: success. If a stray `#include` of the deleted header remains
anywhere, the compiler will tell you; grep for `extension_install_navigation_throttle`
to verify none are left:

```bash
grep -rln "extension_install_navigation_throttle" chrome/ --include="*.cc" --include="*.h" --include="*.gn" --include="*.gni"
```

Expected: no matches.

- [ ] **Step 5: Commit**

```bash
git add chrome/browser/BUILD.gn chrome/browser/chrome_content_browser_client.cc
git commit -m "feat(extensions): remove CWS navigation throttle

Detail pages now render normally. The install trigger is
webstorePrivate.beginInstallWithManifest3, called by the page's own
Install button. ShouldInterceptDownload still catches raw .crx URLs
from non-CWS sources, so the install-from-file pipeline remains
reachable for github-release-style .crx links."
```

---

## Task 6: Add "+ (from store)" button to chrome://extensions toolbar

**Files:**
- Modify: `chrome/browser/resources/extensions/toolbar.ts`
- Modify: `chrome/browser/resources/extensions/toolbar.html.ts`
- Modify: `chrome/browser/ui/webui/extensions/extensions_ui.cc`

- [ ] **Step 1: Register the i18n string in the WebUI handler**

Edit `chrome/browser/ui/webui/extensions/extensions_ui.cc`. Find the block added around line 437 for desktop-android `AddString` fallbacks:

```cpp
#if !BUILDFLAG(ENABLE_EXTENSIONS)
  source->AddString("mv2DeprecationPanelTitle", "");
  source->AddString("mv2DeprecationPanelDismissButton", "");
  …
  source->AddString("mv2DeprecationUnsupportedExtensionOffText", "");
#endif
```

Add after it (still inside the same `#if !BUILDFLAG(ENABLE_EXTENSIONS)` block):

```cpp
  // Afterbird: + (from store) button.
  source->AddString("toolbarLoadFromStore", "+ (from store)");
  source->AddString("toolbarLoadFromStoreTooltip",
                    "Open the Chrome Web Store to find new extensions");
#endif
```

Actually keep that separation clean — rewrite the block so Android gets both:

```cpp
#if !BUILDFLAG(ENABLE_EXTENSIONS)
  source->AddString("mv2DeprecationPanelTitle", "");
  // … all the mv2 strings unchanged …
  source->AddString("mv2DeprecationUnsupportedExtensionOffText", "");
  // Afterbird v1.4: CWS entry button. These keys aren't referenced by any
  // upstream strings file, so always fall back to plain-English text here.
  source->AddString("toolbarLoadFromStore", "+ (from store)");
  source->AddString("toolbarLoadFromStoreTooltip",
                    "Open the Chrome Web Store to find new extensions");
#endif
```

- [ ] **Step 2: Add the click handler to toolbar.ts**

Edit `chrome/browser/resources/extensions/toolbar.ts`. Find
`onLoadUnpackedClick_` (around line 181). Add **above** it:

```ts
  protected onLoadFromStoreClick_() {
    chrome.metricsPrivate.recordUserAction('Options_OpenExtensionsWebStore');
    window.open('https://chromewebstore.google.com/category/extensions');
  }
```

Also add `loadFromStore: HTMLElement,` to the `$` dictionary in
`ExtensionsToolbarElement` (around line 50):

Before:

```ts
export interface ExtensionsToolbarElement {
  $: {
    devDrawer: HTMLElement,
    devMode: CrToggleElement,
    loadUnpacked: HTMLElement,
    packExtensions: HTMLElement,
    toolbar: CrToolbarElement,
    updateNow: HTMLElement,
  };
}
```

After:

```ts
export interface ExtensionsToolbarElement {
  $: {
    devDrawer: HTMLElement,
    devMode: CrToggleElement,
    loadFromStore: HTMLElement,
    loadUnpacked: HTMLElement,
    packExtensions: HTMLElement,
    toolbar: CrToolbarElement,
    updateNow: HTMLElement,
  };
}
```

- [ ] **Step 3: Render the button in toolbar.html.ts**

Edit `chrome/browser/resources/extensions/toolbar.html.ts`. Find:

```ts
  <div id="buttonStrip">
    <cr-button ?hidden="${!this.canLoadUnpacked_()}" id="loadUnpacked"
        @click="${this.onLoadUnpackedClick_}">
      $i18n{toolbarLoadUnpacked}
    </cr-button>
```

Replace with:

```ts
  <div id="buttonStrip">
    <cr-button id="loadFromStore"
        @click="${this.onLoadFromStoreClick_}"
        title="$i18n{toolbarLoadFromStoreTooltip}">
      $i18n{toolbarLoadFromStore}
    </cr-button>
    <cr-button ?hidden="${!this.canLoadUnpacked_()}" id="loadUnpacked"
        @click="${this.onLoadUnpackedClick_}">
      $i18n{toolbarLoadUnpacked}
    </cr-button>
```

- [ ] **Step 4: Build**

Run: `autoninja -C out/android chrome_public_apk`

Expected: success. If the Lit-template preprocessor complains about the
unused `$i18n{toolbarLoadFromStoreTooltip}` on non-android builds, relax
by dropping the tooltip attribute for now — the button text is
self-explanatory.

- [ ] **Step 5: Commit**

```bash
git add chrome/browser/resources/extensions/toolbar.ts chrome/browser/resources/extensions/toolbar.html.ts chrome/browser/ui/webui/extensions/extensions_ui.cc
git commit -m "feat(extensions): add '+ (from store)' button to chrome://extensions

Primary CWS discovery entry point. Button opens
https://chromewebstore.google.com/category/extensions in a new tab.
Unlike the dev-mode load-unpacked button, this is always visible — it
is the normal-user install path."
```

---

## Task 7: Device-side verification

**Files:** none (smoke only).

- [ ] **Step 1: Install the APK**

```bash
autoninja -C out/android chrome_public_apk
adb -s R5CTA2MHJFA install -r out/android/apks/ChromePublic.apk
```

Expected: `Success` line at the bottom of `adb install` output.

- [ ] **Step 2: Open chrome://extensions and verify the button renders**

```bash
adb -s R5CTA2MHJFA shell am start -n com.danosito.afterbird/com.google.android.apps.chrome.Main -a android.intent.action.VIEW -d "chrome://extensions"
sleep 3
adb -s R5CTA2MHJFA shell screencap -p /sdcard/smoke_ext_page.png
adb -s R5CTA2MHJFA pull /sdcard/smoke_ext_page.png /tmp/smoke_ext_page.png
```

Expected: screenshot shows the extensions list with a `+ (from store)`
button above `Load unpacked`. If the button is missing, re-grep for
`toolbarLoadFromStore` and confirm the `AddString` lands in the output
of `chrome://extensions` resource bundle. Common miss: wrong
`#if !BUILDFLAG(ENABLE_EXTENSIONS)` gate — on our build that flag **is**
false (we use `ENABLE_DESKTOP_ANDROID_EXTENSIONS`), so that gate is
correct. If the text displays as literal `$i18n{toolbarLoadFromStore}`,
the key was not registered — re-check extensions_ui.cc.

- [ ] **Step 3: Tap the button, verify CWS homepage loads**

Open the saved screenshot, find the center pixel coords of the button
(roughly `x=250, y=450` in the 1080-wide viewport — adjust per your
snapshot). Then:

```bash
adb -s R5CTA2MHJFA shell input tap 250 450
sleep 4
adb -s R5CTA2MHJFA shell screencap -p /sdcard/smoke_cws.png
adb -s R5CTA2MHJFA pull /sdcard/smoke_cws.png /tmp/smoke_cws.png
```

Expected: a new tab is on `chromewebstore.google.com/category/extensions`
with the store rendering normally.

- [ ] **Step 4: Navigate to a detail page, verify it renders (no throttle)**

```bash
adb -s R5CTA2MHJFA shell am start -n com.danosito.afterbird/com.google.android.apps.chrome.Main -a android.intent.action.VIEW -d "https://chromewebstore.google.com/detail/ublock-origin-lite/ddkjiahejlhfcafbddmgiahcphecmpfh"
sleep 6
adb -s R5CTA2MHJFA shell screencap -p /sdcard/smoke_detail.png
adb -s R5CTA2MHJFA pull /sdcard/smoke_detail.png /tmp/smoke_detail.png
```

Expected: full detail page rendering (uBlock Origin Lite title, rating,
screenshots carousel, description). Crucially: **no** immediate confirm
dialog. If a confirm dialog pops up, the throttle wasn't fully removed
— re-grep `chrome_content_browser_client.cc` for
`ExtensionInstallNavigationThrottle`.

- [ ] **Step 5: Tap Install on the detail page, verify our confirm dialog opens**

Find the Install button coords from the screenshot (roughly `x=890, y=310`
for a 1080x2640 device at the top-right of the content area). Then:

```bash
adb -s R5CTA2MHJFA logcat -c
adb -s R5CTA2MHJFA shell input tap 890 310
sleep 3
adb -s R5CTA2MHJFA shell screencap -p /sdcard/smoke_confirm.png
adb -s R5CTA2MHJFA pull /sdcard/smoke_confirm.png /tmp/smoke_confirm.png
adb -s R5CTA2MHJFA logcat -d | grep -Ei "afterbird|webstoreprivate|CrxInstallCoordinator" | head -20 > /tmp/smoke_install.log
cat /tmp/smoke_install.log
```

Expected:
- Logcat contains `[Afterbird] webstorePrivate install id=...` and
  `CrxInstallCoordinator` fetch lines.
- Screenshot shows our Java confirm dialog (`ExtensionInstallConfirmBridge`)
  titled `Install "uBlock Origin Lite"?` with an extension name,
  version, permissions list, and the fine-print paragraph.

If instead the tap mis-fires to a surrounding affordance, the user is
not signed into CWS, or a Google coachmark intercepts, document the
miss and try again — this is the same class of UI-tap instability the
diagnostic noted.

- [ ] **Step 6: Confirm install — extension lands in chrome://extensions**

Tap the Install button in our confirm dialog (coordinates depend on
screenshot — roughly bottom-right `x=650, y=1900`), then:

```bash
adb -s R5CTA2MHJFA shell input tap 650 1900
sleep 4
adb -s R5CTA2MHJFA shell am start -n com.danosito.afterbird/com.google.android.apps.chrome.Main -a android.intent.action.VIEW -d "chrome://extensions"
sleep 3
adb -s R5CTA2MHJFA shell screencap -p /sdcard/smoke_installed.png
adb -s R5CTA2MHJFA pull /sdcard/smoke_installed.png /tmp/smoke_installed.png
```

Expected: `chrome://extensions` shows a card for uBlock Origin Lite.

- [ ] **Step 7: Test the raw .crx download path (regression check)**

Find a raw .crx URL — a GitHub release of a small extension works.
One known-stable URL: `https://github.com/NicolasDelsaux/Grasp/releases/download/v0.1.8/grasp-0.1.8.crx`
(verify availability via `curl -I` first; if 404, substitute any public
.crx URL).

```bash
adb -s R5CTA2MHJFA shell am start -n com.danosito.afterbird/com.google.android.apps.chrome.Main -a android.intent.action.VIEW -d "https://github.com/.../release.crx"
sleep 8
adb -s R5CTA2MHJFA shell screencap -p /sdcard/smoke_crx_download.png
adb -s R5CTA2MHJFA pull /sdcard/smoke_crx_download.png /tmp/smoke_crx_download.png
```

Expected: our confirm dialog fires (via `ShouldInterceptDownload`), NOT
a Downloads toast, NOT the "open with" system chooser. This validates
that deleting the throttle did not break the download-intercept path.

- [ ] **Step 8: Write a short verification note**

Append the five screenshots (`smoke_ext_page.png`, `smoke_cws.png`,
`smoke_detail.png`, `smoke_confirm.png`, `smoke_installed.png`) as
references in the diagnostic file:

Edit `docs/superpowers/diagnostics/2026-04-17-kiwi-install-flow.md` — at
the bottom add a final `## Verification (v1.4 post-change)` section with
five bullets pointing at `/tmp/` paths and a one-line observation each.

- [ ] **Step 9: Commit the verification note**

```bash
git add docs/superpowers/diagnostics/2026-04-17-kiwi-install-flow.md
git commit -m "docs(extensions): attach v1.4 install-flow smoke verification"
```

---

## Task 8: Cleanup — delete stale Polymer toolbar.html

**Files:**
- Delete (optional): `chrome/browser/resources/extensions/toolbar.html`

This file is dead code — the real template is `toolbar.html.ts`. It
still carries a `loadFromStore` button from the pre-Lit era. Deleting
stops it being mistaken for the live template during future edits.

- [ ] **Step 1: Verify nothing references it**

```bash
grep -rn "toolbar\\.html[\"']" chrome/ --include="*.ts" --include="*.js" --include="*.gn" --include="*.gni"
```

Expected: no matches pointing at the `.html` file directly. If a GN
file pulls it in as a data input, stop and adjust — the rename-to-ts is
not complete. (Sanity check: our other Lit-migrated files like
`extensions.html.ts` co-exist with stale `.html` siblings without
issue.)

- [ ] **Step 2: Delete and build**

```bash
git rm chrome/browser/resources/extensions/toolbar.html
autoninja -C out/android chrome_public_apk
```

Expected: build succeeds.

- [ ] **Step 3: Commit**

```bash
git commit -m "chore(extensions): delete dead Polymer toolbar.html

The live template is toolbar.html.ts (Lit). The .html sibling was left
behind from the pre-Lit migration and its stale 'loadFromStore' button
has caused confusion during install-flow work."
```

---

## Self-review notes

**Spec coverage check:**
- Throttle deletion → Task 5 ✓
- webstorePrivate shim (3 functions) → Tasks 2, 3, 4 ✓
- "+ (from store)" button + string → Task 6 ✓
- ShouldInterceptDownload untouched → no task needed (explicit) ✓
- CrxInstallCoordinator untouched → no task needed (explicit) ✓
- Verification → Task 7 ✓
- Legacy `chrome.google.com/webstore/detail/...` URL → handled by
  `IsWebstoreOrigin` in the shim; Google 301s these at the server, so
  no client rewrite needed. If verification in Task 7 finds the 301
  missing, add a follow-up task for the 15-line client rewrite; not
  included pre-emptively.

**Out of scope (explicit):**
- `chrome-extension://` UI install button (doesn't exist on our page)
- webstorePrivate surface beyond the 3 functions
- Publisher / signature / blocklist verification
- Upstream `ExtensionInstallPrompt` humanised permission warnings
- Telemetry

**Touch-up allowed mid-execution:**
- If the CWS page's current JS needs one more webstorePrivate call we
  didn't anticipate (e.g. `getStoreLogin` or `getIsLauncherEnabled`),
  add a minimal no-op stub and note it — don't block the task graph.
- If `GetSenderWebContents()` is not available on `ExtensionFunction`,
  fall back to `content::WebContents::FromRenderFrameHost(render_frame_host())`
  and adjust the include in Task 4.
