// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_cryptohome.h"  // NOLINT

#include <base/basictypes.h>
#include <base/string_util.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/dbus/service_constants.h>
#include <chromeos/glib/object.h>

#include "marshal.glibmarshal.h"  // NOLINT

namespace chromeos {  // NOLINT

extern "C"
bool ChromeOSCryptohomeCheckKey(const char* user_email, const char* key) {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy proxy(bus,
                    cryptohome::kCryptohomeServiceName,
                    cryptohome::kCryptohomeServicePath,
                    cryptohome::kCryptohomeInterface);
  gboolean done = false;
  glib::ScopedError error;

  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           cryptohome::kCryptohomeCheckKey,
                           &Resetter(&error).lvalue(),
                           G_TYPE_STRING,
                           user_email,
                           G_TYPE_STRING,
                           key,
                           G_TYPE_INVALID,
                           G_TYPE_BOOLEAN,
                           &done,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << cryptohome::kCryptohomeCheckKey << " failed: "
                 << (error->message ? error->message : "Unknown Error.");
  }
  return done;
}

extern "C"
int ChromeOSCryptohomeAsyncCheckKey(const char* user_email, const char* key) {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy proxy(bus,
                    cryptohome::kCryptohomeServiceName,
                    cryptohome::kCryptohomeServicePath,
                    cryptohome::kCryptohomeInterface);
  gint async_call_id = 0;
  glib::ScopedError error;

  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           cryptohome::kCryptohomeAsyncCheckKey,
                           &Resetter(&error).lvalue(),
                           G_TYPE_STRING,
                           user_email,
                           G_TYPE_STRING,
                           key,
                           G_TYPE_INVALID,
                           G_TYPE_INT,
                           &async_call_id,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << cryptohome::kCryptohomeAsyncCheckKey << " failed: "
                 << (error->message ? error->message : "Unknown Error.");
  }
  return async_call_id;
}

extern "C"
bool ChromeOSCryptohomeMigrateKey(const char* user_email, const char* from_key,
                                  const char* to_key) {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy proxy(bus,
                    cryptohome::kCryptohomeServiceName,
                    cryptohome::kCryptohomeServicePath,
                    cryptohome::kCryptohomeInterface);
  gboolean done = false;
  glib::ScopedError error;

  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           cryptohome::kCryptohomeMigrateKey,
                           &Resetter(&error).lvalue(),
                           G_TYPE_STRING,
                           user_email,
                           G_TYPE_STRING,
                           from_key,
                           G_TYPE_STRING,
                           to_key,
                           G_TYPE_INVALID,
                           G_TYPE_BOOLEAN,
                           &done,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << cryptohome::kCryptohomeMigrateKey << " failed: "
                 << (error->message ? error->message : "Unknown Error.");
  }
  return done;
}

extern "C"
int ChromeOSCryptohomeAsyncMigrateKey(const char* user_email,
                                      const char* from_key,
                                      const char* to_key) {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy proxy(bus,
                    cryptohome::kCryptohomeServiceName,
                    cryptohome::kCryptohomeServicePath,
                    cryptohome::kCryptohomeInterface);
  gint async_call_id = 0;
  glib::ScopedError error;

  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           cryptohome::kCryptohomeAsyncMigrateKey,
                           &Resetter(&error).lvalue(),
                           G_TYPE_STRING,
                           user_email,
                           G_TYPE_STRING,
                           from_key,
                           G_TYPE_STRING,
                           to_key,
                           G_TYPE_INVALID,
                           G_TYPE_INT,
                           &async_call_id,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << cryptohome::kCryptohomeAsyncMigrateKey << " failed: "
                 << (error->message ? error->message : "Unknown Error.");
  }
  return async_call_id;
}

extern "C"
bool ChromeOSCryptohomeRemove(const char* user_email) {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy proxy(bus,
                    cryptohome::kCryptohomeServiceName,
                    cryptohome::kCryptohomeServicePath,
                    cryptohome::kCryptohomeInterface);
  gboolean done = false;
  glib::ScopedError error;

  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           cryptohome::kCryptohomeRemove,
                           &Resetter(&error).lvalue(),
                           G_TYPE_STRING,
                           user_email,
                           G_TYPE_INVALID,
                           G_TYPE_BOOLEAN,
                           &done,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << cryptohome::kCryptohomeRemove << " failed: "
                 << (error->message ? error->message : "Unknown Error.");
  }
  return done;
}

extern "C"
int ChromeOSCryptohomeAsyncRemove(const char* user_email) {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy proxy(bus,
                    cryptohome::kCryptohomeServiceName,
                    cryptohome::kCryptohomeServicePath,
                    cryptohome::kCryptohomeInterface);
  gint async_call_id = 0;
  glib::ScopedError error;

  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           cryptohome::kCryptohomeAsyncRemove,
                           &Resetter(&error).lvalue(),
                           G_TYPE_STRING,
                           user_email,
                           G_TYPE_INVALID,
                           G_TYPE_INT,
                           &async_call_id,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << cryptohome::kCryptohomeAsyncRemove << " failed: "
                 << (error->message ? error->message : "Unknown Error.");
  }
  return async_call_id;
}

extern "C"
CryptohomeBlob ChromeOSCryptohomeGetSystemSalt() {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy proxy(bus,
                    cryptohome::kCryptohomeServiceName,
                    cryptohome::kCryptohomeServicePath,
                    cryptohome::kCryptohomeInterface);
  GArray* salt;
  glib::ScopedError error;

  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           cryptohome::kCryptohomeGetSystemSalt,
                           &Resetter(&error).lvalue(),
                           G_TYPE_INVALID,
                           DBUS_TYPE_G_UCHAR_ARRAY,
                           &salt,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << cryptohome::kCryptohomeGetSystemSalt << " failed: "
                 << (error->message ? error->message : "Unknown Error.");
    return CryptohomeBlob();
  }
  CryptohomeBlob system_salt;
  system_salt.resize(salt->len);
  if (system_salt.size() == salt->len) {
    memcpy(&system_salt[0], static_cast<const void*>(salt->data), salt->len);
  } else {
    system_salt.clear();
  }
  g_array_free(salt, false);
  return system_salt;
}

extern "C"
bool ChromeOSCryptohomeIsMounted() {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy proxy(bus,
                    cryptohome::kCryptohomeServiceName,
                    cryptohome::kCryptohomeServicePath,
                    cryptohome::kCryptohomeInterface);
  gboolean done = false;
  glib::ScopedError error;

  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           cryptohome::kCryptohomeIsMounted,
                           &Resetter(&error).lvalue(),
                           G_TYPE_INVALID,
                           G_TYPE_BOOLEAN,
                           &done,
                           G_TYPE_INVALID)) {

    LOG(WARNING) << cryptohome::kCryptohomeIsMounted << " failed: "
                 << (error->message ? error->message : "Unknown Error.");

  }
  return done;
}

gchar** ChromeOSCryptohomeCopyStringArray(
    const std::vector<std::string>& input_array) {
  gchar** return_array = g_new(gchar*, input_array.size() + 1);
  if (!return_array) {
    return NULL;
  }
  int i = 0;
  for (std::vector<std::string>::const_iterator itr = input_array.begin();
       itr != input_array.end();
       itr++, i++) {
    return_array[i] = g_strdup((*itr).c_str());
    if (!return_array[i]) {
      // return_array is automatically NULL-terminated in this case
      g_strfreev(return_array);
      return NULL;
    }
  }
  return_array[i] = NULL;
  return return_array;
}

extern "C"
bool ChromeOSCryptohomeMount(
    const char* user_email,
    const char* key,
    bool create_if_missing,
    bool replace_tracked_subdirectories,
    const std::vector<std::string>& tracked_subdirectories,
    int* mount_error) {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy proxy(bus,
                    cryptohome::kCryptohomeServiceName,
                    cryptohome::kCryptohomeServicePath,
                    cryptohome::kCryptohomeInterface);
  gint local_mount_error = 0;
  gboolean done = false;
  glib::ScopedError error;

  // Make a full copy of the string array as we don't own
  // tracked_subdirectories, and c_str() is only guaranteed to be unchanged as
  // long as only non-const string functions are called.
  char** dbus_tracked_subdirectories =
      ChromeOSCryptohomeCopyStringArray(tracked_subdirectories);
  if (dbus_tracked_subdirectories == NULL) {
    return false;
  }

  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           cryptohome::kCryptohomeMount,
                           &Resetter(&error).lvalue(),
                           G_TYPE_STRING,
                           user_email,
                           G_TYPE_STRING,
                           key,
                           G_TYPE_BOOLEAN,
                           create_if_missing,
                           G_TYPE_BOOLEAN,
                           replace_tracked_subdirectories,
                           G_TYPE_STRV,
                           dbus_tracked_subdirectories,
                           G_TYPE_INVALID,
                           G_TYPE_INT,
                           &local_mount_error,
                           G_TYPE_BOOLEAN,
                           &done,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << cryptohome::kCryptohomeMount << " failed: "
                 << (error->message ? error->message : "Unknown Error.");
  }

  g_strfreev(dbus_tracked_subdirectories);

  if (mount_error) {
    *mount_error = local_mount_error;
  }
  return done;
}

extern "C"
int ChromeOSCryptohomeAsyncMount(
    const char* user_email,
    const char* key,
    bool create_if_missing,
    bool replace_tracked_subdirectories,
    const std::vector<std::string>& tracked_subdirectories) {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy proxy(bus,
                    cryptohome::kCryptohomeServiceName,
                    cryptohome::kCryptohomeServicePath,
                    cryptohome::kCryptohomeInterface);
  gint async_call_id = 0;
  glib::ScopedError error;

  char** dbus_tracked_subdirectories =
      ChromeOSCryptohomeCopyStringArray(tracked_subdirectories);
  if (dbus_tracked_subdirectories == NULL) {
    return 0;
  }

  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           cryptohome::kCryptohomeAsyncMount,
                           &Resetter(&error).lvalue(),
                           G_TYPE_STRING,
                           user_email,
                           G_TYPE_STRING,
                           key,
                           G_TYPE_BOOLEAN,
                           create_if_missing,
                           G_TYPE_BOOLEAN,
                           replace_tracked_subdirectories,
                           G_TYPE_STRV,
                           dbus_tracked_subdirectories,
                           G_TYPE_INVALID,
                           G_TYPE_INT,
                           &async_call_id,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << cryptohome::kCryptohomeAsyncMount << " failed: "
                 << (error->message ? error->message : "Unknown Error.");
  }

  g_strfreev(dbus_tracked_subdirectories);

  return async_call_id;
}

extern "C"
bool ChromeOSCryptohomeMountAllowFail(const char* user_email,
    const char* key,
    int* mount_error) {
  return ChromeOSCryptohomeMount(user_email, key, true, false,
                                 std::vector<std::string>(), mount_error);
}

extern "C"
bool ChromeOSCryptohomeMountGuest(int* mount_error) {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy proxy(bus,
                    cryptohome::kCryptohomeServiceName,
                    cryptohome::kCryptohomeServicePath,
                    cryptohome::kCryptohomeInterface);
  gint local_mount_error = 0;
  gboolean done = false;
  glib::ScopedError error;

  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           cryptohome::kCryptohomeMountGuest,
                           &Resetter(&error).lvalue(),
                           G_TYPE_INVALID,
                           G_TYPE_INT,
                           &local_mount_error,
                           G_TYPE_BOOLEAN,
                           &done,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << cryptohome::kCryptohomeMountGuest << " failed: "
                 << (error->message ? error->message : "Unknown Error.");
  }
  if (mount_error) {
    *mount_error = local_mount_error;
  }
  return done;
}

extern "C"
int ChromeOSCryptohomeAsyncMountGuest() {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy proxy(bus,
                    cryptohome::kCryptohomeServiceName,
                    cryptohome::kCryptohomeServicePath,
                    cryptohome::kCryptohomeInterface);
  gint async_call_id = 0;
  glib::ScopedError error;

  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           cryptohome::kCryptohomeAsyncMountGuest,
                           &Resetter(&error).lvalue(),
                           G_TYPE_INVALID,
                           G_TYPE_INT,
                           &async_call_id,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << cryptohome::kCryptohomeAsyncMountGuest << " failed: "
                 << (error->message ? error->message : "Unknown Error.");
  }
  return async_call_id;
}

extern "C"
bool ChromeOSCryptohomeUnmount() {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy proxy(bus,
                    cryptohome::kCryptohomeServiceName,
                    cryptohome::kCryptohomeServicePath,
                    cryptohome::kCryptohomeInterface);
  gboolean done = false;
  glib::ScopedError error;

  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           cryptohome::kCryptohomeUnmount,
                           &Resetter(&error).lvalue(),
                           G_TYPE_INVALID,
                           G_TYPE_BOOLEAN,
                           &done,
                           G_TYPE_INVALID)) {

    LOG(WARNING) << cryptohome::kCryptohomeUnmount << " failed: "
                 << (error->message ? error->message : "Unknown Error.");

  }
  return done;
}

extern "C"
bool ChromeOSCryptohomeRemoveTrackedSubdirectories() {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy proxy(bus,
                    cryptohome::kCryptohomeServiceName,
                    cryptohome::kCryptohomeServicePath,
                    cryptohome::kCryptohomeInterface);
  gboolean done = false;
  glib::ScopedError error;

  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           cryptohome::kCryptohomeRemoveTrackedSubdirectories,
                           &Resetter(&error).lvalue(),
                           G_TYPE_INVALID,
                           G_TYPE_BOOLEAN,
                           &done,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << cryptohome::kCryptohomeRemoveTrackedSubdirectories
                 << " failed: "
                 << (error->message ? error->message : "Unknown Error.");
  }
  return done;
}

extern "C"
int ChromeOSCryptohomeAsyncRemoveTrackedSubdirectories() {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy proxy(bus,
                    cryptohome::kCryptohomeServiceName,
                    cryptohome::kCryptohomeServicePath,
                    cryptohome::kCryptohomeInterface);
  gint async_call_id = 0;
  glib::ScopedError error;

  if (!::dbus_g_proxy_call(proxy.gproxy(),
           cryptohome::kCryptohomeAsyncRemoveTrackedSubdirectories,
           &Resetter(&error).lvalue(),
           G_TYPE_INVALID,
           G_TYPE_INT,
           &async_call_id,
           G_TYPE_INVALID)) {
    LOG(WARNING) << cryptohome::kCryptohomeAsyncRemoveTrackedSubdirectories
                 << " failed: "
                 << (error->message ? error->message : "Unknown Error.");
  }
  return async_call_id;
}

extern "C"
bool ChromeOSCryptohomeTpmIsReady() {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy proxy(bus,
                    cryptohome::kCryptohomeServiceName,
                    cryptohome::kCryptohomeServicePath,
                    cryptohome::kCryptohomeInterface);
  gboolean done = false;
  glib::ScopedError error;

  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           cryptohome::kCryptohomeTpmIsReady,
                           &Resetter(&error).lvalue(),
                           G_TYPE_INVALID,
                           G_TYPE_BOOLEAN,
                           &done,
                           G_TYPE_INVALID)) {

    LOG(WARNING) << cryptohome::kCryptohomeTpmIsReady << " failed: "
                 << (error->message ? error->message : "Unknown Error.");

  }
  return done;
}

extern "C"
bool ChromeOSCryptohomeTpmIsEnabled() {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy proxy(bus,
                    cryptohome::kCryptohomeServiceName,
                    cryptohome::kCryptohomeServicePath,
                    cryptohome::kCryptohomeInterface);
  gboolean done = false;
  glib::ScopedError error;

  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           cryptohome::kCryptohomeTpmIsEnabled,
                           &Resetter(&error).lvalue(),
                           G_TYPE_INVALID,
                           G_TYPE_BOOLEAN,
                           &done,
                           G_TYPE_INVALID)) {

    LOG(WARNING) << cryptohome::kCryptohomeTpmIsEnabled << " failed: "
                 << (error->message ? error->message : "Unknown Error.");

  }
  return done;
}

extern "C"
bool ChromeOSCryptohomeTpmIsOwned() {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy proxy(bus,
                    cryptohome::kCryptohomeServiceName,
                    cryptohome::kCryptohomeServicePath,
                    cryptohome::kCryptohomeInterface);
  gboolean done = false;
  glib::ScopedError error;

  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           cryptohome::kCryptohomeTpmIsOwned,
                           &Resetter(&error).lvalue(),
                           G_TYPE_INVALID,
                           G_TYPE_BOOLEAN,
                           &done,
                           G_TYPE_INVALID)) {

    LOG(WARNING) << cryptohome::kCryptohomeTpmIsOwned << " failed: "
                 << (error->message ? error->message : "Unknown Error.");

  }
  return done;
}

extern "C"
bool ChromeOSCryptohomeTpmIsBeingOwned() {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy proxy(bus,
                    cryptohome::kCryptohomeServiceName,
                    cryptohome::kCryptohomeServicePath,
                    cryptohome::kCryptohomeInterface);
  gboolean done = false;
  glib::ScopedError error;

  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           cryptohome::kCryptohomeTpmIsBeingOwned,
                           &Resetter(&error).lvalue(),
                           G_TYPE_INVALID,
                           G_TYPE_BOOLEAN,
                           &done,
                           G_TYPE_INVALID)) {

    LOG(WARNING) << cryptohome::kCryptohomeTpmIsBeingOwned << " failed: "
                 << (error->message ? error->message : "Unknown Error.");

  }
  return done;
}

extern "C"
bool ChromeOSCryptohomeTpmGetPassword(std::string* password) {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy proxy(bus,
                    cryptohome::kCryptohomeServiceName,
                    cryptohome::kCryptohomeServicePath,
                    cryptohome::kCryptohomeInterface);
  gchar* local_password = NULL;
  glib::ScopedError error;

  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           cryptohome::kCryptohomeTpmGetPassword,
                           &Resetter(&error).lvalue(),
                           G_TYPE_INVALID,
                           G_TYPE_STRING,
                           &local_password,
                           G_TYPE_INVALID)) {

    LOG(WARNING) << cryptohome::kCryptohomeTpmGetPassword << " failed: "
                 << (error->message ? error->message : "Unknown Error.");

  }

  if (local_password) {
    password->assign(local_password);
    g_free(local_password);
    return true;
  }
  return false;
}

extern "C"
bool ChromeOSCryptohomeGetStatusString(std::string* status) {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy proxy(bus,
                    cryptohome::kCryptohomeServiceName,
                    cryptohome::kCryptohomeServicePath,
                    cryptohome::kCryptohomeInterface);
  gchar* local_status = NULL;
  glib::ScopedError error;

  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           cryptohome::kCryptohomeGetStatusString,
                           &Resetter(&error).lvalue(),
                           G_TYPE_INVALID,
                           G_TYPE_STRING,
                           &local_status,
                           G_TYPE_INVALID)) {

    LOG(WARNING) << cryptohome::kCryptohomeGetStatusString << " failed: "
                 << (error->message ? error->message : "Unknown Error.");

  }

  if (local_status) {
    status->assign(local_status);
    g_free(local_status);
    return true;
  }
  return false;
}

class CryptohomeSessionConnection {
 public:
  CryptohomeSessionConnection(const CryptohomeSignalCallback& monitor,
                              void* monitor_context)
      : monitor_(monitor),
        monitor_context_(monitor_context) {
  }

  virtual ~CryptohomeSessionConnection() {}

  void Notify(const CryptohomeAsyncCallStatus& call_status) {
    monitor_(call_status, monitor_context_);
  }

 private:
  CryptohomeSignalCallback monitor_;
  void* monitor_context_;

  DISALLOW_COPY_AND_ASSIGN(CryptohomeSessionConnection);
};


#ifndef SAFE_MESSAGE
#define SAFE_MESSAGE(e) (e.message ? e.message : "unknown error")
#endif  // SAFE_MESSAGE

bool CryptohomeExtractAsyncStatus(DBusMessage* message,
                                  CryptohomeAsyncCallStatus* call_status) {
  DBusError error;
  dbus_error_init (&error);
  bool unpack = dbus_message_get_args(message,
                                      &error,
                                      DBUS_TYPE_INT32,
                                      &(call_status->async_id),
                                      DBUS_TYPE_BOOLEAN,
                                      &(call_status->return_status),
                                      DBUS_TYPE_INT32,
                                      &(call_status->return_code),
                                      DBUS_TYPE_INVALID);
  if (!unpack) {
    LOG(INFO) << "Couldn't get arg: " << SAFE_MESSAGE(error);
    return false;
  }
  return true;
}

// A message filter to receive signals.
DBusHandlerResult CryptohomeSignalFilter(DBusConnection* dbus_connection,
                         DBusMessage* message,
                         void* connection) {
  CryptohomeSessionConnection* self =
      static_cast<CryptohomeSessionConnection*>(connection);
  if (dbus_message_is_signal(message, cryptohome::kCryptohomeInterface,
                             cryptohome::kSignalAsyncCallStatus)) {
    LOG(INFO) << "Filter:: AsyncCallStatus signal received";
    CryptohomeAsyncCallStatus call_status;
    if (CryptohomeExtractAsyncStatus(message, &call_status)) {
      self->Notify(call_status);
      return DBUS_HANDLER_RESULT_HANDLED;
    }
  }
  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

extern "C"
void* ChromeOSCryptohomeMonitorSession(CryptohomeSignalCallback monitor,
                                       void* monitor_context) {
  const std::string filter = StringPrintf("type='signal', interface='%s'",
                                          cryptohome::kCryptohomeInterface);

  DBusError error;
  ::dbus_error_init(&error);
  DBusConnection* dbus_connection = ::dbus_g_connection_get_connection(
      dbus::GetSystemBusConnection().g_connection());
  CHECK(dbus_connection);
  ::dbus_bus_add_match(dbus_connection, filter.c_str(), &error);
  if (::dbus_error_is_set(&error)) {
    LOG(WARNING) << "Failed to add a filter:" << error.name << ", message="
                 << SAFE_MESSAGE(error);
    return NULL;
  }

  CryptohomeSessionConnection* connection = new CryptohomeSessionConnection(
      monitor,
      monitor_context);
  CHECK(dbus_connection_add_filter(dbus_connection,
                                   &CryptohomeSignalFilter,
                                   connection,
                                   NULL));

  LOG(INFO) << "Cryptohome API event monitoring started";
  return connection;
}

extern "C"
void ChromeOSCryptohomeDisconnectSession(void* connection) {
  CryptohomeSessionConnection* self =
      static_cast<CryptohomeSessionConnection*>(connection);
  DBusConnection *dbus_connection = ::dbus_g_connection_get_connection(
      dbus::GetSystemBusConnection().g_connection());
  ::dbus_connection_remove_filter(dbus_connection,
                                  &CryptohomeSignalFilter,
                                  connection);
  delete self;
  LOG(INFO) << "Disconnected from Cryptohome event monitoring";
}

}  // namespace chromeos
