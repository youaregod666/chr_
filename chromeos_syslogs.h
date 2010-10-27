// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SYSLOGS_H_
#define CHROMEOS_SYSLOGS_H_

#include <string>
#include <map>

#include <base/file_path.h>

namespace chromeos { //NOLINT

typedef std::map<std::string, std::string> LogDictionaryType;

// Retrieve system logs by running the system log scripts
// If |zip_file_name| is provided, it will be used to store the content of
// compressed logs.
//
// The context parameter is a string that will be passed down to the syslog
// collection script - the valid values are "sysinfo" to indicate that these
// logs are being requested for the purpose of displaying on chrome://system;
// the other valid parameter is "feedback", which indicates that these logs
// will be used to send feedback - though currently any value can be passed
// to this parameter; this is by intention, to allow further context types
// to be added later without needing to change libcros. The script will dfault
// to "sysinfo" in case of an invalid value.
//
// Note: The returned objects must be freed by the caller
//       including the temp filename (if not previously allocated)
//
//       Additionally, the log file created needs to be cleaned up
//       by the caller
extern LogDictionaryType* (*GetSystemLogs)(FilePath* zip_file_name,
                                           const std::string& context);
}  // namespace chromeos

#endif  // CHROMEOS_SYSLOGS_H_
