// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_keyboard.h"

#include <utility>

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
      XklEngineWrapper wrapper;
      if (wrapper.Init() &&
          !xkl_engine_is_group_per_toplevel_window(wrapper.xkl_engine())) {
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
bool ChromeOSRemapModifierKeys(const chromeos::ModifierMap& modifier_map) {
  return XKeyboard::Get()->RemapModifierKeys(modifier_map);
}

extern "C"
const std::string ChromeOSGetCurrentKeyboardLayoutName() {
  return XKeyboard::Get()->GetLayout();
}
