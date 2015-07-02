// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/management/management_api_constants.h"

namespace extension_management_api_constants {

const char kDisabledReasonKey[] = "disabledReason";

const char kDisabledReasonPermissionsIncrease[] = "permissions_increase";

const char kExtensionCreateError[] =
    "Failed to create extension from manifest.";
const char kGestureNeededForEscalationError[] =
    "Re-enabling an extension disabled due to permissions increase "
    "requires a user gesture.";
const char kGestureNeededForUninstallError[] =
    "chrome.management.uninstall requires a user gesture.";
const char kManifestParseError[] = "Failed to parse manifest.";
const char kNoExtensionError[] = "Failed to find extension with id *.";
const char kNotAnAppError[] = "Extension * is not an App.";
const char kUserCantModifyError[] = "Extension * cannot be modified by user.";
const char kUninstallCanceledError[] =
    "Extension * uninstall canceled by user.";
const char kUserDidNotReEnableError[] =
    "The user did not accept the re-enable dialog.";
const char kMissingRequirementsError[] = "There were missing requirements: *.";
const char kGestureNeededForCreateAppShortcutError[] =
    "chrome.management.createAppShortcut requires a user gesture.";
const char kNoBrowserToCreateShortcut[] =
    "There is no browser window to create shortcut.";
const char kCreateOnlyPackagedAppShortcutMac[] =
    "Shortcuts can only be created for new-style packaged apps on Mac.";
const char kCreateShortcutCanceledError[] =
    "App shortcuts creation canceled by user.";
const char kGestureNeededForSetLaunchTypeError[] =
    "chrome.management.setLaunchType requires a user gesture.";
const char kLaunchTypeNotAvailableError[] =
    "The launch type is not available for this app.";
const char kGestureNeededForGenerateAppForLinkError[] =
    "chrome.management.generateAppForLink requires a user gesture.";
const char kInvalidURLError[] = "The URL \"*\" is invalid.";
const char kEmptyTitleError[] = "The title can not be empty.";
const char kGenerateAppForLinkInstallError[] =
    "Failed to install the generated app.";

}  // namespace extension_management_api_constants
