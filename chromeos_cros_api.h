// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CROS_API_VERSION_H_
#define CHROMEOS_CROS_API_VERSION_H_

#include <string>

// This file defines two version numbers for the CrosAPI.
//
// We have two components, the libcros.so (so) and the libcros API which is
// built into chrome (app). The version of the two components may not match.
// The so will support any app version in the range
// [kCrosAPIMinVersion, kCrosAPIVersion]. Currently the app will not support
// older versions of the so.

// The expected workflow is that the so is updated to be backward compatible
// by at least one version of the app. This allows the so to be updated
// then the app without breaking either build. Support for older than n-1
// versions should only be done if there isn't any advantage to breaking the
// older versions (for example, cleaning out otherwise dead code).

// Removing an API method is a bit tricky. It takes a few steps:
// 1) First remove all calls to that method in Chrome.
// 2) Deprecate the method but keep the method implementation. Remove the
//      binding of the method in load.cc and increment kCrosAPIVersion in this
//      file. Check this in.
// 3) Once ChromeOS looks good, update cros_deps/DEPS in Chrome source tree.
//      This new libcros.so will work with new Chrome (which does not bind with
//      the deprecated method) and it will still work with an older Chrome
//      (which does bind with the deprecated method but does not call it).
// 4) Wait until all versions of Chrome (i.e. the binary release of Chrome)
//      is using this new libcros.so.
// 5) Now, delete the method implementation. Increment kCrosAPIVersion and set
//      kCrosAPIMinVersion to what kCrosAPIVersion was. Check this in.
// 6) Once ChromeOS looks good, update cros_deps/DEPS in Chrome source tree.
//      This new libcros.so will work with both the new Chrome and the one
//      version back Chrome, since neither Chrome will be trying to bind to
//      this now deleted method.

// It is not recommended to change the signature of a method as that can never
// be backwards compatible. So just name it differently. Add the new method and
// remove the old method as described above.

// Current version numbers:
//  0: Version number prior to the version scheme.
//  1: Added CrosVersionCheck API.
//     Changed load to take file path instead of so handle.
//  2: Changed the interface for network monitoring callbacks.
//  3: Added support disconnecting the network monitor.
//  4: Added Update API
//  5: Added IPConfig code
//  6: Deprecated GetIPConfigProperty and SetIPConfigProperty.

namespace chromeos {  // NOLINT

enum CrosAPIVersion {
  kCrosAPIMinVersion = 4,
  kCrosAPIVersion = 6
};

// Default path to pass to LoadCros: "/opt/google/chrome/chromeos/libcros.so"
extern char const * const kCrosDefaultPath;

// TODO(davemoore) Vestigial API. Remove when Chrome is calling the new API.
bool LoadCros(const char* path_to_libcros);

// |path_to_libcros| is the path to the libcros.so file.
// Returns true to indicate success.
// If returns false, |load_error| will contain a string describing the
// problem.
bool LoadLibcros(const char* path_to_libcros, std::string& load_error);

}  // namespace chromeos

#endif /* CHROMEOS_CROS_API_VERSION_H_ */

