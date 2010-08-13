// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_login.h"  // NOLINT

#include <chromeos/dbus/dbus.h>
#include <chromeos/dbus/service_constants.h>
#include <chromeos/glib/object.h>

#include "marshal.glibmarshal.h"  // NOLINT

namespace chromeos {  // NOLINT

extern "C"
bool ChromeOSEmitLoginPromptReady() {
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

extern "C"
bool ChromeOSRestartJob(int pid, const char* command_line) {
  chromeos::dbus::BusConnection bus =
      chromeos::dbus::GetSystemBusConnection();
  chromeos::dbus::Proxy proxy(bus,
                              login_manager::kSessionManagerServiceName,
                              login_manager::kSessionManagerServicePath,
                              login_manager::kSessionManagerInterface);
  gboolean done = false;
  chromeos::glib::ScopedError error;

  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           login_manager::kSessionManagerRestartJob,
                           &Resetter(&error).lvalue(),
                           G_TYPE_INT,
                           pid,
                           G_TYPE_STRING,
                           command_line,
                           G_TYPE_INVALID,
                           G_TYPE_BOOLEAN,
                           &done,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << login_manager::kSessionManagerRestartJob << " failed: "
                 << (error->message ? error->message : "Unknown Error.");
  }
  return done;
}

}  // namespace chromeos
