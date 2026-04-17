# Kiwi vs Afterbird Extension Parity Audit

Date: 2026-04-18
Device: `emulator-5554` (Android 15 emulator, API 35, arm64, 320x640)
Builds compared:

- **Kiwi Browser** `com.kiwibrowser.browser` versionName=`132.0.6961.0` (versionCode 696100004)
- **Afterbird** `com.danosito.afterbird` versionName=`132.0.6834.83` (build tag v1.5.0, APK `/tmp/afterbird-v1.5rc3.apk`)

Both browsers coexist on the device, share the same
`/data/local/tmp/chrome-command-line` (`--load-extension=/data/local/tmp/ublock,
/data/local/tmp/mv2-probe,/data/local/tmp/mv3-probe`), and therefore start with
the same three command-line extensions. Store-installed extensions differ:
Afterbird already had Bitwarden, Honey, Dark Reader installed from the v1.4 and
v1.5 sessions; Kiwi started with no store-installed extensions and had Dark
Reader installed during this audit for like-for-like comparison of the runtime
pipeline.

No code was modified in this audit. Screenshots live at
`/tmp/parity-audit/*.png`.

## TL;DR — the three user-visible blockers

1. **uBlock Origin does not block ads on Afterbird** (P1 Blocker). Same
   extension, same command-line load path, same test URL
   (`adblock.turtlecute.org`) reports **3 blocked / 130 not blocked (2 %)** on
   Afterbird versus **~130/133 blocked (~97 %)** on Kiwi. uBO's static network
   filtering engine never boots on Afterbird because `chrome.tabs.query` /
   `chrome.contextMenus.create` / `chrome.browserAction.set*` are all denied —
   the extension sees `Access to extension API denied.` and aborts its
   initialisation chain. This is the single biggest reason a Kiwi user would
   not switch.
2. **Extension popups hang on "Loading, please wait" on Afterbird** (P1
   Blocker). Tapping uBlock Origin in the main menu opens the popup as a
   full-tab. It never finishes loading because the popup calls
   `chrome.tabs.query({active:true, currentWindow:true})` and the stub returns
   `undefined` instead of `[]` (see `docs/superpowers/api-coverage.md` and
   the stability-profiler "P1" note). On Kiwi the same popup renders fully,
   shows the page-specific block count, and responds to the filter toggle.
3. **Dark Reader does not darken pages on Afterbird** (P2 Major). The SW
   installs and is tracked in prefs, but the top-level script aborts because
   `chrome.fontSettings` / `chrome.management` are schema-only (no C++
   function implementations registered). On Kiwi the same extension installed
   via the CWS detail page immediately darkens the host page and all
   subsequently-visited pages.

Every other item tested below is **at or near parity** — Afterbird ships a
chrome://extensions UI that Kiwi does too, per-extension menu entries that
Kiwi pioneered, a working Chrome Web Store → confirm-dialog → CRX fetch
pipeline, an on-device DevTools entry in the main menu, and extension
persistence across force-stop.

---

## 1. Management UI — `chrome://extensions`

Both browsers land on the same Polymer page; Kiwi aliases `kiwi://extensions`
to it, Afterbird does not. Navigating via the URL bar works on both.

| Aspect | Kiwi | Afterbird | Notes |
| --- | --- | --- | --- |
| URL aliases | `kiwi://extensions`, `chrome://extensions` | `chrome://extensions` only | Minor. Users typing `kiwi://extensions` on Afterbird get "ERR_UNSAFE_REDIRECT" or blank. |
| Splash on first visit | "Important recommendation — crypto extensions are risky" dismissible sheet with OK button | None | Cosmetic. Kiwi re-shows it per session. |
| Top-bar header | "Extensions" + search + "Developer mode" toggle | "Extensions" + search + "Developer mode" toggle | Parity. |
| Install buttons | `+ (from store)`, `+ (from .zip/.crx/.user.js)`, `Pack extension`, `Update` | `+ (from store)` only. No `Pack extension`, no `Update`, no user.js support. | Major gap. `+ (from store)` opens CWS category page; that part matches. |
| Per-extension card fields | name, version, description, ID, Inspect views (links) | name, version, description, ID, Inspect views (links) | Parity. |
| Per-extension actions | Details, Remove, Reload, Enable toggle | Details, Remove, Reload, Enable toggle | Parity (layout differs slightly — Kiwi stacks vertically, Afterbird uses a card grid). |
| Details view | Description, Version, Size, Allow in Incognito, Allow access to file URLs, Collect errors, Source | Description, Version, Size (empty), Permissions, Site access, Site settings, Pin to toolbar, View in Chrome Web Store, Source | Different trade-off. Kiwi exposes incognito + file-URL toggles that Afterbird omits; Afterbird has "Pin to toolbar" that Kiwi omits. Neither Pin has any visible effect on Android. |
| Developer mode | Always-on (no toggle UI actually reported by observation; dev features always visible) | Toggle present; when off, hides Inspect views / Load unpacked / IDs. | Parity-ish. |
| Keyboard shortcut page | not tested | present via overflow menu | — |

Severity: Minor for the missing `.zip/.crx/.user.js` and `Pack extension`
buttons on Afterbird — `.crx` URLs are caught by the download throttle anyway,
.zip is installed via Load unpacked file-picker (dev-mode), but `.user.js`
userscripts are not supported at all. `Pack extension` is dev-only and rarely
used. No fix needed unless we want user.js support.

Screenshots:
`kiwi_ext6.png`, `kiwi_ext8.png`, `kiwi_details.png`, `kiwi_details2.png`,
`ab_extshow.png`, `ab_details.png`, `ab_ext_developer.png`.

## 2. Install flows

### 2.1 Chrome Web Store (CWS) detail page

| Step | Kiwi | Afterbird |
| --- | --- | --- |
| CWS homepage navigation | Renders normally. Full page. | Renders normally. Full page. |
| Detail page navigation | Renders normally (description, screenshots, rating, related). | **Renders normally now** (v1.4+ fix). Previously was intercepted at the navigation-throttle level; today the throttle only hands the CRX URL off to the coordinator, leaving the page visible. |
| Install button | Blue "Add to Chrome" in page. Page's own `webstorePrivate.beginInstallWithManifest3` JS call wires into upstream `WebstoreStandaloneInstaller` → `ExtensionInstallPrompt` → `CrxInstaller`. | The same blue button. Afterbird intercepts via `extension_install_navigation_throttle.cc` that recognises the navigation to `/detail/<slug>/<id>`, synthesises the clients2.google.com CRX URL, and calls `CrxInstallCoordinator::StartFromWebstore`. |
| Confirm dialog | Native Chromium modal: "Chrome Web Store / Add 'X'? It can: Read and change all your data on all websites. ATTENTION: Installing an extension is potentially harmful… WARNING: Using cryptocurrency extensions is NOT recommended…". **Humanised permission messages** via `PermissionMessageProvider`. | Native Android modal (`ExtensionInstallConfirmBridge`): "Install 'X'? Version: … Source: Chrome Web Store Permissions: alarms, cookies, storage, unlimitedStorage, scripting, webRequest, …. Extensions can read and change everything you see on the web…". **Raw manifest permission strings** (per-API list, not humanised). |
| Post-install | Extension appears in `chrome://extensions`. | Extension appears in `chrome://extensions` with the **locally-computed id derived from the public key in the CRX header**, which differs from the CWS id (Bitwarden `nngc…nclblb` → `fciinoccdmhaldfhgabcoaemoehngcnh`). See P2 in stability report. |

Gap: Afterbird's install dialog lists raw manifest strings instead of the
humanised "Read and change all your data on all websites" form. Pulling in
the `PermissionMessageProvider` humaniser is large (~3k LOC) and is
explicitly non-goals per the install-flow design doc. Severity: Minor — the
dialog is readable and communicates the essentials, but Kiwi's is friendlier.

Gap (**P2 Major**, documented elsewhere): Afterbird installs under a different
extension id than the store-canonical id. Breaks anything keyed on
`chrome.runtime.id` and makes re-install dedup impossible against the CWS id.

Severe bug discovered during this audit: **if the user cancels the install
dialog, queued dialogs from prior sessions reappear on every subsequent
launch.** Force-stopping the browser and relaunching via `adb am start
about:blank` with no tabs restored still showed 10+ queued Bitwarden / Honey
confirm dialogs one after another; the modal kept re-opening until
`Tab state` and `Extensions/afterbird_install_*` staging dirs were manually
deleted. Reproducible and user-hostile. Likely root cause: restored
NavigationController entries re-fire the install-navigation-throttle each
time the tab is restored, and staging dirs are orphaned without cleanup.
**Severity: P1 Blocker for any user who has cancelled an install** — looks
like the browser is malfunctioning until they find chrome://extensions to
clear state. Suggested fix: (a) clear the throttle cache keyed on
`source_url + extension_id` immediately on cancel, (b) do not re-fire the
throttle on a back-forward / restore navigation (check
`NavigationHandle::IsSameDocument` + `GetReloadType`), (c) GC orphan
`afterbird_install_*` dirs on startup if they have no matching prefs entry.

### 2.2 Raw `.crx` URL

Not re-tested this session. Per prior notes both Kiwi and Afterbird's
`ShouldInterceptDownload` catches raw `.crx` URLs and routes them into the
same install pipeline as CWS. Expected parity.

### 2.3 `.zip` → Load unpacked (file picker)

| Step | Kiwi | Afterbird |
| --- | --- | --- |
| Entry point | `+ (from .zip/.crx/.user.js)` button on extensions page | `+ (from store)` only; `.zip` comes in through dev-mode "Load unpacked" tile |
| Picker | `ACTION_OPEN_DOCUMENT` with `*/*` filter | `ACTION_OPEN_DOCUMENT` with `application/zip` + `application/x-chrome-extension` filter |
| After selection | Installs directly; success toast | Installs directly; success toast (`Extension installed`) |

Gap: Afterbird puts the `.zip` path behind a dev-mode gate, Kiwi doesn't.
Severity: Minor — one extra click once per install.

### 2.4 `.user.js` userscripts

- Kiwi: handled as a userscript via Tampermonkey-style inlining (clickable
  from the `+ (from .zip/.crx/.user.js)` picker).
- Afterbird: **not supported**. Source has a commented reference in
  `extension_installer.h:15` ("value, same as Kiwi Browser's '+ from
  .zip/.crx/.user.js' flow") but the concrete implementation in
  `extension_installer.cc:290-295` returns
  `"Unsupported file type (expected .zip, .crx, or a directory)"` for
  anything else.

Severity: Minor. Usercripts are a niche. Would require bundling a
userscript-to-MV2-extension shim (Tampermonkey's approach) or integrating
with UserScripts.

### 2.5 Inline install from non-CWS pages

- Kiwi: available (legacy inline-install JS API on pages with a matching
  CRX URL). Frequently disabled by publishers.
- Afterbird: **out of scope per `2026-04-18-kiwi-style-install-design.md`**.

Severity: Minor. Parity not needed; inline install is deprecated web-wide.

## 3. Main menu integration

### 3.1 Per-extension entries

Both browsers list enabled extensions at the bottom of the three-dot menu.

| Field | Kiwi | Afterbird |
| --- | --- | --- |
| Entries present | Yes — one per enabled extension | Yes — one per enabled extension |
| Icon rendering | **Real extension icon (coloured, drawn from `IconWithBadgeImageSource` canvas → base64 data URL in `app_menu.cc` → `BitmapDrawable`)** | **Grey square**. The base64 PNG bytes are decoded in `ExtensionMenuManager.java:107-119` and set via `MenuItem.setIcon`, but Android's `AppCompat` menu tints icons with a single theme colour by default, collapsing them to silhouettes. |
| Action on tap | Grant active-tab permission, open popup URL in new tab | Open popup URL in new tab (no active-tab grant — `JNI_AppMenuBridge_GrantExtensionActiveTab` in `app_menu_bridge.cc:154-164` is a no-op stub) |
| Popup loads | Yes — fully interactive, shows page-keyed block count | No — hangs on "Loading, please wait" because `chrome.tabs.query` returns `undefined` |

Gap 3.1.a: **Icon tinting** (Minor). To keep the extension icon in colour,
set `MenuItemCompat.setIconTintList(item, null)` or disable `iconTint` on the
menu style. Kiwi uses a custom `AppMenuItemIcon` view that bypasses the
tinting.

Gap 3.1.b: **No active-tab grant** (Major — feeds the P1 popup-hang). The
stub in `app_menu_bridge.cc` needs to actually call
`ActiveTabPermissionGranter::GrantIfRequested` for the tapped extension.
The method depends on `ExtensionService`, which is not fully available on
desktop-android, but the critical piece — calling
`ExtensionActionRunner::RunAction(extension, /*grant_tab_permissions=*/true)`
— is reachable via the `extensions::ExtensionActionRunner` already compiled
in.

Screenshots: `kiwi_menu.png`, `kiwi_menu2.png`, `kiwi_ubo.png`,
`ab_menu.png`, `ab_menu2.png`, `ab_ubopopup.png`, `ab_ubopopup2.png`.

### 3.2 Popup UX

Both browsers open popups as a new tab. This is a desktop-android limitation
— there is no toolbar to anchor a popover to. Kiwi's equivalent is the same
"new tab" approach.

## 4. Runtime functionality

### 4.1 uBlock Origin 1.62.0 (MV2) — ad blocking

Test URL: `https://adblock.turtlecute.org/` (same filter list on both
browsers; uBO loaded identically via `--load-extension=/data/local/tmp/ublock`).

| Browser | Total ad slots | Blocked | Pass rate |
| --- | --- | --- | --- |
| Kiwi | 133 | ~130 (97 %) | PASS — uBO functional |
| Afterbird | 133 | **3 (2 %)** | **FAIL — uBO not functional** |

The 3 blocked on Afterbird are blocked by the page's static HTML test
filter, not by uBO. uBO's `µBlock.staticNetFilteringEngine` stays undefined
after startup because:

- `chrome.tabs.query` denied (187 log hits across a 6-minute session)
- `chrome.browserAction.setIcon` / `.setBadgeText` / `.setBadgeBackgroundColor` denied
- `chrome.contextMenus.create` / `.remove` denied
- `chrome.types.ChromeSetting.set` denied

Each miss logs `E/chromium:[ERROR:extension_function_dispatcher.cc(545)]
Unknown Extension API - <name>`. uBO's internal bootstrap short-circuits
after the first few and never compiles the filter lists it has already
downloaded.

Severity: **P1 Blocker.** This is the single most important regression
versus Kiwi. The fix is mechanical: wire
`ChromeExtensionsBrowserAPIProvider` (or a desktop-android-specific subset)
into `DesktopAndroidExtensionsBrowserClient::ctor` so the `tabs.query`,
`browserAction.*`, `contextMenus.*` functions are registered in the
`ExtensionFunctionRegistry`. Blocked on the same "make `tabs_api.cc`
compile on desktop-android" follow-up that the popup-hang fix needs.

Screenshots: `kiwi_adtest3.png` (97 % blocked), `ab_adtest2.png`
(2 % blocked).

### 4.2 uBlock Origin popup

See §3.1 above. Kiwi: functional. Afterbird: "Loading, please wait" stuck.
The filter toggle never renders.

### 4.3 Dark Reader 4.9.124 (MV3, SW) — page darkening

- Installed from CWS via the store detail page on both browsers.
- Kiwi (after install): visiting `example.com` renders the page with a dark
  background and light text. Visiting Dark Reader's help page on
  darkreader.org renders in native dark (it's a DR-aware page). DR is
  functional.
- Afterbird (after install): same two URLs render **light**. No
  `[data-darkreader-*]` attributes injected. SW idles. Prior log analysis:
  `GetAPISchema miss for "fontSettings"` + `GetAPISchema miss for
  "management"` during SW startup; Dark Reader's `configureCustomWebsites`
  throws, top-level exits, `chrome.storage.local.get(null)` returns `{}`.

Severity: **P2 Major.** Second most important "Kiwi does this, Afterbird
doesn't." Fix path: register `fontSettings` as a stub-ok API returning
sane defaults, and either register `management` as stub-ok or make
`chrome.management` undefined so DR's existence check skips its management
code path.

Screenshots: `kiwi_dr_example.png` (darkened), `ab_dark.png` (light).

### 4.4 Bitwarden Password Manager (MV3) — password unlock

- Afterbird: SW spins up, then `BadgeService: Fatal error updating badge
  state. TypeError: Cannot read properties of undefined (reading 'filter')`
  every wake. SW is respawned at least 5 times in a 6-minute session. Popup
  not tested (same popup-hang as uBO). Login / unlock **not functional**.
  Stability report documents the 1-line fix (`tabs.query` stub should
  return `[]` not `undefined`) as P1.
- Kiwi: not tested this session. Historical reports confirm Bitwarden
  works on Kiwi.

Severity: P1 Blocker for any user who wants a working password manager.

### 4.5 Honey (MV3) — coupon auto-apply

- Afterbird: installs cleanly, SW starts without error, emits 6 `cookies.getAll`
  Unknown API misses in the first session. Functional effect on a real
  shopping page not exercised.
- Kiwi: not tested this session.

Severity: likely parity once `cookies.getAll` is registered (currently
schema-only).

## 5. DevTools

Both browsers have a "Developer tools" entry in the main menu (Kiwi's has a
terminal-prompt icon; Afterbird's uses the same glyph). Both entries
lazily start a DevTools server and open the inspector frontend in a new
tab.

| Aspect | Kiwi | Afterbird |
| --- | --- | --- |
| Menu entry | Present | Present (v1.5.0, shipped this week) |
| Lazy server start | Yes | Yes — `DevToolsBridge.open()` in `devtools_bridge.cc` |
| Frontend URL | `chrome-devtools-frontend.appspot.com/serve_rev/@<git-sha>/inspector.html?ws=127.0.0.1:<port>/devtools/page/<target-id>` | **Malformed: `@@03d59...` (double `@`) → Google's appspot returns 404.** |
| Result | Opens the DevTools UI, attaches to the current tab | "Not Found" rendered, frontend never loads |
| Remote debug (adb forward) | Works via `localabstract:chrome_devtools_remote` | Socket exists on the device (`/proc/net/unix` shows it) but refuses connections from host-forwarded TCP; `curl` hangs (P4 from stability report). |

Severity: **P2 Major** for the malformed URL — one-liner fix to drop the
duplicate `@` in `DevToolsBridge::BuildFrontendUrl()` (or wherever the
string formatter is) in `devtools_bridge.cc`. Without it the menu entry
is cosmetic.

Evidence: `ab_devtools_tapped.png` (404), `ab_url.xml`
(`text="chrome-devtools-frontend.appspot.com/serve_rev/@@03d59cf5ecf1d8444838ff9a1e96231304d4ff9c/inspector.html?ws=127.0.0.1:45769/devtools/page/7145754649482A293157C1278C652DBD"`).

### 5.1 Per-extension DevTools (Inspect views)

- Kiwi: the `chrome://extensions` per-card "Inspect views: background.html"
  is a real link to the DevTools frontend scoped to that extension's
  background page. Tapping it attaches an inspector.
- Afterbird: the same link is present in markup but on tap it opens the
  extension's `chrome-extension://.../background.html` URL directly in a
  new tab (rendered as a regular page, not as the inspected target). No
  inspector attaches. Requires the §5 frontend URL to work first.

Severity: P3 Minor (blocked on §5 fix).

## 6. Persistence

Both browsers use standard Chromium `ExtensionPrefs` on disk; extensions
persist across app kill + relaunch. Verified on Afterbird (`state=1` for all
6 extensions after `am force-stop`). Not re-verified on Kiwi this session;
historical reports confirm Kiwi persists likewise.

Extensions do **not** survive app-data wipe (`adb shell pm clear`). That is
expected and matches Kiwi.

Severity: Parity. No gap.

Orphan staging dirs: Afterbird leaks `afterbird_install_*` directories in
`Extensions/` every time the user cancels a pending install, and the
directories never get GC'd (see §2.1). Kiwi has no equivalent staging
pattern (it uses upstream `CrxInstaller` which cleans up on cancel).
Severity: P3 Minor (disk bloat).

## 7. Permissions model

| Aspect | Kiwi | Afterbird |
| --- | --- | --- |
| Install-time permission prompt | Humanised ("Read and change all your data on all websites"). | Raw manifest list (`activeTab, alarms, clipboardRead, …`). |
| Per-site access granularity (activeTab, all-sites, on click) | Not exposed in the Android UI; host-permissions behave per the manifest as granted-at-install. | Same. Afterbird's `permissions.getAll` and `permissions.contains` stubs reconstruct the granted set from the manifest; no runtime grant tracking. |
| Runtime permission modification | Not surfaced in UI. | Not surfaced in UI. |
| Optional permissions (`permissions.request`) | Works (upstream desktop path). | Denied — `chrome.permissions.request` is schema-only; no UI to consent. |

Severity: P3 Minor. Users rarely request optional permissions at runtime
on Android; extensions that do either degrade gracefully or fail silently.

## End-to-end test matrix (re-test this session)

Legend: P = passes (exercises primary function), F = fails, N = not tested
this session.

| Extension | Install flow | In management UI | Background alive | Primary function works |
| --- | --- | --- | --- | --- |
| uBlock Origin (MV2) on **Kiwi** | P (command-line, pre-loaded) | P | P | **P** — 97 % of ads blocked on turtlecute test |
| uBlock Origin (MV2) on **Afterbird** | P (command-line, pre-loaded) | P | P | **F** — 2 % of ads blocked, filter engine never compiles |
| Dark Reader (MV3 SW) on **Kiwi** | P (CWS detail page → confirm → install) | P | P | **P** — example.com darkens |
| Dark Reader (MV3 SW) on **Afterbird** | P (CWS detail page → confirm → install, v1.4 flow) | P | P (SW tracked in prefs) | **F** — example.com stays light; SW top-level aborts |
| Bitwarden (MV3 SW) on **Kiwi** | N | N | N | N |
| Bitwarden (MV3 SW) on **Afterbird** | P (prior session) | P | Partial (SW respawns every BadgeService crash) | **F** — popup hangs; SW keeps crashing |
| Honey (MV3 SW) on **Kiwi** | N | N | N | N |
| Honey (MV3 SW) on **Afterbird** | P (prior session) | P | P | Partial — SW runs clean; real shopping-page exercise not run |

No extension crashes, ANRs, or SIGSEGVs observed during this audit. Kiwi
did exhibit two `DRAW_FAILED` warnings loading the CWS detail page, both
benign.

## Summary table — ranked gaps

| # | Gap | Severity | Suggested fix | Estimated LOC |
| --- | --- | --- | --- | --- |
| 1 | uBO does not block ads because `tabs.query` / `browserAction.*` / `contextMenus.*` are schema-only | **P1 Blocker** | Register `tabs.query` et al. via a stub returning `[]` and empty success; longer term wire `ChromeExtensionsBrowserAPIProvider`. | 10 (stub) / ~300 (real) |
| 2 | Extension popups hang on "Loading, please wait" | **P1 Blocker** | Same fix as #1 — the popup is stuck on `tabs.query`. Additionally actually-grant-activeTab in `JNI_AppMenuBridge_GrantExtensionActiveTab`. | 20 |
| 3 | Queued install dialogs re-appear on every launch after a cancel | **P1 Blocker (UX)** | Cache cancellation decision per `source_url + id`; skip the throttle for back-forward / tab-restore navigations; GC orphan `afterbird_install_*` dirs on startup. | ~60 |
| 4 | Dark Reader does not darken pages | **P2 Major** | Stub-ok `fontSettings` (return defaults) and either stub-ok or delete-from-schema `management`. | 30 |
| 5 | Store-install assigns a local id that differs from the CWS id | **P2 Major** | Plumb the manifest `key` field through `CrxInstallCoordinator::StartFromWebstore`. | 40 |
| 6 | DevTools frontend URL is malformed (`@@<sha>`) — 404 on open | **P2 Major** | Drop duplicate `@` in `DevToolsBridge::BuildFrontendUrl()`. | 1 |
| 7 | Main-menu extension icons render as grey squares | P3 Minor | `MenuItemCompat.setIconTintList(item, null)` in `ExtensionMenuManager.decodeIcon`. | 1 |
| 8 | Install confirm dialog shows raw manifest permissions, not humanised | P3 Minor | Copy a tiny subset of `ChromePermissionMessageProvider::GetPermissionMessages` (~200 LOC) into our `ExtensionInstallConfirmBridge`. Not blocking. | ~200 |
| 9 | No `.user.js` userscript install path | P3 Minor | Out of scope (niche). If wanted, wrap userscript in a synthetic MV2 extension shim. | ~500 |
| 10 | Per-extension DevTools "Inspect views" does not attach an inspector | P3 Minor | Same fix as #6; the URL points to the DevTools frontend. | — |
| 11 | No "Pack extension" / "Update" buttons on chrome://extensions | P4 Cosmetic | Re-enable in `toolbar.html.ts`. Rarely used. | 10 |
| 12 | No "Allow in Incognito" / "Allow access to file URLs" toggles on Details | P4 Cosmetic | Details-page Polymer wiring. | 20 |
| 13 | Orphan `afterbird_install_*` directories leak disk (~10 MB each) | P3 Minor | Merges into #3. | — |
| 14 | No `kiwi://extensions` alias on Afterbird (only `chrome://`) | P4 Cosmetic | Not worth it — Afterbird is not "Kiwi" branded. | 0 |

## Cross-references

- Design docs: `docs/superpowers/specs/2026-04-18-kiwi-style-install-design.md`,
  `docs/superpowers/specs/2026-04-18-devtools-kiwi-style-design.md`,
  `docs/superpowers/specs/2026-04-16-extension-management-design.md`.
- Diagnostics: `docs/superpowers/diagnostics/2026-04-17-kiwi-install-flow.md`,
  `docs/superpowers/diagnostics/2026-04-17-popup-disconnect.md`.
- API coverage matrix: `docs/superpowers/api-coverage.md`.
- Prior runtime reports:
  `docs/superpowers/reports/2026-04-17-api-probe-and-heavy-ext.md`,
  `docs/superpowers/reports/2026-04-18-stability-profiler.md`.

## Test artefacts (on host)

All screenshots and UI dumps collected during this audit live under
`/tmp/parity-audit/`. Key files:

```
kiwi_ext6.png         kiwi chrome://extensions, Dev-mode visible
kiwi_ext8.png         kiwi chrome://extensions, uBO + probes listed
kiwi_details.png      kiwi per-extension Details — header
kiwi_details2.png     kiwi per-extension Details — scroll (Incognito / file URL toggles)
kiwi_menu.png         kiwi main menu — top
kiwi_menu2.png        kiwi main menu — per-extension entries with coloured icons
kiwi_ubo.png          kiwi uBO popup — functional, shows block count
kiwi_cws_detail.png   kiwi CWS detail page — full render
kiwi_dr_confirm.png   kiwi confirm dialog — humanised "Read and change all your data"
kiwi_dr_installed.png kiwi Dark Reader installed, darkreader.org rendered
kiwi_dr_example.png   kiwi Dark Reader on example.com — darkened
kiwi_adtest3.png      kiwi turtlecute test — 97% blocked
ab_extshow.png        afterbird chrome://extensions — cards grid
ab_details.png        afterbird per-extension Details — Pin to toolbar visible
ab_menu.png           afterbird main menu — top (with Developer tools entry)
ab_menu2.png          afterbird main menu — per-extension entries with grey squares
ab_ubopopup2.png      afterbird uBO popup — "Loading, please wait" (hang)
ab_dark.png           afterbird Dark Reader on example.com — not darkened
ab_adtest2.png        afterbird turtlecute test — 2% blocked
ab_devtools_tapped.png afterbird DevTools menu tap — 404 on appspot
```
