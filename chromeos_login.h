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


extern SessionConnection (*MonitorSession)(SessionMonitor monitor, void*);
extern void (*DisconnectSession)(SessionConnection connection);

// Kicks off an attempt to emit the "login-prompt-ready" upstart signal.
extern bool (*EmitLoginPromptReady)();

// Kicks off an attempt to emit the "login-prompt-visible" upstart signal.
extern bool (*EmitLoginPromptVisible)();

extern bool (*RestartJob)(int pid, const char* command_line);

extern bool (*RestartEntd)();

// Fetches the policy blob stored by the session manager.
// Upon completion of the retrieve attempt, we will call the provided callback.
// Policies are serialized protocol buffers.  Upon success, we will pass a
// protobuf to the callback.  On failure, we will pass NULL.
extern void (*RetrievePolicy)(RetrievePolicyCallback callback, void* delegate);

extern bool (*StartSession)(const char* user_email,
                            const char* unique_id /* unused */);

extern bool (*StopSession)(const char* unique_id /* unused */);

// Attempts to store the policy blob |prop| asynchronously.
// Takes |len| because |prop| may have embedded NULL characters.
// Upon completion of the store attempt, we will call callback(delegate, ...)
extern void (*StorePolicy)(const char* prop,
                           const unsigned int len,
                           StorePolicyCallback callback,
                           void* delegate);
}  // namespace chromeos

#endif  // CHROMEOS_LOGIN_H_
