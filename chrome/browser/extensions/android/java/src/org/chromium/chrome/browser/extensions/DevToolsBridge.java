// Copyright 2026 The Afterbird Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.extensions;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.WebContents;

/**
 * Bridge for the Kiwi-style "Developer Tools" main-menu entry. On tap,
 * {@link #open} lazily starts a DevTools HTTP server on 127.0.0.1 and
 * returns a URL pointing at the bundled inspector frontend. The caller
 * should open that URL in a new tab.
 *
 * <p>Note on packaging: this class lives in the extensions/android module
 * (rather than a dedicated devtools/android module) to reuse the existing
 * JNI generate_jni / source_set wiring. The functionality itself has no
 * dependency on the extension system — it only depends on content's
 * DevToolsAgentHost APIs. Splitting it out would add BUILD.gn boilerplate
 * without clear benefit.
 */
@JNINamespace("extensions")
public class DevToolsBridge {
    /**
     * Ensures the DevTools HTTP server is running and returns a URL that,
     * when loaded in a tab, renders the DevTools frontend targeting the
     * given WebContents.
     *
     * @return a fully-formed http://127.0.0.1:PORT/... URL, or the empty
     *     string if the server failed to start.
     */
    public static String open(Profile profile, WebContents webContents) {
        if (profile == null || webContents == null) return "";
        return DevToolsBridgeJni.get().open(profile, webContents);
    }

    @NativeMethods
    public interface Natives {
        @JniType("std::string")
        String open(
                @JniType("Profile*") Profile profile,
                @JniType("content::WebContents*") WebContents webContents);
    }
}
