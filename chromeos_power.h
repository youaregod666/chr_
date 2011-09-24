// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_POWER_H_
#define CHROMEOS_POWER_H_

#include <base/basictypes.h>
#include <string>

namespace chromeos {

enum BatteryState {
  BATTERY_STATE_UNKNOWN,
  BATTERY_STATE_FULLY_CHARGED
};

// Callback for GetIdleTime method.
typedef void (*GetIdleTimeCallback)(void* object,
                                    int64 time_idle_ms,
                                    bool success);

struct PowerStatus {
  bool line_power_on;

  // Amount of energy, measured in Wh, in the battery.
  double battery_energy;

  // Amount of energy being drained from the battery, measured in W. If
  // positive, the source is being discharged, if negative it's being charged.
  double battery_energy_rate;

  double battery_voltage;

  // Time in seconds until the battery is considered empty, 0 for unknown.
  ::int64 battery_time_to_empty;
  // Time in seconds until the battery is considered full. 0 for unknown.
  ::int64 battery_time_to_full;

  double battery_percentage;
  bool battery_is_present;  // [needed?]

  BatteryState battery_state;
};

class OpaquePowerStatusConnection;
typedef OpaquePowerStatusConnection* PowerStatusConnection;
typedef void(*PowerMonitor)(void*, const PowerStatus&);

extern PowerStatusConnection (*MonitorPowerStatus)(PowerMonitor monitor, void*);

extern void (*GetIdleTime)(GetIdleTimeCallback callback,
                           void* object);

extern void (*DisconnectPowerStatus)(PowerStatusConnection connection);

extern void (*EnableScreenLock)(bool enable);

// Request restart of the system.
extern void (*RequestRestart)();

// Request shutdown of the system.
extern void (*RequestShutdown)();

}  // namespace chromeos

#endif  // CHROMEOS_POWER_H_
