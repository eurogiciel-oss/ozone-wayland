// Copyright 2014 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OZONE_UI_IME_INPUT_METHOD_CONTEXT_FACTORY_OZONE_H_
#define OZONE_UI_IME_INPUT_METHOD_CONTEXT_FACTORY_OZONE_H_

#include "ozone/platform/ozone_export_wayland.h"
#include "ui/ozone/ime/input_method_context_factory_ozone.h"

namespace ui {

class OZONE_WAYLAND_EXPORT InputMethodContextFactoryWayland
    : public InputMethodContextFactoryOzone  {
 public:
  InputMethodContextFactoryWayland();
  virtual ~InputMethodContextFactoryWayland();

  virtual scoped_ptr<LinuxInputMethodContext> CreateInputMethodContext(
      ui::LinuxInputMethodContextDelegate* delegate) const OVERRIDE;
};

}  // namespace ui

#endif  // OZONE_UI_IME_INPUT_METHOD_CONTEXT_FACTORY_OZONE_H_
