// Functional checks: does each extension actually do its job on a page, not
// just load? extension-matrix.mjs proves the background context starts and
// registers listeners; this one proves observable behaviour.
//
//   node checks/extension-function.mjs            # every subject
//   node checks/extension-function.mjs darkreader
//
// Each subject launches in a clean profile with only itself loaded.

import { execFile } from 'node:child_process';
import { promisify } from 'node:util';
import { chromium } from 'playwright';

const pexec = promisify(execFile);
const PKG = process.env.AB_PKG || 'com.danosito.afterbird';
const PORT = Number(process.env.AB_CDP_PORT || 9222);
const CMDLINE = '/data/local/tmp/chrome-command-line';

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const adb = (args) => pexec('adb', args, { maxBuffer: 64 * 1024 * 1024 });
const shell = (cmd) => adb(['shell', cmd]);

const AD_HOST = /doubleclick|googlesyndication|googletagservices|googletagmanager|google-analytics|adservice|adsystem|amazon-adsystem|criteo|taboola|outbrain|pubmatic|rubiconproject|scorecardresearch|adnxs|moatads|quantserve/i;

const SUBJECTS = [
  {
    id: 'ubo',
    name: 'uBlock Origin',
    path: '/data/local/tmp/ubo173',
    settleMs: 90000,
    // Ad requests must not reach the network on an ad-carrying page.
    async run(page, context) {
      const cdp = await context.newCDPSession(page);
      await cdp.send('Network.enable');
      let reached = 0;
      let neutralized = 0;
      cdp.on('Network.responseReceived', (e) => {
        if (!AD_HOST.test(e.response.url)) return;
        if (/^data:/.test(e.response.url)) neutralized++;
        else reached++;
      });
      cdp.on('Network.loadingFailed', (e) => {
        if (e.blockedReason || /ERR_BLOCKED_BY_CLIENT/.test(e.errorText || '')) neutralized++;
      });
      await page.goto('https://www.dictionary.com/browse/test', { waitUntil: 'networkidle', timeout: 90000 }).catch(() => {});
      await page.waitForTimeout(10000);
      return { detail: { adRequestsReachingNetwork: reached, neutralized }, pass: reached <= 3 };
    },
  },
  {
    id: 'darkreader',
    name: 'Dark Reader',
    path: '/data/local/tmp/ext/darkreader',
    settleMs: 25000,
    // Dark Reader injects a style element and darkens the page background.
    async run(page) {
      await page.goto('https://example.com/', { waitUntil: 'load', timeout: 60000 }).catch(() => {});
      await page.waitForTimeout(8000);
      const info = await page.evaluate(() => {
        const bg = getComputedStyle(document.body).backgroundColor;
        const m = bg.match(/\d+/g) || [];
        const luminance = m.length >= 3 ? (Number(m[0]) + Number(m[1]) + Number(m[2])) / 3 : 255;
        return {
          bg,
          luminance,
          styleNodes: document.querySelectorAll('style.darkreader, style[class*="darkreader"]').length,
        };
      });
      return { detail: info, pass: info.styleNodes > 0 || info.luminance < 128 };
    },
  },
  {
    id: 'violentmonkey',
    name: 'Violentmonkey',
    path: '/data/local/tmp/ext/violentmonkey-mv2',
    settleMs: 25000,
    // Its dashboard is an extension page that must render its own UI.
    async run(page, context, extId) {
      await page.goto(`chrome-extension://${extId}/options/index.html`, { timeout: 40000 }).catch(() => {});
      await page.waitForTimeout(6000);
      const info = await page.evaluate(() => ({
        text: document.body.innerText.replace(/\s+/g, ' ').slice(0, 80),
        nodes: document.querySelectorAll('div, button, a').length,
      }));
      return { detail: info, pass: info.nodes > 10 };
    },
  },
  {
    id: 'stylus',
    name: 'Stylus',
    path: '/data/local/tmp/ext/stylus-mv2',
    settleMs: 25000,
    async run(page, context, extId) {
      await page.goto(`chrome-extension://${extId}/manage.html`, { timeout: 40000 }).catch(() => {});
      await page.waitForTimeout(6000);
      const info = await page.evaluate(() => ({
        text: document.body.innerText.replace(/\s+/g, ' ').slice(0, 80),
        nodes: document.querySelectorAll('div, button, a').length,
      }));
      return { detail: info, pass: info.nodes > 10 };
    },
  },
];

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
      if (info['Android-Package'] === PKG) return;
      throw new Error(`socket belongs to ${info['Android-Package']}`);
    } catch (e) {
      if (/belongs to/.test(e.message)) throw e;
    }
    await sleep(2000);
  }
  throw new Error('CDP did not come up');
}

async function extensionId(browser) {
  const page = await browser.contexts()[0].newPage();
  try {
    await page.goto('chrome://extensions-internals/', { timeout: 25000 }).catch(() => {});
    await page.waitForTimeout(2500);
    const text = await page.evaluate(() => document.body.innerText);
    return (text.match(/"id":\s*"([a-p]{32})"/) || [])[1] ?? null;
  } finally {
    await page.close().catch(() => {});
  }
}

const wanted = process.argv.slice(2);
const subjects = wanted.length ? SUBJECTS.filter((s) => wanted.includes(s.id)) : SUBJECTS;
const rows = [];

for (const subject of subjects) {
  process.stdout.write(`[function] ${subject.name}… `);
  let row = { id: subject.id, name: subject.name };
  try {
    await shell(`am force-stop ${PKG}`);
    await shell(`pm clear ${PKG}`).catch(() => {});
    await shell(`printf '%s' '_ --disable-fre --no-default-browser-check --load-extension=${subject.path}' > ${CMDLINE}`);
    await shell(`am start -a android.intent.action.VIEW -d about:blank ${PKG}`);
    await sleep(subject.settleMs);
    await attach();

    const browser = await chromium.connectOverCDP(`http://localhost:${PORT}`);
    try {
      const extId = await extensionId(browser);
      const context = browser.contexts()[0];
      const page = await context.newPage();
      const out = await subject.run(page, context, extId);
      row = { ...row, extId, ...out };
      await page.close().catch(() => {});
    } finally {
      await browser.close().catch(() => {});
    }
  } catch (e) {
    row.error = String(e.message).slice(0, 100);
    row.pass = false;
  }
  rows.push(row);
  console.log(row.pass ? `ok ${JSON.stringify(row.detail ?? {})}` : `FAIL ${JSON.stringify(row)}`);
}

console.log('\nFUNCTION-JSON', JSON.stringify(rows));
process.exit(rows.every((r) => r.pass) ? 0 : 1);
