// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/offline_page_item.h"

#include "net/base/filename_util.h"

namespace offline_pages {

namespace {
const int kCurrentVersion = 1;
}

OfflinePageItem::OfflinePageItem()
    : version(kCurrentVersion),
      file_size(0) {
}

OfflinePageItem::OfflinePageItem(const GURL& url,
                                 const std::string& title,
                                 const base::FilePath& file_path,
                                 int64 file_size)
    : url(url),
      title(title),
      version(kCurrentVersion),
      file_path(file_path),
      file_size(file_size) {
}

OfflinePageItem::OfflinePageItem(const GURL& url,
                                 const std::string& title,
                                 const base::FilePath& file_path,
                                 int64 file_size,
                                 const base::Time& creation_time)
    : url(url),
      title(title),
      version(kCurrentVersion),
      file_path(file_path),
      file_size(file_size),
      creation_time(creation_time),
      last_access_time(creation_time) {
}

OfflinePageItem::~OfflinePageItem() {
}

GURL OfflinePageItem::GetOfflineURL() {
  return net::FilePathToFileURL(file_path);
}

}  // namespace offline_pages
