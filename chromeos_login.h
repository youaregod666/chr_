// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
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

extern SessionConnection (*MonitorSession)(SessionMonitor monitor, void*);
extern void (*DisconnectSession)(SessionConnection connection);

// DEPRECATED due to memory unsafety.
extern bool (*CheckWhitelist)(const char* email,
                              std::vector<uint8>* OUT_signature);

// Checks if |email| is on the whitelist.
// Returns true if so, and allocates a CryptoBlob to pass back in the out param.
// If not, returns false and |OUT_signature| is untouched.
// Free |OUT_signature| using FreeCryptoBlob().
extern bool (*CheckWhitelistSafe)(const char* email,
                                  CryptoBlob** OUT_signature);

// Kicks off an attempt to emit the "login-prompt-ready" upstart signal.
extern bool (*EmitLoginPromptReady)();

// DEPRECATED due to memory unsafety.
extern bool (*EnumerateWhitelisted)(std::vector<std::string>* OUT_whitelisted);

// EnumerateWhitelisted() is for informational purposes only.  The data
// is returned without signatures.  To determine if a user is allowed to log in
// to the device, YOU MUST use CheckWhitelist and verify the signature that is
// returned.
// Free |OUT_whitelisted| using FreeUserList().
extern bool (*EnumerateWhitelistedSafe)(UserList** OUT_whitelisted);

// These methods are used to create structures to pass to
// the "*Safe" functions defined in this file.  We need to do this
// to safely pass data over the dll boundary between our .so and Chrome.
extern CryptoBlob* (*CreateCryptoBlob)(const uint8* in, const int in_len);
extern Property* (*CreateProperty)(const char* name, const char* value,
                                   const uint8* sig, const int sig_len);
extern UserList* (*CreateUserList)(char** users);

// These methods are used to free structures that were returned in
// out-params from "*Safe" functions defined in this file.  We need to do this
// to safely pass data over the dll boundary between our .so and Chrome.
extern void (*FreeCryptoBlob)(CryptoBlob* blob);
extern void (*FreeProperty)(Property* property);
extern void (*FreeUserList)(UserList* userlist);

extern bool (*RestartJob)(int pid, const char* command_line);

// DEPRECATED due to memory unsafety.
extern bool (*RetrieveProperty)(const char* name,
                                std::string* OUT_value,
                                std::vector<uint8>* OUT_signature);

// Fetches the property called |name|.
// Returns true if it can be fetched, allocates a Property to pass back.
// If not, returns false and |OUT_property| is untouched
// Free |OUT_property| using FreeProperty.
extern bool (*RetrievePropertySafe)(const char* name, Property** OUT_property);

// DEPRECATED due to memory unsafety.
extern bool (*SetOwnerKey)(const std::vector<uint8>& public_key_der);

// Attempts to set the Owner key to |public_key_der|.
// Returns true if the attempt starts successfully.
extern bool (*SetOwnerKeySafe)(const CryptoBlob* public_key_der);

extern bool (*StartSession)(const char* user_email,
                            const char* unique_id /* unused */);

extern bool (*StopSession)(const char* unique_id /* unused */);

// DEPRECATED due to memory unsafety.
extern bool (*StoreProperty)(const char* name,
                             const char* value,
                             const std::vector<uint8>& signature);

// Attempts to store |prop|.
// Returns true if the attempt starts successfully.
extern bool (*StorePropertySafe)(const Property* prop);

// DEPRECATED due to memory unsafety.
extern bool (*Unwhitelist)(const char* email,
                           const std::vector<uint8>& signature);

// Attempts to remove |email| from the whitelist.
// Returns true if the attempt is started successfully.
extern bool (*UnwhitelistSafe)(const char* email,
                               const CryptoBlob* signature);

// DEPRECATED due to memory unsafety.
extern bool (*Whitelist)(const char* email,
                         const std::vector<uint8>& signature);

// Attempts to whitelist |email|.
// Returns true if the attempt is successfully started.
extern bool (*WhitelistSafe)(const char* email,
                             const CryptoBlob* signature);

}  // namespace chromeos

#endif  // CHROMEOS_LOGIN_H_
