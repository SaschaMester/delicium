// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/update_client/chrome_update_query_params_delegate.h"

#include "base/lazy_instance.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_version_info.h"

namespace {

const char kUnknown[] = "unknown";
const char kStable[] = "stable";
const char kBeta[] = "beta";
const char kDev[] = "dev";
const char kCanary[] = "canary";

base::LazyInstance<ChromeUpdateQueryParamsDelegate> g_delegate =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

ChromeUpdateQueryParamsDelegate::ChromeUpdateQueryParamsDelegate() {
}

ChromeUpdateQueryParamsDelegate::~ChromeUpdateQueryParamsDelegate() {
}

// static
ChromeUpdateQueryParamsDelegate*
ChromeUpdateQueryParamsDelegate::GetInstance() {
  return g_delegate.Pointer();
}

std::string ChromeUpdateQueryParamsDelegate::GetExtraParams() {
  return base::StringPrintf("&prodchannel=%s&prodversion=%s&lang=%s",
                            GetChannelString(),
                            chrome::VersionInfo().Version().c_str(), GetLang());
}

// static
const char* ChromeUpdateQueryParamsDelegate::GetChannelString() {
  switch (chrome::VersionInfo::GetChannel()) {
    case chrome::VersionInfo::CHANNEL_STABLE:
      return kStable;
      break;
    case chrome::VersionInfo::CHANNEL_BETA:
      return kBeta;
      break;
    case chrome::VersionInfo::CHANNEL_DEV:
      return kDev;
      break;
    case chrome::VersionInfo::CHANNEL_CANARY:
      return kCanary;
      break;
    case chrome::VersionInfo::CHANNEL_UNKNOWN:
      return kUnknown;
      break;
  }
  return kUnknown;
}

// static
const char* ChromeUpdateQueryParamsDelegate::GetLang() {
  return g_browser_process->GetApplicationLocale().c_str();
}
