// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LOGIN_H_
#define CHROMEOS_LOGIN_H_

#include <string>
#include <vector>

#include <base/basictypes.h>

namespace chromeos { // NOLINT

// TODO(cmasone): change references to "login" (LoginLibrary, etc) to "session"
// or similar.  The API implemented here doesn't really deal with logging in
// so much as state relating to user and the user sessions.

enum OwnershipEvent {
  SetKeySuccess = 0,
  SetKeyFailure = 1,
  WhitelistOpSuccess = 2,
  WhitelistOpFailure = 3,
  PropertyOpSuccess = 4,
  PropertyOpFailure = 5,
};

struct CryptoBlob {
  const uint8* data;
  int length;
};

struct Property {
  const char* name;
  const char* value;
  CryptoBlob* signature;
};

struct UserList {
  const char** users;  // array of NULL-terminated C-strings
  int num_users;
};

static const char kOwnerKeyFile[] = "/var/lib/whitelist/owner.key";

class OpaqueSessionConnection;
typedef OpaqueSessionConnection* SessionConnection;
typedef void(*SessionMonitor)(void*, const OwnershipEvent&);

// Async callback functions
typedef void(*RetrievePolicyCallback)(void*, const char*, const unsigned int);
typedef void(*StorePolicyCallback)(void*, bool);
typedef void(*RetrievePropertyCallback)(void* user_data,
                                        bool success,
                                        const Property* property);

}  // namespace chromeos

#endif  // CHROMEOS_LOGIN_H_
