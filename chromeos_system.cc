// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_system.h"

#include <base/file_path.h>
#include <base/file_util.h>
#include <base/logging.h>

namespace chromeos {  // NOLINT

namespace { // NOLINT

// The filepath to the timezone file that symlinks to the actual timezone file.
static const char kTimezoneSymlink[] = "/var/lib/timezone/localtime";
static const char kTimezoneSymlink2[] = "/var/lib/timezone/localtime2";

// The directory that contains all the timezone files. So for timezone
// "US/Pacific", the actual timezone file is: "/usr/share/zoneinfo/US/Pacific"
static const char kTimezoneFilesDir[] = "/usr/share/zoneinfo/";

}  // namespace

extern "C"
std::string ChromeOSGetTimezoneID() {
  // Look at kTimezoneSymlink, see which timezone we are symlinked to.
  char buf[256];
  const ssize_t len = readlink(kTimezoneSymlink, buf,
                               sizeof(buf)-1);
  if (len == -1) {
    LOG(ERROR) << "GetTimezoneID: Cannot read timezone symlink "
               << kTimezoneSymlink;
    return std::string();
  }

  std::string timezone(buf, len);
  // Remove kTimezoneFilesDir from the beginning.
  if (timezone.find(kTimezoneFilesDir) != 0) {
    LOG(ERROR) << "GetTimezoneID: Timezone symlink is wrong "
               << timezone;
    return std::string();
  }

  return timezone.substr(strlen(kTimezoneFilesDir));
}

extern "C"
void ChromeOSSetTimezoneID(const std::string& id) {
  // Change the kTimezoneSymlink symlink to the path for this timezone.
  // We want to do this in an atomic way. So we are going to create the symlink
  // at kTimezoneSymlink2 and then move it to kTimezoneSymlink

  FilePath timezone_symlink(kTimezoneSymlink);
  FilePath timezone_symlink2(kTimezoneSymlink2);
  FilePath timezone_file(kTimezoneFilesDir + id);

  // Make sure timezone_file exists.
  if (!file_util::PathExists(timezone_file)) {
    LOG(ERROR) << "SetTimezoneID: Cannot find timezone file "
               << timezone_file.value();
    return;
  }

  // Delete old symlink2 if it exists.
  file_util::Delete(timezone_symlink2, false);

  // Create new symlink2.
  if (symlink(timezone_file.value().c_str(),
              timezone_symlink2.value().c_str()) == -1) {
    LOG(ERROR) << "SetTimezoneID: Unable to create symlink "
               << timezone_symlink2.value() << " to " << timezone_file.value();
    return;
  }

  // Move symlink2 to symlink.
  if (!file_util::ReplaceFile(timezone_symlink2, timezone_symlink)) {
    LOG(ERROR) << "SetTimezoneID: Unable to move symlink "
               << timezone_symlink2.value() << " to "
               << timezone_symlink.value();
  }
}

}  // namespace chromeos
