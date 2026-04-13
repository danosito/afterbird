# Changelog

All notable changes to this repository are documented in this file.

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
