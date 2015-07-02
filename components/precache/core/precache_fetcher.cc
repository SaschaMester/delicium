// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/precache/core/precache_fetcher.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "components/precache/core/precache_switches.h"
#include "components/precache/core/proto/precache.pb.h"
#include "net/base/completion_callback.h"
#include "net/base/escape.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_fetcher_response_writer.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

using net::URLFetcher;

namespace precache {

namespace {

GURL GetConfigURL() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kPrecacheConfigSettingsURL)) {
    return GURL(
        command_line.GetSwitchValueASCII(switches::kPrecacheConfigSettingsURL));
  }

#if defined(PRECACHE_CONFIG_SETTINGS_URL)
  return GURL(PRECACHE_CONFIG_SETTINGS_URL);
#else
  // The precache config settings URL could not be determined, so return an
  // empty, invalid GURL.
  return GURL();
#endif
}

std::string GetDefaultManifestURLPrefix() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kPrecacheManifestURLPrefix)) {
    return command_line.GetSwitchValueASCII(
        switches::kPrecacheManifestURLPrefix);
  }

#if defined(PRECACHE_MANIFEST_URL_PREFIX)
  return PRECACHE_MANIFEST_URL_PREFIX;
#else
  // The precache manifest URL prefix could not be determined, so return an
  // empty string.
  return std::string();
#endif
}

// Construct the URL of the precache manifest for the given name (either host or
// URL). The server is expecting a request for a URL consisting of the manifest
// URL prefix followed by the doubly escaped name.
std::string ConstructManifestURL(const std::string& prefix,
                                 const std::string& name) {
  return prefix + net::EscapeQueryParamValue(
                      net::EscapeQueryParamValue(name, false), false);
}

// Attempts to parse a protobuf message from the response string of a
// URLFetcher. If parsing is successful, the message parameter will contain the
// parsed protobuf and this function will return true. Otherwise, returns false.
bool ParseProtoFromFetchResponse(const URLFetcher& source,
                                 ::google::protobuf::MessageLite* message) {
  std::string response_string;

  if (!source.GetStatus().is_success()) {
    DLOG(WARNING) << "Fetch failed: " << source.GetOriginalURL().spec();
    return false;
  }
  if (!source.GetResponseAsString(&response_string)) {
    DLOG(WARNING) << "No response string present: "
                  << source.GetOriginalURL().spec();
    return false;
  }
  if (!message->ParseFromString(response_string)) {
    DLOG(WARNING) << "Unable to parse proto served from "
                  << source.GetOriginalURL().spec();
    return false;
  }
  return true;
}

// URLFetcherResponseWriter that ignores the response body, in order to avoid
// the unnecessary memory usage. Use it rather than the default if you don't
// care about parsing the response body. We use it below as a means to populate
// the cache with requested resource URLs.
class URLFetcherNullWriter : public net::URLFetcherResponseWriter {
 public:
  int Initialize(const net::CompletionCallback& callback) override {
    return net::OK;
  }

  int Write(net::IOBuffer* buffer,
            int num_bytes,
            const net::CompletionCallback& callback) override {
    return num_bytes;
  }

  int Finish(const net::CompletionCallback& callback) override {
    return net::OK;
  }
};

}  // namespace

// Class that fetches a URL, and runs the specified callback when the fetch is
// complete. This class exists so that a different method can be run in
// response to different kinds of fetches, e.g. OnConfigFetchComplete when
// configuration settings are fetched, OnManifestFetchComplete when a manifest
// is fetched, etc.
class PrecacheFetcher::Fetcher : public net::URLFetcherDelegate {
 public:
  // Construct a new Fetcher. This will create and start a new URLFetcher for
  // the specified URL using the specified request context.
  Fetcher(net::URLRequestContextGetter* request_context,
          const GURL& url,
          const base::Callback<void(const URLFetcher&)>& callback,
          bool ignore_response_body);
  ~Fetcher() override {}
  void OnURLFetchComplete(const URLFetcher* source) override;

 private:
  const base::Callback<void(const URLFetcher&)> callback_;
  scoped_ptr<URLFetcher> url_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(Fetcher);
};

PrecacheFetcher::Fetcher::Fetcher(
    net::URLRequestContextGetter* request_context,
    const GURL& url,
    const base::Callback<void(const URLFetcher&)>& callback,
    bool ignore_response_body)
    : callback_(callback) {
  url_fetcher_ = URLFetcher::Create(url, URLFetcher::GET, this);
  url_fetcher_->SetRequestContext(request_context);
  url_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES |
                             net::LOAD_DO_NOT_SEND_COOKIES);
  if (ignore_response_body) {
    scoped_ptr<URLFetcherNullWriter> null_writer(new URLFetcherNullWriter);
    url_fetcher_->SaveResponseWithWriter(null_writer.Pass());
  }
  url_fetcher_->Start();
}

void PrecacheFetcher::Fetcher::OnURLFetchComplete(const URLFetcher* source) {
  callback_.Run(*source);
}

PrecacheFetcher::PrecacheFetcher(
    const std::vector<std::string>& starting_hosts,
    net::URLRequestContextGetter* request_context,
    const std::string& manifest_url_prefix,
    PrecacheFetcher::PrecacheDelegate* precache_delegate)
    : starting_hosts_(starting_hosts),
      request_context_(request_context),
      manifest_url_prefix_(manifest_url_prefix),
      precache_delegate_(precache_delegate) {
  DCHECK(request_context_.get());  // Request context must be non-NULL.
  DCHECK(precache_delegate_);  // Precache delegate must be non-NULL.

  DCHECK_NE(GURL(), GetConfigURL())
      << "Could not determine the precache config settings URL.";
  DCHECK_NE(std::string(), GetDefaultManifestURLPrefix())
      << "Could not determine the default precache manifest URL prefix.";
}

PrecacheFetcher::~PrecacheFetcher() {
}

void PrecacheFetcher::Start() {
  DCHECK(!fetcher_);  // Start shouldn't be called repeatedly.

  GURL config_url = GetConfigURL();
  DCHECK(config_url.is_valid());

  // Fetch the precache configuration settings from the server.
  fetcher_.reset(new Fetcher(request_context_.get(), config_url,
                             base::Bind(&PrecacheFetcher::OnConfigFetchComplete,
                                        base::Unretained(this)),
                             false /* ignore_response_body */));
}

void PrecacheFetcher::StartNextFetch() {
  if (!resource_urls_to_fetch_.empty()) {
    // Fetch the next resource URL.
    fetcher_.reset(
        new Fetcher(request_context_.get(), resource_urls_to_fetch_.front(),
                    base::Bind(&PrecacheFetcher::OnResourceFetchComplete,
                               base::Unretained(this)),
                    true /* ignore_response_body */));

    resource_urls_to_fetch_.pop_front();
    return;
  }

  if (!manifest_urls_to_fetch_.empty()) {
    // Fetch the next manifest URL.
    fetcher_.reset(
        new Fetcher(request_context_.get(), manifest_urls_to_fetch_.front(),
                    base::Bind(&PrecacheFetcher::OnManifestFetchComplete,
                               base::Unretained(this)),
                    false /* ignore_response_body */));

    manifest_urls_to_fetch_.pop_front();
    return;
  }

  // There are no more URLs to fetch, so end the precache cycle.
  precache_delegate_->OnDone();
  // OnDone may have deleted this PrecacheFetcher, so don't do anything after it
  // is called.
}

void PrecacheFetcher::OnConfigFetchComplete(const URLFetcher& source) {
  // Attempt to parse the config proto. On failure, continue on with the default
  // configuration.
  PrecacheConfigurationSettings config;
  ParseProtoFromFetchResponse(source, &config);

  std::string prefix = manifest_url_prefix_.empty()
                           ? GetDefaultManifestURLPrefix()
                           : manifest_url_prefix_;
  DCHECK_NE(std::string(), prefix)
      << "Could not determine the precache manifest URL prefix.";

  // Keep track of manifest URLs that are being fetched, in order to remove
  // duplicates.
  base::hash_set<std::string> unique_manifest_urls;

  // Attempt to fetch manifests for starting hosts up to the maximum top sites
  // count. If a manifest does not exist for a particular starting host, then
  // the fetch will fail, and that starting host will be ignored.
  int64 rank = 0;
  for (const std::string& host : starting_hosts_) {
    ++rank;
    if (rank > config.top_sites_count())
      break;
    unique_manifest_urls.insert(ConstructManifestURL(prefix, host));
  }

  for (const std::string& url : config.forced_site())
    unique_manifest_urls.insert(ConstructManifestURL(prefix, url));

  for (const std::string& manifest_url : unique_manifest_urls)
    manifest_urls_to_fetch_.push_back(GURL(manifest_url));

  StartNextFetch();
}

void PrecacheFetcher::OnManifestFetchComplete(const URLFetcher& source) {
  PrecacheManifest manifest;

  if (ParseProtoFromFetchResponse(source, &manifest)) {
    for (int i = 0; i < manifest.resource_size(); ++i) {
      if (manifest.resource(i).has_url()) {
        resource_urls_to_fetch_.push_back(GURL(manifest.resource(i).url()));
      }
    }
  }

  StartNextFetch();
}

void PrecacheFetcher::OnResourceFetchComplete(const URLFetcher& source) {
  // The resource has already been put in the cache during the fetch process, so
  // nothing more needs to be done for the resource.
  StartNextFetch();
}

}  // namespace precache
