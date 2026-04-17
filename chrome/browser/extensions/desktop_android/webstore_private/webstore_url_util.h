// Copyright 2026 The Afterbird Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_DESKTOP_ANDROID_WEBSTORE_PRIVATE_WEBSTORE_URL_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_DESKTOP_ANDROID_WEBSTORE_PRIVATE_WEBSTORE_URL_UTIL_H_

#include <string>

class GURL;

namespace extensions::desktop_android {

// Returns the 32-character [a-p] extension id parsed from a Chrome Web Store
// detail URL, or the empty string if `url` is not a recognised CWS detail
// URL. Accepts both the current host (chromewebstore.google.com) and the
// legacy host (chrome.google.com/webstore).
std::string ExtractWebstoreExtensionId(const GURL& url);

// Builds the clients2.google.com update endpoint URL that returns a CRX
// redirect for the given extension id.
GURL BuildWebstoreCrxUrl(const std::string& extension_id);

// Returns true iff `url` is on the current or legacy Chrome Web Store origin.
// Used by the webstorePrivate shim to gate the API to the CWS origin.
bool IsWebstoreOrigin(const GURL& url);

}  // namespace extensions::desktop_android

#endif  // CHROME_BROWSER_EXTENSIONS_DESKTOP_ANDROID_WEBSTORE_PRIVATE_WEBSTORE_URL_UTIL_H_
