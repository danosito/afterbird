// Copyright 2026 The Afterbird Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ANDROID_EXTENSION_INSTALL_BRIDGE_H_
#define CHROME_BROWSER_EXTENSIONS_ANDROID_EXTENSION_INSTALL_BRIDGE_H_

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "third_party/jni_zero/jni_zero.h"

namespace content {
class WebContents;
}

namespace extensions {

// Single-shot bridge around the Java ExtensionInstallBridge picker.
//
// `ShowFilePicker` creates one of these on the heap, hands its `this`
// pointer to Java as a jlong, and self-deletes in OnFilePicked after
// invoking the stored callback. Java calls back exactly once per show
// (either with a path string or an empty string on cancel).
class ExtensionInstallCallback {
 public:
  using PickFileCallback =
      base::OnceCallback<void(const base::FilePath& path)>;

  // Opens the Android file picker anchored on `web_contents` and invokes
  // `cb` on the UI thread with the picked file's cached path (or empty
  // on cancel). The ExtensionInstallCallback instance is owned by the
  // Java call and self-destructs in OnFilePicked.
  static void Show(content::WebContents* web_contents, PickFileCallback cb);

  // Called from JNI once Java has finished picking (or failing).
  void OnFilePicked(JNIEnv* env,
                    const jni_zero::JavaParamRef<jstring>& path);

 private:
  explicit ExtensionInstallCallback(PickFileCallback cb);
  ~ExtensionInstallCallback();

  PickFileCallback cb_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_ANDROID_EXTENSION_INSTALL_BRIDGE_H_
