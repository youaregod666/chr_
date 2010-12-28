// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_input_method.h"
#include "chromeos_input_method_whitelist.h"

#include <ibusversion.h>

#if !IBUS_CHECK_VERSION(1, 3, 99)
#include <dbus/dbus-glib-lowlevel.h>  // for dbus_g_connection_get_connection.
#endif
#include <ibus.h>

#include <algorithm>  // for std::reverse.
#include <cstdio>
#include <cstring>  // for std::strcmp.
#include <set>
#include <sstream>
#include <stack>
#include <utility>

#include "base/singleton.h"
#if !IBUS_CHECK_VERSION(1, 3, 99)
#include "chromeos/dbus/dbus.h"
#include "chromeos/glib/object.h"
#endif
#include "ibus_input_methods.h"

namespace {

// TODO(yusukes): Remove all code for ibus-1.3 once we finish migrating to 1.4.
#if !IBUS_CHECK_VERSION(1, 3, 99)
#define ibus_engine_desc_get_name(d) ((d)->name)
#define ibus_engine_desc_get_longname(d) ((d)->longname)
#define ibus_engine_desc_get_layout(d) ((d)->layout)
#define ibus_engine_desc_get_language(d) ((d)->language)

const char kCandidateWindowService[] = "org.freedesktop.IBus.Panel";
const char kCandidateWindowObjectPath[] = "/org/chromium/Chrome/LanguageBar";
const char kCandidateWindowInterface[] = "org.freedesktop.IBus.Panel";
#endif

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

// Returns true if |input_method_id| is whitelisted.
// TODO(yusukes): Write unittest.
bool InputMethodIdIsWhitelisted(const char* input_method_id) {
  static std::set<std::string>* g_supported_input_methods = NULL;
  if (!g_supported_input_methods) {
    g_supported_input_methods = new std::set<std::string>;
    for (size_t i = 0; i < arraysize(kInputMethodIdsWhitelist); ++i) {
      g_supported_input_methods->insert(kInputMethodIdsWhitelist[i]);
    }
  }
  return (g_supported_input_methods->count(input_method_id) > 0);
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
void AddInputMethodNames(
    const GList* engines, chromeos::InputMethodDescriptors* out) {
  DCHECK(out);
  for (; engines; engines = g_list_next(engines)) {
    IBusEngineDesc* engine_desc = IBUS_ENGINE_DESC(engines->data);
    const gchar* name = ibus_engine_desc_get_name(engine_desc);
    const gchar* longname = ibus_engine_desc_get_longname(engine_desc);
    const gchar* layout = ibus_engine_desc_get_layout(engine_desc);
    const gchar* language = ibus_engine_desc_get_language(engine_desc);
    if (InputMethodIdIsWhitelisted(name)) {
      out->push_back(chromeos::InputMethodDescriptor(name,
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
#if IBUS_CHECK_VERSION(1, 3, 99)
  GDBusConnection* connection = ibus_bus_get_connection(ibus);
#else
  IBusConnection* connection = ibus_bus_get_connection(ibus);
#endif
  if (!connection) {
    LOG(ERROR) << "IBusConnection is null";
    return NULL;
  }
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
                     chromeos::ImePropertyList* out_prop_list) {
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
      selection_item_id : chromeos::ImeProperty::kInvalidSelectionItemId;

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

  out_prop_list->push_back(chromeos::ImeProperty(ibus_prop->key,
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
bool FlattenProperty(
    IBusProperty* ibus_prop, chromeos::ImePropertyList* out_prop_list) {
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
    IBusPropList* ibus_prop_list, chromeos::ImePropertyList* out_prop_list) {
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

namespace chromeos {

// A singleton class that holds IBus and DBus connections.
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

    InputMethodStatusConnection* object = Singleton<InputMethodStatusConnection,
        LeakySingletonTraits<InputMethodStatusConnection> >::get();
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

  // Restores IBus and DBus connections if they are not ready.
  void MaybeRestoreConnections() {
    MaybeCreateIBus();
    MaybeRestoreIBusConfig();
#if !IBUS_CHECK_VERSION(1, 3, 99)
    MaybeRestoreDBus();
#endif
  }

  // InputMethodType is used for GetInputMethods().
  enum InputMethodType {
    kActiveInputMethods,  // Get active input methods.
    kSupportedInputMethods,  // Get supported input methods.
  };

  // Called by cros API ChromeOSGet(Active|Supported)InputMethods().
  // Returns a list of input methods that are currently active or supported
  // depending on |mode|. Returns NULL on error.
  InputMethodDescriptors* GetInputMethods(InputMethodType type) {
    if (type == kActiveInputMethods &&
        active_engines_.empty() &&
        !IBusConnectionIsAlive()) {
      LOG(ERROR) << "GetInputMethods: IBus connection is not alive";
      return NULL;
    }

    InputMethodDescriptors* input_methods = new InputMethodDescriptors;
    if (type == kActiveInputMethods && IBusConnectionIsAlive()) {
      GList* engines = NULL;
      engines = ibus_bus_list_active_engines(ibus_);
      // Note that it's not an error for |engines| to be NULL.
      // NULL simply means an empty GList.
      AddInputMethodNames(engines, input_methods);
      FreeInputMethodNames(engines);
      // An empty list is returned if preload engines has not yet been set.
      // If that happens, we instead use our cached list.
      if (input_methods->empty()) {
        AddIBusInputMethodNames(type, input_methods);
      }
    } else {
      AddIBusInputMethodNames(type, input_methods);
    }

    return input_methods;
  }

  bool SetActiveInputMethods(const ImeConfigValue& value) {
    DCHECK(value.type == ImeConfigValue::kValueTypeStringList);

    // Sanity check: do not preload unknown/unsupported input methods.
    std::vector<std::string> string_list;
    FilterInputMethods(value.string_list_value, &string_list);

    active_engines_.clear();
    active_engines_.insert(string_list.begin(), string_list.end());
    return true;
  }

  // Called by cros API ChromeOS(Activate|Deactive)ImeProperty().
  void SetImePropertyActivated(const char* key, bool activated) {
    if (!IBusConnectionIsAlive()) {
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
    if (!IBusConnectionIsAlive()) {
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

    if (!ibus_bus_set_global_engine(ibus_, name)) {
      return false;
    }

    // Sometimes ibus_bus_set_global_engine() fails, but still returns true.
    // Therefore, we need to check that the engine was actually set.
    // See: http://crosbug.com/5188
    IBusEngineDesc* engine_desc = ibus_bus_get_global_engine(ibus_);
    if (!engine_desc) {
      return false;
    }
    const bool success =
        (0 == strcmp(name, ibus_engine_desc_get_name(engine_desc)));
    g_object_unref(engine_desc);
    return success;
  }

  // Get a configuration of ibus-daemon or IBus engines and stores it on
  // |out_value|. Returns true if |gvalue| is successfully updated.
  //
  // For more information, please read a comment for GetImeConfig() function
  // in chromeos_language.h.
  bool GetImeConfig(const char* section,
                    const char* config_name,
                    ImeConfigValue* out_value) {
    // Check if the connection is alive. The config object is invalidated
    // if the connection has been closed.
    if (!IBusConnectionIsAlive()) {
      LOG(ERROR) << "GetImeConfig: IBus connection is not alive";
      return false;
    }

    DCHECK(section);
    DCHECK(config_name);
    if (!section || !config_name) {
      return false;
    }

#if IBUS_CHECK_VERSION(1, 3, 99)
    // IBus-1.4 uses GVariant.
    GVariant* variant
      = ibus_config_get_value(ibus_config_, section, config_name);
    if (!variant) {
      LOG(ERROR) << "GetImeConfig: ibus_config_get_value returned NULL";
      return false;
    }

    bool success = true;
    switch(g_variant_classify(variant)) {
      case G_VARIANT_CLASS_STRING: {
        const char* value = g_variant_get_string(variant, NULL);
        DCHECK(value);
        out_value->type = ImeConfigValue::kValueTypeString;
        out_value->string_value = value;
        break;
      }
      case G_VARIANT_CLASS_INT32: {
        out_value->type = ImeConfigValue::kValueTypeInt;
        out_value->int_value = g_variant_get_int32(variant);
        break;
      }
      case G_VARIANT_CLASS_BOOLEAN: {
        out_value->type = ImeConfigValue::kValueTypeBool;
        out_value->bool_value = g_variant_get_boolean(variant);
        break;
      }
      case G_VARIANT_CLASS_ARRAY: {
        const GVariantType* variant_element_type
            = g_variant_type_element(g_variant_get_type(variant));
        if (g_variant_type_equal(variant_element_type, G_VARIANT_TYPE_STRING)) {
          out_value->type = ImeConfigValue::kValueTypeStringList;
          out_value->string_list_value.clear();

          GVariantIter iter;
          g_variant_iter_init(&iter, variant);
          GVariant* element = g_variant_iter_next_value(&iter);
          while (element) {
            const char* value = g_variant_get_string(element, NULL);
            DCHECK(value);
            out_value->string_list_value.push_back(value);
            g_variant_unref(element);
            element = g_variant_iter_next_value(&iter);
          }
        } else {
          // TODO(yusukes): Support other array types if needed.
          LOG(ERROR) << "Unsupported array type.";
          success = false;
        }
        break;
      }
      default: {
        // TODO(yusukes): Support other types like DOUBLE if needed.
        LOG(ERROR) << "Unsupported config type.";
        success = false;
        break;
      }
    }

    g_variant_unref(variant);
#else
    // IBus-1.3 uses GValue.
    GValue gvalue = { 0 };  // g_value_init() is not necessary.
    bool success =
        ibus_config_get_value(ibus_config_, section, config_name, &gvalue);
    if (!success) {
      if (G_IS_VALUE(&gvalue)) {
        g_value_unset(&gvalue);
      }
      return false;
    }

    // Convert the type of the result from GValue to our structure.
    GType type = G_VALUE_TYPE(&gvalue);
    if (type == G_TYPE_STRING) {
      const char* value = g_value_get_string(&gvalue);
      DCHECK(value);
      out_value->type = ImeConfigValue::kValueTypeString;
      out_value->string_value = value;
    } else if (type == G_TYPE_INT) {
      out_value->type = ImeConfigValue::kValueTypeInt;
      out_value->int_value = g_value_get_int(&gvalue);
    } else if (type == G_TYPE_BOOLEAN) {
      out_value->type = ImeConfigValue::kValueTypeBool;
      out_value->bool_value = g_value_get_boolean(&gvalue);
    } else if (type == G_TYPE_VALUE_ARRAY) {
      out_value->type = ImeConfigValue::kValueTypeStringList;
      out_value->string_list_value.clear();
      GValueArray* array
          = reinterpret_cast<GValueArray*>(g_value_get_boxed(&gvalue));
      for (guint i = 0; array && (i < array->n_values); ++i) {
        const GType element_type = G_VALUE_TYPE(&(array->values[i]));
        if (element_type != G_TYPE_STRING) {
          LOG(ERROR) << "Array element type is not STRING: " << element_type;
          g_value_unset(&gvalue);
          return false;
        }
        const char* value = g_value_get_string(&(array->values[i]));
        DCHECK(value);
        out_value->string_list_value.push_back(value);
      }
    } else {
      LOG(ERROR) << "Unsupported config type: " << G_VALUE_TYPE(&gvalue);
      success = false;
    }

    g_value_unset(&gvalue);
#endif
    return success;
  }

  // Updates a configuration of ibus-daemon or IBus engines with |value|.
  // Returns true if the configuration is successfully updated.
  //
  // For more information, please read a comment for SetImeConfig() function
  // in chromeos_language.h.
  bool SetImeConfig(const char* section,
                    const char* config_name,
                    const ImeConfigValue& value) {
    // See comments in GetImeConfig() where ibus_config_get_value() is used.
    if (!IBusConnectionIsAlive()) {
      LOG(ERROR) << "SetImeConfig: IBus connection is not alive";
      return false;
    }

    DCHECK(section);
    DCHECK(config_name);
    if (!section || !config_name) {
      return false;
    }

    // Sanity check: do not preload unknown/unsupported input methods.
    std::vector<std::string> string_list;
    if ((value.type == ImeConfigValue::kValueTypeStringList) &&
        !strcmp(section, kGeneralSectionName) &&
        !strcmp(config_name, kPreloadEnginesConfigName)) {
      FilterInputMethods(value.string_list_value, &string_list);
    } else {
      string_list = value.string_list_value;
    }

#if IBUS_CHECK_VERSION(1, 3, 99)
    // IBus-1.4 uses GVariant.
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
    const gboolean success = ibus_config_set_value(ibus_config_,
                                                   section,
                                                   config_name,
                                                   variant);
    g_variant_unref(variant);
#else
    // IBus-1.3 uses GValue.
    // Convert the type of |value| from our structure to GValue.
    GValue gvalue = { 0 };
    switch (value.type) {
      case ImeConfigValue::kValueTypeString:
        g_value_init(&gvalue, G_TYPE_STRING);
        g_value_set_string(&gvalue, value.string_value.c_str());
        break;
      case ImeConfigValue::kValueTypeInt:
        g_value_init(&gvalue, G_TYPE_INT);
        g_value_set_int(&gvalue, value.int_value);
        break;
      case ImeConfigValue::kValueTypeBool:
        g_value_init(&gvalue, G_TYPE_BOOLEAN);
        g_value_set_boolean(&gvalue, value.bool_value);
        break;
      case ImeConfigValue::kValueTypeStringList:
        g_value_init(&gvalue, G_TYPE_VALUE_ARRAY);
        const size_t size = string_list.size();  // don't use string_list_value.
        GValueArray* array = g_value_array_new(size);
        for (size_t i = 0; i < size; ++i) {
          GValue array_element = {0};
          g_value_init(&array_element, G_TYPE_STRING);
          g_value_set_string(&array_element, string_list[i].c_str());
          g_value_array_append(array, &array_element);
        }
        g_value_take_boxed(&gvalue, array);
        break;
    }

    const gboolean success = ibus_config_set_value(ibus_config_,
                                                   section,
                                                   config_name,
                                                   &gvalue);
    g_value_unset(&gvalue);
#endif

    DLOG(INFO) << "SetImeConfig: " << section << "/" << config_name
               << ": result=" << success;
    return (success == TRUE);
  }

  // Checks if IBus connection is alive. Returns true even if |dbus_connection_|
  // and/or |dbus_proxy_| are not ready yet, since we can call ibus-daemon APIs
  // without the DBus (candidate_window) connection.
  bool IBusConnectionIsAlive() {
    return ibus_ && ibus_bus_is_connected(ibus_) && ibus_config_;
  }

#if !IBUS_CHECK_VERSION(1, 3, 99)
  // Checks if the DBus connection (to ibus-daemon) is alive.
  bool DBusConnectionIsAlive() {
    if (!dbus_connection_.get()) {
      return false;
    }
    if (!dbus_connection_->g_connection()) {
      return false;
    }
    return dbus_connection_get_is_connected(dbus_g_connection_get_connection(
        dbus_connection_->g_connection()));
  }
#endif

 private:
  friend struct DefaultSingletonTraits<InputMethodStatusConnection>;
  InputMethodStatusConnection()
      : current_input_method_changed_(NULL),
        register_ime_properties_(NULL),
        update_ime_property_(NULL),
        connection_change_handler_(NULL),
        language_library_(NULL),
        ibus_(NULL),
        ibus_config_(NULL),
        notify_focus_in_count_(0) {
  }

  ~InputMethodStatusConnection() {
    // TODO(yusukes): rewrite the comment for IBus-1.4.

    // Since the class is used as a leaky singleton, this destructor is never
    // called. However, if you would delete an instance of this class, you have
    // to disconnect all signals so the handler functions will not be called
    // with |this| which is already freed.
    //
    // if (dbus_proxy_) {
    //  g_signal_handlers_disconnect_by_func(
    //      dbus_proxy_.gproxy(),
    //      reinterpret_cast<gpointer>(G_CALLBACK(DBusProxyDestroyCallback)),
    //      this);
    // }
    // if (ibus_) {
    //   g_signal_handlers_disconnect_by_func(
    //       ibus_,
    //       reinterpret_cast<gpointer>(
    //           G_CALLBACK(IBusBusConnectedCallback)),
    //       this);
    //   g_signal_handlers_disconnect_by_func(
    //       ibus_,
    //       reinterpret_cast<gpointer>(
    //           G_CALLBACK(IBusBusDisconnectedCallback)),
    //       this);
    //   g_signal_handlers_disconnect_by_func(
    //       ibus_,
    //       reinterpret_cast<gpointer>(
    //           G_CALLBACK(IBusBusGlobalEngineChangedCallback)),
    //       this);
    // }
    //
    // Second, it is not a good idea to close |dbus_connection_| here since it
    // actually shares the same socket file descriptor with the connection used
    // in IBusBus. If we close |dbus_connection_| here, the connection used in
    // the IBus IM module (im-ibus.so) will also be closed, that causes
    // input methods to malfunction in Chrome.
    //
    // Note that not closing |dbus_connection_| produces DBus warnings
    // like below, but it's ok as warnings are not critical (See also
    // crosbug.com/3596):
    //
    // "The last reference on a connection was dropped without closing the
    // connection."
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
    if (ibus_bus_is_connected(ibus_)) {
      LOG(INFO) << "ibus_bus_is_connected(). IBus connection is ready!";
#if IBUS_CHECK_VERSION(1, 3, 99)
      AddMatchRules();
      // TODO(yusukes): should we call connection_change_handler_() here?
#endif
    } else {
      // We should not reach this line since /etc/init/ibus.conf ensures that
      // Chrome starts after ibus-daemon writes its socket file. In this case,
      // the IBusBusConnectedCallback will be called later. All IBus API calls
      // before the "connected" signel would be lost.
      LOG(ERROR) << "ibus_bus_is_connected() returned false. "
                 << "IBus connection is NOT ready. Chrome has started before "
                 << "ibus-daemon starts?";
    }

    // Register three callback functions for IBusBus signals.
    g_signal_connect(ibus_,
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
  }

  // Creates IBusConfig object if it's not created yet AND |ibus_| connection
  // is ready.
  void MaybeRestoreIBusConfig() {
    if (!ibus_) {
      return;
    }

    if (ibus_config_ && !ibus_bus_is_connected(ibus_)) {
      // IBusConfig object can't be used when IBusBus connection is dead.
#if IBUS_CHECK_VERSION(1, 3, 99)
      g_object_unref(ibus_config_);
#endif
      ibus_config_ = NULL;
      // TODO(yusukes): On IBus-1.3, currently there's no safe way to unref the
      // |ibus_config_| object. See http://crosbug.com/3962 and
      // http://code.google.com/p/ibus/issues/detail?id=959 for details.
    }

    if (!ibus_config_) {
#if IBUS_CHECK_VERSION(1, 3, 99)
      GDBusConnection* ibus_connection = ibus_bus_get_connection(ibus_);
#else
      IBusConnection* ibus_connection = ibus_bus_get_connection(ibus_);
#endif
      if (!ibus_connection) {
        LOG(ERROR) << "ibus_bus_get_connection() failed. ibus-daemon is "
                   << "restarted and |ibus_| connection is not recovered yet?";
        return;
      }
#if IBUS_CHECK_VERSION(1, 3, 99)
      const gboolean disconnected
          = g_dbus_connection_is_closed(ibus_connection);
#else
      const gboolean disconnected
          = !ibus_connection_is_connected(ibus_connection);
#endif
      if (disconnected) {
        // |ibus_| object is not NULL, but the connection between ibus-daemon
        // is not yet established. In this case, we don't create |ibus_config_|
        // object.
        LOG(WARNING) << "Couldn't create an ibus config object since "
                     << "ibus_connection_is_connected() returned false.";
        return;
      }
      // If memconf has not successfully started yet, ibus_config_new() will
      // return NULL.
#if IBUS_CHECK_VERSION(1, 3, 99)
      ibus_config_ = ibus_config_new(ibus_connection,
                                     NULL /* do not cancel the operation */,
                                     NULL /* do not get error information */);
#else
      ibus_config_ = ibus_config_new(ibus_connection);
#endif
      if (!ibus_config_) {
        LOG(ERROR) << "ibus_config_new() failed";
        return;
      }
      g_object_ref_sink(ibus_config_);  // ibus_config_ is floating.
    }
  }

#if !IBUS_CHECK_VERSION(1, 3, 99)
  // Creates |dbus_connection_| and |dbus_proxy_| objects, that are necessary
  // to receive signals from candidate_window.
  void MaybeRestoreDBus() {
    if (dbus_connection_.get() && !DBusConnectionIsAlive()) {
      LOG(ERROR) << "DBus connection to ibus-daemon is dead (ibus-daemon "
                 << "restarted?). Discard dbus_connection_ and dbus_proxy_ "
                 << "objects.";
      dbus_proxy_ = dbus::Proxy();
      dbus_connection_.reset();
      // The "The last reference on a connection was dropped without closing the
      // connection" error should not be displayed since the connection is
      // already closed.
    }

    if (!dbus_connection_.get()) {
      // Establish a DBus connection between the candidate_window process for
      // Chromium OS to handle signals (e.g. "FocusIn") from the process.
      const char* address = ibus_get_address();
      if (!address) {
        LOG(ERROR) << "Can't create DBus connection object since "
                   << "ibus_get_address() returned NULL. The socket file of "
                   << "ibus-daemon is not ready?";
        return;
      }

      dbus_connection_.reset(
          new dbus::BusConnection(dbus::GetPrivateBusConnection(address)));
      if (!dbus_connection_->HasConnection()) {
        // When a user logs out, ibus-daemon might be terminated by Upstart
        // before Chrome exits.
        dbus_connection_.reset();
        LOG(ERROR) << "Can't create DBus connection object since "
                   << "dbus_connection_open_private() failed. The socket file "
                   << "of ibus-daemon exitst, but ibus-daemon is not running?";
        return;
      }
      LOG(INFO) << "Established private DBus connection to: " << address;
    }

    if (!dbus_proxy_) {  // If not connected.
      // Connect to the candidate_window. Note that dbus_connection_add_filter()
      // does not work without calling dbus::Proxy().
      const bool kConnectToNameOwner = true;
      dbus_proxy_ = dbus::Proxy(*dbus_connection_,
                                kCandidateWindowService,
                                kCandidateWindowObjectPath,
                                kCandidateWindowInterface,
                                kConnectToNameOwner);
      if (!dbus_proxy_) {
        // candidate_window is not ready yet. We should not delete the
        // |dbus_connection_| here since we can reuse the object in the next
        // MaybeRestoreDBus() call.
        return;
      }

      // Register the callback function for "destroy".
      g_signal_connect(dbus_proxy_.gproxy(),
                       "destroy",
                       G_CALLBACK(DBusProxyDestroyCallback),
                       this);
      // Register DBus signal handler.
      dbus_connection_add_filter(
          dbus_g_connection_get_connection(dbus_connection_->g_connection()),
          &InputMethodStatusConnection::DispatchSignalFromCandidateWindow,
          this, NULL);
      LOG(INFO) << "Proxy object for the candidate_window is ready!";
    }
  }
#endif

  // Handles "FocusIn" signal from the candidate_window process.
  void FocusIn(const char* input_context_path) {
    if (!input_context_path) {
      LOG(ERROR) << "NULL context passed";
    } else {
      DLOG(INFO) << "FocusIn: " << input_context_path;
    }

    // Remember the current ic path.
    input_context_path_ = Or(input_context_path, "");

    if (notify_focus_in_count_ < kMaxNotifyFocusInCount) {
      // Usually, we don't have to update the UI on FocusIn since the status of
      // IBus is not per input context on Chrome OS and the update operation is
      // not so cheap. However, because the first GlobalEngineChanged signal
      // from ibus-daemon might not be delivered to libcros due to process start
      // up race, we updates the UI for the first |kMaxNotifyFocusInCount|
      // times. See http://crosbug.com/8284#c14 for details of the race.
      // TODO(yusukes): Get rid of the workaround once the race is solved.
      // http://crosbug.com/8766
      ++notify_focus_in_count_;
      UpdateUI();
    }
  }

#if !IBUS_CHECK_VERSION(1, 3, 99)
  // Handles "FocusOut" signal from the candidate_window process.
  void FocusOut(const char* input_context_path) {
    if (!input_context_path) {
      LOG(ERROR) << "NULL context passed";
    } else {
      DLOG(INFO) << "FocusOut: " << input_context_path;
    }
  }
#endif

  // Handles "StateChanged" signal from the candidate_window process.
  void StateChanged() {
    DLOG(INFO) << "StateChanged";
    UpdateUI();
  }

  // Handles "RegisterProperties" signal from the candidate_window process.
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

  // Handles "UpdateProperty" signal from the candidate_window process.
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
  // Warning: you can call this function only from ibus callback functions
  // like FocusIn(). See http://crosbug.com/5217#c9 for details.
  void UpdateUI() {
    if (!IBusConnectionIsAlive()) {
      // When ibus-daemon is killed right after receiving GlobalEngineChanged
      // notification for SetImeConfig("preload_engine", "xkb:us::eng") request,
      // this path might be taken. This is not an error. We can safely skip the
      // UI update.
      LOG(INFO) << "UpdateUI: IBus connection is not alive";
      return;
    }

    IBusEngineDesc* engine_desc = ibus_bus_get_global_engine(ibus_);
    if (!engine_desc) {
      LOG(ERROR) << "Global engine is not set";
      return;
    }

    const gchar* name = ibus_engine_desc_get_name(engine_desc);
    const gchar* longname = ibus_engine_desc_get_longname(engine_desc);
    const gchar* layout = ibus_engine_desc_get_layout(engine_desc);
    const gchar* language = ibus_engine_desc_get_language(engine_desc);
    InputMethodDescriptor current_input_method(name,
                                               longname,
                                               layout,
                                               language);
    DLOG(INFO) << "Updating the UI. ID:" << current_input_method.id
               << ", display_name:" << current_input_method.display_name
               << ", keyboard_layout:" << current_input_method.keyboard_layout;

    // Notify the change to update UI.
    current_input_method_changed_(language_library_, current_input_method);
    g_object_unref(engine_desc);
  }

  void AddIBusInputMethodNames(InputMethodType type,
                               chromeos::InputMethodDescriptors* out) {
    DCHECK(out);
    for (size_t i = 0; i < arraysize(chromeos::ibus_engines); ++i) {
      if (InputMethodIdIsWhitelisted(chromeos::ibus_engines[i].name) &&
          (type == kSupportedInputMethods ||
           active_engines_.count(chromeos::ibus_engines[i].name) > 0)) {
        out->push_back(chromeos::InputMethodDescriptor(
            chromeos::ibus_engines[i].name,
            chromeos::ibus_engines[i].longname,
            chromeos::ibus_engines[i].layout,
            chromeos::ibus_engines[i].language));
        if (type != kSupportedInputMethods) {
          DLOG(INFO) << chromeos::ibus_engines[i].name << " (preload later)";
        }
      }
    }
  }

#if !IBUS_CHECK_VERSION(1, 3, 99)
  static void DBusProxyDestroyCallback(DBusGProxy* proxy, gpointer user_data) {
    LOG(ERROR) << "DBus proxy for candidate_window is destroyed!";
    // candidate_window might be terminated. libdbus automatically recovers
    // |dbus_proxy_| when the process restarts. Call MaybeRestoreConnections()
    // just in case.
    InputMethodStatusConnection* self
        = static_cast<InputMethodStatusConnection*>(user_data);
    if (self) {
      self->MaybeRestoreConnections();
      self->notify_focus_in_count_ = 0;
    }
  }
#endif

  static void IBusBusConnectedCallback(IBusBus* bus, gpointer user_data) {
    LOG(WARNING) << "IBus connection is recovered.";
    // ibus-daemon might be restarted, or the daemon was not running when Chrome
    // started. Anyway, since |ibus_| connection is now ready, it's possible to
    // create |ibus_config_| object by calling MaybeRestoreConnections().
    InputMethodStatusConnection* self
        = static_cast<InputMethodStatusConnection*>(user_data);
    if (self) {
      self->MaybeRestoreConnections();
#if IBUS_CHECK_VERSION(1, 3, 99)
      self->AddMatchRules();
#endif
      if (self->connection_change_handler_) {
        self->connection_change_handler_(self->language_library_, true);
      }
      self->notify_focus_in_count_ = 0;
    }
    // TODO(yusukes): Probably we should send all input method preferences in
    // Chrome to ibus-memconf as soon as |ibus_config_| gets ready again.
  }

  static void IBusBusDisconnectedCallback(IBusBus* bus, gpointer user_data) {
    LOG(ERROR) << "IBus connection to ibus-daemon is terminated!";
    // ibus-daemon might be terminated. Since |ibus_| object will automatically
    // connect to the daemon if it restarts, we don't have to set NULL on ibus_.
    // Call MaybeRestoreConnections() to set NULL to ibus_config_ temporarily.
    // The function might delete and recreate |dbus_connection_| object as well.
    InputMethodStatusConnection* self
        = static_cast<InputMethodStatusConnection*>(user_data);
    if (self) {
      self->MaybeRestoreConnections();
      if (self->connection_change_handler_) {
        self->connection_change_handler_(self->language_library_, false);
      }
      self->notify_focus_in_count_ = 0;
    }
  }

  static void IBusBusGlobalEngineChangedCallback(
      IBusBus* bus, gpointer user_data) {
    DLOG(INFO) << "Global engine is changed";
    InputMethodStatusConnection* self
        = static_cast<InputMethodStatusConnection*>(user_data);
    if (self) {
      self->UpdateUI();
    }
  }

#if IBUS_CHECK_VERSION(1, 3, 99)
  // Ask ibus-daemon to forward messages for the panel process to licros. See
  // http://dbus.freedesktop.org/doc/dbus-specification.html#message-bus-routing
  // for details.
  void AddMatchRules() {
    g_return_if_fail(ibus_ != NULL);

    const gchar* kMethodNames[] = {
      "FocusIn", "StateChanged", "RegisterProperties", "UpdateProperty",
    };
    for (size_t i = 0; i < arraysize(kMethodNames); ++i) {
      gchar method_rule[512] = {0};
      snprintf(method_rule, sizeof(method_rule) - 1,
               "type='method_call',path='%s',interface='%s',member='%s'",
               IBUS_PATH_PANEL, IBUS_INTERFACE_PANEL, kMethodNames[i]);
      ibus_bus_add_match(ibus_, method_rule);
    }
    GDBusConnection* connection = ibus_bus_get_connection(ibus_);
    g_dbus_connection_add_filter(connection, PanelMessageFilter, this, NULL);
  }

  // A data passed from PanelMessageFilter to PanelMessageFilterIdle.
  struct PanelMessageFilterData {
    explicit PanelMessageFilterData(InputMethodStatusConnection* in_self)
        : self(in_self), type(kDataUnknown),
          context_path(NULL), prop_list(NULL), property(NULL) {}

    ~PanelMessageFilterData() {
      if (context_path) {
        g_free(context_path);
      }
      if (prop_list) {
        g_object_unref(prop_list);
      }
      if (property) {
        g_object_unref(property);
      }
    }

    enum DataType {
      kDataForFocusIn,
      kDataForStateChanged,
      kDataForRegisterProperties,
      kDataForUpdateProperty,
      kDataUnknown
    };

    InputMethodStatusConnection* self;
    DataType type;

    gchar* context_path;
    IBusPropList* prop_list;
    IBusProperty* property;
  };

  // A idle function called in the main thread context.
  static gboolean PanelMessageFilterIdle(gpointer user_data) {
    PanelMessageFilterData* data
        = static_cast<PanelMessageFilterData*>(user_data);
    DCHECK(data);
    DCHECK(data->self);

    switch(data->type) {
      case PanelMessageFilterData::kDataForFocusIn:
        DCHECK(data->context_path);
        data->self->FocusIn(data->context_path);
        break;
      case PanelMessageFilterData::kDataForStateChanged:
        data->self->StateChanged();
        break;
      case PanelMessageFilterData::kDataForRegisterProperties:
        DCHECK(data->prop_list);
        data->self->RegisterProperties(data->prop_list);
        break;
      case PanelMessageFilterData::kDataForUpdateProperty:
        DCHECK(data->property);
        data->self->UpdateProperty(data->property);
        break;
      default:
        LOG(ERROR) << "Unknown data type:" << data->type;
        break;
    }
    delete data;
    return FALSE;  // stop the idle timer.
  }

  // A filter function that is called for all incoming and outgoing messages.
  // This function is called by glib (GDBus library) in a dedicated thread for
  // GDBus. YOU SHOULD BE CAREFUL not to call thread unsafe IBus and libcros
  // functions!!
  static GDBusMessage* PanelMessageFilter(GDBusConnection* dbus_connection,
                                          GDBusMessage* message,
                                          gboolean incoming,
                                          gpointer user_data) {
    if (!incoming) {
      // We don't inspect outgoing messages.
      return message;
    }

    GDBusMessageType type =  g_dbus_message_get_message_type(message);
    if (type != G_DBUS_MESSAGE_TYPE_SIGNAL &&
        type != G_DBUS_MESSAGE_TYPE_METHOD_CALL) {
      // We only inspect signals and method calls. don't for method returns,
      // etc.
      return message;
    }

    const gchar* interface = g_dbus_message_get_interface(message);
    if (g_strcmp0(interface, IBUS_INTERFACE_PANEL) &&
        g_strcmp0(interface, "org.freedesktop.DBus")) {
      // We only inspect panel and dbus messages.
      return message;
    }

    const gchar* member = g_dbus_message_get_member(message);
    GVariant* parameters = g_dbus_message_get_body(message);

    InputMethodStatusConnection* self
        = static_cast<InputMethodStatusConnection*>(user_data);
    DCHECK(self);
    PanelMessageFilterData* data = new PanelMessageFilterData(self);

    if (!g_strcmp0(member, "FocusIn")) {
      gchar* context_path = NULL;
      g_variant_get(parameters, "(&o)", &context_path);
      data->type = PanelMessageFilterData::kDataForFocusIn;
      data->context_path = g_strdup(context_path);
    } else if (!g_strcmp0(member, "StateChanged")) {
      data->type = PanelMessageFilterData::kDataForStateChanged;
    } else if (!g_strcmp0(member, "RegisterProperties")) {
      GVariant* variant = g_variant_get_child_value(parameters, 0);
      IBusPropList* prop_list
          = IBUS_PROP_LIST(ibus_serializable_deserialize(variant));
      if (prop_list) {
        g_object_ref_sink(prop_list);
      }
      g_variant_unref(variant);
      data->type = PanelMessageFilterData::kDataForRegisterProperties;
      data->prop_list = prop_list;
    } else if (!g_strcmp0(member, "UpdateProperty")) {
      GVariant* variant = g_variant_get_child_value(parameters, 0);
      IBusProperty* property
          = IBUS_PROPERTY(ibus_serializable_deserialize(variant));
      if (property) {
        g_object_ref_sink(property);
      }
      g_variant_unref(variant);
      data->type = PanelMessageFilterData::kDataForUpdateProperty;
      data->property = property;
    }
    g_idle_add_full(G_PRIORITY_DEFAULT, PanelMessageFilterIdle, data, NULL);

    return message;
  }
#else
  // Dispatches signals from candidate_window. In this function, we use the
  // IBus's DBus binding (rather than the dbus-glib and its C++ wrapper).
  // This is because arguments of "RegisterProperties" and "UpdateProperty"
  // are fairly complex IBus types, and thus it's probably not a good idea
  // to write a deserializer for these types from scratch using dbus-glib.
  static DBusHandlerResult DispatchSignalFromCandidateWindow(
      DBusConnection* connection, DBusMessage* message, void* object) {
    DCHECK(message);
    DCHECK(object);

    InputMethodStatusConnection* self
        = static_cast<InputMethodStatusConnection*>(object);
    // Checks the connection to ibus-daemon is still alive. Note that the
    // |connection| object is for candidate_window, not for ibus-daemon!
    if (!self->IBusConnectionIsAlive()) {
      LOG(ERROR) << "IBus connection is lost! DBus signal from the candidate "
                 << "window is ignored.";
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    IBusError* error = NULL;
    if (ibus_message_is_signal(message,
                               kCandidateWindowInterface,
                               "FocusIn")) {
      gchar* input_context_path = NULL;
      const gboolean retval = ibus_message_get_args(message, &error,
                                                    G_TYPE_STRING,
                                                    &input_context_path,
                                                    G_TYPE_INVALID);
      if (!retval) {
        LOG(ERROR) << "FocusIn";
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
      }
      self->FocusIn(input_context_path);
      return DBUS_HANDLER_RESULT_HANDLED;
    }

    if (ibus_message_is_signal(message,
                               kCandidateWindowInterface,
                               "FocusOut")) {
      gchar* input_context_path = NULL;
      const gboolean retval = ibus_message_get_args(message, &error,
                                                    G_TYPE_STRING,
                                                    &input_context_path,
                                                    G_TYPE_INVALID);
      if (!retval) {
        LOG(ERROR) << "FocusOut";
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
      }
      self->FocusOut(input_context_path);
      return DBUS_HANDLER_RESULT_HANDLED;
    }

    if (ibus_message_is_signal(message,
                               kCandidateWindowInterface,
                               "StateChanged")) {
      const gboolean retval = ibus_message_get_args(message, &error,
                                                    /* no arguments */
                                                    G_TYPE_INVALID);
      if (!retval) {
        LOG(ERROR) << "StateChanged";
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
      }
      self->StateChanged();
      return DBUS_HANDLER_RESULT_HANDLED;
    }

    if (ibus_message_is_signal(message,
                               kCandidateWindowInterface,
                               "RegisterProperties")) {
      IBusPropList* prop_list = NULL;

      // The ibus_message_get_args() function automagically deserializes the
      // complex IBus structure.
      const gboolean retval = ibus_message_get_args(message, &error,
                                                    IBUS_TYPE_PROP_LIST,
                                                    &prop_list,
                                                    G_TYPE_INVALID);

      if (!retval) {
        LOG(ERROR) << "RegisterProperties";
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
      }
      self->RegisterProperties(prop_list);
      g_object_unref(prop_list);
      return DBUS_HANDLER_RESULT_HANDLED;
    }

    if (ibus_message_is_signal(message,
                               kCandidateWindowInterface,
                               "UpdateProperty")) {
      IBusProperty* prop = NULL;
      const gboolean retval = ibus_message_get_args(message, &error,
                                                    IBUS_TYPE_PROPERTY,
                                                    &prop,
                                                    G_TYPE_INVALID);
      if (!retval) {
        LOG(ERROR) << "UpdateProperty";
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
      }
      self->UpdateProperty(prop);
      g_object_unref(prop);
      return DBUS_HANDLER_RESULT_HANDLED;
    }

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }
#endif

  // A function pointers which point LanguageLibrary::XXXHandler functions.
  // The functions are used when libcros receives signals from the
  // candidate_window.
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

#if !IBUS_CHECK_VERSION(1, 3, 99)
  // Connection to the ibus-daemon via DBus API. These objects are used to
  // receive signals (e.g. focus in) from the candidate_window.
  scoped_ptr<dbus::BusConnection> dbus_connection_;
  dbus::Proxy dbus_proxy_;
#endif

  // Current input context path.
  std::string input_context_path_;

  // Update the UI on FocusIn signal for the first |kMaxNotifyFocusInCount|
  // times. 1 should be enough for the counter, but we use 5 for safety.
  int notify_focus_in_count_;
  static const int kMaxNotifyFocusInCount = 5;

  std::set<std::string> active_engines_;
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

// TODO(yusukes): remove the function.
extern "C"
void ChromeOSDisconnectInputMethodStatus(
    InputMethodStatusConnection* connection) {
  LOG(INFO) << "DisconnectInputMethodStatus (NOP)";
}

extern "C"
InputMethodDescriptors* ChromeOSGetActiveInputMethods(
    InputMethodStatusConnection* connection) {
  g_return_val_if_fail(connection, NULL);
  connection->MaybeRestoreConnections();
  // Pass ownership to a caller. Note: GetInputMethods() might return NULL.
  return connection->GetInputMethods(
      InputMethodStatusConnection::kActiveInputMethods);
}

extern "C"
bool ChromeOSSetActiveInputMethods(InputMethodStatusConnection* connection,
                                   const ImeConfigValue& value) {
  g_return_val_if_fail(connection, FALSE);
  return connection->SetActiveInputMethods(value);
}

extern "C"
InputMethodDescriptors* ChromeOSGetSupportedInputMethods(
    InputMethodStatusConnection* connection) {
  g_return_val_if_fail(connection, NULL);
  // We don't need to try to restore the connection here because GetInputMethods
  // does not communicate with ibus.
  // Pass ownership to a caller. Note: GetInputMethods() might return NULL.
  return connection->GetInputMethods(
      InputMethodStatusConnection::kSupportedInputMethods);
}

extern "C"
void ChromeOSSetImePropertyActivated(
    InputMethodStatusConnection* connection, const char* key, bool activated) {
  DLOG(INFO) << "SetImePropertyeActivated: " << key << ": " << activated;
  DCHECK(key);
  g_return_if_fail(connection);
  connection->MaybeRestoreConnections();
  connection->SetImePropertyActivated(key, activated);
}

extern "C"
bool ChromeOSChangeInputMethod(
    InputMethodStatusConnection* connection, const char* name) {
  DCHECK(name);
  DLOG(INFO) << "ChangeInputMethod: " << name;
  g_return_val_if_fail(connection, false);
  connection->MaybeRestoreConnections();
  return connection->ChangeInputMethod(name);
}

extern "C"
bool ChromeOSGetImeConfig(InputMethodStatusConnection* connection,
                          const char* section,
                          const char* config_name,
                          ImeConfigValue* out_value) {
  DCHECK(out_value);
  g_return_val_if_fail(connection, FALSE);
  connection->MaybeRestoreConnections();
  return connection->GetImeConfig(section, config_name, out_value);
}

extern "C"
bool ChromeOSSetImeConfig(InputMethodStatusConnection* connection,
                          const char* section,
                          const char* config_name,
                          const ImeConfigValue& value) {
  g_return_val_if_fail(connection, FALSE);
  connection->MaybeRestoreConnections();
  return connection->SetImeConfig(section, config_name, value);
}

// TODO(yusukes): We can remove the function.
extern "C"
bool ChromeOSInputMethodStatusConnectionIsAlive(
    InputMethodStatusConnection* connection) {
  g_return_val_if_fail(connection, false);
  const bool is_connected = connection->IBusConnectionIsAlive();
  if (!is_connected) {
    LOG(WARNING) << "ChromeOSInputMethodStatusConnectionIsAlive: NOT alive";
  }
  return is_connected;
}

}  // namespace chromeos
