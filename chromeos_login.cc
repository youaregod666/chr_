// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_login.h"  // NOLINT

#include <vector>

#include <base/basictypes.h>
#include <base/logging.h>
#include <base/string_util.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/dbus/service_constants.h>
#include <chromeos/glib/object.h>

#include "marshal.glibmarshal.h"  // NOLINT

namespace chromeos {  // NOLINT


#define SCOPED_SAFE_MESSAGE(e) (e->message ? e->message : "unknown error")

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
bool ChromeOSCheckWhitelist(const char* email,
                            std::vector<uint8>* signature) {
  DCHECK(signature);
  chromeos::dbus::Proxy proxy = CreateProxy();
  chromeos::glib::ScopedError error;

  GArray* sig;

  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           login_manager::kSessionManagerCheckWhitelist,
                           &Resetter(&error).lvalue(),
                           G_TYPE_STRING, email,
                           G_TYPE_INVALID,
                           DBUS_TYPE_G_UCHAR_ARRAY, &sig,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << login_manager::kSessionManagerCheckWhitelist << " failed: "
                 << SCOPED_SAFE_MESSAGE(error);
    return false;
  }
  bool rv = false;
  signature->resize(sig->len);
  if (signature->size() == sig->len) {
    memcpy(&(signature->at(0)), static_cast<const void*>(sig->data), sig->len);
    rv = true;
  }
  g_array_free(sig, false);
  return rv;
}

extern "C"
bool ChromeOSEmitLoginPromptReady() {
  chromeos::dbus::Proxy proxy = CreateProxy();
  gboolean done = false;
  chromeos::glib::ScopedError error;

  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           login_manager::kSessionManagerEmitLoginPromptReady,
                           &Resetter(&error).lvalue(),
                           G_TYPE_INVALID,
                           G_TYPE_BOOLEAN, &done,
                           G_TYPE_INVALID)) {

    LOG(WARNING) << login_manager::kSessionManagerEmitLoginPromptReady
                 << " failed: " << SCOPED_SAFE_MESSAGE(error);
  }
  return done;
}

extern "C"
bool ChromeOSEnumerateWhitelisted(std::vector<std::string>* out_whitelisted) {
  chromeos::dbus::Proxy proxy = CreateProxy();
  gchar** whitelisted = NULL;
  chromeos::glib::ScopedError error;

  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           login_manager::kSessionManagerEnumerateWhitelisted,
                           &Resetter(&error).lvalue(),
                           G_TYPE_INVALID,
                           G_TYPE_STRV, &whitelisted,
                           G_TYPE_INVALID)) {

    LOG(WARNING) << login_manager::kSessionManagerEnumerateWhitelisted
                 << " failed: " << SCOPED_SAFE_MESSAGE(error);
    return false;
  }
  for (int i = 0; whitelisted[i] != NULL; ++i)
    out_whitelisted->push_back(std::string(whitelisted[i]));

  g_strfreev(whitelisted);
  return true;
}

extern "C"
bool ChromeOSRetrieveProperty(const char* name,
                              std::string* out_value,
                              std::vector<uint8>* signature) {
  DCHECK(signature);
  chromeos::dbus::Proxy proxy = CreateProxy();
  chromeos::glib::ScopedError error;

  GArray* sig;
  gchar* value = NULL;

  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           login_manager::kSessionManagerRetrieveProperty,
                           &Resetter(&error).lvalue(),
                           G_TYPE_STRING, name,
                           G_TYPE_INVALID,
                           G_TYPE_STRING, &value,
                           DBUS_TYPE_G_UCHAR_ARRAY, &sig,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << login_manager::kSessionManagerRetrieveProperty
                 << " failed: " << SCOPED_SAFE_MESSAGE(error);
    return false;
  }
  bool rv = false;
  signature->resize(sig->len);
  if (signature->size() == sig->len) {
    memcpy(&(signature->at(0)), static_cast<const void*>(sig->data), sig->len);
    rv = true;
  }
  g_array_free(sig, false);
  out_value->assign(value);
  g_free(value);
  return rv;
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
                           DBUS_TYPE_G_UCHAR_ARRAY, key_der,
                           G_TYPE_INVALID,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << login_manager::kSessionManagerSetOwnerKey << " failed: "
                 << SCOPED_SAFE_MESSAGE(error);
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
                           G_TYPE_STRING, user_email,
                           G_TYPE_STRING, unique_id,
                           G_TYPE_INVALID,
                           G_TYPE_BOOLEAN, &done,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << login_manager::kSessionManagerStartSession << " failed: "
                 << SCOPED_SAFE_MESSAGE(error);
  }
  return done;
}

extern "C"
bool ChromeOSStopSession(const char* unique_id /* unused */) {
  chromeos::dbus::Proxy proxy = CreateProxy();
  // TODO(cmasone): clear up the now-defunct variables here.
  gboolean unused = false;
  ::dbus_g_proxy_call_no_reply(proxy.gproxy(),
                               login_manager::kSessionManagerStopSession,
                               G_TYPE_STRING, unique_id,
                               G_TYPE_INVALID,
                               G_TYPE_BOOLEAN, &unused,
                               G_TYPE_INVALID);
  return true;
}

extern "C"
bool ChromeOSRestartJob(int pid, const char* command_line) {
  chromeos::dbus::Proxy proxy = CreateProxy();
  gboolean done = false;
  chromeos::glib::ScopedError error;

  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           login_manager::kSessionManagerRestartJob,
                           &Resetter(&error).lvalue(),
                           G_TYPE_INT, pid,
                           G_TYPE_STRING, command_line,
                           G_TYPE_INVALID,
                           G_TYPE_BOOLEAN, &done,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << login_manager::kSessionManagerRestartJob << " failed: "
                 << SCOPED_SAFE_MESSAGE(error);
  }
  return done;
}

extern "C"
bool ChromeOSStoreProperty(const char* name,
                           const char* value,
                           const std::vector<uint8>& signature) {
  chromeos::dbus::Proxy proxy = CreateProxy();
  chromeos::glib::ScopedError error;

  GArray* sig = g_array_sized_new(FALSE, FALSE, 1, signature.size());
  g_array_append_vals(sig, &signature[0], signature.size());

  bool rv = true;
  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           login_manager::kSessionManagerStoreProperty,
                           &Resetter(&error).lvalue(),
                           G_TYPE_STRING, name,
                           G_TYPE_STRING, value,
                           DBUS_TYPE_G_UCHAR_ARRAY, sig,
                           G_TYPE_INVALID,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << login_manager::kSessionManagerStoreProperty << " failed: "
                 << SCOPED_SAFE_MESSAGE(error);
    rv = false;
  }
  g_array_free(sig, TRUE);
  return rv;
}

namespace {
bool WhitelistOpHelper(const char* op,
                       const char* email,
                       const std::vector<uint8>& signature) {
  chromeos::dbus::Proxy proxy = CreateProxy();
  chromeos::glib::ScopedError error;

  GArray* sig = g_array_sized_new(FALSE, FALSE, 1, signature.size());
  g_array_append_vals(sig, &signature[0], signature.size());

  bool rv = true;
  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           op,
                           &Resetter(&error).lvalue(),
                           G_TYPE_STRING, email,
                           DBUS_TYPE_G_UCHAR_ARRAY, sig,
                           G_TYPE_INVALID,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << op << " failed: " << SCOPED_SAFE_MESSAGE(error);
    rv = false;
  }
  g_array_free(sig, TRUE);
  return rv;
}
}  // anonymous namespace

extern "C"
bool ChromeOSUnwhitelist(const char* email,
                         const std::vector<uint8>& signature) {
  return WhitelistOpHelper(login_manager::kSessionManagerUnwhitelist,
                           email,
                           signature);
}

extern "C"
bool ChromeOSWhitelist(const char* email,
                       const std::vector<uint8>& signature) {
  return WhitelistOpHelper(login_manager::kSessionManagerWhitelist,
                           email,
                           signature);
}

namespace {
#define SAFE_MESSAGE(e) (e.message ? e.message : "unknown error")

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
                                    chromium::kPropertyChangeCompleteSignal)) {
    LOG(INFO) << "Filter:: PropertyChangeComplete signal received";
    self->Notify(IsSuccess(message) ? PropertyOpSuccess : PropertyOpFailure);
    return DBUS_HANDLER_RESULT_HANDLED;
  } else if (dbus_message_is_signal(message, chromium::kChromiumInterface,
                                    chromium::kWhitelistChangeCompleteSignal)) {
    LOG(INFO) << "Filter:: WhitelistChangeComplete signal received";
    self->Notify(IsSuccess(message) ? WhitelistOpSuccess : WhitelistOpFailure);
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
