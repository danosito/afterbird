// Run every spec against each requested target and print a parity table.
import { openDesktop, openAndroid } from './lib/target.mjs';
import * as adblock from './specs/adblock.mjs';

const SPECS = [adblock];
const want = process.argv.slice(2);                 // e.g. "android" "desktop"
const targets = [];
if (!want.length || want.includes('desktop')) targets.push(await openDesktop());
if (want.includes('android')) targets.push(await openAndroid());

const rows = [];
for (const t of targets) {
  for (const spec of SPECS) {
    let r; try { r = await spec.run(t.page); } catch (e) { r = { error: String(e) }; }
    rows.push({ target: t.name, spec: spec.id, ...r });
    console.log(`[${t.name}] ${spec.id}:`, JSON.stringify(r));
  }
  await t.close();
}
console.log('\nPARITY-JSON', JSON.stringify(rows));
