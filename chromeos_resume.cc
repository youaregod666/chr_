// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_resume.h"

#include <string>

#include <dbus/dbus-glib-lowlevel.h>

#include "base/logging.h"
#include "chromeos/dbus/dbus.h"
#include "chromeos/dbus/service_constants.h"

using std::string;

namespace chromeos {

class OpaqueResumeConnection : public dbus::SignalWatcher {
 public:
  OpaqueResumeConnection(ResumeMonitor callback, void* context)
     : callback_(callback),
       context_(context) {
    StartMonitoring(power_manager::kPowerManagerInterface,
                    power_manager::kPowerStateChangedSignal);
  }

  virtual void OnSignal(DBusMessage* message) {
    DBusError error;
    dbus_error_init(&error);
    const char* arg = NULL;
    if (dbus_message_get_args(message, &error,
                              DBUS_TYPE_STRING, &arg,
                              DBUS_TYPE_INVALID)) {
      if (!strcmp(arg, "on")) {
        LOG(INFO) << "Resume signal received";
        callback_(context_);
      }
    } else {
      LOG(WARNING) << "Unable to read argument from "
                   << power_manager::kPowerStateChangedSignal << " signal";
    }
  }

 private:
  // Callback within Chrome that we invoke when the system resumes.
  ResumeMonitor callback_;

  // Opaque pointer supplied to the callback.
  void* context_;
};

extern "C"
ResumeConnection ChromeOSMonitorResume(ResumeMonitor monitor, void* context) {
  return new OpaqueResumeConnection(monitor, context);
}

extern "C"
void ChromeOSDisconnectResume(ResumeConnection connection) {
  DCHECK(connection);
  delete connection;
}

}  // namespace chromeos
