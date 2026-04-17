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
//   // Single-shot (picker flow; auto-confirmed by virtue of user tapping
//   // Install in the file picker):
//   auto installer = std::make_unique<DesktopAndroidExtensionInstaller>(ctx);
//   installer->InstallFromFile(path, cb);
//
//   // Split flow (download / webstore-URL flow, gated by confirm dialog):
//   installer->PrepareFromFile(path, base::BindOnce(
//       [](std::unique_ptr<DesktopAndroidExtensionInstaller::PreparedInstall>
//              prepared) {
//         // Inspect prepared->extension to build the confirm dialog.
//         // On yes:  installer->CommitPrepared(std::move(prepared), cb);
//         // On no:   installer->DiscardPrepared(std::move(prepared));
//       }));
//
// The callbacks run on the UI thread. Zip extraction + manifest reads run on
// a blocking thread pool worker.

#ifndef CHROME_BROWSER_EXTENSIONS_DESKTOP_ANDROID_EXTENSION_INSTALLER_H_
#define CHROME_BROWSER_EXTENSIONS_DESKTOP_ANDROID_EXTENSION_INSTALLER_H_

#include <memory>
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

  // Result of `PrepareFromFile`. Owns the staging directory path so that a
  // non-`CommitPrepared` path (user cancels) can delete it. `extension` is
  // the loaded-but-not-yet-registered extension, ready to be shown in a
  // confirm dialog. `error` is non-empty iff preparation failed (in which
  // case `extension` is null and `staging_root` is empty).
  struct PreparedInstall {
    PreparedInstall();
    PreparedInstall(PreparedInstall&&);
    PreparedInstall& operator=(PreparedInstall&&);
    PreparedInstall(const PreparedInstall&) = delete;
    PreparedInstall& operator=(const PreparedInstall&) = delete;
    ~PreparedInstall();

    scoped_refptr<const Extension> extension;
    base::FilePath staging_root;
    std::string error;
  };

  using PrepareCallback =
      base::OnceCallback<void(std::unique_ptr<PreparedInstall> prepared)>;

  explicit DesktopAndroidExtensionInstaller(content::BrowserContext* browser_context);
  DesktopAndroidExtensionInstaller(const DesktopAndroidExtensionInstaller&) = delete;
  DesktopAndroidExtensionInstaller& operator=(const DesktopAndroidExtensionInstaller&) = delete;
  ~DesktopAndroidExtensionInstaller();

  // Legacy convenience wrapper for the `loadUnpacked`-from-picker flow.
  // Equivalent to `PrepareFromFile` + `CommitPrepared` with no gap for a
  // confirmation dialog.
  //
  // `file_path` may be:
  //   * an extension directory (with manifest.json in root)
  //   * a .zip  (extracted into the install dir)
  //   * a .crx  (CRX3 header stripped, zip body extracted)
  // Detection is by extension AND magic bytes. The file is NOT moved or
  // deleted — callers own it.
  void InstallFromFile(const base::FilePath& file_path, Callback cb);

  // Unpacks `file_path` into a staging directory and loads the manifest, but
  // does NOT register the extension. The returned `PreparedInstall` contains
  // enough to build a confirm dialog (`extension->name()`, version,
  // permissions) and to finish the install later via `CommitPrepared`.
  //
  // Callbacks fire on the UI thread.
  void PrepareFromFile(const base::FilePath& file_path, PrepareCallback cb);

  // Promotes a `PreparedInstall` to a real install: registers the extension
  // via `DesktopAndroidExtensionSystem::AddExtension`. On failure the
  // staging directory is deleted.
  void CommitPrepared(std::unique_ptr<PreparedInstall> prepared, Callback cb);

  // Discards a prepared install without registering it. Deletes the staging
  // directory on the blocking pool. Safe to call with a null prepared.
  void DiscardPrepared(std::unique_ptr<PreparedInstall> prepared);

 private:
  struct UnpackedResult;

  static UnpackedResult UnpackOnBlockingThread(const base::FilePath& src,
                                               const base::FilePath& dest_root);

  // Completion handler for a `PrepareFromFile` call.
  void OnUnpackedForPrepare(PrepareCallback cb, UnpackedResult result);

  // Completion handler for the legacy one-shot `InstallFromFile` path.
  void OnUnpacked(Callback cb, UnpackedResult result);

  // Common promotion step shared by `InstallFromFile` and `CommitPrepared`.
  void FinishInstall(scoped_refptr<const Extension> extension,
                     const base::FilePath& unpacked_root,
                     Callback cb);

  raw_ptr<content::BrowserContext> browser_context_;

  base::WeakPtrFactory<DesktopAndroidExtensionInstaller> weak_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_DESKTOP_ANDROID_EXTENSION_INSTALLER_H_
