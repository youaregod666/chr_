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

static const char kOwnerKeyFile[] = "/var/lib/whitelist/owner.key";

class OpaqueSessionConnection;
typedef OpaqueSessionConnection* SessionConnection;
typedef void(*SessionMonitor)(void*, const OwnershipEvent&);

extern SessionConnection (*MonitorSession)(SessionMonitor monitor, void*);
extern void (*DisconnectSession)(SessionConnection connection);

extern bool (*CheckWhitelist)(const char* email,
                              std::vector<uint8>* OUT_signature);
extern bool (*EmitLoginPromptReady)();

// EnumerateWhitelisted() is for informational purposes only.  The data
// is returned without signatures.  To determine if a user is allowed to log in
// to the device, YOU MUST use CheckWhitelist and verify the signature that is
// returned.
extern bool (*EnumerateWhitelisted)(std::vector<std::string>* OUT_whitelisted);
extern bool (*RestartJob)(int pid, const char* command_line);
extern bool (*RetrieveProperty)(const char* name,
                                std::string* OUT_value,
                                std::vector<uint8>* OUT_signature);
extern bool (*SetOwnerKey)(const std::vector<uint8>& public_key_der);
extern bool (*StartSession)(const char* user_email,
                            const char* unique_id /* unused */);
extern bool (*StopSession)(const char* unique_id /* unused */);
extern bool (*StoreProperty)(const char* name,
                             const char* value,
                             const std::vector<uint8>& signature);
extern bool (*Unwhitelist)(const char* email,
                           const std::vector<uint8>& signature);
extern bool (*Whitelist)(const char* email,
                         const std::vector<uint8>& signature);

}  // namespace chromeos

#endif  // CHROMEOS_LOGIN_H_
