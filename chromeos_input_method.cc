// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_input_method.h"

#include <dbus/dbus-glib-lowlevel.h>  // for dbus_g_connection_get_connection.
#include <ibus.h>

#include <algorithm>  // for std::reverse.
#include <cstring>  // for std::strcmp.
#include <set>
#include <sstream>
#include <stack>
#include <utility>

#include "chromeos/dbus/dbus.h"
#include "chromeos/glib/object.h"

namespace {

const char kCandidateWindowService[] = "org.freedesktop.IBus.Panel";
const char kCandidateWindowObjectPath[] = "/org/chromium/Chrome/LanguageBar";
const char kCandidateWindowInterface[] = "org.freedesktop.IBus.Panel";

// Also defined in chrome/browser/chromeos/language_preferences.h (Chrome tree).
const char kGeneralSectionName[] = "general";
const char kPreloadEnginesConfigName[] = "preload_engines";

// The list of input method IDs that we handle. This filtering is necessary
// since some input methods are definitely unnecessary for us. For example, we
// should disable "ja:anthy", "zh:cangjie", and "zh:pinyin" engines in
// ibus-m17n since we (will) have better equivalents outside of ibus-m17n.
const char* kInputMethodIdsWhitelist[] = {
  "chewing",  // ibus-chewing - Traditional Chinese
  "hangul",   // ibus-hangul - Korean
  "mozc",     // ibus-mozc - Japanese (with English keyboard)
  "mozc-jp",  // ibus-mozc - Japanese (with Japanese keyboard)
  "pinyin",   // Pinyin engine in ibus-pinyin - Simplified Chinese

  // ibus-m17n input methods (language neutral ones).
  "m17n:t:latn-pre",
  "m17n:t:latn-post",

  // ibus-m17n input methods.
  "m17n:ar:kbd",         // Arabic
  "m17n:he:kbd",         // Hebrew
  "m17n:hi:itrans",      // Hindi
  // Note: the m17n-contrib package has some more Hindi definitions.
  "m17n:fa:isiri",       // Persian
  "m17n:th:kesmanee",    // Thai (simulate the Kesmanee keyboard)
  "m17n:th:pattachote",  // Thai (simulate the Pattachote keyboard)
  "m17n:th:tis820",      // Thai (simulate the TIS-820.2538 keyboard)
  "m17n:vi:tcvn",        // Vietnames (TCVN6064 sequence)
  "m17n:vi:telex",       // Vietnames (TELEX key sequence)
  "m17n:vi:viqr",        // Vietnames (VIQR key sequence)
  "m17n:vi:vni",         // Vietnames (VNI key sequence)
  // Note: Since ibus-m17n does not support "get-surrounding-text" feature yet,
  // Vietnames input methods, except 4 input methods above, in m17n-db should
  // not work fine. The 4 input methods in m17n-db (>= 1.6.0) don't require the
  // feature.
  "m17n:zh:cangjie",     // Traditional Chinese (Cangjie)
  "m17n:zh:quick",       // Traditional Chinese (Quick) in m17n-contrib
  // TODO(suzhe): Add CantonHK and Stroke5 if (it's really) necessary.
  
  // ibux-xkb-layouts input methods (keyboard layouts).
  "xkb:be::fra",        // Belgium - French
  "xkb:br::por",        // Brazil - Portuguese
  "xkb:bg::bul",        // Bulgaria - Bulgarian
  "xkb:cz::cze",        // Czech Republic - Czech
  "xkb:de::ger",        // Germany - German
  "xkb:ee::est",        // Estonia - Estonian
  "xkb:es::spa",        // Spain - Spanish
  "xkb:es:cat:cat",     // Spain - Catalan
  "xkb:dk::dan",        // Denmark - Danish
  "xkb:gr::gre",        // Greece - Greek
  "xkb:lt::lit",        // Lithuania - Lithuanian
  "xkb:lv::lav",        // Latvia - Latvian
  "xkb:hr::scr",        // Croatia - Croatian
  "xkb:nl::nld",        // Netherlands - Dutch
  "xkb:gb::eng",        // United Kingdom - English
  "xkb:fi::fin",        // Finland - Finnish
  "xkb:fr::fra",        // France - French
  "xkb:hu::hun",        // Hungary - Hungarian
  "xkb:it::ita",        // Italy - Italian
  "xkb:jp::jpn",        // Japan - Japanese
  "xkb:no::nor",        // Norway - Norwegian
  "xkb:pl::pol",        // Poland - Polish
  "xkb:pt::por",        // Portugal - Portuguese
  "xkb:ro::rum",        // Romania - Romanian
  "xkb:se::swe",        // Sweden - Swedish
  "xkb:sk::slo",        // Slovakia - Slovak
  "xkb:si::slv",        // Slovenia - Slovene (Slovenian)
  "xkb:rs::srp",        // Serbia - Serbian
  "xkb:ch::ger",        // Switzerland - German
  "xkb:ru::rus",        // Russia - Russian
  "xkb:tr::tur",        // Turkey - Turkish
  "xkb:ua::ukr",        // Ukraine - Ukrainian
  "xkb:us::eng",        // US - English
  "xkb:us:dvorak:eng",  // US - Dvorak - English
  // TODO(satorux,jshin): Add more keyboard layouts.
};

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
    if (InputMethodIdIsWhitelisted(engine_desc->name)) {
      out->push_back(chromeos::InputMethodDescriptor(engine_desc->name,
                                                     engine_desc->longname,
                                                     engine_desc->layout,
                                                     engine_desc->language));
      DLOG(INFO) << engine_desc->name << " (SUPPORTED)";
    }
  }
}

// Returns IBusInputContext for |input_context_path|. NULL on errors.
IBusInputContext* GetInputContext(
    const std::string& input_context_path, IBusBus* ibus) {
  IBusConnection* connection = ibus_bus_get_connection(ibus);
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

// A class that holds IBus and DBus connections.
class InputMethodStatusConnection {
 public:
  InputMethodStatusConnection(
      void* language_library,
      LanguageCurrentInputMethodMonitorFunction current_input_method_changed,
      LanguageRegisterImePropertiesFunction register_ime_properties,
      LanguageUpdateImePropertyFunction update_ime_property,
      LanguageFocusChangeMonitorFunction focus_changed)
      : current_input_method_changed_(current_input_method_changed),
        register_ime_properties_(register_ime_properties),
        update_ime_property_(update_ime_property),
        focus_changed_(focus_changed),
        language_library_(language_library),
        ibus_(NULL),
        dbus_proxy_is_alive_(false),
        input_context_path_("") {
    DCHECK(current_input_method_changed),
    DCHECK(register_ime_properties_);
    DCHECK(update_ime_property_);
    DCHECK(focus_changed_);
    DCHECK(language_library_);
  }

  ~InputMethodStatusConnection() {
    // We can call gproxy() only when the operator bool returns true (or DCHECK
    // fails.)
    if (dbus_proxy_.get() && (dbus_proxy_->operator bool())) {
      g_signal_handlers_disconnect_by_func(
          dbus_proxy_->gproxy(),
          reinterpret_cast<gpointer>(G_CALLBACK(DBusProxyDestroyCallback)),
          this);
    }

    if (dbus_connection_.get()) {
      // Close |dbus_connection_| since the connection is "private connection"
      // and we know |this| is the only instance which uses the
      // |dbus_connection_|. Otherwise, we may see an error message from dbus
      // library like "The last reference on a connection was dropped without
      // closing the connection."
      DBusConnection* raw_connection = dbus_g_connection_get_connection(
          dbus_connection_->g_connection());
      if (raw_connection) {
        dbus_connection_close(raw_connection);
      }
    }

    if (ibus_) {
      // Destruct IBus object.
      g_signal_handlers_disconnect_by_func(
          ibus_,
          reinterpret_cast<gpointer>(
              G_CALLBACK(IBusBusDisconnectedCallback)),
          this);
      g_signal_handlers_disconnect_by_func(
          ibus_,
          reinterpret_cast<gpointer>(
              G_CALLBACK(IBusBusGlobalEngineChangedCallback)),
          this);
      // Since the connection which ibus_ has is a "shared connection". We
      // should not close the connection. Just call g_object_unref.
      g_object_unref(ibus_);
    }
    // |dbus_connection_| and |dbus_proxy_| will be destructed by ~scoped_ptr().
  }

  // Initializes IBus and DBus connections.
  bool Init() {
    // Establish IBus connection between ibus-daemon to retrieve the list of
    // available input method engines, change the current input method engine,
    // and so on.
    ibus_init();
    ibus_ = ibus_bus_new();

    // Check the IBus connection status.
    if (!ibus_) {
      LOG(ERROR) << "ibus_bus_new() failed";
      return false;
    }
    if (!ibus_bus_is_connected(ibus_)) {
      DLOG(INFO) << "ibus_bus_is_connected() failed";
      return false;
    }

    // Establish a DBus connection between the candidate_window process for
    // Chromium OS to handle signals (e.g. "FocusIn") from the process.
    const char* address = ibus_get_address();
    if (!address) {
      LOG(ERROR) << "ibus_get_address returned NULL";
      return false;
    }
    dbus_connection_.reset(
        new dbus::BusConnection(dbus::GetPrivateBusConnection(address)));
    LOG(INFO) << "Established private D-Bus connection to: '" << address << "'";

    // Connect to the candidate_window. Note that dbus_connection_add_filter()
    // does not work without calling dbus::Proxy().
    // TODO(yusukes): Investigate how we can eliminate the call.
    const bool kConnectToNameOwner = true;
    dbus_proxy_.reset(new dbus::Proxy(*dbus_connection_,
                                      kCandidateWindowService,
                                      kCandidateWindowObjectPath,
                                      kCandidateWindowInterface,
                                      kConnectToNameOwner));
    // The operator bool checks if a D-Bus connection is established or not..
    if (!(dbus_proxy_->operator bool())) {
      LOG(ERROR) << "Failed to connect to the candidate_window.";
      return false;
    }

    // Register the callback function for "destroy".
    dbus_proxy_is_alive_ = true;
    g_signal_connect(dbus_proxy_->gproxy(),
                     "destroy",
                     G_CALLBACK(DBusProxyDestroyCallback),
                     this);

    // Register the callback function for IBusBus signals.
    g_signal_connect(ibus_,
                     "disconnected",
                     G_CALLBACK(IBusBusDisconnectedCallback),
                     this);
    g_signal_connect(ibus_,
                     "global-engine-changed",
                     G_CALLBACK(IBusBusGlobalEngineChangedCallback),
                     this);
    
    // Register DBus signal handler.
    dbus_connection_add_filter(
        dbus_g_connection_get_connection(dbus_connection_->g_connection()),
        &InputMethodStatusConnection::DispatchSignalFromCandidateWindow,
        this, NULL);

    // TODO(yusukes): Investigate what happens if candidate_window process is
    // restarted. I'm not sure but we should use dbus_g_proxy_new_for_name(),
    // not dbus_g_proxy_new_for_name_owner()?
    return true;
  }

  // GetInputMethodMode is used for GetInputMethods().
  enum GetInputMethodMode {
    kActiveInputMethods,  // Get active input methods.
    kSupportedInputMethods,  // Get supported input methods.
  };

  // Returns a list of input methods that are currently active or supported
  // depending on |mode|. Returns NULL on error.
  InputMethodDescriptors* GetInputMethods(GetInputMethodMode mode) {
    GList* engines = NULL;
    if (mode == kActiveInputMethods) {
      DLOG(INFO) << "GetInputMethods (kActiveInputMethods)";
      engines = ibus_bus_list_active_engines(ibus_);
    } else if (mode == kSupportedInputMethods) {
      DLOG(INFO) << "GetInputMethods (kSupportedInputMethods)";
      engines = ibus_bus_list_engines(ibus_);
    } else {
      NOTREACHED();
      return NULL;
    }
    // Note that it's not an error for |engines| to be NULL.
    // NULL simply means an empty GList.

    InputMethodDescriptors* input_methods = new InputMethodDescriptors;
    AddInputMethodNames(engines, input_methods);

    FreeInputMethodNames(engines);
    return input_methods;
  }

  // Called by cros API ChromeOS(Activate|Deactive)ImeProperty().
  void SetImePropertyActivated(const char* key, bool activated) {
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
    g_object_unref(context);

    UpdateUI();
  }

  // Called by cros API ChromeOSChangeInputMethod().
  bool ChangeInputMethod(const char* name) {
    if (input_context_path_.empty()) {
      LOG(ERROR) << "Input context is unknown";
      return false;
    }

    IBusInputContext* context = GetInputContext(input_context_path_, ibus_);
    if (!context) {
      LOG(ERROR) << "Input context is unknown";
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

    ibus_input_context_set_engine(context, name);
    g_object_unref(context);
    UpdateUI();
    return true;
  }

  // Get a configuration of ibus-daemon or IBus engines and stores it on
  // |out_value|. Returns true if |gvalue| is successfully updated.
  //
  // For more information, please read a comment for GetImeConfig() function
  // in chromeos_language.h.
  bool GetImeConfig(const char* section,
                    const char* config_name,
                    ImeConfigValue* out_value) {
    DCHECK(section);
    DCHECK(config_name);
    if (!section || !config_name) {
      return false;
    }
    GValue gvalue = { 0 };  // g_value_init() is not necessary.

    IBusConfig* ibus_config = CreateConfigObject();
    g_return_val_if_fail(ibus_config, false);

    // TODO(yusukes): make sure the get_value() function does not abort even
    // when the ibus-memconf process does not exist.
    bool success =
        ibus_config_get_value(ibus_config, section, config_name, &gvalue);
    g_object_unref(ibus_config);
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
    DCHECK(section);
    DCHECK(config_name);
    if (!section || !config_name) {
      return false;
    }

    // Sanity check: do not preload unknown/unsupported input methods.
    std::vector<std::string> string_list = value.string_list_value;
    if ((value.type == ImeConfigValue::kValueTypeStringList) &&
        !strcmp(section, kGeneralSectionName) &&
        !strcmp(config_name, kPreloadEnginesConfigName)) {
      FilterInputMethods(value.string_list_value, &string_list);
    }
  
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

    IBusConfig* ibus_config = CreateConfigObject();
    g_return_val_if_fail(ibus_config, false);
    const gboolean success = ibus_config_set_value(ibus_config,
                                                   section,
                                                   config_name,
                                                   &gvalue);
    g_object_unref(ibus_config);
    g_value_unset(&gvalue);
    
    DLOG(INFO) << "SetImeConfig: " << section << "/" << config_name
               << ": result=" << success;
    return (success == TRUE);
  }

  // Checks if IBus connection is alive.
  bool ConnectionIsAlive() {
    return dbus_proxy_is_alive_ && ibus_ && ibus_bus_is_connected(ibus_);
  }
  
 private:
  // Creates IBusConfig object. Caller have to call g_object_unref() for the
  // returned object.
  IBusConfig* CreateConfigObject() {
    // Get the IBus connection which is owned by ibus_. ibus_bus_get_connection
    // nor ibus_config_new don't ref() the |ibus_connection| object. This means
    // that the |ibus_connection| object could be destructed, regardless of
    // whether an IBusConfig object exists, when ibus-daemon reboots. For this
    // reason, it seems to be safer to create IBusConfig object every time
    // using a fresh pointer which is returned from ibus_bus_get_connection(),
    // rather than holding it as a member varibale of this class.
    IBusConnection* ibus_connection = ibus_bus_get_connection(ibus_);
    if (!ibus_connection) {
      LOG(ERROR) << "ibus_bus_get_connection() failed";
      return NULL;
    }
    if (!ibus_connection_is_connected(ibus_connection)) {
      LOG(ERROR) << "ibus_connection_is_connected() failed";
      return NULL;
    }

    // TODO(satorux): Creating an IBusConfig is expensive. We should reuse
    // this object. That is, SetImeConfig() is called many times from
    // Chrome, and every time it's called, IBusConfig object is created
    // here, which is not efficient.
    IBusConfig* ibus_config = ibus_config_new(ibus_connection);
    if (!ibus_config) {
      LOG(ERROR) << "ibus_config_new() failed";
      return NULL;
    }

    return ibus_config;
  }

  // Handles "FocusIn" signal from the candidate_window process.
  void FocusIn(const char* input_context_path) {
    DCHECK(input_context_path) << "NULL context passed";
    if (!input_context_path) {
      LOG(ERROR) << "NULL context passed";
    }
    DLOG(INFO) << "FocusIn: " << input_context_path;

    // Remember the current ic path.
    input_context_path_ = Or(input_context_path, "");

    if (focus_changed_) {
      focus_changed_(language_library_, true);
    }
    UpdateUI();  // This is necessary since input method status is held per ic.
  }

  // Handles "FocusOut" signal from the candidate_window process.
  void FocusOut(const char* input_context_path) {
    DCHECK(input_context_path) << "NULL context passed";
    DLOG(INFO) << "FocusOut: " << input_context_path;
    if (focus_changed_) {
      focus_changed_(language_library_, false);
    }
  }

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

  // Retrieve input method status and notify them to the UI.
  void UpdateUI() {
    IBusEngineDesc* engine_desc = ibus_bus_get_global_engine(ibus_);
    if (!engine_desc) {
      LOG(ERROR) << "Global engine is not set";
      return;
    }

    InputMethodDescriptor current_input_method(engine_desc->name,
                                               engine_desc->longname,
                                               engine_desc->layout,
                                               engine_desc->language);
    DLOG(INFO) << "Updating the UI. ID:" << current_input_method.id
               << ", display_name:" << current_input_method.display_name
               << ", keyboard_layout:" << current_input_method.keyboard_layout;

    // Notify the change to update UI.
    current_input_method_changed_(language_library_, current_input_method);
    g_object_unref(engine_desc);
  }

  static void DBusProxyDestroyCallback(DBusGProxy* proxy, gpointer user_data) {
    InputMethodStatusConnection* self
        = static_cast<InputMethodStatusConnection*>(user_data);
    if (self) {
      self->dbus_proxy_is_alive_ = false;
    }
    LOG(ERROR) << "D-Bus connection to candidate_window is terminated!";
  }

  static void IBusBusDisconnectedCallback(IBusBus* bus, gpointer user_data) {
    LOG(ERROR) << "IBus connection to ibus-daemon is terminated!";
    // TODO(yusukes): Desturct |ibus_| object.
  }

  static void IBusBusGlobalEngineChangedCallback(
      IBusBus* bus, gpointer user_data) {
    InputMethodStatusConnection* self
        = static_cast<InputMethodStatusConnection*>(user_data);
    if (self) {
      self->UpdateUI();
    }
    DLOG(INFO) << "Global engine is changed";
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

    InputMethodStatusConnection* self
        = static_cast<InputMethodStatusConnection*>(object);
    // Checks the connection to ibus-daemon is still alive. Note that the
    // |connection| object is for candidate_window, not for ibus-daemon!
    if (!self->ConnectionIsAlive()) {
      LOG(ERROR) << "D-Bus or IBus connection (likely the latter) is lost!";
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

  // A function pointers which point LanguageLibrary::XXXHandler functions.
  // The functions are used when libcros receives signals from the
  // candidate_window.
  LanguageCurrentInputMethodMonitorFunction current_input_method_changed_;
  LanguageRegisterImePropertiesFunction register_ime_properties_;
  LanguageUpdateImePropertyFunction update_ime_property_;
  LanguageFocusChangeMonitorFunction focus_changed_;

  // Points to a chromeos::LanguageLibrary object. |language_library_| is used
  // as the first argument of the monitor functions above.
  void* language_library_;

  // Connections to IBus and DBus.
  IBusBus* ibus_;
  scoped_ptr<dbus::BusConnection> dbus_connection_;
  scoped_ptr<dbus::Proxy> dbus_proxy_;
  bool dbus_proxy_is_alive_;

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
    LanguageFocusChangeMonitorFunction focus_changed) {
  DLOG(INFO) << "MonitorInputMethodStatus";
  InputMethodStatusConnection* connection = new InputMethodStatusConnection(
      language_library,
      current_input_method_changed,
      register_ime_properties,
      update_ime_property,
      focus_changed);
  if (!connection->Init()) {
    DLOG(INFO) << "Failed to Init() InputMethodStatusConnection. "
               << "Returning NULL";
    delete connection;
    connection = NULL;
  }
  return connection;
}

extern "C"
void ChromeOSDisconnectInputMethodStatus(
    InputMethodStatusConnection* connection) {
  LOG(INFO) << "DisconnectInputMethodStatus";
  delete connection;
}

extern "C"
InputMethodDescriptors* ChromeOSGetActiveInputMethods(
    InputMethodStatusConnection* connection) {
  g_return_val_if_fail(connection, NULL);
  // Pass ownership to a caller. Note: GetInputMethods() might return NULL.
  return connection->GetInputMethods(
      InputMethodStatusConnection::kActiveInputMethods);
}

extern "C"
InputMethodDescriptors* ChromeOSGetSupportedInputMethods(
    InputMethodStatusConnection* connection) {
  g_return_val_if_fail(connection, NULL);
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
bool ChromeOSGetImeConfig(InputMethodStatusConnection* connection,
                          const char* section,
                          const char* config_name,
                          ImeConfigValue* out_value) {
  DCHECK(out_value);
  g_return_val_if_fail(connection, FALSE);
  return connection->GetImeConfig(section, config_name, out_value);
}

extern "C"
bool ChromeOSSetImeConfig(InputMethodStatusConnection* connection,
                          const char* section,
                          const char* config_name,
                          const ImeConfigValue& value) {
  g_return_val_if_fail(connection, FALSE);
  return connection->SetImeConfig(section, config_name, value);
}

extern "C"
bool ChromeOSInputMethodStatusConnectionIsAlive(
    InputMethodStatusConnection* connection) {
  g_return_val_if_fail(connection, false);
  const bool is_connected = connection->ConnectionIsAlive();
  if (!is_connected) {
    LOG(WARNING) << "ChromeOSInputMethodStatusConnectionIsAlive: NOT alive";
  }
  return is_connected;
}

}  // namespace chromeos
