// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_input_method.h"

#include "chromeos_input_method_ui.h"
#include "chromeos_input_method_whitelist.h"
#include "chromeos_keyboard_overlay_map.h"

#include <ibus.h>

#include <algorithm>  // for std::reverse.
#include <cstdio>
#include <cstring>  // for std::strcmp.
#include <set>
#include <sstream>
#include <stack>
#include <utility>

#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "ibus_input_methods.h"

namespace chromeos {

// Returns true if |input_method_id| is whitelisted.
bool InputMethodIdIsWhitelisted(const std::string& input_method_id) {
  static std::set<std::string>* g_supported_input_methods = NULL;
  if (!g_supported_input_methods) {
    g_supported_input_methods = new std::set<std::string>;
    for (size_t i = 0; i < arraysize(kInputMethodIdsWhitelist); ++i) {
      g_supported_input_methods->insert(kInputMethodIdsWhitelist[i]);
    }
  }
  return (g_supported_input_methods->count(input_method_id) > 0);
}

// Returns true if |xkb_layout| is supported.
bool XkbLayoutIsSupported(const std::string& xkb_layout) {
  static std::set<std::string>* g_supported_layouts = NULL;
  if (!g_supported_layouts) {
    g_supported_layouts = new std::set<std::string>;
    for (size_t i = 0; i < arraysize(kXkbLayoutsWhitelist); ++i) {
      g_supported_layouts->insert(kXkbLayoutsWhitelist[i]);
    }
  }
  return (g_supported_layouts->count(xkb_layout) > 0);
}

// Creates an InputMethodDescriptor object. |raw_layout| is a comma-separated
// list of XKB and virtual keyboard layouts.
// (e.g. "special-us-virtual-keyboard-for-the-input-method,us")
InputMethodDescriptor CreateInputMethodDescriptor(
    const std::string& id,
    const std::string& display_name,
    const std::string& raw_layout,
    const std::string& language_code) {
  static const char fallback_layout[] = "us";
  std::string physical_keyboard_layout = fallback_layout;
  const std::string& virtual_keyboard_layout = raw_layout;

  std::vector<std::string> layout_names;
  base::SplitString(raw_layout, ',', &layout_names);

  // Find a valid XKB layout name from the comma-separated list, |raw_layout|.
  // Only the first acceptable XKB layout name in the list is used as the
  // |keyboard_layout| value of the InputMethodDescriptor object to be created.
  for (size_t i = 0; i < layout_names.size(); ++i) {
    if (XkbLayoutIsSupported(layout_names[i])) {
      physical_keyboard_layout = layout_names[i];
      break;
    }
  }

  return InputMethodDescriptor(id,
                               display_name,
                               physical_keyboard_layout,
                               virtual_keyboard_layout,
                               language_code);
}

namespace {

// Also defined in chrome/browser/chromeos/language_preferences.h (Chrome tree).
const char kGeneralSectionName[] = "general";
const char kPreloadEnginesConfigName[] = "preload_engines";

// The list of input method property keys that we don't handle.
const char* kInputMethodPropertyKeysBlacklist[] = {
  "setup",  // menu for showing setup dialog used in anthy and hangul.
  "chewing_settings_prop",  // menu for showing setup dialog used in chewing.
  "status",  // used in m17n.
};

const char* Or(const char* str1, const char* str2) {
  return str1 ? str1 : str2;
}

// Returns true if |key| is blacklisted.
bool PropertyKeyIsBlacklisted(const char* key) {
  for (size_t i = 0; i < arraysize(kInputMethodPropertyKeysBlacklist); ++i) {
    if (!std::strcmp(key, kInputMethodPropertyKeysBlacklist[i])) {
      return true;
    }
  }
  return false;
}

// Removes input methods that are not whitelisted from |requested_input_methods|
// and stores them on |out_filtered_input_methods|.
// TODO(yusukes): Write unittest.
void FilterInputMethods(const std::vector<std::string>& requested_input_methods,
                        std::vector<std::string>* out_filtered_input_methods) {
  out_filtered_input_methods->clear();
  for (size_t i = 0; i < requested_input_methods.size(); ++i) {
    const std::string& input_method = requested_input_methods[i];
    if (InputMethodIdIsWhitelisted(input_method.c_str())) {
      out_filtered_input_methods->push_back(input_method);
    } else {
      LOG(ERROR) << "Unsupported input method: " << input_method;
    }
  }
}

// Frees input method names in |engines| and the list itself. Please make sure
// that |engines| points the head of the list.
void FreeInputMethodNames(GList* engines) {
  if (engines) {
    for (GList* cursor = engines; cursor; cursor = g_list_next(cursor)) {
      g_object_unref(IBUS_ENGINE_DESC(cursor->data));
    }
    g_list_free(engines);
  }
}

// Copies input method names in |engines| to |out|.
// TODO(yusukes): Write unittest.
void AddInputMethodNames(const GList* engines, InputMethodDescriptors* out) {
  DCHECK(out);
  for (; engines; engines = g_list_next(engines)) {
    IBusEngineDesc* engine_desc = IBUS_ENGINE_DESC(engines->data);
    const gchar* name = ibus_engine_desc_get_name(engine_desc);
    const gchar* longname = ibus_engine_desc_get_longname(engine_desc);
    const gchar* layout = ibus_engine_desc_get_layout(engine_desc);
    const gchar* language = ibus_engine_desc_get_language(engine_desc);
    if (InputMethodIdIsWhitelisted(name)) {
      out->push_back(CreateInputMethodDescriptor(name,
                                                 longname,
                                                 layout,
                                                 language));
      DLOG(INFO) << name << " (preloaded)";
    }
  }
}

// Returns IBusInputContext for |input_context_path|. NULL on errors.
IBusInputContext* GetInputContext(
    const std::string& input_context_path, IBusBus* ibus) {
  GDBusConnection* connection = ibus_bus_get_connection(ibus);
  if (!connection) {
    LOG(ERROR) << "IBusConnection is null";
    return NULL;
  }
  // This function does not issue an IBus IPC.
  IBusInputContext* context = ibus_input_context_get_input_context(
      input_context_path.c_str(), connection);
  if (!context) {
    LOG(ERROR) << "IBusInputContext is null: " << input_context_path;
  }
  return context;
}

// Returns true if |prop| has children.
bool PropertyHasChildren(IBusProperty* prop) {
  return prop && prop->sub_props && ibus_prop_list_get(prop->sub_props, 0);
}

// This function is called by and FlattenProperty() and converts IBus
// representation of a property, |ibus_prop|, to our own and push_back the
// result to |out_prop_list|. This function returns true on success, and
// returns false if sanity checks for |ibus_prop| fail.
bool ConvertProperty(IBusProperty* ibus_prop,
                     int selection_item_id,
                     ImePropertyList* out_prop_list) {
  DCHECK(ibus_prop);
  DCHECK(ibus_prop->key);
  DCHECK(out_prop_list);

  // Sanity checks.
  const bool has_sub_props = PropertyHasChildren(ibus_prop);
  if (has_sub_props && (ibus_prop->type != PROP_TYPE_MENU)) {
    LOG(ERROR) << "The property has sub properties, "
               << "but the type of the property is not PROP_TYPE_MENU";
    return false;
  }
  if ((!has_sub_props) && (ibus_prop->type == PROP_TYPE_MENU)) {
    // This is usually not an error. ibus-daemon sometimes sends empty props.
    DLOG(INFO) << "Property list is empty";
    return false;
  }
  if (ibus_prop->type == PROP_TYPE_SEPARATOR ||
      ibus_prop->type == PROP_TYPE_MENU) {
    // This is not an error, but we don't push an item for these types.
    return true;
  }

  const bool is_selection_item = (ibus_prop->type == PROP_TYPE_RADIO);
  selection_item_id = is_selection_item ?
      selection_item_id : ImeProperty::kInvalidSelectionItemId;

  bool is_selection_item_checked = false;
  if (ibus_prop->state == PROP_STATE_INCONSISTENT) {
    LOG(WARNING) << "The property is in PROP_STATE_INCONSISTENT, "
                 << "which is not supported.";
  } else if ((!is_selection_item) && (ibus_prop->state == PROP_STATE_CHECKED)) {
    LOG(WARNING) << "PROP_STATE_CHECKED is meaningful only if the type is "
                 << "PROP_TYPE_RADIO.";
  } else {
    is_selection_item_checked = (ibus_prop->state == PROP_STATE_CHECKED);
  }

  if (!ibus_prop->key) {
    LOG(ERROR) << "key is NULL";
  }
  if (ibus_prop->tooltip && (!ibus_prop->tooltip->text)) {
    LOG(ERROR) << "tooltip is NOT NULL, but tooltip->text IS NULL: key="
               << Or(ibus_prop->key, "");
  }
  if (ibus_prop->label && (!ibus_prop->label->text)) {
    LOG(ERROR) << "label is NOT NULL, but label->text IS NULL: key="
               << Or(ibus_prop->key, "");
  }

  // This label will be localized on Chrome side.
  // See src/chrome/browser/chromeos/status/language_menu_l10n_util.h.
  std::string label =
      ((ibus_prop->tooltip &&
        ibus_prop->tooltip->text) ? ibus_prop->tooltip->text : "");
  if (label.empty()) {
    // Usually tooltips are more descriptive than labels.
    label = (ibus_prop->label && ibus_prop->label->text)
        ? ibus_prop->label->text : "";
  }
  if (label.empty()) {
    // ibus-pinyin has a property whose label and tooltip are empty. Fall back
    // to the key.
    label = Or(ibus_prop->key, "");
  }

  out_prop_list->push_back(ImeProperty(ibus_prop->key,
                                       label,
                                       is_selection_item,
                                       is_selection_item_checked,
                                       selection_item_id));
  return true;
}

// Converts |ibus_prop| to |out_prop_list|. Please note that |ibus_prop|
// may or may not have children. See the comment for FlattenPropertyList
// for details. Returns true if no error is found.
// TODO(yusukes): Write unittest.
bool FlattenProperty(IBusProperty* ibus_prop, ImePropertyList* out_prop_list) {
  DCHECK(ibus_prop);
  DCHECK(out_prop_list);

  int selection_item_id = -1;
  std::stack<std::pair<IBusProperty*, int> > prop_stack;
  prop_stack.push(std::make_pair(ibus_prop, selection_item_id));

  while (!prop_stack.empty()) {
    IBusProperty* prop = prop_stack.top().first;
    const int current_selection_item_id = prop_stack.top().second;
    prop_stack.pop();

    // Filter out unnecessary properties.
    if (PropertyKeyIsBlacklisted(prop->key)) {
      continue;
    }

    // Convert |prop| to ImeProperty and push it to |out_prop_list|.
    if (!ConvertProperty(prop, current_selection_item_id, out_prop_list)) {
      return false;
    }

    // Process childrens iteratively (if any). Push all sub properties to the
    // stack.
    if (PropertyHasChildren(prop)) {
      ++selection_item_id;
      for (int i = 0;; ++i) {
        IBusProperty* sub_prop = ibus_prop_list_get(prop->sub_props, i);
        if (!sub_prop) {
          break;
        }
        prop_stack.push(std::make_pair(sub_prop, selection_item_id));
      }
      ++selection_item_id;
    }
  }
  std::reverse(out_prop_list->begin(), out_prop_list->end());

  return true;
}

// Converts IBus representation of a property list, |ibus_prop_list| to our
// own. This function also flatten the original list (actually it's a tree).
// Returns true if no error is found. The conversion to our own type is
// necessary since our language switcher in Chrome tree don't (or can't) know
// IBus types. Here is an example:
//
// ======================================================================
// Input:
//
// --- Item-1
//  |- Item-2
//  |- SubMenuRoot --- Item-3-1
//  |               |- Item-3-2
//  |               |- Item-3-3
//  |- Item-4
//
// (Note: Item-3-X is a selection item since they're on a sub menu.)
//
// Output:
//
// Item-1, Item-2, Item-3-1, Item-3-2, Item-3-3, Item-4
// (Note: SubMenuRoot does not appear in the output.)
// ======================================================================
// TODO(yusukes): Write unittest.
bool FlattenPropertyList(
    IBusPropList* ibus_prop_list, ImePropertyList* out_prop_list) {
  DCHECK(ibus_prop_list);
  DCHECK(out_prop_list);

  IBusProperty* fake_root_prop = ibus_property_new("Dummy.Key",
                                                   PROP_TYPE_MENU,
                                                   NULL, /* label */
                                                   "", /* icon */
                                                   NULL, /* tooltip */
                                                   FALSE, /* sensitive */
                                                   FALSE, /* visible */
                                                   PROP_STATE_UNCHECKED,
                                                   ibus_prop_list);
  g_return_val_if_fail(fake_root_prop, false);
  // Increase the ref count so it won't get deleted when |fake_root_prop|
  // is deleted.
  g_object_ref(ibus_prop_list);
  const bool result = FlattenProperty(fake_root_prop, out_prop_list);
  g_object_unref(fake_root_prop);

  return result;
}

// Debug print function.
const char* PropTypeToString(int prop_type) {
  switch (static_cast<IBusPropType>(prop_type)) {
    case PROP_TYPE_NORMAL:
      return "NORMAL";
    case PROP_TYPE_TOGGLE:
      return "TOGGLE";
    case PROP_TYPE_RADIO:
      return "RADIO";
    case PROP_TYPE_MENU:
      return "MENU";
    case PROP_TYPE_SEPARATOR:
      return "SEPARATOR";
  }
  return "UNKNOWN";
}

// Debug print function.
const char* PropStateToString(int prop_state) {
  switch (static_cast<IBusPropState>(prop_state)) {
    case PROP_STATE_UNCHECKED:
      return "UNCHECKED";
    case PROP_STATE_CHECKED:
      return "CHECKED";
    case PROP_STATE_INCONSISTENT:
      return "INCONSISTENT";
  }
  return "UNKNOWN";
}

// Debug print function.
std::string Spacer(int n) {
  return std::string(n, ' ');
}

std::string PrintPropList(IBusPropList *prop_list, int tree_level);
// Debug print function.
std::string PrintProp(IBusProperty *prop, int tree_level) {
  if (!prop) {
    return "";
  }

  std::stringstream stream;
  stream << Spacer(tree_level) << "=========================" << std::endl;
  stream << Spacer(tree_level) << "key: " << Or(prop->key, "<none>")
         << std::endl;
  stream << Spacer(tree_level) << "icon: " << Or(prop->icon, "<none>")
         << std::endl;
  stream << Spacer(tree_level) << "label: "
         << ((prop->label && prop->label->text) ? prop->label->text : "<none>")
         << std::endl;
  stream << Spacer(tree_level) << "tooptip: "
         << ((prop->tooltip && prop->tooltip->text)
             ? prop->tooltip->text : "<none>") << std::endl;
  stream << Spacer(tree_level) << "sensitive: "
         << (prop->sensitive ? "YES" : "NO") << std::endl;
  stream << Spacer(tree_level) << "visible: " << (prop->visible ? "YES" : "NO")
         << std::endl;
  stream << Spacer(tree_level) << "type: " << PropTypeToString(prop->type)
         << std::endl;
  stream << Spacer(tree_level) << "state: " << PropStateToString(prop->state)
         << std::endl;
  stream << Spacer(tree_level) << "sub_props: "
         << (PropertyHasChildren(prop) ? "" : "<none>") << std::endl;
  stream << PrintPropList(prop->sub_props, tree_level + 1);
  stream << Spacer(tree_level) << "=========================" << std::endl;

  return stream.str();
}

// Debug print function.
std::string PrintPropList(IBusPropList *prop_list, int tree_level) {
  if (!prop_list) {
    return "";
  }

  std::stringstream stream;
  for (int i = 0;; ++i) {
    IBusProperty* prop = ibus_prop_list_get(prop_list, i);
    if (!prop) {
      break;
    }
    stream << PrintProp(prop, tree_level);
  }
  return stream.str();
}

}  // namespace

// A singleton class that holds IBus connections.
class InputMethodStatusConnection {
 public:
  // Returns a singleton object of the class. If the singleton object is already
  // initialized, the arguments are ignored.
  // Warning: you can call the callback functions only in the ibus callback
  // functions like FocusIn(). See http://crosbug.com/5217#c9 for details.
  static InputMethodStatusConnection* GetConnection(
      void* language_library,
      LanguageCurrentInputMethodMonitorFunction current_input_method_changed,
      LanguageRegisterImePropertiesFunction register_ime_properties,
      LanguageUpdateImePropertyFunction update_ime_property,
      LanguageConnectionChangeMonitorFunction connection_change_handler) {
    DCHECK(language_library);
    DCHECK(current_input_method_changed),
    DCHECK(register_ime_properties);
    DCHECK(update_ime_property);

    InputMethodStatusConnection* object = GetInstance();
    if (!object->language_library_) {
      object->language_library_ = language_library;
      object->current_input_method_changed_ = current_input_method_changed;
      object->register_ime_properties_= register_ime_properties;
      object->update_ime_property_ = update_ime_property;
      object->connection_change_handler_ = connection_change_handler;
      object->MaybeRestoreConnections();
    } else if (object->language_library_ != language_library) {
      LOG(ERROR) << "Unknown language_library is passed";
    }
    return object;
  }

  static InputMethodStatusConnection* GetInstance() {
    return Singleton<InputMethodStatusConnection,
        LeakySingletonTraits<InputMethodStatusConnection> >::get();
  }

  // Called by cros API ChromeOSStopInputMethodProcess().
  bool StopInputMethodProcess() {
    if (!IBusConnectionsAreAlive()) {
      LOG(ERROR) << "StopInputMethodProcess: IBus connection is not alive";
      return false;
    }

    // Ask IBus to exit *asynchronously*.
    ibus_bus_exit_async(ibus_,
                        FALSE  /* do not restart */,
                        -1  /* timeout */,
                        NULL  /* cancellable */,
                        NULL  /* callback */,
                        NULL  /* user_data */);

    if (ibus_config_) {
      // Release |ibus_config_| unconditionally to make sure next
      // IBusConnectionsAreAlive() call will return false.
      g_object_unref(ibus_config_);
      ibus_config_ = NULL;
    }
    return true;
  }

  // Called by cros API ChromeOS(Activate|Deactive)ImeProperty().
  void SetImePropertyActivated(const char* key, bool activated) {
    if (!IBusConnectionsAreAlive()) {
      LOG(ERROR) << "SetImePropertyActivated: IBus connection is not alive";
      return;
    }
    if (!key || (key[0] == '\0')) {
      return;
    }
    if (input_context_path_.empty()) {
      LOG(ERROR) << "Input context is unknown";
      return;
    }

    IBusInputContext* context = GetInputContext(input_context_path_, ibus_);
    if (!context) {
      return;
    }
    // Activate the property *asynchronously*.
    ibus_input_context_property_activate(
        context, key, (activated ? PROP_STATE_CHECKED : PROP_STATE_UNCHECKED));

    // We don't have to call ibus_proxy_destroy(context) explicitly here,
    // i.e. we can just call g_object_unref(context), since g_object_unref can
    // trigger both dispose, which is overridden by src/ibusproxy.c, and
    // finalize functions. For details, see
    // http://library.gnome.org/devel/gobject/stable/gobject-memory.html
    g_object_unref(context);
  }

  // Called by cros API ChromeOSChangeInputMethod().
  bool ChangeInputMethod(const char* name) {
    if (!IBusConnectionsAreAlive()) {
      LOG(ERROR) << "ChangeInputMethod: IBus connection is not alive";
      return false;
    }
    if (!name) {
      return false;
    }
    if (!InputMethodIdIsWhitelisted(name)) {
      LOG(ERROR) << "Input method '" << name << "' is not supported";
      return false;
    }

    // Clear all input method properties unconditionally.
    //
    // When switching to another input method and no text area is focused,
    // RegisterProperties signal for the new input method will NOT be sent
    // until a text area is focused. Therefore, we have to clear the old input
    // method properties here to keep the input method switcher status
    // consistent.
    RegisterProperties(NULL);

    // Change the global engine *asynchronously*.
    ibus_bus_set_global_engine_async(ibus_,
                                     name,
                                     -1,  // use the default ibus timeout
                                     NULL,  // cancellable
                                     NULL,  // callback
                                     NULL);  // user_data
    return true;
  }

  // Updates a configuration of ibus-daemon or IBus engines with |value|.
  // Returns true if the configuration is successfully updated.
  //
  // For more information, please read a comment for SetImeConfig() function
  // in chromeos_language.h.
  bool SetImeConfig(const std::string& section,
                    const std::string& config_name,
                    const ImeConfigValue& value) {
    // See comments in GetImeConfig() where ibus_config_get_value() is used.
    if (!IBusConnectionsAreAlive()) {
      LOG(ERROR) << "SetImeConfig: IBus connection is not alive";
      return false;
    }

    bool is_preload_engines = false;

    // Sanity check: do not preload unknown/unsupported input methods.
    std::vector<std::string> string_list;
    if ((value.type == ImeConfigValue::kValueTypeStringList) &&
        (section == kGeneralSectionName) &&
        (config_name == kPreloadEnginesConfigName)) {
      FilterInputMethods(value.string_list_value, &string_list);
      is_preload_engines = true;
    } else {
      string_list = value.string_list_value;
    }

    // Convert the type of |value| from our structure to GVariant.
    GVariant* variant = NULL;
    switch (value.type) {
      case ImeConfigValue::kValueTypeString:
        variant = g_variant_new_string(value.string_value.c_str());
        break;
      case ImeConfigValue::kValueTypeInt:
        variant = g_variant_new_int32(value.int_value);
        break;
      case ImeConfigValue::kValueTypeBool:
        variant = g_variant_new_boolean(value.bool_value);
        break;
      case ImeConfigValue::kValueTypeStringList:
        GVariantBuilder variant_builder;
        g_variant_builder_init(&variant_builder, G_VARIANT_TYPE("as"));
        const size_t size = string_list.size();  // don't use string_list_value.
        for (size_t i = 0; i < size; ++i) {
          g_variant_builder_add(&variant_builder, "s", string_list[i].c_str());
        }
        variant = g_variant_builder_end(&variant_builder);
        break;
    }

    if (!variant) {
      LOG(ERROR) << "SetImeConfig: variant is NULL";
      return false;
    }
    DCHECK(g_variant_is_floating(variant));

    // Set an ibus configuration value *asynchronously*.
    ibus_config_set_value_async(ibus_config_,
                                section.c_str(),
                                config_name.c_str(),
                                variant,
                                -1,  // use the default ibus timeout
                                NULL,  // cancellable
                                SetImeConfigCallback,
                                g_object_ref(ibus_config_));

    // Since |variant| is floating, ibus_config_set_value_async consumes
    // (takes ownership of) the variable.

    if (is_preload_engines) {
      DLOG(INFO) << "SetImeConfig: " << section << "/" << config_name
                 << ": " << value.ToString();
    }
    return true;
  }

  void SendHandwritingStroke(const HandwritingStroke& stroke) {
    if (stroke.size() < 2) {
      LOG(WARNING) << "Empty stroke data or a single dot is passed.";
      return;
    }

    IBusInputContext* context = GetInputContext(input_context_path_, ibus_);
    if (!context) {
      return;
    }

    const size_t raw_stroke_size = stroke.size() * 2;
    scoped_array<double> raw_stroke(new double[raw_stroke_size]);
    for (size_t n = 0; n < stroke.size(); ++n) {
      raw_stroke[n * 2] = stroke[n].first;  // x
      raw_stroke[n * 2 + 1] = stroke[n].second;  // y
    }
    ibus_input_context_process_hand_writing_event(
        context, raw_stroke.get(), raw_stroke_size);
    g_object_unref(context);
  }

  void CancelHandwriting(int n_strokes) {
    IBusInputContext* context = GetInputContext(input_context_path_, ibus_);
    if (!context) {
      return;
    }
    ibus_input_context_cancel_hand_writing(context, n_strokes);
    g_object_unref(context);
  }

 private:
  friend struct DefaultSingletonTraits<InputMethodStatusConnection>;
  InputMethodStatusConnection()
      : current_input_method_changed_(NULL),
        register_ime_properties_(NULL),
        update_ime_property_(NULL),
        connection_change_handler_(NULL),
        language_library_(NULL),
        ibus_(NULL),
        ibus_config_(NULL) {
  }

  ~InputMethodStatusConnection() {
    // Since the class is used as a leaky singleton, this destructor is never
    // called. However, if you would delete an instance of this class, you have
    // to disconnect all signals so the handler functions will not be called
    // with |this| which is already freed.
    //
    // if (ibus_) {
    //   g_signal_handlers_disconnect_by_func(
    //       ibus_,
    //       reinterpret_cast<gpointer>(
    //           G_CALLBACK(IBusBusConnectedCallback)),
    //       this);
    //   ...
    // }
    // if (ibus_panel_service_) {
    //   g_signal_handlers_disconnect_by_func(
    //       ...
  }

  // Checks if |ibus_| and |ibus_config_| connections are alive.
  bool IBusConnectionsAreAlive() {
    return ibus_ && ibus_bus_is_connected(ibus_) && ibus_config_;
  }

  // Restores connections to ibus-daemon and ibus-memconf if they are not ready.
  // If both |ibus_| and |ibus_config_| become ready, the function sends a
  // notification to Chrome.
  void MaybeRestoreConnections() {
    if (IBusConnectionsAreAlive()) {
      return;
    }
    MaybeCreateIBus();
    MaybeRestoreIBusConfig();
    if (IBusConnectionsAreAlive()) {
      ConnectPanelServiceSignals();
      if (connection_change_handler_) {
        LOG(INFO) << "Notifying Chrome that IBus is ready.";
        connection_change_handler_(language_library_, true);
      }
    }
  }

  // Creates IBusBus object if it's not created yet.
  void MaybeCreateIBus() {
    if (ibus_) {
      return;
    }

    ibus_init();
    // Establish IBus connection between ibus-daemon to retrieve the list of
    // available input method engines, change the current input method engine,
    // and so on.
    ibus_ = ibus_bus_new();

    // Check the IBus connection status.
    if (!ibus_) {
      LOG(ERROR) << "ibus_bus_new() failed";
      return;
    }
    // Register callback functions for IBusBus signals.
    ConnectIBusSignals();

    // Ask libibus to watch the NameOwnerChanged signal *asynchronously*.
    ibus_bus_set_watch_dbus_signal(ibus_, TRUE);
    // Ask libibus to watch the GlobalEngineChanged signal *asynchronously*.
    ibus_bus_set_watch_ibus_signal(ibus_, TRUE);

    if (ibus_bus_is_connected(ibus_)) {
      LOG(INFO) << "IBus connection is ready.";
    }
  }

  // Creates IBusConfig object if it's not created yet AND |ibus_| connection
  // is ready.
  void MaybeRestoreIBusConfig() {
    if (!ibus_) {
      return;
    }

    // Destroy the current |ibus_config_| object. No-op if it's NULL.
    MaybeDestroyIBusConfig();

    if (!ibus_config_) {
      GDBusConnection* ibus_connection = ibus_bus_get_connection(ibus_);
      if (!ibus_connection) {
        LOG(INFO) << "Couldn't create an ibus config object since "
                  << "IBus connection is not ready.";
        return;
      }
      const gboolean disconnected
          = g_dbus_connection_is_closed(ibus_connection);
      if (disconnected) {
        // |ibus_| object is not NULL, but the connection between ibus-daemon
        // is not yet established. In this case, we don't create |ibus_config_|
        // object.
        LOG(ERROR) << "Couldn't create an ibus config object since "
                   << "IBus connection is closed.";
        return;
      }
      // If memconf is not successfully started yet, ibus_config_new() will
      // return NULL. Otherwise, it returns a transfer-none and non-floating
      // object. ibus_config_new() sometimes issues a D-Bus *synchronous* IPC
      // to check if the org.freedesktop.IBus.Config service is available.
      ibus_config_ = ibus_config_new(ibus_connection,
                                     NULL /* do not cancel the operation */,
                                     NULL /* do not get error information */);
      if (!ibus_config_) {
        LOG(ERROR) << "ibus_config_new() failed. ibus-memconf is not ready?";
        return;
      }

      // TODO(yusukes): g_object_weak_ref might be better since it allows
      // libcros to detect the delivery of the "destroy" glib signal the
      // |ibus_config_| object.
      g_object_ref(ibus_config_);
      LOG(INFO) << "ibus_config_ is ready.";
    }
  }

  // Destroys IBusConfig object if |ibus_| connection is not ready. This
  // function does nothing if |ibus_config_| is NULL or |ibus_| connection is
  // still alive. Note that the IBusConfig object can't be used when |ibus_|
  // connection is not ready.
  void MaybeDestroyIBusConfig() {
    if (!ibus_) {
      LOG(ERROR) << "MaybeDestroyIBusConfig: ibus_ is NULL";
      return;
    }
    if (ibus_config_ && !ibus_bus_is_connected(ibus_)) {
      g_object_unref(ibus_config_);
      ibus_config_ = NULL;
    }
  }

  // Handles "FocusIn" signal from chromeos_input_method_ui.
  void FocusIn(const char* input_context_path) {
    if (!input_context_path) {
      LOG(ERROR) << "NULL context passed";
    } else {
      DLOG(INFO) << "FocusIn: " << input_context_path;
    }
    // Remember the current ic path.
    input_context_path_ = Or(input_context_path, "");
  }

  // Handles "RegisterProperties" signal from chromeos_input_method_ui.
  void RegisterProperties(IBusPropList* ibus_prop_list) {
    DLOG(INFO) << "RegisterProperties" << (ibus_prop_list ? "" : " (clear)");

    ImePropertyList prop_list;  // our representation.
    if (ibus_prop_list) {
      // You can call
      //   LOG(INFO) << "\n" << PrintPropList(ibus_prop_list, 0);
      // here to dump |ibus_prop_list|.
      if (!FlattenPropertyList(ibus_prop_list, &prop_list)) {
        // Clear properties on errors.
        RegisterProperties(NULL);
        return;
      }
    }
    // Notify the change.
    register_ime_properties_(language_library_, prop_list);
  }

  // Handles "UpdateProperty" signal from chromeos_input_method_ui.
  void UpdateProperty(IBusProperty* ibus_prop) {
    DLOG(INFO) << "UpdateProperty";
    DCHECK(ibus_prop);

    // You can call
    //   LOG(INFO) << "\n" << PrintProp(ibus_prop, 0);
    // here to dump |ibus_prop|.

    ImePropertyList prop_list;  // our representation.
    if (!FlattenProperty(ibus_prop, &prop_list)) {
      // Don't update the UI on errors.
      LOG(ERROR) << "Malformed properties are detected";
      return;
    }
    // Notify the change.
    if (!prop_list.empty()) {
      update_ime_property_(language_library_, prop_list);
    }
  }

  // Retrieves input method status and notifies them to the UI.
  // |current_global_engine_id| is the current global engine id such as "mozc"
  // and "xkb:us::eng". If the id is unknown, an empty string "" can be passed.
  // Warning: you can call this function only from ibus callback functions
  // like FocusIn(). See http://crosbug.com/5217#c9 for details.
  void UpdateUI(const char* current_global_engine_id) {
    DCHECK(current_global_engine_id);

    const IBusEngineInfo* engine_info = NULL;
    for (size_t i = 0; i < arraysize(kIBusEngines); ++i) {
      if (kIBusEngines[i].name == std::string(current_global_engine_id)) {
        engine_info = &kIBusEngines[i];
        break;
      }
    }

    if (!engine_info) {
      LOG(ERROR) << current_global_engine_id
                 << " is not found in the input method white-list.";
      return;
    }

    InputMethodDescriptor current_input_method =
        CreateInputMethodDescriptor(engine_info->name,
                                    engine_info->longname,
                                    engine_info->layout,
                                    engine_info->language);

    DLOG(INFO) << "Updating the UI. ID:" << current_input_method.id
               << ", keyboard_layout:" << current_input_method.keyboard_layout;

    // Notify the change to update UI.
    current_input_method_changed_(language_library_, current_input_method);
  }

  // Installs gobject signal handlers to |ibus_|.
  void ConnectIBusSignals() {
    if (!ibus_) {
      return;
    }

    // We use g_signal_connect_after here since the callback should be called
    // *after* the IBusBusDisconnectedCallback in chromeos_input_method_ui.cc
    // is called. chromeos_input_method_ui.cc attaches the panel service object
    // to |ibus_|, and the callback in this file use the attached object.
    g_signal_connect_after(ibus_,
                           "connected",
                           G_CALLBACK(IBusBusConnectedCallback),
                           this);

    g_signal_connect(ibus_,
                     "disconnected",
                     G_CALLBACK(IBusBusDisconnectedCallback),
                     this);
    g_signal_connect(ibus_,
                     "global-engine-changed",
                     G_CALLBACK(IBusBusGlobalEngineChangedCallback),
                     this);
    g_signal_connect(ibus_,
                     "name-owner-changed",
                     G_CALLBACK(IBusBusNameOwnerChangedCallback),
                     this);
  }

  // Installs gobject signal handlers to the panel service.
  void ConnectPanelServiceSignals() {
    if (!ibus_) {
      return;
    }

    IBusPanelService* ibus_panel_service = IBUS_PANEL_SERVICE(
        g_object_get_data(G_OBJECT(ibus_), kPanelObjectKey));
    if (!ibus_panel_service) {
      LOG(ERROR) << "IBusPanelService is NOT available.";
      return;
    }
    // We don't _ref() or _weak_ref() the panel service object, since we're not
    // interested in the life time of the object.

    g_signal_connect(ibus_panel_service,
                     "focus-in",
                     G_CALLBACK(FocusInCallback),
                     this);
    g_signal_connect(ibus_panel_service,
                     "register-properties",
                     G_CALLBACK(RegisterPropertiesCallback),
                     this);
    g_signal_connect(ibus_panel_service,
                     "update-property",
                     G_CALLBACK(UpdatePropertyCallback),
                     this);
  }

  // Handles "connected" signal from ibus-daemon.
  static void IBusBusConnectedCallback(IBusBus* bus, gpointer user_data) {
    LOG(WARNING) << "IBus connection is recovered.";
    // ibus-daemon might be restarted, or the daemon was not running when Chrome
    // started. Anyway, since |ibus_| connection is now ready, it's possible to
    // create |ibus_config_| object by calling MaybeRestoreConnections().
    g_return_if_fail(user_data);
    InputMethodStatusConnection* self
        = static_cast<InputMethodStatusConnection*>(user_data);
    self->MaybeRestoreConnections();
  }

  // Handles "disconnected" signal from ibus-daemon.
  static void IBusBusDisconnectedCallback(IBusBus* bus, gpointer user_data) {
    LOG(WARNING) << "IBus connection is terminated.";
    g_return_if_fail(user_data);
    InputMethodStatusConnection* self
        = static_cast<InputMethodStatusConnection*>(user_data);
    // ibus-daemon might be terminated. Since |ibus_| object will automatically
    // connect to the daemon if it restarts, we don't have to set NULL on ibus_.
    // Call MaybeDestroyIBusConfig() to set |ibus_config_| to NULL temporarily.
    self->MaybeDestroyIBusConfig();
    if (self->connection_change_handler_) {
      LOG(INFO) << "Notifying Chrome that IBus is terminated.";
      self->connection_change_handler_(self->language_library_, false);
    }
  }

  // Handles "global-engine-changed" signal from ibus-daemon.
  static void IBusBusGlobalEngineChangedCallback(
      IBusBus* bus, const gchar* engine_name, gpointer user_data) {
    DCHECK(engine_name);
    DLOG(INFO) << "Global engine is changed to " << engine_name;
    g_return_if_fail(user_data);
    InputMethodStatusConnection* self
        = static_cast<InputMethodStatusConnection*>(user_data);
    self->UpdateUI(engine_name);
  }

  // Handles "name-owner-changed" signal from ibus-daemon. The signal is sent
  // to libcros when an IBus component such as ibus-memconf, ibus-engine-*, ..
  // is started.
  static void IBusBusNameOwnerChangedCallback(
      IBusBus* bus,
      const gchar* name, const gchar* old_name, const gchar* new_name,
      gpointer user_data) {
    DCHECK(name);
    DCHECK(old_name);
    DCHECK(new_name);
    DLOG(INFO) << "Name owner is changed: name=" << name
               << ", old_name=" << old_name << ", new_name=" << new_name;

    if (name != std::string("org.freedesktop.IBus.Config")) {
      // Not a signal for ibus-memconf.
      return;
    }

    const std::string empty_string;
    if (old_name != empty_string || new_name == empty_string) {
      // ibus-memconf died?
      LOG(WARNING) << "Unexpected name owner change: name=" << name
                   << ", old_name=" << old_name << ", new_name=" << new_name;
      // TODO(yusukes): it might be nice to set |ibus_config_| to NULL and call
      // |connection_change_handler_| with false here to allow Chrome to
      // recover all input method configurations when ibus-memconf is
      // automatically restarted by ibus-daemon. Though ibus-memconf is pretty
      // stable and unlikely crashes.
      return;
    }

    LOG(INFO) << "IBus config daemon is started. Recovering ibus_config_";
    g_return_if_fail(user_data);
    InputMethodStatusConnection* self
        = static_cast<InputMethodStatusConnection*>(user_data);

    // Try to recover |ibus_config_|. If the |ibus_config_| object is
    // successfully created, |connection_change_handler_| will be called to
    // notify Chrome that IBus is ready.
    self->MaybeRestoreConnections();
  }

  // Handles "FocusIn" signal from chromeos_input_method_ui.
  static void FocusInCallback(IBusPanelService* panel,
                              const gchar* path,
                              gpointer user_data) {
    g_return_if_fail(user_data);
    InputMethodStatusConnection* self
        = static_cast<InputMethodStatusConnection*>(user_data);
    self->FocusIn(path);
  }

  // Handles "RegisterProperties" signal from chromeos_input_method_ui.
  static void RegisterPropertiesCallback(IBusPanelService* panel,
                                         IBusPropList* prop_list,
                                         gpointer user_data) {
    g_return_if_fail(user_data);
    InputMethodStatusConnection* self
        = static_cast<InputMethodStatusConnection*>(user_data);
    self->RegisterProperties(prop_list);
  }

  // Handles "UpdateProperty" signal from chromeos_input_method_ui.
  static void UpdatePropertyCallback(IBusPanelService* panel,
                                     IBusProperty* ibus_prop,
                                     gpointer user_data) {
    g_return_if_fail(user_data);
    InputMethodStatusConnection* self
        = static_cast<InputMethodStatusConnection*>(user_data);
    self->UpdateProperty(ibus_prop);
  }

  // A callback function that will be called when ibus_config_set_value_async()
  // request is finished.
  static void SetImeConfigCallback(GObject* source_object,
                                   GAsyncResult* res,
                                   gpointer user_data) {
    IBusConfig* config = IBUS_CONFIG(user_data);
    g_return_if_fail(config);

    GError* error = NULL;
    const gboolean result =
        ibus_config_set_value_async_finish(config, res, &error);

    if (!result) {
      std::string message = "(unknown error)";
      if (error && error->message) {
        message = error->message;
      }
      LOG(ERROR) << "ibus_config_set_value_async failed: " << message;
    }

    if (error) {
      g_error_free(error);
    }
    g_object_unref(config);
  }

  // A function pointers which point LanguageLibrary::XXXHandler functions.
  // The functions are used when libcros receives signals from ibus-daemon.
  LanguageCurrentInputMethodMonitorFunction current_input_method_changed_;
  LanguageRegisterImePropertiesFunction register_ime_properties_;
  LanguageUpdateImePropertyFunction update_ime_property_;
  LanguageConnectionChangeMonitorFunction connection_change_handler_;

  // Points to a chromeos::LanguageLibrary object. |language_library_| is used
  // as the first argument of the monitor functions above.
  void* language_library_;

  // Connection to the ibus-daemon via IBus API. These objects are used to
  // call ibus-daemon's API (e.g. activate input methods, set config, ...)
  IBusBus* ibus_;
  IBusConfig* ibus_config_;

  // Current input context path.
  std::string input_context_path_;
};

//
// cros APIs
//

// The function will be bound to chromeos::MonitorInputMethodStatus with dlsym()
// in load.cc so it needs to be in the C linkage, so the symbol name does not
// get mangled.
extern "C"
InputMethodStatusConnection* ChromeOSMonitorInputMethodStatus(
    void* language_library,
    LanguageCurrentInputMethodMonitorFunction current_input_method_changed,
    LanguageRegisterImePropertiesFunction register_ime_properties,
    LanguageUpdateImePropertyFunction update_ime_property,
    LanguageConnectionChangeMonitorFunction connection_changed) {
  DLOG(INFO) << "MonitorInputMethodStatus";
  return InputMethodStatusConnection::GetConnection(
      language_library,
      current_input_method_changed,
      register_ime_properties,
      update_ime_property,
      connection_changed);
}

extern "C"
bool ChromeOSStopInputMethodProcess(InputMethodStatusConnection* connection) {
  g_return_val_if_fail(connection, false);
  return connection->StopInputMethodProcess();
}

extern "C"
InputMethodDescriptors* ChromeOSGetSupportedInputMethodDescriptors() {
  InputMethodDescriptors* input_methods = new InputMethodDescriptors;
  for (size_t i = 0; i < arraysize(chromeos::kIBusEngines); ++i) {
    if (InputMethodIdIsWhitelisted(chromeos::kIBusEngines[i].name)) {
      input_methods->push_back(chromeos::CreateInputMethodDescriptor(
          chromeos::kIBusEngines[i].name,
          chromeos::kIBusEngines[i].longname,
          chromeos::kIBusEngines[i].layout,
          chromeos::kIBusEngines[i].language));
    }
  }
  return input_methods;
}

extern "C"
void ChromeOSSetImePropertyActivated(
    InputMethodStatusConnection* connection, const char* key, bool activated) {
  DLOG(INFO) << "SetImePropertyeActivated: " << key << ": " << activated;
  DCHECK(key);
  g_return_if_fail(connection);
  connection->SetImePropertyActivated(key, activated);
}

extern "C"
bool ChromeOSChangeInputMethod(
    InputMethodStatusConnection* connection, const char* name) {
  DCHECK(name);
  DLOG(INFO) << "ChangeInputMethod: " << name;
  g_return_val_if_fail(connection, false);
  return connection->ChangeInputMethod(name);
}

extern "C"
bool ChromeOSSetImeConfig(InputMethodStatusConnection* connection,
                          const char* section,
                          const char* config_name,
                          const ImeConfigValue& value) {
  DCHECK(section);
  DCHECK(config_name);
  g_return_val_if_fail(connection, FALSE);
  return connection->SetImeConfig(section, config_name, value);
}

extern "C"
std::string ChromeOSGetKeyboardOverlayId(const std::string& input_method_id) {
  for (size_t i = 0; i < arraysize(kKeyboardOverlayMap); ++i) {
    if (kKeyboardOverlayMap[i].input_method_id == input_method_id) {
      return kKeyboardOverlayMap[i].keyboard_overlay_id;
    }
  }
  return "";
}

extern "C"
void ChromeOSSendHandwritingStroke(InputMethodStatusConnection* connection,
                                   const HandwritingStroke& stroke) {
  g_return_if_fail(connection);
  connection->SendHandwritingStroke(stroke);
}

extern "C"
void ChromeOSCancelHandwriting(InputMethodStatusConnection* connection,
                               int n_strokes) {
  g_return_if_fail(connection);
  connection->CancelHandwriting(n_strokes);
}

}  // namespace chromeos
