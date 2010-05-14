// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_screen_lock.h"  // NOLINT

#include <dbus/dbus-glib-lowlevel.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/dbus/service_constants.h>
#include <chromeos/glib/object.h>

namespace chromeos {

class OpaqueScreenLockConnection {
 public:
  typedef dbus::MonitorConnection<void (void)>* ConnectionType;

  OpaqueScreenLockConnection(const ScreenLockMonitor& monitor, void* object)
      : monitor_(monitor),
        object_(object),
        connection_(NULL) {
  }

  ConnectionType& connection() {
    return connection_;
  }

  void Notify(ScreenLockState state) {
    monitor_(object_, state);
  }

 private:
  ScreenLockMonitor monitor_;
  void* object_;
  ConnectionType connection_;

  DISALLOW_COPY_AND_ASSIGN(OpaqueScreenLockConnection);
};

namespace {

#define SCREEN_LOCK_INTERFACE "org.chromium.ScreenLock"
const char* kScreenLockedSignal = "ScreenLocked";
const char* kScreenUnlockedSignal = "ScreenUnlocked";

//  static
DBusHandlerResult Filter(DBusConnection* connection,
                         DBusMessage* message,
                         void* object) {
  ScreenLockConnection self =
      static_cast<ScreenLockConnection>(object);
  if (dbus_message_is_signal(message, SCREEN_LOCK_INTERFACE,
                             kScreenLockedSignal)) {
    DLOG(INFO) << "Filter:: ScreenLocked event";
    self->Notify(Locked);
    return DBUS_HANDLER_RESULT_HANDLED;
  } else if (dbus_message_is_signal(message, SCREEN_LOCK_INTERFACE,
                                    kScreenUnlockedSignal)) {
    self->Notify(Unlocked);
  } else {
    DLOG(INFO) << "Filter:: not ScreenLocked event:";
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }
}

}  // namespace

extern "C"
void ChromeOSNotifyScreenLockCompleted() {
  NOTREACHED();
}

extern "C"
void ChromeOSNotifyScreenLockRequested() {
  NOTREACHED();
}

extern "C"
void ChromeOSNotifyScreenUnlockRequested() {
  NOTREACHED();
}

extern "C"
void ChromeOSNotifyScreenUnlocked() {
  NOTREACHED();
}

#define SAFE_MESSAGE(e) (e.message ? e.message : "unknown error")

extern "C"
ScreenLockConnection
ChromeOSMonitorScreenLock(ScreenLockMonitor monitor, void* object) {
  // TODO(oshima): Current implementation is for testing purpose only
  // and using low level dbus API so that it can be receive a signal
  // from dbus-send command line tool. This will be changed to use
  // chromeos::dbus library once PowerManager implmenets dbus
  // interface.

  DBusError error;
  dbus_error_init(&error);

  DBusConnection *connection = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
  LOG_IF(FATAL, dbus_error_is_set(&error)) <<
      "No D-Bus connection: " << SAFE_MESSAGE(error);

  dbus_connection_setup_with_g_main(connection, NULL);
  dbus_bus_add_match(connection,
                     "type='signal', interface='" SCREEN_LOCK_INTERFACE "'",
                     &error);

  ScreenLockConnection result =
      new OpaqueScreenLockConnection(monitor, object);


  dbus_connection_add_filter(connection, &Filter, result, NULL);

  return result;
}

extern "C"
void ChromeOSDisconnectScreenLock(ScreenLockConnection connection) {
  // TODO(oshima): Use chromeos::dbus library. See comment above.

  DBusError error;
  dbus_error_init(&error);
  DBusConnection *bus = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
  if (dbus_error_is_set(&error)) {
    LOG(WARNING) << "No D-Bus connection: " << SAFE_MESSAGE(error);
  } else {
    dbus_connection_remove_filter(bus, &Filter, connection);
  }
  if (connection)
    delete connection;
}

}  // namespace chromeos
