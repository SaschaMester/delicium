// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/mock_browsing_data_channel_id_helper.h"

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

MockBrowsingDataChannelIDHelper::MockBrowsingDataChannelIDHelper()
    : BrowsingDataChannelIDHelper() {}

MockBrowsingDataChannelIDHelper::
~MockBrowsingDataChannelIDHelper() {}

void MockBrowsingDataChannelIDHelper::StartFetching(
    const FetchResultCallback& callback) {
  ASSERT_FALSE(callback.is_null());
  ASSERT_TRUE(callback_.is_null());
  callback_ = callback;
}

void MockBrowsingDataChannelIDHelper::DeleteChannelID(
    const std::string& server_id) {
  ASSERT_FALSE(callback_.is_null());
  ASSERT_TRUE(channel_ids_.find(server_id) != channel_ids_.end());
  channel_ids_[server_id] = false;
}

void MockBrowsingDataChannelIDHelper::AddChannelIDSample(
    const std::string& server_id) {
  ASSERT_TRUE(channel_ids_.find(server_id) == channel_ids_.end());
  scoped_ptr<crypto::ECPrivateKey> key(crypto::ECPrivateKey::Create());
  channel_id_list_.push_back(
      net::ChannelIDStore::ChannelID(server_id, base::Time(), key.Pass()));
  channel_ids_[server_id] = true;
}

void MockBrowsingDataChannelIDHelper::Notify() {
  net::ChannelIDStore::ChannelIDList channel_id_list;
  for (net::ChannelIDStore::ChannelIDList::iterator i =
       channel_id_list_.begin();
       i != channel_id_list_.end(); ++i) {
    if (channel_ids_[i->server_identifier()])
      channel_id_list.push_back(*i);
  }
  callback_.Run(channel_id_list);
}

void MockBrowsingDataChannelIDHelper::Reset() {
  for (std::map<const std::string, bool>::iterator i =
       channel_ids_.begin();
       i != channel_ids_.end(); ++i)
    i->second = true;
}

bool MockBrowsingDataChannelIDHelper::AllDeleted() {
  for (std::map<const std::string, bool>::const_iterator i =
       channel_ids_.begin();
       i != channel_ids_.end(); ++i)
    if (i->second)
      return false;
  return true;
}
