// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
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

// Processes a callback from a d-bus signal. Caller provides a callback function
// monitor that will be called to process callback. object will be passed as one
// of arguments to callback function,
extern BurnStatusConnection (*MonitorBurnStatus)(BurnMonitor monitor, 
                                                 void* object);

// Disconnects a listener from the burning events (connection).
extern void (*DisconnectBurnStatus)(BurnStatusConnection connection);

// Initiates image burning. Image located on path from_path is burnt to a device
// on to_path. BurnStatusConnection is not used anymore. It is left here due to
// back-compatibility.
extern void (*RequestBurn)(const char* from_path, const char* to_path,
                           BurnMonitor callback, void* user_data);

} // namespace chromeos
#endif  // PLATFORM_CROS_CHROMEOS_IMAGEBURN_H_
