// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/chrome_switches.h"

namespace switches {

// -----------------------------------------------------------------------------
// When commenting your switch, please use the same voice as surrounding
// comments. Imagine "This switch..." at the beginning of the phrase, and it'll
// all work out.
// -----------------------------------------------------------------------------

// Disables the use of WKWebView instead of UIWebView.
const char kDisableIOSWKWebView[] = "disable-wkwebview";

// Enables the use of WKWebView instead of UIWebView.
const char kEnableIOSWKWebView[] = "enable-wkwebview";

// Enables context-sensitive reader mode button in the toolbar.
const char kEnableReaderModeToolbarIcon[] = "enable-reader-mode-toolbar-icon";

// Defines the value in MB of the memory wedge to insert at cold launch.
const char kIOSMemoryWedgeSize[] = "ios-memory-wedge-size-mb";

}  // namespace switches
