// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_ime.h"

#include <base/logging.h>
#include <base/string_util.h>
#include <ibus.h>
#include <ibuspanelservice.h>

namespace chromeos {

// IBusChromeOSPanelService is a subclass of IBusPanelService, that is
// used for implementing IME UI for Chrome OS.
//
// The anonymous namespace contains boilerplate code for creating a sub
// class with GObject, as well as member functions.
namespace {

const char kLanguageBarObjectPath[] = "/org/chromium/Chrome/LanguageBar";

// Define IBusChromeOSPanelService class.
#define IBUS_TYPE_CHROMEOS_PANEL_SERVICE        \
  (ibus_chromeos_panel_service_get_type())
#define IBUS_CHROMEOS_PANEL_SERVICE(obj)                                \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), IBUS_TYPE_CHROMEOS_PANEL_SERVICE,  \
                              IBusChromeOSPanelService))
#define IBUS_CHROMEOS_PANEL_SERVICE_CLASS(klass)                        \
  (G_TYPE_CHECK_CLASS_CAST((klass), IBUS_TYPE_CHROMEOS_PANEL_SERVICE,   \
                           IBusChromeOSPanelServiceClass))
#define CHROMEOS_IS_PANEL_SERVICE(obj)                                  \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), IBUS_TYPE_CHROMEOS_PANEL_SERVICE))
#define CHROMEOS_IS_PANEL_SERVICE_CLASS(klass)                          \
  (G_TYPE_CHECK_CLASS_TYPE((klass), IBUS_TYPE_CHROMEOS_PANEL_SERVICE))
#define IBUS_CHROMEOS_PANEL_SERVICE_GET_CLASS(obj)                      \
  (G_TYPE_INSTANCE_GET_CLASS((obj), IBUS_TYPE_CHROMEOS_PANEL_SERVICE,   \
                             IBusChromeOSPanelService))

// The class definition.
struct IBusChromeOSPanelService {
  // The parent object.
  IBusPanelService service;

  // The IBus connection is used for sending signals.
  // The ownership is not transferred.
  IBusConnection* ibus_connection;

  // The object of the client IME library. This will be used as the first
  // argument of monitor functions.
  void* ime_library;

  // The monitor functions are called upon certain events.
  chromeos::ImeStatusMonitorFunctions monitor_functions;
};

struct IBusChromeOSPanelServiceClass {
  IBusPanelServiceClass parent_class;
};

G_DEFINE_TYPE(IBusChromeOSPanelService, ibus_chromeos_panel_service,
              IBUS_TYPE_PANEL_SERVICE)

// Handles IBus's |FocusIn| method call.
// Just sends a signal to the language bar.
gboolean ibus_chromeos_panel_service_focus_in(IBusPanelService *panel,
                                              const gchar* input_context_path,
                                              IBusError **error) {
  IBusConnection* ibus_connection =
      IBUS_CHROMEOS_PANEL_SERVICE(panel)->ibus_connection;
  ibus_connection_send_signal(ibus_connection,
                              kLanguageBarObjectPath,
                              IBUS_INTERFACE_PANEL,
                              "FocusIn",
                              G_TYPE_STRING, &input_context_path,
                              G_TYPE_INVALID);
  return TRUE;
}

// Handles IBus's |FocusOut| method call.
// Just sends a signal to the language bar.
gboolean ibus_chromeos_panel_service_focus_out(IBusPanelService *panel,
                                               const gchar* input_context_path,
                                               IBusError **error) {
  IBusConnection* ibus_connection =
      IBUS_CHROMEOS_PANEL_SERVICE(panel)->ibus_connection;
  ibus_connection_send_signal(ibus_connection,
                              kLanguageBarObjectPath,
                              IBUS_INTERFACE_PANEL,
                              "FocusOut",
                              G_TYPE_STRING, &input_context_path,
                              G_TYPE_INVALID);
  return TRUE;
}

// Handles IBus's |HideLookupTable| method call.
// Calls |hide_lookup_table| in |monitor_functions|.
gboolean ibus_chromeos_panel_service_hide_lookup_table(IBusPanelService *panel,
                                                       IBusError **error) {
  const ImeStatusMonitorFunctions& monitor_functions =
      IBUS_CHROMEOS_PANEL_SERVICE(panel)->monitor_functions;
  void* ime_library =
      IBUS_CHROMEOS_PANEL_SERVICE(panel)->ime_library;

  if (monitor_functions.hide_lookup_table) {
    monitor_functions.hide_lookup_table(ime_library);
  }
  return TRUE;
}

// Handles IBus's |RegisterProperties| method call.
// Just sends a signal to the language bar.
gboolean ibus_chromeos_panel_service_register_properties(
    IBusPanelService* panel, IBusPropList* prop_list, IBusError** error) {
  IBusConnection* ibus_connection =
      IBUS_CHROMEOS_PANEL_SERVICE(panel)->ibus_connection;
  ibus_connection_send_signal(ibus_connection,
                              kLanguageBarObjectPath,
                              IBUS_INTERFACE_PANEL,
                              "RegisterProperties",
                              IBUS_TYPE_PROP_LIST, &prop_list,
                              G_TYPE_INVALID);
  return TRUE;
}

// Handles IBus's |UpdateLookupTable| method call.
// Creates a ImeLookupTable object and calls |update_lookup_table| in
// |monitor_functions|
gboolean ibus_chromeos_panel_service_update_lookup_table(
    IBusPanelService *panel,
    IBusLookupTable *table,
    gboolean visible,
    IBusError **error) {
  ImeLookupTable lookup_table;
  lookup_table.visible = (visible == TRUE);

  // Copy candidates to |lookup_table|.
  for (int i = 0; ; i++) {
    IBusText *text = ibus_lookup_table_get_candidate(table, i);
    if (!text) {
      break;
    }
    lookup_table.candidates.push_back(text->text);
  }

  lookup_table.cursor_absolute_index =
      ibus_lookup_table_get_cursor_pos(table);
  lookup_table.page_size = ibus_lookup_table_get_page_size(table);
  // Ensure that the page_size is non-zero to avoid div-by-zero error.
  if (lookup_table.page_size <= 0) {
    LOG(DFATAL) << "Invalid page size: " << lookup_table.page_size;
    lookup_table.page_size = 1;
  }

  lookup_table.cursor_row_index =
      lookup_table.cursor_absolute_index % lookup_table.page_size;
  lookup_table.current_page_index =
      lookup_table.cursor_absolute_index / lookup_table.page_size;

  // Compute the number of pages. This is a bit tricky.
  lookup_table.num_pages =
      lookup_table.candidates.size() / lookup_table.page_size;
  if (lookup_table.num_pages % lookup_table.page_size != 0) {
    lookup_table.num_pages += 1;
  }

  // Compute the number of candidates in the current page.
  lookup_table.num_candidates_in_current_page = lookup_table.page_size;
  if (lookup_table.current_page_index + 1 == lookup_table.num_pages) {
    // On the last page, the number can be smaller than the page size.
    lookup_table.num_candidates_in_current_page =
        lookup_table.candidates.size() % lookup_table.page_size;
  }

  const ImeStatusMonitorFunctions& monitor_functions =
      IBUS_CHROMEOS_PANEL_SERVICE(panel)->monitor_functions;
  void* ime_library =
      IBUS_CHROMEOS_PANEL_SERVICE(panel)->ime_library;

  if (monitor_functions.update_lookup_table) {
    monitor_functions.update_lookup_table(ime_library, lookup_table);
  }
  return TRUE;
}

// Handles IBus's |UpdateProperty| method call.
// Just sends a signal to the language bar.
gboolean ibus_chromeos_panel_service_update_property(
    IBusPanelService* panel, IBusProperty* prop, IBusError** error) {
  IBusConnection* ibus_connection =
      IBUS_CHROMEOS_PANEL_SERVICE(panel)->ibus_connection;
  ibus_connection_send_signal(ibus_connection,
                              kLanguageBarObjectPath,
                              IBUS_INTERFACE_PANEL,
                              "UpdateProperty",
                              IBUS_TYPE_PROPERTY, &prop,
                              G_TYPE_INVALID);
  return TRUE;
}

// Handles IBus's |StateChanged| method call.
// Just sends a signal to the language bar.
gboolean ibus_chromeos_panel_service_state_changed(IBusPanelService *panel,
                                                   IBusError **error) {
  IBusConnection* ibus_connection =
      IBUS_CHROMEOS_PANEL_SERVICE(panel)->ibus_connection;
  // TODO(yusukes): Get rid of the dummy string. As of writing, the
  // language bar needs a dummy string parameter for some implementation
  // reason.
  static const char kDummy[] = "dummy";
  const char* dummy = kDummy;
  ibus_connection_send_signal(ibus_connection,
                              kLanguageBarObjectPath,
                              IBUS_INTERFACE_PANEL,
                              "StateChanged",
                              G_TYPE_STRING, &dummy,
                              G_TYPE_INVALID);
  return TRUE;
}

// Handles IBus's |SetCursorLocation| method call.
// Calls |set_cursor_location| in |monitor_functions|.
gboolean ibus_chromeos_panel_service_set_cursor_location(
    IBusPanelService *panel,
    gint x,
    gint y,
    gint width,
    gint height,
    IBusError **error) {
  const ImeStatusMonitorFunctions& monitor_functions =
      IBUS_CHROMEOS_PANEL_SERVICE(panel)->monitor_functions;
  void* ime_library =
      IBUS_CHROMEOS_PANEL_SERVICE(panel)->ime_library;

  if (monitor_functions.set_cursor_location) {
    monitor_functions.set_cursor_location(ime_library, x, y, width, height);
  }
  return TRUE;
}

// Creates an IBusChromeOSPanelService. Returns as IBusPanelService for
// convenience (i.e. it can be passed to ibus_panel_service_* functions
// without cast).
IBusPanelService* ibus_chromeos_panel_service_new(
    IBusConnection *ibus_connection,
    void* ime_library,
    const ImeStatusMonitorFunctions& monitor_functions) {
  IBusPanelService* service = IBUS_PANEL_SERVICE(
      g_object_new(IBUS_TYPE_CHROMEOS_PANEL_SERVICE,
                   "path", IBUS_PATH_PANEL,
                   "connection", ibus_connection,
                   NULL));

  // Set members specific to IBusChromeOSPanelService.
  IBUS_CHROMEOS_PANEL_SERVICE(service)->ibus_connection =
      ibus_connection;
  IBUS_CHROMEOS_PANEL_SERVICE(service)->ime_library =
      ime_library;
  IBUS_CHROMEOS_PANEL_SERVICE(service)->monitor_functions =
      monitor_functions;
}

// Destroys the given IBusChromeOSPanelService object.
void ibus_chromeos_panel_service_destroy(IBusObject* object) {
  DLOG(INFO) << "ibus_chromeos_panel_service_destroy";
  IBUS_OBJECT_CLASS(ibus_chromeos_panel_service_parent_class)
      ->destroy(IBUS_OBJECT(object));
}

// Initializes the IBusChromeOSPanel class.
void ibus_chromeos_panel_service_class_init(
    IBusChromeOSPanelServiceClass* klass) {
  IBusPanelServiceClass* panel_class =
      reinterpret_cast<IBusPanelServiceClass*>(klass);
  // Install member functions. Sorted in alphabetical order.
  panel_class->focus_in = ibus_chromeos_panel_service_focus_in;
  panel_class->focus_out = ibus_chromeos_panel_service_focus_out;
  panel_class->hide_lookup_table =
      ibus_chromeos_panel_service_hide_lookup_table;
  panel_class->register_properties =
      ibus_chromeos_panel_service_register_properties;
  panel_class->set_cursor_location =
      ibus_chromeos_panel_service_set_cursor_location;
  panel_class->state_changed =
      ibus_chromeos_panel_service_state_changed;
  panel_class->update_lookup_table =
      ibus_chromeos_panel_service_update_lookup_table;
  panel_class->update_property = ibus_chromeos_panel_service_update_property;

  // Set the destructor function.
  IBusObjectClass* object_class =
      reinterpret_cast<IBusObjectClass*>(klass);
  object_class->destroy = ibus_chromeos_panel_service_destroy;
}

// Initializes the given IBusChromeOSPanelService object.
void ibus_chromeos_panel_service_init(IBusChromeOSPanelService* service) {
  service->ime_library = NULL;
  service->ibus_connection = NULL;
}

}  // namespace

// A thin wrapper for IBusPanelService.
class ImeStatusConnection {
 public:
  ImeStatusConnection(const ImeStatusMonitorFunctions& monitor_functions,
                      void* ime_library)
      : monitor_functions_(monitor_functions),
        ime_library_(ime_library),
        ibus_(NULL),
        ibus_panel_service_(NULL) {
  }

  ~ImeStatusConnection() {
    // ibus_panel_service_ depends on ibus_, thus unref it first.
    if (ibus_panel_service_) {
      g_object_unref(ibus_panel_service_);
    }
    if (ibus_) {
      g_object_unref(ibus_);
    }
  }

  // Initializes the object. Returns true on success.
  bool Init() {
    // Initialize an IBus bus.
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

    // Request the object name.
    const int status = ibus_bus_request_name(ibus_, IBUS_SERVICE_PANEL, 0);
    if (status == 0) {
      LOG(ERROR) << "ibus_bus_request_name() failed";
      return false;
    }

    // Establish the connection to ibus-daemon.  Note that the
    // ibus_connection object is owned by ibus_.
    IBusConnection* ibus_connection = ibus_bus_get_connection(ibus_);
    if (!ibus_connection) {
      LOG(ERROR) << "ibus_bus_get_connection() failed";
      return false;
    }

    // Create a ChromeOS's version of IBusPanelService.
    ibus_panel_service_ = ibus_chromeos_panel_service_new(ibus_connection,
                                                          ime_library_,
                                                          monitor_functions_);
    if (!ibus_panel_service_) {
      LOG(ERROR) << "ibus_chromeos_panel_service_new() failed";
      return false;
    }

    return true;
  }

  IBusPanelService* ibus_panel_service() {
    return ibus_panel_service_;
  }

 private:
  ImeStatusMonitorFunctions monitor_functions_;
  void* ime_library_;
  IBusBus* ibus_;
  IBusPanelService* ibus_panel_service_;
};

//
// cros APIs
//

// The function will be bound to chromeos::MonitorImeStatus with dlsym()
// in load.cc so it needs to be in the C linkage, so the symbol name does not
// get mangled.
extern "C"
ImeStatusConnection* ChromeOSMonitorImeStatus(
    const ImeStatusMonitorFunctions& monitor_functions,
    void* ime_library) {
  DLOG(INFO) << "MonitorImeStatus";

  ImeStatusConnection* connection =
      new ImeStatusConnection(monitor_functions, ime_library);
  if (!connection->Init()) {
    LOG(WARNING) << "Failed to Init() ImeStatusConnection. "
                 << "Returning NULL";
    delete connection;
    connection = NULL;
  }
  return connection;
}

extern "C"
void ChromeOSDisconnectImeStatus(ImeStatusConnection* connection) {
  DLOG(INFO) << "DisconnectLanguageStatus";
  delete connection;
}

extern "C"
void ChromeOSNotifyCandidateClicked(ImeStatusConnection* connection,
                                    int index, int button, int flags) {
  DLOG(INFO) << "NotifyCandidateClicked";
  DCHECK(connection);
  if (connection) {
    ibus_panel_service_candidate_clicked(connection->ibus_panel_service(),
                                         index,
                                         button,
                                         flags);
  }
}

}  // namespace chromeos
