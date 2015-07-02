// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_JSON_SAFE_JSON_PARSER_H_
#define COMPONENTS_SAFE_JSON_SAFE_JSON_PARSER_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/utility_process_host_client.h"

namespace base {
class ListValue;
class SingleThreadTaskRunner;
class Value;
}

namespace IPC {
class Message;
}

namespace safe_json {

// SafeJsonParser parses a given JSON safely via a utility process. The object
// is ref-counted and kept alive after Start() is called until one of the two
// callbacks is called.
class SafeJsonParser : public content::UtilityProcessHostClient {
 public:
  typedef base::Callback<void(scoped_ptr<base::Value>)> SuccessCallback;
  typedef base::Callback<void(const std::string&)> ErrorCallback;

  SafeJsonParser(const std::string& unsafe_json,
                 const SuccessCallback& success_callback,
                 const ErrorCallback& error_callback);

  void Start();

 private:
  ~SafeJsonParser() override;

  void StartWorkOnIOThread();

  void OnJSONParseSucceeded(const base::ListValue& wrapper);
  void OnJSONParseFailed(const std::string& error_message);

  void ReportResults();
  void ReportResultsOnOriginThread();

  // Implementing pieces of the UtilityProcessHostClient interface.
  bool OnMessageReceived(const IPC::Message& message) override;

  const std::string unsafe_json_;
  SuccessCallback success_callback_;
  ErrorCallback error_callback_;
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

  scoped_ptr<base::Value> parsed_json_;
  std::string error_;

  DISALLOW_COPY_AND_ASSIGN(SafeJsonParser);
};

}  // namespace safe_json

#endif  // COMPONENTS_SAFE_JSON_SAFE_JSON_PARSER_H_
