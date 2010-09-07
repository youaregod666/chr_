// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_touchpad.h"  // NOLINT

#include <string>

#include "base/logging.h"
#include "base/string_util.h"

namespace chromeos {  // NOLINT

namespace { // NOLINT
const char* kTpControl = "/opt/google/touchpad/tpcontrol";
}  // namespace


extern "C"
void ChromeOSSetTouchpadSensitivity(int value) {
  std::string command = StringPrintf("%s sensitivity %d", kTpControl, value);
  if (system(command.c_str()) < 0)
    LOG(WARNING) << "Got error while running \"" << command << "\"";
}

extern "C"
void ChromeOSSetTouchpadTapToClick(bool enabled) {
  std::string command =
      StringPrintf("%s taptoclick %s", kTpControl, enabled ? "on" : "off");
  if (system(command.c_str()) < 0)
    LOG(WARNING) << "Got error while running \"" << command << "\"";
}

}  // namespace chromeos
