// Copyright 2026 The Afterbird Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.extensions;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.Intent;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.task.AsyncTask;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.UUID;

/**
 * Java side of the extension file-install picker. C++ calls
 * {@link #showFilePicker(long, WebContents)} when the user clicks
 * "Load unpacked" in chrome://extensions. We launch a document picker
 * via the WebContents' WindowAndroid, stream the picked content:// URI
 * into a temp file under the app cache directory, and report the
 * resulting path (or the empty string on cancel) back to C++.
 */
@JNINamespace("extensions")
public class ExtensionInstallBridge {
    private static final String TAG = "AfterbirdExtInstall";

    private ExtensionInstallBridge() {}

    @CalledByNative
    public static void showFilePicker(long nativeCallback, WebContents webContents) {
        WindowAndroid window =
                webContents != null ? webContents.getTopLevelNativeWindow() : null;
        if (window == null) {
            Log.w(TAG, "no WindowAndroid, cannot show picker");
            ExtensionInstallBridgeJni.get().onFilePicked(nativeCallback, "");
            return;
        }
        Activity activity = window.getActivity().get();
        if (activity == null) {
            Log.w(TAG, "no Activity, cannot show picker");
            ExtensionInstallBridgeJni.get().onFilePicked(nativeCallback, "");
            return;
        }
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        // Document providers don't have a reliable mime type for .crx, so
        // accept anything openable and filter in native code.
        intent.setType("*/*");
        intent.putExtra(Intent.EXTRA_MIME_TYPES,
                new String[] {"application/zip", "application/x-chrome-extension",
                        "application/octet-stream", "*/*"});

        WindowAndroid.IntentCallback cb = new WindowAndroid.IntentCallback() {
            @Override
            public void onIntentCompleted(int resultCode, Intent data) {
                if (resultCode != Activity.RESULT_OK || data == null
                        || data.getData() == null) {
                    ExtensionInstallBridgeJni.get().onFilePicked(nativeCallback, "");
                    return;
                }
                final Uri uri = data.getData();
                new AsyncTask<String>() {
                    @Override
                    protected String doInBackground() {
                        return copyUriToCache(uri);
                    }

                    @Override
                    protected void onPostExecute(String path) {
                        ExtensionInstallBridgeJni.get().onFilePicked(
                                nativeCallback, path == null ? "" : path);
                    }
                }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
            }
        };

        // Pass null errorId so we don't depend on a specific chrome string
        // resource — if the intent fails WindowAndroid surfaces its own
        // toast. showIntent returns true iff the intent was actually
        // launched; false means no Activity could handle it.
        boolean launched = window.showIntent(intent, cb, null);
        if (!launched) {
            Log.w(TAG, "showIntent returned false — no Activity for picker");
            ExtensionInstallBridgeJni.get().onFilePicked(nativeCallback, "");
        }
    }

    /**
     * Streams `uri` into a file under the app cache. Returns the absolute
     * path, or null on failure.
     */
    private static String copyUriToCache(Uri uri) {
        try {
            ContentResolver resolver =
                    ContextUtils.getApplicationContext().getContentResolver();
            File parent = new File(
                    ContextUtils.getApplicationContext().getCacheDir(),
                    "afterbird_picker");
            if (!parent.exists() && !parent.mkdirs()) {
                return null;
            }
            String name = deriveFileName(uri);
            File out = new File(parent, UUID.randomUUID() + "_" + name);
            try (InputStream in = resolver.openInputStream(uri);
                 OutputStream outStream = new FileOutputStream(out)) {
                if (in == null) return null;
                byte[] buf = new byte[64 * 1024];
                int read;
                while ((read = in.read(buf)) > 0) {
                    outStream.write(buf, 0, read);
                }
            }
            return out.getAbsolutePath();
        } catch (Exception e) {
            Log.w(TAG, "copyUriToCache failed", e);
            return null;
        }
    }

    private static String deriveFileName(Uri uri) {
        String last = uri.getLastPathSegment();
        if (last != null && !last.isEmpty()) {
            int slash = last.lastIndexOf('/');
            if (slash >= 0 && slash + 1 < last.length()) {
                last = last.substring(slash + 1);
            }
            // Only keep a .zip/.crx suffix if present, else fall back to
            // .dat so the installer can still detect via magic bytes.
            String lower = last.toLowerCase();
            if (lower.endsWith(".zip") || lower.endsWith(".crx")
                    || lower.endsWith(".user.js")) {
                return last;
            }
        }
        return "picked.dat";
    }

    @NativeMethods
    public interface Natives {
        void onFilePicked(long nativeCallback, String pathOrEmpty);
    }
}
