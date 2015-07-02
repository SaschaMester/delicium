// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/android/shell/chrome_main_delegate_chrome_shell_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"

ChromeMainDelegateAndroid* ChromeMainDelegateAndroid::Create() {
  return new ChromeMainDelegateChromeShellAndroid();
}

ChromeMainDelegateChromeShellAndroid::ChromeMainDelegateChromeShellAndroid() {
}

ChromeMainDelegateChromeShellAndroid::~ChromeMainDelegateChromeShellAndroid() {
}

bool ChromeMainDelegateChromeShellAndroid::BasicStartupComplete(
    int* exit_code) {
  return ChromeMainDelegateAndroid::BasicStartupComplete(exit_code);
}

