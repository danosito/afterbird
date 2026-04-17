// Afterbird API Probe — popup (shared shape across MV2 and MV3).
(function () {
  'use strict';

  var ICON = {
    pass: '\u2713',         // check mark
    fail: '\u2717',         // ballot x
    unavailable: '\u2014',  // em dash
    'n/a': '\u2014'
  };

  function $(id) { return document.getElementById(id); }

  function render(payload) {
    var manifest = (payload && payload.manifest) || {};
    var results = (payload && payload.results) || [];

    var name = manifest.name || 'Afterbird API Probe';
    var mv = manifest.manifest_version || '?';
    $('title').textContent = name;

    var subParts = ['manifest_version: ' + mv];
    if (manifest.version) subParts.push('v' + manifest.version);
    if (payload && payload.finishedAt) {
      var ago = Math.max(0, Math.round((Date.now() - payload.finishedAt) / 1000));
      subParts.push('last run ' + ago + 's ago');
    } else if (payload && payload.startedAt && !payload.finishedAt) {
      subParts.push('running...');
    }
    $('sub').textContent = subParts.join(' \u2022 ');

    var body = $('body');
    if (!results.length) {
      body.innerHTML = '<div class="empty">No results yet.</div>';
      $('summary').textContent = '';
      return;
    }

    var counts = { pass: 0, fail: 0, unavailable: 0, 'n/a': 0 };
    for (var i = 0; i < results.length; i++) {
      var s = results[i].status;
      if (counts[s] == null) counts[s] = 0;
      counts[s]++;
    }
    $('summary').textContent =
      counts.pass + ' pass, ' +
      counts.fail + ' fail, ' +
      counts.unavailable + ' unavail';

    var rows = ['<table><thead><tr><th></th><th>API</th><th>Note</th><th>ms</th></tr></thead><tbody>'];
    for (var j = 0; j < results.length; j++) {
      var r = results[j];
      var cls = r.status === 'pass' ? 'pass'
        : r.status === 'fail' ? 'fail'
        : r.status === 'unavailable' ? 'unavailable'
        : 'na';
      var icon = ICON[r.status] || '?';
      rows.push(
        '<tr>' +
        '<td class="icon ' + cls + '">' + icon + '</td>' +
        '<td>' + escapeHtml(r.name || '') + '</td>' +
        '<td class="note">' + escapeHtml(r.note || '') + '</td>' +
        '<td class="ms">' + (typeof r.ms === 'number' ? r.ms : '') + '</td>' +
        '</tr>'
      );
    }
    rows.push('</tbody></table>');
    body.innerHTML = rows.join('');
  }

  function escapeHtml(s) {
    return String(s)
      .replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;')
      .replace(/'/g, '&#39;');
  }

  function send(what) {
    return new Promise(function (resolve) {
      try {
        chrome.runtime.sendMessage({ what: what }, function (resp) {
          var err = chrome.runtime.lastError;
          if (err) { resolve({ error: err.message }); return; }
          resolve(resp || {});
        });
      } catch (e) {
        resolve({ error: String(e) });
      }
    });
  }

  function refresh() {
    send('probe-results').then(function (resp) {
      if (resp && resp.error) {
        $('sub').textContent = 'error: ' + resp.error;
        return;
      }
      render(resp);
    });
  }

  function rerun() {
    $('sub').textContent = 'running...';
    send('probe-rerun').then(function (resp) {
      if (resp && resp.error) {
        $('sub').textContent = 'error: ' + resp.error;
        return;
      }
      render(resp);
    });
  }

  document.addEventListener('DOMContentLoaded', function () {
    $('rerun').addEventListener('click', rerun);
    refresh();
  });
})();
