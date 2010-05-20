// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_screen_lock.h"  // NOLINT

#include <base/string_util.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/dbus/service_constants.h>
#include <chromeos/glib/object.h>

namespace chromeos {

class OpaqueScreenLockConnection {
 public:
  OpaqueScreenLockConnection(const ScreenLockMonitor& monitor, void* object)
      : monitor_(monitor),
        object_(object) {
  }

  virtual ~OpaqueScreenLockConnection() {}

  void Notify(ScreenLockEvent event) {
    monitor_(object_, event);
  }

 private:
  ScreenLockMonitor monitor_;
  void* object_;

  DISALLOW_COPY_AND_ASSIGN(OpaqueScreenLockConnection);
};

namespace {

// A utility function to send a signal to PowerManager.
void SendSignalToPowerManager(const char* signal_name) {
  chromeos::dbus::Proxy proxy(chromeos::dbus::GetSystemBusConnection(),
                              "/",
                              power_manager::kPowerManagerInterface);
  DBusMessage* signal = ::dbus_message_new_signal(
      "/",
      power_manager::kPowerManagerInterface,
      signal_name);
  DCHECK(signal);
  ::dbus_g_proxy_send(proxy.gproxy(), signal, NULL);
  ::dbus_message_unref(signal);
}

// A message filter to receive signals.
DBusHandlerResult Filter(DBusConnection* connection,
                         DBusMessage* message,
                         void* object) {
  ScreenLockConnection self =
      static_cast<ScreenLockConnection>(object);
  if (dbus_message_is_signal(message, chromium::kChromiumInterface,
                             chromium::kUnlockScreenFailedSignal)) {
    LOG(INFO) << "Filter:: UnlockScreenFailed event";
    self->Notify(UnlockScreenFailed);
    return DBUS_HANDLER_RESULT_HANDLED;
  } else if (dbus_message_is_signal(message, chromium::kChromiumInterface,
                                    chromium::kLockScreenSignal)) {
    LOG(INFO) << "Filter:: LockScreen event";
    self->Notify(LockScreen);
    return DBUS_HANDLER_RESULT_HANDLED;
  } else if (dbus_message_is_signal(message, chromium::kChromiumInterface,
                                    chromium::kUnlockScreenSignal)) {
    LOG(INFO) << "Filter:: UnlockScreen event";
    self->Notify(UnlockScreen);
    return DBUS_HANDLER_RESULT_HANDLED;
  } else {
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }
}

}  // namespace

extern "C"
void ChromeOSNotifyScreenLockCompleted() {
  SendSignalToPowerManager(power_manager::kScreenIsLockedSignal);
}

extern "C"
void ChromeOSNotifyScreenUnlockCompleted() {
  SendSignalToPowerManager(power_manager::kScreenIsUnlockedSignal);
}

extern "C"
void ChromeOSNotifyScreenLockRequested() {
  SendSignalToPowerManager(power_manager::kRequestLockScreenSignal);
}

extern "C"
void ChromeOSNotifyScreenUnlockRequested() {
  SendSignalToPowerManager(power_manager::kRequestUnlockScreenSignal);
}

extern "C"
void ChromeOSNotifyScreenUnlocked() {
  NOTREACHED();
}

#define SAFE_MESSAGE(e) (e.message ? e.message : "unknown error")

extern "C"
ScreenLockConnection
ChromeOSMonitorScreenLock(ScreenLockMonitor monitor, void* object) {
  const std::string filter = StringPrintf("type='signal', interface='%s'",
                                          chromium::kChromiumInterface);

  DBusError error;
  ::dbus_error_init(&error);
  DBusConnection* connection = ::dbus_g_connection_get_connection(
      dbus::GetSystemBusConnection().g_connection());
  ::dbus_bus_add_match(connection, filter.c_str(), &error);
  if (::dbus_error_is_set(&error)) {
    DLOG(WARNING) << "Failed to add a filter:" << error.name << ", message="
                  << SAFE_MESSAGE(error);
    return NULL;
  }

  ScreenLockConnection result = new OpaqueScreenLockConnection(monitor, object);
  CHECK(dbus_connection_add_filter(connection, &Filter, result, NULL));

  DLOG(INFO) << "Screen Lock monitoring started";
  return result;
}

extern "C"
void ChromeOSDisconnectScreenLock(ScreenLockConnection connection) {
  DBusConnection *bus = ::dbus_g_connection_get_connection(
      dbus::GetSystemBusConnection().g_connection());
  ::dbus_connection_remove_filter(bus, &Filter, connection);
  delete connection;
}

}  // namespace chromeos
