// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_AUTOMATION_INTERNAL_CUSTOM_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_AUTOMATION_INTERNAL_CUSTOM_BINDINGS_H_

#include "base/compiler_specific.h"
#include "extensions/renderer/object_backed_native_handler.h"
#include "ipc/ipc_message.h"
#include "ui/accessibility/ax_tree.h"
#include "v8/include/v8.h"

struct ExtensionMsg_AccessibilityEventParams;

namespace extensions {

class AutomationMessageFilter;

// The native component of custom bindings for the chrome.automationInternal
// API.
class AutomationInternalCustomBindings : public ObjectBackedNativeHandler {
 public:
  explicit AutomationInternalCustomBindings(ScriptContext* context);

  ~AutomationInternalCustomBindings() override;

  bool OnMessageReceived(const IPC::Message& message);

 private:
  // Returns whether this extension has the "interact" permission set (either
  // explicitly or implicitly after manifest parsing).
  void IsInteractPermitted(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Returns an object with bindings that will be added to the
  // chrome.automation namespace.
  void GetSchemaAdditions(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Get the routing ID for the extension.
  void GetRoutingID(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Handle accessibility events from the browser process.
  void OnAccessibilityEvent(
      const ExtensionMsg_AccessibilityEventParams& params);

  scoped_refptr<AutomationMessageFilter> message_filter_;

  DISALLOW_COPY_AND_ASSIGN(AutomationInternalCustomBindings);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_AUTOMATION_INTERNAL_CUSTOM_BINDINGS_H_
