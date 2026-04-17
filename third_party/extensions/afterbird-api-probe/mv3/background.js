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
