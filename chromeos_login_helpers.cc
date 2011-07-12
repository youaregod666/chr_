// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_login_helpers.h"  // NOLINT

#include <base/basictypes.h>
#include <base/logging.h>
#include <base/string_util.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/dbus/service_constants.h>
#include <chromeos/glib/object.h>
#include <chromeos/string.h>
#include <vector>

namespace chromeos {  // NOLINT

ChromeOSLoginHelpers::ChromeOSLoginHelpers() {}

ChromeOSLoginHelpers::~ChromeOSLoginHelpers() {}

// static
dbus::Proxy ChromeOSLoginHelpers::CreateProxy() {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  return dbus::Proxy(bus,
                     login_manager::kSessionManagerServiceName,
                     login_manager::kSessionManagerServicePath,
                     login_manager::kSessionManagerInterface);
}

}  // namespace chromeos
