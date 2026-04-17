// Afterbird API Probe — MV3 service worker background.
//
// Loads probes.js, runs the probe list on startup/install, and responds to
// messages from popup.js:
//   { what: "probe-results" } -> { results, manifest }
//   { what: "probe-rerun" }   -> { results, manifest } after re-running
//   { what: "probe-echo", payload } -> { ok: true, payload }

try {
  importScripts('probes.js');
} catch (e) {
  // If probes.js failed to load, keep the listener below functional with an
  // empty result set so the popup still shows something.
  self.AfterbirdProbes = self.AfterbirdProbes || { allMv3: [], runAll: function () { return Promise.resolve([]); } };
  console.error('[afterbird-probe] failed to load probes.js:', e);
}

var LAST = {
  startedAt: 0,
  finishedAt: 0,
  results: []
};

function runProbes() {
  LAST.startedAt = Date.now();
  LAST.finishedAt = 0;
  return self.AfterbirdProbes.runAll(self.AfterbirdProbes.allMv3).then(function (results) {
    LAST.results = results;
    LAST.finishedAt = Date.now();
    // Emit a compact one-line summary to the console so logcat can pick it up
    // — useful for headless probe runs where no popup is opened.
    try {
      var counts = { pass: 0, fail: 0, unavailable: 0 };
      var failed = [];
      for (var i = 0; i < results.length; i++) {
        var s = results[i].status;
        if (counts[s] == null) counts[s] = 0;
        counts[s]++;
        if (s === 'fail') {
          failed.push(results[i].name + ' :: ' + (results[i].note || ''));
        }
      }
      var summary = 'pass=' + counts.pass + ' fail=' + counts.fail +
        ' unavailable=' + counts.unavailable + ' in ' + (LAST.finishedAt - LAST.startedAt) + 'ms';
      console.log('[afterbird-probe mv3] ' + summary);
      if (failed.length) {
        console.log('[afterbird-probe mv3] failures:');
        for (var j = 0; j < failed.length; j++) console.log('  - ' + failed[j]);
      }
      // Also persist to storage.local so a headless harness can pull results
      // without opening DevTools. The storage backend round-trips through the
      // profile's LevelDB extensions store.
      try {
        chrome.storage.local.set({
          __afterbird_probe_last: {
            mv: 3,
            startedAt: LAST.startedAt,
            finishedAt: LAST.finishedAt,
            counts: counts,
            failed: failed,
            results: results
          }
        });
      } catch (e) { /* ignore */ }
    } catch (e) { /* ignore */ }
    return results;
  });
}

// Run on install and browser startup.
chrome.runtime.onInstalled.addListener(function () { runProbes(); });
if (chrome.runtime.onStartup) {
  chrome.runtime.onStartup.addListener(function () { runProbes(); });
}
// Also run once when the service worker spins up for the first time.
runProbes();

chrome.runtime.onMessage.addListener(function (msg, sender, sendResponse) {
  if (!msg || typeof msg !== 'object') return false;
  if (msg.what === 'probe-echo') {
    sendResponse({ ok: true, payload: msg.payload });
    return false;
  }
  if (msg.what === 'probe-results') {
    sendResponse({
      manifest: chrome.runtime.getManifest(),
      startedAt: LAST.startedAt,
      finishedAt: LAST.finishedAt,
      results: LAST.results
    });
    return false;
  }
  if (msg.what === 'probe-rerun') {
    runProbes().then(function (results) {
      sendResponse({
        manifest: chrome.runtime.getManifest(),
        startedAt: LAST.startedAt,
        finishedAt: LAST.finishedAt,
        results: results
      });
    });
    return true; // async response
  }
  return false;
});

chrome.runtime.onConnect.addListener(function (port) {
  if (port.name !== 'probe-port') return;
  port.onMessage.addListener(function (m) {
    if (m && m.hello) port.postMessage({ echoed: 'hello' });
  });
});

if (chrome.commands && chrome.commands.onCommand) {
  chrome.commands.onCommand.addListener(function (name) {
    if (name === 'probe-rerun') runProbes();
  });
}
