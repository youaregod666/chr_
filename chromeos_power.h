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
  BATTERY_STATE_CHARGING,
  BATTERY_STATE_DISCHARGING,
  BATTERY_STATE_EMPTY,
  BATTERY_STATE_FULLY_CHARGED
};

enum BatteryTechnology {
  BATTERY_TECHNOLOGY_UNKNOWN,
  BATTERY_TECHNOLOGY_LITHIUM_ION,
  BATTERY_TECHNOLOGY_LITHIUM_POLYMER,
  BATTERY_TECHNOLOGY_IRON_PHOSPHATE,
  BATTERY_TECHNOLOGY_LEAD_ACID,
  BATTERY_TECHNOLOGY_NICKEL_CADMIUM,
  BATTERY_TECHNOLOGY_NICKEL_METAL_HYDRIDE
};

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

struct PowerInformation {
  PowerStatus power_status;

  // Amount of energy, measured in Wh, in the battery when it's considered
  // empty.
  double battery_energy_empty;

  // Amount of energy, measured in Wh, in the battery when it's considered full.
  double battery_energy_full;

  // Amount of energy, measured in Wh, the battery is designed to hold when it's
  // considered full.
  double battery_energy_full_design;

  bool battery_is_rechargeable;  // [needed?]
  double battery_capacity;

  std::string battery_technology;  // [needed?]

  std::string battery_vendor;
  std::string battery_model;
  std::string battery_serial;

  std::string line_power_vendor;
  std::string line_power_model;
  std::string line_power_serial;

  std::string battery_state_string;
};

class OpaquePowerStatusConnection;
typedef OpaquePowerStatusConnection* PowerStatusConnection;
typedef void(*PowerMonitor)(void*, const PowerStatus&);

extern PowerStatusConnection (*MonitorPowerStatus)(PowerMonitor monitor, void*);
extern void (*DisconnectPowerStatus)(PowerStatusConnection connection);
extern bool (*RetrievePowerInformation)(PowerInformation* information);

extern void (*EnableScreenLock)(bool enable);

// Request restart of the system.
extern void (*RequestRestart)();

// Request shutdown of the system.
extern void (*RequestShutdown)();

}  // namespace chromeos

#endif  // CHROMEOS_POWER_H_
