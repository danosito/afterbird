// Copyright 2026 The Afterbird Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ANDROID_DEVTOOLS_BRIDGE_H_
#define CHROME_BROWSER_EXTENSIONS_ANDROID_DEVTOOLS_BRIDGE_H_

#include <cstdint>
#include <string>

namespace content {
class WebContents;
}  // namespace content

namespace extensions {

// Helper for the Kiwi-style "Developer Tools" main-menu entry.
//
// Manages a single lazily-started DevTools HTTP server bound to
// 127.0.0.1 on a kernel-assigned port. The server is created on the
// first call to EnsureServerStarted() and kept alive for the life of
// the browser process (user guidance: "if the 'open DevTools' button
// hasn't been tapped, there's no reason to keep the socket open;
// once started, keep it running").
//
// The returned URL points at the bundled inspector frontend, which
// is served by the DevTools HTTP handler from embedded resources
// when DevToolsManagerDelegate reports HasBundledFrontendResources()
// == true (Android builds have bundled resources by default).
class DevToolsBridge {
 public:
  // Lazily starts the DevTools HTTP server on 127.0.0.1:0. Returns the
  // kernel-assigned port, or 0 if starting failed (or a previous start
  // had already failed and we're in a bad state).
  //
  // Safe to call multiple times; only the first call does work.
  static uint16_t EnsureServerStarted();

  // Builds a DevTools front-end URL for the given WebContents, starting
  // the HTTP server if needed. Returns the empty string on failure
  // (null web_contents, no agent host, server failed to start).
  //
  // The returned URL has the shape:
  //   http://127.0.0.1:<port>/devtools/inspector.html
  //     ?ws=127.0.0.1:<port>/devtools/page/<agent-host-id>
  //
  // Callers should open this URL in a new tab.
  static std::string BuildFrontendUrlFor(content::WebContents* web_contents);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_ANDROID_DEVTOOLS_BRIDGE_H_
