// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/content_browser_test.h"

#include "base/command_line.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

IN_PROC_BROWSER_TEST_F(ContentBrowserTest, MANUAL_ShouldntRun) {
  // Ensures that tests with MANUAL_ prefix don't run automatically.
  ASSERT_TRUE(false);
}

class ContentBrowserTestSanityTest : public ContentBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    const testing::TestInfo* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    if (std::string(test_info->name()) == "SingleProcess")
      command_line->AppendSwitch(switches::kSingleProcess);
  }

  void Test() {
    GURL url = GetTestUrl(".", "simple_page.html");

    base::string16 expected_title(base::ASCIIToUTF16("OK"));
    TitleWatcher title_watcher(shell()->web_contents(), expected_title);
    NavigateToURL(shell(), url);
    base::string16 title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expected_title, title);
  }
};

IN_PROC_BROWSER_TEST_F(ContentBrowserTestSanityTest, Basic) {
  Test();
}

IN_PROC_BROWSER_TEST_F(ContentBrowserTestSanityTest, SingleProcess) {
  Test();
}

namespace {

void CallbackChecker(bool* non_nested_task_ran) {
  *non_nested_task_ran = true;
}

}  // namespace

IN_PROC_BROWSER_TEST_F(ContentBrowserTestSanityTest, NonNestableTask) {
  bool non_nested_task_ran = false;
  base::ThreadTaskRunnerHandle::Get()->PostNonNestableTask(
      FROM_HERE, base::Bind(&CallbackChecker, &non_nested_task_ran));
  content::RunAllPendingInMessageLoop();
  ASSERT_TRUE(non_nested_task_ran);
}

}  // namespace content
