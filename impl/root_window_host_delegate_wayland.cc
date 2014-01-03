// Copyright 2014 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ozone/impl/root_window_host_delegate_wayland.h"

#include "ozone/impl/desktop_root_window_host_wayland.h"
#include "ozone/impl/ozone_display.h"
#include "ui/events/event_utils.h"

namespace ozonewayland {

RootWindowHostDelegateWayland::RootWindowHostDelegateWayland()
    : current_focus_window_(0),
      handle_event_(true),
      stop_propogation_(false),
      current_dispatcher_(NULL),
      current_capture_(NULL),
      current_active_window_(NULL),
      open_windows_(NULL),
      aura_windows_(NULL) {
  DCHECK(base::MessagePumpOzone::Current());
  base::MessagePumpOzone::Current()->AddDispatcherForRootWindow(this);
  OzoneDisplay::GetInstance()->SetWindowChangeObserver(this);
}

RootWindowHostDelegateWayland::~RootWindowHostDelegateWayland() {
}

void RootWindowHostDelegateWayland::OnRootWindowCreated(unsigned handle) {
  open_windows().push_back(handle);

  if (aura_windows_) {
    delete aura_windows_;
    aura_windows_ = NULL;
  }
}

void RootWindowHostDelegateWayland::OnRootWindowClosed(unsigned handle) {
  open_windows().remove(handle);
  if (!open_windows().size()) {
    delete open_windows_;
    open_windows_ = NULL;
    SetActiveWindow(NULL);

    DCHECK(base::MessagePumpOzone::Current());
    base::MessagePumpOzone::Current()->RemoveDispatcherForRootWindow(this);
    OzoneDisplay::GetInstance()->SetWindowChangeObserver(NULL);
  }

  if (aura_windows_) {
    delete aura_windows_;
    aura_windows_ = NULL;
  }

  if (!current_active_window_ || current_active_window_->window_ != handle ||
        !open_windows_) {
    return;
  }

  DCHECK(!current_active_window_->window_parent_);
  current_active_window_->HandleNativeWidgetActivationChanged(false);
  // Set first top level window in the list of open windows as dispatcher.
  // This is just a guess of the window which would eventually be focussed.
  // We should set the correct root window as dispatcher in OnWindowFocused.
  // This is needed to ensure we always have a dispatcher for RootWindow.
  const std::list<gfx::AcceleratedWidget>& windows = open_windows();
  DesktopRootWindowHostWayland* rootWindow =
      DesktopRootWindowHostWayland::GetHostForAcceleratedWidget(
          windows.front());
  SetActiveWindow(rootWindow);
  rootWindow->HandleNativeWidgetActivationChanged(true);
}

void RootWindowHostDelegateWayland::SetActiveWindow(
    DesktopRootWindowHostWayland* dispatcher) {
  current_active_window_ = dispatcher;
  current_dispatcher_ = current_active_window_;
  if (!current_active_window_)
    return;

  // Make sure the stacking order is correct. The activated window should be
  // first one in list of open windows.
  std::list<gfx::AcceleratedWidget>& windows = open_windows();
  DCHECK(windows.size());
  unsigned window_handle = current_active_window_->window_;
  if (windows.front() != window_handle) {
    windows.remove(window_handle);
    windows.insert(open_windows().begin(), window_handle);
  }

  current_active_window_->Activate();
}

DesktopRootWindowHostWayland*
RootWindowHostDelegateWayland::GetActiveWindow() const {
  return current_active_window_;
}

void RootWindowHostDelegateWayland::SetCapture(
    DesktopRootWindowHostWayland* dispatcher) {
  if (current_capture_)
    current_capture_->OnCaptureReleased();

  current_capture_ = dispatcher;
  stop_propogation_ = current_capture_ ? true : false;
  current_dispatcher_ = current_capture_;
  if (!current_dispatcher_)
    current_dispatcher_ = current_active_window_;
}

DesktopRootWindowHostWayland*
RootWindowHostDelegateWayland::GetCurrentCapture() const {
  return current_capture_;
}

std::vector<aura::Window*>&
RootWindowHostDelegateWayland::GetAllOpenWindows() {
  if (!aura_windows_) {
    std::list<gfx::AcceleratedWidget>& windows = open_windows();
    DCHECK(windows.size());
    aura_windows_ = new std::vector<aura::Window*>(windows.size());
    std::transform(
        windows.begin(), windows.end(), aura_windows_->begin(),
            DesktopRootWindowHostWayland::GetContentWindowForAcceleratedWidget);
  }

  return *aura_windows_;
}

////////////////////////////////////////////////////////////////////////////////
// RootWindowHostDelegateWayland, Private implementation:
void RootWindowHostDelegateWayland::DispatchMouseEvent(
         ui::MouseEvent* event) {
  if (handle_event_)
    current_dispatcher_->delegate_->OnHostMouseEvent(event);
  else if (event->type() == ui::ET_MOUSE_PRESSED)
    SetCapture(NULL);

  // Stop event propogation as this window is acting as event grabber. All
  // event's we create are "cancelable". If in future we use events that are not
  // cancelable, then a check for cancelable events needs to be added here.
  if (stop_propogation_)
    event->StopPropagation();
}

std::list<gfx::AcceleratedWidget>&
RootWindowHostDelegateWayland::open_windows() {
  if (!open_windows_)
    open_windows_ = new std::list<gfx::AcceleratedWidget>();

  return *open_windows_;
}

////////////////////////////////////////////////////////////////////////////////
// RootWindowHostDelegateWayland, MessageLoop::Dispatcher implementation:
bool RootWindowHostDelegateWayland::Dispatch(const base::NativeEvent& ne) {
  ui::EventType type = ui::EventTypeFromNative(ne);
  DCHECK(current_dispatcher_);

  switch (type) {
    case ui::ET_TOUCH_MOVED:
    case ui::ET_TOUCH_PRESSED:
    case ui::ET_TOUCH_CANCELLED:
    case ui::ET_TOUCH_RELEASED: {
      ui::TouchEvent* touchev = static_cast<ui::TouchEvent*>(ne);
      current_dispatcher_->delegate_->OnHostTouchEvent(touchev);
      break;
    }
    case ui::ET_KEY_PRESSED: {
      ui::KeyEvent* keydown_event = static_cast<ui::KeyEvent*>(ne);
      current_dispatcher_->delegate_->OnHostKeyEvent(keydown_event);
      break;
    }
    case ui::ET_KEY_RELEASED: {
      ui::KeyEvent* keyup_event = static_cast<ui::KeyEvent*>(ne);
      current_dispatcher_->delegate_->OnHostKeyEvent(keyup_event);
      break;
    }
    case ui::ET_MOUSEWHEEL: {
      ui::MouseWheelEvent* wheelev = static_cast<ui::MouseWheelEvent*>(ne);
      DispatchMouseEvent(wheelev);
      break;
    }
    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_DRAGGED:
    case ui::ET_MOUSE_PRESSED:
    case ui::ET_MOUSE_RELEASED:
    case ui::ET_MOUSE_ENTERED:
    case ui::ET_MOUSE_EXITED: {
      ui::MouseEvent* mouseev = static_cast<ui::MouseEvent*>(ne);
      DispatchMouseEvent(mouseev);
      break;
    }
    case ui::ET_SCROLL_FLING_START:
    case ui::ET_SCROLL_FLING_CANCEL:
    case ui::ET_SCROLL: {
      ui::ScrollEvent* scrollev = static_cast<ui::ScrollEvent*>(ne);
      current_dispatcher_->delegate_->OnHostScrollEvent(scrollev);
      break;
    }
    case ui::ET_UMA_DATA:
      break;
    case ui::ET_UNKNOWN:
      break;
    default:
      NOTIMPLEMENTED() << "RootWindowHostDelegateWayland: unknown event type.";
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// DesktopRootWindowHostWayland, WindowChangeObserver implementation:
void RootWindowHostDelegateWayland::OnWindowFocused(unsigned handle) {
  current_focus_window_ = handle;
  // Don't dispatch events in case a window has installed itself as capture
  // window but doesn't have the focus.
  handle_event_ = current_capture_ ? current_focus_window_ ==
          current_capture_->GetAcceleratedWidget() : true;
  if (current_active_window_->window_ == handle)
    return;

  // A new window should not steal focus in case the current window has a open
  // popup.
  if (current_capture_ && current_capture_ != current_active_window_)
    return;

  DesktopRootWindowHostWayland* window = NULL;
  if (handle)
    window = DesktopRootWindowHostWayland::GetHostForAcceleratedWidget(handle);

  if (!window || window->window_parent_)
    return;

  current_active_window_->HandleNativeWidgetActivationChanged(false);
  SetCapture(NULL);

  SetActiveWindow(window);
  window->HandleNativeWidgetActivationChanged(true);
}

void RootWindowHostDelegateWayland::OnWindowEnter(unsigned handle) {
  OnWindowFocused(handle);
}

void RootWindowHostDelegateWayland::OnWindowLeave(unsigned handle) {
}

}  // namespace ozonewayland