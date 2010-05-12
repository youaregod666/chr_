// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SYSLOGS_H_
#define CHROMEOS_SYSLOGS_H_

#include <string>
#include <map>

namespace chromeos { //NOLINT

typedef std::map<std::string, std::string> LogDictionaryType;

// Retrieve system logs by running the system log scripts
// If tmpfilename is provided, it will be used, otherwise
// a new filename will be returned in the tmpfilename parameter
//
// Note: The returned objects must be freed by the caller
//       including the temp filename (if not previously allocated)
//
//       Additionally, the log file created needs to be cleaned up
//       by the caller
extern LogDictionaryType* (*GetSystemLogs)(char** const tmpfilename);

}  // namespace chromeos

#endif  // CHROMEOS_SYSLOGS_H_
