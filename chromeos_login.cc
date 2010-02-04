// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_login.h"  // NOLINT

#include <chromeos/dbus/dbus.h>
#include <chromeos/dbus/service_constants.h>
#include <chromeos/glib/object.h>
#include <dbus/dbus.h>

#include "marshal.h"  // NOLINT

namespace chromeos {  // NOLINT

namespace { // NOLINT
const char* kConsoleKitManagerInterface = "org.freedesktop.ConsoleKit.Manager";
const char* kConsoleKitManagerPath = "/org/freedesktop/ConsoleKit/Manager";
const char* kConsoleKitServiceName = "org.freedesktop.ConsoleKit";

const char* kConsoleKitOpenSession = "OpenSession";

const char* kConsoleKitEnvironmentVariable = "XDG_SESSION_COOKIE";

// \brief Mimic the functonality of ConsoleKit's ck-launch-session tool.
// Does not use the dbus-glib bindings (because we can't), so we use native
// dbus calls and data structures.
void CkLaunchSession() {
  ::DBusError local_error;
  dbus_error_init(&local_error);
  ::DBusConnection* raw_connection =
      ::dbus_bus_get_private(DBUS_BUS_SYSTEM, &local_error);
  CHECK(raw_connection) << "Can't get private connection: "
                        << local_error.message;

  ::dbus_connection_set_exit_on_disconnect(raw_connection, FALSE);

  DBusMessage* message =
      dbus_message_new_method_call(kConsoleKitServiceName,
                                   kConsoleKitManagerPath,
                                   kConsoleKitManagerInterface,
                                   kConsoleKitOpenSession);
  CHECK(message) << "Could not create message";

  dbus_error_init(&local_error);
  DBusMessage* reply =
      dbus_connection_send_with_reply_and_block(raw_connection,
                                                message,
                                                -1,
                                                &local_error);
  CHECK(reply) << "Can't get reply to message: " << local_error.message;

  dbus_error_init(&local_error);
  char* cookie = NULL;
  CHECK(dbus_message_get_args(reply,
                              &local_error,
                              DBUS_TYPE_STRING,
                              &cookie,
                              DBUS_TYPE_INVALID)) << "Can't get cookie: "
                                                  << local_error.message;
  setenv(kConsoleKitEnvironmentVariable, cookie, 1);

  // Cleanup.
  if (reply != NULL)
    dbus_message_unref(reply);
  if (message != NULL)
    dbus_message_unref(message);
}
}  // namespace

extern "C"
bool ChromeOSEmitLoginPromptReady() {
  LOG(INFO) << "trying to launch session";
  CkLaunchSession();

  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  chromeos::dbus::Proxy proxy(bus,
                              login_manager::kSessionManagerServiceName,
                              login_manager::kSessionManagerServicePath,
                              login_manager::kSessionManagerInterface);
  gboolean done = false;
  chromeos::glib::ScopedError error;

  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           login_manager::kSessionManagerEmitLoginPromptReady,
                           &Resetter(&error).lvalue(),
                           G_TYPE_INVALID,
                           G_TYPE_BOOLEAN,
                           &done,
                           G_TYPE_INVALID)) {

    LOG(WARNING) << login_manager::kSessionManagerEmitLoginPromptReady
                 << " failed: "
                 << (error->message ? error->message : "Unknown Error.");

  }
  return done;
}

extern "C"
bool ChromeOSStartSession(const char* user_email,
                          const char* unique_id /* unused */) {
  chromeos::dbus::BusConnection bus =
      chromeos::dbus::GetSystemBusConnection();
  chromeos::dbus::Proxy proxy(bus,
                              login_manager::kSessionManagerServiceName,
                              login_manager::kSessionManagerServicePath,
                              login_manager::kSessionManagerInterface);
  gboolean done = false;
  chromeos::glib::ScopedError error;

  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           login_manager::kSessionManagerStartSession,
                           &Resetter(&error).lvalue(),
                           G_TYPE_STRING,
                           user_email,
                           G_TYPE_STRING,
                           unique_id,
                           G_TYPE_INVALID,
                           G_TYPE_BOOLEAN,
                           &done,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << login_manager::kSessionManagerStartSession << " failed: "
                 << (error->message ? error->message : "Unknown Error.");
  }
  return done;
}

extern "C"
bool ChromeOSStopSession(const char* unique_id /* unused */) {
  chromeos::dbus::BusConnection bus =
      chromeos::dbus::GetSystemBusConnection();
  chromeos::dbus::Proxy proxy(bus,
                              login_manager::kSessionManagerServiceName,
                              login_manager::kSessionManagerServicePath,
                              login_manager::kSessionManagerInterface);
  gboolean done = false;
  chromeos::glib::ScopedError error;

  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           login_manager::kSessionManagerStopSession,
                           &Resetter(&error).lvalue(),
                           G_TYPE_STRING,
                           unique_id,
                           G_TYPE_INVALID,
                           G_TYPE_BOOLEAN,
                           &done,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << login_manager::kSessionManagerStopSession << " failed: "
                 << (error->message ? error->message : "Unknown Error.");
  }
  return done;
}

}  // namespace chromeos
