// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_CHROMEOS_SCREEN_LOCK_H_
#define CROS_CHROMEOS_SCREEN_LOCK_H_

#include <base/basictypes.h>

namespace chromeos {

class OpaqueScreenLockConnection;
typedef OpaqueScreenLockConnection* ScreenLockConnection;
typedef void(*ScreenLockMonitor)(void*);

extern ScreenLockConnection (*MonitorScreenLock)
    (ScreenLockMonitor monitor, void*);
extern void (*DisconnectScreenLock)(ScreenLockConnection connection);

// Notifies PowerManager that the screen lock has been completed.
extern void (*NotifyScreenLockCompleted)();

// Notifies PowerManager that the a user requested to lock the
// screen (ctrl-l).
extern void (*NotifyScreenLockRequested)();

// Notifies PowerManager that the a user unlocked the screen.
extern void (*NotifyScreenUnlocked)();

}  // namespace chromeos

#endif  // CROS_CHROMEOS_SCREEN_LOCK_H_
