// Afterbird API Probe — MV2 probe list.
//
// Shape mirrors mv3/probes.js. Kept separate (duplicated) on purpose — no
// build step; diffs are intentionally small.

(function (root) {
  'use strict';

  var ONE_PX_PNG =
    'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNkYAAAAAYAAjCB0C8AAAAASUVORK5CYII=';

  function callAsync(fn) {
    return new Promise(function (resolve, reject) {
      try {
        var maybe = fn(function (result) {
          var err = (typeof chrome !== 'undefined' && chrome.runtime && chrome.runtime.lastError) || null;
          if (err) reject(new Error(err.message || String(err)));
          else resolve(result);
        });
        if (maybe && typeof maybe.then === 'function') maybe.then(resolve, reject);
      } catch (e) { reject(e); }
    });
  }

  function has(pathStr) {
    var parts = pathStr.split('.');
    var cur = (typeof chrome !== 'undefined') ? chrome : undefined;
    for (var i = 0; i < parts.length; i++) {
      if (cur == null) return false;
      cur = cur[parts[i]];
    }
    return cur != null;
  }

  function unavailable(note) { return { status: 'unavailable', note: note || '' }; }
  function pass(note) { return { status: 'pass', note: note || '' }; }
  function fail(note) { return { status: 'fail', note: note || '' }; }

  var sharedProbes = [
    {
      name: 'chrome.runtime.id',
      run: function () {
        if (!has('runtime')) return Promise.resolve(unavailable('chrome.runtime missing'));
        var id = chrome.runtime.id;
        if (typeof id === 'string' && id.length > 0) return Promise.resolve(pass(id));
        return Promise.resolve(fail('no id'));
      }
    },
    {
      name: 'chrome.runtime.getManifest()',
      run: function () {
        if (!has('runtime.getManifest')) return Promise.resolve(unavailable());
        try {
          var m = chrome.runtime.getManifest();
          return Promise.resolve(pass('mv=' + m.manifest_version + ' v=' + m.version));
        } catch (e) { return Promise.resolve(fail(String(e))); }
      }
    },
    {
      name: 'chrome.runtime.onMessage round-trip',
      run: function () {
        if (!has('runtime.onMessage')) return Promise.resolve(unavailable());
        if (typeof window !== 'undefined' && window === self) {
          // Background (MV2 background page has a window).
          return new Promise(function (resolve) {
            try {
              chrome.runtime.sendMessage({ what: 'probe-echo', payload: 42 }, function (resp) {
                var err = chrome.runtime.lastError;
                if (err) { resolve(fail(err.message)); return; }
                if (resp && resp.ok && resp.payload === 42) resolve(pass());
                else resolve(fail('no echo'));
              });
            } catch (e) { resolve(fail(String(e))); }
          });
        }
        return Promise.resolve(pass('listener API present'));
      }
    },
    {
      name: 'chrome.runtime.connect port round-trip',
      run: function () {
        if (!has('runtime.connect')) return Promise.resolve(unavailable());
        return new Promise(function (resolve) {
          var done = false;
          function finish(r) { if (!done) { done = true; resolve(r); } }
          try {
            var port = chrome.runtime.connect({ name: 'probe-port' });
            port.onMessage.addListener(function (msg) {
              if (msg && msg.echoed === 'hello') finish(pass());
              else finish(fail('bad echo'));
              try { port.disconnect(); } catch (e) { /* ignore */ }
            });
            port.onDisconnect.addListener(function () {
              var err = chrome.runtime.lastError;
              if (err) finish(fail(err.message));
            });
            port.postMessage({ hello: true });
            setTimeout(function () { finish(fail('timeout')); }, 1500);
          } catch (e) { finish(fail(String(e))); }
        });
      }
    },
    {
      name: 'chrome.storage.local set/get/remove',
      run: function () {
        if (!has('storage.local')) return Promise.resolve(unavailable());
        var key = '__probe_local_' + Date.now();
        return callAsync(function (cb) { chrome.storage.local.set({ [key]: 'ok' }, cb); })
          .then(function () { return callAsync(function (cb) { chrome.storage.local.get(key, cb); }); })
          .then(function (out) {
            if (!out || out[key] !== 'ok') throw new Error('roundtrip mismatch');
            return callAsync(function (cb) { chrome.storage.local.remove(key, cb); });
          })
          .then(function () { return pass(); })
          .catch(function (e) { return fail(String(e && e.message || e)); });
      }
    },
    {
      name: 'chrome.storage.sync set/get',
      run: function () {
        if (!has('storage.sync')) return Promise.resolve(unavailable());
        var key = '__probe_sync_' + Date.now();
        return callAsync(function (cb) { chrome.storage.sync.set({ [key]: 'ok' }, cb); })
          .then(function () { return callAsync(function (cb) { chrome.storage.sync.get(key, cb); }); })
          .then(function (out) {
            if (!out || out[key] !== 'ok') throw new Error('roundtrip mismatch');
            try { chrome.storage.sync.remove(key); } catch (e) { /* ignore */ }
            return pass();
          })
          .catch(function (e) { return fail(String(e && e.message || e)); });
      }
    },
    {
      name: 'chrome.tabs.query({})',
      run: function () {
        if (!has('tabs.query')) return Promise.resolve(unavailable());
        return callAsync(function (cb) { chrome.tabs.query({}, cb); })
          .then(function (tabs) {
            if (!Array.isArray(tabs)) return fail('not an array');
            return pass(tabs.length + ' tab(s)');
          })
          .catch(function (e) { return fail(String(e && e.message || e)); });
      }
    },
    {
      name: 'chrome.tabs.create then remove',
      run: function () {
        if (!has('tabs.create') || !has('tabs.remove')) return Promise.resolve(unavailable());
        return callAsync(function (cb) { chrome.tabs.create({ url: 'about:blank', active: false }, cb); })
          .then(function (tab) {
            if (!tab || typeof tab.id !== 'number') throw new Error('no tab.id');
            return callAsync(function (cb) { chrome.tabs.remove(tab.id, cb); })
              .then(function () { return pass('id=' + tab.id); });
          })
          .catch(function (e) { return fail(String(e && e.message || e)); });
      }
    },
    {
      name: 'chrome.alarms.create + onAlarm fires',
      run: function () {
        if (!has('alarms.create') || !has('alarms.onAlarm')) return Promise.resolve(unavailable());
        return new Promise(function (resolve) {
          var name = 'probe-alarm-' + Date.now();
          var done = false;
          function finish(r) { if (!done) { done = true; try { chrome.alarms.onAlarm.removeListener(listener); } catch (e) {} resolve(r); } }
          function listener(alarm) { if (alarm && alarm.name === name) finish(pass()); }
          try {
            chrome.alarms.onAlarm.addListener(listener);
            chrome.alarms.create(name, { when: Date.now() + 500 });
            setTimeout(function () { finish(fail('alarm did not fire within 3s')); }, 3000);
          } catch (e) { finish(fail(String(e))); }
        });
      }
    },
    {
      name: 'chrome.contextMenus.create',
      run: function () {
        if (!has('contextMenus.create')) return Promise.resolve(unavailable());
        try {
          var id = 'probe-menu-' + Date.now();
          var created = chrome.contextMenus.create({ id: id, title: 'probe', contexts: ['all'] }, function () {
            // ignore
          });
          try { chrome.contextMenus.remove(id); } catch (e) {}
          return Promise.resolve(pass(String(created || id)));
        } catch (e) { return Promise.resolve(fail(String(e))); }
      }
    },
    {
      name: 'chrome.declarativeNetRequest.updateDynamicRules',
      run: function () {
        if (!has('declarativeNetRequest.updateDynamicRules')) return Promise.resolve(unavailable());
        var ruleId = 999000 + Math.floor(Math.random() * 1000);
        return callAsync(function (cb) {
          chrome.declarativeNetRequest.updateDynamicRules({
            addRules: [{
              id: ruleId,
              priority: 1,
              action: { type: 'block' },
              condition: { urlFilter: 'https://example.invalid/probe', resourceTypes: ['xmlhttprequest'] }
            }],
            removeRuleIds: [ruleId]
          }, cb);
        })
          .then(function () {
            try {
              chrome.declarativeNetRequest.updateDynamicRules({ removeRuleIds: [ruleId] }, function () {
                var _ = chrome.runtime.lastError;
              });
            } catch (e) {}
            return pass();
          })
          .catch(function (e) { return fail(String(e && e.message || e)); });
      }
    },
    {
      name: 'chrome.commands.getAll()',
      run: function () {
        if (!has('commands.getAll')) return Promise.resolve(unavailable());
        return callAsync(function (cb) { chrome.commands.getAll(cb); })
          .then(function (cmds) { return pass((Array.isArray(cmds) ? cmds.length : '?') + ' command(s)'); })
          .catch(function (e) { return fail(String(e && e.message || e)); });
      }
    },
    {
      name: 'chrome.permissions.getAll()',
      run: function () {
        if (!has('permissions.getAll')) return Promise.resolve(unavailable());
        return callAsync(function (cb) { chrome.permissions.getAll(cb); })
          .then(function (p) {
            var nPerms = (p && p.permissions) ? p.permissions.length : 0;
            var nOrigins = (p && p.origins) ? p.origins.length : 0;
            return pass(nPerms + ' perm / ' + nOrigins + ' origin');
          })
          .catch(function (e) { return fail(String(e && e.message || e)); });
      }
    },
    {
      name: 'chrome.i18n.getUILanguage()',
      run: function () {
        if (!has('i18n.getUILanguage')) return Promise.resolve(unavailable());
        try {
          var l = chrome.i18n.getUILanguage();
          return Promise.resolve(l ? pass(l) : fail('empty'));
        } catch (e) { return Promise.resolve(fail(String(e))); }
      }
    },
    {
      name: 'chrome.i18n.getMessage("extName")',
      run: function () {
        if (!has('i18n.getMessage')) return Promise.resolve(unavailable());
        try {
          var m = chrome.i18n.getMessage('extName');
          if (m && m.indexOf('Afterbird API Probe') !== -1) return Promise.resolve(pass(m));
          return Promise.resolve(fail('got: ' + JSON.stringify(m)));
        } catch (e) { return Promise.resolve(fail(String(e))); }
      }
    },
    {
      name: 'chrome.notifications.create',
      run: function () {
        if (!has('notifications.create')) return Promise.resolve(unavailable());
        return callAsync(function (cb) {
          chrome.notifications.create('probe-note-' + Date.now(), {
            type: 'basic',
            iconUrl: 'icons/128.png',
            title: 'Probe',
            message: 'probe'
          }, cb);
        })
          .then(function (id) { return id ? pass(String(id)) : fail('no id'); })
          .catch(function (e) { return fail(String(e && e.message || e)); });
      }
    }
  ];

  // MV2-only probes.
  var mv2Probes = [
    {
      name: 'chrome.browserAction.setBadgeText',
      run: function () {
        if (!has('browserAction.setBadgeText')) return Promise.resolve(unavailable());
        try {
          chrome.browserAction.setBadgeText({ text: 'OK' });
          return Promise.resolve(pass());
        } catch (e) { return Promise.resolve(fail(String(e))); }
      }
    },
    {
      name: 'chrome.browserAction.setIcon (canvas data URL)',
      run: function () {
        if (!has('browserAction.setIcon')) return Promise.resolve(unavailable());
        try {
          // Background pages have DOM access — canvas is fine here.
          var canvas = document.createElement('canvas');
          canvas.width = 16; canvas.height = 16;
          var cx = canvas.getContext('2d');
          cx.fillStyle = '#2ea043';
          cx.fillRect(0, 0, 16, 16);
          var img = cx.getImageData(0, 0, 16, 16);
          return callAsync(function (cb) { chrome.browserAction.setIcon({ imageData: img }, cb); })
            .then(function () { return pass('imageData'); })
            .catch(function (e) { return fail(String(e && e.message || e)); });
        } catch (e) { return Promise.resolve(fail(String(e))); }
      }
    },
    {
      name: 'chrome.webRequest.onBeforeRequest.addListener',
      run: function () {
        if (!has('webRequest.onBeforeRequest')) return Promise.resolve(unavailable());
        try {
          var listener = function () { /* no-op */ };
          chrome.webRequest.onBeforeRequest.addListener(listener, { urls: ['https://example.invalid/*'] });
          try { chrome.webRequest.onBeforeRequest.removeListener(listener); } catch (e) {}
          return Promise.resolve(pass());
        } catch (e) { return Promise.resolve(fail(String(e))); }
      }
    },
    {
      name: 'chrome.scripting (expected unavailable in MV2)',
      run: function () {
        if (!has('scripting.executeScript')) return Promise.resolve(unavailable('not in MV2 by design'));
        return Promise.resolve(pass('present (unusual for MV2)'));
      }
    },
    {
      name: 'chrome.storage.session (expected unavailable in MV2)',
      run: function () {
        if (!has('storage.session')) return Promise.resolve(unavailable('MV3-only'));
        return Promise.resolve(pass('present (unusual for MV2)'));
      }
    }
  ];

  function runAll(probes) {
    var results = [];
    var i = 0;
    function next() {
      if (i >= probes.length) return Promise.resolve(results);
      var p = probes[i++];
      var start = Date.now();
      return Promise.resolve().then(function () { return p.run(); })
        .catch(function (e) { return { status: 'fail', note: 'threw: ' + (e && e.message || e) }; })
        .then(function (r) {
          r = r || { status: 'fail', note: 'no result' };
          r.name = p.name;
          r.ms = Date.now() - start;
          results.push(r);
          return next();
        });
    }
    return next();
  }

  root.AfterbirdProbes = {
    ONE_PX_PNG: ONE_PX_PNG,
    shared: sharedProbes,
    mv2: mv2Probes,
    allMv2: sharedProbes.concat(mv2Probes),
    runAll: runAll
  };
})(typeof self !== 'undefined' ? self : this);
