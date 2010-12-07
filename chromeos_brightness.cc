// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_brightness.h"

#include <string>

#include <dbus/dbus-glib-lowlevel.h>

#include "base/logging.h"
#include "base/stringprintf.h"
#include "chromeos/dbus/dbus.h"
#include "chromeos/dbus/service_constants.h"

using std::string;

namespace {

// Returns a string matching the D-Bus messages that we want to listen for.
string GetDBusMatchString() {
  return StringPrintf("type='signal', interface='%s', member='%s'",
                      power_manager::kPowerManagerInterface,
                      power_manager::kBrightnessChangedSignal);
}

}  // namespace

namespace chromeos {

class OpaqueBrightnessConnection {
 public:
  OpaqueBrightnessConnection(const BrightnessMonitorFunction& monitor_function,
                             void* object)
     : monitor_function_(monitor_function),
       object_(object) {
  }

  // A D-Bus message filter to receive signals.
  static DBusHandlerResult FilterDBusMessage(DBusConnection* dbus_conn,
                                             DBusMessage* message,
                                             void* data) {
    BrightnessConnection self = static_cast<BrightnessConnection>(data);
    if (dbus_message_is_signal(message,
                               power_manager::kPowerManagerInterface,
                               power_manager::kBrightnessChangedSignal)) {
      DBusError error;
      dbus_error_init(&error);
      int brightness_level = 0;
      if (dbus_message_get_args(message, &error,
                                DBUS_TYPE_INT32, &brightness_level,
                                DBUS_TYPE_INVALID)) {
        self->monitor_function_(self->object_, brightness_level);
      } else {
        LOG(WARNING) << "Unable to read brightness level from "
                     << power_manager::kBrightnessChangedSignal << " signal";
      }
      return DBUS_HANDLER_RESULT_HANDLED;
    } else {
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
  }

 private:
  // Function within Chrome that we invoke when the brightness changes.
  BrightnessMonitorFunction monitor_function_;

  // Opaque pointer supplied to ChromeOSMonitorBrightness() that we pass back
  // via |monitor_function_|.
  void* object_;
};

extern "C"
BrightnessConnection ChromeOSMonitorBrightness(
    BrightnessMonitorFunction monitor_function, void* object) {
  BrightnessConnection connection =
      new OpaqueBrightnessConnection(monitor_function, object);

  // Snoop on D-Bus messages so we can get notified about brightness changes.
  DBusConnection* dbus_conn =
      dbus_g_connection_get_connection(
          dbus::GetSystemBusConnection().g_connection());
  DCHECK(dbus_conn);

  DBusError error;
  dbus_error_init(&error);
  dbus_bus_add_match(dbus_conn, GetDBusMatchString().c_str(), &error);
  if (dbus_error_is_set(&error)) {
    LOG(DFATAL) << "Got error while adding D-Bus match rule: " << error.name
                << " (" << error.message << ")";
  }

  if (!dbus_connection_add_filter(
           dbus_conn,
           &OpaqueBrightnessConnection::FilterDBusMessage,
           connection,  // user_data
           NULL)) {     // free_data_function
    LOG(DFATAL) << "Unable to add D-Bus filter";
  }

  return connection;
}

extern "C"
void ChromeOSDisconnectBrightness(BrightnessConnection connection) {
  DCHECK(connection);

  DBusConnection* dbus_conn =
      dbus_g_connection_get_connection(
          dbus::GetSystemBusConnection().g_connection());
  DCHECK(dbus_conn);

  dbus_connection_remove_filter(
      dbus_conn,
      &OpaqueBrightnessConnection::FilterDBusMessage,
      connection);
  delete connection;

  DBusError error;
  dbus_error_init(&error);
  dbus_bus_remove_match(dbus_conn, GetDBusMatchString().c_str(), &error);
  if (dbus_error_is_set(&error)) {
    LOG(DFATAL) << "Got error while removing D-Bus match rule: " << error.name
                << " (" << error.message << ")";
  }
}

}  // namespace chromeos
