// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/manage_passwords_bubble_blacklist_view_controller.h"

#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/passwords/manage_passwords_bubble_blacklist_view_controller.h"
#include "chrome/browser/ui/cocoa/passwords/manage_passwords_controller_test.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"
#include "components/password_manager/core/common/password_manager_ui.h"

// Helper delegate for testing the blacklist view of the password management
// bubble.
@interface ManagePasswordsBubbleBlacklistViewTestDelegate
    : NSObject<ManagePasswordsBubbleContentViewDelegate> {
  BOOL dismissed_;
}
@property(readonly) BOOL dismissed;
@end

@implementation ManagePasswordsBubbleBlacklistViewTestDelegate

@synthesize dismissed = dismissed_;

- (void)viewShouldDismiss {
  dismissed_ = YES;
}

@end

namespace {

// Tests for the blacklist view of the password management bubble.
class ManagePasswordsBubbleBlacklistViewControllerTest
    : public ManagePasswordsControllerTest {
 public:
  ManagePasswordsBubbleBlacklistViewControllerTest() : controller_(nil) {}

  void SetUp() override {
    ManagePasswordsControllerTest::SetUp();
    delegate_.reset(
        [[ManagePasswordsBubbleBlacklistViewTestDelegate alloc] init]);
  }

  ManagePasswordsBubbleBlacklistViewTestDelegate* delegate() {
    return delegate_.get();
  }

  ManagePasswordsBubbleBlacklistViewController* controller() {
    if (!controller_) {
      controller_.reset([[ManagePasswordsBubbleBlacklistViewController alloc]
          initWithModel:model()
               delegate:delegate()]);
      [controller_ loadView];
    }
    return controller_.get();
  }

 private:
  base::scoped_nsobject<ManagePasswordsBubbleBlacklistViewController>
      controller_;
  base::scoped_nsobject<ManagePasswordsBubbleBlacklistViewTestDelegate>
      delegate_;
};

TEST_F(ManagePasswordsBubbleBlacklistViewControllerTest,
       ShouldDismissWhenDoneClicked) {
  NSButton* doneButton = controller().doneButton;
  [doneButton performClick:nil];
  EXPECT_TRUE(delegate().dismissed);
}

TEST_F(ManagePasswordsBubbleBlacklistViewControllerTest,
       ShouldDismissAndUnblacklistWhenUnblacklistClicked) {
  NSButton* undoButton = controller().undoBlacklistButton;
  [undoButton performClick:nil];
  EXPECT_TRUE(delegate().dismissed);
  EXPECT_TRUE(ui_controller()->unblacklist_site());
}

}  // namespace
