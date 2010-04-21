// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/logging.h>
#include <base/scoped_ptr.h>
#include <dlfcn.h>
#include <glib-object.h>

#include <iostream>  // NOLINT

#include "chromeos_cros_api.h"  // NOLINT
#include "chromeos_input_method_ui.h"
#include "monitor_utils.h" //NOLINT

// \file This is a simple console application which checks whether the cros
// library (chromeos_input_method_ui.cc) can monitor input method UI status
// changes or not.

// How to use this tool:
// 1. Set up your IBus daemon using ibus-setup command.
//    Add at least one input method.
// 2. Kill the IBus UI application (/usr/share/ibus/ui/gtk/main.py)
// 3. Start this tool ***in your gnome-terminal***. You need to have X desktop.
// 4. Verify that status messages are printed as you make actions like
//    clicking on other windows, and turning on/off the input method.
// 5. Verify that this tool exits after UpdateLookupTable() is called five
//    times.

namespace {

chromeos::InputMethodUiStatusConnection* global_connection = NULL;

// Callback is an example object which can be passed to
// MonitorInputMethodUiStatus.
class Callback {
 public:
  explicit Callback(GMainLoop* loop)
      : count_(0), loop_(loop) {
  }

  static void HideAuxiliaryText(void* object) {
    LOG(INFO) << "HideAuxiliaryText";
  }

  static void HideLookupTable(void* object) {
    LOG(INFO) << "HideLookupTable";
  }

  static void SetCursorLocation(void* object,
                                int x, int y, int width, int height) {
    LOG(INFO) << "SetCursorLocation: "
              << "x=" << x << ", "
              << "y=" << y << ", "
              << "width=" << width << ", "
              << "height=" << height;
  }

  static void UpdateAuxiliaryText(void* object,
                                const std::string& text,
                                bool visible) {
    LOG(INFO) << "UpdateAuxiliaryText: ["
              << text << "]: " << visible;
  }

  static void UpdateLookupTable(
      void* object,
      const chromeos::InputMethodLookupTable& table) {
    LOG(INFO) << "UpdateLookupTable: " << table.ToString();

    Callback* self = static_cast<Callback*>(object);
    ++self->count_;
    if (self->count_ == 5) {
      ::g_main_loop_quit(self->loop_);
    }
  }

 private:
  int count_;
  GMainLoop* loop_;
};

}  // namespace

int main(int argc, const char** argv) {
  // Initialize the g_type systems an g_main event loop, normally this would be
  // done by chrome.
  ::g_type_init();
  GMainLoop* loop = ::g_main_loop_new(NULL, false);
  DCHECK(LoadCrosLibrary(argv)) << "Failed to load cros.so";

  chromeos::InputMethodUiStatusMonitorFunctions monitor_functions;
  monitor_functions.hide_auxiliary_text = Callback::HideAuxiliaryText;
  monitor_functions.hide_lookup_table = Callback::HideLookupTable;
  monitor_functions.set_cursor_location = Callback::SetCursorLocation;
  monitor_functions.update_auxiliary_text = Callback::UpdateAuxiliaryText;
  monitor_functions.update_lookup_table = Callback::UpdateLookupTable;

  Callback callback(loop);
  global_connection
      = chromeos::MonitorInputMethodUiStatus(monitor_functions, &callback);
  DCHECK(global_connection) << "MonitorLanguageStatus() failed. ";

  ::g_main_loop_run(loop);
  chromeos::DisconnectInputMethodUiStatus(global_connection);
  ::g_main_loop_unref(loop);

  return 0;
}
