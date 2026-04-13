# Emulator extension/devtools checks: automated vs manual

## Automated in `ci/android_emulator_test.sh`

- APK installability on running emulator/device (`adb install -r -d`)
- Browser startup smoke launch
- Internal page launchability intent checks:
  - `chrome://version/`
  - `chrome://flags/`
  - `chrome://extensions/`
  - `chrome://inspect/`
  - `chrome://inspect/#devices`
- Modern-site e2e traversal for a target duration (default 120 seconds)
- Obvious crash-signal scan from `logcat`
- Memory trend sampling (`dumpsys meminfo <package>`)

## Manual checks still required

- Verify extensions UI behavior inside `chrome://extensions/`:
  - install/uninstall flow
  - enable/disable toggles
  - extension options page and permission prompts
- Verify DevTools remote debugging UX in `chrome://inspect/#devices`
- Confirm first-run/onboarding and account/sign-in edge cases
- Validate extension compatibility with real-world CRX packages (including pinned uBlock package)
- Confirm stability under long sessions beyond smoke duration and background/foreground transitions

## Notes

Automation here is intentionally smoke-level. It validates launchability and obvious crash/memory regressions, not complete product-level extension/devtools correctness.
