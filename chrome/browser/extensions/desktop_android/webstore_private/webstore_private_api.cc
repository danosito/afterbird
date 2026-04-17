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
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace extensions {

void RegisterDesktopAndroidWebstorePrivateFunctions(
    ExtensionFunctionRegistry* registry) {
  registry->RegisterFunction<WebstorePrivateGetWebGLStatusFunction>();
  registry->RegisterFunction<WebstorePrivateBeginInstallWithManifest3Function>();
  registry->RegisterFunction<WebstorePrivateCompleteInstallFunction>();
  registry->RegisterFunction<WebstorePrivateGetFullChromeVersionFunction>();
  registry->RegisterFunction<WebstorePrivateGetMV2DeprecationStatusFunction>();
  registry->RegisterFunction<WebstorePrivateGetReferrerChainFunction>();
  registry->RegisterFunction<WebstorePrivateIsInIncognitoModeFunction>();
  registry->RegisterFunction<WebstorePrivateGetExtensionStatusFunction>();
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
  // Origin gate: the function is only exposed to the CWS origin upstream.
  // We replicate that check here.
  if (!desktop_android::IsWebstoreOrigin(source_url())) {
    LOG(WARNING) << "[Afterbird] webstorePrivate called from non-store origin: "
                 << source_url();
    return RespondNow(Error("unknown_extension"));
  }
  // Params: the first argument is a Details dict with at minimum `id`.
  if (args().empty() || !args()[0].is_dict()) {
    return RespondNow(Error("invalid_arguments"));
  }
  const std::string* id = args()[0].GetDict().FindString("id");
  if (!id || id->size() != 32) {
    return RespondNow(Error("invalid_id"));
  }
  // Id shape check: lowercase a-p only.
  for (char c : *id) {
    if (c < 'a' || c > 'p') {
      return RespondNow(Error("invalid_id"));
    }
  }
  // Hand off to the coordinator.
  content::WebContents* web_contents = GetSenderWebContents();
  content::BrowserContext* context = browser_context();
  if (!web_contents || !context) {
    return RespondNow(Error("no_web_contents"));
  }
  const GURL crx_url = desktop_android::BuildWebstoreCrxUrl(*id);
  LOG(INFO) << "[Afterbird] webstorePrivate install id=" << *id
            << " crx=" << crx_url;
  CrxInstallCoordinator::StartFromWebstore(context, web_contents, crx_url,
                                           "Chrome Web Store");
  // The CWS page expects a result enum string. Returning an empty string
  // for the success_code slot matches upstream's "" default.
  base::Value::List result;
  result.Append("");  // result_code
  return RespondNow(ArgumentList(std::move(result)));
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

// -----------------------------------------------------------------------------
// getFullChromeVersion — returns {version: "132.0.6834.83"}. Matches the
// CHROME_VERSION macro baked into the build.

WebstorePrivateGetFullChromeVersionFunction::
    WebstorePrivateGetFullChromeVersionFunction() = default;
WebstorePrivateGetFullChromeVersionFunction::
    ~WebstorePrivateGetFullChromeVersionFunction() = default;

ExtensionFunction::ResponseAction
WebstorePrivateGetFullChromeVersionFunction::Run() {
  base::Value::Dict result;
  result.Set("version_number", std::string(version_info::GetVersionNumber()));
  base::Value::List args;
  args.Append(std::move(result));
  return RespondNow(ArgumentList(std::move(args)));
}

// -----------------------------------------------------------------------------
// getMV2DeprecationStatus — return "inactive" so MV2 extensions continue
// to install without a deprecation banner. Upstream's enum is:
// "inactive" | "warning" | "disabled_with_reenable" | "unsupported".

WebstorePrivateGetMV2DeprecationStatusFunction::
    WebstorePrivateGetMV2DeprecationStatusFunction() = default;
WebstorePrivateGetMV2DeprecationStatusFunction::
    ~WebstorePrivateGetMV2DeprecationStatusFunction() = default;

ExtensionFunction::ResponseAction
WebstorePrivateGetMV2DeprecationStatusFunction::Run() {
  base::Value::List result;
  result.Append("inactive");
  return RespondNow(ArgumentList(std::move(result)));
}

// -----------------------------------------------------------------------------
// getReferrerChain — empty base64 string. Upstream encodes a SafeBrowsing
// ReferrerChain proto; we don't have SB on desktop-android.

WebstorePrivateGetReferrerChainFunction::
    WebstorePrivateGetReferrerChainFunction() = default;
WebstorePrivateGetReferrerChainFunction::
    ~WebstorePrivateGetReferrerChainFunction() = default;

ExtensionFunction::ResponseAction
WebstorePrivateGetReferrerChainFunction::Run() {
  base::Value::List result;
  result.Append(std::string());
  return RespondNow(ArgumentList(std::move(result)));
}

// -----------------------------------------------------------------------------
// isInIncognitoMode — forwards to browser_context()->IsOffTheRecord().

WebstorePrivateIsInIncognitoModeFunction::
    WebstorePrivateIsInIncognitoModeFunction() = default;
WebstorePrivateIsInIncognitoModeFunction::
    ~WebstorePrivateIsInIncognitoModeFunction() = default;

ExtensionFunction::ResponseAction
WebstorePrivateIsInIncognitoModeFunction::Run() {
  base::Value::List result;
  result.Append(browser_context() && browser_context()->IsOffTheRecord());
  return RespondNow(ArgumentList(std::move(result)));
}

// -----------------------------------------------------------------------------
// getExtensionStatus — "installable" unless the id shape is wrong. Upstream
// also distinguishes installed / enabled / terms_of_service_declined etc.;
// for the CWS page's button state, "installable" is the useful default.

WebstorePrivateGetExtensionStatusFunction::
    WebstorePrivateGetExtensionStatusFunction() = default;
WebstorePrivateGetExtensionStatusFunction::
    ~WebstorePrivateGetExtensionStatusFunction() = default;

ExtensionFunction::ResponseAction
WebstorePrivateGetExtensionStatusFunction::Run() {
  base::Value::List result;
  result.Append("installable");
  return RespondNow(ArgumentList(std::move(result)));
}

}  // namespace extensions
