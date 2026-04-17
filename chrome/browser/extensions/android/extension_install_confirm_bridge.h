// Copyright 2026 The Afterbird Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ANDROID_EXTENSION_INSTALL_CONFIRM_BRIDGE_H_
#define CHROME_BROWSER_EXTENSIONS_ANDROID_EXTENSION_INSTALL_CONFIRM_BRIDGE_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "third_party/jni_zero/jni_zero.h"

namespace content {
class WebContents;
}

namespace extensions {

// Single-shot bridge around the Java ExtensionInstallConfirmBridge dialog.
//
// `Show` creates one of these on the heap, hands its `this` pointer to Java
// as a jlong, and self-deletes in OnConfirmDecision after invoking the
// stored callback. Java calls back exactly once per show (either with
// confirmed=true for Install or confirmed=false for Cancel/dismiss).
//
// Mirrors the self-owned pattern of ExtensionInstallCallback.
class ExtensionInstallConfirmCallback {
 public:
  // `confirmed` is true iff the user tapped Install. false for Cancel,
  // back-press, lost WebContents, or any other dismissal.
  using ConfirmCallback = base::OnceCallback<void(bool confirmed)>;

  // Opens the install-confirm dialog anchored on `web_contents` and invokes
  // `cb` on the UI thread once with the user's decision.
  //
  //   `name`              — extension display name (for "Install \"%s\"?").
  //   `version`           — manifest.version, raw string.
  //   `permissions`       — joined, user-facing list of permission strings
  //                         (e.g. "tabs, storage, <all_urls>"). Empty is OK.
  //   `source_label`      — "Chrome Web Store" or URL host; shown as "Source:".
  //
  // If `web_contents` is null or the ModalDialogManager is unavailable, `cb`
  // fires synchronously-ish with `confirmed=false`.
  static void Show(content::WebContents* web_contents,
                   const std::string& name,
                   const std::string& version,
                   const std::string& permissions,
                   const std::string& source_label,
                   ConfirmCallback cb);

  // Called from JNI once Java has finished the dialog.
  void OnConfirmDecision(JNIEnv* env, jboolean confirmed);

 private:
  explicit ExtensionInstallConfirmCallback(ConfirmCallback cb);
  ~ExtensionInstallConfirmCallback();

  ConfirmCallback cb_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_ANDROID_EXTENSION_INSTALL_CONFIRM_BRIDGE_H_
