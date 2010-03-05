// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chromeos_cros_api.h"  // NOLINT

// Construct a path for the shared library. This example uses a local path
// but on chromeos the library is installed in:
//  "/opt/google/chrome/chromeos/libcros.so"
bool LoadCrosLibrary(const char** argv) {
  std::string app_path = argv[0];
  app_path.erase(app_path.begin() + app_path.find_last_of("/"), app_path.end());
  app_path += "/libcros.so";

  std::string error_string = std::string();
  bool success = chromeos::LoadLibcros(app_path.c_str(), error_string);
  DCHECK(success) << "LoadLibcros('" << app_path.c_str() << "') failed: "
                  << error_string;
  return true;
}
