// Copyright 2026 The Afterbird Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CrxInstallCoordinator runs the v1.2 extension install pipeline:
//
//     (download or webstore-URL trigger)
//           │
//           ▼
//     FETCHING  ─► SimpleURLLoader::DownloadToFile into the cache dir
//           │
//           ▼
//     UNPACKING ─► DesktopAndroidExtensionInstaller::PrepareFromFile
//           │
//           ▼
//     AWAIT_CONFIRM ─► ExtensionInstallConfirmCallback::Show
//           │
//           ▼
//     INSTALLING ─► DesktopAndroidExtensionInstaller::CommitPrepared
//           │
//           ▼
//     done (success or failure; self-delete either way)
//
// Each coordinator is self-owned: construct on the heap, call Start*(), and
// forget. The coordinator `delete this`es at every terminal transition.
//
// Two entry points, both taking a WebContents for dialog anchoring:
//   * StartFromDownload(url, web_contents)
//       — called from the download interceptor when a response we
//         recognise (MIME or .crx suffix) is about to hit Downloads/.
//   * StartFromWebstore(crx_url, web_contents, source_label)
//       — called from the webstorePrivate.beginInstallWithManifest3
//         shim when the CWS detail page's Install button is pressed.
//         The shim synthesises the "/service/update2/crx" endpoint
//         from the extension id it receives.

#ifndef CHROME_BROWSER_EXTENSIONS_DESKTOP_ANDROID_CRX_INSTALL_COORDINATOR_H_
#define CHROME_BROWSER_EXTENSIONS_DESKTOP_ANDROID_CRX_INSTALL_COORDINATOR_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/desktop_android/extension_installer.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace extensions {

class Extension;

class CrxInstallCoordinator : public content::WebContentsObserver {
 public:
  // Entry point for the download-interceptor path. Takes ownership of the
  // install pipeline: creates its own self-owned instance, kicks off the
  // SimpleURLLoader fetch, and deletes itself on terminal transitions.
  //
  // The source label shown in the confirm dialog is the URL's host.
  static void StartFromDownload(content::BrowserContext* browser_context,
                                content::WebContents* web_contents,
                                const GURL& url);

  // Entry point for the webstore-URL-throttle path. `source_label` is
  // typically "Chrome Web Store" (the throttle synthesises a
  // clients2.google.com URL whose host would otherwise be shown).
  static void StartFromWebstore(content::BrowserContext* browser_context,
                                content::WebContents* web_contents,
                                const GURL& crx_url,
                                const std::string& source_label);

  CrxInstallCoordinator(const CrxInstallCoordinator&) = delete;
  CrxInstallCoordinator& operator=(const CrxInstallCoordinator&) = delete;

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;

 private:
  // `web_contents` may be null (some download contexts). It is only used
  // for dialog anchoring — install can proceed without it if the user
  // already confirmed.
  CrxInstallCoordinator(content::BrowserContext* browser_context,
                        content::WebContents* web_contents,
                        const GURL& url,
                        const std::string& source_label);
  ~CrxInstallCoordinator() override;

  // S1 → fetch.
  void Start();

  // S1 → S2: the SimpleURLLoader has written `path` (or empty on failure).
  void OnDownloaded(base::FilePath path);

  // S2 → S3: the installer has finished unpacking into staging; we now
  // have extension metadata suitable for the dialog.
  void OnPrepared(std::unique_ptr<DesktopAndroidExtensionInstaller::PreparedInstall>
                      prepared);

  // S3 → S4 or cancel: user decision from the modal dialog.
  void OnConfirmDecision(bool confirmed);

  // S4: final AddExtension(). `was_already_installed` captures the
  // pre-commit registry state (for the "Replaced" vs "Installed" toast).
  void OnCommitted(bool was_already_installed,
                   scoped_refptr<const Extension> extension,
                   const std::string& error);

  // Terminal: log + toast + delete staging + self-delete.
  void FailWith(const std::string& reason);
  // Terminal: silent cancellation (dialog gone, webcontents gone); delete
  // staging without a toast.
  void CancelSilently(const char* reason);
  // Success terminal: toast + self-delete.
  void SucceedWith(scoped_refptr<const Extension> extension, bool replaced);

  // Best-effort toast posted to the current Activity.
  void ShowToast(const std::string& message);

  // Is the given extension id already in the registry? Used to switch the
  // success message between "Installed" and "Replaced".
  bool IsAlreadyInstalled(const std::string& extension_id) const;

  base::WeakPtr<content::BrowserContext> browser_context_;
  GURL url_;
  std::string source_label_;
  base::FilePath fetched_crx_path_;

  std::unique_ptr<network::SimpleURLLoader> loader_;
  std::unique_ptr<DesktopAndroidExtensionInstaller> installer_;
  std::unique_ptr<DesktopAndroidExtensionInstaller::PreparedInstall> prepared_;

  base::WeakPtrFactory<CrxInstallCoordinator> weak_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_DESKTOP_ANDROID_CRX_INSTALL_COORDINATOR_H_
