// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/precache/content/precache_manager.h"

#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/precache/core/precache_switches.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace precache {

namespace {

using ::testing::_;
using ::testing::SaveArg;

// A map of histogram names to the total sample counts.
typedef std::map<std::string, base::HistogramBase::Count> HistogramCountMap;

const char kConfigURL[] = "http://config-url.com";
const char kManifestURLPrefix[] = "http://manifest-url-prefix.com/";
const char kGoodManifestURL[] =
    "http://manifest-url-prefix.com/good-manifest.com";

base::HistogramBase::Count GetHistogramTotalCount(const char* histogram_name) {
  base::HistogramBase* histogram =
      base::StatisticsRecorder::FindHistogram(histogram_name);
  return histogram ? histogram->SnapshotSamples()->TotalCount() : 0;
}

HistogramCountMap GetHistogramCountMap() {
  // Note that the PrecacheManager tests don't care about the ".Cellular"
  // histograms.
  const char* kHistogramNames[] = {"Precache.DownloadedPrecacheMotivated",
                                   "Precache.DownloadedNonPrecache",
                                   "Precache.Saved"};

  HistogramCountMap histogram_count_map;
  for (size_t i = 0; i < arraysize(kHistogramNames); ++i) {
    histogram_count_map[kHistogramNames[i]] =
        GetHistogramTotalCount(kHistogramNames[i]);
  }
  return histogram_count_map;
}

class TestURLFetcherCallback {
 public:
  scoped_ptr<net::FakeURLFetcher> CreateURLFetcher(
      const GURL& url, net::URLFetcherDelegate* delegate,
      const std::string& response_data, net::HttpStatusCode response_code,
      net::URLRequestStatus::Status status) {
    scoped_ptr<net::FakeURLFetcher> fetcher(new net::FakeURLFetcher(
        url, delegate, response_data, response_code, status));

    requested_urls_.insert(url);
    return fetcher.Pass();
  }

  const std::multiset<GURL>& requested_urls() const {
    return requested_urls_;
  }

 private:
  // Multiset with one entry for each URL requested.
  std::multiset<GURL> requested_urls_;
};

class MockHistoryService : public history::HistoryService {
 public:
  MOCK_CONST_METHOD2(TopHosts,
                     void(int num_hosts, const TopHostsCallback& callback));
};

ACTION_P(ReturnHosts, starting_hosts) {
  arg1.Run(starting_hosts);
}

class TestPrecacheCompletionCallback {
 public:
  TestPrecacheCompletionCallback() : was_on_done_called_(false) {}

  void OnDone() {
    was_on_done_called_ = true;
  }

  PrecacheManager::PrecacheCompletionCallback GetCallback() {
    return base::Bind(&TestPrecacheCompletionCallback::OnDone,
                      base::Unretained(this));
  }

  bool was_on_done_called() const {
    return was_on_done_called_;
  }

 private:
  bool was_on_done_called_;
};

class PrecacheManagerTest : public testing::Test {
 public:
  PrecacheManagerTest()
      : precache_manager_(&browser_context_, nullptr /* sync_service */),
        factory_(nullptr,
                 base::Bind(&TestURLFetcherCallback::CreateURLFetcher,
                            base::Unretained(&url_callback_))) {}

 protected:
  void SetUp() override {
    base::StatisticsRecorder::Initialize();

    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kPrecacheConfigSettingsURL, kConfigURL);
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kPrecacheManifestURLPrefix, kManifestURLPrefix);

    // Make the fetch of the precache configuration settings fail. Precaching
    // should still complete normally in this case.
    factory_.SetFakeResponse(GURL(kConfigURL), "",
                             net::HTTP_INTERNAL_SERVER_ERROR,
                             net::URLRequestStatus::FAILED);
  }

  content::TestBrowserThreadBundle test_browser_thread_bundle_;
  content::TestBrowserContext browser_context_;
  PrecacheManager precache_manager_;
  TestURLFetcherCallback url_callback_;
  net::FakeURLFetcherFactory factory_;
  TestPrecacheCompletionCallback precache_callback_;
};

TEST_F(PrecacheManagerTest, StartAndFinishPrecaching) {
  EXPECT_FALSE(precache_manager_.IsPrecaching());

  MockHistoryService history_service;
  MockHistoryService::TopHostsCallback top_hosts_callback;
  EXPECT_CALL(history_service, TopHosts(NumTopHosts(), _))
      .WillOnce(SaveArg<1>(&top_hosts_callback));

  factory_.SetFakeResponse(GURL(kGoodManifestURL), "", net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);

  precache_manager_.StartPrecaching(precache_callback_.GetCallback(),
                                    history_service);

  EXPECT_TRUE(precache_manager_.IsPrecaching());

  top_hosts_callback.Run(
      history::TopHostsList(1, std::make_pair("good-manifest.com", 1)));
  base::MessageLoop::current()->RunUntilIdle();  // For PrecacheFetcher.
  EXPECT_FALSE(precache_manager_.IsPrecaching());
  EXPECT_TRUE(precache_callback_.was_on_done_called());

  std::multiset<GURL> expected_requested_urls;
  expected_requested_urls.insert(GURL(kConfigURL));
  expected_requested_urls.insert(GURL(kGoodManifestURL));
  EXPECT_EQ(expected_requested_urls, url_callback_.requested_urls());
}

TEST_F(PrecacheManagerTest, StartAndCancelPrecachingBeforeURLsReceived) {
  EXPECT_FALSE(precache_manager_.IsPrecaching());

  MockHistoryService history_service;
  MockHistoryService::TopHostsCallback top_hosts_callback;
  EXPECT_CALL(history_service, TopHosts(NumTopHosts(), _))
      .WillOnce(SaveArg<1>(&top_hosts_callback));

  precache_manager_.StartPrecaching(precache_callback_.GetCallback(),
                                    history_service);
  EXPECT_TRUE(precache_manager_.IsPrecaching());

  precache_manager_.CancelPrecaching();
  EXPECT_FALSE(precache_manager_.IsPrecaching());

  top_hosts_callback.Run(
      history::TopHostsList(1, std::make_pair("starting-url.com", 1)));
  base::MessageLoop::current()->RunUntilIdle();  // For PrecacheFetcher.
  EXPECT_FALSE(precache_manager_.IsPrecaching());
  EXPECT_FALSE(precache_callback_.was_on_done_called());
  EXPECT_TRUE(url_callback_.requested_urls().empty());
}

TEST_F(PrecacheManagerTest, StartAndCancelPrecachingAfterURLsReceived) {
  EXPECT_FALSE(precache_manager_.IsPrecaching());

  MockHistoryService history_service;
  EXPECT_CALL(history_service, TopHosts(NumTopHosts(), _))
      .WillOnce(ReturnHosts(
          history::TopHostsList(1, std::make_pair("starting-url.com", 1))));

  precache_manager_.StartPrecaching(precache_callback_.GetCallback(),
                                    history_service);

  // Since the |history_service| ran the callback immediately, Start() has
  // been called on the PrecacheFetcher, and the precache config settings have
  // been requested. The response has not yet been received though, so
  // precaching is still in progress.
  EXPECT_TRUE(precache_manager_.IsPrecaching());

  precache_manager_.CancelPrecaching();
  EXPECT_FALSE(precache_manager_.IsPrecaching());

  base::MessageLoop::current()->RunUntilIdle();  // For PrecacheFetcher.
  EXPECT_FALSE(precache_manager_.IsPrecaching());
  EXPECT_FALSE(precache_callback_.was_on_done_called());

  // Even though the response for the precache config settings should not have
  // been received, the request should still have been made.
  std::multiset<GURL> expected_requested_urls;
  expected_requested_urls.insert(GURL(kConfigURL));
  EXPECT_EQ(expected_requested_urls, url_callback_.requested_urls());
}

TEST_F(PrecacheManagerTest, RecordStatsForFetchWithIrrelevantFetches) {
  HistogramCountMap expected_histogram_count_map = GetHistogramCountMap();

  // Fetches with size 0 should be ignored.
  precache_manager_.RecordStatsForFetch(GURL("http://url.com"), base::Time(), 0,
                                        false);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(expected_histogram_count_map, GetHistogramCountMap());

  // Fetches for URLs with schemes other than HTTP or HTTPS should be ignored.
  precache_manager_.RecordStatsForFetch(GURL("ftp://ftp.com"), base::Time(),
                                        1000, false);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(expected_histogram_count_map, GetHistogramCountMap());

  // Fetches for empty URLs should be ignored.
  precache_manager_.RecordStatsForFetch(GURL(), base::Time(), 1000, false);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(expected_histogram_count_map, GetHistogramCountMap());
}

TEST_F(PrecacheManagerTest, RecordStatsForFetchDuringPrecaching) {
  HistogramCountMap expected_histogram_count_map = GetHistogramCountMap();

  MockHistoryService history_service;
  EXPECT_CALL(history_service, TopHosts(NumTopHosts(), _))
      .WillOnce(ReturnHosts(history::TopHostsList()));

  precache_manager_.StartPrecaching(precache_callback_.GetCallback(),
                                    history_service);

  EXPECT_TRUE(precache_manager_.IsPrecaching());
  precache_manager_.RecordStatsForFetch(GURL("http://url.com"), base::Time(),
                                        1000, false);

  precache_manager_.CancelPrecaching();

  // For PrecacheFetcher and RecordURLPrecached.
  base::MessageLoop::current()->RunUntilIdle();
  expected_histogram_count_map["Precache.DownloadedPrecacheMotivated"]++;
  EXPECT_EQ(expected_histogram_count_map, GetHistogramCountMap());
}

TEST_F(PrecacheManagerTest, RecordStatsForFetchHTTP) {
  HistogramCountMap expected_histogram_count_map = GetHistogramCountMap();

  precache_manager_.RecordStatsForFetch(GURL("http://http-url.com"),
                                        base::Time(), 1000, false);
  base::MessageLoop::current()->RunUntilIdle();

  expected_histogram_count_map["Precache.DownloadedNonPrecache"]++;
  EXPECT_EQ(expected_histogram_count_map, GetHistogramCountMap());
}

TEST_F(PrecacheManagerTest, RecordStatsForFetchHTTPS) {
  HistogramCountMap expected_histogram_count_map = GetHistogramCountMap();

  precache_manager_.RecordStatsForFetch(GURL("https://https-url.com"),
                                        base::Time(), 1000, false);
  base::MessageLoop::current()->RunUntilIdle();

  expected_histogram_count_map["Precache.DownloadedNonPrecache"]++;
  EXPECT_EQ(expected_histogram_count_map, GetHistogramCountMap());
}

TEST_F(PrecacheManagerTest, DeleteExpiredPrecacheHistory) {
  // This test has to use Time::Now() because StartPrecaching uses Time::Now().
  const base::Time kCurrentTime = base::Time::Now();
  HistogramCountMap expected_histogram_count_map = GetHistogramCountMap();

  MockHistoryService history_service;
  EXPECT_CALL(history_service, TopHosts(NumTopHosts(), _))
      .Times(2)
      .WillRepeatedly(ReturnHosts(history::TopHostsList()));

  precache_manager_.StartPrecaching(precache_callback_.GetCallback(),
                                    history_service);
  EXPECT_TRUE(precache_manager_.IsPrecaching());

  // Precache a bunch of URLs, with different fetch times.
  precache_manager_.RecordStatsForFetch(
      GURL("http://old-fetch.com"),
      kCurrentTime - base::TimeDelta::FromDays(61), 1000, false);
  precache_manager_.RecordStatsForFetch(
      GURL("http://recent-fetch.com"),
      kCurrentTime - base::TimeDelta::FromDays(59), 1000, false);
  precache_manager_.RecordStatsForFetch(
      GURL("http://yesterday-fetch.com"),
      kCurrentTime - base::TimeDelta::FromDays(1), 1000, false);
  expected_histogram_count_map["Precache.DownloadedPrecacheMotivated"] += 3;

  precache_manager_.CancelPrecaching();
  // For PrecacheFetcher and RecordURLPrecached.
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(expected_histogram_count_map, GetHistogramCountMap());

  // The expired precache will be deleted during precaching this time.
  precache_manager_.StartPrecaching(precache_callback_.GetCallback(),
                                    history_service);
  EXPECT_TRUE(precache_manager_.IsPrecaching());

  precache_manager_.CancelPrecaching();
  // For PrecacheFetcher and RecordURLPrecached.
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_FALSE(precache_manager_.IsPrecaching());

  // A fetch for the same URL as the expired precache was served from the cache,
  // but it isn't reported as saved bytes because it had expired in the precache
  // history.
  precache_manager_.RecordStatsForFetch(
      GURL("http://old-fetch.com"),
      kCurrentTime, 1000, true);

  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(expected_histogram_count_map, GetHistogramCountMap());

  // The other precaches should not have expired, so the following fetches from
  // the cache should count as saved bytes.
  precache_manager_.RecordStatsForFetch(
      GURL("http://recent-fetch.com"),
      kCurrentTime, 1000, true);
  precache_manager_.RecordStatsForFetch(
      GURL("http://yesterday-fetch.com"),
      kCurrentTime, 1000, true);
  expected_histogram_count_map["Precache.Saved"] += 2;

  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(expected_histogram_count_map, GetHistogramCountMap());
}

}  // namespace

}  // namespace precache
