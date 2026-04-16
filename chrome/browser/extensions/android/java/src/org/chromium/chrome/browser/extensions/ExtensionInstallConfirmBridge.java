// Copyright 2026 The Afterbird Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.extensions;

import android.content.Context;
import android.content.res.Resources;
import android.text.TextUtils;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Java side of the extension install confirmation dialog. C++ calls
 * {@link #showConfirmDialog} with the metadata extracted from a
 * just-unpacked extension, we show a modal dialog anchored on the
 * WebContents' ModalDialogManager, and report back via JNI exactly once
 * with "confirmed" (true) or "cancelled" (false).
 *
 * This mirrors the {@link ExtensionInstallBridge} picker pattern: native
 * owns a single-shot bridge object, hands its pointer over as a jlong, and
 * waits for exactly one callback.
 */
@JNINamespace("extensions")
public class ExtensionInstallConfirmBridge {
    private static final String TAG = "AfterbirdExtConfirm";

    // v1.2: these strings live in Java as constants to avoid spinning up an
    // android_resources target just for a confirm dialog. When the Afterbird
    // string grd lands, lift these into IDS_AFTERBIRD_* IDs.
    private static final String FINE_PRINT =
            "Extensions can read and change everything you see on the web, "
                    + "including passwords and personal data. Only install "
                    + "extensions from sources you trust.";
    private static final String TITLE_FORMAT = "Install \"%s\"?";
    private static final String POSITIVE_BUTTON = "Install";
    private static final String NEGATIVE_BUTTON = "Cancel";

    private ExtensionInstallConfirmBridge() {}

    @CalledByNative
    public static void showConfirmDialog(
            long nativeExtensionInstallConfirmCallback,
            WebContents webContents,
            String name,
            String version,
            String permissionsJoined,
            String sourceLabel) {
        if (webContents == null) {
            Log.w(TAG, "no WebContents, cancelling confirm");
            reportDecision(nativeExtensionInstallConfirmCallback, false);
            return;
        }
        WindowAndroid window = webContents.getTopLevelNativeWindow();
        if (window == null) {
            Log.w(TAG, "no WindowAndroid, cancelling confirm");
            reportDecision(nativeExtensionInstallConfirmCallback, false);
            return;
        }
        ModalDialogManager manager = window.getModalDialogManager();
        if (manager == null) {
            Log.w(TAG, "no ModalDialogManager, cancelling confirm");
            reportDecision(nativeExtensionInstallConfirmCallback, false);
            return;
        }

        Context context = ContextUtils.getApplicationContext();
        Resources resources = context.getResources();

        String safeName = TextUtils.isEmpty(name) ? "Extension" : name;
        String title = String.format(TITLE_FORMAT, safeName);

        StringBuilder body = new StringBuilder();
        if (!TextUtils.isEmpty(version)) {
            body.append("Version: ").append(version).append('\n');
        }
        if (!TextUtils.isEmpty(sourceLabel)) {
            body.append("Source: ").append(sourceLabel).append('\n');
        }
        if (!TextUtils.isEmpty(permissionsJoined)) {
            body.append("Permissions: ").append(permissionsJoined).append('\n');
        }
        if (body.length() > 0) {
            body.append('\n');
        }
        body.append(FINE_PRINT);

        final long nativePtr = nativeExtensionInstallConfirmCallback;
        final boolean[] reported = new boolean[] {false};

        ModalDialogProperties.Controller controller =
                new ModalDialogProperties.Controller() {
                    @Override
                    public void onClick(PropertyModel model, int buttonType) {
                        if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
                            manager.dismissDialog(
                                    model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                        } else if (buttonType == ModalDialogProperties.ButtonType.NEGATIVE) {
                            manager.dismissDialog(
                                    model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                        }
                    }

                    @Override
                    public void onDismiss(PropertyModel model, int dismissalCause) {
                        if (reported[0]) return;
                        reported[0] = true;
                        boolean confirmed =
                                dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED;
                        reportDecision(nativePtr, confirmed);
                    }
                };

        PropertyModel model =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, controller)
                        .with(ModalDialogProperties.TITLE, title)
                        .with(ModalDialogProperties.MESSAGE_PARAGRAPH_1, body.toString())
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                POSITIVE_BUTTON)
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                NEGATIVE_BUTTON)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, false)
                        .build();

        manager.showDialog(model, ModalDialogManager.ModalDialogType.APP);
    }

    private static void reportDecision(long nativePtr, boolean confirmed) {
        ExtensionInstallConfirmBridgeJni.get().onConfirmDecision(nativePtr, confirmed);
    }

    @NativeMethods
    public interface Natives {
        void onConfirmDecision(long nativeExtensionInstallConfirmCallback, boolean confirmed);
    }
}
