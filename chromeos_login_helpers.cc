// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_login_helpers.h"  // NOLINT

#include <base/basictypes.h>
#include <base/logging.h>
#include <base/string_util.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/dbus/service_constants.h>
#include <chromeos/glib/object.h>
#include <chromeos/string.h>
#include <vector>

#include "marshal.glibmarshal.h"  // NOLINT

namespace {

#define SCOPED_SAFE_MESSAGE(e) (e->message ? e->message : "unknown error")

struct RetrievePropertyContext {
  RetrievePropertyContext(const char* name,
                          chromeos::RetrievePropertyCallback cb,
                          void* data)
      : property_name(name),
        callback(cb),
        user_data(data),
        proxy(chromeos::ChromeOSLoginHelpers::CreateProxy()) {
  }

  std::string property_name;
  chromeos::RetrievePropertyCallback callback;
  void* user_data;
  chromeos::dbus::Proxy proxy;
};

void RetrievePropertyNotify(DBusGProxy* proxy,
                            DBusGProxyCall* call_id,
                            void* user_data) {
  RetrievePropertyContext* context =
      static_cast<RetrievePropertyContext*>(user_data);
  DCHECK(context);

  chromeos::glib::ScopedError error;
  gchar* value;
  GArray* sig;
  if (!::dbus_g_proxy_end_call(proxy,
                               call_id,
                               &chromeos::Resetter(&error).lvalue(),
                               G_TYPE_STRING, &value,
                               DBUS_TYPE_G_UCHAR_ARRAY, &sig,
                               G_TYPE_INVALID)) {
    LOG(WARNING) << login_manager::kSessionManagerRetrieveProperty
                 << " failed: " << SCOPED_SAFE_MESSAGE(error);
    context->callback(context->user_data, false, NULL);
  } else {
    chromeos::CryptoBlob sig_blob = {
        reinterpret_cast<const uint8*>(sig->data),
        sig->len
    };
    chromeos::Property property = {
        context->property_name.c_str(),
        value,
        &sig_blob
    };
    context->callback(context->user_data, true, &property);

    g_array_free(sig, true);
    g_free(value);
  }
}

void FreeRetrievePropertyContext(gpointer data) {
  delete static_cast<RetrievePropertyContext*>(data);
}

}  // namespace

namespace chromeos {  // NOLINT

ChromeOSLoginHelpers::ChromeOSLoginHelpers() {}

ChromeOSLoginHelpers::~ChromeOSLoginHelpers() {}

// static
dbus::Proxy ChromeOSLoginHelpers::CreateProxy() {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  return dbus::Proxy(bus,
                     login_manager::kSessionManagerServiceName,
                     login_manager::kSessionManagerServicePath,
                     login_manager::kSessionManagerInterface);
}

// Memory allocated by this method should be freed with delete, not delete [].
// static
uint8* ChromeOSLoginHelpers::NewBufferCopy(const uint8* x, int len) {
  uint8* result = static_cast<uint8*>(::operator new(len));
  std::memcpy(result, x, len);
  return result;
}

// static
GArray* ChromeOSLoginHelpers::CreateGArrayFromBytes(const uint8* in,
                                                    const int in_len) {
  GArray* ary = g_array_sized_new(FALSE, FALSE, 1, in_len);
  g_array_append_vals(ary, in, in_len);
  return ary;
}

// static
bool ChromeOSLoginHelpers::CheckWhitelistHelper(const char* email,
                                                GArray** sig) {
  dbus::Proxy proxy = CreateProxy();
  glib::ScopedError error;
  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           login_manager::kSessionManagerCheckWhitelist,
                           &Resetter(&error).lvalue(),
                           G_TYPE_STRING, email,
                           G_TYPE_INVALID,
                           DBUS_TYPE_G_UCHAR_ARRAY, sig,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << login_manager::kSessionManagerCheckWhitelist << " failed: "
                 << SCOPED_SAFE_MESSAGE(error);
    return false;
  }
  return true;
}

// static
bool ChromeOSLoginHelpers::EnumerateWhitelistedHelper(gchar*** whitelisted) {
  dbus::Proxy proxy = ChromeOSLoginHelpers::CreateProxy();
  glib::ScopedError error;
  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           login_manager::kSessionManagerEnumerateWhitelisted,
                           &Resetter(&error).lvalue(),
                           G_TYPE_INVALID,
                           G_TYPE_STRV, whitelisted,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << login_manager::kSessionManagerEnumerateWhitelisted
                 << " failed: " << SCOPED_SAFE_MESSAGE(error);
    return false;
  }
  return true;
}

// static
void ChromeOSLoginHelpers::RequestRetrievePropertyHelper(const char* name,
    RetrievePropertyCallback callback, void* user_data) {
  RetrievePropertyContext* context = new RetrievePropertyContext(name,
      callback, user_data);

  DBusGProxyCall* call_id = ::dbus_g_proxy_begin_call(context->proxy.gproxy(),
      login_manager::kSessionManagerRetrieveProperty,
      &RetrievePropertyNotify,
      context,
      &FreeRetrievePropertyContext,
      G_TYPE_STRING, name,
      G_TYPE_INVALID);
  if (!call_id) {
    LOG(WARNING) << login_manager::kSessionManagerRetrieveProperty
                 << " dbus_g_proxy_begin_call failed";
    delete context;
    callback(user_data, false, NULL);
  }
}

// static
bool ChromeOSLoginHelpers::RetrievePropertyHelper(const char* name,
                                                  gchar** value,
                                                  GArray** sig) {
  dbus::Proxy proxy = ChromeOSLoginHelpers::CreateProxy();
  glib::ScopedError error;
  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           login_manager::kSessionManagerRetrieveProperty,
                           &Resetter(&error).lvalue(),
                           G_TYPE_STRING, name,
                           G_TYPE_INVALID,
                           G_TYPE_STRING, value,
                           DBUS_TYPE_G_UCHAR_ARRAY, sig,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << login_manager::kSessionManagerRetrieveProperty
                 << " failed: " << SCOPED_SAFE_MESSAGE(error);
    return false;
  }
  return true;
}

// static
bool ChromeOSLoginHelpers::SetOwnerKeyHelper(GArray* key_der) {
  dbus::Proxy proxy = ChromeOSLoginHelpers::CreateProxy();
  glib::ScopedError error;
  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           login_manager::kSessionManagerSetOwnerKey,
                           &Resetter(&error).lvalue(),
                           DBUS_TYPE_G_UCHAR_ARRAY, key_der,
                           G_TYPE_INVALID,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << login_manager::kSessionManagerSetOwnerKey << " failed: "
                 << SCOPED_SAFE_MESSAGE(error);
    return false;
  }
  return true;
}

// static
bool ChromeOSLoginHelpers::StorePropertyHelper(const char* name,
                                               const char* value,
                                               GArray* sig) {
  dbus::Proxy proxy = ChromeOSLoginHelpers::CreateProxy();
  glib::ScopedError error;
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
    return false;
  }
  return true;
}

// static
bool ChromeOSLoginHelpers::WhitelistOpHelper(
    const char* op,
    const char* email,
    const std::vector<uint8>& signature) {
  dbus::Proxy proxy = ChromeOSLoginHelpers::CreateProxy();
  glib::ScopedError error;
  GArray* sig = CreateGArrayFromBytes(&signature[0], signature.size());

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

// static
CryptoBlob* ChromeOSLoginHelpers::CreateCryptoBlob(GArray* in) {
  CryptoBlob* blob = new CryptoBlob;
  blob->length = in->len;
  blob->data = ChromeOSLoginHelpers::NewBufferCopy(
      reinterpret_cast<const uint8*>(in->data), in->len);
  return blob;
}

// static
Property* ChromeOSLoginHelpers::CreateProperty(const char* name,
                                               const gchar* value,
                                               GArray* sig) {
  Property* prop = new Property;
  prop->signature = CreateCryptoBlob(sig);
  prop->name = NewStringCopy(name);
  prop->value = NewStringCopy(value);
  return prop;
}

// static
UserList* ChromeOSLoginHelpers::CreateUserList(const char* const* users) {
  UserList* list = new UserList;
  list->users = NULL;
  for (list->num_users = 0; users[list->num_users] != NULL; list->num_users++)
    ;
  list->users = static_cast<const char**>(
      ::operator new(sizeof(char*) * (list->num_users + 1)));
  list->users[list->num_users] = NULL;
  for (int i = 0; users[i] != NULL; ++i)
    list->users[i] = NewStringCopy(users[i]);
  return list;
}

// These Free* methods all use delete (as opposed to delete []) on purpose,
// following the pattern established by code that uses NewStringCopy.
// static
void ChromeOSLoginHelpers::FreeCryptoBlob(CryptoBlob* blob) {
  delete blob->data;
  delete blob;
}

// static
void ChromeOSLoginHelpers::FreeProperty(Property* property) {
  FreeCryptoBlob(property->signature);
  delete property->name;
  delete property->value;
}

// static
void ChromeOSLoginHelpers::FreeUserList(UserList* userlist) {
  for (int i = 0; i < userlist->num_users; ++i)
    delete userlist->users[i];
  delete userlist->users;
  delete userlist;
}
}  // namespace chromeos
