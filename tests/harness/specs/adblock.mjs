// Parity spec: does a content blocker actually keep ad/tracker requests off the
// network?
//
// Method: load an ad-carrying page and count how many requests to known ad and
// tracker hosts reach the network. A request is "neutralized" when it is either
// cancelled (ERR_BLOCKED_BY_CLIENT) or redirected to an inert resource — uBO's
// `redirect-rule=nooptext` sends the request to `data:text/plain;base64,Cg==`
// instead of cancelling it, which is just as effective but looks like a 200 to
// the page.
//
// Why not adblock.turtlecute.org's own verdict: it probes with HEAD xhr, uBO
// answers most of those with redirect-rule, and the page scores the resulting
// 200s as "not blocked" — it reported 1-2% for a uBO that was demonstrably
// filtering. Measured against a live site instead, the same setup takes ad
// requests from 13 to 1.
export const id = 'adblock-live-site';

const TEST_URL = process.env.AB_ADS_URL || 'https://www.dictionary.com/browse/test';
// Baseline (no extension) for TEST_URL; requests above this fraction fail.
const MAX_FRACTION = Number(process.env.AB_ADS_MAX_FRACTION || 0.25);
const BASELINE = Number(process.env.AB_ADS_BASELINE || 13);

const AD_HOST = /doubleclick|googlesyndication|googletagservices|googletagmanager|google-analytics|adservice|adsystem|amazon-adsystem|criteo|taboola|outbrain|pubmatic|rubiconproject|scorecardresearch|adnxs|moatads|quantserve|casalemedia|openx|33across/i;

export async function run(page) {
  const cdp = await page.context().newCDPSession(page);
  await cdp.send('Network.enable');

  let attempted = 0;
  let reachedNetwork = 0;
  let neutralized = 0;

  cdp.on('Network.requestWillBeSent', (e) => {
    if (AD_HOST.test(e.request.url)) attempted++;
  });
  cdp.on('Network.responseReceived', (e) => {
    if (!AD_HOST.test(e.response.url)) return;
    // A redirect to an inert data: URL means the blocker swallowed it.
    if (/^data:/.test(e.response.url)) neutralized++;
    else reachedNetwork++;
  });
  cdp.on('Network.loadingFailed', (e) => {
    if (e.blockedReason || /ERR_BLOCKED_BY_CLIENT/.test(e.errorText || '')) neutralized++;
  });

  await page.goto(TEST_URL, { waitUntil: 'networkidle', timeout: 90000 }).catch(() => {});
  await page.waitForTimeout(12000);

  const fraction = BASELINE ? reachedNetwork / BASELINE : 1;
  return {
    url: TEST_URL,
    attempted,
    reachedNetwork,
    neutralized,
    baseline: BASELINE,
    fractionOfBaseline: Number(fraction.toFixed(2)),
    pass: reachedNetwork <= BASELINE * MAX_FRACTION,
  };
}
