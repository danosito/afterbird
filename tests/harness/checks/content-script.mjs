// Standalone on-device check: does a manifest content_script inject (JS + CSS)?
//
// Uses the adb driver (extensions can't load under Playwright). Result comes
// from a logcat console marker, so it's robust to GPU-compositing flakiness.
//
// Prereq: emulator on `-gpu swiftshader_indirect`; the probe extension pushed to
// /data/local/tmp/csprobe (manifest v3, content_scripts <all_urls> at
// document_start that console.logs AB-CS-INJECTED and paints the page magenta).
//
//   node checks/content-script.mjs
//
// Exit 0 = content scripts inject; exit 1 = they don't (or env is wrong).

import {
  preflightGpu, writeCommandLine, launchAndAwaitExtension,
  navigate, consoleLines, clearLogcat, screencap,
} from '../lib/android.mjs';

const EXT = '/data/local/tmp/csprobe';
const BG_ALIVE = 'AB-BG-ALIVE';   // printed by the probe's service worker
const CS_MARK = 'AB-CS-INJECTED'; // printed by the probe's content script
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

const gl = await preflightGpu();
console.log('GPU:', gl);

await writeCommandLine([EXT]);
await launchAndAwaitExtension({ aliveMarker: BG_ALIVE, timeoutMs: 60000 });
console.log('extension background is alive');

await clearLogcat();
await navigate('https://example.com/');
await sleep(9000);

const lines = await consoleLines();
const injected = lines.some((l) => l.includes(CS_MARK));
await screencap('/tmp/ab-content-script.png').catch(() => {});

console.log('content-script console markers:', lines.filter((l) => l.includes(CS_MARK)));
console.log(injected ? 'PASS: content scripts inject' : 'FAIL: no content-script injection');
process.exit(injected ? 0 : 1);
