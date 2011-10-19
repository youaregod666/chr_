// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_BRIGHTNESS_H_
#define CHROMEOS_BRIGHTNESS_H_

namespace chromeos {

// Signature of function invoked to handle brightness changes.  Arguments are
// the object passed to MonitorBrightness(), the current brightness level
// (between 0 and 100, inclusive), and whether the change was user-initiated
// (i.e. caused by the brightness keys) or not.
typedef void (*BrightnessMonitorFunctionV2)(void*, int, bool);
typedef void (*BrightnessMonitorFunction)(void*, int);  // DEPRECATED

class OpaqueBrightnessConnection;
typedef OpaqueBrightnessConnection* BrightnessConnection;

}  // namespace chromeos

#endif  // CHROMEOS_BRIGHTNESS_H_
