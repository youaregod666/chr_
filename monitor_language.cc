// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/logging.h>
#include <base/scoped_ptr.h>
#include <dlfcn.h>
#include <glib-object.h>
#include <gtest/gtest.h>
#include <unistd.h>

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
// 5. Verify that the last IME is deactivated and then reactivated.
// 6. Focus another X application, then focus back the gnome-terminal in order
//    to make candidate_window send "FocusIn" signal to this process. Please
//    note that Callback::UpdateCurrentLanguage() is called upon "FocusIn" and
//    "StateChanged" signals from candidate_window. Likewise,
//    Callback::RegisterProperties() is for "RegisterProperties" signal and
//    Callback::UpdateProperty() is for "UpdateProperty" signal from the window.
// 7. Verify that this tool automatically exits in a second or so.
// 8. Verify that all TEST()s pass.

namespace {

const int kTestCount = 5;
chromeos::LanguageStatusConnection* global_connection = NULL;

TEST(GetSetImeConfigTest, String) {
  // Write a dummy value to "/desktop/ibus/dummy/dummy_string" in gconf.
  static const char kSection[] = "dummy";
  static const char kConfigName[] = "dummy_string";

  // Get current value. For the first run, GetImeConfig() will return false and
  // you might get warnings like:
  //  (process:1898): IBUS-WARNING **: org.freedesktop.DBus.Error.Failed:
  //                                     Can not get value [dummy->dummy_string]
  //  (process:1898): GLib-GObject-CRITICAL **: g_value_unset:
  //                                     assertion `G_IS_VALUE (value)' failed
  // But you can safely ignore these messages.
  std::string dummy_value = "ABCDE";
  chromeos::ImeConfigValue config;
  if (chromeos::GetImeConfig(global_connection, kSection, kConfigName, &config)
      && (config.string_value.length() == dummy_value.length())) {
    LOG(INFO) << "Current configuration of " << kSection << "/" << kConfigName
              << ": " << config.ToString();
    // Rotate the current string to the left.
    dummy_value = config.string_value.substr(1) + config.string_value[0];
  }

  // Set |dummy_value|.
  config.type = chromeos::ImeConfigValue::kValueTypeString;
  config.string_value = dummy_value;
  EXPECT_TRUE(chromeos::SetImeConfig(
      global_connection, kSection, kConfigName, config));

  // Get and compare.
  chromeos::ImeConfigValue updated_config;
  EXPECT_TRUE(chromeos::GetImeConfig(
      global_connection, kSection, kConfigName, &updated_config));
  EXPECT_EQ(updated_config.type, chromeos::ImeConfigValue::kValueTypeString);
  EXPECT_EQ(updated_config.string_value, dummy_value);
}

TEST(GetSetImeConfigTest, Int) {
  static const char kSection[] = "dummy";
  static const char kConfigName[] = "dummy_int";

  // Get current value.
  int dummy_value = 12345;
  chromeos::ImeConfigValue config;
  if (chromeos::GetImeConfig(global_connection, kSection, kConfigName, &config)
      && (config.type == chromeos::ImeConfigValue::kValueTypeInt)) {
    LOG(INFO) << "Current configuration of " << kSection << "/" << kConfigName
              << ": " << config.ToString();
    dummy_value = config.int_value + 1;
  }

  // Set |dummy_value|.
  config.type = chromeos::ImeConfigValue::kValueTypeInt;
  config.int_value = dummy_value;
  EXPECT_TRUE(chromeos::SetImeConfig(
      global_connection, kSection, kConfigName, config));

  // Get and compare.
  chromeos::ImeConfigValue updated_config;
  EXPECT_TRUE(chromeos::GetImeConfig(
      global_connection, kSection, kConfigName, &updated_config));
  EXPECT_EQ(updated_config.type, chromeos::ImeConfigValue::kValueTypeInt);
  EXPECT_EQ(updated_config.int_value, dummy_value);
}

TEST(GetSetImeConfigTest, Bool) {
  static const char kSection[] = "dummy";
  static const char kConfigName[] = "dummy_bool";

  // Get current value.
  bool dummy_value = true;
  chromeos::ImeConfigValue config;
  if (chromeos::GetImeConfig(global_connection, kSection, kConfigName, &config)
      && (config.type == chromeos::ImeConfigValue::kValueTypeBool)) {
    LOG(INFO) << "Current configuration of " << kSection << "/" << kConfigName
              << ": " << config.ToString();
    dummy_value = !config.bool_value;
  }

  // Set |dummy_value|.
  config.type = chromeos::ImeConfigValue::kValueTypeBool;
  config.bool_value = dummy_value;
  EXPECT_TRUE(chromeos::SetImeConfig(
      global_connection, kSection, kConfigName, config));

  // Get and compare.
  chromeos::ImeConfigValue updated_config;
  EXPECT_TRUE(chromeos::GetImeConfig(
      global_connection, kSection, kConfigName, &updated_config));
  EXPECT_EQ(updated_config.type, chromeos::ImeConfigValue::kValueTypeBool);
  EXPECT_EQ(updated_config.bool_value, dummy_value);
}

void DumpProperties(const chromeos::ImePropertyList& prop_list) {
  for (size_t i = 0; i < prop_list.size(); ++i) {
    DLOG(INFO) << "Property #" << i << ": " << prop_list[i].ToString();
  }
}

// Shows the active languages.
void ShowActiveLanguages() {
  const scoped_ptr<chromeos::InputLanguageList> languages(
      chromeos::GetActiveLanguages(global_connection));
  for (size_t i = 0; i < languages->size(); ++i) {
    const chromeos::InputLanguage &language = languages->at(i);
    LOG(INFO) << "* " << language.ToString();
  }
}

// Shows the supported languages.
void ShowSupportedLanguages() {
  const scoped_ptr<chromeos::InputLanguageList> languages(
      chromeos::GetSupportedLanguages(global_connection));
  for (size_t i = 0; i < languages->size(); ++i) {
    const chromeos::InputLanguage &language = languages->at(i);
    LOG(INFO) << "* " << language.ToString();
  }
}

// Callback is an example object which can be passed to MonitorLanguageStatus.
class Callback {
 public:
  // You can store whatever state is needed in the function object.
  explicit Callback(GMainLoop* loop)
      : count_(0), loop_(loop) {
  }

  static void UpdateCurrentLanguage(void* object,
                                    const chromeos::InputLanguage& language) {
    Callback* self = static_cast<Callback*>(object);

    ++self->count_;
    if (self->count_ == kTestCount) {
      LOG(INFO) << "*** Done ***";
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
      //    Callback::UpdateCurrentLanguage() function again.
      //
      // As a result, "SetEngine" method and "Disable" method in ibus-daemon
      // are called in turn rapidly.
      if (language.category == chromeos::LANGUAGE_CATEGORY_XKB) {
        // This triggers the UpdateCurrentLanguage() function to be called again
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

  static void RegisterProperties(void* object,
                                 const chromeos::ImePropertyList& prop_list) {
    DLOG(INFO) << "In callback function for the RegisterProperties signal";
    DumpProperties(prop_list);
  }

  static void UpdateProperty(void* object,
                             const chromeos::ImePropertyList& prop_list) {
    DLOG(INFO) << "In callback function for the UpdateProperty signal";
    DumpProperties(prop_list);
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

}  // namespace

int main(int argc, char** argv) {
  // Initialize the g_type systems an g_main event loop, normally this would be
  // done by chrome.
  ::g_type_init();
  GMainLoop* loop = ::g_main_loop_new(NULL, false);
  DCHECK(LoadCrosLibrary(const_cast<const char**>(argv)))
      << "Failed to load cros.so";

  chromeos::LanguageStatusMonitorFunctions monitor;
  monitor.current_language = &Callback::UpdateCurrentLanguage;
  monitor.register_ime_properties = &Callback::RegisterProperties;
  monitor.update_ime_property = &Callback::UpdateProperty;

  Callback callback(loop);
  global_connection
      = chromeos::MonitorLanguageStatus(monitor, &callback);
  DCHECK(global_connection) << "MonitorLanguageStatus() failed. "
                            << "candidate_window is not running?";

  // Check the connection status.
  DCHECK(chromeos::LanguageStatusConnectionIsAlive(global_connection))
      << "CheckConnection() failed.";
  LOG(INFO) << "Connection is OK.";

  // Try to disconnect, then reconnect.
  chromeos::DisconnectLanguageStatus(global_connection);
  global_connection
      = chromeos::MonitorLanguageStatus(monitor, &callback);
  DCHECK(global_connection) << "MonitorLanguageStatus() failed.";

  DCHECK(chromeos::LanguageStatusConnectionIsAlive(global_connection))
      << "CheckConnection() failed.";
  LOG(INFO) << "Connection is OK.";
  
  const scoped_ptr<chromeos::InputLanguageList> languages(
      chromeos::GetActiveLanguages(global_connection));
  DCHECK(languages.get()) << "GetActiveLanguages() failed";

  // Check if we have at least one IME. Languages are sorted in a way that
  // IMEs come after XKBs, so we check the last language.
  DCHECK(!languages->empty()) << "No activated languages";
  DCHECK(languages->back().category == chromeos::LANGUAGE_CATEGORY_IME)
      << "No IME found";

  LOG(INFO) << "---------------------";
  LOG(INFO) << "Supported IMEs and XKB layouts: ";
  ShowSupportedLanguages();
  LOG(INFO) << "---------------------";
  LOG(INFO) << "Activated IMEs and XKB layouts:";
  ShowActiveLanguages();
  LOG(INFO) << "---------------------";

  // Remember (at least) one XKB id and one IME id.
  for (size_t i = 0; i < languages->size(); ++i) {
    const chromeos::InputLanguage &language = languages->at(i);
    if (language.category ==  chromeos::LANGUAGE_CATEGORY_XKB) {
      callback.set_xkb_id(language.id);
    } else {
      callback.set_ime_id(language.id);
    }
  }

  // Deactivate the last language for testing.
  const chromeos::InputLanguage& language = languages->back();
  DCHECK(chromeos::SetLanguageActivated(global_connection,
                                        language.category,
                                        language.id.c_str(),
                                        false));
  // This is not reliable, but wait for a moment so the config change
  // takes effect in IBus.
  sleep(1);
  LOG(INFO) << "Deactivated: " << language.display_name;
  ShowActiveLanguages();

  // Reactivate the language.
  DCHECK(chromeos::SetLanguageActivated(global_connection,
                                        language.category,
                                        language.id.c_str(),
                                        true));
  sleep(1);
  LOG(INFO) << "Reactivated: " << language.display_name;
  ShowActiveLanguages();

  ::g_main_loop_run(loop);

  // Test if we can read/write IME configurations. This should be done before
  // |global_connection| is terminated.
  ::testing::InitGoogleTest(&argc, argv);
  const int result = RUN_ALL_TESTS();

  // TODO(yusukes): Add stress tests for GetImeConfig, GetSupportedLanguages,
  // SetLanguageActivated, and ChangeLanguage. We might have to implement a RSS
  // size checker. We should be aware of D-Bus memory management internals when
  // we write the tests: http://code.google.com/p/ibus/issues/detail?id=800

  chromeos::DisconnectLanguageStatus(global_connection);
  ::g_main_loop_unref(loop);
  return result;
}
