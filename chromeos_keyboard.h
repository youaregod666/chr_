// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_KEYBOARD_H_
#define CHROMEOS_KEYBOARD_H_

#include <map>
#include <string>
#include <vector>

namespace chromeos {

struct AutoRepeatRate {
  AutoRepeatRate() : initial_delay_in_ms(0), repeat_interval_in_ms(0) {}
  unsigned int initial_delay_in_ms;
  unsigned int repeat_interval_in_ms;
};

enum ModifierKey {
  kSearchKey = 0,
  kLeftControlKey,
  kLeftAltKey,
  kVoidKey,
  kCapsLockKey,

  kNumModifierKeys,
};

struct ModifierKeyPair {
  ModifierKeyPair(ModifierKey in_original, ModifierKey in_replacement)
      : original(in_original), replacement(in_replacement) {}
  bool operator==(const ModifierKeyPair& rhs) const {
    // For CheckMap() in chromeos_keyboard_unittest.cc.
    return (rhs.original == original) && (rhs.replacement == replacement);
  }
  ModifierKey original;      // Replace the key with
  ModifierKey replacement;   // this key.
};
typedef std::vector<ModifierKeyPair> ModifierMap;

// DEPRECATED. WILL BE REMOVED SOON.
// Returns the hardware layout name.
// TODO(satorux): Remove the function.
extern std::string (*GetHardwareKeyboardLayoutName)();

// DEPRECATED. WILL BE REMOVED SOON.
// Returns the current layout name like "us". On error, returns "".
// TODO(satorux): Remove the function.
extern std::string (*GetCurrentKeyboardLayoutName)();

// Sets the current keyboard layout to |layout_name|. This function does not
// change the current mapping of the modifier keys. Returns true on success.
extern bool (*SetCurrentKeyboardLayoutByName)(const std::string& layout_name);

// Remaps modifier keys. This function does not change the current keyboard
// layout. Returns true on success.
// Notice: For now, you can't remap Left Control and Left Alt keys to CapsLock.
extern bool (*RemapModifierKeys)(const ModifierMap& modifier_map);

// Gets the current auto-repeat mode of the keyboard. The result is stored in
// |out_mode|. Returns true on success.
extern bool (*GetAutoRepeatEnabled)(bool* enabled);

// Turns on and off the auto-repeat of the keyboard. Returns true on success.
extern bool (*SetAutoRepeatEnabled)(bool enabled);

// Gets the current auto-repeat rate of the keyboard. The result is stored in
// |out_rate|. Returns true on success.
extern bool (*GetAutoRepeatRate)(AutoRepeatRate* out_rate);

// Sets the auto-repeat rate of the keyboard, initial delay in ms, and repeat
// interval in ms.  Returns true on success.
extern bool (*SetAutoRepeatRate)(const AutoRepeatRate& rate);

//
// WARNING: DO NOT USE FUNCTIONS/CLASSES/TYPEDEFS BELOW. They are only for
// unittests. See the definitions in chromeos_keyboard.cc for details.
//
typedef std::map<std::string, ModifierMap> StringToModifierMap;

// Converts |key| to a modifier key name which is used in
// /usr/share/X11/xkb/symbols/chromeos.
inline std::string ModifierKeyToString(ModifierKey key) {
  switch (key) {
    case kSearchKey:
      return "search";
    case kLeftControlKey:
      return "leftcontrol";
    case kLeftAltKey:
      return "leftalt";
    case kVoidKey:
      return "disabled";
    case kCapsLockKey:
      return "capslock";
    case kNumModifierKeys:
      break;
  }
  return "";
}

// Creates a full XKB layout name like
//   "gb(extd)+chromeos(leftcontrol_disabled_leftalt)+version(v1_7_r7),us"
// from modifier key mapping and |layout_name|, such as "us", "us(dvorak)", and
// "gb(extd)". Returns an empty string on error.
// If |use_version| is false, the function does not add "+version(...)" to the
// layout name. See http://crosbug.com/6261 for details.
std::string CreateFullXkbLayoutName(const std::string& layout_name,
                                    const ModifierMap& modifire_map,
                                    bool use_version);

// Returns a layout name which is used in libcros from a full XKB layout name.
// On error, it returns an empty string.
// Example: "gb(extd)+chromeos(leftcontrol_disabled_leftalt),us" -> "gb(extd)"
std::string ExtractLayoutNameFromFullXkbLayoutName(
    const std::string& full_xkb_layout_name);

// Initializes a std::map that holds mappings like:
//   "leftcontrol_disabled_leftalt" ->
//     {{ kSearchKey -> kLeftControlKey },
//      { kLeftControlKey -> kVoidKey },
//      { kLeftAltKey -> kLeftAltKey }}
void InitializeStringToModifierMap(
    StringToModifierMap* out_string_to_modifier_map);

// Returns a mapping of modifier keys from a full XKB layout name. Returns true
// on success.
// Example: "gb(extd)+chromeos(leftcontrol_disabled_leftalt),us" ->
//     {{ kSearchKey -> kLeftControlKey },
//      { kLeftControlKey -> kVoidKey },
//      { kLeftAltKey -> kLeftAltKey }}
bool ExtractModifierMapFromFullXkbLayoutName(
    const std::string& full_xkb_layout_name,
    const StringToModifierMap& string_to_modifier_map,
    ModifierMap* out_modifier_map);

// Returns true if caps lock is enabled.
bool CapsLockIsEnabled();

// Sets the caps lock status to |enable_caps_lock|.
void SetCapsLockEnabled(bool enabled);

// Returns true if |key| is in |modifier_map| as replacement.
bool ContainsModifierKeyAsReplacement(
    const ModifierMap& modifier_map, ModifierKey key);

}  // namespace chromeos

#endif  // CHROMEOS_KEYBOARD_H_
