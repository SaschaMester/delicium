// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/view_manager/native_viewport/platform_viewport.h"

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "components/view_manager/public/interfaces/view_manager.mojom.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/input_events/input_events_type_converters.h"
#include "mojo/converters/input_events/mojo_extended_key_event_data.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/stub/stub_window.h"

#if defined(OS_WIN)
#include "ui/platform_window/win/win_window.h"
#elif defined(USE_X11)
#include "ui/platform_window/x11/x11_window.h"
#elif defined(OS_ANDROID)
#include "ui/platform_window/android/platform_window_android.h"
#endif

namespace native_viewport {
namespace {

float ConvertUIWheelValueToMojoValue(int offset) {
  // Mojo's event type takes a value between -1 and 1. Normalize by allowing
  // up to 20 of ui's offset. This is a bit arbitrary.
  return std::max(
      -1.0f, std::min(1.0f, static_cast<float>(offset) /
                                (20 * static_cast<float>(
                                          ui::MouseWheelEvent::kWheelDelta))));
}
}  // namespace

class PlatformViewportCommon : public PlatformViewport,
                               public ui::PlatformWindowDelegate {
 public:
  PlatformViewportCommon(Delegate* delegate, bool headless)
      : delegate_(delegate), headless_(headless) {}

  ~PlatformViewportCommon() override {
    // Destroy the platform-window while |this| is still alive.
    platform_window_.reset();
  }

 private:
  // Overridden from PlatformViewport:
  void Init(const gfx::Rect& bounds) override {
    CHECK(!platform_window_);

    metrics_ = mojo::ViewportMetrics::New();
    metrics_->size_in_pixels = mojo::Size::From(bounds.size());

    if (headless_) {
      platform_window_.reset(new ui::StubWindow(this));
    } else {
#if defined(OS_WIN)
      platform_window_.reset(new ui::WinWindow(this, bounds));
#elif defined(USE_X11)
      platform_window_.reset(new ui::X11Window(this));
#elif defined(OS_ANDROID)
      platform_window_.reset(new ui::PlatformWindowAndroid(this));
#endif
    }
    platform_window_->SetBounds(bounds);
  }

  void Show() override { platform_window_->Show(); }

  void Hide() override { platform_window_->Hide(); }

  void Close() override { platform_window_->Close(); }

  gfx::Size GetSize() override {
    return metrics_->size_in_pixels.To<gfx::Size>();
  }

  void SetBounds(const gfx::Rect& bounds) override {
    platform_window_->SetBounds(bounds);
  }

  // ui::PlatformWindowDelegate:
  void OnBoundsChanged(const gfx::Rect& new_bounds) override {
    delegate_->OnMetricsChanged(new_bounds.size(),
                                metrics_->device_pixel_ratio);
  }

  void OnDamageRect(const gfx::Rect& damaged_region) override {}

  void DispatchEvent(ui::Event* event) override {
    mojo::EventPtr mojo_event(mojo::Event::From(*event));
    if (event->IsMouseWheelEvent()) {
      // Mojo's event type has a different meaning for wheel events. Convert
      // between the two.
      ui::MouseWheelEvent* wheel_event =
          static_cast<ui::MouseWheelEvent*>(event);
      DCHECK(mojo_event->pointer_data);
      mojo_event->pointer_data->horizontal_wheel =
          ConvertUIWheelValueToMojoValue(wheel_event->x_offset());
      mojo_event->pointer_data->horizontal_wheel =
          ConvertUIWheelValueToMojoValue(wheel_event->y_offset());
    }
    delegate_->OnEvent(mojo_event.Pass());

    switch (event->type()) {
      case ui::ET_MOUSE_PRESSED:
      case ui::ET_TOUCH_PRESSED:
        platform_window_->SetCapture();
        break;
      case ui::ET_MOUSE_RELEASED:
      case ui::ET_TOUCH_RELEASED:
        platform_window_->ReleaseCapture();
        break;
      default:
        break;
    }

#if defined(USE_X11)
    // We want to emulate the WM_CHAR generation behaviour of Windows.
    //
    // On Linux, we've previously inserted characters by having
    // InputMethodAuraLinux take all key down events and send a character event
    // to the TextInputClient. This causes a mismatch in code that has to be
    // shared between Windows and Linux, including blink code. Now that we're
    // trying to have one way of doing things, we need to standardize on and
    // emulate Windows character events.
    //
    // This is equivalent to what we're doing in the current Linux port, but
    // done once instead of done multiple times in different places.
    if (event->type() == ui::ET_KEY_PRESSED) {
      ui::KeyEvent* key_press_event = static_cast<ui::KeyEvent*>(event);
      ui::KeyEvent char_event(key_press_event->GetCharacter(),
                              key_press_event->key_code(),
                              key_press_event->flags());

      DCHECK_EQ(key_press_event->GetCharacter(), char_event.GetCharacter());
      DCHECK_EQ(key_press_event->key_code(), char_event.key_code());
      DCHECK_EQ(key_press_event->flags(), char_event.flags());

      char_event.SetExtendedKeyEventData(
          make_scoped_ptr(new mojo::MojoExtendedKeyEventData(
              key_press_event->GetLocatedWindowsKeyboardCode(),
              key_press_event->GetText(),
              key_press_event->GetUnmodifiedText())));
      char_event.set_platform_keycode(key_press_event->platform_keycode());

      delegate_->OnEvent(mojo::Event::From(char_event));
    }
#endif
  }

  void OnCloseRequest() override { platform_window_->Close(); }

  void OnClosed() override { delegate_->OnDestroyed(); }

  void OnWindowStateChanged(ui::PlatformWindowState state) override {}

  void OnLostCapture() override {}

  void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget,
                                    float device_pixel_ratio) override {
    metrics_->device_pixel_ratio = device_pixel_ratio;
    delegate_->OnAcceleratedWidgetAvailable(widget,
                                            metrics_->device_pixel_ratio);
  }

  void OnActivationChanged(bool active) override {}

  scoped_ptr<ui::PlatformWindow> platform_window_;
  Delegate* delegate_;
  bool headless_;
  mojo::ViewportMetricsPtr metrics_;

  DISALLOW_COPY_AND_ASSIGN(PlatformViewportCommon);
};

// static
scoped_ptr<PlatformViewport> PlatformViewport::Create(Delegate* delegate,
                                                      bool headless) {
  return make_scoped_ptr(new PlatformViewportCommon(delegate, headless));
}

}  // namespace native_viewport
