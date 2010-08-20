// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LOGIN_H_
#define CHROMEOS_LOGIN_H_

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
  SettingsOpSuccess = 4,
  SettingsOpFailure = 5,
};

static const char kOwnerKeyFile[] = "/var/lib/whitelist/owner.key";

class OpaqueSessionConnection;
typedef OpaqueSessionConnection* SessionConnection;
typedef void(*SessionMonitor)(void*, const OwnershipEvent&);

extern SessionConnection (*MonitorSession)(SessionMonitor monitor, void*);
extern void (*DisconnectSession)(SessionConnection connection);

extern bool (*EmitLoginPromptReady)();
extern bool (*SetOwnerKey)(const std::vector<uint8>& public_key_der);
extern bool (*StartSession)(const char* user_email,
                            const char* unique_id /* unused */);
extern bool (*StopSession)(const char* unique_id /* unused */);
extern bool (*RestartJob)(int pid, const char* command_line);

}  // namespace chromeos

#endif  // CHROMEOS_LOGIN_H_
