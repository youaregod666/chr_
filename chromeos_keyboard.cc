// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_keyboard.h"

#include <X11/Xlib.h>
#include <base/logging.h>
#include <base/string_util.h>
#include <libxklavier/xklavier.h>
#include <stdlib.h>

namespace {

// This is a wrapper class around XklEngine, that opens and closes X
// display as needed.
class XklEngineWrapper {
 public:
  XklEngineWrapper()
      : display_(NULL),
        xkl_engine_(NULL) {
  }

  // Initializes the object. Returns true on success.
  bool Init() {
    display_ = XOpenDisplay(NULL);
    if (!display_) {
      LOG(ERROR) << "XOpenDisplay() failed";
      return false;
    }
    xkl_engine_ = xkl_engine_get_instance(display_);
    if (!xkl_engine_) {
      LOG(ERROR) << "xkl_engine_get_instance() failed";
      return false;
    }
    return true;
  }

  ~XklEngineWrapper() {
    if (display_) {
      XCloseDisplay(display_);
    }
    // We don't destruct xkl_engine_ as it's a singleton.
  }

  XklEngine* xkl_engine() { return xkl_engine_; }

 private:
  Display* display_;
  XklEngine* xkl_engine_;
  DISALLOW_COPY_AND_ASSIGN(XklEngineWrapper);
};

extern "C"
bool ChromeOSSetKeyboardLayoutPerWindow(bool is_per_window) {
  XklEngineWrapper wrapper;
  if (!wrapper.Init()) {
    LOG(ERROR) << "XklEngineWrapper::Init() failed";
    return false;
  }

  xkl_engine_set_group_per_toplevel_window(wrapper.xkl_engine(),
                                           is_per_window);
  LOG(INFO) << "XKB layout per window setting is changed to: "
            << is_per_window;
  return true;
}

extern "C"
bool ChromeOSGetKeyboardLayoutPerWindow(bool* is_per_window) {
  CHECK(is_per_window);
  XklEngineWrapper wrapper;
  if (!wrapper.Init()) {
    LOG(ERROR) << "XklEngineWrapper::Init() failed";
    return false;
  }

  *is_per_window = xkl_engine_is_group_per_toplevel_window(
      wrapper.xkl_engine());
  LOG(INFO) << "XKB layout per window setting is: " << *is_per_window;
  return true;
}

extern "C"
bool ChromeOSSetCurrentKeyboardLayoutByName(const std::string& layout_name) {
  // TODO(satorux): Rewrite this function with xklavier.
  // The implementation is more like a prototype. Will rework later.
  std::string command = StringPrintf("setxkbmap -layout %s",
                                     layout_name.c_str());
  const int status_code = system(command.c_str());
  if (status_code == 0) {
    return true;
  }
  LOG(INFO) << "Failed to change XKB layout to: " << layout_name;
  return false;
}

extern "C"
const std::string ChromeOSGetCurrentKeyboardLayoutName() {
  // TODO(satorux): Rewrite this function with xklavier.
  // The implementation is more like a prototype. Will rework later.
  FILE* input = popen("setxkbmap -print", "r");
  if (!input) {
    LOG(ERROR) << "popen() failed";
    return "";
  }
  char buf[1024];  // Big enough for setxkbmap output.
  const size_t num_bytes = fread(buf, 1, sizeof(buf), input);
  pclose(input);
  if (num_bytes == 0) {
    LOG(ERROR) << "fread() failed (nothings is read)";
    return "";
  }

  // Prase a line like:
  // xkb_symbols   { include "pc+us+inet(evdev)"};
  const char* cursor = strstr(buf, "pc+");
  if (!cursor) {
    LOG(ERROR) << "strstr() failed";
    return "";
  }
  cursor += 3;  // Skip "pc+".
  const char* next_plus = strstr(cursor, "+");
  if (!next_plus) {
    LOG(ERROR) << "strstr() failed";
    return "";
  }
  const std::string layout_name = std::string(cursor, next_plus);
  LOG(INFO) << "Current XKB layout name: " << layout_name;
  return layout_name;
}

}  // namespace chromeos
