// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_power.h"

#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <vector>

#include <base/file_util.h>
#include <base/logging.h>
#include <base/string_util.h>

#include "chromeos/dbus/dbus.h"
#include <chromeos/dbus/service_constants.h>
#include "chromeos/glib/object.h"
#include "chromeos/string.h"

namespace chromeos {

struct PowerRequestCallbackData {
  PowerRequestCallbackData(GetIdleTimeCallback cb, void* obj)
      : proxy(new dbus::Proxy(dbus::GetSystemBusConnection(),
                              power_manager::kPowerManagerInterface,
                              power_manager::kPowerManagerServicePath,
                              power_manager::kPowerManagerInterface)),
        callback(cb),
        object(obj) {}
  scoped_ptr<dbus::Proxy> proxy;
  GetIdleTimeCallback callback;
  void* object;
};

// DBus will always call the Delete function passed to it by
// dbus_g_proxy_begin_call, whether DBus calls the callback or not.
void DeletePowerCallbackData(void* user_data) {
  PowerRequestCallbackData* cb_data =
      reinterpret_cast<PowerRequestCallbackData*>(user_data);
  delete cb_data;
}

namespace {  // NOLINT

bool GetPowerProperty(const dbus::Proxy& proxy,
                      const char *param_name,
                      GType data_type,
                      void *result) {
  GError* error = NULL;
  // Use a dbus call to read the requested parameter.
  if(!dbus_g_proxy_call(proxy.gproxy(), "GetProperty", &error,
                        G_TYPE_STRING, param_name,
                        G_TYPE_INVALID,
                        data_type, result,
                        G_TYPE_INVALID)) {
    LOG(WARNING) << (error->message ? error->message : "GetProperty failed.");
    return false;
  }
  return true;
}

bool RetrievePowerStatus(const dbus::Proxy& proxy, PowerStatus* status) {
#define GET_PROPERTY(proxy, object, type, property) \
    GetPowerProperty(proxy, #property, type, &(object)->property)
  if (!GET_PROPERTY(proxy, status, G_TYPE_BOOLEAN, line_power_on) ||
      !GET_PROPERTY(proxy, status, G_TYPE_DOUBLE,  battery_energy) ||
      !GET_PROPERTY(proxy, status, G_TYPE_DOUBLE,  battery_energy_rate) ||
      !GET_PROPERTY(proxy, status, G_TYPE_DOUBLE,  battery_voltage) ||
      !GET_PROPERTY(proxy, status, G_TYPE_INT64,   battery_time_to_empty) ||
      !GET_PROPERTY(proxy, status, G_TYPE_INT64,   battery_time_to_full) ||
      !GET_PROPERTY(proxy, status, G_TYPE_DOUBLE,  battery_percentage) ||
      !GET_PROPERTY(proxy, status, G_TYPE_BOOLEAN, battery_is_present) ||
      !GET_PROPERTY(proxy, status, G_TYPE_INT,     battery_state)) {
    return false;
  }
#undef GET_PROPERTY
  return true;
}

} // namespace

class OpaquePowerStatusConnection {
 public:
  typedef dbus::MonitorConnection<void (const char*)>* ConnectionType;

  OpaquePowerStatusConnection(const PowerStatus& status,
                              const dbus::Proxy& proxy,
                              const PowerMonitor& monitor,
                              void* object)
     : status_(status),
       proxy_(proxy),
       monitor_(monitor),
       object_(object),
       connection_(NULL) {
  }

  void Run() {
    if (!RetrievePowerStatus(proxy_, &status_))
      return;
    monitor_(object_, status_);
  }

  ConnectionType& connection() {
    return connection_;
  }

 private:
  PowerStatus status_;
  dbus::Proxy proxy_;
  PowerMonitor monitor_;
  void* object_;
  ConnectionType connection_;
};

namespace {  // NOLINT

DBusHandlerResult DBusMessageHandler(DBusConnection* connection,
                                     DBusMessage* message,
                                     void* data) {
  OpaquePowerStatusConnection* power_connection =
      static_cast<OpaquePowerStatusConnection*>(data);

  if (dbus_message_is_signal(message, power_manager::kPowerManagerInterface,
                             "PowerSupplyPoll")) {
    power_connection->Run();
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

}  // namespace

void GetIdleTimeNotify(DBusGProxy* gproxy,
                       DBusGProxyCall* call_id,
                       void* user_data) {
  PowerRequestCallbackData* cb_data =
      reinterpret_cast<PowerRequestCallbackData*>(
          user_data);
  DCHECK(cb_data);
  glib::ScopedError error;
  int64 time_idle_ms = 0;
  if (!::dbus_g_proxy_end_call(gproxy,
                               call_id,
                               &Resetter(&error).lvalue(),
                               G_TYPE_INT64, &time_idle_ms,
                               G_TYPE_INVALID)) {
    if (error->domain == DBUS_GERROR &&
        error->code == DBUS_GERROR_REMOTE_EXCEPTION) {
      LOG(WARNING) << "Remote DBus error";
    } else {
      LOG(WARNING) << "GetIdleTimeNotify error: "
                   << (error->message ? error->message : "Unknown Error.");
    }
    cb_data->callback(cb_data->object, 0, false);
  } else {
    cb_data->callback(cb_data->object, time_idle_ms, true);
  }
}

extern "C"
PowerStatusConnection ChromeOSMonitorPowerStatus(PowerMonitor monitor,
                                                 void* object) {

  dbus::BusConnection bus = dbus::GetSystemBusConnection();

  dbus::Proxy power_status_proxy(bus,
                                 power_manager::kPowerManagerServiceName,
                                 power_manager::kPowerManagerServicePath,
                                 power_manager::kPowerManagerInterface);
  PowerStatus status = { };

  RetrievePowerStatus(power_status_proxy, &status);
  monitor(object, status);

  PowerStatusConnection result =
      new OpaquePowerStatusConnection(status,
                                      power_status_proxy,
                                      monitor,
                                      object);

  DBusConnection* connection = dbus_g_connection_get_connection(
      bus.g_connection());
  CHECK(connection);

  DBusError error;
  dbus_error_init(&error);
  std::string match = StringPrintf("type='signal', interface='%s'",
                                   power_manager::kPowerManagerInterface);
  dbus_bus_add_match(connection, match.c_str(), &error);
  if (dbus_error_is_set(&error)) {
    LOG(DFATAL) << "Failed to add match \"" << match << "\": "
                << error.name << ", message=" << error.message;
  }

  CHECK(dbus_connection_add_filter(connection, &DBusMessageHandler, result,
                                   NULL));

  return result;
}

extern "C"
void ChromeOSGetIdleTime(GetIdleTimeCallback callback,
                         void* object) {
 PowerRequestCallbackData* cb_data =
      new PowerRequestCallbackData(callback, object);
  DBusGProxyCall* call_id =
      ::dbus_g_proxy_begin_call(cb_data->proxy->gproxy(),
                                "GetIdleTime",
                                &GetIdleTimeNotify,
                                cb_data,
                                &DeletePowerCallbackData,
                                G_TYPE_INVALID);
  if (!call_id) {
    LOG(ERROR) << "ChromeOSGetIdleTime call failed";
    callback(object, 0, false);
    delete cb_data;
  }
}

extern "C"
void ChromeOSDisconnectPowerStatus(PowerStatusConnection connection) {
  if (connection && connection->connection()) {
    dbus::Disconnect(connection->connection());
    delete connection;
  }
}

// TODO(satorux): Remove this. DEPRECATED.
extern "C"
bool ChromeOSRetrievePowerInformation(PowerInformation* info) {
  // Func has been stubbed out because some functions it calls has been removed.
  // It will be removed at a later time.
  return true;
}

extern "C"
void ChromeOSEnableScreenLock(bool enable) {
  static const char kPowerManagerConfig[] =
      "/var/lib/power_manager/lock_on_idle_suspend";

  std::string config = base::StringPrintf("%d", enable);
  file_util::WriteFile(FilePath(kPowerManagerConfig),
                       config.c_str(),
                       config.size());
}

extern "C"
void ChromeOSRequestRestart() {
  chromeos::dbus::SendSignalWithNoArgumentsToSystemBus(
      "/",
      power_manager::kPowerManagerInterface,
      power_manager::kRequestRestartSignal);
}

extern "C"
void ChromeOSRequestShutdown() {
  chromeos::dbus::SendSignalWithNoArgumentsToSystemBus(
      "/",
      power_manager::kPowerManagerInterface,
      power_manager::kRequestShutdownSignal);
}

}  // namespace chromeos
