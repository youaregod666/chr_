// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include <base/basictypes.h>
#include <chromeos/dbus/dbus.h>
#include <glib.h>

#include "chromeos_login.h"  // NOLINT

namespace chromeos {  // NOLINT

class ChromeOSLoginHelpers {
 public:
  static dbus::Proxy CreateProxy();

 private:
  ChromeOSLoginHelpers();
  ~ChromeOSLoginHelpers();
  DISALLOW_COPY_AND_ASSIGN(ChromeOSLoginHelpers);
};

}  // namespace chromeos
