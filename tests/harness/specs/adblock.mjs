// Parity spec: uBlock Origin network filtering on the turtlecute test page.
// Returns a normalized result so desktop and Android are directly comparable.
export const id = 'adblock-turtlecute';
export async function run(page) {
  await page.goto('https://adblock.turtlecute.org/', { waitUntil: 'networkidle', timeout: 60000 }).catch(() => {});
  await page.waitForTimeout(8000);
  // The page renders "N blocked" / "M not blocked"; read them from the DOM.
  const body = await page.evaluate(() => document.body.innerText);
  const blocked = Number((body.match(/(\d+)\s+blocked/i) || [])[1] ?? NaN);
  const notBlocked = Number((body.match(/(\d+)\s+not blocked/i) || [])[1] ?? NaN);
  const total = blocked + notBlocked;
  const pct = total ? Math.round((blocked / total) * 100) : 0;
  return { blocked, notBlocked, total, pct, pass: pct >= 90 };
}
