// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LOGIN_H_
#define CHROMEOS_LOGIN_H_

namespace chromeos { // NOLINT

static const char kOwnerKeyFile[] = "/var/lib/whitelist/owner.key";

extern bool (*EmitLoginPromptReady)();
extern bool (*StartSession)(const char* user_email,
                            const char* unique_id /* unused */);
extern bool (*StopSession)(const char* unique_id /* unused */);
extern bool (*RestartJob)(int pid, const char* command_line);

}  // namespace chromeos

#endif  // CHROMEOS_LOGIN_H_
