# Installing extensions from the Chrome Web Store

Date: 2026-08-20. Build: Chromium 151.0.7922.38 + `patches/m151/0001-0005`.
Device: emulator API 35, clean profile.

## What was broken

The store's detail page showed only **"Available on desktop"** — no install
button — so nothing could be installed from it.

## What was actually missing: one header

Everything else was already there. The store gates its install button on the
user agent; with Chrome for Android's mobile UA it never calls
`webstorePrivate.beginInstallWithManifest3` at all. Overriding the UA by hand
(`Emulation.setUserAgentOverride`) made the page show "Add to Chrome", and the
entire rest of the flow ran on stock upstream code:

- `chrome.webstorePrivate` is fully present on desktop-android — all 20 methods,
  including `beginInstallWithManifest3`, `completeInstall` and `install`
- the native install prompt appears, listing the extension's permissions
- accepting it downloads the CRX, unpacks and installs it
- the extension lands in the profile with `location: INTERNAL`, no disable
  reasons, and survives a restart

So no install dialog, no CRX handling and no download interception had to be
written — unlike the 132-era layer, where all of that was custom code.

## The fix: `patches/m151/0005-webstore-desktop-user-agent.patch`

Force the desktop UA on store URLs (`chromewebstore.google.com`, and
`chrome.google.com/webstore` for old links). Two decision points need it:

- `DesktopSiteUtils.shouldOverrideDesktopSite` — the Java side, consulted for
  browser-initiated navigations
- `RequestDesktopSiteWebContentsObserverAndroid::DidStartNavigation` — the C++
  side, for renderer-initiated ones. Its guard normally defers browser-initiated
  navigations to Java, which cannot see this forced exception, so the guard is
  relaxed for store URLs.

Patching only one of them leaves the UA flipping back to mobile mid-flow: during
development, navigating *within* the store worked while entering it from the
address bar did not.

The override is forced rather than written as a content-setting exception for
the domain. The store's mobile layout offers no install path at all, so there is
nothing for a user to prefer, and seeding a setting would silently overwrite one
they had set themselves.

## Verified end to end

Clean profile each time, no bypass flags:

| Entry path | UA | Install button |
|---|---|---|
| external link, cold start | desktop | yes |
| address bar | desktop | yes |
| navigation inside the store | desktop | yes |

Installing Dark Reader from the store: permission prompt → install → present in
the profile as `eimadpbcbfnmbkopoojfekhnkhdbieeh`, `location: INTERNAL`, no
disable reasons, survives a restart, and visibly darkens pages afterwards.

Note when testing: a profile that already visited the store before the patch can
keep serving the mobile layout. Run `pm clear` before drawing conclusions — an
earlier round of this investigation chased a phantom "browser-initiated
navigation is broken" bug that a clean profile did not reproduce.

## Cosmetics, not addressed

The store still renders a "Switch to Chrome?" promo and a banner saying
extensions require Chrome. They are Google's own upsell, shown to any
non-Chrome desktop browser, and do not block installation.
