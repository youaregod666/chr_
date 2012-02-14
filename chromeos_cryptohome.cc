// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/string_util.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/dbus/service_constants.h>
#include <chromeos/glib/object.h>

namespace chromeos {  // NOLINT

struct CryptohomeAsyncCallStatus {
  int async_id;
  bool return_status;
  int return_code;
};

typedef void(*CryptohomeSignalCallback)(
    const CryptohomeAsyncCallStatus& call_status, void* callback_context);

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
bool ChromeOSCryptohomeGetSystemSaltSafe(char** salt, int* length) {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy proxy(bus,
                    cryptohome::kCryptohomeServiceName,
                    cryptohome::kCryptohomeServicePath,
                    cryptohome::kCryptohomeInterface);
  GArray* dbus_salt;
  glib::ScopedError error;

  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           cryptohome::kCryptohomeGetSystemSalt,
                           &Resetter(&error).lvalue(),
                           G_TYPE_INVALID,
                           DBUS_TYPE_G_UCHAR_ARRAY,
                           &dbus_salt,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << cryptohome::kCryptohomeGetSystemSalt << " failed: "
                 << (error->message ? error->message : "Unknown Error.");
    return false;
  }
  char* local_salt = static_cast<char*>(g_malloc(dbus_salt->len));
  if (local_salt) {
    memcpy(local_salt, static_cast<const void*>(dbus_salt->data),
           dbus_salt->len);
    *salt = local_salt;
    *length = dbus_salt->len;
    g_array_free(dbus_salt, false);
    return true;
  }
  g_array_free(dbus_salt, false);
  return false;
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

    LOG(FATAL) << cryptohome::kCryptohomeIsMounted << " failed: "
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
int ChromeOSCryptohomeAsyncMountSafe(
    const char* user_email,
    const char* key,
    bool create_if_missing,
    bool replace_tracked_subdirectories,
    const char** tracked_subdirectories) {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy proxy(bus,
                    cryptohome::kCryptohomeServiceName,
                    cryptohome::kCryptohomeServicePath,
                    cryptohome::kCryptohomeInterface);
  gint async_call_id = 0;
  glib::ScopedError error;

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
                           tracked_subdirectories,
                           G_TYPE_INVALID,
                           G_TYPE_INT,
                           &async_call_id,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << cryptohome::kCryptohomeAsyncMount << " failed: "
                 << (error->message ? error->message : "Unknown Error.");
  }

  return async_call_id;
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
int ChromeOSCryptohomeAsyncSetOwnerUser(const char* username) {
  return 0;
}

extern "C"
bool ChromeOSCryptohomeTpmIsReady() {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy proxy(bus,
                    cryptohome::kCryptohomeServiceName,
                    cryptohome::kCryptohomeServicePath,
                    cryptohome::kCryptohomeInterface);
  gboolean ready = false;
  glib::ScopedError error;

  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           cryptohome::kCryptohomeTpmIsReady,
                           &Resetter(&error).lvalue(),
                           G_TYPE_INVALID,
                           G_TYPE_BOOLEAN,
                           &ready,
                           G_TYPE_INVALID)) {

    LOG(WARNING) << cryptohome::kCryptohomeTpmIsReady << " failed: "
                 << (error->message ? error->message : "Unknown Error.");

  }
  return ready;
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
bool ChromeOSCryptohomeTpmGetPasswordSafe(char** password) {
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
    *password = local_password;
    return true;
  }
  *password = NULL;
  return false;
}

extern "C"
void ChromeOSCryptohomeTpmCanAttemptOwnership() {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy proxy(bus,
                    cryptohome::kCryptohomeServiceName,
                    cryptohome::kCryptohomeServicePath,
                    cryptohome::kCryptohomeInterface);
  glib::ScopedError error;

  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           cryptohome::kCryptohomeTpmCanAttemptOwnership,
                           &Resetter(&error).lvalue(),
                           G_TYPE_INVALID,
                           G_TYPE_INVALID)) {

    LOG(WARNING) << cryptohome::kCryptohomeTpmCanAttemptOwnership << " failed: "
                 << (error->message ? error->message : "Unknown Error.");

  }
}

extern "C"
void ChromeOSCryptohomeTpmClearStoredPassword() {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy proxy(bus,
                    cryptohome::kCryptohomeServiceName,
                    cryptohome::kCryptohomeServicePath,
                    cryptohome::kCryptohomeInterface);
  glib::ScopedError error;

  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           cryptohome::kCryptohomeTpmClearStoredPassword,
                           &Resetter(&error).lvalue(),
                           G_TYPE_INVALID,
                           G_TYPE_INVALID)) {

    LOG(WARNING) << cryptohome::kCryptohomeTpmClearStoredPassword << " failed: "
                 << (error->message ? error->message : "Unknown Error.");

  }
}

// TODO(ellyjones): Remove this. crosbug.com/22120
extern "C"
bool ChromeOSCryptohomePkcs11IsTpmTokenReady() {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy proxy(bus,
                    cryptohome::kCryptohomeServiceName,
                    cryptohome::kCryptohomeServicePath,
                    cryptohome::kCryptohomeInterface);
  gboolean ready = FALSE;
  glib::ScopedError error;

  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           cryptohome::kCryptohomePkcs11IsTpmTokenReady,
                           &Resetter(&error).lvalue(),
                           G_TYPE_INVALID,
                           G_TYPE_BOOLEAN,
                           &ready,
                           G_TYPE_INVALID)) {

    LOG(WARNING) << cryptohome::kCryptohomePkcs11IsTpmTokenReady << " failed: "
                 << (error->message ? error->message : "Unknown Error.");

  }
  return ready;
}

extern "C"
bool ChromeOSCryptohomePkcs11IsTpmTokenReadyForUser(const std::string& user) {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy proxy(bus,
                    cryptohome::kCryptohomeServiceName,
                    cryptohome::kCryptohomeServicePath,
                    cryptohome::kCryptohomeInterface);
  gboolean ready = FALSE;
  glib::ScopedError error;

  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           cryptohome::kCryptohomePkcs11IsTpmTokenReadyForUser,
                           &Resetter(&error).lvalue(),
                           G_TYPE_STRING,
                           user.c_str(),
                           G_TYPE_INVALID,
                           G_TYPE_BOOLEAN,
                           &ready,
                           G_TYPE_INVALID)) {

    LOG(WARNING) << cryptohome::kCryptohomePkcs11IsTpmTokenReadyForUser
                 << " failed: "
                 << (error->message ? error->message : "Unknown Error.");

  }
  return ready;
}

// TODO(ellyjones): Remove this. crosbug.com/22120
extern "C"
void ChromeOSCryptohomePkcs11GetTpmTokenInfo(std::string* label,
                                             std::string* user_pin) {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy proxy(bus,
                    cryptohome::kCryptohomeServiceName,
                    cryptohome::kCryptohomeServicePath,
                    cryptohome::kCryptohomeInterface);
  gchar* local_label = NULL;
  gchar* local_user_pin = NULL;
  glib::ScopedError error;

  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           cryptohome::kCryptohomePkcs11GetTpmTokenInfo,
                           &Resetter(&error).lvalue(),
                           G_TYPE_INVALID,
                           G_TYPE_STRING,
                           &local_label,
                           G_TYPE_STRING,
                           &local_user_pin,
                           G_TYPE_INVALID)) {

    LOG(WARNING) << cryptohome::kCryptohomePkcs11GetTpmTokenInfo << " failed: "
                 << (error->message ? error->message : "Unknown Error.");

  }

  if (local_label) {
    *label = local_label;
    g_free(local_label);
  }
  if (local_user_pin) {
    *user_pin = local_user_pin;
    g_free(local_user_pin);
  }
}

extern "C"
void ChromeOSCryptohomePkcs11GetTpmTokenInfoForUser(const std::string& user,
                                                    std::string* label,
                                                    std::string* user_pin) {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy proxy(bus,
                    cryptohome::kCryptohomeServiceName,
                    cryptohome::kCryptohomeServicePath,
                    cryptohome::kCryptohomeInterface);
  gchar* local_label = NULL;
  gchar* local_user_pin = NULL;
  glib::ScopedError error;

  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           cryptohome::kCryptohomePkcs11GetTpmTokenInfoForUser,
                           &Resetter(&error).lvalue(),
                           G_TYPE_STRING,
                           user.c_str(),
                           G_TYPE_INVALID,
                           G_TYPE_STRING,
                           &local_label,
                           G_TYPE_STRING,
                           &local_user_pin,
                           G_TYPE_INVALID)) {

    LOG(WARNING) << cryptohome::kCryptohomePkcs11GetTpmTokenInfo << " failed: "
                 << (error->message ? error->message : "Unknown Error.");

  }

  if (local_label) {
    *label = local_label;
    g_free(local_label);
  }
  if (local_user_pin) {
    *user_pin = local_user_pin;
    g_free(local_user_pin);
  }
}

extern "C"
bool ChromeOSCryptohomeGetStatusStringSafe(char** status) {
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
    *status = local_status;
    return true;
  }
  *status = NULL;
  return false;
}

extern "C"
bool ChromeOSCryptohomeGetStatusString(std::string* status) {
  gchar* local_status = NULL;
  if (!ChromeOSCryptohomeGetStatusStringSafe(&local_status)) {
    return false;
  }

  status->assign(local_status);
  g_free(local_status);
  return true;
}

extern "C"
bool ChromeOSCryptohomeInstallAttributesGet(const char* name, char** value) {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy proxy(bus,
                    cryptohome::kCryptohomeServiceName,
                    cryptohome::kCryptohomeServicePath,
                    cryptohome::kCryptohomeInterface);
  gboolean done = false;
  GArray* dbus_value = NULL;
  glib::ScopedError error;

  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           cryptohome::kCryptohomeInstallAttributesGet,
                           &Resetter(&error).lvalue(),
                           G_TYPE_STRING,
                           name,
                           G_TYPE_INVALID,
                           DBUS_TYPE_G_UCHAR_ARRAY,
                           &dbus_value,
                           G_TYPE_BOOLEAN,
                           &done,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << cryptohome::kCryptohomeInstallAttributesGet << " failed: "
                 << (error->message ? error->message : "Unknown Error.");
  }

  if (!dbus_value || !done)
    return false;

  char* local_value = static_cast<char*>(g_malloc(dbus_value->len));
  if (local_value) {
    memcpy(local_value, static_cast<const void*>(dbus_value->data),
           dbus_value->len);
    *value = local_value;
    g_array_free(dbus_value, false);
    return done;
  }
  g_array_free(dbus_value, false);
  return false;
}

extern "C"
bool ChromeOSCryptohomeInstallAttributesSet(const char* name,
                                            const char* value) {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy proxy(bus,
                    cryptohome::kCryptohomeServiceName,
                    cryptohome::kCryptohomeServicePath,
                    cryptohome::kCryptohomeInterface);
  gboolean done = false;
  // TODO(pastarmovj): Change this to a bin data compatible impl. when needed.
  int value_len = strlen(value)+1;
  gchar* local_value = static_cast<char*>(g_malloc(value_len));
  if (!local_value)
    return false;

  memcpy(local_value, static_cast<const gchar*>(value), value_len);
  GArray *dbus_value = g_array_new(FALSE, FALSE, 1);
  if (!dbus_value)
    return false;

  dbus_value->data = local_value;
  dbus_value->len = value_len;
  glib::ScopedError error;

  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           cryptohome::kCryptohomeInstallAttributesSet,
                           &Resetter(&error).lvalue(),
                           G_TYPE_STRING,
                           name,
                           DBUS_TYPE_G_UCHAR_ARRAY,
                           dbus_value,
                           G_TYPE_INVALID,
                           G_TYPE_BOOLEAN,
                           &done,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << cryptohome::kCryptohomeInstallAttributesSet << " failed: "
                 << (error->message ? error->message : "Unknown Error.");
  }

  g_array_free(dbus_value, FALSE);
  g_free(local_value);

  return done;
}

bool CallCryptohomeBoolFunction(const char* function) {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy proxy(bus,
                    cryptohome::kCryptohomeServiceName,
                    cryptohome::kCryptohomeServicePath,
                    cryptohome::kCryptohomeInterface);
  gboolean done = false;
  glib::ScopedError error;

  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           function,
                           &Resetter(&error).lvalue(),
                           G_TYPE_INVALID,
                           G_TYPE_BOOLEAN,
                           &done,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << function << " failed: "
                 << (error->message ? error->message : "Unknown Error.");
  }
  return done;
}


extern "C"
bool ChromeOSCryptohomeInstallAttributesFinalize() {
  return CallCryptohomeBoolFunction(
      cryptohome::kCryptohomeInstallAttributesFinalize);
}

extern "C"
bool ChromeOSCryptohomeInstallAttributesIsReady() {
  return CallCryptohomeBoolFunction(
      cryptohome::kCryptohomeInstallAttributesIsReady);
}

extern "C"
bool ChromeOSCryptohomeInstallAttributesIsInvalid() {
  return CallCryptohomeBoolFunction(
      cryptohome::kCryptohomeInstallAttributesIsInvalid);
}

extern "C"
bool ChromeOSCryptohomeInstallAttributesIsFirstInstall() {
  return CallCryptohomeBoolFunction(
      cryptohome::kCryptohomeInstallAttributesIsFirstInstall);
}

extern "C"
void ChromeOSCryptohomeFreeString(char* value) {
  if (value) {
    g_free(value);
  }
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

}  // namespace chromeos
