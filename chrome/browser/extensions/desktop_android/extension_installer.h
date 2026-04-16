// Copyright 2026 The Afterbird Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DesktopAndroidExtensionInstaller unpacks an extension payload (.zip, .crx, or an already-
// unpacked directory) to `<profile>/Extensions/<id>/<version>/` and hands the
// resulting Extension back to DesktopAndroidExtensionSystem::AddExtension.
//
// This is the bottom half of all UI-driven install flows on desktop-android
// (developerPrivate.loadUnpacked, Chrome Web Store URL interceptor,
// drag-and-drop onto chrome://extensions). It is NOT a substitute for
// //chrome/browser/extensions/crx_installer.h on desktop — that one does CRX3
// signature verification, policy checks, ExtensionService plumbing etc., none
// of which are compiled here. We accept the file the user picked at face
// value, same as Kiwi Browser's "+ from .zip/.crx/.user.js" flow.
//
// Usage (on the UI thread):
//
//   auto* installer = DesktopAndroidExtensionInstaller::Get(browser_context);
//   installer->InstallFromFile(
//       file_path,
//       base::BindOnce([](scoped_refptr<const Extension> ext,
//                         const std::string& err) {
//         if (ext) { /* success */ }
//       }));
//
// The callback runs on the UI thread. Zip extraction + manifest reads run on
// a blocking thread pool worker.

#ifndef CHROME_BROWSER_EXTENSIONS_DESKTOP_ANDROID_EXTENSION_INSTALLER_H_
#define CHROME_BROWSER_EXTENSIONS_DESKTOP_ANDROID_EXTENSION_INSTALLER_H_

#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class Extension;

class DesktopAndroidExtensionInstaller {
 public:
  using Callback =
      base::OnceCallback<void(scoped_refptr<const Extension> extension,
                              const std::string& error)>;

  explicit DesktopAndroidExtensionInstaller(content::BrowserContext* browser_context);
  DesktopAndroidExtensionInstaller(const DesktopAndroidExtensionInstaller&) = delete;
  DesktopAndroidExtensionInstaller& operator=(const DesktopAndroidExtensionInstaller&) = delete;
  ~DesktopAndroidExtensionInstaller();

  // `file_path` may be:
  //   * an extension directory (with manifest.json in root)
  //   * a .zip  (extracted into the install dir)
  //   * a .crx  (CRX3 header stripped, zip body extracted)
  // Detection is by extension AND magic bytes. The file is NOT moved or
  // deleted — callers own it.
  void InstallFromFile(const base::FilePath& file_path, Callback cb);

 private:
  struct UnpackedResult;

  static UnpackedResult UnpackOnBlockingThread(const base::FilePath& src,
                                               const base::FilePath& dest_root);

  void OnUnpacked(Callback cb, UnpackedResult result);

  raw_ptr<content::BrowserContext> browser_context_;

  base::WeakPtrFactory<DesktopAndroidExtensionInstaller> weak_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_DESKTOP_ANDROID_EXTENSION_INSTALLER_H_
