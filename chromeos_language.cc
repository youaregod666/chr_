// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_language.h"

#include <X11/Xlib.h>
#include <dbus/dbus-glib-lowlevel.h>  // for dbus_g_connection_get_connection.
#include <ibus.h>

#include <algorithm>  // for std::sort.
#include <cstring>  // for std::strcmp.
#include <sstream>
#include <stack>
#include <utility>

#include "chromeos/dbus/dbus.h"
#include "chromeos/glib/object.h"

// Note: X11/Xlib.h defines macros like FocusIn and FocusOut. Since we're using
// them as function names in this file, undef them here.
#undef FocusIn
#undef FocusOut

namespace {

const char kCandidateWindowService[] = "org.freedesktop.IBus.Panel";
const char kCandidateWindowObjectPath[] = "/org/chromium/Chrome/LanguageBar";
const char kCandidateWindowInterface[] = "org.freedesktop.IBus.Panel";

// The list of IME IDs that we handle. This filtering is necessary since some
// IMEs are definitely unnecessary for us. For example, we should disable
// "ja:anthy", "zh:cangjie", and "zh:pinyin" engines in ibus-m17n since we
// (will) have better equivalents outside of ibus-m17n.
const char* kImeIdsWhitelist[] = {
  "anthy",  // ibus-anthy (for libcros debugging on Ubuntu 9.10)
  // "chewing",  // ibus-chewing
  "hangul",  // ibus-hangul
  "pinyin",  // ibus-pinyin
  // TODO(yusukes): re-enable chewing once we resolve issue 1253.

  // ibus-table IMEs.
  "cangjie3",  // ibus-table-cangjie
  "cangjie5",  // ibus-table-cangjie
  // TODO(yusukes): Add additional ibus-table modules here once they're ready.

  // ibus-m17n IMEs (language neutral ones).
  "m17n:t:latn-pre",
  "m17n:t:latn-post",

  // ibus-m17n IMEs.
  "m17n:ar:kbd",  // Arabic
  "m17n:hr:kbd",  // Croatian
  "m17n:da:post",  // Danish
  "m17n:el:kbd",  // Modern Greek
  "m17n:he:kbd",  // Hebrew
  "m17n:hi:itrans",  // Hindi
  // Note: the m17n-contrib package has some more Hindi definitions.
  "m17n:fa:isiri",  // Persian
  "m17n:sr:kbd",  // Serbian
  "m17n:sk:kbd",  // Slovak
  // TODO(yusukes): Check if we should use the ibus-m17n IME for Swedish.
  "m17n:th:pattachote",  // Thai
  // TODO(yusukes): Add tis820 and kesmanee for Thai if needed.
  // TODO(yusukes): Update m17n-db package to the latest so we can use
  // Vietnamese IME which does not require the "get surrounding text" feature.

  // ibux-xkb-layouts IMEs (keyboard layouts).
  "xkb:fi::fin",
  "xkb:fr::fra",
  "xkb:jp::jpn",
  "xkb:us::eng",
  "xkb:us:dvorak:eng",
  // TODO(satorux): Add more keyboard layouts.
};

// The list of IME property keys that we don't handle.
const char* kImePropertyKeysBlacklist[] = {
  "setup",  // menu for showing setup dialog used in anthy and hangul.
  "chewing_settings_prop",  // menu for showing setup dialog used in chewing.
  "status",  // used in m17n.
};

const char* Or(const char* str1, const char* str2) {
  return str1 ? str1 : str2;
}

// Returns true if |key| is blacklisted.
bool PropertyKeyIsBlacklisted(const char* key) {
  for (size_t i = 0; i < arraysize(kImePropertyKeysBlacklist); ++i) {
    if (!std::strcmp(key, kImePropertyKeysBlacklist[i])) {
      return true;
    }
  }
  return false;
}

// Returns true if |ime_id| is whitelisted.
bool ImeIdIsWhitelisted(const char* ime_id) {
  // TODO(yusukes): Use hash_set if necessary.
  const std::string id = ime_id;
  for (size_t i = 0; i < arraysize(kImeIdsWhitelist); ++i) {
    // Older version of m17n-db which is used in Ubuntu 9.10
    // doesn't add the "m17n:" prefix. We support both.
    if ((id == kImeIdsWhitelist[i]) || ("m17n:" + id == kImeIdsWhitelist[i])) {
      return true;
    }
  }
  return false;
}

// Frees IME names in |engines| and the list itself. Please make sure that
// |engines| points the head of the list.
void FreeIMELanguages(GList* engines) {
  if (engines) {
    for (GList* cursor = engines; cursor; cursor = g_list_next(cursor)) {
      g_object_unref(IBUS_ENGINE_DESC(cursor->data));
    }
    g_list_free(engines);
  }
}

// Copies IME names in |engines| to |out|.
void AddIMELanguages(const GList* engines, chromeos::InputLanguageList* out) {
  DCHECK(out);
  for (; engines; engines = g_list_next(engines)) {
    IBusEngineDesc* engine_desc = IBUS_ENGINE_DESC(engines->data);
    if (ImeIdIsWhitelisted(engine_desc->name)) {
      out->push_back(chromeos::InputLanguage(
          chromeos::LANGUAGE_CATEGORY_IME,
          engine_desc->name, engine_desc->longname, engine_desc->icon,
          engine_desc->language));
      LOG(INFO) << engine_desc->name << " (SUPPORTED)";
    } else {
      LOG(INFO) << engine_desc->name << " (not supported)";
    }
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
      "" /* no icon */,
      kFallbackXKBLanguageCode));  // mock
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
class LanguageStatusConnection {
 public:
  LanguageStatusConnection(LanguageStatusMonitorFunctions monitor_functions,
                           void* language_library)
      : monitor_functions_(monitor_functions),
        language_library_(language_library),
        ibus_(NULL),
        input_context_path_("") {
    DCHECK(monitor_functions_.current_language);
    DCHECK(monitor_functions_.register_ime_properties);
    DCHECK(monitor_functions_.update_ime_property);
    DCHECK(language_library_);
  }

  ~LanguageStatusConnection() {
    // Destruct IBus object.
    if (ibus_) {
      if (ibus_bus_is_connected(ibus_)) {
        // Close |dbus_connection_| since the connection is "private connection"
        // and we know |this| is the only instance which uses the
        // |dbus_connection_|. Otherwise, we may see an error message from dbus
        // library like "The last reference on a connection was dropped without
        // closing the connection."
        DBusConnection* raw_connection = dbus_g_connection_get_connection(
            dbus_connection_->g_connection());
        if (raw_connection) {
          ::dbus_connection_close(raw_connection);
        }
      }

      // Since the connection which ibus_ has is a "shared connection". We
      // should not close the connection. Just call g_object_unref.
      g_object_unref(ibus_);
    }

    // |dbus_connection_| and |dbus_proxy_| will be destructed by ~scoped_ptr().
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

    // TODO(yusukes): Investigate what happens if candidate_window process is
    // restarted. I'm not sure but we should use dbus_g_proxy_new_for_name(),
    // not dbus_g_proxy_new_for_name_owner()?
    // TODO(yusukes): Investigate if we can automatically restart ibus-gconf,
    // candidate_window, (and ibus-x11?) processes when they die. Upstart only
    // takes care of respawning the ibus-daemon process.

    return true;
  }

  // GetLanguagesMode is used for GetLanguages().
  enum GetLanguagesMode {
    kActiveLanguages,  // Get active languages.
    kSupportedLanguages,  // Get supported languages.
  };

  // Returns a list of IMEs and XKB layouts that are currently active or
  // supported depending on |mode|. Returns NULL on error.
  InputLanguageList* GetLanguages(GetLanguagesMode mode) {
    GList* engines = NULL;
    if (mode == kActiveLanguages) {
      LOG(INFO) << "GetLanguages (kActiveLanguages)";
      engines = ibus_bus_list_active_engines(ibus_);
    } else if (mode == kSupportedLanguages) {
      LOG(INFO) << "GetLanguages (kSupportedLanguages)";
      engines = ibus_bus_list_engines(ibus_);
    } else {
      NOTREACHED();
      return NULL;
    }
    // Note that it's not an error for |engines| to be NULL.
    // NULL simply means an empty GList.

    InputLanguageList* language_list = new InputLanguageList;
    AddIMELanguages(engines, language_list);
    AddXKBLayouts(language_list);
    std::sort(language_list->begin(), language_list->end());

    FreeIMELanguages(engines);
    return language_list;
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

  // Called by cros API ChromeOSSetLanguageActivated().
  bool SetXkbActivated(const char* xkb_name, bool activated) {
    // TODO(yusukes,satorux): implement this.
    return false;
  }

  // Called by cros API ChromeOSSetLanguageActivated().
  bool SetImeActivated(const char* ime_name, bool activated) {
    GList* const engines = ibus_bus_list_active_engines(ibus_);

    // Convert |engines| to a GValueArray of names.
    GValueArray* engine_names = g_value_array_new(0);
    for (GList* cursor = engines; cursor; cursor = g_list_next(cursor)) {
      IBusEngineDesc* engine_desc = IBUS_ENGINE_DESC(cursor->data);
      // Skip the IME if the mode is to deactivate and it matches the given
      // name.
      if (!activated && std::strcmp(engine_desc->name, ime_name) == 0) {
        continue;
      }
      GValue name_value = { 0 };
      g_value_init(&name_value, G_TYPE_STRING);
      g_value_set_string(&name_value, engine_desc->name);
      g_value_array_append(engine_names, &name_value);
      g_value_unset(&name_value);
    }

    if (activated) {
      // Add a new IME here.
      GValue name_value = { 0 };
      g_value_init(&name_value, G_TYPE_STRING);
      g_value_set_string(&name_value, ime_name);
      // Prepend to add the new IME as the first choice.
      g_value_array_prepend(engine_names, &name_value);
      g_value_unset(&name_value);
    }

    // Make the array into a GValue.
    GValue value = { 0 };
    g_value_init(&value, G_TYPE_VALUE_ARRAY);
    g_value_take_boxed(&value, engine_names);
    // We don't have to unref |engine_names| any more.

    // Set the config value.
    const bool success = SetImeConfig("general", "preload_engines", &value);
    g_value_unset(&value);

    FreeIMELanguages(engines);
    return success;
  }

  // Get a configuration of ibus-daemon or IBus engines and stores it on
  // |gvalue|. Returns true if |gvalue| is successfully updated.
  //
  // For more information, please read a comment for GetImeConfig() function
  // in chromeos_language.h.
  bool GetImeConfig(const char* section,
                    const char* config_name,
                    GValue* gvalue) {
    DCHECK(section);
    DCHECK(config_name);

    IBusConfig* ibus_config = CreateConfigObject();
    g_return_val_if_fail(ibus_config, false);

    // TODO(yusukes): make sure the get_value() function does not abort even
    // when the ibus-gconf process does not exist.
    const gboolean success = ibus_config_get_value(ibus_config,
                                                   section,
                                                   config_name,
                                                   gvalue);
    g_object_unref(ibus_config);
    return (success == TRUE);
  }

  // Updates a configuration of ibus-daemon or IBus engines with |gvalue|.
  // Returns true if the configuration is successfully updated.
  //
  // For more information, please read a comment for SetImeConfig() function
  // in chromeos_language.h.
  bool SetImeConfig(const char* section,
                    const char* config_name,
                    const GValue* gvalue) {
    DCHECK(section);
    DCHECK(config_name);

    IBusConfig* ibus_config = CreateConfigObject();
    g_return_val_if_fail(ibus_config, false);
    const gboolean success = ibus_config_set_value(ibus_config,
                                                   section,
                                                   config_name,
                                                   gvalue);
    g_object_unref(ibus_config);
    return (success == TRUE);
  }

  // Checks if IBus and X server (XKB) connections are alive.
  bool ConnectionIsAlive() {
    // TODO(yusukes): check X server (XKB) connection.

    // Note: Since the IBus connection automatically recovers even if
    // ibus-daemon reboots, ibus_bus_is_connected() usually returns true.
    return (ibus_ && (ibus_bus_is_connected(ibus_) == TRUE)) ? true : false;
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
    
    IBusConfig* ibus_config = ibus_config_new(ibus_connection);
    if (!ibus_config) {
      LOG(ERROR) << "ibus_config_new() failed";
      return NULL;
    }

    return ibus_config;
  }

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
    if (!input_context_path) {
      LOG(ERROR) << "NULL context passed";
    }
    DLOG(INFO) << "FocusIn: " << input_context_path;

    // Remember the current ic path.
    input_context_path_ = Or(input_context_path, "");
    UpdateUI();  // This is necessary since IME status is held per ic.
  }

  // Handles "FocusOut" signal from the candidate_window process.
  void FocusOut(const char* input_context_path) {
    DCHECK(input_context_path) << "NULL context passed";
    DLOG(INFO) << "FocusOut: " << input_context_path;
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
        g_object_unref(context);
        return;
      }
      current_language = InputLanguage(LANGUAGE_CATEGORY_IME,
                                       engine_desc->name,
                                       engine_desc->longname,
                                       engine_desc->icon,
                                       engine_desc->language);
    } else {
      DLOG(INFO) << "IME is not active";
      // Set XKB layout name on current_languages.
      current_language = InputLanguage(LANGUAGE_CATEGORY_XKB,
                                       kFallbackXKBId,
                                       kFallbackXKBDisplayName,
                                       "" /* no icon */,
                                       kFallbackXKBLanguageCode);  // mock
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
InputLanguageList* ChromeOSGetActiveLanguages(
    LanguageStatusConnection* connection) {
  g_return_val_if_fail(connection, NULL);
  // Pass ownership to a caller. Note: GetLanguages() might return NULL.
  return connection->GetLanguages(
      LanguageStatusConnection::kActiveLanguages);
}

extern "C"
InputLanguageList* ChromeOSGetSupportedLanguages(
    LanguageStatusConnection* connection) {
  g_return_val_if_fail(connection, NULL);
  // Pass ownership to a caller. Note: GetLanguages() might return NULL.
  return connection->GetLanguages(
      LanguageStatusConnection::kSupportedLanguages);
}

extern "C"
void ChromeOSSetImePropertyActivated(
    LanguageStatusConnection* connection, const char* key, bool activated) {
  DLOG(INFO) << "SetImePropertyeActivated: " << key << ": " << activated;
  DCHECK(key);
  g_return_if_fail(connection);
  connection->SetImePropertyActivated(key, activated);
}

// DEPRECATED: TODO(satorux): Remove this when it's ready.
extern "C"
void ChromeOSActivateImeProperty(
    LanguageStatusConnection* connection, const char* key) {
  ChromeOSSetImePropertyActivated(connection, key, true);
}

// DEPRECATED: TODO(satorux): Remove this when it's ready.
extern "C"
void ChromeOSDeactivateImeProperty(
    LanguageStatusConnection* connection, const char* key) {
  ChromeOSSetImePropertyActivated(connection, key, false);
}

extern "C"
void ChromeOSChangeLanguage(LanguageStatusConnection* connection,
                            LanguageCategory category,
                            const char* name) {
  DCHECK(name);
  DLOG(INFO) << "ChangeLanguage: " << name;
  g_return_if_fail(connection);
  connection->ChangeLanguage(category, name);
  // TODO(yusukes): The return type of this function should be bool.
}

extern "C"
bool ChromeOSSetLanguageActivated(LanguageStatusConnection* connection,
                                  LanguageCategory category,
                                  const char* name,
                                  bool activated) {
  DLOG(INFO) << "SetLanguageActivated: " << name << " [category "
             << category << "]" << ": " << activated;

  DCHECK(name);
  g_return_val_if_fail(connection, FALSE);
  bool success = false;
  if (category == LANGUAGE_CATEGORY_XKB) {
    success = connection->SetXkbActivated(name, activated);
  } else if (category == LANGUAGE_CATEGORY_IME) {
    success = connection->SetImeActivated(name, activated);
  }
  return success;
}

// DEPRECATED: TODO(satorux): Remove this when it's ready.
extern "C"
bool ChromeOSActivateLanguage(LanguageStatusConnection* connection,
                              LanguageCategory category,
                              const char* name) {
  return ChromeOSSetLanguageActivated(connection, category, name, true);
}

// DEPRECATED: TODO(satorux): Remove this when it's ready.
extern "C"
bool ChromeOSDeactivateLanguage(LanguageStatusConnection* connection,
                                LanguageCategory category,
                                const char* name) {
  return ChromeOSSetLanguageActivated(connection, category, name, false);
}

extern "C"
bool ChromeOSGetImeConfig(LanguageStatusConnection* connection,
                          const char* section,
                          const char* config_name,
                          ImeConfigValue* out_value) {
  DCHECK(out_value);
  g_return_val_if_fail(connection, FALSE);

  GValue gvalue = { 0 };  // g_value_init() is not necessary.
  if (!connection->GetImeConfig(section, config_name, &gvalue)) {
    if (G_IS_VALUE(&gvalue)) {
      g_value_unset(&gvalue);
    }
    return false;
  }

  // Convert the type of the result from GValue to our structure.
  // TODO(yusukes): Support string list.
  bool success = true;
  switch (G_VALUE_TYPE(&gvalue)) {
    case G_TYPE_STRING: {
      const char* value = g_value_get_string(&gvalue);
      DCHECK(value);
      out_value->type = ImeConfigValue::kValueTypeString;
      out_value->string_value = value;
      break;
    }
    case G_TYPE_INT: {
      out_value->type = ImeConfigValue::kValueTypeInt;
      out_value->int_value = g_value_get_int(&gvalue);
      break;
    }
    case G_TYPE_BOOLEAN: {
      out_value->type = ImeConfigValue::kValueTypeBool;
      out_value->bool_value = g_value_get_boolean(&gvalue);
      break;
    }
    default: {
      LOG(ERROR) << "Unsupported config type: " << G_VALUE_TYPE(&gvalue);
      success = false;
      break;
    }      
  }

  g_value_unset(&gvalue);
  return success;
}

extern "C"
bool ChromeOSSetImeConfig(LanguageStatusConnection* connection,
                          const char* section,
                          const char* config_name,
                          const ImeConfigValue& value) {
  g_return_val_if_fail(connection, FALSE);

  // Convert the type of |value| from our structure to GValue.
  // TODO(yusukes): Support ImeConfigValue::kValueTypeStringList.
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
    default:
      LOG(ERROR) << "Unsupported config type: " << value.type;
      return false;
  }

  const bool success = connection->SetImeConfig(section, config_name, &gvalue);
  g_value_unset(&gvalue);

  return success;
}

extern "C"
bool ChromeOSLanguageStatusConnectionIsAlive(
    LanguageStatusConnection* connection) {
  g_return_val_if_fail(connection, false);
  const bool is_connected = connection->ConnectionIsAlive();
  DLOG(INFO) << "ChromeOSLanguageStatusConnectionIsAlive: "
             << (is_connected ? "" : "NOT ") << "alive";
  return is_connected;
}

}  // namespace chromeos
