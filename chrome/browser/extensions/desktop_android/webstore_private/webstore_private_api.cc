// Copyright 2026 The Afterbird Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/desktop_android/webstore_private/webstore_private_api.h"

#include <memory>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/extensions/desktop_android/crx_install_coordinator.h"
#include "chrome/browser/extensions/desktop_android/webstore_private/webstore_url_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace extensions {

void RegisterDesktopAndroidWebstorePrivateFunctions(
    ExtensionFunctionRegistry* registry) {
  registry->RegisterFunction<WebstorePrivateGetWebGLStatusFunction>();
  registry->RegisterFunction<WebstorePrivateBeginInstallWithManifest3Function>();
  registry->RegisterFunction<WebstorePrivateCompleteInstallFunction>();
}

// -----------------------------------------------------------------------------
// getWebGLStatus — returns a fixed "webgl_allowed". Matches upstream for
// desktop platforms where WebGL is available.

WebstorePrivateGetWebGLStatusFunction::
    WebstorePrivateGetWebGLStatusFunction() = default;
WebstorePrivateGetWebGLStatusFunction::
    ~WebstorePrivateGetWebGLStatusFunction() = default;

ExtensionFunction::ResponseAction
WebstorePrivateGetWebGLStatusFunction::Run() {
  base::Value::List result;
  result.Append("webgl_allowed");
  return RespondNow(ArgumentList(std::move(result)));
}

// -----------------------------------------------------------------------------
// beginInstallWithManifest3 — not yet implemented; returns an error so the
// CWS page doesn't hang. Task 4 fills this in.

WebstorePrivateBeginInstallWithManifest3Function::
    WebstorePrivateBeginInstallWithManifest3Function() = default;
WebstorePrivateBeginInstallWithManifest3Function::
    ~WebstorePrivateBeginInstallWithManifest3Function() = default;

ExtensionFunction::ResponseAction
WebstorePrivateBeginInstallWithManifest3Function::Run() {
  return RespondNow(Error("not_implemented_yet"));
}

// -----------------------------------------------------------------------------
// completeInstall — no-op. Our install pipeline is already async-committed.

WebstorePrivateCompleteInstallFunction::
    WebstorePrivateCompleteInstallFunction() = default;
WebstorePrivateCompleteInstallFunction::
    ~WebstorePrivateCompleteInstallFunction() = default;

ExtensionFunction::ResponseAction
WebstorePrivateCompleteInstallFunction::Run() {
  return RespondNow(NoArguments());
}

}  // namespace extensions
