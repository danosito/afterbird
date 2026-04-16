// Copyright 2026 The Afterbird Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ANDROID_EXTENSION_INSTALL_BRIDGE_H_
#define CHROME_BROWSER_EXTENSIONS_ANDROID_EXTENSION_INSTALL_BRIDGE_H_

#include "base/files/file_path.h"
#include "base/functional/callback.h"

namespace content {
class WebContents;
}

namespace extensions::android {

// Opens an Android file picker anchored on `web_contents` and invokes `cb`
// on the UI thread with either the absolute path of the picked file
// (streamed into the app's cache dir) or an empty path if the user cancelled
// or the picker failed to launch.
//
// Must be called from the UI thread. The callback runs on the UI thread.
//
// Takes ownership of the callback via an opaque integer handle passed to
// Java so a single Natives interface can be shared across concurrent picks.
using PickFileCallback = base::OnceCallback<void(const base::FilePath& path)>;
void ShowFilePicker(content::WebContents* web_contents,
                    PickFileCallback cb);

}  // namespace extensions::android

#endif  // CHROME_BROWSER_EXTENSIONS_ANDROID_EXTENSION_INSTALL_BRIDGE_H_
