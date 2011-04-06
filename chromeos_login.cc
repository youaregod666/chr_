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
#include <chromeos/string.h>

#include "chromeos_login_helpers.h"  // NOLINT
#include "marshal.glibmarshal.h"  // NOLINT

namespace chromeos {  // NOLINT

// TODO(cmasone): do a bigger refactor, get rid of this copy-paste macro.
#define SCOPED_SAFE_MESSAGE(e) (e->message ? e->message : "unknown error")

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
  // TODO(cmasone): Remove this code, once we're sure NOTREACHED isn't hit.
  NOTREACHED() << "ChromeOSCheckWhitelist is deprecated";
  DCHECK(signature);
  GArray* sig;
  if (!ChromeOSLoginHelpers::CheckWhitelistHelper(email, &sig))
    return false;
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
bool ChromeOSCheckWhitelistSafe(const char* email,
                                CryptoBlob** OUT_signature) {
  DCHECK(OUT_signature);
  GArray* sig;
  if (!ChromeOSLoginHelpers::CheckWhitelistHelper(email, &sig))
    return false;
  *OUT_signature = ChromeOSLoginHelpers::CreateCryptoBlob(sig);
  g_array_free(sig, true);
  return true;
}

extern "C"
bool ChromeOSEmitLoginPromptReady() {
  chromeos::dbus::Proxy proxy = ChromeOSLoginHelpers::CreateProxy();
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
bool ChromeOSEnumerateWhitelisted(std::vector<std::string>* OUT_whitelisted) {
  // TODO(cmasone): Remove this code, once we're sure NOTREACHED isn't hit.
  NOTREACHED() << "ChromeOSEnumerateWhitelisted is deprecated";
  DCHECK(OUT_whitelisted);
  gchar** whitelisted = NULL;
  if (!ChromeOSLoginHelpers::EnumerateWhitelistedHelper(&whitelisted))
    return false;
  for (int i = 0; whitelisted[i] != NULL; ++i)
    OUT_whitelisted->push_back(std::string(whitelisted[i]));

  g_strfreev(whitelisted);
  return true;
}

extern "C"
bool ChromeOSEnumerateWhitelistedSafe(UserList** OUT_whitelisted) {
  DCHECK(OUT_whitelisted);
  gchar** whitelisted = NULL;
  if (!ChromeOSLoginHelpers::EnumerateWhitelistedHelper(&whitelisted))
    return false;
  *OUT_whitelisted = ChromeOSLoginHelpers::CreateUserList(whitelisted);
  g_strfreev(whitelisted);
  return true;
}

extern "C"
CryptoBlob* ChromeOSCreateCryptoBlob(const uint8* in, const int in_len) {
  GArray* ary = ChromeOSLoginHelpers::CreateGArrayFromBytes(in, in_len);
  CryptoBlob* blob = ChromeOSLoginHelpers::CreateCryptoBlob(ary);
  g_array_free(ary, TRUE);
  return blob;
}

extern "C"
Property* ChromeOSCreateProperty(const char* name, const char* value,
                                 const uint8* sig, const int sig_len) {
  GArray* ary = ChromeOSLoginHelpers::CreateGArrayFromBytes(sig, sig_len);
  Property* prop = ChromeOSLoginHelpers::CreateProperty(name, value, ary);
  g_array_free(ary, TRUE);
  return prop;
}

extern "C"
UserList* ChromeOSCreateUserList(char** users) {
  return ChromeOSLoginHelpers::CreateUserList(users);
}

// These Free* methods all use delete (as opposed to delete []) on purpose,
// following the pattern established by code that uses NewStringCopy.
extern "C"
void ChromeOSFreeCryptoBlob(CryptoBlob* blob) {
  ChromeOSLoginHelpers::FreeCryptoBlob(blob);
}

extern "C"
void ChromeOSFreeProperty(Property* property) {
  ChromeOSLoginHelpers::FreeProperty(property);
}

extern "C"
void ChromeOSFreeUserList(UserList* userlist) {
  ChromeOSLoginHelpers::FreeUserList(userlist);
}

extern "C"
bool ChromeOSRestartJob(int pid, const char* command_line) {
  chromeos::dbus::Proxy proxy = ChromeOSLoginHelpers::CreateProxy();
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
bool ChromeOSRestartEntd() {
  chromeos::dbus::Proxy proxy = ChromeOSLoginHelpers::CreateProxy();

  ::dbus_g_proxy_call_no_reply(proxy.gproxy(),
                               login_manager::kSessionManagerRestartEntd,
                               G_TYPE_INVALID,
                               G_TYPE_INVALID);
  return true;
}

extern "C"
void ChromeOSRequestRetrieveProperty(const char* name,
                                     RetrievePropertyCallback callback,
                                     void* user_data) {
  ChromeOSLoginHelpers::RequestRetrievePropertyHelper(name, callback,
      user_data);
}

extern "C"
bool ChromeOSRetrieveProperty(const char* name,
                              std::string* out_value,
                              std::vector<uint8>* signature) {
  // TODO(cmasone): Remove this code, once we're sure NOTREACHED isn't hit.
  NOTREACHED() << "ChromeOSRetrieveProperty is deprecated";
  DCHECK(signature);
  DCHECK(out_value);
  GArray* sig;
  gchar* value = NULL;
  if (!ChromeOSLoginHelpers::RetrievePropertyHelper(name, &value, &sig))
    return false;
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
bool ChromeOSRetrievePropertySafe(const char* name, Property** OUT_property) {
  DCHECK(OUT_property);
  GArray* sig;
  gchar* value = NULL;
  if (!ChromeOSLoginHelpers::RetrievePropertyHelper(name, &value, &sig))
    return false;
  *OUT_property = ChromeOSLoginHelpers::CreateProperty(name, value, sig);
  g_array_free(sig, false);
  g_free(value);
  return true;
}

extern "C"
bool ChromeOSSetOwnerKey(const std::vector<uint8>& public_key_der) {
  // TODO(cmasone): Remove this code, once we're sure NOTREACHED isn't hit.
  NOTREACHED() << "ChromeOSSetOwnerKey is deprecated";
  GArray* key_der =
      ChromeOSLoginHelpers::CreateGArrayFromBytes(&public_key_der[0],
                                                  public_key_der.size());
  bool rv = ChromeOSLoginHelpers::SetOwnerKeyHelper(key_der);
  g_array_free(key_der, TRUE);
  return rv;
}

extern "C"
bool ChromeOSSetOwnerKeySafe(const CryptoBlob* public_key_der) {
  GArray* key_der =
      ChromeOSLoginHelpers::CreateGArrayFromBytes(public_key_der->data,
                                                  public_key_der->length);
  bool rv = ChromeOSLoginHelpers::SetOwnerKeyHelper(key_der);
  g_array_free(key_der, TRUE);
  return rv;
}

extern "C"
bool ChromeOSStartSession(const char* user_email,
                          const char* unique_id /* unused */) {
  chromeos::dbus::Proxy proxy = ChromeOSLoginHelpers::CreateProxy();
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
  chromeos::dbus::Proxy proxy = ChromeOSLoginHelpers::CreateProxy();
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
bool ChromeOSStoreProperty(const char* name,
                           const char* value,
                           const std::vector<uint8>& signature) {
  // TODO(cmasone): Remove this code, once we're sure NOTREACHED isn't hit.
  NOTREACHED() << "ChromeOSStoreProperty is deprecated!";
  GArray* sig = ChromeOSLoginHelpers::CreateGArrayFromBytes(&signature[0],
                                                            signature.size());
  bool rv = ChromeOSLoginHelpers::StorePropertyHelper(name, value, sig);
  g_array_free(sig, TRUE);
  return rv;
}

extern "C"
bool ChromeOSStorePropertySafe(const Property* prop) {
  GArray* sig =
      ChromeOSLoginHelpers::CreateGArrayFromBytes(prop->signature->data,
                                                  prop->signature->length);
  bool rv = ChromeOSLoginHelpers::StorePropertyHelper(prop->name,
                                                      prop->value,
                                                      sig);
  g_array_free(sig, TRUE);
  return rv;
}

extern "C"
bool ChromeOSUnwhitelist(const char* email,
                         const std::vector<uint8>& signature) {
  // TODO(cmasone): Remove this code, once we're sure NOTREACHED isn't hit.
  NOTREACHED() << "ChromeOSUnwhitelist is deprecated!";
  return ChromeOSLoginHelpers::WhitelistOpHelper(
      login_manager::kSessionManagerUnwhitelist,
      email,
      signature);
}

extern "C"
bool ChromeOSUnwhitelistSafe(const char* email, const CryptoBlob* signature) {
  std::vector<uint8> sig(signature->data, signature->data + signature->length);
  return ChromeOSLoginHelpers::WhitelistOpHelper(
      login_manager::kSessionManagerUnwhitelist,
      email,
      sig);
}

extern "C"
bool ChromeOSWhitelist(const char* email,
                       const std::vector<uint8>& signature) {
  // TODO(cmasone): Remove this code, once we're sure NOTREACHED isn't hit.
  NOTREACHED() << "ChromeOSWhitelist is deprecated!";
  return ChromeOSLoginHelpers::WhitelistOpHelper(
      login_manager::kSessionManagerWhitelist,
      email,
      signature);
}

extern "C"
bool ChromeOSWhitelistSafe(const char* email, const CryptoBlob* signature) {
  std::vector<uint8> sig(signature->data, signature->data + signature->length);
  return ChromeOSLoginHelpers::WhitelistOpHelper(
      login_manager::kSessionManagerWhitelist,
      email,
      sig);
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

// Asynchronous call infrastructure
template<class T> struct CallbackData {
  CallbackData(T cb, void* obj)
      : proxy(ChromeOSLoginHelpers::CreateProxy()),
        callback(cb),
        object(obj) {
  }
  dbus::Proxy proxy;
  // Owned by the caller (i.e. Chrome), do not destroy them:
  T callback;
  void* object;
};

// DBus will always call the Delete function passed to it by
// dbus_g_proxy_begin_call, whether DBus calls the callback or not.
template<class T>
void DeleteCallbackData(void* user_data) {
  CallbackData<T>* cb_data = reinterpret_cast<CallbackData<T>*>(user_data);
  delete cb_data;
}

void RetrievePolicyNotify(DBusGProxy* gproxy,
                          DBusGProxyCall* call_id,
                          void* user_data) {
  CallbackData<RetrievePolicyCallback>* cb_data =
      reinterpret_cast<CallbackData<RetrievePolicyCallback>*>(user_data);
  DCHECK(cb_data);
  glib::ScopedError error;
  GArray* policy_blob = NULL;
  if (!::dbus_g_proxy_end_call(gproxy,
                               call_id,
                               &Resetter(&error).lvalue(),
                               DBUS_TYPE_G_UCHAR_ARRAY, &policy_blob,
                               G_TYPE_INVALID)) {
    LOG(ERROR) << login_manager::kSessionManagerRetrievePolicy
               << " failed: " << SCOPED_SAFE_MESSAGE(error);
  }
  if (policy_blob) {
    cb_data->callback(cb_data->object, policy_blob->data, policy_blob->len);
    g_array_free(policy_blob, TRUE);
  } else {
    cb_data->callback(cb_data->object, NULL, 0);
  }
}

extern "C"
void ChromeOSRetrievePolicy(RetrievePolicyCallback callback, void* delegate) {
  DCHECK(delegate);
  CallbackData<RetrievePolicyCallback>* cb_data =
      new CallbackData<RetrievePolicyCallback>(callback, delegate);
  DBusGProxyCall* call_id =
      ::dbus_g_proxy_begin_call(cb_data->proxy.gproxy(),
                                login_manager::kSessionManagerRetrievePolicy,
                                &RetrievePolicyNotify,
                                cb_data,
                                &DeleteCallbackData<RetrievePolicyCallback>,
                                G_TYPE_INVALID);
  if (!call_id) {
    LOG(ERROR) << "RetrievePolicy async call failed";
    delete cb_data;
    callback(delegate, NULL, 0);
  }
}

void StorePolicyNotify(DBusGProxy* gproxy,
                       DBusGProxyCall* call_id,
                       void* user_data) {
  CallbackData<StorePolicyCallback>* cb_data =
      reinterpret_cast<CallbackData<StorePolicyCallback>*>(user_data);
  DCHECK(cb_data);
  glib::ScopedError error;
  gboolean success = FALSE;
  if (!::dbus_g_proxy_end_call(gproxy,
                               call_id,
                               &Resetter(&error).lvalue(),
                               G_TYPE_BOOLEAN, &success,
                               G_TYPE_INVALID)) {
    LOG(ERROR) << login_manager::kSessionManagerStorePolicy
               << " failed: " << SCOPED_SAFE_MESSAGE(error);
  }
  cb_data->callback(cb_data->object, success);
}

extern "C"
void ChromeOSStorePolicy(const char* prop,
                         const unsigned int len,
                         StorePolicyCallback callback,
                         void* delegate) {
  DCHECK(prop);
  DCHECK(delegate);
  CallbackData<StorePolicyCallback>* cb_data =
      new CallbackData<StorePolicyCallback>(callback, delegate);
  GArray* policy = g_array_new(FALSE, FALSE, 1);
  policy->data = const_cast<gchar*>(prop);  // This just gets copied below.
  policy->len = len;
  DBusGProxyCall* call_id =
      ::dbus_g_proxy_begin_call(cb_data->proxy.gproxy(),
                                login_manager::kSessionManagerStorePolicy,
                                &StorePolicyNotify,
                                cb_data,
                                &DeleteCallbackData<StorePolicyCallback>,
                                DBUS_TYPE_G_UCHAR_ARRAY, policy,
                                G_TYPE_INVALID);
  if (!call_id) {
    LOG(ERROR) << "StorePolicy async call failed";
    delete cb_data;
    callback(delegate, false);
  }
  g_array_free(policy, FALSE);
}

}  // namespace chromeos
