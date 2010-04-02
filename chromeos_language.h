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
static const char kFallbackXKBLanguageCode[] = "en";

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
                const std::string& in_icon_path,
                const std::string& in_language_code)
      : category(in_category),
        id(in_id),
        display_name(in_display_name),
        icon_path(in_icon_path),
        language_code(in_language_code) {
  }

  InputLanguage() : category(LANGUAGE_CATEGORY_XKB) {
  }

  // Languages are sorted by category, then by language_code,
  // then by display_name, then by id.
  bool operator<(const InputLanguage& other) const {
    if (category != other.category) {
      return category < other.category;
    }
    if (language_code != other.language_code) {
      return language_code < other.language_code;
    }
    if (display_name != other.display_name) {
      return display_name < other.display_name;
    }
    return id < other.id;
  }

  bool operator==(const InputLanguage& other) const {
    return (category == other.category) && (id == other.id);
  }

  // Debug print function.
  std::string ToString() const {
    std::stringstream stream;
    stream << "category=" << category
           << ", id=" << id
           << ", display_name=" << display_name
           << ", icon_path=" << icon_path
           << ", language_code=" << language_code;
    return stream.str();
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
  // Language codes like "ko", "ja", "zh_CN", and "t".
  // "t" is used for languages in the "Others" category.
  std::string language_code;
};
typedef std::vector<InputLanguage> InputLanguageList;

// A structure which represents a property for an IME engine. For details,
// please check a comment for the LanguageRegisterImePropertiesFunction typedef
// below.
struct ImeProperty {
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

// A structure which represents a value of an IME configuration item.
// This struct is used by GetImeConfig() and SetImeConfig() cros APIs.
struct ImeConfigValue {
  ImeConfigValue()
      : type(kValueTypeString),
        int_value(0),
        bool_value(false) {
  }

  // Debug print function.
  std::string ToString() const {
    std::stringstream stream;
    stream << "type=" << type;
    switch (type) {
      case kValueTypeString:
        stream << ", string_value=" << string_value;
        break;
      case kValueTypeInt:
        stream << ", int_value=" << int_value;
        break;
      case kValueTypeBool:
        stream << ", bool_value=" << (bool_value ? "true" : "false");
        break;
      case kValueTypeStringList:
        // TODO(yusukes): Support this type.
        break;
    }
    return stream.str();
  }

  enum ValueType {
    kValueTypeString = 0,
    kValueTypeInt,
    kValueTypeBool,
    kValueTypeStringList,
  };

  // A value is stored on |string_value| member if |type| is kValueTypeString.
  // The same is true for other enum values.
  ValueType type;

  std::string string_value;
  int int_value;
  bool bool_value;
  std::vector<std::string> string_list_value;
};

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
                    "" /* no icon */,
                    kFallbackXKBLanguageCode));
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
extern InputLanguageList* (*GetActiveLanguages)(LanguageStatusConnection*
                                                connection);

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

// Sets whether the language specified by |category| and |id| is
// activated. If |activated| is true, activates the language. If
// |activated| is false, deactivates the language. Returns true on success.
extern bool (*SetLanguageActivated)(LanguageStatusConnection* connection,
                                    LanguageCategory category,
                                    const char* name,
                                    bool activated);

// Sets whether the IME property specified by |key| is activated. If
// |activated| is true, activates the property. If |activated| is false,
// deactivates the property.
extern void (*SetImePropertyActivated)(LanguageStatusConnection* connection,
                                       const char* key,
                                       bool activated);

// DEPRECATED: TODO(satorux): Remove this when it's ready.
// Activates the language specified by |category| and |name|. Returns true on
// success.
extern bool (*ActivateLanguage)(LanguageStatusConnection* connection,
                                LanguageCategory category, const char* name);

// DEPRECATED: TODO(satorux): Remove this when it's ready.
// Deactivates the language specified by |category| and |name|. Returns true
// on success.
extern bool (*DeactivateLanguage)(LanguageStatusConnection* connection,
                                  LanguageCategory category, const char* name);

// DEPRECATED: TODO(satorux): Remove this when it's ready.
// Activates an IME property identified by |key|. Examples of keys are:
// "InputMode.Katakana", "InputMode.HalfWidthKatakana", "TypingMode.Romaji",
// "TypingMode.Kana."
extern void (*ActivateImeProperty)(LanguageStatusConnection* connection,
                                   const char* key);

// DEPRECATED: TODO(satorux): Remove this when it's ready.
// Deactivates an IME property identified by |key|.
extern void (*DeactivateImeProperty)(LanguageStatusConnection* connection,
                                     const char* key);

// Get a configuration of ibus-daemon or IBus engines and stores it on
// |out_value|. Returns true if |out_value| is successfully updated.
//
// To retrieve 'panel/custom_font' (see below), |section| should be "panel",
// and |config_name| should be "custom_font".
//
// % gconftool-2 --dump /desktop/ibus | grep -A 3 -B 1 '<key>panel/custom_font'
//     <entry>
//       <key>panel/custom_font</key>
//       <value>
//         <string>Sans 10</string>
//       </value>
extern bool (*GetImeConfig)(LanguageStatusConnection* connection,
                            const char* section,
                            const char* config_name,
                            ImeConfigValue* out_value);

// Updates a configuration of ibus-daemon or IBus engines with |value|.
// Returns true if the configuration is successfully updated.
// You can specify |section| and |config_name| arguments in the same way
// as GetImeConfig() above.
extern bool (*SetImeConfig)(LanguageStatusConnection* connection,
                            const char* section,
                            const char* config_name,
                            const ImeConfigValue& value);

// Returns true if IBus and XKB sessions are still alive.
// If this function returns false, you might have to discard the current
// |connection| by calling DisconnectLanguageStatus() API above, and create
// a new connection by calling MonitorLanguageStatus() API.
extern bool (*LanguageStatusConnectionIsAlive)(
    LanguageStatusConnection* connection);

}  // namespace chromeos

#endif  // CHROMEOS_POWER_H_
