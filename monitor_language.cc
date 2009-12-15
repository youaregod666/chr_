// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/logging.h>
#include <base/scoped_ptr.h>
#include <dlfcn.h>
#include <glib-object.h>

#include <iostream>  // NOLINT

#include "chromeos_cros_api.h"  // NOLINT
#include "chromeos_language.h"
#include "monitor_utils.h" //NOLINT

// \file This is a simple console application which checks whether the cros
// library (chromeos_language.cc) can monitor IME/XKB status changes or not.

// How to use this tool:
// 1. Set up your IBus daemon using ibus-setup command. Add at least one IME.
// 2. Start the candidate_window for ChromeOS.
// 3. Start this tool ***in your gnome-terminal***. You need to have X desktop.
// 4. Verify that all IME languages and XKB layouts are displayed.
// 5. Focus another X application, then focus back the gnome-terminal in order
//    to make candidate_window send "FocusIn" signal to this process. Please
//    note that Callback::Run() is called upon "FocusIn" and "StateChanged"
//    signals from candidate_window.
// 6. Verify that this tool automatically exits in a second or so.

namespace {

const size_t kTestCount = 5;
chromeos::LanguageStatusConnection* global_connection = NULL;

}  // namespace

// Callback is an example object which can be passed to MonitorLanguageStatus.
class Callback {
 public:
  // You can store whatever state is needed in the function object.
  explicit Callback(GMainLoop* loop)
      : count_(0), loop_(loop) {
  }

  static void Run(void* object, const chromeos::InputLanguage& language) {
    Callback* self = static_cast<Callback*>(object);

    ++self->count_;
    if (self->count_ == kTestCount) {
      std::cout << "*** Done ***" << std::endl;
      ::g_main_loop_quit(self->loop_);
    } else {
      // Change the current IME engine or XKB layout by calling
      // ChromeOSChangeLanguage() function in libcros.
      //
      // 1. Calls the ChromeOSChangeLanguage() function.
      // 2. The function calls "SetEngine" method (if language->category is
      //    IME) or "Disable" method (if the category is XKB) in ibus-daemon.
      // 3. ibus-daemon changes its state and calls "StateChanged" method in
      //    candidate_window.
      // 4. candidate_window sends "StateChanged" signal to this process.
      // 5. Since |self| is registered as a monitor function, libcros calls
      //    Callback::Run() function again.
      //
      // As a result, "SetEngine" method and "Disable" method in ibus-daemon
      // are called in turn rapidly.
      if (language.category == chromeos::LANGUAGE_CATEGORY_XKB) {
        // This triggers the Run() function to be called again
        // (see the comment above).
        chromeos::ChangeLanguage(global_connection,
                                 chromeos::LANGUAGE_CATEGORY_IME,
                                 self->ime_id().c_str());
      } else {
        // Ditto.
        chromeos::ChangeLanguage(global_connection,
                                 chromeos::LANGUAGE_CATEGORY_XKB,
                                 self->xkb_id().c_str());
      }
    }
  }

  std::string xkb_id() const {
    return xkb_id_;
  }
  void set_xkb_id(const std::string& id) {
    xkb_id_ = id;
  }
  std::string ime_id() const {
    return ime_id_;
  }
  void set_ime_id(const std::string& id) {
    ime_id_ = id;
  }

 private:
  int count_;
  GMainLoop* loop_;
  std::string xkb_id_;
  std::string ime_id_;
};

int main(int argc, const char** argv) {
  // Initialize the g_type systems an g_main event loop, normally this would be
  // done by chrome.
  ::g_type_init();
  GMainLoop* loop = ::g_main_loop_new(NULL, false);
  DCHECK(LoadCrosLibrary(argv)) << "Failed to load cros.so";

  Callback callback(loop);
  global_connection
      = chromeos::MonitorLanguageStatus(&Callback::Run, &callback);
  DCHECK(global_connection) << "MonitorLanguageStatus() failed. "
                            << "candidate_window is not running?";

  const scoped_ptr<chromeos::InputLanguageList> engines(
      chromeos::GetLanguages(global_connection));
  DCHECK(engines.get()) << "GetLanguages() failed";

  std::cout << "Available IMEs and XKB layouts:" << std::endl;
  for (size_t i = 0; i < engines->size(); ++i) {
    const chromeos::InputLanguage &engine = engines->at(i);
    std::cout << "* " << engine.display_name << std::endl;
    // Remember (at least) one XKB id and one IME id.
    if (engine.category ==  chromeos::LANGUAGE_CATEGORY_XKB) {
      callback.set_xkb_id(engine.id);
    } else {
      callback.set_ime_id(engine.id);
    }
  }

  ::g_main_loop_run(loop);
  chromeos::DisconnectLanguageStatus(global_connection);
  ::g_main_loop_unref(loop);

  return 0;
}
