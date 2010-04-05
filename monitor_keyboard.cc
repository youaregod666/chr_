// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/logging.h>
#include <glib-object.h>
#include <gtest/gtest.h>

#include "chromeos_cros_api.h"  // NOLINT
#include "chromeos_keyboard.h"
#include "monitor_utils.h" //NOLINT

// \file This is a simple console application which checks whether
// keyboard layout switching functions in the cros library
// (chromeos_keyboard.cc) work as expected.
//
// WARNING: running the program can be dangerous as it changes the
// keyboard layout of the machine that runs the program.

TEST(ChromeOSKeyboardTest, KeyboardLayout) {
  // Remember the original layout setting.
  const std::string original_layout_name =
      chromeos::GetCurrentKeyboardLayoutName();
  ASSERT_FALSE(original_layout_name.empty());

  // We'll switch the layout to "jp", but if the original layout is set to
  // "jp", we'll use "fr" instead.
  std::string target_layout_name = "jp";
  if (original_layout_name == "jp") {
    target_layout_name = "fr";
  }
  ASSERT_TRUE(chromeos::SetCurrentKeyboardLayoutByName(target_layout_name));
  ASSERT_EQ(target_layout_name, chromeos::GetCurrentKeyboardLayoutName());

  // Restore the original lyaout.
  ASSERT_TRUE(chromeos::SetCurrentKeyboardLayoutByName(original_layout_name));
  ASSERT_EQ(original_layout_name, chromeos::GetCurrentKeyboardLayoutName());

  // Setting a bogus name should fail.
  ASSERT_FALSE(chromeos::SetCurrentKeyboardLayoutByName("fakefake"));
}

TEST(ChromeOSKeyboardTest, KeyboardLayoutPerWindow) {
  // Remember the original per window setting.
  bool original_is_per_window = false;
  ASSERT_TRUE(chromeos::GetKeyboardLayoutPerWindow(&original_is_per_window));

  // Reverse the per window setting.
  bool is_per_window = false;
  ASSERT_TRUE(chromeos::SetKeyboardLayoutPerWindow(!original_is_per_window));
  ASSERT_TRUE(chromeos::GetKeyboardLayoutPerWindow(&is_per_window));
  ASSERT_NE(original_is_per_window, is_per_window);

  // Restore the original per window setting.
  ASSERT_TRUE(chromeos::SetKeyboardLayoutPerWindow(original_is_per_window));
  ASSERT_TRUE(chromeos::GetKeyboardLayoutPerWindow(&is_per_window));
  ASSERT_EQ(original_is_per_window, is_per_window);
}

int main(int argc, char** argv) {
  g_type_init();
  CHECK(LoadCrosLibrary(const_cast<const char**>(argv)))
      << "Failed to load cros.so";
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
