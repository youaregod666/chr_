// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_language.h"

#include <ibus.h>
#include <dbus/dbus-glib-lowlevel.h>  // for dbus_g_connection_get_connection.

#include <algorithm>  // for std::sort.
#include <sstream>
#include <stack>
#include <utility>

#include "chromeos/dbus/dbus.h"
#include "chromeos/glib/object.h"

namespace {

const char kCandidateWindowService[] = "org.freedesktop.IBus.Panel";
const char kCandidateWindowObjectPath[] = "/org/chromium/Chrome/LanguageBar";
const char kCandidateWindowInterface[] = "org.freedesktop.IBus.Panel";

// The list of IME property keys that we don't handle.
const char* kImePropertyKeysBlacklist[] = {
  "setup",  // menu for showing setup dialog used in anthy and hangul.
  "chewing_settings_prop",  // menu for showing setup dialog used in chewing.
  "status",  // used in m17n.
};

// Copies IME names in |engines| to |out|.
void AddIMELanguages(const GList* engines, chromeos::InputLanguageList* out) {
  DCHECK(out);
  for (; engines; engines = g_list_next(engines)) {
    IBusEngineDesc* engine_desc = IBUS_ENGINE_DESC(engines->data);
    out->push_back(chromeos::InputLanguage(
        chromeos::LANGUAGE_CATEGORY_IME,
        engine_desc->name, engine_desc->longname, engine_desc->icon));
    g_object_unref(engine_desc);
  }
}

// Copies XKB layout names in (TBD) to |out|.
void AddXKBLayouts(chromeos::InputLanguageList* out) {
  DCHECK(out);
  // TODO(yusukes): implement this.
  out->push_back(chromeos::InputLanguage(
      chromeos::LANGUAGE_CATEGORY_XKB,
      kFallbackXKBId,
      kFallbackXKBDisplayName,
      "" /* no icon */));  // mock
}

// Returns IBusInputContext for |input_context_path|. NULL on errors.
IBusInputContext* GetInputContext(
    const std::string& input_context_path, IBusBus* ibus) {
  IBusInputContext* context = ibus_input_context_get_input_context(
      input_context_path.c_str(), ibus_bus_get_connection(ibus));
  if (!context) {
    LOG(ERROR) << "IBusInputContext is null: " << input_context_path;
  }
  return context;
}

// Returns true if |key| is blacklisted.
bool KeyIsBlacklisted(const char* key) {
  for (size_t i = 0; i < arraysize(kImePropertyKeysBlacklist); ++i) {
    if (!std::strcmp(key, kImePropertyKeysBlacklist[i])) {
      return true;
    }
  }
  return false;
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
    LOG(ERROR) << "The property does not have sub properties, "
               << "but the type of the property is PROP_TYPE_MENU";
    return false;
  }
  if (ibus_prop->type == PROP_TYPE_SEPARATOR ||
      ibus_prop->type == PROP_TYPE_MENU) {
    // This is not an error, but we don't push an item for these types.
    return true;
  }

  const bool is_selection_item = (ibus_prop->type == PROP_TYPE_RADIO);
  selection_item_id
      = is_selection_item ? selection_item_id : kInvalidSelectionItemId;

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

  // TODO(yusukes): Probably it's better to generate our own label from the key?
  std::string label = (ibus_prop->tooltip ? ibus_prop->tooltip->text : "");
  if (label.empty()) {
    // Usually tooltips are more descriptive than labels.
    label = (ibus_prop->label ? ibus_prop->label->text : "");
  }
  if (label.empty()) {
    // ibus-pinyin has a property whose label and tooltip are empty. Fall back
    // to the key.
    label = ibus_prop->key;
  }

  out_prop_list->push_back(chromeos::ImeProperty(ibus_prop->key,
                                                 ibus_prop->icon,
                                                 label,
                                                 is_selection_item,
                                                 is_selection_item_checked,
                                                 selection_item_id));
  return true;
}

// Converts |ibus_prop| to |out_prop_list|. Please note that |ibus_prop|
// may or may not have children. See the comment for FlattenPropertyList
// for details. Returns true if no error is found.
bool FlattenProperty(
    IBusProperty* ibus_prop, chromeos::ImePropertyList* out_prop_list) {
  // TODO(yusukes): Find a good way to write unit tests for this function.
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
    if (KeyIsBlacklisted(prop->key)) {
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
bool FlattenPropertyList(
    IBusPropList* ibus_prop_list, chromeos::ImePropertyList* out_prop_list) {
  // TODO(yusukes): Find a good way to write unit tests for this function.
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

  const bool result
      = fake_root_prop && FlattenProperty(fake_root_prop, out_prop_list);
  g_object_unref(fake_root_prop);

  return result;
}

// Debug print functions.
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

std::string Spacer(int n) {
  return std::string(n, ' ');
}

std::string PrintPropList(IBusPropList *prop_list, int tree_level);
std::string PrintProp(IBusProperty *prop, int tree_level) {
  if (!prop) {
    return "";
  }

  std::stringstream stream;
  stream << Spacer(tree_level) << "=========================" << std::endl;
  stream << Spacer(tree_level) << "key: " << (prop->key ? prop->key : "<none>")
         << std::endl;
  stream << Spacer(tree_level) << "icon: "
         << (prop->icon ? prop->icon : "<none>") << std::endl;
  stream << Spacer(tree_level) << "label: "
         << (prop->label ? prop->label->text : "<none>") << std::endl;
  stream << Spacer(tree_level) << "tooptip: "
         << (prop->tooltip ? prop->tooltip->text : "<none>") << std::endl;
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

// A class that holds IBus and DBus connections.
class LanguageStatusConnection {
 public:
  LanguageStatusConnection(LanguageStatusMonitorFunctions monitor_functions,
                           void* language_library)
      : monitor_functions_(monitor_functions),
        language_library_(language_library),
        ibus_(NULL),
        ibus_config_(NULL),
        input_context_path_("") {
    DCHECK(monitor_functions_.current_language);
    DCHECK(monitor_functions_.register_ime_properties);
    DCHECK(monitor_functions_.update_ime_property);
    DCHECK(language_library_);
  }

  ~LanguageStatusConnection() {
    // Close IBus and DBus connections.
    if (ibus_config_) {
      g_object_unref(ibus_config_);
    }
    if (ibus_) {
      g_object_unref(ibus_);
    }
  }

  // Initializes IBus and DBus connections.
  bool Init() {
    // Establish IBus connection between ibus-daemon to retrieve the list of
    // available IME engines, change the current IME engine, and so on.
    ibus_init();
    ibus_ = ibus_bus_new();

    // Check the IBus connection status.
    if (!ibus_) {
      LOG(ERROR) << "ibus_bus_new() failed";
      return false;
    }
    if (!ibus_bus_is_connected(ibus_)) {
      LOG(ERROR) << "ibus_bus_is_connected() failed";
      return false;
    }

    // Get the IBus connection, which is owned by ibus_.
    IBusConnection* ibus_connection = ibus_bus_get_connection(ibus_);
    if (!ibus_connection) {
      LOG(ERROR) << "ibus_bus_get_connection() failed";
      return false;
    }

    // Create the IBus config.
    ibus_config_ = ibus_config_new(ibus_connection);
    if (!ibus_config_) {
      LOG(ERROR) << "ibus_bus_config_new() failed";
      return false;
    }

    // Establish a DBus connection between the candidate_window process for
    // Chromium OS to handle signals (e.g. "FocusIn") from the process.
    const char* address = ibus_get_address();
    dbus_connection_.reset(
        new dbus::BusConnection(dbus::GetPrivateBusConnection(address)));
    LOG(INFO) << "Established private D-Bus connection to: '" << address << "'";

    // Connect to the candidate_window. Note that dbus_connection_add_filter()
    // does not work without calling dbus::Proxy().
    // TODO(yusukes): Investigate how we can eliminate the call.

    const bool kConnectToNameOwner = true;
    // TODO(yusukes): dbus::Proxy instantiation might fail (and abort due to
    // DCHECK failure) when candidate_window process does not exist yet.
    // Would be better to add "bool dbus::Proxy::Init()" or something like that
    // to handle such case?
    dbus_proxy_.reset(new dbus::Proxy(*dbus_connection_,
                                      kCandidateWindowService,
                                      kCandidateWindowObjectPath,
                                      kCandidateWindowInterface,
                                      kConnectToNameOwner));

    // Register DBus signal handler.
    dbus_connection_add_filter(
        dbus_g_connection_get_connection(dbus_connection_->g_connection()),
        &LanguageStatusConnection::DispatchSignalFromCandidateWindow,
        this, NULL);

    // TODO(yusukes): Investigate what happens if IBus/DBus connections are
    // suddenly closed.
    // TODO(yusukes): Investigate what happens if candidate_window process is
    // restarted. I'm not sure but we should use dbus_g_proxy_new_for_name(),
    // not dbus_g_proxy_new_for_name_owner()?

    return true;
  }

  // GetLanguagesMode is used for GetLanguages().
  enum GetLanguagesMode {
    kActiveLanguages,  // Get active languages.
    kSupportedLanguages,  // Get supported languages.
  };

  // Called by cros API ChromeOSGetLanguages() and returns a list of IMEs and
  // XKB layouts that are currently active.
  InputLanguageList* GetLanguages(GetLanguagesMode mode) {
    GList* engines = NULL;
    if (mode == kActiveLanguages) {
      engines = ibus_bus_list_active_engines(ibus_);
    } else if (mode == kSupportedLanguages) {
      engines = ibus_bus_list_engines(ibus_);
    } else {
      NOTREACHED();
      return NULL;
    }
    if (!engines) {
      // IBus connection is broken?
      LOG(ERROR) << "ibus_bus_(active_)list_engines() failed.";
      return NULL;
    }
    InputLanguageList* language_list = new InputLanguageList;
    AddIMELanguages(engines, language_list);
    AddXKBLayouts(language_list);
    std::sort(language_list->begin(), language_list->end());

    g_list_free(engines);
    return language_list;
  }

  // Called by cros API ChromeOS(Activate|Deactive)ImeProperty().
  void ActivateOrDeactiveImeProperty(const char* key, bool active) {
    if (input_context_path_.empty()) {
      LOG(ERROR) << "Input context is unknown";
      return;
    }

    IBusInputContext* context = GetInputContext(input_context_path_, ibus_);
    if (!context) {
      return;
    }
    ibus_input_context_property_activate(
        context, key, (active ? PROP_STATE_CHECKED : PROP_STATE_UNCHECKED));
    g_object_unref(context);

    UpdateUI();
  }

  // Called by cros API ChromeOSChangeLanguage().
  void ChangeLanguage(LanguageCategory category, const char* name) {
    // Clear all IME properties unconditionally.
    //  - When switching to XKB, it's necessary since XKB layout does not have
    //    IME properties.
    //  - When switching to IME and a text area is focused, it's okay to clear
    //    IME properties here since RegisterProperties signal for the new IME
    //    will be sent anyway.
    //  - When switching to IME and no text area is focused, RegisterProperties
    //    signal for the new IME will NOT be sent until a text area is focused.
    //    Therefore, we have to clear the old IME properties here to keep the
    //    IME switcher status consistent.
    RegisterProperties(NULL);

    switch (category) {
      case LANGUAGE_CATEGORY_XKB:
        SwitchToXKB(name);
        break;
      case LANGUAGE_CATEGORY_IME:
        SwitchToIME(name);
        break;
    }
  }

  // UpdateMode is used for specifying whether we are activating or
  // deactivating an input language. We use this mode to consolidate logic
  // for activation and deactivation.
  enum UpdateMode {
    kActivate,  // Activate an input language.
    kDeactivate,  // Deactivate an input language.
  };

  // Called by cros API ChromeOSActivateLanguage().
  bool UpdateXKB(UpdateMode mode, const char* xkb_name) {
    // TODO(yusukes,satorux): implement this.
    return false;
  }

  // Called by cros API ChromeOSActivateLanguage().
  bool UpdateIME(UpdateMode mode, const char* ime_name) {
    GList* engines = ibus_bus_list_active_engines(ibus_);
    if (!engines) {
      // IBus connection is broken?
      LOG(ERROR) << "ibus_bus_list_active_engines() failed.";
      return false;
    }

    // Convert |engines| to a GValueArray of names.
    GValueArray* engine_names = g_value_array_new(0);
    for (GList* cursor = engines; cursor; cursor = g_list_next(cursor)) {
      IBusEngineDesc* engine_desc = IBUS_ENGINE_DESC(cursor->data);
      // Skip the IME if the mode is to deactivate and it matches the given
      // name.
      if (mode == kDeactivate && strcmp(engine_desc->name, ime_name) == 0) {
        continue;
      }
      GValue name_value = { 0 };
      g_value_init(&name_value, G_TYPE_STRING);
      g_value_set_string(&name_value, engine_desc->name);
      g_value_array_append(engine_names, &name_value);
    }

    if (mode == kActivate) {
      // Add a new IME here.
      GValue name_value = { 0 };
      g_value_init(&name_value, G_TYPE_STRING);
      g_value_set_string(&name_value, ime_name);
      // Prepend to add the new IME as the first choice.
      g_value_array_prepend(engine_names, &name_value);
    }

    // Make the array into a GValue.
    GValue value = { 0 };
    g_value_init(&value, G_TYPE_VALUE_ARRAY);
    g_value_take_boxed(&value, engine_names);

    // Set the config value.
    const gboolean success = ibus_config_set_value(ibus_config_,
                                                   "general",
                                                   "preload_engines",
                                                   &value);
    g_value_unset(&value);
    g_list_free(engines);

    return success;
  }

  InputLanguage* GetCurrentLanguage() {
    IBusInputContext* context = GetInputContext(input_context_path_, ibus_);
    if (!context) {
      return NULL;
    }

    const bool ime_is_enabled = ibus_input_context_is_enabled(context);
    g_object_unref(context);

    InputLanguage* current_language = NULL;
    if (ime_is_enabled) {
      DLOG(INFO) << "IME is active";
      // Set IME name on current_language.
      const IBusEngineDesc* engine_desc
          = ibus_input_context_get_engine(context);
      DCHECK(engine_desc);
      if (!engine_desc) {
        return NULL;
      }
      current_language = new InputLanguage(LANGUAGE_CATEGORY_IME,
                                           engine_desc->name,
                                           engine_desc->longname,
                                           engine_desc->icon);
    } else {
      DLOG(INFO) << "IME is not active";
      // Set XKB layout name on current_languages.
      current_language = new InputLanguage(LANGUAGE_CATEGORY_XKB,
                                           kFallbackXKBId,
                                           kFallbackXKBDisplayName,
                                           "" /* no icon */);  // mock
      // TODO(yusukes): implemente this.
    }
    return current_language;
  }

 private:
  // Changes the current language to |name|, which is XKB layout.
  void SwitchToXKB(const char* name) {
    // TODO(yusukes): implement XKB switching.
    if (input_context_path_.empty()) {
      LOG(ERROR) << "Input context is unknown";
      return;
    }

    IBusInputContext* context = GetInputContext(input_context_path_, ibus_);
    if (!context) {
      return;
    }
    ibus_input_context_disable(context);
    g_object_unref(context);
    UpdateUI();
  }

  // Changes the current language to |name|, which is IME.
  void SwitchToIME(const char* name) {
    if (input_context_path_.empty()) {
      LOG(ERROR) << "Input context is unknown";
      return;
    }

    IBusInputContext* context = GetInputContext(input_context_path_, ibus_);
    if (!context) {
      return;
    }
    ibus_input_context_set_engine(context, name);
    g_object_unref(context);
    UpdateUI();
  }

  // Handles "FocusIn" signal from the candidate_window process.
  void FocusIn(const char* input_context_path) {
    DCHECK(input_context_path) << "NULL context passed";
    DLOG(INFO) << "FocusIn: " << input_context_path;

    // Remember the current ic path.
    input_context_path_ = input_context_path;
    UpdateUI();  // This is necessary since IME status is held per ic.
  }

  // Handles "FocusOut" signal from the candidate_window process.
  void FocusOut(const char* input_context_path) {
    DCHECK(input_context_path) << "NULL context passed";
    DLOG(INFO) << "FocusOut: " << input_context_path;
  }

  // Handles "StateChanged" signal from the candidate_window process.
  void StateChanged() {
    // TODO(yusukes): Modify common/chromeos/dbus/dbus.h so that we can handle
    // signals without argument. Then remove the |dummy|.
    DLOG(INFO) << "StateChanged";
    UpdateUI();
  }

  // Handles "RegisterProperties" signal from the candidate_window process.
  void RegisterProperties(IBusPropList* ibus_prop_list) {
    DLOG(INFO) << "RegisterProperties" << (ibus_prop_list ? "" : " (clear)");

    ImePropertyList prop_list;  // our representation.
    if (ibus_prop_list) {
      // You can call
      //  LOG(INFO) << "\n" << PrintPropList(ibus_prop_list, 0);
      // here to dump |ibus_prop_list|.
      if (!FlattenPropertyList(ibus_prop_list, &prop_list)) {
        LOG(WARNING) << "Malformed properties are detected";
      }
    }
    // Notify the change.
    monitor_functions_.register_ime_properties(language_library_, prop_list);
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
      LOG(WARNING) << "Malformed properties are detected";
    }
    // Notify the change.
    if (!prop_list.empty()) {
      monitor_functions_.update_ime_property(language_library_, prop_list);
    }
  }

  // Retrieve IME/XKB status and notify them to the UI.
  void UpdateUI() {
    if (input_context_path_.empty()) {
      LOG(ERROR) << "Input context is unknown";
      return;
    }

    IBusInputContext* context = GetInputContext(input_context_path_, ibus_);
    if (!context) {
      return;
    }

    InputLanguage current_language;
    const bool ime_is_enabled = ibus_input_context_is_enabled(context);
    if (ime_is_enabled) {
      DLOG(INFO) << "IME is active";
      // Set IME name on current_language.
      const IBusEngineDesc* engine_desc
          = ibus_input_context_get_engine(context);
      DCHECK(engine_desc);
      if (!engine_desc) {
        return;
      }
      current_language = InputLanguage(LANGUAGE_CATEGORY_IME,
                                       engine_desc->name,
                                       engine_desc->longname,
                                       engine_desc->icon);
    } else {
      DLOG(INFO) << "IME is not active";
      // Set XKB layout name on current_languages.
      current_language = InputLanguage(LANGUAGE_CATEGORY_XKB,
                                       kFallbackXKBId,
                                       kFallbackXKBDisplayName,
                                       "" /* no icon */);  // mock
      // TODO(yusukes): implemente this.
    }
    DLOG(INFO) << "Updating the UI. ID:" << current_language.id
               << ", display_name:" << current_language.display_name;

    // Notify the change to update UI.
    monitor_functions_.current_language(language_library_, current_language);
    g_object_unref(context);
  }

  // Dispatches signals from candidate_window. In this function, we use the
  // IBus's DBus binding (rather than the dbus-glib and its C++ wrapper).
  // This is because arguments of "RegisterProperties" and "UpdateProperty"
  // are fairly complex IBus types, and thus it's probably not a good idea
  // to write a deserializer for these types from scratch using dbus-glib.
  static DBusHandlerResult DispatchSignalFromCandidateWindow(
      DBusConnection* connection, DBusMessage* message, void* object) {
    DCHECK(message);
    DCHECK(object);

    LanguageStatusConnection* self
        = static_cast<LanguageStatusConnection*>(object);
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

  // A function pointers which point LanguageLibrary::XXXHandler functions.
  // |monitor_funcions_| is used when libcros receives signals from the
  // candidate_window.
  LanguageStatusMonitorFunctions monitor_functions_;

  // Points to a chromeos::LanguageLibrary object. |language_library_| is used
  // as the first argument of the |monitor_functions_| functions.
  void* language_library_;

  // Connections to IBus and DBus.
  IBusBus* ibus_;
  IBusConfig* ibus_config_;
  scoped_ptr<dbus::BusConnection> dbus_connection_;
  scoped_ptr<dbus::Proxy> dbus_proxy_;

  // Current input context path.
  std::string input_context_path_;
};

//
// cros APIs
//

// The function will be bound to chromeos::MonitorLanguageStatus with dlsym()
// in load.cc so it needs to be in the C linkage, so the symbol name does not
// get mangled.
extern "C"
LanguageStatusConnection* ChromeOSMonitorLanguageStatus(
    LanguageStatusMonitorFunctions monitor_functions, void* language_library) {
  LOG(INFO) << "MonitorLanguageStatus";
  LanguageStatusConnection* connection
      = new LanguageStatusConnection(monitor_functions, language_library);
  if (!connection->Init()) {
    LOG(WARNING) << "Failed to Init() LanguageStatusConnection. "
                 << "Returning NULL";
    delete connection;
    connection = NULL;
  }
  return connection;
}

extern "C"
void ChromeOSDisconnectLanguageStatus(LanguageStatusConnection* connection) {
  LOG(INFO) << "DisconnectLanguageStatus";
  delete connection;
}

extern "C"
InputLanguageList* ChromeOSGetLanguages(LanguageStatusConnection* connection) {
  // TODO(yusukes): Add DCHECK(connection); here when candidate_window for
  // Chrome OS gets ready.
  if (!connection) {
    LOG(WARNING) << "LanguageStatusConnection is NULL";
    return NULL;
  }
  // Pass ownership to a caller. Note: GetLanguages() might return NULL.
  return connection->GetLanguages(
      LanguageStatusConnection::kActiveLanguages);
}

extern "C"
InputLanguageList* ChromeOSGetSupportedLanguages(LanguageStatusConnection*
                                                 connection) {
  if (!connection) {
    LOG(WARNING) << "LanguageStatusConnection is NULL";
    return NULL;
  }
  // Pass ownership to a caller. Note: GetLanguages() might return NULL.
  return connection->GetLanguages(
      LanguageStatusConnection::kSupportedLanguages);
}

extern "C"
void ChromeOSActivateImeProperty(
    LanguageStatusConnection* connection, const char* key) {
  DLOG(INFO) << "ActivateImeProperty";
  DCHECK(key);
  // TODO(yusukes): Add DCHECK(connection); here when candidate_window for
  // Chrome OS gets ready.
  if (!connection) {
    LOG(WARNING) << "LanguageStatusConnection is NULL";
    return;
  }
  connection->ActivateOrDeactiveImeProperty(key, true);
}

extern "C"
void ChromeOSDeactivateImeProperty(
    LanguageStatusConnection* connection, const char* key) {
  DLOG(INFO) << "DeactivateImeProperty";
  DCHECK(key);
  // TODO(yusukes): Add DCHECK(connection); here when candidate_window for
  // Chrome OS gets ready.
  if (!connection) {
    LOG(WARNING) << "LanguageStatusConnection is NULL";
    return;
  }
  connection->ActivateOrDeactiveImeProperty(key, false);
}

extern "C"
void ChromeOSChangeLanguage(LanguageStatusConnection* connection,
                            LanguageCategory category,
                            const char* name) {
  DCHECK(name);
  DLOG(INFO) << "ChangeLanguage: " << name;
  // TODO(yusukes): Add DCHECK(connection); here when candidate_window for
  // Chrome OS gets ready.
  if (!connection) {
    LOG(WARNING) << "LanguageStatusConnection is NULL";
    return;
  }
  connection->ChangeLanguage(category, name);
}

// Helper function for ChromeOSActivateLanguage() and
// ChromeOSDeactivateLanguage().
static bool ActivateOrDeactivateLanguage(
    LanguageStatusConnection::UpdateMode mode,
    LanguageStatusConnection* connection,
    LanguageCategory category,
    const char* name) {
  if (!connection) {
    LOG(WARNING) << "LanguageStatusConnection is NULL";
    return false;
  }
  DCHECK(name);
  bool success = false;
  switch (category) {
    case LANGUAGE_CATEGORY_XKB:
      success = connection->UpdateXKB(mode, name);
      break;
    case LANGUAGE_CATEGORY_IME:
      success = connection->UpdateIME(mode, name);
      break;
  }
  return success;
}

extern "C"
bool ChromeOSActivateLanguage(LanguageStatusConnection* connection,
                              LanguageCategory category,
                              const char* name) {
  DLOG(INFO) << "ActivateLanguage: " << name << " [category "
             << category << "]";
  return ActivateOrDeactivateLanguage(
      LanguageStatusConnection::kActivate, connection, category, name);
}

extern "C"
bool ChromeOSDeactivateLanguage(LanguageStatusConnection* connection,
                                LanguageCategory category,
                                const char* name) {
  DLOG(INFO) << "DeactivateLanguage: " << name << " [category "
             << category << "]";
  return ActivateOrDeactivateLanguage(
      LanguageStatusConnection::kDeactivate, connection, category, name);
}

}  // namespace chromeos
