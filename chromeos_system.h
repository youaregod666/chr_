// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SYSTEM_H_
#define CHROMEOS_SYSTEM_H_

#include <string>

namespace chromeos {

// Returns the current timezone ID, such as "US/Pacific".
// Returns an empty string if there's an error.
extern std::string (*GetTimezoneID)();

// Sets the current timezone ID.
extern void (*SetTimezoneID)(const std::string& id);

struct MachineInfo {
  struct NVPair {
    char* name;
    char* value;
  };
  int name_value_size;
  NVPair* name_values;
};

// Returns the system hardware info.
// The MachineInfo instance that is returned by this function MUST be
// deleted by calling FreeMachineInfo.
//
// Returns NULL on error.
extern MachineInfo* (*GetMachineInfo)();

// Deletes a MachineInfo type that was allocated in the ChromeOS dll. We need
// to do this to safely pass data over the dll boundary between our .so and
// Chrome.
extern void (*FreeMachineInfo)(MachineInfo* info);


}  // namespace chromeos

#endif  // CHROMEOS_SYSTEM_H_
