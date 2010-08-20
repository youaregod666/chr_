// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_login.h"  // NOLINT

#include <vector>

#include <base/basictypes.h>
#include <base/string_util.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/dbus/service_constants.h>
#include <chromeos/glib/object.h>

#include "marshal.glibmarshal.h"  // NOLINT

namespace chromeos {  // NOLINT

namespace {
chromeos::dbus::Proxy CreateProxy() {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  return chromeos::dbus::Proxy(bus,
                               login_manager::kSessionManagerServiceName,
                               login_manager::kSessionManagerServicePath,
                               login_manager::kSessionManagerInterface);
}
}  // Anonymous namespace.

class OpaqueSessionConnection {
 public:
  OpaqueSessionConnection(const SessionMonitor& monitor, void* object)
      : monitor_(monitor),
        object_(object) {
  }

  virtual ~OpaqueSessionConnection() {}

  void Notify(OwnershipEvent event) {
    monitor_(object_, event);
  }

 private:
  SessionMonitor monitor_;
  void* object_;

  DISALLOW_COPY_AND_ASSIGN(OpaqueSessionConnection);
};

extern "C"
bool ChromeOSEmitLoginPromptReady() {
  chromeos::dbus::Proxy proxy = CreateProxy();
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
bool ChromeOSSetOwnerKey(const std::vector<uint8>& public_key_der) {
  chromeos::dbus::Proxy proxy = CreateProxy();
  chromeos::glib::ScopedError error;

  GArray* key_der = g_array_sized_new(FALSE, FALSE, 1, public_key_der.size());
  g_array_append_vals(key_der, &public_key_der[0], public_key_der.size());

  bool rv = true;
  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           login_manager::kSessionManagerSetOwnerKey,
                           &Resetter(&error).lvalue(),
                           DBUS_TYPE_G_UCHAR_ARRAY,
                           key_der,
                           G_TYPE_INVALID,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << login_manager::kSessionManagerSetOwnerKey << " failed: "
                 << (error->message ? error->message : "Unknown Error.");
    rv = false;
  }
  g_array_free(key_der, TRUE);
  return rv;
}

extern "C"
bool ChromeOSStartSession(const char* user_email,
                          const char* unique_id /* unused */) {
  chromeos::dbus::Proxy proxy = CreateProxy();
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
  chromeos::dbus::Proxy proxy = CreateProxy();
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
  chromeos::dbus::Proxy proxy = CreateProxy();
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



#define SAFE_MESSAGE(e) (e.message ? e.message : "unknown error")

namespace {
bool IsSuccess(DBusMessage* message) {
  char* out_string = NULL;
  DBusError error;
  dbus_error_init (&error);
  bool unpack = dbus_message_get_args(message,
                                      &error,
                                      DBUS_TYPE_STRING,
                                      &out_string,
                                      DBUS_TYPE_INVALID);
  if (!unpack) {
    LOG(INFO) << "Couldn't get arg: " << SAFE_MESSAGE(error);
    return false;
  }
  return strncmp("success", out_string, strlen("success")) == 0;
}

// A message filter to receive signals.
DBusHandlerResult Filter(DBusConnection* connection,
                         DBusMessage* message,
                         void* object) {
  SessionConnection self =
      static_cast<SessionConnection>(object);
  if (dbus_message_is_signal(message, chromium::kChromiumInterface,
                             chromium::kOwnerKeySetSignal)) {
    LOG(INFO) << "Filter:: OwnerKeySet signal received";
    self->Notify(IsSuccess(message) ? SetKeySuccess : SetKeyFailure);
    return DBUS_HANDLER_RESULT_HANDLED;
  } else if (dbus_message_is_signal(message, chromium::kChromiumInterface,
                                    chromium::kSettingChangeCompleteSignal)) {
    LOG(INFO) << "Filter:: SettingChangeComplete signal received";
    self->Notify(IsSuccess(message) ? SetKeySuccess : SetKeyFailure);
    return DBUS_HANDLER_RESULT_HANDLED;
  } else if (dbus_message_is_signal(message, chromium::kChromiumInterface,
                                    chromium::kWhitelistChangeCompleteSignal)) {
    LOG(INFO) << "Filter:: WhitelistChangeComplete signal received";
    self->Notify(IsSuccess(message) ? SetKeySuccess : SetKeyFailure);
    return DBUS_HANDLER_RESULT_HANDLED;
  } else {
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }
}

}  // namespace

extern "C"
SessionConnection
ChromeOSMonitorSession(SessionMonitor monitor, void* object) {
  const std::string filter = StringPrintf("type='signal', interface='%s'",
                                          chromium::kChromiumInterface);

  DBusError error;
  ::dbus_error_init(&error);
  DBusConnection* connection = ::dbus_g_connection_get_connection(
      dbus::GetSystemBusConnection().g_connection());
  CHECK(connection);
  ::dbus_bus_add_match(connection, filter.c_str(), &error);
  if (::dbus_error_is_set(&error)) {
    LOG(WARNING) << "Failed to add a filter:" << error.name << ", message="
                 << SAFE_MESSAGE(error);
    return NULL;
  }

  SessionConnection result = new OpaqueSessionConnection(monitor, object);
  CHECK(dbus_connection_add_filter(connection, &Filter, result, NULL));

  LOG(INFO) << "Ownership API status monitoring started";
  return result;
}

extern "C"
void ChromeOSDisconnectSession(SessionConnection connection) {
  DBusConnection *bus = ::dbus_g_connection_get_connection(
      dbus::GetSystemBusConnection().g_connection());
  ::dbus_connection_remove_filter(bus, &Filter, connection);
  delete connection;
  LOG(INFO) << "Disconnected from session manager";
}

}  // namespace chromeos
