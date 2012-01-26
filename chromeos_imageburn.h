// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_CROS_CHROMEOS_IMAGEBURN_H_
#define PLATFORM_CROS_CHROMEOS_IMAGEBURN_H_

#include <string>
#include <base/basictypes.h>

namespace chromeos {

struct BurnStatus{
  const char* target_path;
  int64 amount_burnt;
  int64 total_size;
  const char* error;
};

enum BurnEventType{
  BURN_STARTED,
  BURN_UPDATED,
  BURN_CANCELED,
  BURN_COMPLETE
};

class OpaqueBurnStatusConnection;
typedef OpaqueBurnStatusConnection* BurnStatusConnection;

// The expected callback signature that will be provided by the client who
// calls MonitorBurnStatus. Object given while calling MonitorBurnStatus, status
// of burning process and burning event will be passed when calling a callback.
typedef void(*BurnMonitor)(void*,
                           const BurnStatus&,
                           BurnEventType);
} // namespace chromeos
#endif  // PLATFORM_CROS_CHROMEOS_IMAGEBURN_H_
