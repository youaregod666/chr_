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

}  // namespace chromeos

#endif  // CHROMEOS_SYSTEM_H_
