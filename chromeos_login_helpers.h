// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include <base/basictypes.h>
#include <chromeos/dbus/dbus.h>
#include <glib.h>

#include "chromeos_login.h"  // NOLINT

namespace chromeos {  // NOLINT

struct CryptoBlob;
struct Property;
struct UserList;

class ChromeOSLoginHelpers {
 public:
  static dbus::Proxy CreateProxy();
  static uint8* NewBufferCopy(const uint8* x, int len);
  static GArray* CreateGArrayFromBytes(const uint8* in, const int in_len);
  static bool CheckWhitelistHelper(const char* email, GArray** sig);
  static bool EnumerateWhitelistedHelper(gchar*** whitelisted);
  static void RequestRetrievePropertyHelper(const char* name,
                                            RetrievePropertyCallback callback,
                                            void* user_data);
  static bool RetrievePropertyHelper(const char* name,
                                     gchar** value,
                                     GArray** sig);
  static bool SetOwnerKeyHelper(GArray* key_der);
  static bool StorePropertyHelper(const char* name,
                                  const char* value,
                                  GArray* sig);
  static bool WhitelistOpHelper(const char* op,
                                const char* email,
                                const std::vector<uint8>& signature);
  // Constructors, Destructors for useful structs
  static CryptoBlob* CreateCryptoBlob(GArray* in);
  static Property* CreateProperty(const char* name,
                                  const gchar* value,
                                  GArray* sig);
  static UserList* CreateUserList(const char* const* users);
  static void FreeCryptoBlob(CryptoBlob* blob);
  static void FreeProperty(Property* property);
  static void FreeUserList(UserList* userlist);

 private:
  ChromeOSLoginHelpers();
  ~ChromeOSLoginHelpers();
  DISALLOW_COPY_AND_ASSIGN(ChromeOSLoginHelpers);
};

}  // namespace chromeos
