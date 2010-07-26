// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_keyboard.h"

#include <X11/Xlib.h>
#include <base/logging.h>
#include <base/string_util.h>
#include <libxklavier/xklavier.h>
#include <stdlib.h>
#include <string.h>

#include "base/singleton.h"

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

// A singleton class which wraps the setxkbmap command.
class XKeyboard {
 public:
  // Returns the singleton instance of the class. Use LeakySingletonTraits.
  // We don't delete the instance at exit.
  static XKeyboard* Get() {
    return Singleton<XKeyboard, LeakySingletonTraits<XKeyboard> >::get();
  }

  bool SetLayout(const std::string& layout_name) {
    // Use CapsLock as the second Windows-L key so netbook users could open
    // and close NTP by pressing CapsLock.
    std::string layouts_to_set = layout_name + "+capslock(super)";
    if ((layouts_to_set.substr(0, 3) != "us+") &&
        (layouts_to_set.substr(0, 3) != "us(")) {
      layouts_to_set += ",us";
    }

    // Since executing setxkbmap takes more than 200 ms on EeePC, and this
    // function is called on every focus-in event, try to reduce the number of
    // the system() calls.
    if (last_layout_ == layouts_to_set) {
      return true;
    }

    std::string command = StringPrintf("setxkbmap -layout '%s'",
                                       layouts_to_set.c_str());
    const int status_code = system(command.c_str());
    if (status_code == 0) {
      XklEngineWrapper wrapper;
      if (wrapper.Init() &&
          !xkl_engine_is_group_per_toplevel_window(wrapper.xkl_engine())) {
        // Use caching only when XKB setting is not per-window.
        last_layout_ = layouts_to_set;
      }
      DLOG(INFO) << "XKB layout is changed to " << layouts_to_set;
      return true;
    }
    LOG(INFO) << "Failed to change XKB layout to: " << layouts_to_set;
    last_layout_.clear();  // invalidate the cache.
    return false;
  }

  const std::string GetLayout() {
    const char* cursor = NULL;
    char buf[1024];  // Big enough for setxkbmap output.

    if (!last_layout_.empty()) {
      cursor = last_layout_.c_str();
    } else {
      FILE* input = popen("setxkbmap -print", "r");
      if (!input) {
        LOG(ERROR) << "popen() failed";
        return "";
      }
      const size_t num_bytes = fread(buf, 1, sizeof(buf), input);
      pclose(input);
      if (num_bytes == 0) {
        LOG(ERROR) << "fread() failed (nothings is read)";
        return "";
      }
      // Parse a line like:
      // xkb_symbols   { include "pc+us+capslock(super)+inet(pc105)"};
      const char* cursor = strstr(buf, "pc+");
      if (!cursor) {
        LOG(ERROR) << "strstr() failed";
        return "";
      }
      cursor += 3;  // Skip "pc+".
    }

    const char* next_plus = strstr(cursor, "+");
    if (!next_plus) {
      LOG(ERROR) << "strstr() failed";
      return "";
    }
    const std::string layout_name = std::string(cursor, next_plus);
    LOG(INFO) << "Current XKB layout name: " << layout_name;
    return layout_name;
  }

 private:
  friend struct DefaultSingletonTraits<XKeyboard>;
  XKeyboard() {}
  ~XKeyboard() {}

  std::string last_layout_;  // ex "us+capslock(super)"

  DISALLOW_COPY_AND_ASSIGN(XKeyboard);
};

}  // namespace

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
  return XKeyboard::Get()->SetLayout(layout_name);
}

extern "C"
const std::string ChromeOSGetCurrentKeyboardLayoutName() {
  return XKeyboard::Get()->GetLayout();
}
