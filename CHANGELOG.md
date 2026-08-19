# Changelog

All notable changes to this repository are documented in this file.

## v1.9.0 - 2026-08-19

### Changed

- **Chromium baseline bumped 132.0.6834.83 → 151.0.7922.38.** The build now
  uses the upstream desktop-android extension stack
  (`is_desktop_android=true`) instead of the custom 132-era
  `desktop_android` layer. Validated on device: uBlock Origin network
  blocking ~85–95% on adblock.turtlecute.org vs 3% on the old layer
  (see `docs/superpowers/reports/2026-07-16-m151-stock-plus-mv2patch.md`
  and `docs/superpowers/diagnostics/2026-07-17-webrequest-browseraction-fatal.md`).
- **Repository restructured to a delta model.** Pruned the ~9.8k-file
  132/Kiwi tracked-source subset (`base/`, `chrome/`, `components/`,
  `content/`, `extensions/`, `net/`, `remoting/`, `services/`,
  `third_party/` sans `extensions/`, `ui/`, Chromium root meta files).
  The repo now carries only: branding
  (`chrome/android/java/res_chromium_base/**`), `patches/m151/*.patch`,
  API probe extensions, args variants, automation, docs. History and the
  `chromium`/`kiwi` branches preserve the old subset.
- **Pipeline (`ci/chromium_android_pipeline.sh`):** overlay switched from
  exclude-list (everything tracked) to include-list (branding only); new
  `apply_patches` step (`git apply` with --check / --reverse / --3way
  gates, dir derived from `CHROMIUM_MAJOR`); GN args variant selection
  via `AFTERBIRD_ARGS_VARIANT=test|release`; `~/depot_tools` auto-added
  to PATH when `gclient` is absent.
- **GN args:** `.build/production_build_reference/args.gn` (132/Kiwi-era
  flags) replaced by `.build/args/test.gn` (validated M151 set,
  debuggable, CDP socket for the harness — default) and
  `.build/args/release.gn` (`is_official_build=true`,
  `chrome_pgo_phase=0`, experimental/unvalidated).
- `patches/m151/0001-mv2-reenable.patch` regenerated as a valid
  git-apply-able unified diff (was prose hunk header).

### Removed

- Kiwi/132-era custom behaviors superseded by upstream M151
  desktop-android: custom install dialog + navigation throttle,
  `chrome://extensions` bridges, DevTools bridge, permission-grant fix
  (v1.8), API stubs (v1.4–v1.7). Re-porting anything still missing on the
  phone form factor is explicit follow-up work.

### Known limitations

- Remaining ~5–15% uBO gap: filter-list download variance on emulator,
  cosmetic-only entries, MV2 action functions no-oping (schema-only).
- `release` args variant not yet validated end-to-end.
- Store-install (CWS) UX on the phone form factor not re-verified on M151.

## v1.8.0 - 2026-04-18

### Fixed

- **Grant manifest permissions on extension load.** On desktop-android,
  `chrome/browser/extensions/` (where `PermissionsUpdater::InitializePermissions`
  lives) is not linked because `enable_extensions=false`. Without it, every
  extension loaded rules `active_permissions` empty and the manifest's
  host_permissions never reached the active set, so
  `WebRequestPermissions::CanExtensionAccessURL` returned `kWithheld`/`kDenied`
  for every URL. Extensions could register listeners but the event router
  dropped events before dispatching — uBlock Origin matched 0 ads.
  The fix reaches back into the registry-owned `Extension` after
  `ExtensionRegistrar::AddExtension` and calls
  `permissions_data()->SetPermissions(required, empty_withheld)` using the
  manifest-declared required permissions. This matches what
  `InitializePermissions` does on desktop. Both code paths (`AddExtension`
  for fresh installs, `reload-persisted` for restart recovery) are
  covered. Verified: uBO's `active_hosts=2`, `withheld_hosts=0`;
  `CanExtensionAccessURL` returns `kAllowed`.

### Known limitations

- uBlock Origin ad-blocking is still not fully end-to-end. With the grant
  fix, the URL permission check passes (`access=kAllowed`), but the
  webRequest event router still returns `net::OK` instead of
  `ERR_IO_PENDING` from `OnBeforeRequest` — the blocking dispatch chain
  is broken somewhere between `GetMatchingListeners` and the renderer
  reply. Diagnostic in
  `docs/superpowers/diagnostics/2026-04-18-webrequest-blocking.md`.
  Ready-to-go probe on `feature/v1.8-webrequest-trace`. Will land in
  v1.9.
- DevTools frontend still fetched from appspot (see v1.6 notes).
- Store-install ID rewriting unchanged.

## v1.7.0 - 2026-04-18

### Added

- **Last-mile `chrome.*` stubs so "Unknown Extension API" drops to zero**
  on a fresh launch. All the bootstrap calls that the CWS detail page
  and uBlock Origin / Dark Reader make on startup are now registered.

  `webstorePrivate` (five new shape-only stubs on the v1.4 scaffolding):
  `getFullChromeVersion` (returns the real version string),
  `getMV2DeprecationStatus` → `"inactive"`,
  `getReferrerChain` → `""`,
  `isInIncognitoMode` (delegates to `browser_context()->IsOffTheRecord()`),
  `getExtensionStatus` → `"installable"`.

  `scripting`: `insertCSS`, `removeCSS`, `executeScript`. uBO's
  cosmetic-filter bootstrap short-circuited when `insertCSS` threw, and
  the short-circuit prevented the network-filter engine from compiling
  on the same startup pass.

### Known limitations

- uBlock Origin still reports ~3 % blocked on the parity test page even
  though the Unknown-API noise is gone. The `WebRequestAPI` keyed
  service is registered and the URL-loader proxy installs on
  desktop-android, but something downstream of
  `onBeforeRequest` + `{cancel: true}` isn't being honoured
  end-to-end. Needs deeper tracing — separate v1.8 task.
- DevTools frontend still appspot-hosted. Local bundling deferred.
- Store-installed extensions get local IDs differing from their CWS
  IDs. CRX key still not pinned through `CrxInstallCoordinator`.

## v1.6.0 - 2026-04-18

### Added

- **Broad `chrome.*` defensive stubs.** tabs / windows / action / browserAction
  / contextMenus / cookies / notifications / types.ChromeSetting / extension.
  API-probe pass count: MV3 9 → 30, MV2 9 → 31. "Unknown Extension API"
  logcat spam 187 → 2. Fixes the Bitwarden service-worker wake crash
  (`tabs.query` was returning `undefined` → the BadgeService `.filter()`
  call blew up on every alarm).
- **Native Chrome Web Store install.** The CWS detail page loads normally
  and its own "Add to Chrome" button drives the install via
  `chrome.webstorePrivate.beginInstallWithManifest3`. The old
  `ExtensionInstallNavigationThrottle`-based wrapper dialog is retired.
  Throttle registration is kept so raw redirect-to-`.crx` chains still
  reach the download layer unchanged.
- **`webstorePrivate` schema exposed on desktop-android.** Hoisted
  `webstore_private.json` out of the `enable_extensions`-gated schema set
  into the sibling `enable_desktop_android_extensions && !enable_extensions`
  block next to `developer_private.idl`. Verified in generated
  `generated_schemas.cc`.

### Fixed

- **DevTools frontend 404.** `CHROMIUM_GIT_REVISION` already embeds the
  `@` prefix (`"@03d5…"`); the previous format string `.../serve_rev/@%s/...`
  double-stamped it, producing `@@<sha>` which 404s on appspot. Dropped
  the literal `@` so the URL matches what Chromium's own
  `DevToolsHttpHandler::GetFrontendURLInternal` emits in `/json/list`.

### Known limitations

- DevTools frontend is still fetched from `chrome-devtools-frontend.appspot.com`.
  Local bundling of the `devtools_resources` pak on Android would add
  ~30-40 MB to the APK; tracked as a follow-up. Today's hotfix unblocks
  the menu entry without the size cost.
- uBlock Origin still reports 0/133 ads blocked on the parity test page.
  The API-denied log noise is gone, but the DNR/webRequest engine
  integration needs a separate pass — stubs prevent crashes but don't
  provide real rule application. Tracked for v1.7.
- CRX key/signature isn't pinned through `CrxInstallCoordinator` yet, so
  store-installed extensions get local IDs that differ from their CWS
  IDs (Bitwarden `nngc…` → `fciin…`). Visible in the install confirm
  dialog. Parity-report P2; tracked for v1.7.

## v1.5.0 - 2026-04-18

### Added

- **On-device DevTools.** New main-menu entry "Developer tools" opens
  the current tab's DevTools frontend in a new tab. The remote-debugging
  HTTP server binds to 127.0.0.1:<ephemeral> lazily on first use and is
  pinned to the process lifetime (no teardown path — once started,
  stays up).
- **DevToolsBridge** (`chrome/browser/extensions/android/devtools_bridge.{cc,h}`)
  — lazy loopback server + frontend URL builder. JNI entry point
  `DevToolsBridge.open(Profile, WebContents)` returns the URL that
  Chromium itself emits in `/json/list` (`devtoolsFrontendUrl`) so the
  appspot-hosted frontend loads reliably.

### Fixed

- **DevTools frontend no longer 404s.** The appspot service
  (`chrome-devtools-frontend.appspot.com`) requires a real Chromium git
  revision pin returned by `content::GetChromiumGitRevision()`, not
  `@HEAD` or `@latest`. The bridge now uses the same URL pattern as
  Chromium's own `DevToolsHttpHandler::GetFrontendURLInternal`, pinned
  to the build's `LASTCHANGE` revision.

### Known Limitations

- DevTools frontend is fetched from `chrome-devtools-frontend.appspot.com`
  — network required on first use. Bundled resources aren't linked on
  Android (see `content/browser/devtools/BUILD.gn:13`:
  `if (!is_android && !is_ios)`); serving the frontend locally would
  need `debug_frontend_dir` wired to the build tree or the `front_end`
  bundle packaged into APK assets. Deferred.

## v1.4.0 - 2026-04-18

### Added

- **API coverage matrix** at `docs/superpowers/api-coverage.md` mapping
  every `chrome.*` surface to full / stub / schema-only / unavailable /
  infeasible, with rationale for the infeasible ones.
- **Four API stubs** in new `desktop_android_stub_apis.{cc,h}`:
  `chrome.permissions.getAll`, `chrome.permissions.contains`,
  `chrome.commands.getAll`, `chrome.notifications.create`.
  `permissions.getAll` reconstructs `{permissions, origins}` directly
  from the manifest; `contains` tests against it. The four stubs move
  the MV3 probe from 8 pass → 12 pass, MV2 from 8 → 13.
- **`+ (from store)` button** in the chrome://extensions toolbar
  (always visible — not dev-mode gated). Opens the Chrome Web Store
  extensions category page in a new child tab.
- **`webstorePrivate` shim** (functions + URL helpers) under
  `chrome/browser/extensions/desktop_android/webstore_private/` as
  scaffolding for a Kiwi-style install flow where the CWS page's own
  Install button drives the install via
  `chrome.webstorePrivate.beginInstallWithManifest3`. **Not yet wired
  through to the CWS page context** — the features JSON would need
  `webstorePrivate` exposed before the CWS page can see the binding.
  WIP preserved on `feature/install-webstore-private-wip`.
- Install-flow diagnostic + design spec + execution plan under
  `docs/superpowers/`.
- Popup-lifecycle diagnostic pinning the 'uBO filter button doesn't
  work + flicker' symptoms to missing `chrome.tabs` / `chrome.windows`
  API bindings (not to the popup hosting model).

### Known Limitations

- CWS install still flows through the v1.1 `ExtensionInstallNavigationThrottle`.
  The Kiwi-style cutover is preserved on
  `feature/install-webstore-private-wip` and depends on exposing
  `webstorePrivate` in an Afterbird-owned features JSON.
- `chrome.tabs.query` / `chrome.windows.*` are not yet implemented.
  uBlock Origin's filter engine doesn't boot without them; Dark Reader
  can't inject themes without `chrome.scripting.executeScript`.
  Tracked as v1.5 follow-ups in `api-coverage.md`.

## v1.3.0 - 2026-04-17

### Added

- **API probe extension.** `third_party/extensions/afterbird-api-probe/`
  ships as two unpacked extensions (MV2 and MV3) that exercise the
  important `chrome.*` APIs — `runtime.sendMessage` round-trips, ports,
  storage.local/session/sync, tabs.query/create/remove, alarms,
  action / browserAction badges and icons, contextMenus, DNR updates,
  MV3 `scripting.executeScript`, commands, permissions, i18n,
  notifications, plus `webRequest` on MV2. Each popup shows a per-API
  pass / fail / unavailable table so the compatibility surface is
  visible at a glance.

### Fixed

- **Extensions menu tabs are now proper child tabs.** The main-menu
  "Extensions" entry and every per-extension entry launched with
  `TabLaunchType.FROM_CHROME_UI`, which left the tab parent-less —
  back-swipe from the new tab killed the activity instead of going
  back. Switched to `FROM_LINK` with the active tab as parent. On
  device, the swipe now closes only the extensions tab and returns to
  the previous page.

### Changed

- **Per-extension menu entries carry the extension's icon.**
  `app_menu_bridge.cc` now picks a 24–64 px icon, base64-encodes it,
  and hands it through to the Java side. `ExtensionMenuManager`
  decodes into a `BitmapDrawable` and calls `MenuItem.setIcon`, so the
  entries are visually distinguishable instead of all sharing a
  generic puzzle piece.

### Known Limitations

- DevTools is still not wired up the Kiwi way — deferred to v1.4.
- Extensions without a `browser_action.default_popup` (or MV3
  equivalent with an empty popup) don't appear in the per-extension
  menu list. Intentional for v1.3.

## v1.2.0 - 2026-04-17

### Added

- **Install confirmation dialog.** Every extension install — whether from
  a Chrome Web Store detail URL, a `.crx` link, or a store→CDN redirect —
  now routes through a `CrxInstallCoordinator` that fetches the package,
  unpacks it into a staging dir, and shows a `ModalDialogManager`-driven
  dialog with the extension's name, version, source label, raw manifest
  permission list, and a fine-print risk warning. Accept promotes; Cancel
  deletes the staging + the cached CRX.
- **Install triggered on download, not navigation.** The navigation throttle
  is narrowed to only the Chrome Web Store detail-page case.
  `.crx` / `application/x-chrome-extension` responses are now caught at
  `DownloadManagerDelegate::InterceptDownloadIfApplicable`, matching the
  desktop Chrome primitive — the browser recognises an extension download
  and hands it to the coordinator instead of writing to the Downloads
  folder.
- `DesktopAndroidExtensionInstaller` split into `PrepareFromFile` (unpack +
  read manifest + stage) and `CommitPrepared` (promote + register), so the
  confirm dialog can display manifest data before the install commits.

### Fixed

- **chrome://extensions Enable toggle now actually disables the extension.**
  `chrome.metricsPrivate` isn't bound on desktop-android; every
  `service.ts` method that starts with `recordUserAction(...)` used to
  throw before reaching the real API call. The toggle, Remove, and
  Update buttons all exited early. Guard `recordUserAction` with a
  single try/catch and route every call site through it.
- **chrome://extensions list refreshes after state changes.** Previously
  flipping the toggle or removing an extension left the UI showing stale
  data. `developerPrivate.onItemStateChanged` is now broadcast from
  `updateExtensionConfiguration`, `removeMultipleExtensions`, `reload`,
  and `loadUnpacked` handlers, with the correct `extensionInfo`
  (camelCase) payload the Polymer manager expects.
- **Card icons render.** `chrome://extension-icon/` isn't served on
  desktop-android builds. `developerPrivate.getExtensionsInfo` now
  inlines the icon bytes as a `data:image/<type>;base64,...` URL.
- **Details panel renders.** The Lit template dereferences several fields
  (`errorCollection.isEnabled`, `manifestHomePageUrl.length`,
  `blocklistText`) unconditionally; emit them in the payload so the
  render doesn't throw.

## v1.1.0 - 2026-04-17

### Added

- **Install extensions from the Chrome Web Store.** Navigating to a
  `chromewebstore.google.com/detail/<slug>/<id>` URL (or the legacy
  `chrome.google.com/webstore/detail/...` form) now auto-installs the
  extension. The `ExtensionInstallNavigationThrottle` extracts the
  extension ID, fetches the signed `.crx` from Google's update
  endpoint, and pipes it through `DesktopAndroidExtensionInstaller`.
- **Install from any `.crx` / `.user.js` link on the web.** Same
  throttle catches direct navigations to extension payloads. The
  download is cancelled on the navigation side and handled by a
  self-owned `CrxDownloadInstaller` helper so it outlives the throttle.
- **chrome://extensions list cards render.** `developerPrivate` now
  emits `runtimeWarnings`; the upstream Polymer item used to throw on
  `.length` of an undefined field and every row collapsed to `height=0`.

### Changed

- The `--load-extension` command-line hack is no longer required for
  any end-user flow. Both the built-in Load Unpacked picker and the
  new URL throttles cover first-time install, and persisted extensions
  come back on restart.

### Known Limitations

- Extension icons on `chrome://extensions` render as broken images —
  `chrome://extension-icon/` is not yet wired up on desktop-android.
- Re-installing the same extension produces a new staging dir + new
  unpacked-location ID, so the list accumulates duplicates. Desktop
  Chrome dedups by public key; Afterbird doesn't yet.
- The install flow is silent — no permission confirmation sheet.
- Enable toggle / Remove / Details / Pack / dev-mode toggle on
  `chrome://extensions` still need polish; some actions don't wire
  through to the real extension system yet.

## v1.0.0 - 2026-04-17

### Added

- **Full extension messaging.** Enabled `extensions::MessageService` on
  `enable_desktop_android_extensions` builds — `chrome.runtime.sendMessage`
  and port-based `chrome.runtime.connect` now actually route between
  extension frames. Before this, every message was silently dropped by
  `ExtensionFrameHost::OpenChannelToExtension`, which kept dashboards and
  popups (uBlock Origin, etc.) blank even though their JS ran.
  - Relaxed the assert in `extensions/browser/api/messaging/BUILD.gn` to
    accept `enable_desktop_android_extensions`.
  - Shimmed `WebViewGuest::FromRenderFrameHost` in `message_service.cc`
    since there are no `<webview>` frames on Android.
  - `DesktopAndroidExtensionsBrowserClient` now installs a subclassed
    `ExtensionsAPIClient` that returns a default `MessagingDelegate`
    (the base class returns null and `MessageService` dereferences it
    on first message → crash fixed).
  - `DesktopAndroidExtensionSystem::InitForRegularProfile` forces the
    `MessageService` factory to materialise so `MessageServiceApi` is
    bound before any extension frame opens a channel.

### Known Limitations

- End-to-end extension workflows beyond uBlock Origin are **not yet
  tested** in this release. Expect rough edges with extensions that
  rely on `chrome.management`, background service workers, or APIs
  still stubbed in the desktop-android extension system.
- Chrome Web Store install flow and install-from-page (`.crx` link
  clicks) are still deferred to a follow-up.

## v0.6.0 - 2026-04-17

### Added

- **Runtime extension management on `chrome://extensions`**: enable /
  disable / remove / reload buttons now drive the real extension system.
  Wired through `developerPrivate.updateExtensionConfiguration`,
  `removeMultipleExtensions`, and `reload` against
  `DesktopAndroidExtensionSystem` / `ExtensionRegistrar`.
- **`DesktopAndroidExtensionInstaller::InstallFromFile`**: unpacks
  `.zip`, `.crx` (strips the CRX3 header), or a plain directory into
  `<profile>/Extensions/afterbird_install_<rand>/`, then registers the
  unpacked tree via `AddExtension`. Also backs the
  `--install-extension=/path` command-line flag and a fallback
  install-list file for non-debug builds.
- **Load Unpacked file picker**: new Java `ExtensionInstallBridge` +
  JNI plumbing pops the Android `ACTION_OPEN_DOCUMENT` picker, streams
  the `content://` URI into the app cache, and hands the staged path to
  C++ `ExtensionInstallCallback::OnFilePicked`. Wired to
  `developerPrivate.loadUnpacked({})` so the Load Unpacked button on
  `chrome://extensions` works on Android.
- **Persistence across restart**:
  `DesktopAndroidExtensionSystem::InitForRegularProfile` now walks
  `ExtensionPrefs::GetInstalledExtensionsInfo()` and re-registers every
  previously-installed extension on startup. Prefs entries whose staging
  directory was deleted out-of-band are pruned.

### Changed

- `DesktopAndroidExtensionSystem` returns a `NullAppSorting` so uninstall
  flows no longer SIGSEGV when the extensions WebUI asks for ordering.
- Uninstall loop now snapshots extension ids before iterating and uses
  the real install location instead of a synthetic placeholder.

### Known Limitations

- Chrome Web Store install flow is not implemented yet (planned for
  v0.7). Installs go through the picker or the `--install-extension`
  flag.
- JavaScript execution inside `chrome-extension://*` pages is still
  partial: uBO's background page loads but its popup/dashboard render
  blank. Content scripts and DNR rules run.
- `chrome.management` API is not wired up.
- Extension messaging (`chrome.runtime.sendMessage`) remains stubbed
  (unchanged from prior releases).

## v0.2.0 - 2026-04-16

### Added

- **Kiwi-style Extensions menu**: main menu now has a static "Extensions"
  entry (puzzle-piece icon) that opens `chrome://extensions`.
- **Per-extension menu items**: each running extension gets its own entry
  at the bottom of the main menu (e.g. "uBlock Origin"). Tapping it opens
  the extension's browser_action `default_popup` URL in a new tab.
- New `//chrome/browser/extensions/android` build module containing:
  - `AppMenuBridge` JNI class that enumerates
    `ExtensionRegistry::enabled_extensions()` for the menu
  - `ExtensionMenuManager` Java helper that maps menu item ids to popup
    URLs and parses the `name/id/popupUrl/icon/active` tuple from native
- Multi-extension loading via comma-separated `--load-extension` paths.

### Changed

- `AppMenuPropertiesDelegateImpl.prepareMenu()` now appends extension
  entries after standard items when on `PAGE_MENU` with a valid tab.
- `ChromeActivity.onMenuOrKeyboardAction()` handles `R.id.extensions_id`
  and the dynamic extension id range (`ExtensionMenuManager.MENU_ITEM_ID_BASE`).

### Known Limitations (unchanged from v0.1)

- Extension popup pages open but their JavaScript doesn't execute
  (extensions/renderer pipeline needs work for desktop-android).
- External `<script src>` in extension pages causes renderer crash.
- MessageService still stubbed; `chrome.runtime.sendMessage` drops silently.
- `chrome://extensions` WebUI page still blank (desktop string resources).

## v0.1.0 - 2026-04-16

### Added

- First working Afterbird build for Android with extension loading support.
- `--load-extension=/path` command-line flag now loads unpacked extensions at
  startup (backport to `DesktopAndroidExtensionSystem::InitForRegularProfile`).
- `components/guest_view/renderer/BUILD.gn` overlay allows GuestViews to build
  on Android (removed `assert(!is_android)` as the original comment suggested).
- Custom `extensions/browser/extension_frame_host.cc` and
  `extensions/browser/service_worker/service_worker_host.cc` with null-safe
  MessageService access to prevent SIGSEGV on desktop-android builds.
- `chrome/browser/profiles/BUILD.gn`, `chrome/browser/BUILD.gn`,
  `chrome/browser/extensions/BUILD.gn` overlay entries that gate desktop-only
  dependencies (`//apps`, `platform_apps`, `web_applications`, `//components/drive`,
  `//media/cast`, mirroring) behind `!is_android`.
- `third_party/blink/renderer/build/scripts/gperf.py` fix for duplicate
  fallthrough annotations on modern gperf builds.
- `chrome/browser/resources/extensions/managed_footnote_stub.d.ts` plus gen-dir
  JS stub to satisfy rollup bundling on Android (where managed_footnote is
  desktop-only).

### Changed

- `.build/production_build_reference/args.gn`:
  - `enable_extensions = false` (use `enable_desktop_android_extensions = true`
    instead, which provides the core extension infrastructure without
    triggering the ~100 `assert(!is_android)` failures the full flag would).
  - `enable_guest_view = false`
  - `enable_platform_apps = false`
  - `chrome_pgo_phase = 0`
  - `use_login_database_as_backend = true`
- `chrome/browser/chrome_browser_main.cc`: removed `InspectUIConfig`
  registration on Android (implementation is gated off Android).

### Known Limitations

- Extension messaging API is stubbed; extensions that depend on
  `chrome.runtime.sendMessage` between background and content scripts
  have limited functionality.
- Extension popups are not supported (requires full `enable_guest_view`).
- `chrome://extensions` WebUI page is disabled (extensions WebUI depends on
  desktop-only string resources like `IDS_CONTROLLED_SETTING_*`, `IDS_SEARCH_*`).
- No UI for runtime extension management yet.

## 2026-04-13

### Added

- `ARCHITECTURE.md` with source-tree/branch architecture and a practical revival roadmap.
- `AGENTS.md` with contributor/agent workflow rules for branching, QA cross-review, merge policy, and commit hygiene.
- `CHANGELOG.md` baseline.
- `ci/chromium_android_pipeline.sh` to run a pinned Chromium external checkout pipeline:
  - Reads `CHROMIUM_VERSION`
  - Checks out exact Chromium tag into external workdir
  - Runs `gclient sync`
  - Applies Afterbird overlay
  - Runs `gn gen` using `.build/production_build_reference/args.gn`
  - Runs smoke graph checks by default, with optional explicit full build mode (`--full-build`)
- `.github/workflows/chromium_smoke_pipeline.yml` for smoke checks on `push`/`pull_request` to `afterbird`.
- `.github/workflows/chromium_full_build.yml` for manual full build runs (`workflow_dispatch`).
- `ci/android_emulator_test.sh` for emulator APK automation:
  - install APK on emulator/device
  - startup smoke launch check
  - internal pages launchability checks for extension/devtools entry points
  - modern-site e2e flow (~120 seconds default)
  - logcat crash signal scan + `dumpsys meminfo` trend report
- `tests/emulator/modern_sites.txt` and `tests/emulator/internal_pages_smoke.txt` to define automated URL coverage.
- `tests/emulator/manual_checks.md` to explicitly separate manual-only extension/devtools checks from automation scope.
- `.github/workflows/android_emulator_e2e.yml` for manual-dispatch emulator automation (plus optional weekly schedule path).
- `ci/extensions/ublock_chromium_132.lock.json` pinning uBlock Chromium package `1.62.0` (`2025-01-01`) with URL/size/SHA256 metadata.
- `ci/fetch_ublock_chromium.sh` to fetch and verify the pinned uBlock package from the lock manifest.

### Changed

- Rebuilt Android icon assets from `afterbird-browser-icon.png` with adaptive-layer semantics across `res_chromium_base` and `res_chromium_contributor`: opaque `ic_background`/`layered_app_icon_background`, transparent `ic_foreground`/`layered_app_icon`, and updated flattened `app_icon` renders, while preserving existing per-density dimensions and PNG formats.
- Rewrote `README.md` to reflect the current Afterbird state, including project rename/purpose, branch roles (`afterbird`, `kiwi`, `chromium`), in-tree version markers with caveats, and realistic build requirements.
- Clarified branch wording to avoid time-sensitive claims about `afterbird`/`kiwi` alignment after future merges.
- Documented how to create a local `chromium` branch when only `origin/chromium` exists: `git switch -c chromium --track origin/chromium`.
- Merged latest `origin/chromium` into `afterbird` on feature branch, resolving large source conflicts by preserving Chromium updates while keeping Afterbird governance/docs files (`README.md`, `ARCHITECTURE.md`, `AGENTS.md`, `CHANGELOG.md`) under project control.
- Updated docs to reflect the Chromium 132 baseline now present on `afterbird` (`CHROMIUM_VERSION=132.0.6834.83`).
- Updated governance docs to reflect current reality: Chromium sync is performed by merging/rebasing `origin/chromium` into a feature branch from `afterbird`, then PR back to `afterbird` (with `kiwi` treated as legacy/reference, not mandatory intermediary).
- Added explicit risk notes that direct Chromium merges may drop Kiwi-specific integrations and that re-porting these integrations is tracked as revival backlog work.
- Updated `README.md` with local prerequisites and exact smoke/full commands for the new external Chromium pipeline.
- Updated `ARCHITECTURE.md` automation/build sections to describe the new smoke/full CI and externalized build model.
- Updated docs to include exact local emulator-test commands/prerequisites and lockfile-based extension package prep workflow.
- Hardened `ci/chromium_android_pipeline.sh` idempotency by resetting/cleaning the external Chromium `src` checkout before overlay application, preventing stale state from previous runs.
- Added strict `--out-dir` validation in `ci/chromium_android_pipeline.sh` to allow only safe relative paths under `src` (rejects absolute and traversal segments).
- Narrowed `chromium_smoke_pipeline.yml` triggers using `paths` filters so docs-only updates do not run the expensive smoke job.
- Optimized Chromium startup fetch in `ci/chromium_android_pipeline.sh` by fetching only the pinned tag ref (`refs/tags/<version>`) instead of all remote tags.
- Added retry/backoff + timeout controls for remote tag fetch and `gclient sync`, with clearer logs and mirror guidance for blocked/slow networks.
- Added environment override support for Chromium source URL (`AFTERBIRD_CHROMIUM_SRC_GIT_URL`) and lightweight sync controls (`AFTERBIRD_GCLIENT_NO_HISTORY`, retry/timeout knobs).
- Fixed retry handling in `run_with_retries` so it preserves the actual command exit code (including timeout exit code `124`) across retry decisions.
- Restored workspace-config backward compatibility: existing `.gclient` and `src` origin are now preserved by default and changed only when missing or when `AFTERBIRD_FORCE_WORKSPACE_CONFIG=1`.
- Added explicit warning behavior when no `timeout`/`gtimeout` binary is available and timeout enforcement cannot be applied.
- Replaced top-level `LICENSE` with an explicit combined licensing notice covering both Afterbird/Kiwi fork-origin files and Chromium-origin files, with third-party license caveats.
- Fixed top-level `OWNERS` references to missing paths by replacing unresolved `file://...` targets (`build/OWNERS`, `styleguide/c++/OWNERS`, `styleguide/rust/OWNERS`) with existing in-repo ownership references.
- Noted that the Chromium 132 merge may have dropped historical Kiwi integrations; those are tracked for explicit re-port follow-up.
- Fixed Chromium 132 `gn gen` failure in the Android production args by explicitly enabling `enable_guest_view = true` when `enable_extensions = true` (required by `//extensions/BUILD.gn` assert).
- Disabled Chrome PGO phase in Android production GN args (`chrome_pgo_phase = 0`) so `gn gen` does not fail when pinned PGO profile artifacts are unavailable in external CI/local workdirs.
- Switched Android extension args from `enable_extensions = true` to `enable_desktop_android_extensions = true` (`enable_extensions = false`) to match Chromium 132 Android constraints and avoid `//apps` assertion paths pulled by `//extensions/shell`.
- Set `enable_guest_view = false` for Android production args in the desktop-android extensions configuration to avoid pulling `web_view/web_ui` targets that require full `enable_extensions`.
- Updated Android signing args to Chromium debug keystore defaults (`android_keystore_path = "//build/android/chromium-debug.keystore"`, `android_keystore_name = "chromiumdebugkey"`, `android_keystore_password = "chromium"`) so full APK packaging can proceed in external workdirs without a repo-local `keystore.jks`.
- Disabled `cc_wrapper` in Android production args (`cc_wrapper = ""`) for server builds where `ccache` is not installed, preventing toolchain invocation failures during `autoninja`.
- Added missing `res_chromium_base` adaptive icon PNG entries (`ic_background`/`ic_foreground` for all densities) to `chrome/android/BUILD.gn` resource lists so Android resource packaging does not fail in `chrome_base_module_resources`.
- Added missing `components/browser_ui/styles/android` density PNG entries (`ic_chrome`, `ic_pause_white_24dp`, `ic_play_arrow_white_24dp`, `ic_stop_white_36dp`) to `java_resources` sources in `components/browser_ui/styles/android/BUILD.gn` to fix `prepare_resources.py` strict source-list validation during full APK builds.
- Added missing `chrome/browser/ui/android/omnibox` density PNG entries (`btn_mic`, `btn_star`) to `java_resources` sources in `chrome/browser/ui/android/omnibox/BUILD.gn` to fix `prepare_resources.py` source-list validation in full builds.
- Added missing `components/browser_ui/widget/android` density PNG entries (`ic_drag_handle_grey600_24dp`) to `java_resources` sources in `components/browser_ui/widget/android/BUILD.gn` to fix `prepare_resources.py` source-list validation in full builds.
- Added missing `chrome/browser/ui/android/toolbar` resource entries (`btn_toolbar_hand`, `incognito_switch`, `start_top_toolbar.xml`, `tab_switcher_toolbar.xml`) to `java_resources` sources in `chrome/browser/ui/android/toolbar/BUILD.gn` to fix `prepare_resources.py` source-list validation in full builds.
- Enabled `allow_missing_resources = true` for `chrome_app_java_resources` in `chrome/android/BUILD.gn` to tolerate additional overlay resource files not explicitly listed in `chrome_java_resources` during external full builds.
- Added missing `getLastNonExtensionActiveIndex()` overrides in `EmptyTabModel`, `IncognitoTabModelImpl`, and `TabGroupModelFilterImpl` so `chrome/browser/tabmodel:java` compiles against the current `TabList` interface.
- Fixed `WebContentsDarkModeController` Java compile break by removing a duplicated `getEnabledState(...)` implementation and replacing unavailable `SharedPreferencesManager` usage with `ContextUtils.getAppSharedPreferences()` writes for `night_mode_settings`.
- Added a default `TabList#getLastNonExtensionActiveIndex()` implementation (`return index();`) to keep Android tabmodel implementations source-compatible while extension-tab specific overrides are absent in some Chromium paths.
- Fixed Android full-build compile stop in `extensions/common/command.cc` by mapping `BUILDFLAG(IS_ANDROID)` to Linux-style keybinding platform instead of hitting the unsupported-platform preprocessor error path.
- Removed legacy density PNG overlays for `ic_incognito` (`drawable-{h,md, x,xx,xxx}dpi`) so Android resource linking no longer conflicts with the upstream vector `drawable/ic_incognito.xml`.
- Removed legacy `chrome/android` copy of `custom_tabs_toast_branding_layout.xml` to avoid duplicate resource collisions with Chromium 132 `//chrome/browser/android/customtabs/branding:java_resources`.
- Reverted `chrome_app_java_resources` back to strict resource listing (`allow_missing_resources = false`) to stop broad duplicate-resource collisions between legacy overlay files and modular Android resource deps.
- Removed legacy `chrome/android` Autofill editor resources that are now provided by `//chrome/browser/autofill/android:java_resources`, preventing duplicate layout/menu collisions during `chrome_public_apk` resource linking.
- Restored `allow_missing_resources = true` after strict mode exposed a very large legacy overlay drift set; continued with targeted duplicate-resource removals instead.
- Removed legacy `passwords_error_dialog.xml` and `passwords_progress_dialog.xml` from `chrome/android` to avoid duplicate resource definitions with `//chrome/browser/password_manager/android:java_resources`.
- Removed legacy `sync_promo_view.xml` from `chrome/android` to avoid duplicate resource definitions with `//chrome/browser/ui/android/signin:java_resources`.
- Removed legacy `sheet_tab_toolbar.xml` from `chrome/android` to avoid duplicate resource definitions with `//chrome/browser/ui/android/toolbar:java_resources`.
- Removed legacy `accessibility_preferences.xml` from `chrome/android` to avoid duplicate resource definitions with `//components/browser_ui/accessibility/android:java_resources`.
- Dropped conflicting PNG entries (`ic_chrome`, `ic_pause_white_24dp`, `ic_play_arrow_white_24dp`) from `components/browser_ui/styles/android/BUILD.gn` so vector drawables remain the single source of truth during APK resource linking.
- Removed legacy PNG assets (`ic_chrome`, `ic_pause_white_24dp`, `ic_play_arrow_white_24dp` across all densities) from `components/browser_ui/styles/android` to align with Chromium 132 vector resources and satisfy strict resource-source checks.
- Removed additional legacy `chrome/android` layouts (`accessibility_tab_switcher*`, `bookmark_*`, `empty_background_view_tablet`, `experimental_explore_sites_section`) that referenced non-existent resources and blocked `chrome_public_apk` aapt2 linking.
- Removed legacy explore-sites layout overlays from chrome/android/java/res/layout (not present in pinned Chromium tree) to resolve missing-resource aapt2 link failures in full APK builds.
- Removed overlay-only layouts fre_tosanduma/fullscreen_notification/history_toggle from chrome/android/java/res/layout to fix missing-resource aapt2 link failures during chrome_public_apk packaging.
- Removed overlay-only infobar/password-generation layouts and added default base strings for legacy radio-button preferences to fix aapt2 resource linking.
- Removed additional overlay-only layouts (revamped incognito, shopping filter, top sites, and start-surface toolbar variants) and dropped their explicit toolbar resource-list entries to fix aapt2 missing-resource failures.
- Removed overlay-only grid/tab-switcher, top-sites-condensed, bookmark-action-bar, autofill-assistant-preferences, and lite-mode preferences resources from `chrome/android` to resolve missing-resource aapt2 link failures in `chrome_public_apk`.
- Registered InspectUIConfig and extensions::ExtensionsUIConfig at startup for Android desktop-extensions mode so chrome://inspect/#devices and chrome://extensions no longer fall through to ERR_INVALID_URL in runtime checks.
- Added android-side compilation of webui/inspect_ui and webui/extensions/extensions_ui when enable_desktop_android_extensions is enabled, fixing libchrome__combined.so linker errors for InspectUI and ExtensionsUIConfig.
- Enabled component_extension_resources generation and dependency wiring for Android desktop-extensions mode, so chrome/grit/extensions_resources*.h is available during extensions WebUI compilation.
- Enabled chrome/browser/resources/extensions WebUI resource generation for desktop-android extensions mode (without full enable_extensions), wiring extensions:resources into chrome/browser/resources so chrome/grit/extensions_resources*.h is produced.
- Made chrome/browser/resources/extensions TypeScript deps Android-safe by excluding managed_footnote:build_ts under is_android, resolving gn assert failures in desktop-extensions mode.
- Enabled desktop-android extensions mode to include Polymer-era WebUI TypeScript surfaces in //ui/webui/resources/js and //ui/webui/resources/cr_elements, so //chrome/browser/resources/extensions:build_ts can resolve chrome://resources module imports on Android.
- Kept //third_party/polymer/v3_0 TypeScript dependency disabled on Android in //ui/webui/resources/cr_elements (polymer target asserts on Android), while still enabling desktop-extension WebUI element sources.
- Added an Android-only TypeScript compatibility shim for polymer_bundled imports in //ui/webui/resources/cr_elements desktop-extensions mode, avoiding //third_party/polymer Android assert while keeping required cr_elements wrappers buildable.
- Expanded the Android desktop-extensions polymer TS shim with wildcard module matching and permissive typings for polymer_bundled imports used by cr_elements generated wrappers.
- Added explicit TS path mappings for polymer_bundled.min.js in //ui/webui/resources/cr_elements desktop-android mode, so generated Polymer-style wrappers resolve without enabling //third_party/polymer Android targets.
- Narrowed Android desktop-extensions WebUI inclusion to a targeted cr_elements/js subset (plus focus_without_ink), avoiding full Polymer surface compilation while keeping required extension-facing modules.
