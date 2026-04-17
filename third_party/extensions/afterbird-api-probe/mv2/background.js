// Afterbird API Probe — MV2 background page.
//
// Loaded from background.html after probes.js.

var LAST = {
  startedAt: 0,
  finishedAt: 0,
  results: []
};

function runProbes() {
  LAST.startedAt = Date.now();
  LAST.finishedAt = 0;
  return self.AfterbirdProbes.runAll(self.AfterbirdProbes.allMv2).then(function (results) {
    LAST.results = results;
    LAST.finishedAt = Date.now();
    return results;
  });
}

if (chrome.runtime.onInstalled) {
  chrome.runtime.onInstalled.addListener(function () { runProbes(); });
}
if (chrome.runtime.onStartup) {
  chrome.runtime.onStartup.addListener(function () { runProbes(); });
}
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
    return true;
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
