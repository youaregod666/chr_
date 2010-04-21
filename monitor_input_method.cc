// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
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
#include "chromeos_input_method.h"
#include "monitor_utils.h" //NOLINT

// \file This is a simple console application which checks whether the cros
// library (chromeos_input_method.cc) can monitor input method status changes
// or not.

// How to use this tool:
// 1. Set up your IBus daemon using ibus-setup command. Add at least two input
//    methods.
// 2. Start the candidate_window for ChromeOS.
// 3. Start this tool ***in your gnome-terminal***. You need to have X desktop.
// 4. Verify that all input methods are displayed.
// 5. Verify that the last input method is deactivated and then reactivated.
// 6. Focus another X application, then focus back the gnome-terminal in order
//    to make candidate_window send "FocusIn" signal to this process. Please
//    note that Callback::UpdateCurrentInputMethod() is called upon "FocusIn"
//    and "StateChanged" signals from candidate_window. Likewise,
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

TEST(GetSetImeConfigTest, StringList) {
  // Write a dummy value to "/desktop/ibus/dummy/dummy_string" in gconf.
  static const char kSection[] = "dummy";
  static const char kConfigName[] = "dummy_string_list";
  chromeos::ImeConfigValue config;
  chromeos::ImeConfigValue updated_config;

  // Set a dummy list.
  config.type = chromeos::ImeConfigValue::kValueTypeStringList;
  config.string_list_value.push_back("1");
  EXPECT_TRUE(chromeos::SetImeConfig(
      global_connection, kSection, kConfigName, config));

  // Get and compare.
  EXPECT_TRUE(chromeos::GetImeConfig(
      global_connection, kSection, kConfigName, &updated_config));
  EXPECT_EQ(
      updated_config.type, chromeos::ImeConfigValue::kValueTypeStringList);
  ASSERT_EQ(updated_config.string_list_value.size(), 1);
  EXPECT_EQ(updated_config.string_list_value.at(0), "1");
  
  // Set a longer dummy list.
  config.type = chromeos::ImeConfigValue::kValueTypeStringList;
  config.string_list_value.clear();
  config.string_list_value.push_back("A");
  config.string_list_value.push_back("B");
  config.string_list_value.push_back("C");
  EXPECT_TRUE(chromeos::SetImeConfig(
      global_connection, kSection, kConfigName, config));

  // Get and compare.
  EXPECT_TRUE(chromeos::GetImeConfig(
      global_connection, kSection, kConfigName, &updated_config));
  EXPECT_EQ(
      updated_config.type, chromeos::ImeConfigValue::kValueTypeStringList);
  ASSERT_EQ(updated_config.string_list_value.size(), 3);
  EXPECT_EQ(updated_config.string_list_value.at(0), "A");
  EXPECT_EQ(updated_config.string_list_value.at(1), "B");
  EXPECT_EQ(updated_config.string_list_value.at(2), "C");
  
  // Set an empty list.
  config.type = chromeos::ImeConfigValue::kValueTypeStringList;
  config.string_list_value.clear();
  EXPECT_TRUE(chromeos::SetImeConfig(
      global_connection, kSection, kConfigName, config));

  // Get and compare.
  EXPECT_TRUE(chromeos::GetImeConfig(
      global_connection, kSection, kConfigName, &updated_config));
  EXPECT_EQ(
      updated_config.type, chromeos::ImeConfigValue::kValueTypeStringList);
  EXPECT_TRUE(updated_config.string_list_value.empty());
}

void DumpProperties(const chromeos::ImePropertyList& prop_list) {
  for (size_t i = 0; i < prop_list.size(); ++i) {
    DLOG(INFO) << "Property #" << i << ": " << prop_list[i].ToString();
  }
}

// Shows the active input methods.
void ShowActiveInputMethods() {
  const scoped_ptr<chromeos::InputMethodDescriptors> descriptors(
      chromeos::GetActiveInputMethods(global_connection));
  for (size_t i = 0; i < descriptors->size(); ++i) {
    const chromeos::InputMethodDescriptor &descriptor = descriptors->at(i);
    LOG(INFO) << "* " << descriptor.ToString();
  }
}

// Shows the supported input methods.
void ShowSupportedInputMethods() {
  const scoped_ptr<chromeos::InputMethodDescriptors> descriptors(
      chromeos::GetSupportedInputMethods(global_connection));
  for (size_t i = 0; i < descriptors->size(); ++i) {
    const chromeos::InputMethodDescriptor &descriptor = descriptors->at(i);
    LOG(INFO) << "* " << descriptor.ToString();
  }
}

// Callback is an example object which can be passed to
// MonitorInputMethodStatus.
class Callback {
 public:
  // You can store whatever state is needed in the function object.
  explicit Callback(GMainLoop* loop)
      : count_(0), loop_(loop) {
  }

  static void UpdateCurrentInputMethod(
      void* object, const chromeos::InputMethodDescriptor& descriptor) {
    DLOG(INFO) << "In UpdateCurrentInputMethod";
    Callback* self = static_cast<Callback*>(object);

    ++self->count_;
    if (self->count_ == kTestCount) {
      LOG(INFO) << "*** Done ***";
      ::g_main_loop_quit(self->loop_);
    } else {
      // Change the current input method by calling
      // ChromeOSChangeInputMethod() function in libcros.
      //
      // 1. Calls the ChromeOSChangeInputMethod() function.
      // 2. ibus-daemon changes its state and calls "StateChanged" method in
      //    candidate_window.
      // 3. candidate_window sends "StateChanged" signal to this process.
      // 4. Since |self| is registered as a monitor function, libcros calls
      //    Callback::UpdateCurrentInputMethod() function again.
      //
      // As a result, ChromeOSChangeInputMethod() API in libcros are called
      // many times rapidly.
      if (descriptor.id != self->first_ime_id_) {
        chromeos::ChangeInputMethod(
            global_connection, self->first_ime_id_.c_str());
      } else {
        chromeos::ChangeInputMethod(
            global_connection, self->second_ime_id_.c_str());
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

  static void FocusChanged(void* object, bool focus_in) {
    DLOG(INFO) << "In callback function for the FocusChanged: "
               << (focus_in ? "focus in" : "focus out");
  }

  void set_first_ime_id(const std::string& id) {
    first_ime_id_ = id;
  }
  void set_second_ime_id(const std::string& id) {
    second_ime_id_ = id;
  }

 private:
  int count_;
  GMainLoop* loop_;
  std::string first_ime_id_;
  std::string second_ime_id_;
};

}  // namespace

int main(int argc, char** argv) {
  // Initialize the g_type systems an g_main event loop, normally this would be
  // done by chrome.
  ::g_type_init();
  GMainLoop* loop = ::g_main_loop_new(NULL, false);
  DCHECK(LoadCrosLibrary(const_cast<const char**>(argv)))
      << "Failed to load cros.so";

  Callback callback(loop);
  global_connection
      = chromeos::MonitorInputMethodStatus(&callback,
                                           &Callback::UpdateCurrentInputMethod,
                                           &Callback::RegisterProperties,
                                           &Callback::UpdateProperty,
                                           &Callback::FocusChanged);
  DCHECK(global_connection) << "MonitorInputMethodStatus() failed. "
                            << "candidate_window is not running?";

  // Check the connection status.
  DCHECK(chromeos::InputMethodStatusConnectionIsAlive(global_connection))
      << "CheckConnection() failed.";
  LOG(INFO) << "Connection is OK.";

  // Try to disconnect, then reconnect.
  chromeos::DisconnectInputMethodStatus(global_connection);
  global_connection
      = chromeos::MonitorInputMethodStatus(&callback,
                                           &Callback::UpdateCurrentInputMethod,
                                           &Callback::RegisterProperties,
                                           &Callback::UpdateProperty,
                                           &Callback::FocusChanged);
  DCHECK(global_connection) << "MonitorInputMethodStatus() failed.";

  DCHECK(chromeos::InputMethodStatusConnectionIsAlive(global_connection))
      << "CheckConnection() failed.";
  LOG(INFO) << "Connection is OK.";
  
  const scoped_ptr<chromeos::InputMethodDescriptors> descriptors(
      chromeos::GetActiveInputMethods(global_connection));
  DCHECK(descriptors.get()) << "GetActiveInputMethods() failed";

  // Check if we have at least three input methods.
  DCHECK(!descriptors->empty()) << "No active input methods";
  DCHECK(descriptors->size() >= 3)
      << "Only one input method is found. "
      << "You have to activeate at least 3 input methods.";

  LOG(INFO) << "---------------------";
  LOG(INFO) << "Supported input methods: ";
  ShowSupportedInputMethods();
  LOG(INFO) << "---------------------";
  LOG(INFO) << "Activated input methods:";
  ShowActiveInputMethods();
  LOG(INFO) << "---------------------";

  callback.set_first_ime_id(descriptors->at(1).id);
  callback.set_second_ime_id(descriptors->at(2).id);

  // Deactivate the last input method for testing.
  chromeos::ImeConfigValue input_methods;
  input_methods.type = chromeos::ImeConfigValue::kValueTypeStringList;
  for (size_t i = 0; i < descriptors->size() - 1; ++i) {
    input_methods.string_list_value.push_back(descriptors->at(i).id);
  }
  DCHECK(chromeos::SetImeConfig(
      global_connection, "general", "preload_engines", input_methods));
  // This is not reliable, but wait for a moment so the config change
  // takes effect in IBus.
  sleep(1);
  LOG(INFO) << "Deactivated: " << descriptors->back().display_name;
  ShowActiveInputMethods();

  // Reactivate the input method.
  input_methods.type = chromeos::ImeConfigValue::kValueTypeStringList;
  input_methods.string_list_value.clear();
  for (size_t i = 0; i < descriptors->size(); ++i) {
    input_methods.string_list_value.push_back(descriptors->at(i).id);
  }
  DCHECK(chromeos::SetImeConfig(
      global_connection, "general", "preload_engines", input_methods));
  sleep(1);
  LOG(INFO) << "Reactivated: " << descriptors->back().display_name;
  ShowActiveInputMethods();

  ::g_main_loop_run(loop);

  // Test if we can read/write input method configurations. This should be done
  // before |global_connection| is terminated.
  ::testing::InitGoogleTest(&argc, argv);
  const int result = RUN_ALL_TESTS();

  // TODO(yusukes): Add stress tests for GetImeConfig, GetSupportedInputMethods,
  // SetInputMethodActivated, and ChangeInputMethod. We might have to implement
  // a RSS size checker.
  // We should be aware of D-Bus memory management internals when we write the
  // tests: http://code.google.com/p/ibus/issues/detail?id=800

  chromeos::DisconnectInputMethodStatus(global_connection);
  ::g_main_loop_unref(loop);
  return result;
}
