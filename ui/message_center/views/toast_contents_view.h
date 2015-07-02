// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_TOAST_CONTENTS_VIEW_H_
#define UI_MESSAGE_CENTER_VIEWS_TOAST_CONTENTS_VIEW_H_

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/message_center/views/message_center_controller.h"
#include "ui/views/widget/widget_delegate.h"

namespace gfx {
class Animation;
class SlideAnimation;
}

namespace views {
class View;
}

namespace message_center {

class MessagePopupCollection;
class MessageView;
class Notification;

// The widget host for a popup. Also implements MessageCenterController
// which delegates over to MessagePopupCollection, but takes care about
// checking the weakref since MessagePopupCollection may disappear before
// widget/views are closed/destructed.
class ToastContentsView : public views::WidgetDelegateView,
                          public MessageCenterController,
                          public gfx::AnimationDelegate {
 public:
  // Computes the size of a toast assuming it will host the given view.
  static gfx::Size GetToastSizeForView(const views::View* view);

  ToastContentsView(const std::string& notification_id,
                    base::WeakPtr<MessagePopupCollection> collection);
  ~ToastContentsView() override;

  // Sets the inner view of the toast. If it has contents already,
  // |a11y_feedback_for_updates| causes the view to notify that the
  // accessibility message should be read after this update.
  void SetContents(MessageView* view, bool a11y_feedback_for_updates);

  void UpdateContents(const Notification& notification,
                      bool a11y_feedback_for_updates);

  // Shows the new toast for the first time, animated.
  // |origin| is the right-bottom corner of the toast.
  void RevealWithAnimation(gfx::Point origin);

  // Disconnectes the toast from the rest of the system immediately and start
  // an animation. Once animation finishes, closes the widget.
  void CloseWithAnimation();

  void SetBoundsWithAnimation(gfx::Rect new_bounds);

  // Origin and bounds are not 'instant', but rather 'current stable values',
  // there could be animation in progress that targets these values.
  gfx::Point origin() { return origin_; }
  gfx::Rect bounds() { return gfx::Rect(origin_, preferred_size_); }

  const std::string& id() { return id_; }

  // Overridden from views::View:
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void Layout() override;
  gfx::Size GetPreferredSize() const override;
  void GetAccessibleState(ui::AXViewState* state) override;

 private:
  // Overridden from MessageCenterController:
  void ClickOnNotification(const std::string& notification_id) override;
  void RemoveNotification(const std::string& notification_id,
                          bool by_user) override;
  scoped_ptr<ui::MenuModel> CreateMenuModel(
      const NotifierId& notifier_id,
      const base::string16& display_source) override;
  bool HasClickedListener(const std::string& notification_id) override;
  void ClickOnNotificationButton(const std::string& notification_id,
                                 int button_index) override;

  // Overridden from gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

  // Overridden from views::WidgetDelegate:
  views::View* GetContentsView() override;
  void WindowClosing() override;
  void OnDisplayChanged() override;
  void OnWorkAreaChanged() override;

  // Initialization and update.
  void CreateWidget(gfx::NativeView parent);

  // Immediately moves the toast without any sort of delay or animation.
  void SetBoundsInstantly(gfx::Rect new_bounds);

  // Given the bounds of a toast on the screen, compute the bouds for that
  // toast in 'closed' state. The 'closed' state is used as origin/destination
  // in reveal/closing animations.
  gfx::Rect GetClosedToastBounds(gfx::Rect bounds);

  void StartFadeIn();
  void StartFadeOut();  // Will call Widget::Close() when animation ends.
  void OnBoundsAnimationEndedOrCancelled(const gfx::Animation* animation);

  base::WeakPtr<MessagePopupCollection> collection_;

  // Id if the corresponding Notification.
  std::string id_;

  scoped_ptr<gfx::SlideAnimation> bounds_animation_;
  scoped_ptr<gfx::SlideAnimation> fade_animation_;

  gfx::Rect animated_bounds_start_;
  gfx::Rect animated_bounds_end_;
  // Started closing animation, will close at the end.
  bool is_closing_;
  // Closing animation - when it ends, close the widget. Weak, only used
  // for referential equality.
  gfx::Animation* closing_animation_;

  gfx::Point origin_;
  gfx::Size preferred_size_;

  DISALLOW_COPY_AND_ASSIGN(ToastContentsView);
};

}  // namespace message_center

#endif // UI_MESSAGE_CENTER_VIEWS_TOAST_CONTENTS_VIEW_H_
