// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_input_method_ui.h"

#include <base/logging.h>
#include <base/string_util.h>
#include <base/utf_string_conversions.h>
#include <ibus.h>
#include <ibuspanelservice.h>

namespace chromeos {

// IBusChromeOSPanelService is a subclass of IBusPanelService, that is
// used for implementing input method UI for Chrome OS.
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

  // The object of the client input method library. This will be used as
  // the first argument of monitor functions.
  void* input_method_library;

  // The monitor functions are called upon certain events.
  chromeos::InputMethodUiStatusMonitorFunctions monitor_functions;
};

struct IBusChromeOSPanelServiceClass {
  IBusPanelServiceClass parent_class;
};

G_DEFINE_TYPE(IBusChromeOSPanelService, ibus_chromeos_panel_service,
              IBUS_TYPE_PANEL_SERVICE)

// Checks the attribute if this indicates annotation.
gboolean IsAnnotation(IBusAttribute *attr) {
  g_return_val_if_fail(attr, FALSE);

  // Define annotation text color.
  static const guint kAnnotationColor = 0x888888;

  // Currently, we can discriminate annotation by specific value |attr->value|
  // TODO(nhiroki): We should change the way when iBus supports annotations.
  if (attr->type == IBUS_ATTR_TYPE_FOREGROUND &&
      attr->value == kAnnotationColor) {
    return TRUE;
  }
  return FALSE;
}

// Handles IBus's |FocusIn| method call.
// Just sends a signal to the language bar.
gboolean ibus_chromeos_panel_service_focus_in(IBusPanelService *panel,
                                              const gchar* input_context_path,
                                              IBusError **error) {
  // TODO(satorux): We should create an IBusError object and return it as
  // |error| if we return FALSE.  Otherwise, the program will crash in
  // ibuspanelservice.cc as the caller expects an error object to be
  // returned.
  LOG(INFO) << "Sending FocusIn signal to Chrome";
  g_return_val_if_fail(panel, FALSE);
  g_return_val_if_fail(input_context_path, FALSE);

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
  g_return_val_if_fail(panel, FALSE);
  g_return_val_if_fail(input_context_path, FALSE);

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

// Handles IBus's |HideAuxiliaryText| method call.
// Calls |hide_auxiliary_text| in |monitor_functions|.
gboolean ibus_chromeos_panel_service_hide_auxiliary_text(
    IBusPanelService *panel,
    IBusError **error) {
  g_return_val_if_fail(panel, FALSE);

  const InputMethodUiStatusMonitorFunctions& monitor_functions =
      IBUS_CHROMEOS_PANEL_SERVICE(panel)->monitor_functions;
  void* input_method_library =
      IBUS_CHROMEOS_PANEL_SERVICE(panel)->input_method_library;
  g_return_val_if_fail(monitor_functions.hide_auxiliary_text, FALSE);

  monitor_functions.hide_auxiliary_text(input_method_library);
  return TRUE;
}

// Handles IBus's |HideLookupTable| method call.
// Calls |hide_lookup_table| in |monitor_functions|.
gboolean ibus_chromeos_panel_service_hide_lookup_table(IBusPanelService *panel,
                                                       IBusError **error) {
  g_return_val_if_fail(panel, FALSE);

  const InputMethodUiStatusMonitorFunctions& monitor_functions =
      IBUS_CHROMEOS_PANEL_SERVICE(panel)->monitor_functions;
  void* input_method_library =
      IBUS_CHROMEOS_PANEL_SERVICE(panel)->input_method_library;
  g_return_val_if_fail(monitor_functions.hide_lookup_table, FALSE);

  monitor_functions.hide_lookup_table(input_method_library);
  return TRUE;
}

// Handles IBus's |RegisterProperties| method call.
// Just sends a signal to the language bar.
gboolean ibus_chromeos_panel_service_register_properties(
    IBusPanelService* panel, IBusPropList* prop_list, IBusError** error) {
  g_return_val_if_fail(panel, FALSE);
  g_return_val_if_fail(prop_list, FALSE);

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

// Handles IBus's |UpdateAuxiliaryText| method call.
// Converts IBusText to a std::string, and calls |update_auxiliary_text| in
// |monitor_functions|
gboolean ibus_chromeos_panel_service_update_auxiliary_text(
    IBusPanelService *panel,
    IBusText *text,
    gboolean visible,
    IBusError **error) {
  g_return_val_if_fail(panel, FALSE);
  g_return_val_if_fail(text, FALSE);
  g_return_val_if_fail(text->text, FALSE);

  const InputMethodUiStatusMonitorFunctions& monitor_functions =
      IBUS_CHROMEOS_PANEL_SERVICE(panel)->monitor_functions;
  void* input_method_library =
      IBUS_CHROMEOS_PANEL_SERVICE(panel)->input_method_library;
  g_return_val_if_fail(monitor_functions.update_auxiliary_text, FALSE);

  // Convert IBusText to a std::string. IBusText is an attributed text,
  const std::string simple_text = text->text;
  monitor_functions.update_auxiliary_text(input_method_library,
                                          simple_text,
                                          visible == TRUE);
  return TRUE;
}

// Returns an string representation of |table| for debugging.
std::string IBusLookupTableToString(IBusLookupTable* table) {
  std::stringstream stream;
  stream << "page_size: " << table->page_size << "\n";
  stream << "cursor_pos: " << table->cursor_pos << "\n";
  stream << "cursor_visible: " << table->cursor_visible << "\n";
  stream << "round: " << table->round << "\n";
  stream << "orientation: " << table->orientation << "\n";
  stream << "candidates:";
  for (int i = 0; ; i++) {
    IBusText *text = ibus_lookup_table_get_candidate(table, i);
    if (!text) {
      break;
    }
    stream << " " << text->text;
  }
  return stream.str();
}

// Handles IBus's |UpdateLookupTable| method call.
// Creates an InputMethodLookupTable object and calls |update_lookup_table| in
// |monitor_functions|
gboolean ibus_chromeos_panel_service_update_lookup_table(
    IBusPanelService *panel,
    IBusLookupTable *table,
    gboolean visible,
    IBusError **error) {
  g_return_val_if_fail(panel, FALSE);
  g_return_val_if_fail(table, FALSE);

  const InputMethodUiStatusMonitorFunctions& monitor_functions =
      IBUS_CHROMEOS_PANEL_SERVICE(panel)->monitor_functions;
  void* input_method_library =
      IBUS_CHROMEOS_PANEL_SERVICE(panel)->input_method_library;
  g_return_val_if_fail(monitor_functions.update_lookup_table, FALSE);

  InputMethodLookupTable lookup_table;
  lookup_table.visible = (visible == TRUE);

  // Copy the orientation information.
  const gint orientation = ibus_lookup_table_get_orientation(table);
  if (orientation == IBUS_ORIENTATION_VERTICAL) {
    lookup_table.orientation = InputMethodLookupTable::kVertical;
  } else if (orientation == IBUS_ORIENTATION_HORIZONTAL) {
    lookup_table.orientation = InputMethodLookupTable::kHorizontal;
  }

  // Copy candidates and annotations to |lookup_table|.
  for (int i = 0; ; i++) {
    IBusText *text = ibus_lookup_table_get_candidate(table, i);
    if (!text) {
      break;
    }

    if (!text->attrs || !text->attrs->attributes) {
      lookup_table.candidates.push_back(text->text);
      lookup_table.annotations.push_back("");
      continue;
    }

    // Divide candidate and annotation by specific attribute.
    const guint length = text->attrs->attributes->len;
    for (int j = 0; ; j++) {
      IBusAttribute *attr = ibus_attr_list_get(text->attrs, j);

      // The candidate does not have annotation.
      if (!attr) {
        lookup_table.candidates.push_back(text->text);
        lookup_table.annotations.push_back("");
        break;
      }

      // Check that the attribute indicates annotation.
      if (IsAnnotation(attr) && j + 1 == static_cast<int>(length)) {
        const std::wstring candidate_word =
            UTF8ToWide(text->text).substr(0, attr->start_index);
        lookup_table.candidates.push_back(WideToUTF8(candidate_word));

        const std::wstring annotation_word =
            UTF8ToWide(text->text).substr(attr->start_index, attr->end_index);
        lookup_table.annotations.push_back(WideToUTF8(annotation_word));

        break;
      }
    }
  }
  DCHECK_EQ(lookup_table.candidates.size(),
            lookup_table.annotations.size());

  // Copy labels to |lookup_table|.
  for (int i = 0; ; i++) {
    IBusText *text = ibus_lookup_table_get_label(table, i);
    if (!text) {
      break;
    }
    lookup_table.labels.push_back(text->text);
  }

  lookup_table.cursor_absolute_index =
      ibus_lookup_table_get_cursor_pos(table);
  lookup_table.page_size = ibus_lookup_table_get_page_size(table);
  // Ensure that the page_size is non-zero to avoid div-by-zero error.
  if (lookup_table.page_size <= 0) {
    LOG(DFATAL) << "Invalid page size: " << lookup_table.page_size;
    lookup_table.page_size = 1;
  }

  monitor_functions.update_lookup_table(input_method_library, lookup_table);
  return TRUE;
}

// Handles IBus's |UpdateProperty| method call.
// Just sends a signal to the language bar.
gboolean ibus_chromeos_panel_service_update_property(
    IBusPanelService* panel, IBusProperty* prop, IBusError** error) {
  g_return_val_if_fail(panel, FALSE);
  g_return_val_if_fail(prop, FALSE);

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
  g_return_val_if_fail(panel, FALSE);

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
  g_return_val_if_fail(panel, FALSE);

  const InputMethodUiStatusMonitorFunctions& monitor_functions =
      IBUS_CHROMEOS_PANEL_SERVICE(panel)->monitor_functions;
  void* input_method_library =
      IBUS_CHROMEOS_PANEL_SERVICE(panel)->input_method_library;
  g_return_val_if_fail(monitor_functions.set_cursor_location, FALSE);

  monitor_functions.set_cursor_location(input_method_library,
                                        x, y, width, height);
  return TRUE;
}

// Creates an IBusChromeOSPanelService. Returns as IBusPanelService for
// convenience (i.e. it can be passed to ibus_panel_service_* functions
// without cast).
IBusPanelService* ibus_chromeos_panel_service_new(
    IBusConnection *ibus_connection,
    void* input_method_library,
    const InputMethodUiStatusMonitorFunctions& monitor_functions) {
  IBusPanelService* service = IBUS_PANEL_SERVICE(
      g_object_new(IBUS_TYPE_CHROMEOS_PANEL_SERVICE,
                   "path", IBUS_PATH_PANEL,
                   "connection", ibus_connection,
                   NULL));

  // Set members specific to IBusChromeOSPanelService.
  IBUS_CHROMEOS_PANEL_SERVICE(service)->ibus_connection =
      ibus_connection;
  IBUS_CHROMEOS_PANEL_SERVICE(service)->input_method_library =
      input_method_library;
  IBUS_CHROMEOS_PANEL_SERVICE(service)->monitor_functions =
      monitor_functions;

  return service;
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
  panel_class->hide_auxiliary_text =
      ibus_chromeos_panel_service_hide_auxiliary_text;
  panel_class->hide_lookup_table =
      ibus_chromeos_panel_service_hide_lookup_table;
  panel_class->register_properties =
      ibus_chromeos_panel_service_register_properties;
  panel_class->set_cursor_location =
      ibus_chromeos_panel_service_set_cursor_location;
  panel_class->state_changed =
      ibus_chromeos_panel_service_state_changed;
  panel_class->update_auxiliary_text =
      ibus_chromeos_panel_service_update_auxiliary_text;
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
  service->input_method_library = NULL;
  service->ibus_connection = NULL;
}

}  // namespace

// A thin wrapper for IBusPanelService.
class InputMethodUiStatusConnection {
 public:
  InputMethodUiStatusConnection(
      const InputMethodUiStatusMonitorFunctions& monitor_functions,
      void* input_method_library)
      : monitor_functions_(monitor_functions),
        connection_change_handler_(NULL),
        input_method_library_(input_method_library),
        ibus_(NULL),
        ibus_panel_service_(NULL) {
  }

  ~InputMethodUiStatusConnection() {
    // ibus_panel_service_ depends on ibus_, thus unref it first.
    if (ibus_panel_service_) {
      g_object_unref(ibus_panel_service_);
    }
    if (ibus_) {
       g_signal_handlers_disconnect_by_func(
           ibus_,
           reinterpret_cast<gpointer>(
               G_CALLBACK(IBusBusConnectedCallback)),
           this);
       g_signal_handlers_disconnect_by_func(
           ibus_,
           reinterpret_cast<gpointer>(
               G_CALLBACK(IBusBusDisconnectedCallback)),
           this);
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

    g_signal_connect(ibus_,
                     "connected",
                     G_CALLBACK(IBusBusConnectedCallback),
                     this);
    g_signal_connect(ibus_,
                     "disconnected",
                     G_CALLBACK(IBusBusDisconnectedCallback),
                     this);

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
                                                          input_method_library_,
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

  void MonitorInputMethodConnection(
      InputMethodConnectionChangeMonitorFunction connection_change_handler) {
    connection_change_handler_ = connection_change_handler;
  }

 private:

  static void IBusBusConnectedCallback(IBusBus* bus, gpointer user_data) {
    LOG(WARNING) << "IBus connection is recovered.";
    InputMethodUiStatusConnection* self
        = static_cast<InputMethodUiStatusConnection*>(user_data);
    if (self) {
      if (self->connection_change_handler_)
        self->connection_change_handler_(self->input_method_library_, true);
    }
  }

  static void IBusBusDisconnectedCallback(IBusBus* bus, gpointer user_data) {
    LOG(ERROR) << "IBus connection is terminated!";
    InputMethodUiStatusConnection* self
        = static_cast<InputMethodUiStatusConnection*>(user_data);
    if (self) {
      if (self->connection_change_handler_)
        self->connection_change_handler_(self->input_method_library_, false);
    }
  }

  InputMethodUiStatusMonitorFunctions monitor_functions_;
  InputMethodConnectionChangeMonitorFunction connection_change_handler_;
  void* input_method_library_;
  IBusBus* ibus_;
  IBusPanelService* ibus_panel_service_;
};

//
// cros APIs
//

// The function will be bound to chromeos::MonitorInputMethodUiStatus with
// dlsym() in load.cc so it needs to be in the C linkage, so the symbol
// name does not get mangled.
extern "C"
InputMethodUiStatusConnection* ChromeOSMonitorInputMethodUiStatus(
    const InputMethodUiStatusMonitorFunctions& monitor_functions,
    void* input_method_library) {
  DLOG(INFO) << "MonitorInputMethodUiStatus";

  InputMethodUiStatusConnection* connection =
      new InputMethodUiStatusConnection(monitor_functions,
                                        input_method_library);
  if (!connection->Init()) {
    LOG(WARNING) << "Failed to Init() InputMethodUiStatusConnection. "
                 << "Returning NULL";
    delete connection;
    connection = NULL;
  }
  return connection;
}

extern "C"
void ChromeOSDisconnectInputMethodUiStatus(
    InputMethodUiStatusConnection* connection) {
  DLOG(INFO) << "DisconnectInputMethodUiStatus";
  delete connection;
}

extern "C"
void ChromeOSNotifyCandidateClicked(InputMethodUiStatusConnection* connection,
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

extern "C"
void ChromeOSMonitorInputMethodConnection(
    InputMethodUiStatusConnection* connection,
    InputMethodConnectionChangeMonitorFunction connection_change_handler) {
  DLOG(INFO) << "MonitorInputMethodConnection";
  DCHECK(connection);
  if (connection) {
    connection->MonitorInputMethodConnection(connection_change_handler);
  }
}

}  // namespace chromeos
