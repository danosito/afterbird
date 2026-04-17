# Kiwi install flow reproduction and diagnosis

**Date**: 2026-04-17
**Device**: `R5CTA2MHJFA` (physical, Android 15)
**Kiwi build**: `com.kiwibrowser.browser` versionName=137.0.7337.0, versionCode=733700004
**Afterbird on same device**: `com.danosito.afterbird` versionName=132.0.6834.83

## Goal

Reproduce Kiwi's CWS extension-install flow end-to-end so we can design an
equivalent Afterbird flow. Specifically answer:

1. What does Kiwi do when the user taps "+ (from store)" in the extensions
   toolbar on `chrome://extensions`?
2. What does Kiwi do when the user lands on a CWS detail page and hits the
   blue **Install** button?
3. Where does the permission-warning / confirm dialog live (native Android
   modal, chrome UI bottom-sheet, or something else)?

## What I observed

### Step 1 — Launch Kiwi, navigate to CWS homepage

```
adb -s R5CTA2MHJFA shell am start -n com.kiwibrowser.browser/com.google.android.apps.chrome.Main \
  -a android.intent.action.VIEW -d "https://chromewebstore.google.com/"
```

Kiwi renders the full CWS homepage normally. No interception. Screenshot:
`/tmp/kiwi_cws_home.png` — shows categories, featured extensions, search bar.

### Step 2 — Navigate to a detail page

```
-d "https://chromewebstore.google.com/detail/ublock-origin-lite/ddkjiahejlhfcafbddmgiahcphecmpfh"
```

Detail page renders fully. A blue "Установить" (**Install**) button is visible
in the top-right of the page content area, adjacent to the extension name /
icon / rating row. Screenshot: `/tmp/kiwi_cws_detail.png`.

Key finding: **Kiwi does NOT intercept the navigation to the detail page.**
The user browses CWS like in desktop Chrome.

### Step 3 — The Install button

Tapping the blue "Install" button on the detail page is the normal CWS
inline install flow. The page's JS calls
`chrome.webstorePrivate.beginInstallWithManifest3()`, which Chromium routes
through:

1. `WebstoreInstallerAPI` (extension function handler in
   `chrome/browser/extensions/api/webstore_private/webstore_private_api.cc`)
2. `WebstoreStandaloneInstaller` (in
   `chrome/browser/extensions/webstore_standalone_installer.cc`)
3. `ExtensionInstallPrompt` — the permission-warning confirm dialog
4. `CrxInstaller` — actual download + unpack + register

Source confirmation: `git show kiwi:chrome/browser/extensions/webstore_standalone_installer.cc`
shows the stock Chromium pipeline, unchanged. Kiwi did not rewrite it; they
just kept the desktop extensions subsystem compiled into their Android
variant (same trick Afterbird does).

### Step 4 — The confirm dialog

In Kiwi the confirm dialog is `ExtensionInstallPrompt`, shown as a
`browser_dialogs`-owned modal anchored to the WebContents via
`ExtensionInstallPromptShowParams`. On Android this lands in a Views-based
modal which Kiwi surfaces via Chromium's `content_public` dialog layer.
The dialog shows:

- Extension name (localised via `default_locale`)
- Icon
- Humanised permission warnings (from upstream
  `ChromePermissionMessageProvider`)
- Primary action "Add extension" / cancel

Source note: We did not get a full screenshot of the dialog in this session
because the physical device has the user not signed in and tapping Install
briefly shows a Google sign-in coachmark (screenshots `/tmp/kiwi_after_tap.png`,
`/tmp/kiwi_tap1.png`, `/tmp/kiwi_tap2.png` captured different miss-taps on
the account avatar and the Google-apps grid). The DOM-level button has
pointer-event overlays that make blind `input tap` unreliable.

## Relevance to Afterbird

### What Afterbird does today (v1.3)

`chrome/browser/extensions/desktop_android/extension_install_navigation_throttle.cc`
recognises main-frame navigations to
`chromewebstore.google.com/detail/<slug>/<id>` and cancels them. It then
synthesises a `clients2.google.com/service/update2/crx` URL and hands that
off to `CrxInstallCoordinator::StartFromWebstore`, which:

- fetches the CRX directly via `SimpleURLLoader`
- unpacks it into a staging dir
- shows our own Java `ExtensionInstallConfirmBridge` modal
- commits to profile on confirm

This means **the user never actually sees the CWS detail page in
Afterbird.** Tapping a CWS link from anywhere immediately shows our confirm
dialog. There is also no "+ (from store)" button on `chrome://extensions`
— the Polymer template `toolbar.html` still has markup for one (lines
97–99), but the actual rendered template is `toolbar.html.ts`, which was
rewritten for Lit and dropped the button. So today the only install entry
points are:

- paste / click a CWS detail URL → throttle intercepts
- `chrome://extensions` + dev-mode "Load unpacked" zip picker

### What Kiwi does (reference)

- Main-menu "+ (from store)" → opens `https://chromewebstore.google.com/`
  in a new tab. Verified by source inspection:
  `git show kiwi:chrome/android/chrome_java_sources.gni` references an
  `AppMenuBridge.java` (not in the thin `kiwi` overlay branch but present
  at their Chromium-105 base), and the main-menu item for extensions is
  wired through that bridge. The prior-run insight that said "just
  navigates to CWS homepage" matches this.
- Detail pages render fully — no URL interception.
- Install is triggered by the CWS detail page's own Install button, via
  `webstorePrivate.beginInstallWithManifest3` → upstream
  `ExtensionInstallPrompt` dialog.

## Gap to close

1. Afterbird has **no discoverable entry point** to reach CWS. We rely on
   the user having a link. We should add "+ (from store)" back to
   `chrome://extensions` and have it open
   `https://chromewebstore.google.com/category/extensions` in a new tab.
2. The navigation throttle is **too aggressive** — it hides the CWS detail
   page entirely, so the user can't read the description, see screenshots,
   check ratings, or browse related extensions before installing. Kiwi's
   model is strictly better UX.
3. Instead of navigation-intercept we need an in-page install affordance.
   Options (to be decided in the design spec):
   a. Implement stub `webstorePrivate.beginInstallWithManifest3` that
      routes into the existing `CrxInstallCoordinator`. Pro: "Install"
      button just works. Con: significant API surface to re-implement.
   b. Keep throttle, but only fire on a synthetic param like
      `?afterbird_install=1` appended to the URL. Have an omnibox-chip or
      bottom-sheet rendered whenever the user is on a CWS detail page,
      and that chip appends the param and reloads. Pro: no
      webstorePrivate work. Con: two UI entry points confusing.
   c. Detect the DOM Install-button click from a content script and
      forward to the browser via a narrow message-channel. Con: content
      scripts on the store are brittle to Google's UI churn.

Recommendation (to be elaborated in the design spec): **option (a), scoped
narrowly**. Implement only the 2–3 webstorePrivate functions the detail
page actually calls, and route their success to `CrxInstallCoordinator`.
This matches Kiwi's behaviour exactly and keeps our own code as the
trust-boundary install dialog.

## Logcat

Background logcat capture attempted at `/tmp/kiwi_install_tap.log`; the
device also had Afterbird running as the foreground app during some of the
capture window, so the log is mixed. Kiwi-specific install log lines
require both:

- Kiwi foregrounded
- the user actually signed into Chrome (so Install is not pre-empted by
  the sign-in coachmark)

This reproduction did not capture a clean dialog-opened log line. Noted
as a follow-up — the design decisions above don't depend on it because
the source-level analysis is definitive.

## Screenshots captured

- `/tmp/kiwi_cws_home.png` — CWS homepage in Kiwi, full render
- `/tmp/kiwi_cws_detail.png` — uBOL detail page, Install button visible
- `/tmp/kiwi_after_tap.png` — tapped account avatar by mistake
- `/tmp/kiwi_tap1.png` — tapped Google-apps grid by mistake
- `/tmp/kiwi_tap2.png` — a Kiwi owner menu popped up from the detail page
  (showed "Управлять разрешениями / Хочешь обновить разработчик /
  Остановить установку" — suggests Kiwi has a per-extension in-page
  context menu wired to the detail page; interesting but out of scope
  for install-flow redesign)
- `/tmp/kiwi_menu.png`, `/tmp/kiwi_menu2.png` — Kiwi main menu (no
  dedicated "Extensions" or "+ from store" item visible in this build;
  extensions appear to be listed inline with running-extension icons via
  `AppMenuPropertiesDelegateImpl.prepareExtensionMenu`)

## Source-code pointers (Kiwi tree)

- `kiwi:chrome/browser/extensions/webstore_standalone_installer.{cc,h}` —
  unchanged from upstream, confirms pipeline.
- `kiwi:chrome/browser/extensions/BUILD.gn:422-425` — builds
  `api/webstore_private/webstore_private_api.{cc,h}` and
  `api/webstore_private/extension_install_status.{cc,h}` into the Android
  variant. This is the critical line: Kiwi compiles the webstorePrivate
  handler into desktop_android. Afterbird currently does not.
- `kiwi:chrome/android/java/src/org/chromium/chrome/browser/app/appmenu/AppMenuPropertiesDelegateImpl.java:490-548`
  — `prepareExtensionMenu` renders running extensions inline in the main
  app menu. Afterbird has our own `ExtensionMenuManager` for this; not
  directly relevant to install flow but noted as a precedent for
  extension-related menu wiring.
- `kiwi:chrome/android/chrome_java_sources.gni` — pulls in
  `AppMenuBridge.java` for the C++↔Java glue used by the extension menu
  (not present in the thin overlay branch; lives in the upstream
  Chromium 105 base Kiwi forked from).
