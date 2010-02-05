// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_cros_api.h"  // NOLINT

#include <base/logging.h>
#include <dbus/dbus.h>
#include <stdlib.h>

namespace {  // NOLINT
const char* kConsoleKitManagerInterface = "org.freedesktop.ConsoleKit.Manager";
const char* kConsoleKitManagerPath = "/org/freedesktop/ConsoleKit/Manager";
const char* kConsoleKitServiceName = "org.freedesktop.ConsoleKit";

const char* kConsoleKitOpenSession = "OpenSession";
const char* kConsoleKitEnvironmentVariable = "XDG_SESSION_COOKIE";

// \brief Mimic the functonality of ConsoleKit's ck-launch-session tool.
// Does not use the dbus-glib bindings (because we can't), so we use native
// dbus calls and data structures.
//
// TODO(cmasone): Really, this should be somewhere else -- perhaps its
// own API, called in LoadCros().
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
bool ChromeOSCrosVersionCheck(chromeos::CrosAPIVersion version) {
  CkLaunchSession();
  return chromeos::kCrosAPIMinVersion <= version
      && version <= chromeos::kCrosAPIVersion;
}
