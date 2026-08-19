// Parity spec: uBlock Origin network filtering.
//
// The result is read from uBO's own counter, not from the test page's verdict.
// adblock.turtlecute.org probes with HEAD xhr requests, and uBO answers many of
// them with `redirect-rule=nooptext` rather than an outright block — the probe
// sees HTTP 200 and scores them "not blocked", so its percentage understates
// filtering badly (a desktop Chromium with the same uBO scores ~8% there).
// uBO's "blocked since install" counter is the same number on every target, so
// desktop and Android stay directly comparable.
export const id = 'adblock-ubo-counter';

const TEST_URL = 'https://adblock.turtlecute.org/';

// An unpacked extension's ID is derived from its install path, so it differs
// between hosts and between /data/local/tmp and app-private loads. Find it from
// the live target list instead of hard-coding it.
async function findExtensionId(page) {
  const context = page.context();
  const idFrom = (url) => (/^chrome-extension:\/\/([a-p]{32})\//.exec(url || '') || [])[1];

  // Persistent desktop contexts expose extension contexts directly; Android
  // over CDP does not, so fall back to the raw target list there.
  for (const list of [context.backgroundPages?.() ?? [], context.serviceWorkers?.() ?? []]) {
    for (const item of list) {
      const id = idFrom(item.url());
      if (id) return id;
    }
  }

  const cdp = await context.newCDPSession(page);
  try {
    const { targetInfos } = await cdp.send('Target.getTargets');
    for (const t of targetInfos) {
      const id = idFrom(t.url);
      if (id) return id;
    }
    return null;
  } finally {
    await cdp.detach().catch(() => {});
  }
}

// While uBO is still compiling its lists it answers navigations to its own
// popup with ERR_BLOCKED_BY_CLIENT, so the read is retried rather than fatal.
async function readCounter(context, extId, { attempts = 5 } = {}) {
  for (let i = 0; i < attempts; i++) {
    const popup = await context.newPage();
    try {
      await popup.goto(`chrome-extension://${extId}/popup-fenix.html`, { timeout: 30000 });
      await popup.waitForTimeout(4000);
      const text = await popup.evaluate(() => document.body.innerText);
      const m = text.match(/Blocked since install\s*\n\s*([\d,]+)/i);
      if (m) return Number(m[1].replace(/,/g, ''));
    } catch {
      // fall through to retry
    } finally {
      await popup.close().catch(() => {});
    }
    await context.pages()[0]?.waitForTimeout(5000).catch(() => {});
  }
  return NaN;
}

export async function run(page) {
  const context = page.context();
  const extId = await findExtensionId(page);
  if (!extId) {
    // Current desktop Chromium refuses to load MV2 extensions at all, so the
    // desktop target cannot serve as a reference for this spec any more (that
    // enforcement is exactly what patches/m151/0001 removes on Afterbird).
    return { skipped: true, reason: 'no chrome-extension target — uBO (MV2) did not load', pass: null };
  }
  const before = await readCounter(context, extId);

  let requests = 0;
  const onRequest = () => { requests++; };
  page.on('request', onRequest);
  await page.goto(TEST_URL, { waitUntil: 'networkidle', timeout: 60000 }).catch(() => {});
  await page.waitForTimeout(10000);
  page.off('request', onRequest);

  const after = await readCounter(context, extId);
  const blocked = Number.isNaN(before) || Number.isNaN(after) ? NaN : after - before;
  const attempted = blocked + requests;
  const pct = attempted ? Math.round((blocked / attempted) * 100) : 0;

  return {
    blocked,
    allowed: requests,
    attempted,
    pct,
    extId,
    counterAvailable: !Number.isNaN(blocked),
    pass: blocked >= 20,
  };
}
