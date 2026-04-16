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
 * Bridge between Java menu code and C++ extension registry to enumerate
 * running extensions for the main app menu.
 *
 * <p>Ported from Kiwi Browser's AppMenuBridge. Returns extension info as a
 * single delimited string to minimize JNI chatter:
 *
 * <pre>
 *   extension1\u001fextension2\u001f...
 * </pre>
 *
 * where each extension is:
 *
 * <pre>
 *   name\u001eid\u001epopupUrl\u001ebase64Icon\u001eactiveState
 * </pre>
 */
@JNINamespace("extensions")
public class AppMenuBridge {
    /** Field separator (FS, \u001e) used between fields of a single extension. */
    public static final String FIELD_SEPARATOR = "\u001e";

    /** Record separator (FS, \u001f) used between extensions. */
    public static final String RECORD_SEPARATOR = "\u001f";

    /**
     * Returns running extensions for the given profile, or an empty string if
     * there are none (or the extension system is not yet ready).
     */
    public static String getRunningExtensions(Profile profile, WebContents webContents) {
        if (profile == null) return "";
        return AppMenuBridgeJni.get().getRunningExtensions(profile, webContents);
    }

    /**
     * Invokes the extension's browser action (equivalent to clicking its
     * toolbar icon on desktop).
     */
    public static void callExtension(Profile profile, WebContents webContents, String extensionId) {
        if (profile == null || extensionId == null || extensionId.isEmpty()) return;
        AppMenuBridgeJni.get().callExtension(profile, webContents, extensionId);
    }

    /**
     * Grants the extension the equivalent of "activeTab" permission for the
     * current web contents. Used before invoking the extension's action so it
     * can interact with the page without persistent host permissions.
     */
    public static void grantExtensionActiveTab(
            Profile profile, WebContents webContents, String extensionId) {
        if (profile == null || extensionId == null || extensionId.isEmpty()) return;
        AppMenuBridgeJni.get().grantExtensionActiveTab(profile, webContents, extensionId);
    }

    @NativeMethods
    public interface Natives {
        @JniType("std::string")
        String getRunningExtensions(
                @JniType("Profile*") Profile profile,
                @JniType("content::WebContents*") WebContents webContents);

        void callExtension(
                @JniType("Profile*") Profile profile,
                @JniType("content::WebContents*") WebContents webContents,
                @JniType("std::string") String extensionId);

        void grantExtensionActiveTab(
                @JniType("Profile*") Profile profile,
                @JniType("content::WebContents*") WebContents webContents,
                @JniType("std::string") String extensionId);
    }
}
