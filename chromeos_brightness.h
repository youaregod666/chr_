// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_BRIGHTNESS_H_
#define CHROMEOS_BRIGHTNESS_H_

namespace chromeos {

// Signature of function invoked to handle brightness changes.  Arguments are
// the object passed to MonitorBrightness() and the current brightness level
// (between 0 and 100, inclusive).  This is currently only invoked in response
// to user-requested brightness changes, i.e. caused by the brightness keys.
typedef void (*BrightnessMonitorFunction)(void*, int);

class OpaqueBrightnessConnection;
typedef OpaqueBrightnessConnection* BrightnessConnection;

// Register a handler that will be called when the screen brightness changes.
extern BrightnessConnection (*MonitorBrightness)(
    BrightnessMonitorFunction monitor_function,
    void* object);

// Unregister the handler.  Takes the BrightnessConnection returned by
// MonitorBrightness().
extern void (*DisconnectBrightness)(BrightnessConnection connection);

}  // namespace chromeos

#endif  // CHROMEOS_BRIGHTNESS_H_
