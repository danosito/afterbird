// Load a set of real-world extensions one at a time and report, for each,
// whether it actually became functional — not just whether it appears in
// chrome://extensions.
//
// For every extension the check reports:
//   loaded        — the browser created an extension context for it
//   enabled       — chrome://extensions-internals lists no disable_reasons
//   listeners     — how many event listeners it registered (0 means its
//                   background never finished starting)
//   apiErrors     — "Unknown API"/FATAL lines attributed to it in logcat
//
// Usage:
//   node checks/extension-matrix.mjs                 # all known extensions
//   node checks/extension-matrix.mjs darkreader ubo  # a subset
//
// Extensions are read from the device paths in EXTENSIONS below; push them
// first (see docs). Exit 0 when every extension under test is enabled with at
// least one listener.

import { execFile } from 'node:child_process';
import { promisify } from 'node:util';
import { chromium } from 'playwright';

const pexec = promisify(execFile);
const PKG = process.env.AB_PKG || 'com.danosito.afterbird';
const PORT = Number(process.env.AB_CDP_PORT || 9222);
const CMDLINE = '/data/local/tmp/chrome-command-line';
const SETTLE_MS = Number(process.env.AB_EXT_SETTLE_MS || 35000);

const EXTENSIONS = [
  { id: 'ubo', name: 'uBlock Origin', mv: 2, path: '/data/local/tmp/ubo173' },
  { id: 'darkreader', name: 'Dark Reader', mv: 2, path: '/data/local/tmp/ext/darkreader' },
  { id: 'stylus', name: 'Stylus', mv: 2, path: '/data/local/tmp/ext/stylus-mv2' },
  { id: 'violentmonkey', name: 'Violentmonkey', mv: 2, path: '/data/local/tmp/ext/violentmonkey-mv2' },
  { id: 'sponsorblock', name: 'SponsorBlock', mv: 3, path: '/data/local/tmp/ext/sponsorblock' },
  { id: 'stylus-mv3', name: 'Stylus (MV3)', mv: 3, path: '/data/local/tmp/ext/stylus-mv3' },
  { id: 'violentmonkey-mv3', name: 'Violentmonkey (MV3)', mv: 3, path: '/data/local/tmp/ext/violentmonkey-mv3' },
  { id: 'bitwarden', name: 'Bitwarden', mv: 3, path: '/data/local/tmp/ext/bitwarden' },
  { id: 'ubolite', name: 'uBO Lite', mv: 3, path: '/data/local/tmp/ext/ubolite', needsWritableDir: true },
];

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const adb = (args, opts = {}) => pexec('adb', args, { maxBuffer: 64 * 1024 * 1024, ...opts });
const shell = (cmd) => adb(['shell', cmd]);

async function cdpSocket() {
  for (let i = 0; i < 15; i++) {
    const { stdout: pidOut } = await shell(`pidof ${PKG}`).catch(() => ({ stdout: '' }));
    const pid = pidOut.trim().split(/\s+/)[0];
    if (pid) {
      const { stdout } = await shell('cat /proc/net/unix').catch(() => ({ stdout: '' }));
      if (stdout.includes(`@chrome_devtools_remote_${pid}`)) return `chrome_devtools_remote_${pid}`;
      if (stdout.includes('@chrome_devtools_remote')) return 'chrome_devtools_remote';
    }
    await sleep(2000);
  }
  throw new Error('no CDP socket');
}

async function attach() {
  const socket = await cdpSocket();
  await adb(['forward', '--remove', `tcp:${PORT}`]).catch(() => {});
  await adb(['forward', `tcp:${PORT}`, `localabstract:${socket}`]);
  for (let i = 0; i < 15; i++) {
    try {
      const info = await (await fetch(`http://localhost:${PORT}/json/version`)).json();
      if (info['Android-Package'] === PKG) return info['Browser'];
      throw new Error(`socket belongs to ${info['Android-Package']}`);
    } catch (e) {
      if (/belongs to/.test(e.message)) throw e;
    }
    await sleep(2000);
  }
  throw new Error('CDP did not come up');
}

// Chromium writes a DNR extension's indexed rulesets into `_metadata` next to
// the unpacked extension, and it cannot write to /data/local/tmp. Copy such
// extensions into the app's own data directory first, or every MV3 blocker
// fails with a misleading "Internal error while parsing rules".
async function stageWritable(ext) {
  if (!ext.needsWritableDir) return ext.path;
  const dest = `/data/data/${PKG}/${ext.id}`;
  await shell(`run-as ${PKG} sh -c 'rm -rf ${dest}; cp -r ${ext.path} ${dest}'`);
  return dest;
}

async function runOne(ext) {
  await shell(`am force-stop ${PKG}`);
  await shell(`pm clear ${PKG}`).catch(() => {});
  const loadPath = await stageWritable(ext);
  await shell(`printf '%s' '_ --disable-fre --no-default-browser-check --load-extension=${loadPath}' > ${CMDLINE}`);
  await adb(['logcat', '-c']).catch(() => {});
  await shell(`am start -a android.intent.action.VIEW -d about:blank ${PKG}`);
  await sleep(SETTLE_MS);

  const result = { id: ext.id, name: ext.name, mv: ext.mv, loaded: false, enabled: false, listeners: 0, apiErrors: 0 };
  await attach();

  const browser = await chromium.connectOverCDP(`http://localhost:${PORT}`);
  try {
    const page = await browser.contexts()[0].newPage();
    await page.goto('chrome://extensions-internals/', { timeout: 25000 }).catch(() => {});
    await page.waitForTimeout(3000);
    const text = await page.evaluate(() => document.body.innerText);
    // An MV3 service worker sleeps, so its CDP target comes and goes; the
    // internals page is the authoritative record of what the browser loaded.
    const ids = [...text.matchAll(/"id":\s*"([a-p]{32})"/g)].map((m) => m[1]);
    result.loaded = ids.length > 0;
    result.extId = ids[0] ?? null;
    const reasons = [...text.matchAll(/"disable_reasons":\s*\[([^\]]*)\]/g)].map((m) => m[1].trim());
    result.enabled = result.loaded && reasons.length > 0 && reasons.every((r) => r === '');
    result.disableReasons = reasons.filter(Boolean).map((r) => r.replace(/\s+/g, ' ').slice(0, 60));
    result.listeners = (text.match(/"event_name"/g) || []).length;
    await page.close().catch(() => {});
  } finally {
    await browser.close().catch(() => {});
  }

  const { stdout: log } = await adb(['logcat', '-d']).catch(() => ({ stdout: '' }));
  result.apiErrors = (log.match(/Unknown (Extension )?API|FATAL:.*extension/gi) || []).length;
  result.pass = result.loaded && result.enabled && result.listeners > 0 && result.apiErrors === 0;
  return result;
}

const wanted = process.argv.slice(2);
const subjects = wanted.length ? EXTENSIONS.filter((e) => wanted.includes(e.id)) : EXTENSIONS;
const rows = [];
for (const ext of subjects) {
  process.stdout.write(`[matrix] ${ext.name} (MV${ext.mv})… `);
  let row;
  try {
    row = await runOne(ext);
  } catch (e) {
    row = { id: ext.id, name: ext.name, mv: ext.mv, error: String(e.message).slice(0, 90), pass: false };
  }
  rows.push(row);
  console.log(row.pass ? 'ok' : `FAIL ${JSON.stringify(row)}`);
}
console.log('\nMATRIX-JSON', JSON.stringify(rows));
process.exit(rows.every((r) => r.pass) ? 0 : 1);
