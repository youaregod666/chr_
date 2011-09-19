// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_brightness.h"

#include <string>

#include <dbus/dbus-glib-lowlevel.h>

#include "base/logging.h"
#include "chromeos/dbus/dbus.h"
#include "chromeos/dbus/service_constants.h"

using std::string;

namespace chromeos {

extern "C"
void ChromeOSDecreaseScreenBrightness(bool allow_off) {
  dbus::Proxy proxy(dbus::GetSystemBusConnection(),
                    power_manager::kPowerManagerServiceName,
                    power_manager::kPowerManagerServicePath,
                    power_manager::kPowerManagerInterface);
  LOG(INFO) << "Sending call to decrease screen brightness";
  ::dbus_g_proxy_call_no_reply(proxy.gproxy(),
      power_manager::kPowerManagerDecreaseScreenBrightness,
      G_TYPE_BOOLEAN, allow_off,
      G_TYPE_INVALID, G_TYPE_INVALID);
}

extern "C"
void ChromeOSIncreaseScreenBrightness(void) {
  dbus::Proxy proxy(dbus::GetSystemBusConnection(),
                    power_manager::kPowerManagerServiceName,
                    power_manager::kPowerManagerServicePath,
                    power_manager::kPowerManagerInterface);
  LOG(INFO) << "Sending call to increase screen brightness";
  ::dbus_g_proxy_call_no_reply(proxy.gproxy(),
      power_manager::kPowerManagerIncreaseScreenBrightness,
      G_TYPE_INVALID, G_TYPE_INVALID);
}

class OpaqueBrightnessConnection : public dbus::SignalWatcher {
 public:
  OpaqueBrightnessConnection(
      const BrightnessMonitorFunctionV2& monitor_function,
      const BrightnessMonitorFunction& old_monitor_function,
      void* object)
      : monitor_function_(monitor_function),
        old_monitor_function_(old_monitor_function),
        object_(object) {
    StartMonitoring(power_manager::kPowerManagerInterface,
                    power_manager::kBrightnessChangedSignal);
  }

  virtual void OnSignal(DBusMessage* message) {
    DBusError error;
    dbus_error_init(&error);
    int brightness_level = 0;
    int user_initiated = 0;
    if (dbus_message_get_args(message, &error,
                              DBUS_TYPE_INT32, &brightness_level,
                              DBUS_TYPE_BOOLEAN, &user_initiated,
                              DBUS_TYPE_INVALID)) {
      if (monitor_function_ != NULL)
        monitor_function_(object_, brightness_level, user_initiated);
      if (old_monitor_function_ != NULL && user_initiated)
        old_monitor_function_(object_, brightness_level);
    } else {
      LOG(WARNING) << "Unable to read arguments from "
                   << power_manager::kBrightnessChangedSignal << " signal";
    }
  }

 private:
  // Function within Chrome that we invoke when the brightness changes.
  BrightnessMonitorFunctionV2 monitor_function_;
  BrightnessMonitorFunction old_monitor_function_;  // DEPRECATED

  // Opaque pointer supplied to ChromeOSMonitorBrightness() that we pass back
  // via |monitor_function_|.
  void* object_;
};

extern "C"
BrightnessConnection ChromeOSMonitorBrightnessV2(
    BrightnessMonitorFunctionV2 monitor_function, void* object) {
  return new OpaqueBrightnessConnection(monitor_function, NULL, object);
}

// TODO(satorux): Remove this. DEPRECATED.
extern "C"
BrightnessConnection ChromeOSMonitorBrightness(
    BrightnessMonitorFunction monitor_function, void* object) {
  return new OpaqueBrightnessConnection(NULL, monitor_function, object);
}

extern "C"
void ChromeOSDisconnectBrightness(BrightnessConnection connection) {
  DCHECK(connection);
  delete connection;
}

}  // namespace chromeos
