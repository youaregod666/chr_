// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_TOUCHPAD_H_
#define CHROMEOS_TOUCHPAD_H_

namespace chromeos { // NOLINT

// Sets the touchpad sensitivity in range from 1 to 5.
extern void (*SetTouchpadSensitivity)(int value);
// Turns tap to click on / off.
extern void (*SetTouchpadTapToClick)(bool enabled);
}  // namespace chromeos

#endif  // CHROMEOS_TOUCHPAD_H_

