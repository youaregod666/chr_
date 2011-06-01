// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_INPUT_METHOD_H_
#define CHROMEOS_INPUT_METHOD_H_

#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <base/basictypes.h>
#include <base/logging.h>  // DCHECK
#include <base/string_split.h>

namespace chromeos {

// A structure which represents an input method. All methods in this class have
// to be in chromeos_input_method.h since Chrome also creates an instance of
// the class.
struct InputMethodDescriptor {
  InputMethodDescriptor() {
  }

  // Do not call this function directly. Use CreateInputMethodDescriptor() in
  // chromeos_input_method.cc whenever possible.
  InputMethodDescriptor(const std::string& in_id,
                        const std::string& in_display_name,
                        const std::string& in_keyboard_layout,
                        const std::string& in_virtual_keyboard_layouts,
                        const std::string& in_language_code)
      : virtual_keyboard_layouts_(in_virtual_keyboard_layouts),
        id(in_id),
        display_name(in_display_name),
        keyboard_layout(in_keyboard_layout),
        language_code(in_language_code) {
    DCHECK(keyboard_layout.find(",") == std::string::npos);
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
           << ", virtual_keyboard_layouts=" << virtual_keyboard_layouts_
           << ", language_code=" << language_code;
    return stream.str();
  }

  // TODO(yusukes): When libcros is moved to Chrome, do the following:
  // 1. Change the type of the virtual_keyboard_layouts_ variable to
  //    std::vector<std::string> and rename it back to virtual_keyboard_layouts.
  // 2. Remove the virtual_keyboard_layouts() function.
  // We can't do them right now because it will break ABI compatibility...
  std::vector<std::string> virtual_keyboard_layouts() const {
    std::vector<std::string> layout_names;
    base::SplitString(virtual_keyboard_layouts_, ',', &layout_names);
    return layout_names;
  }

  // Preferred virtual keyboard layouts for the input method. Comma separated
  // layout names in order of priority, such as "handwriting,us", could appear.
  // Note: DO NOT ACCESS THIS VARIABLE DIRECTLY. USE virtual_keyboard_layouts()
  // INSTEAD. SEE THE TODO ABOVE.
  std::string virtual_keyboard_layouts_;

  // An ID that identifies an input method engine (e.g., "t:latn-post",
  // "pinyin", "hangul").
  std::string id;
  // An input method name which can be used in the UI (e.g., "Pinyin").
  std::string display_name;
  // A preferred physical keyboard layout for the input method (e.g., "us",
  // "us(dvorak)", "jp"). Comma separated layout names do NOT appear.
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
                          // kInvalidSelectionItemId if |is_selection_item| is
                          // false.
  static const int kInvalidSelectionItemId = -1;
};
typedef std::vector<ImeProperty> ImePropertyList;

// A structure which represents a value of an input method configuration item.
// This struct is used by SetImeConfig().
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

// A monitor function which is called when current input method is changed by a
// user.
typedef void(*LanguageCurrentInputMethodMonitorFunction)(
    void* language_library, const InputMethodDescriptor& current_input_method);

// A monitor function which is called when "RegisterProperties" signal is sent
// from ibus-daemon. The signal contains a list of properties for a specific
// input method engine. For example, Japanese input method might have the
// following properties:
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
// from ibus-daemon. The signal contains one or more properties which is updated
// recently. Keys the signal contains are a subset of keys registered by the
// "RegisterProperties" signal above. For example,
// Japanese input method might send the following properties:
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

// Establishes IBus connection to the ibus-daemon. LanguageXXXFunction functions
// will be called when status of input method engines is changed.
class InputMethodStatusConnection;
extern InputMethodStatusConnection* (*MonitorInputMethodStatus)(
    void* language_library,
    LanguageCurrentInputMethodMonitorFunction current_input_method,
    LanguageRegisterImePropertiesFunction register_ime_properties,
    LanguageUpdateImePropertyFunction update_ime_property,
    LanguageConnectionChangeMonitorFunction connection_changed);

// Stops ibus-daemon. Returns true on success.
extern bool (*StopInputMethodProcess)(InputMethodStatusConnection* connection);

// Gets all input method engines that are supported, including ones not active.
// Caller has to delete the returned list. This function never returns NULL.
extern InputMethodDescriptors* (*GetSupportedInputMethodDescriptors)();

// Changes the current input method engine to |name|. Returns true on success.
// Examples of names: "pinyin", "m17n:ar:kbd", "xkb:us:dvorak:eng".
extern bool (*ChangeInputMethod)(
    InputMethodStatusConnection* connection, const char* name);

// Sets whether the input method property specified by |key| is activated.
// If |activated| is true, activates the property. If |activated| is false,
// deactivates the property.
// TODO(yusukes): "SetInputMethodPropertyActivated" might be better?
extern void (*SetImePropertyActivated)(InputMethodStatusConnection* connection,
                                       const char* key,
                                       bool activated);

// Sets a configuration of ibus-daemon or IBus engines to |value|.
// Returns true if the configuration is successfully set.
//
// To set 'panel/custom_font', |section| should be "panel", and
// |config_name| should be "custom_font".
// TODO(yusukes): "SetInputMethodConfig" might be better?
extern bool (*SetImeConfig)(InputMethodStatusConnection* connection,
                            const char* section,
                            const char* config_name,
                            const ImeConfigValue& value);

// Returns the keyboard overlay ID corresponding to |input_method_id|.
// Returns an empty string if there is no corresponding keyboard overlay ID.
extern std::string (*GetKeyboardOverlayId)(
    const std::string& input_method_id);

// Sends a handwriting stroke to ibus-daemon. The std::pair contains x and y
// coordinates. (0.0, 0.0) represents the top-left corner of a handwriting area,
// and (1.0, 1.0) does the bottom-right. For example, the second stroke for ロ
// (Katakana character Ro) would be something like [(0,0), (1,0), (1,1)].
// stroke.size() should always be >= 2 (i.e. a single dot is not allowed).
typedef std::vector<std::pair<double, double> > HandwritingStroke;
extern void (*SendHandwritingStroke)(
    InputMethodStatusConnection* connection, const HandwritingStroke& stroke);

// Clears the last N handwriting strokes. Pass zero for clearing all strokes.
// TODO(yusukes): Currently ibus-daemon only accepts 0 for |n_strokes|.
extern void (*CancelHandwriting)(InputMethodStatusConnection* connection,
                                 int n_strokes);

//
// FUNCTIONS BELOW ARE ONLY FOR UNIT TESTS. DO NOT USE THEM.
//
bool InputMethodIdIsWhitelisted(const std::string& input_method_id);
bool XkbLayoutIsSupported(const std::string& xkb_layout);
InputMethodDescriptor CreateInputMethodDescriptor(
    const std::string& id,
    const std::string& display_name,
    const std::string& raw_layout,
    const std::string& language_code);

}  // namespace chromeos

#endif  // CHROMEOS_INPUT_METHOD_H_
