// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/intercept_download_resource_throttle.h"

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "content/public/browser/android/download_controller_android.h"
#include "content/public/browser/resource_controller.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"

namespace chrome {

InterceptDownloadResourceThrottle::InterceptDownloadResourceThrottle(
    net::URLRequest* request,
    int render_process_id,
    int render_view_id,
    int request_id)
    : request_(request),
      render_process_id_(render_process_id),
      render_view_id_(render_view_id),
      request_id_(request_id) {
}

InterceptDownloadResourceThrottle::~InterceptDownloadResourceThrottle() {
}

void InterceptDownloadResourceThrottle::WillProcessResponse(bool* defer) {
  ProcessDownloadRequest();
}

const char* InterceptDownloadResourceThrottle::GetNameForLogging() const {
  return "InterceptDownloadResourceThrottle";
}

void InterceptDownloadResourceThrottle::ProcessDownloadRequest() {
  // TODO(qinmin): add UMA stats for all reasons that downloads are not
  // intercepted by the Android Download Manager. http://crbug.com/385272.
  if (request_->url_chain().empty())
    return;

  GURL url = request_->url_chain().back();
  if (!url.SchemeIsHTTPOrHTTPS())
    return;

  if (request_->method() != net::HttpRequestHeaders::kGetMethod)
    return;

  net::HttpRequestHeaders headers;
  if (!request_->GetFullRequestHeaders(&headers))
    return;

  // In general, if the request uses HTTP authorization, either with the origin
  // or a proxy, then the network stack should handle the download. The one
  // exception is a request that is fetched via the Chrome Proxy and does not
  // authenticate with the origin.
  if (request_->response_info().did_use_http_auth) {
    if (headers.HasHeader(net::HttpRequestHeaders::kAuthorization) ||
        !(request_->response_info().headers.get() &&
          data_reduction_proxy::HasDataReductionProxyViaHeader(
              request_->response_info().headers.get(), NULL))) {
      return;
    }
  }

  // If the cookie is possibly channel-bound, don't pass it to android download
  // manager.
  // TODO(qinmin): add a test for this. http://crbug.com/430541.
  if (request_->ssl_info().channel_id_sent)
    return;

  content::DownloadControllerAndroid::Get()->CreateGETDownload(
      render_process_id_, render_view_id_, request_id_);
  controller()->Cancel();
}

}  // namespace chrome
