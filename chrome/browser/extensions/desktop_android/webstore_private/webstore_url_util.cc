// Copyright 2026 The Afterbird Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/desktop_android/webstore_private/webstore_url_util.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "url/gurl.h"

namespace extensions::desktop_android {

std::string ExtractWebstoreExtensionId(const GURL& url) {
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return std::string();
  }
  const std::string host = url.host();
  const std::string path = url.path();
  std::string id_candidate;
  if (host == "chromewebstore.google.com") {
    std::vector<std::string_view> parts = base::SplitStringPiece(
        path, "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (parts.size() >= 2 && parts[0] == "detail") {
      id_candidate = std::string(parts.back());
    }
  } else if (host == "chrome.google.com" &&
             base::StartsWith(path, "/webstore/detail/",
                              base::CompareCase::SENSITIVE)) {
    std::vector<std::string_view> parts = base::SplitStringPiece(
        path, "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (parts.size() >= 3) {
      id_candidate = std::string(parts.back());
    }
  }
  if (id_candidate.size() != 32) {
    return std::string();
  }
  for (char c : id_candidate) {
    if (c < 'a' || c > 'p') {
      return std::string();
    }
  }
  return id_candidate;
}

GURL BuildWebstoreCrxUrl(const std::string& extension_id) {
  return GURL(
      "https://clients2.google.com/service/update2/crx?response=redirect"
      "&prodversion=128.0&acceptformat=crx2,crx3&x=id%3D" +
      extension_id + "%26installsource%3Dondemand%26uc");
}

bool IsWebstoreOrigin(const GURL& url) {
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return false;
  }
  const std::string host = url.host();
  return host == "chromewebstore.google.com" || host == "chrome.google.com";
}

}  // namespace extensions::desktop_android
