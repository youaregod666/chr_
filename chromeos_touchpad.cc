// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <sys/wait.h>

#include "base/logging.h"
#include "base/string_util.h"

namespace chromeos {  // NOLINT

namespace { // NOLINT
const char* kTpControl = "/opt/google/touchpad/tpcontrol";
}  // namespace


extern "C"
void ChromeOSSetTouchpadSensitivity(int value) {
  pid_t child_pid = fork();
  if (!child_pid) {
    std::string value_string = StringPrintf("%d", value);
    if (!daemon(1, 0)) {
      execl(kTpControl, kTpControl, "sensitivity", value_string.c_str(), NULL);
    }
    _exit(0);
  } else if (child_pid > 0) {
    int status = 0;
    waitpid(child_pid, &status, 0);
  }
}

extern "C"
void ChromeOSSetTouchpadTapToClick(bool enabled) {
  pid_t child_pid = fork();
  if (!child_pid) {
    if (!daemon(1, 0))
      execl(kTpControl, kTpControl, "taptoclick", enabled ? "on" : "off", NULL);
    _exit(0);
  } else if (child_pid > 0) {
    int status = 0;
    waitpid(child_pid, &status, 0);
  }
}

}  // namespace chromeos
