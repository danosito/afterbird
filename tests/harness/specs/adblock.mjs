// Parity spec: does the content blocker keep ad/tracker requests off the
// network, and does the page-level ad-block test agree?
//
// Two numbers, because either alone misleads:
//
//   reachedNetwork — ad/tracker requests that got a real response. This is the
//     number that matters; a request that reaches its server is a tracker hit
//     regardless of what any test page reports.
//   pagePct — adblock.turtlecute.org's own verdict. Useful as a cross-check,
//     but it scores `redirect-rule=nooptext` responses (HTTP 200 from an inert
//     data: URL) as "not blocked", so it can read far lower than reality.
//
// The spec also reports the blocker's enabled filter lists. A low score is far
// more often a list-selection problem than a browser problem: uBO auto-selects
// regional lists from navigator.language, so an en-US device never enables
// RUS-0 and scores 1% on a page that probes mostly regional hosts — while the
// same build on a ru-RU device scores 100%.
export const id = 'adblock-live';

const TEST_URL = process.env.AB_ADS_URL || 'https://adblock.turtlecute.org/';

const AD_HOST = /doubleclick|googlesyndication|googletagservices|googletagmanager|google-analytics|adservice|adsystem|amazon-adsystem|criteo|taboola|outbrain|pubmatic|rubiconproject|scorecardresearch|adnxs|moatads|quantserve|appmetrica|tiktok|yahoo|analytics|fakepage/i;

async function filterLists(context) {
  // Read the blocker's list selection straight from its background context.
  try {
    const targets = await (await fetch(`http://localhost:${process.env.AB_CDP_PORT || 9222}/json/list`)).json();
    const bg = targets.find((t) => /chrome-extension:\/\/[a-p]{32}\/background/.test(t.url || ''));
    if (!bg) return null;
    const page = await context.newPage();
    try {
      await page.goto(bg.url, { timeout: 20000 });
      return await page.evaluate(() => {
        // Navigating to the background page gives a fresh context without the
        // extension's globals, so µBlock may not be reachable from here.
        const ub = globalThis.µBlock;
        const count = ub?.selectedFilterLists?.length;
        return { count: count ?? 'unavailable', lang: navigator.language };
      });
    } finally {
      await page.close().catch(() => {});
    }
  } catch {
    return null;
  }
}

export async function run(page) {
  const context = page.context();
  const cdp = await context.newCDPSession(page);
  await cdp.send('Network.enable');

  let reachedNetwork = 0;
  let neutralized = 0;

  cdp.on('Network.responseReceived', (e) => {
    const url = e.response.url;
    if (/^data:/.test(url)) { neutralized++; return; }
    if (AD_HOST.test(url)) reachedNetwork++;
  });
  cdp.on('Network.loadingFailed', (e) => {
    if (e.blockedReason || /ERR_BLOCKED_BY_CLIENT/.test(e.errorText || '')) neutralized++;
  });

  await page.goto(TEST_URL, { waitUntil: 'networkidle', timeout: 90000 }).catch(() => {});
  await page.waitForTimeout(12000);

  const text = await page.evaluate(() => document.body.innerText).catch(() => '');
  const pageBlocked = Number((text.match(/(\d+)\s+blocked/i) || [])[1] ?? NaN);
  const pageNotBlocked = Number((text.match(/(\d+)\s+not blocked/i) || [])[1] ?? NaN);
  const total = pageBlocked + pageNotBlocked;

  const lists = await filterLists(context);

  return {
    reachedNetwork,
    neutralized,
    pageBlocked,
    pageNotBlocked,
    pagePct: total ? Math.round((pageBlocked / total) * 100) : null,
    filterLists: lists,
    pass: reachedNetwork === 0,
  };
}
