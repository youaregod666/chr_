// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// How to run the test:
//   $ FEATURES="test" emerge-x86-generic -a libcros

#include "chromeos_keyboard.h"

#include <algorithm>
#include <set>
#include <string>

#include <gtest/gtest.h>
#include <X11/Xlib.h>

#include "base/logging.h"

namespace chromeos {

namespace {

// Returns a ModifierMap object that contains the following mapping:
// - kSearchKey is mapped to |search|.
// - kLeftControl key is mapped to |control|.
// - kLeftAlt key is mapped to |alt|.
ModifierMap GetMap(ModifierKey search, ModifierKey control, ModifierKey alt) {
  ModifierMap modifier_key;
  // Use the Search key as |search|.
  modifier_key.push_back(ModifierKeyPair(kSearchKey, search));
  modifier_key.push_back(ModifierKeyPair(kLeftControlKey, control));
  modifier_key.push_back(ModifierKeyPair(kLeftAltKey, alt));
  return modifier_key;
}

// Checks |modifier_map| and returns true if the following conditions are met:
// - kSearchKey is mapped to |search|.
// - kLeftControl key is mapped to |control|.
// - kLeftAlt key is mapped to |alt|.
bool CheckMap(const ModifierMap& modifier_map,
              ModifierKey search, ModifierKey control, ModifierKey alt) {
  ModifierMap::const_iterator begin = modifier_map.begin();
  ModifierMap::const_iterator end = modifier_map.end();
  if ((std::count(begin, end, ModifierKeyPair(kSearchKey, search)) == 1) &&
      (std::count(begin, end,
                  ModifierKeyPair(kLeftControlKey, control)) == 1) &&
      (std::count(begin, end, ModifierKeyPair(kLeftAltKey, alt)) == 1)) {
    return true;
  }
  return false;
}

// Returns true if X display is available.
bool DisplayAvailable() {
  Display* display = XOpenDisplay(NULL);
  if (!display) {
    return false;
  }
  XCloseDisplay(display);
  return true;
}

}  // namespace

// Tests CreateFullXkbLayoutName() function.
TEST(ChromeOSKeyboardTest, TestCreateFullXkbLayoutNameBasic) {
  // CreateFullXkbLayoutName should not accept an empty |layout_name|.
  EXPECT_STREQ("", CreateFullXkbLayoutName(
      "", GetMap(kVoidKey, kVoidKey, kVoidKey), false).c_str());

  // CreateFullXkbLayoutName should not accept an empty ModifierMap.
  EXPECT_STREQ("", CreateFullXkbLayoutName("us", ModifierMap(), false).c_str());

  // CreateFullXkbLayoutName should not accept an incomplete ModifierMap.
  ModifierMap tmp_map = GetMap(kVoidKey, kVoidKey, kVoidKey);
  tmp_map.pop_back();
  EXPECT_STREQ("", CreateFullXkbLayoutName("us", tmp_map, false).c_str());

  // CreateFullXkbLayoutName should not accept redundant ModifierMaps.
  tmp_map = GetMap(kVoidKey, kVoidKey, kVoidKey);
  tmp_map.push_back(ModifierKeyPair(kSearchKey, kVoidKey));  // two search maps
  EXPECT_STREQ("", CreateFullXkbLayoutName("us", tmp_map, false).c_str());
  tmp_map = GetMap(kVoidKey, kVoidKey, kVoidKey);
  tmp_map.push_back(ModifierKeyPair(kLeftControlKey, kVoidKey));  // two ctrls
  EXPECT_STREQ("", CreateFullXkbLayoutName("us", tmp_map, false).c_str());
  tmp_map = GetMap(kVoidKey, kVoidKey, kVoidKey);
  tmp_map.push_back(ModifierKeyPair(kLeftAltKey, kVoidKey));  // two alts.
  EXPECT_STREQ("", CreateFullXkbLayoutName("us", tmp_map, false).c_str());

  // CreateFullXkbLayoutName should not accept invalid ModifierMaps.
  tmp_map = GetMap(kVoidKey, kVoidKey, kVoidKey);
  tmp_map.push_back(ModifierKeyPair(kVoidKey, kSearchKey));  // can't remap void
  EXPECT_STREQ("", CreateFullXkbLayoutName("us", tmp_map, false).c_str());
  tmp_map = GetMap(kVoidKey, kVoidKey, kVoidKey);
  tmp_map.push_back(ModifierKeyPair(kCapsLockKey, kSearchKey));  // ditto
  EXPECT_STREQ("", CreateFullXkbLayoutName("us", tmp_map, false).c_str());

  // CreateFullXkbLayoutName can remap Search/Ctrl/Alt to CapsLock.
  EXPECT_STREQ("us+chromeos(capslock_disabled_disabled)",
               CreateFullXkbLayoutName(
                   "us",
                   GetMap(kCapsLockKey, kVoidKey, kVoidKey), false).c_str());
  EXPECT_STREQ("us+chromeos(disabled_capslock_disabled)",
               CreateFullXkbLayoutName(
                   "us",
                   GetMap(kVoidKey, kCapsLockKey, kVoidKey), false).c_str());
  EXPECT_STREQ("us+chromeos(disabled_disabled_capslock)",
               CreateFullXkbLayoutName(
                   "us",
                   GetMap(kVoidKey, kVoidKey, kCapsLockKey), false).c_str());

  // CreateFullXkbLayoutName should not accept non-alphanumeric characters
  // except "()-_".
  EXPECT_STREQ("", CreateFullXkbLayoutName(
      "us!", GetMap(kVoidKey, kVoidKey, kVoidKey), false).c_str());
  EXPECT_STREQ("", CreateFullXkbLayoutName(
      "us; /bin/sh", GetMap(kVoidKey, kVoidKey, kVoidKey), false).c_str());
  EXPECT_STREQ("ab-c_12+chromeos(disabled_disabled_disabled),us",
               CreateFullXkbLayoutName(
                   "ab-c_12",
                   GetMap(kVoidKey, kVoidKey, kVoidKey), false).c_str());

  // CreateFullXkbLayoutName should not accept upper-case ascii characters.
  EXPECT_STREQ("", CreateFullXkbLayoutName(
      "US", GetMap(kVoidKey, kVoidKey, kVoidKey), false).c_str());

  // CreateFullXkbLayoutName should accept lower-case ascii characters.
  for (int c = 'a'; c <= 'z'; ++c) {
    EXPECT_STRNE("", CreateFullXkbLayoutName(
        std::string(3, c),
        GetMap(kVoidKey, kVoidKey, kVoidKey), false).c_str());
  }

  // CreateFullXkbLayoutName should accept numbers.
  for (int c = '0'; c <= '9'; ++c) {
    EXPECT_STRNE("", CreateFullXkbLayoutName(
        std::string(3, c),
        GetMap(kVoidKey, kVoidKey, kVoidKey), false).c_str());
  }

  // CreateFullXkbLayoutName should accept a layout with a variant name.
  EXPECT_STREQ("us(dvorak)+chromeos(disabled_disabled_disabled)",
               CreateFullXkbLayoutName(
                   "us(dvorak)",
                   GetMap(kVoidKey, kVoidKey, kVoidKey), false).c_str());
  EXPECT_STREQ("gb(extd)+chromeos(disabled_disabled_disabled),us",
               CreateFullXkbLayoutName(
                   "gb(extd)",
                   GetMap(kVoidKey, kVoidKey, kVoidKey), false).c_str());
  EXPECT_STREQ("gb(extd)+", CreateFullXkbLayoutName(
      "gb(extd)",
      GetMap(kVoidKey, kVoidKey, kVoidKey), false).substr(0, 9).c_str());
  EXPECT_STREQ("jp+", CreateFullXkbLayoutName(
      "jp", GetMap(kVoidKey, kVoidKey, kVoidKey), false).substr(0, 3).c_str());

  // When the layout name is not "us", the second layout should be added.
  EXPECT_EQ(-1, CreateFullXkbLayoutName(
      "us", GetMap(kVoidKey, kVoidKey, kVoidKey), false).find(",us"));
  EXPECT_EQ(-1, CreateFullXkbLayoutName(
      "us(dvorak)", GetMap(kVoidKey, kVoidKey, kVoidKey), false).find(",us"));
  EXPECT_NE(-1, CreateFullXkbLayoutName(
      "gb(extd)", GetMap(kVoidKey, kVoidKey, kVoidKey), false).find(",us"));
  EXPECT_NE(-1, CreateFullXkbLayoutName(
      "jp", GetMap(kVoidKey, kVoidKey, kVoidKey), false).find(",us"));

  // Check if |use_version| works as intended.
  EXPECT_NE(-1, CreateFullXkbLayoutName(
      "us", GetMap(kVoidKey, kVoidKey, kVoidKey), true).find("+version(v"));
}

// Tests if CreateFullXkbLayoutName and ExtractLayoutNameFromFullXkbLayoutName
// functions could handle all combinations of modifier remapping.
TEST(ChromeOSKeyboardTest, TestCreateFullXkbLayoutNameModifierKeys) {
  std::set<std::string> layouts;
  for (int i = 0; i < static_cast<int>(kNumModifierKeys); ++i) {
    for (int j = 0; j < static_cast<int>(kNumModifierKeys); ++j) {
      for (int k = 0; k < static_cast<int>(kNumModifierKeys); ++k) {
        const std::string layout = CreateFullXkbLayoutName(
            "us", GetMap(ModifierKey(i), ModifierKey(j), ModifierKey(k)), true);
        // CreateFullXkbLayoutName should succeed (i.e. should not return "".)
        EXPECT_STREQ("us+", layout.substr(0, 3).c_str())
            << "layout: " << layout;
        // All 4*3*3 layouts should be different.
        EXPECT_TRUE(layouts.insert(layout).second) << "layout: " << layout;
        // Round-trip conversion should be possible.
        EXPECT_STREQ(
            "us", ExtractLayoutNameFromFullXkbLayoutName(layout).c_str())
            << "layout: " << layout;
      }
    }
  }
}

// Tests ExtractLayoutNameFromFullXkbLayoutName() function.
TEST(ChromeOSKeyboardTest, TestExtractLayoutNameFromFullXkbLayoutName) {
  EXPECT_STREQ("us", ExtractLayoutNameFromFullXkbLayoutName(
      "us+chromeos(foo)").c_str());
  EXPECT_STREQ("us(dvorak)", ExtractLayoutNameFromFullXkbLayoutName(
      "us(dvorak)+chromeos(foo)").c_str());
  EXPECT_STREQ("", ExtractLayoutNameFromFullXkbLayoutName("").c_str());
  // ExtractLayoutNameFromFullXkbLayoutName should not accept layout without +.
  EXPECT_STREQ("", ExtractLayoutNameFromFullXkbLayoutName("us").c_str());
}

// Tests ExtractModifierMapFromFullXkbLayoutName() function.
TEST(ChromeOSKeyboardTest, TestExtractModifierMapFromFullXkbLayoutName) {
  StringToModifierMap string_to_modifier_map;
  InitializeStringToModifierMap(&string_to_modifier_map);

  ModifierMap modifier_map;
  EXPECT_FALSE(ExtractModifierMapFromFullXkbLayoutName(
      "", string_to_modifier_map, &modifier_map));
  EXPECT_FALSE(ExtractModifierMapFromFullXkbLayoutName(
      "us", string_to_modifier_map, &modifier_map));
  EXPECT_FALSE(ExtractModifierMapFromFullXkbLayoutName(
      "us(dvorak)", string_to_modifier_map, &modifier_map));
  EXPECT_FALSE(ExtractModifierMapFromFullXkbLayoutName(
      "us(dvorak)+", string_to_modifier_map, &modifier_map));
  EXPECT_FALSE(ExtractModifierMapFromFullXkbLayoutName(
      "us(dvorak)+chromeos(", string_to_modifier_map, &modifier_map));
  EXPECT_FALSE(ExtractModifierMapFromFullXkbLayoutName(
      "us(dvorak)+chromeos()", string_to_modifier_map, &modifier_map));
  EXPECT_FALSE(ExtractModifierMapFromFullXkbLayoutName(
      "us(dvorak)+chromeos(f", string_to_modifier_map, &modifier_map));
  EXPECT_FALSE(ExtractModifierMapFromFullXkbLayoutName(
      "us(dvorak)+chromeos(foo", string_to_modifier_map, &modifier_map));
  EXPECT_FALSE(ExtractModifierMapFromFullXkbLayoutName(
      "us(dvorak)+chromeos(foo)", string_to_modifier_map, &modifier_map));

  EXPECT_TRUE(ExtractModifierMapFromFullXkbLayoutName(
      "us(dvorak)+chromeos(disabled_disabled_disabled)",
      string_to_modifier_map, &modifier_map));
  EXPECT_EQ(3, modifier_map.size());
  EXPECT_TRUE(CheckMap(modifier_map, kVoidKey, kVoidKey, kVoidKey));

  EXPECT_TRUE(ExtractModifierMapFromFullXkbLayoutName(
      "us(dvorak)+chromeos(disabled_disabled_disabled)+",
      string_to_modifier_map, &modifier_map));
  EXPECT_EQ(3, modifier_map.size());
  EXPECT_TRUE(CheckMap(modifier_map, kVoidKey, kVoidKey, kVoidKey));

  EXPECT_TRUE(ExtractModifierMapFromFullXkbLayoutName(
      "us(dvorak)+chromeos(disabled_disabled_disabled)+inet(pc105)",
      string_to_modifier_map, &modifier_map));
  EXPECT_EQ(3, modifier_map.size());
  EXPECT_TRUE(CheckMap(modifier_map, kVoidKey, kVoidKey, kVoidKey));

  EXPECT_TRUE(ExtractModifierMapFromFullXkbLayoutName(
      "+chromeos(disabled_disabled_disabled)",
      string_to_modifier_map, &modifier_map));
  EXPECT_EQ(3, modifier_map.size());
  EXPECT_TRUE(CheckMap(modifier_map, kVoidKey, kVoidKey, kVoidKey));

  EXPECT_TRUE(ExtractModifierMapFromFullXkbLayoutName(
      "+chromeos(disabled_disabled_disabled)+inet(pc105)",
      string_to_modifier_map, &modifier_map));
  EXPECT_EQ(3, modifier_map.size());
  EXPECT_TRUE(CheckMap(modifier_map, kVoidKey, kVoidKey, kVoidKey));

  // Check all cases just in case.
  for (int i = 0; i < static_cast<int>(kNumModifierKeys); ++i) {
    for (int j = 0; j < static_cast<int>(kNumModifierKeys); ++j) {
      for (int k = 0; k < static_cast<int>(kNumModifierKeys); ++k) {
        const std::string layout = CreateFullXkbLayoutName(
            "us", GetMap(ModifierKey(i), ModifierKey(j), ModifierKey(k)), true);
        EXPECT_TRUE(ExtractModifierMapFromFullXkbLayoutName(
            layout, string_to_modifier_map, &modifier_map))
            << "layout: " << layout;
        EXPECT_EQ(3, modifier_map.size()) << "layout: " << layout;
        EXPECT_TRUE(CheckMap(
            modifier_map, ModifierKey(i), ModifierKey(j), ModifierKey(k)))
            << "layout: " << layout;
      }
    }
  }
}

TEST(ChromeOSKeyboardTest, TestSetCapsLockIsEnabled) {
  if (!DisplayAvailable()) {
    return;
  }
  const bool initial_lock_state = CapsLockIsEnabled();
  SetCapsLockEnabled(true);
  EXPECT_TRUE(CapsLockIsEnabled());
  SetCapsLockEnabled(false);
  EXPECT_FALSE(CapsLockIsEnabled());
  SetCapsLockEnabled(true);
  EXPECT_TRUE(CapsLockIsEnabled());
  SetCapsLockEnabled(false);
  EXPECT_FALSE(CapsLockIsEnabled());
  SetCapsLockEnabled(initial_lock_state);
}

TEST(ChromeOSKeyboardTest, TestContainsModifierKeyAsReplacement) {
  EXPECT_FALSE(ContainsModifierKeyAsReplacement(
      GetMap(kVoidKey, kVoidKey, kVoidKey), kCapsLockKey));
  EXPECT_TRUE(ContainsModifierKeyAsReplacement(
      GetMap(kCapsLockKey, kVoidKey, kVoidKey), kCapsLockKey));
  EXPECT_TRUE(ContainsModifierKeyAsReplacement(
      GetMap(kVoidKey, kCapsLockKey, kVoidKey), kCapsLockKey));
  EXPECT_TRUE(ContainsModifierKeyAsReplacement(
      GetMap(kVoidKey, kVoidKey, kCapsLockKey), kCapsLockKey));
  EXPECT_TRUE(ContainsModifierKeyAsReplacement(
      GetMap(kCapsLockKey, kCapsLockKey, kVoidKey), kCapsLockKey));
  EXPECT_TRUE(ContainsModifierKeyAsReplacement(
      GetMap(kCapsLockKey, kCapsLockKey, kCapsLockKey), kCapsLockKey));
  EXPECT_TRUE(ContainsModifierKeyAsReplacement(
      GetMap(kSearchKey, kVoidKey, kVoidKey), kSearchKey));
}

}  // namespace chromeos
