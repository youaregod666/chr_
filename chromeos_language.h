// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LANGUAGE_H_
#define CHROMEOS_LANGUAGE_H_

#include <sstream>
#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/logging.h>  // DCHECK

static const char kFallbackXKBId[] = "USA";
static const char kFallbackXKBDisplayName[] = "US";
static const int kInvalidSelectionItemId = -1;

namespace chromeos {

enum LanguageCategory {
  LANGUAGE_CATEGORY_XKB,
  LANGUAGE_CATEGORY_IME,
};

// A structure which represents an IME language or a XKB layout.
struct InputLanguage {
  InputLanguage(LanguageCategory in_category,
                const std::string& in_id,
                const std::string& in_display_name,
                const std::string& in_icon_path)
      : category(in_category),
        id(in_id),
        display_name(in_display_name),
        icon_path(in_icon_path) {
  }

  InputLanguage() : category(LANGUAGE_CATEGORY_XKB) {
  }

  // Languages are sorted by category, then by display_name, then by id.
  bool operator<(const InputLanguage& other) const {
    if (category == other.category) {
      if (display_name == other.display_name) {
        return id < other.id;
      }
      return display_name < other.display_name;
    }
    return category < other.category;
  }

  bool operator==(const InputLanguage& other) const {
    return (category == other.category) && (id == other.id);
  }

  LanguageCategory category;

  // An ID that identifies an IME engine or a XKB layout (e.g., "anthy",
  // "t:latn-post", "chewing").
  std::string id;
  // An IME or layout name which is used in the UI (e.g., "Anthy").
  std::string display_name;
  // Path to an icon (e.g., "/usr/share/ibus-chewing/icons/ibus-chewing.png").
  // Empty if it does not exist.
  std::string icon_path;
};
typedef std::vector<InputLanguage> InputLanguageList;

// A structure which represents a property for an IME engine. For details,
// please check a comment for the LanguageRegisterImePropertiesFunction typedef
// below.
struct ImeProperty {
  // TODO(yusukes): it might be better to use protocol buffers rather than the
  //                plain C++ struct.
  ImeProperty(const std::string& in_key,
              const std::string& in_icon_path,
              const std::string& in_label,
              bool in_is_selection_item,
              bool in_is_selection_item_checked,
              int in_selection_item_id)
      : key(in_key),
        icon_path(in_icon_path),
        label(in_label),
        is_selection_item(in_is_selection_item),
        is_selection_item_checked(in_is_selection_item_checked),
        selection_item_id(in_selection_item_id) {
    DCHECK(!key.empty());
  }

  ImeProperty()
      : is_selection_item(false),
        is_selection_item_checked(false),
        selection_item_id(kInvalidSelectionItemId) {
  }

  // Debug print function.
  std::string ToString() const {
    std::stringstream stream;
    stream << "key=" << key
           << ", icon_path=" << icon_path
           << ", label=" << label
           << ", is_selection_item=" << is_selection_item
           << ", is_selection_item_checked=" << is_selection_item_checked
           << ", selection_item_id=" << selection_item_id;
    return stream.str();
  }

  std::string key;  // A key which identifies the property. Non-empty string.
                    // (e.g. "InputMode.HalfWidthKatakana")
  std::string icon_path;  // Path to an icon. Can be empty.
  std::string label;  // A description of the property. Non-empty string.
                      // (e.g. "Switch to full punctuation mode", "Hiragana")
  bool is_selection_item;  // true if the property is a selection item.
  bool is_selection_item_checked;  // true if |is_selection_item| is true and
                                   // the selection_item is selected.
  int selection_item_id;  // A group ID (>= 0) of the selection item.
                          // kInvalidSelectionItemId if |id_selection_item| is
                          // false.
};
typedef std::vector<ImeProperty> ImePropertyList;

// Creates dummy InputLanguageList object. Usually, this function is called only
// on development enviromnent where libcros.so does not exist. So, obviously
// you can not move this function to libcros.so. This function is called by
// src/chrome/browser/chromeos/language_library.cc when EnsureLoaded() fails.
inline InputLanguageList* CreateFallbackInputLanguageList() {
  InputLanguageList* language_list = new InputLanguageList;
  language_list->push_back(
      InputLanguage(LANGUAGE_CATEGORY_XKB,
                    kFallbackXKBId,
                    kFallbackXKBDisplayName,
                    "" /* no icon */));
  return language_list;
}

// A monitor function which is called when current IME language or XKB layout
// is changed by user.
typedef void(*LanguageCurrentLanguageMonitorFunction)(
    void* language_library, const InputLanguage& current_language);

// A monitor function which is called when "RegisterProperties" signal is sent
// from the candidate_window process. The signal contains a list of properties
// for a specific IME engine. For example, Japanese IME (ibus-anthy) might have
// the following properties:
//
// ----------------------------------
//   key: InputMode.Hiragana
//   label: Hiragana
//   is_selection_item: true
//   is_selection_item_checked: true
//   selection_item_id: 1
// ----------------------------------
//   key: InputMode.Katakana
//   label: Katakana
//   is_selection_item: true
//   is_selection_item_checked: false
//   selection_item_id: 1
// ----------------------------------
//   ...
// ----------------------------------
typedef void(*LanguageRegisterImePropertiesFunction)(
    void* language_library, const ImePropertyList& prop_list);

// A monitor function which is called when "UpdateProperty" signal is sent
// from the candidate_window process. The signal contains one or more
// properties which is updated recently. Keys the signal contains are a subset
// of keys registered by the "RegisterProperties" signal above. For example,
// Japanese IME (ibus-anthy) might send the following properties:
//
// ----------------------------------
//   key: InputMode.Hiragana
//   label: Hiragana
//   is_selection_item: true
//   is_selection_item_checked: false
//   selection_item_id: ...
// ----------------------------------
//   key: InputMode.Katakana
//   label: Katakana
//   is_selection_item: true
//   is_selection_item_checked: true
//   selection_item_id: ...
// ----------------------------------
//
// Note: Please do not use selection_item_ids in |prop_list|. Dummy values are
//       filled in the field.
typedef void(*LanguageUpdateImePropertyFunction)(
    void* language_library, const ImePropertyList& prop_list);

struct LanguageStatusMonitorFunctions {
  LanguageStatusMonitorFunctions()
      : current_language(NULL),
        register_ime_properties(NULL),
        update_ime_property(NULL) {
  }
  LanguageCurrentLanguageMonitorFunction current_language;
  LanguageRegisterImePropertiesFunction register_ime_properties;
  LanguageUpdateImePropertyFunction update_ime_property;
};

// Establishes IBus connection to the ibus-daemon and DBus connection to
// the candidate window process. |monitor_function| will be called when IME
// language or XKB layout is changed.
class LanguageStatusConnection;
extern LanguageStatusConnection* (*MonitorLanguageStatus)(
    LanguageStatusMonitorFunctions monitor_funcions, void* language_library);

// Terminates IBus and DBus connections.
extern void (*DisconnectLanguageStatus)(LanguageStatusConnection* connection);

// Gets all IME languages and XKB layouts that are currently active. Caller
// has to delete the returned list. This function might return NULL on error.
extern InputLanguageList* (*GetLanguages)(LanguageStatusConnection* connection);

// Gets all IME languages and XKB layouts that are supported, including ones
// not active. Caller has to delete the returned list. This function might
// return NULL on error.
extern InputLanguageList* (*GetSupportedLanguages)(LanguageStatusConnection*
                                                   connection);

// Changes the current IME engine to |name| and enable IME (when |category| is
// LANGUAGE_CATEGORY_IME). Changes the current XKB layout to |name| and disable
// IME (when |category| is LANGUAGE_CATEGORY_XKB).
extern void (*ChangeLanguage)(LanguageStatusConnection* connection,
                              LanguageCategory category, const char* name);

// Activates the language specified by |category| and |name|. Returns true on
// success.
extern bool (*ActivateLanguage)(LanguageStatusConnection* connection,
                                LanguageCategory category, const char* name);

// Deactivates the language specified by |category| and |name|. Returns true
// on success.
extern bool (*DeactivateLanguage)(LanguageStatusConnection* connection,
                                  LanguageCategory category, const char* name);

// Activates an IME property identified by |key|. Examples of keys are:
// "InputMode.Katakana", "InputMode.HalfWidthKatakana", "TypingMode.Romaji",
// "TypingMode.Kana."
extern void (*ActivateImeProperty)(LanguageStatusConnection* connection,
                                   const char* key);

// Deactivates an IME property identified by |key|.
extern void (*DeactivateImeProperty)(LanguageStatusConnection* connection,
                                     const char* key);

}  // namespace chromeos

#endif  // CHROMEOS_POWER_H_
