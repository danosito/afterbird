// On-device uBlock Origin network-filtering check.
//
// Playwright's `_android.launchBrowser` rewrites the command line and drops
// --load-extension, so the extension under test never loads (see README). This
// check launches the browser itself with the adb driver, then attaches to the
// already-running browser over CDP (connectOverCDP does not touch the command
// line) to read the result page.
//
// Prereqs: emulator on `-gpu swiftshader_indirect`; uBlock Origin unpacked on
// the device (default /data/local/tmp/ubo — see ci/fetch_ublock_chromium.sh for
// the pinned package).
//
//   node checks/adblock-device.mjs
//
// Exit 0 = uBO blocked requests on the test page; exit 1 = it did not, or the
// environment is wrong.

import { execFile } from 'node:child_process';
import { promisify } from 'node:util';
import { chromium } from 'playwright';
import { preflightGpu, writeCommandLine, launchAndAwaitExtension } from '../lib/android.mjs';
import * as adblock from '../specs/adblock.mjs';

const pexec = promisify(execFile);
const EXT = process.env.AB_UBO_PATH || '/data/local/tmp/ubo';
const PORT = Number(process.env.AB_CDP_PORT || 9222);
// uBO compiles its filter lists on first run; blocking climbs for a while after
// the background page reports alive.
const SETTLE_MS = Number(process.env.AB_UBO_SETTLE_MS || 60000);

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

// The plain `chrome_devtools_remote` name is first-come-first-served: on a
// device that already has another Chromium browser running (a stock Kiwi, say)
// it belongs to that browser, and forwarding to it silently drives the wrong
// app. Prefer our process's own `chrome_devtools_remote_<pid>` socket.
async function cdpSocketName({ timeoutMs = 30000 } = {}) {
  const pkg = process.env.AB_PKG || 'com.danosito.afterbird';
  const deadline = Date.now() + timeoutMs;
  // The socket is opened from a deferred startup task, so it appears a little
  // after the process does.
  while (Date.now() < deadline) {
    const { stdout: pidOut } = await pexec('adb', ['shell', 'pidof', pkg]).catch(() => ({ stdout: '' }));
    const pid = pidOut.trim().split(/\s+/)[0];
    if (pid) {
      const { stdout } = await pexec('adb', ['shell', 'cat /proc/net/unix']).catch(() => ({ stdout: '' }));
      if (stdout.includes(`@chrome_devtools_remote_${pid}`)) return `chrome_devtools_remote_${pid}`;
    }
    await sleep(2000);
  }
  return 'chrome_devtools_remote';
}

async function forwardCdp() {
  const socket = await cdpSocketName();
  await pexec('adb', ['forward', `tcp:${PORT}`, `localabstract:${socket}`]);
  const deadline = Date.now() + 30000;
  while (Date.now() < deadline) {
    try {
      const res = await fetch(`http://localhost:${PORT}/json/version`);
      if (res.ok) {
        const info = await res.json();
        const pkg = process.env.AB_PKG || 'com.danosito.afterbird';
        if (info['Android-Package'] && info['Android-Package'] !== pkg) {
          throw new Error(
            `CDP socket ${socket} belongs to ${info['Android-Package']}, not ${pkg} — ` +
            'another Chromium browser owns the unnamed socket on this device');
        }
        return `${info['Browser']} via ${socket}`;
      }
    } catch (e) {
      if (/belongs to/.test(e.message)) throw e;
    }
    await sleep(2000);
  }
  throw new Error('CDP socket did not come up — is this a debuggable (test-variant) build?');
}

const gl = await preflightGpu();
console.log('[adblock-device] GPU:', gl);

await writeCommandLine([EXT]);
await launchAndAwaitExtension({ timeoutMs: 60000 });
const browserVersion = await forwardCdp();
console.log('[adblock-device] attached to', browserVersion);

console.log(`[adblock-device] letting uBO compile filter lists (${SETTLE_MS}ms)`);
await sleep(SETTLE_MS);

const browser = await chromium.connectOverCDP(`http://localhost:${PORT}`);
const context = browser.contexts()[0];
const page = await context.newPage();
let result;
try {
  result = await adblock.run(page);
} finally {
  await page.close().catch(() => {});
  await browser.close().catch(() => {});
}

console.log('[adblock-device]', JSON.stringify(result));
console.log('ADBLOCK-JSON', JSON.stringify({ target: 'android-adb', spec: adblock.id, ...result }));
process.exit(result.pass ? 0 : 1);
