// A "target" is any Playwright page we can drive: desktop Chromium (the parity
// reference) or Afterbird on Android over CDP. Suites are written once against
// the `page` and run against every target, so parity is a diff, not a re-write.
import { chromium, _android as android } from 'playwright';

export async function openDesktop({ headless = true } = {}) {
  const browser = await chromium.launch({ headless });
  const page = await browser.newPage();
  return { name: 'desktop-chromium', page, async close() { await browser.close(); } };
}

// Requires the target APK to expose the chrome_devtools_remote CDP socket.
// Release Afterbird builds keep it closed; the test/dev build must enable it.
export async function openAndroid({ pkg = process.env.AB_PKG || 'com.danosito.afterbird' } = {}) {
  const [device] = await android.devices();
  if (!device) throw new Error('no adb device');
  const context = await device.launchBrowser({ command: pkg });
  const page = context.pages()[0] || await context.newPage();
  return {
    name: `android:${pkg}`, page, device, context,
    async close() { await context.close(); await device.close(); },
  };
}
