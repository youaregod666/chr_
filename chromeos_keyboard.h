// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_KEYBOARD_H_
#define CHROMEOS_KEYBOARD_H_

#include <string>

namespace chromeos {

// Returns the current layout name like "us". On error, returns "".
extern std::string (*GetCurrentKeyboardLayoutName)();

// Sets the current keyboard layout to |layout_name|.  Returns true on
// success.
extern bool (*SetCurrentKeyboardLayoutByName)(const std::string& layout_name);

// Gets whehter we have separate keyboard layout per window, or not. The
// result is stored in |is_per_window|.  Returns true on success.
extern bool (*GetKeyboardLayoutPerWindow)(bool* is_per_window);

// Sets whether we have separate keyboard layout per window, or not. If false
// is given, the same keyboard layout will be shared for all applications.
// Returns true on success.
extern bool (*SetKeyboardLayoutPerWindow)(bool is_per_window);

}  // namespace chromeos

#endif  // CHROMEOS_KEYBOARD_H_
