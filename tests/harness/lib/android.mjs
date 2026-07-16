// Reliable Android extension-test driver — adb only, no Playwright.
//
// Why not Playwright `_android.launchBrowser` for extension tests: it rewrites
// /data/local/tmp/chrome-command-line on launch, DROPPING any --load-extension
// and ADDING --disable-extensions. So the extension under test never loads.
// (Playwright/CDP is still fine for extension-less page parity.)
//
// This driver instead does a plain `am start` with a command line we control,
// and reads results from logcat `chromium: [INFO:CONSOLE]` lines — a signal that
// survives even if GPU compositing is flaky, and needs no CDP socket (that
// socket is created in a deferred startup task and isn't reliably up).
//
// HARD EMULATOR REQUIREMENT: run the AVD with `-gpu swiftshader_indirect`.
// The host-GPU (Metal) path gives an unstable Chromium GPU process on
// desktop-android: eglCreateContext ES 3.0 -> EGL_BAD_ATTRIBUTE, the GPU process
// crashes, and after 3 crashes the browser hard-aborts ("GPU process isn't
// usable. Goodbye."). desktop-android cannot fall back to --disable-gpu. That
// instability silently corrupts page rendering and produces false negatives —
// it is what made content-script injection *look* broken for two sessions.
// preflightGpu() asserts the emulator is on a software GPU.

import { execFile } from 'node:child_process';
import { promisify } from 'node:util';
const pexec = promisify(execFile);

const PKG = process.env.AB_PKG || 'com.danosito.afterbird';
const CMDLINE = '/data/local/tmp/chrome-command-line';

async function adb(args, { timeout = 30000 } = {}) {
  const { stdout } = await pexec('adb', args, { timeout, maxBuffer: 64 * 1024 * 1024 });
  return stdout;
}
async function shell(cmd) { return adb(['shell', cmd]); }

// Assert the emulator is on a software GPU. Chromium's GPU process is stable
// there; on host GPU it crashes and corrupts results.
export async function preflightGpu() {
  const gl = await shell('dumpsys SurfaceFlinger | grep -i "GLES:" | head -1').catch(() => '');
  const swiftshader = /swiftshader|swrast|softwarepipe/i.test(gl);
  if (!swiftshader) {
    throw new Error(
      'Emulator GPU is not software-rendered. Relaunch the AVD with ' +
      '`-gpu swiftshader_indirect` — the host-GPU path crashes Chromium and ' +
      'produces false negatives. Detected: ' + gl.trim());
  }
  return gl.trim();
}

export async function forceStop() { await shell(`am force-stop ${PKG}`); }
export async function clearLogcat() { await adb(['logcat', '-c']); }
export async function dumpLogcat() { return adb(['logcat', '-d'], { timeout: 20000 }); }

// Write the command line: extensions ENABLED, the given unpacked extension
// loaded, no --disable-extensions. Do NOT add --use-gl/--use-angle here; on a
// swiftshader_indirect emulator the native path is stable and forcing in-process
// SwiftShader is itself a source of crashes.
export async function writeCommandLine(extPaths = []) {
  const load = extPaths.length ? ` --load-extension=${extPaths.join(',')}` : '';
  const line = `_ --disable-fre --no-default-browser-check${load}`;
  await shell(`printf '%s' ${JSON.stringify(line)} > ${CMDLINE}`);
}

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

// Cold-start to about:blank (this is where extensions actually load; a cold
// start straight to an http URL does not load them), then wait for a console
// marker proving the extension's background context is alive.
export async function launchAndAwaitExtension({ aliveMarker, timeoutMs = 45000 } = {}) {
  await forceStop();
  await sleep(1000);
  await clearLogcat();
  await shell(`am start -a android.intent.action.VIEW -d about:blank ${PKG}`);
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    await shell('input keyevent 4').catch(() => {}); // dismiss any ANR "wait"
    const log = await dumpLogcat();
    if (/GPU process isn't usable/.test(log)) throw new Error('GPU process died — check preflightGpu / swiftshader_indirect');
    if (!aliveMarker || log.includes(aliveMarker)) return;
    await sleep(3000);
  }
  throw new Error(`extension alive-marker not seen within ${timeoutMs}ms: ${aliveMarker}`);
}

// Navigate the already-running browser. The plain VIEW intent is flaky against
// a single-instance activity, so we cache-bust and confirm via a console marker.
export async function navigate(url) {
  await shell(`am start -a android.intent.action.VIEW -d ${JSON.stringify(url)} ${PKG}`);
}

// Collect `[INFO:CONSOLE]` payloads emitted since the last clearLogcat().
export async function consoleLines() {
  const log = await dumpLogcat();
  return log.split('\n')
    .filter((l) => l.includes('INFO:CONSOLE'))
    .map((l) => (l.match(/"(.*)", source:/) || [])[1])
    .filter(Boolean);
}

export async function screencap(outPath) {
  const png = await pexec('adb', ['exec-out', 'screencap', '-p'], { encoding: 'buffer', maxBuffer: 64 * 1024 * 1024 });
  const { writeFile } = await import('node:fs/promises');
  await writeFile(outPath, png.stdout);
}
