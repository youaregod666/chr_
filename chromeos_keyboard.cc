// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_keyboard.h"

#include <utility>

#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <glib.h>
#include <libxklavier/xklavier.h>
#include <stdlib.h>
#include <string.h>

#include "base/logging.h"
#include "base/singleton.h"
#include "base/string_util.h"

namespace {

// The command we use to set/get the current XKB layout and modifier key
// mapping.
const char kSetxkbmapCommand[] = "/usr/bin/setxkbmap";

// This is a wrapper class around Display, that opens and closes X display in
// the constructor and destructor.
class ScopedDisplay {
 public:
  explicit ScopedDisplay(Display* display) : display_(display) {
    if (!display_) {
      LOG(ERROR) << "NULL display_ is passed";
    }
  }

  ~ScopedDisplay() {
    if (display_) {
      XCloseDisplay(display_);
    }
  }

  Display* get() const {
    return display_;
  }

 private:
  Display* display_;

  DISALLOW_COPY_AND_ASSIGN(ScopedDisplay);
};

// A singleton class which wraps the setxkbmap command.
class XKeyboard {
 public:
  // Returns the singleton instance of the class. Use LeakySingletonTraits.
  // We don't delete the instance at exit.
  static XKeyboard* Get() {
    return Singleton<XKeyboard, LeakySingletonTraits<XKeyboard> >::get();
  }

  // Sets the current keyboard layout to |layout_name|. This function does not
  // change the current mapping of the modifier keys. Returns true on success.
  bool SetLayout(const std::string& layout_name) {
    // TODO(yusukes): write auto tests for the function.
    chromeos::ModifierMap modifier_map;
    if (!GetModifierMapping(&modifier_map)) {
      return false;
    }
    return SetLayoutInternal(layout_name, modifier_map);
  }

  // Remaps modifier keys. This function does not change the current keyboard
  // layout. Returns true on success.
  bool RemapModifierKeys(const chromeos::ModifierMap& modifier_map) {
    // TODO(yusukes): write auto tests for the function.
    const std::string layout_name = GetLayout();
    if (layout_name.empty()) {
      return false;
    }
    return SetLayoutInternal(layout_name, modifier_map);
  }

  // Returns the current layout name like "us". On error, returns "".
  std::string GetLayout() {
    // TODO(yusukes): write auto tests for the function.
    std::string command_output = last_full_layout_name_;

    if (command_output.empty()) {
      // Cache is not available. Execute setxkbmap to get the current layout.
      if (!ExecuteGetLayoutCommand(&command_output)) {
        return "";
      }
    }

    const std::string layout_name =
        chromeos::ExtractLayoutNameFromFullXkbLayoutName(command_output);
    LOG(INFO) << "Current XKB layout name: " << layout_name;
    return layout_name;
  }

  // Gets whehter we have separate keyboard layout per window, or not. The
  // result is stored in |is_per_window|.  Returns true on success.
  bool GetKeyboardLayoutPerWindow(bool* is_per_window) {
    DCHECK(is_per_window);
    ScopedDisplay display(XOpenDisplay(NULL));  // A connection to the X server.
    if (!display.get()) {
      return false;
    }
    XklEngine* xkl_engine = xkl_engine_get_instance(display.get());
    if (!xkl_engine) {
      LOG(ERROR) << "Can't get XklEngine instance";
      return false;
    }
    *is_per_window = xkl_engine_is_group_per_toplevel_window(xkl_engine);
    return true;
  }

  // Sets whether we have separate keyboard layout per window, or not. If false
  // is given, the same keyboard layout will be shared for all applications.
  // Returns true on success.
  bool SetKeyboardLayoutPerWindow(bool is_per_window) {
    ScopedDisplay display(XOpenDisplay(NULL));
    if (!display.get()) {
      return false;
    }
    XklEngine* xkl_engine = xkl_engine_get_instance(display.get());
    if (!xkl_engine) {
      LOG(ERROR) << "Can't get XklEngine instance";
      return false;
    }
    xkl_engine_set_group_per_toplevel_window(xkl_engine, is_per_window);
    return true;
  }

  // Gets the current auto-repeat mode of the keyboard. The result is stored in
  // |mode|. Returns true on success.
  bool GetAutoRepeatEnabled(bool* enabled) {
    // TODO(yusukes): write auto tests for the function.
    DCHECK(enabled);
    ScopedDisplay display(XOpenDisplay(NULL));
    if (!display.get()) {
      return false;
    }
    XKeyboardState values = {};
    XGetKeyboardControl(display.get(), &values);
    if (values.global_auto_repeat == 0) {
      *enabled = false;
    } else {
      *enabled = true;
    }
    return true;
  }

  // Turns on and off the auto-repeat of the keyboard. Returns true on success.
  bool SetAutoRepeatEnabled(bool enabled) {
    // TODO(yusukes): write auto tests for the function.
    ScopedDisplay display(XOpenDisplay(NULL));
    if (!display.get()) {
      return false;
    }
    if (enabled) {
      XAutoRepeatOn(display.get());
    } else {
      XAutoRepeatOff(display.get());
    }
    DLOG(INFO) << "Set auto-repeat mode to: " << (enabled ? "on" : "off");
    return true;
  }

  // Gets the current auto-repeat rate of the keyboard. The result is stored in
  // |out_rate|. Returns true on success.
  bool GetAutoRepeatRate(chromeos::AutoRepeatRate* out_rate) {
    // TODO(yusukes): write auto tests for the function.
    ScopedDisplay display(XOpenDisplay(NULL));
    if (!display.get()) {
      return false;
    }
    // XkbDescPtr is defined in /usr/include/X11/extensions/XKBstr.h.
    XkbDescPtr xkb = XkbAllocKeyboard();
    if (!xkb) {
      return false;
    }

    bool successful = false;
    if (XkbGetControls(
            display.get(), XkbRepeatKeysMask, xkb) == Success) {
      out_rate->initial_delay_in_ms = xkb->ctrls->repeat_delay;
      out_rate->repeat_interval_in_ms = xkb->ctrls->repeat_interval;
      successful = true;
    } else {
      out_rate->initial_delay_in_ms = 0;
      out_rate->repeat_interval_in_ms = 0;
    }
    XkbFreeKeyboard(xkb, 0, True /* free_all */);
    return successful;
  }

  // Sets the auto-repeat rate of the keyboard, initial delay in ms, and repeat
  // interval in ms.  Returns true on success.
  bool SetAutoRepeatRate(const chromeos::AutoRepeatRate& rate) {
    // TODO(yusukes): write auto tests for the function.
    ScopedDisplay display(XOpenDisplay(NULL));
    if (!display.get()) {
      return false;
    }
    XkbDescPtr xkb = XkbAllocKeyboard();
    if (!xkb) {
      return false;
    }

    bool successful = false;
    if (XkbGetControls(
            display.get(), XkbRepeatKeysMask, xkb) == Success) {
      xkb->ctrls->repeat_delay = rate.initial_delay_in_ms;
      xkb->ctrls->repeat_interval = rate.repeat_interval_in_ms;
      if (XkbSetControls(display.get(), XkbRepeatKeysMask, xkb) == True) {
        successful = true;
      }
    }
    XkbFreeKeyboard(xkb, 0, True /* free_all */);

    DLOG(INFO) << "Set auto-repeat rate to: "
               << rate.initial_delay_in_ms << " ms delay, "
               << rate.repeat_interval_in_ms << " ms interval";
    if (!successful) {
      LOG(ERROR) << "Failed to set auto-repeat rate";
    }
    return successful;
  }

 private:
  friend struct DefaultSingletonTraits<XKeyboard>;

  XKeyboard() {
    InitializeStringToModifierMap(&string_to_modifier_map_);
  }
  ~XKeyboard() {
  }

  // Gets the current modifier mapping and stores them on |out_modifier_map|.
  bool GetModifierMapping(chromeos::ModifierMap* out_modifier_map) {
    out_modifier_map->clear();

    std::string command_output = last_full_layout_name_;
    if (command_output.empty()) {
      // Cache is not available. Execute setxkbmap to get the current mapping.
      if (!ExecuteGetLayoutCommand(&command_output)) {
        return false;
      }
    }
    if (!chromeos::ExtractModifierMapFromFullXkbLayoutName(
            command_output, string_to_modifier_map_, out_modifier_map)) {
      return false;
    }
    return true;
  }

  // This function is used by SetLayout() and RemapModifierKeys(). Calls
  // setxkbmap command if needed, and updates the last_full_layout_name_ cache.
  bool SetLayoutInternal(const std::string& layout_name,
                         const chromeos::ModifierMap& modifier_map) {
    const std::string layouts_to_set =
        chromeos::CreateFullXkbLayoutName(layout_name, modifier_map);
    if (layouts_to_set.empty()) {
      return false;
    }

    // Since executing setxkbmap takes more than 200 ms on EeePC, and this
    // function is called on every focus-in event, try to reduce the number of
    // the fork/exec calls.
    if (last_full_layout_name_ == layouts_to_set) {
      DLOG(INFO) << "The requested layout is already set: " << layouts_to_set;
      return true;
    }

    gint exit_status = -1;
    const gboolean successful =
        ExecuteSetLayoutCommand(layouts_to_set, &exit_status);

    // On success, update the cache and return true.
    if (successful && (exit_status == 0)) {
      bool is_per_window = true;
      if (GetKeyboardLayoutPerWindow(&is_per_window) && !is_per_window) {
        // Use caching only when XKB setting is not per-window.
        last_full_layout_name_ = layouts_to_set;
      }
      DLOG(INFO) << "XKB layout is changed to " << layouts_to_set;
      return true;
    }

    LOG(ERROR) << "Failed to change XKB layout to: " << layouts_to_set;
    last_full_layout_name_.clear();  // invalidate the cache.
    return false;
  }

  // Executes 'setxkbmap -layout ...' command. Returns true if execve suceeds.
  bool ExecuteSetLayoutCommand(const std::string& layouts_to_set,
                               gint* out_exit_status) {
    *out_exit_status = -1;

    gchar* argv[] = {
      g_strdup(kSetxkbmapCommand), g_strdup("-layout"),
      g_strdup(layouts_to_set.c_str()), NULL
    };
    GError* error = NULL;
    const gboolean successful = g_spawn_sync(NULL, argv, NULL,
                                             static_cast<GSpawnFlags>(0),
                                             NULL, NULL, NULL, NULL,
                                             out_exit_status, &error);
    for (size_t i = 0; argv[i] != NULL; ++i) {
      g_free(argv[i]);
    }

    if (!successful) {
      if (error && error->message) {
        LOG(ERROR) << "Failed to execute setxkbmap: " << error->message;
      }
      g_error_free(error);
    }
    return (successful == TRUE);
  }

  // Executes 'setxkbmap -print' command and parses the output (stdout) from the
  // command. Returns true if both execve and parsing the output succeed.
  // On success, it stores a string like "us+chromeos(...)+..." on
  // |out_command_output|.
  bool ExecuteGetLayoutCommand(std::string* out_command_output) {
    out_command_output->clear();

    gint exit_status = -1;
    gchar* argv[] = { g_strdup(kSetxkbmapCommand), g_strdup("-print"), NULL };
    gchar* raw_command_output = NULL;
    GError* error = NULL;
    const gboolean successful = g_spawn_sync(NULL, argv, NULL,
                                             static_cast<GSpawnFlags>(0),
                                             NULL, NULL,
                                             &raw_command_output, NULL,
                                             &exit_status, &error);
    for (size_t i = 0; argv[i] != NULL; ++i) {
      g_free(argv[i]);
    }

    if (!successful) {
      if (error && error->message) {
        LOG(ERROR) << "Failed to execute setxkbmap: " << error->message;
      }
      g_error_free(error);
      return false;
    }

    // g_spawn_sync succeeded.
    std::string command_output = raw_command_output ? raw_command_output : "";
    g_free(raw_command_output);
    raw_command_output = NULL;  // DO NOT USE |raw_command_output| below.

    if (exit_status != 0) {
      return false;
    }
    // Parse a line like:
    //   "xkb_symbols { include "pc+us+chromeos(...)+inet(pc105)" };"
    const size_t cursor = command_output.find("pc+");
    if (cursor == std::string::npos) {
      LOG(ERROR) << "pc+ is not found: " << command_output;
      return false;
    }
    *out_command_output = command_output.substr(cursor + 3);  // Skip "pc+".
    return true;
  }

  // The XKB layout name which we set last time like
  // "us+chromeos(search_leftcontrol_leftalt)".
  std::string last_full_layout_name_;

  // A std::map that holds mappings like: "leftcontrol_disabled_leftalt" ->
  //   { LEFT_CONTROL_KEY, VOID_KEY, LEFT_ALT_KEY }.
  chromeos::StringToModifierMap string_to_modifier_map_;

  DISALLOW_COPY_AND_ASSIGN(XKeyboard);
};

}  // namespace

namespace chromeos {

std::string CreateFullXkbLayoutName(const std::string& layout_name,
                                    const ModifierMap& modifier_map) {
  static const char kValidLayoutNameCharacters[] =
      "abcdefghijklmnopqrstuvwxyz0123456789()-_";

  if (layout_name.empty()) {
    LOG(ERROR) << "Invalid layout_name: " << layout_name;
    return "";
  }

  if (layout_name.find_first_not_of(kValidLayoutNameCharacters) !=
      std::string::npos) {
    LOG(ERROR) << "Invalid layout_name: " << layout_name;
    return "";
  }

  std::string use_search_key_as_str;
  std::string use_left_control_key_as_str;
  std::string use_left_alt_key_as_str;

  for (size_t i = 0; i < modifier_map.size(); ++i) {
    std::string* target = NULL;
    switch (modifier_map[i].original) {
      case kSearchKey:
        target = &use_search_key_as_str;
        break;
      case kLeftControlKey:
        target = &use_left_control_key_as_str;
        break;
      case kLeftAltKey:
        target = &use_left_alt_key_as_str;
        break;
      default:
        break;
    }
    if (!target) {
      LOG(ERROR) << "We don't support remaping "
                 << ModifierKeyToString(modifier_map[i].original);
      return "";
    }
    if (!(target->empty())) {
      LOG(ERROR) << ModifierKeyToString(modifier_map[i].original)
                 << " appeared twice";
      return "";
    }
    *target = ModifierKeyToString(modifier_map[i].replacement);
  }

  if (use_search_key_as_str.empty() ||
      use_left_control_key_as_str.empty() ||
      use_left_alt_key_as_str.empty()) {
    LOG(ERROR) << "Incomplete ModifierMap: size=" << modifier_map.size();
    return "";
  }

  std::string full_xkb_layout_name =
      StringPrintf("%s+chromeos(%s_%s_%s)", layout_name.c_str(),
                   use_search_key_as_str.c_str(),
                   use_left_control_key_as_str.c_str(),
                   use_left_alt_key_as_str.c_str());

  if ((full_xkb_layout_name.substr(0, 3) != "us+") &&
      (full_xkb_layout_name.substr(0, 3) != "us(")) {
    full_xkb_layout_name += ",us";
  }

  return full_xkb_layout_name;
}

std::string ExtractLayoutNameFromFullXkbLayoutName(
    const std::string& full_xkb_layout_name) {
  const size_t next_plus_pos = full_xkb_layout_name.find('+');
  if (next_plus_pos == std::string::npos) {
    LOG(ERROR) << "Bad layout name: " << full_xkb_layout_name;
    return "";
  }
  return full_xkb_layout_name.substr(0, next_plus_pos);
}

void InitializeStringToModifierMap(StringToModifierMap* out_map) {
  DCHECK(out_map);
  out_map->clear();

  for (int i = 0; i < static_cast<int>(kNumModifierKeys); ++i) {
    for (int j = 0; j < static_cast<int>(kNumModifierKeys); ++j) {
      for (int k = 0; k < static_cast<int>(kNumModifierKeys); ++k) {
        const std::string string_rep = StringPrintf(
            "%s_%s_%s",
            ModifierKeyToString(ModifierKey(i)).c_str(),
            ModifierKeyToString(ModifierKey(j)).c_str(),
            ModifierKeyToString(ModifierKey(k)).c_str());
        ModifierMap modifier_map;
        modifier_map.push_back(ModifierKeyPair(kSearchKey, ModifierKey(i)));
        modifier_map.push_back(
            ModifierKeyPair(kLeftControlKey, ModifierKey(j)));
        modifier_map.push_back(ModifierKeyPair(kLeftAltKey, ModifierKey(k)));
        out_map->insert(make_pair(string_rep, modifier_map));
      }
    }
  }
}

bool ExtractModifierMapFromFullXkbLayoutName(
    const std::string& full_xkb_layout_name,
    const StringToModifierMap& string_to_modifier_map,
    ModifierMap* out_modifier_map) {
  static const char kMark[] = "+chromeos(";
  const size_t kMarkLen = strlen(kMark);

  out_modifier_map->clear();
  const size_t mark_pos = full_xkb_layout_name.find(kMark);
  if (mark_pos == std::string::npos) {
    LOG(ERROR) << "Bad layout name: " << full_xkb_layout_name;
    return false;
  }

  const std::string tmp =  // e.g. "leftcontrol_disabled_leftalt), us"
      full_xkb_layout_name.substr(mark_pos + kMarkLen);
  const size_t next_paren_pos = tmp.find(')');
  if (next_paren_pos == std::string::npos) {
    LOG(ERROR) << "Bad layout name: " << full_xkb_layout_name;
    return false;
  }

  const std::string modifier_map_string = tmp.substr(0, next_paren_pos);
  DLOG(INFO) << "Modifier mapping is: " << modifier_map_string;

  StringToModifierMap::const_iterator iter =
      string_to_modifier_map.find(modifier_map_string);
  if (iter == string_to_modifier_map.end()) {
    LOG(ERROR) << "Bad mapping name '" << modifier_map_string
               << "' in layout name '" << full_xkb_layout_name << "'";
    return false;
  }

  *out_modifier_map = iter->second;
  return true;
}

}  // namespace chromeos


//
// licros APIs.
//
extern "C"
bool ChromeOSSetKeyboardLayoutPerWindow(bool is_per_window) {
  return XKeyboard::Get()->SetKeyboardLayoutPerWindow(is_per_window);
}

extern "C"
bool ChromeOSGetKeyboardLayoutPerWindow(bool* is_per_window) {
  return XKeyboard::Get()->GetKeyboardLayoutPerWindow(is_per_window);
}

extern "C"
bool ChromeOSSetCurrentKeyboardLayoutByName(const std::string& layout_name) {
  return XKeyboard::Get()->SetLayout(layout_name);
}

extern "C"
bool ChromeOSRemapModifierKeys(const chromeos::ModifierMap& modifier_map) {
  return XKeyboard::Get()->RemapModifierKeys(modifier_map);
}

extern "C"
const std::string ChromeOSGetCurrentKeyboardLayoutName() {
  return XKeyboard::Get()->GetLayout();
}

extern "C"
bool ChromeOSGetAutoRepeatEnabled(bool* enabled) {
  return XKeyboard::Get()->GetAutoRepeatEnabled(enabled);
}

extern "C"
bool ChromeOSSetAutoRepeatEnabled(bool enabled) {
  return XKeyboard::Get()->SetAutoRepeatEnabled(enabled);
}

extern "C"
bool ChromeOSGetAutoRepeatRate(chromeos::AutoRepeatRate* out_rate) {
  return XKeyboard::Get()->GetAutoRepeatRate(out_rate);
}

extern "C"
bool ChromeOSSetAutoRepeatRate(const chromeos::AutoRepeatRate& rate) {
  return XKeyboard::Get()->SetAutoRepeatRate(rate);
}
