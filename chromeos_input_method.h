// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_INPUT_METHOD_H_
#define CHROMEOS_INPUT_METHOD_H_

#include <sstream>
#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/logging.h>  // DCHECK

static const char kFallbackInputMethodId[] = "xkb:us::eng";
static const char kFallbackInputMethodDisplayName[] = "USA";
static const char kFallbackKeyboardLayoutName[] = "us";
static const char kFallbackInputMethodLanguageCode[] = "eng";
static const int kInvalidSelectionItemId = -1;

namespace chromeos {

// DEPRECATED: TODO(yusukes): Remove this enum when it's ready.
enum DeprecatedLanguageCategory {
  DEPRECATED_LANGUAGE_CATEGORY
};

// A structure which represents an input method.
struct InputMethodDescriptor {
  InputMethodDescriptor(const std::string& in_id,
                        const std::string& in_display_name,
                        const std::string& in_keyboard_layout,
                        const std::string& in_language_code)
      : deprecated_category(DEPRECATED_LANGUAGE_CATEGORY),
        id(in_id),
        display_name(in_display_name),
        keyboard_layout(in_keyboard_layout),
        language_code(in_language_code) {
  }

  bool operator==(const InputMethodDescriptor& other) const {
    return (id == other.id);
  }

  // Debug print function.
  std::string ToString() const {
    std::stringstream stream;
    stream << "id=" << id
           << ", display_name=" << display_name
           << ", keyboard_layout=" << keyboard_layout
           << ", language_code=" << language_code;
    return stream.str();
  }

  // DEPRECATED: TODO(yusukes): Remove this when it's ready.
  DeprecatedLanguageCategory deprecated_category;

  // An ID that identifies an input method engine (e.g., "t:latn-post",
  // "pinyin", "hangul").
  std::string id;
  // An input method name which can be used in the UI (e.g., "Pinyin").
  std::string display_name;
  // A preferred keyboard layout for the input method (e.g., "us", "us(dvorak)",
  // "jp").
  std::string keyboard_layout;
  // Language codes like "ko", "ja", "zh_CN", and "t".
  // "t" is used for languages in the "Others" category.
  std::string language_code;
};
typedef std::vector<InputMethodDescriptor> InputMethodDescriptors;

// A structure which represents a property for an input method engine. For
// details, please check a comment for the LanguageRegisterImePropertiesFunction
// typedef below.
// TODO(yusukes): Rename this struct. "InputMethodProperty" might be better?
struct ImeProperty {
  ImeProperty(const std::string& in_key,
              const std::string& in_label,
              bool in_is_selection_item,
              bool in_is_selection_item_checked,
              int in_selection_item_id)
      : key(in_key),
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
           << ", label=" << label
           << ", is_selection_item=" << is_selection_item
           << ", is_selection_item_checked=" << is_selection_item_checked
           << ", selection_item_id=" << selection_item_id;
    return stream.str();
  }

  std::string key;  // A key which identifies the property. Non-empty string.
                    // (e.g. "InputMode.HalfWidthKatakana")
  // DEPRECATED: TODO(yusukes): Remove this when it's ready.
  std::string deprecated_icon_path;
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

// A structure which represents a value of an input method configuration item.
// This struct is used by GetImeConfig() and SetImeConfig() cros APIs.
// TODO(yusukes): Rename this struct. "InputMethodConfigValue" might be better?
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
        stream << ", string_list_value=";
        for (size_t i = 0; i < string_list_value.size(); ++i) {
          if (i) {
            stream << ",";
          }
          stream << string_list_value[i];
        }
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

// Creates dummy InputMethodDescriptors object. Usually, this function is
// called only on development enviromnent where libcros.so does not exist.
// So, obviously you can not move this function to libcros.so.
// This function is called by src/chrome/browser/chromeos/language_library.cc
// when EnsureLoaded() fails.
inline InputMethodDescriptors* CreateFallbackInputMethodDescriptors() {
  InputMethodDescriptors* descriptions = new InputMethodDescriptors;
  descriptions->push_back(
      InputMethodDescriptor(kFallbackInputMethodId,
                            kFallbackInputMethodDisplayName,
                            kFallbackKeyboardLayoutName,
                            kFallbackInputMethodLanguageCode));
  return descriptions;
}

// A monitor function which is called when current input method is changed by a
// user.
typedef void(*LanguageCurrentInputMethodMonitorFunction)(
    void* language_library, const InputMethodDescriptor& current_input_method);

// A monitor function which is called when "RegisterProperties" signal is sent
// from the candidate_window process. The signal contains a list of properties
// for a specific input method engine. For example, Japanese input method
// (ibus-anthy) might have the following properties:
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
// Japanese input method (ibus-anthy) might send the following properties:
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

// A monitor function which is called when ibus connects or disconnects.
typedef void(*LanguageConnectionChangeMonitorFunction)(
    void* language_library, bool connected);

// Establishes IBus connection to the ibus-daemon and DBus connection to the
// candidate window process. LanguageXXXFunction functions will be called when
// status of input method engines is changed.
class InputMethodStatusConnection;
extern InputMethodStatusConnection* (*MonitorInputMethodStatus)(
    void* language_library,
    LanguageCurrentInputMethodMonitorFunction current_input_method,
    LanguageRegisterImePropertiesFunction register_ime_properties,
    LanguageUpdateImePropertyFunction update_ime_property,
    LanguageConnectionChangeMonitorFunction connection_changed);

// Terminates IBus and DBus connections.
extern void (*DisconnectInputMethodStatus)(
    InputMethodStatusConnection* connection);

// Updates the list of active input methods.  This will be returned by
// GetActiveInputMethods() if an ibus connection is not available.
extern bool (*SetActiveInputMethods)(InputMethodStatusConnection* connection,
                                     const ImeConfigValue& value);

// Gets all input method engines that are currently active. Caller has to
// delete the returned list. This function might return NULL on error.
extern InputMethodDescriptors* (*GetActiveInputMethods)(
    InputMethodStatusConnection* connection);

// Gets all input method engines that are supported, including ones not active.
// Caller has to delete the returned list. This function might return NULL on
// error.
extern InputMethodDescriptors* (*GetSupportedInputMethods)(
    InputMethodStatusConnection* connection);

// Changes the current input method engine to |name|. Returns true on success.
// Examples of names: "pinyin", "m17n:ar:kbd", "xkb:us:dvorak:eng".
extern bool (*ChangeInputMethod)(
    InputMethodStatusConnection* connection, const char* name);

// Sets whether the input method specified by |name| is activated.
// If |activated| is true activates the input method. If |activated| is false,
// deactivates the input method. Returns true on success.
// TODO(yusukes): Probably we can remove this function.
extern bool (*SetInputMethodActivated)(
    InputMethodStatusConnection* connection, const char* name, bool activated);

// Sets whether the input method property specified by |key| is activated.
// If |activated| is true, activates the property. If |activated| is false,
// deactivates the property.
// TODO(yusukes): "SetInputMethodPropertyActivated" might be better?
extern void (*SetImePropertyActivated)(InputMethodStatusConnection* connection,
                                       const char* key,
                                       bool activated);

// Get a configuration of ibus-daemon or IBus engines and stores it on
// |out_value|. Returns true if |out_value| is successfully updated.
//
// To retrieve 'panel/custom_font', |section| should be "panel", and
// |config_name| should be "custom_font".
// TODO(yusukes): "GetInputMethodConfig" might be better?
extern bool (*GetImeConfig)(InputMethodStatusConnection* connection,
                            const char* section,
                            const char* config_name,
                            ImeConfigValue* out_value);

// Updates a configuration of ibus-daemon or IBus engines with |value|.
// Returns true if the configuration is successfully updated.
// You can specify |section| and |config_name| arguments in the same way
// as GetImeConfig() above.
// TODO(yusukes): "SetInputMethodConfig" might be better?
extern bool (*SetImeConfig)(InputMethodStatusConnection* connection,
                            const char* section,
                            const char* config_name,
                            const ImeConfigValue& value);

// Returns true if IBus connection is still alive.
// If this function returns false, you might have to discard the current
// |connection| by calling DisconnectInputMethodStatus() API above, and create
// a new connection by calling MonitorInputMethodStatus() API.
extern bool (*InputMethodStatusConnectionIsAlive)(
    InputMethodStatusConnection* connection);

}  // namespace chromeos

#endif  // CHROMEOS_INPUT_METHOD_H_
