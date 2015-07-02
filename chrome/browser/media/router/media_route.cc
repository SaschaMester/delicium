// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_route.h"

#include "base/logging.h"
#include "chrome/browser/media/router/media_source.h"

namespace {
const char kRouteUrnPrefix[] = "urn:x-org.chromium:media:route:";
}

namespace media_router {

MediaRoute::MediaRoute(const MediaRoute::Id& media_route_id,
                       const MediaSource& media_source,
                       const MediaSink& media_sink,
                       const std::string& description,
                       bool is_local)
    : media_route_id_(media_route_id),
      media_source_(media_source),
      media_sink_(media_sink),
      description_(description),
      is_local_(is_local),
      state_(MEDIA_ROUTE_STATE_NEW) {
}

MediaRoute::~MediaRoute() {
}

bool MediaRoute::Equals(const MediaRoute& other) const {
  return media_route_id_ == other.media_route_id_;
}

// <route-id> =
//  urn:x-org.chromium:media:route:<presentation-id>/<sink>/<source>
// <source> = <url>|<capture-source>
// <sink> = <provider-name>-<sink-id>
std::pair<std::string, std::string> GetPresentationIdAndUrl(
    const MediaRoute::Id& id) {
  if (id.find(kRouteUrnPrefix) != 0) {
    LOG(ERROR) << "Invalid media route ID. Expecting prefix "
               << kRouteUrnPrefix;
    return std::make_pair(std::string(), std::string());
  }
  size_t prefix_len = arraysize(kRouteUrnPrefix) - 1;
  size_t first_delim = id.find("/");
  if (first_delim == std::string::npos || first_delim == prefix_len) {
    LOG(ERROR) << "Invalid media route ID. Expecting presentation ID.";
    return std::make_pair(std::string(), std::string());
  }

  std::string presentation_id = id.substr(prefix_len, first_delim - prefix_len);
  size_t second_delim = id.find("/", first_delim + 1);
  if (second_delim == std::string::npos || second_delim == first_delim + 1) {
    LOG(ERROR) << "Invalid media route ID. Expecting sink.";
    return std::make_pair(std::string(), std::string());
  }

  if (second_delim == id.size() - 1) {
    LOG(ERROR) << "Invalid media route ID. Expecting source.";
    return std::make_pair(std::string(), std::string());
  }
  std::string source = id.substr(second_delim + 1);
  return std::make_pair(presentation_id, source);
}

}  // namespace media_router
