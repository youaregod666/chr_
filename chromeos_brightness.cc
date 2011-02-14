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

class OpaqueBrightnessConnection : public dbus::SignalWatcher {
 public:
  OpaqueBrightnessConnection(const BrightnessMonitorFunction& monitor_function,
                             void* object)
     : monitor_function_(monitor_function),
       object_(object) {
    StartMonitoring(power_manager::kPowerManagerInterface,
                    power_manager::kBrightnessChangedSignal);
  }

  virtual void OnSignal(DBusMessage* message) {
    DBusError error;
    dbus_error_init(&error);
    int brightness_level = 0;
    if (dbus_message_get_args(message, &error,
                              DBUS_TYPE_INT32, &brightness_level,
                              DBUS_TYPE_INVALID)) {
      monitor_function_(object_, brightness_level);
    } else {
      LOG(WARNING) << "Unable to read brightness level from "
                   << power_manager::kBrightnessChangedSignal << " signal";
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
  return new OpaqueBrightnessConnection(monitor_function, object);
}

extern "C"
void ChromeOSDisconnectBrightness(BrightnessConnection connection) {
  DCHECK(connection);
  delete connection;
}

}  // namespace chromeos
