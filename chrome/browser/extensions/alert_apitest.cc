// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/app_modal/app_modal_dialog.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, AlertBasic) {
  ASSERT_TRUE(RunExtensionTest("alert")) << message_;

  const extensions::Extension* extension = GetSingleLoadedExtension();
  extensions::ExtensionHost* host =
      extensions::ProcessManager::Get(browser()->profile())
          ->GetBackgroundHostForExtension(extension->id());
  ASSERT_TRUE(host);
  host->host_contents()->GetMainFrame()->ExecuteJavaScript(
      base::ASCIIToUTF16("alert('This should not crash.');"));

  app_modal::AppModalDialog* alert = ui_test_utils::WaitForAppModalDialog();
  ASSERT_TRUE(alert);
  alert->CloseModalDialog();
}
