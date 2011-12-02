// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_CHROMEOS_SCREEN_LOCK_H_
#define CROS_CHROMEOS_SCREEN_LOCK_H_

#include <base/basictypes.h>

namespace chromeos {

enum ScreenLockEvent {
  LockScreen = 0,
  UnlockScreen = 1,
  UnlockScreenFailed = 2,
};

class OpaqueScreenLockConnection;
typedef OpaqueScreenLockConnection* ScreenLockConnection;
typedef void(*ScreenLockMonitor)(void*, ScreenLockEvent);

}  // namespace chromeos

#endif  // CROS_CHROMEOS_SCREEN_LOCK_H_
