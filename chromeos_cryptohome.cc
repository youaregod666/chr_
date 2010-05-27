// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_cryptohome.h"  // NOLINT

#include <chromeos/dbus/dbus.h>
#include <chromeos/dbus/service_constants.h>
#include <chromeos/glib/object.h>

#include "marshal.h"  // NOLINT

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
  if(system_salt.size() == salt->len) {
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

extern "C"
bool ChromeOSCryptohomeMount(const char* user_email, const char* key) {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy proxy(bus,
                    cryptohome::kCryptohomeServiceName,
                    cryptohome::kCryptohomeServicePath,
                    cryptohome::kCryptohomeInterface);
  gboolean done = false;
  glib::ScopedError error;

  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           cryptohome::kCryptohomeMount,
                           &Resetter(&error).lvalue(),
                           G_TYPE_STRING,
                           user_email,
                           G_TYPE_STRING,
                           key,
                           G_TYPE_INVALID,
                           G_TYPE_BOOLEAN,
                           &done,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << cryptohome::kCryptohomeMount << " failed: "
                 << (error->message ? error->message : "Unknown Error.");
  }
  return done;
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

}  // namespace chromeos
