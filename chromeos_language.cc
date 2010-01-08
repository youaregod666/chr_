// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_language.h"

#include <base/logging.h>
#include <ibus.h>

#include <algorithm>  // for std::sort.

#include "chromeos/dbus/dbus.h"
#include "chromeos/glib/object.h"

namespace {

const char kCandidateWindowService[] = "org.freedesktop.IBus.Panel";
const char kCandidateWindowObjectPath[] = "/org/chromium/Chrome/LanguageBar";
const char kCandidateWindowInterface[] = "org.freedesktop.IBus.Panel";

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

}  // namespace

namespace chromeos {

// A class that holds IBus and DBus connections.
class LanguageStatusConnection {
 public:
  LanguageStatusConnection(LanguageStatusMonitorFunction monitor_function,
                           void* language_library)
      : monitor_function_(monitor_function),
        language_library_(language_library),
        ibus_(NULL),
        ibus_config_(NULL),
        dbus_focus_in_(NULL),
        dbus_focus_out_(NULL),
        dbus_state_changed_(NULL),
        input_context_path_("") {
    DCHECK(monitor_function_);
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
    if (dbus_focus_in_) {
      dbus::Disconnect(dbus_focus_in_);
    }
    if (dbus_focus_out_) {
      dbus::Disconnect(dbus_focus_out_);
    }
    if (dbus_state_changed_) {
      dbus::Disconnect(dbus_state_changed_);
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
    // Chromium OS to handle "FocusIn", "FocusOut", and "StateChanged" signals
    // from the process.
    const char* address = ibus_get_address();
    dbus::BusConnection dbus(dbus::GetPrivateBusConnection(address));
    LOG(INFO) << "Established private D-Bus connection to: '" << address << "'";

    const bool kConnectToNameOwner = true;
    // TODO(yusukes): dbus::Proxy instantiation might fail (and abort due to
    // DCHECK failure) when candidate_window process does not exist yet.
    // Would be better to add "bool dbus::Proxy::Init()" or something like that
    // to handle such case?
    dbus::Proxy candidate_window(dbus,
                                 kCandidateWindowService,
                                 kCandidateWindowObjectPath,
                                 kCandidateWindowInterface,
                                 kConnectToNameOwner);

    if (!candidate_window) {
      LOG(ERROR) << "Can't construct proxy for the candidate window. "
                 << "candidate window is not running?";
      return false;
    }

    dbus_focus_in_ = dbus::Monitor(candidate_window,
                                   "FocusIn",
                                   &LanguageStatusConnection::FocusIn,
                                   this);
    dbus_focus_out_ = dbus::Monitor(candidate_window,
                                    "FocusOut",
                                    &LanguageStatusConnection::FocusOut,
                                    this);
    dbus_state_changed_ = dbus::Monitor(candidate_window,
                                        "StateChanged",
                                        &LanguageStatusConnection::StateChanged,
                                        this);

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

  // Called by cros API ChromeOSChangeLanguage().
  void SwitchXKB(const char* name) {
    // TODO(yusukes): implement XKB switching.

    if (input_context_path_.empty()) {
      LOG(ERROR) << "Input context is unknown";
      return;
    }

    IBusInputContext* context
        = ibus_input_context_get_input_context(input_context_path_.c_str(),
                                               ibus_bus_get_connection(ibus_));
    ibus_input_context_disable(context);
    g_object_unref(context);
    UpdateUI();
  }

  // Called by cros API ChromeOSChangeLanguage() as well.
  void SwitchIME(const char* name) {
    if (input_context_path_.empty()) {
      LOG(ERROR) << "Input context is unknown";
      return;
    }

    IBusInputContext* context
        = ibus_input_context_get_input_context(input_context_path_.c_str(),
                                               ibus_bus_get_connection(ibus_));
    ibus_input_context_set_engine(context, name);
    g_object_unref(context);
    UpdateUI();
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
    IBusInputContext* context
        = ibus_input_context_get_input_context(
            input_context_path_.c_str(), ibus_bus_get_connection(ibus_));

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
  // Handles "FocusIn" signal from candidate_window.
  static void FocusIn(void* object, const char* input_context_path) {
    DCHECK(input_context_path) << "NULL context passed";
    DLOG(INFO) << "FocusIn: " << input_context_path;
    DCHECK(object);

    // Remember the current ic path.
    LanguageStatusConnection* self
        = static_cast<LanguageStatusConnection*>(object);
    self->input_context_path_ = input_context_path;
    self->UpdateUI();  // This is necessary since IME status is held per ic.
  }

  // Handles "FocusOut" signal from candidate_window.
  static void FocusOut(void* object, const char* input_context_path) {
    DCHECK(input_context_path) << "NULL context passed";
    DLOG(INFO) << "FocusOut: " << input_context_path;
    DCHECK(object);
  }

  // Handles "StateChanged" signal from candidate_window process.
  static void StateChanged(void* object, const char* dummy) {
    // TODO(yusukes): Modify common/chromeos/dbus/dbus.h so that we can handle
    // signals without argument. Then remove the |dummy|.
    DLOG(INFO) << "StateChanged";
    DCHECK(object);
    LanguageStatusConnection* self
        = static_cast<LanguageStatusConnection*>(object);
    self->UpdateUI();
  }

  // Retrieve IME/XKB status and notify them to the UI.
  void UpdateUI() {
    if (input_context_path_.empty()) {
      LOG(ERROR) << "Input context is unknown";
      return;
    }

    IBusInputContext* context
        = ibus_input_context_get_input_context(
            input_context_path_.c_str(), ibus_bus_get_connection(ibus_));

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
    monitor_function_(language_library_, current_language);
    g_object_unref(context);
  }

  // A function pointer which points LanguageLibrary::LanguageChangedHandler
  // function. |monitor_funcion_| is called when Chrome UI needs to be updated.
  LanguageStatusMonitorFunction monitor_function_;
  // Points to a chromeos::LanguageLibrary object. |language_library_| is used
  // as the first argument of the |monitor_function_| function.
  void* language_library_;

  // Connections to IBus and DBus.
  typedef dbus::MonitorConnection<void (const char*)>* DBusConnectionType;
  IBusBus* ibus_;
  IBusConfig* ibus_config_;
  DBusConnectionType dbus_focus_in_;
  DBusConnectionType dbus_focus_out_;
  DBusConnectionType dbus_state_changed_;

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
    LanguageStatusMonitorFunction monitor_function, void* language_library) {
  LOG(INFO) << "MonitorLanguageStatus";
  LanguageStatusConnection* connection
      = new LanguageStatusConnection(monitor_function, language_library);
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
void ChromeOSChangeLanguage(LanguageStatusConnection* connection,
                            LanguageCategory category,
                            const char* name) {
  // TODO(yusukes): Add DCHECK(connection); here when candidate_window for
  // Chrome OS gets ready.
  if (!connection) {
    LOG(WARNING) << "LanguageStatusConnection is NULL";
    return;
  }
  DCHECK(name);
  DLOG(INFO) << "ChangeLanguage: " << name << " [category " << category << "]";
  switch (category) {
    case LANGUAGE_CATEGORY_XKB:
      connection->SwitchXKB(name);
      break;
    case LANGUAGE_CATEGORY_IME:
      connection->SwitchIME(name);
      break;
  }
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
