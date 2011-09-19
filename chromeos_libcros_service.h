// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_CROS_CHROMEOS_LIBCROS_SERVICE_H_
#define PLATFORM_CROS_CHROMEOS_LIBCROS_SERVICE_H_
#pragma once

#include <string>
#include <base/basictypes.h>

namespace chromeos {

class LibCrosService;
typedef LibCrosService* LibCrosServiceConnection;

// The expected callback signature provided by chrome which will
// be invoked by LibCrosService::ResolveNetworkProxy.
// |object| given when calling SetNetworkProxyResolverHandler will be passed
// when calling callback.
typedef void (*NetworkProxyResolver)(void* object,
                                     const char* source_url);

} // namespace chromeos

#endif  // PLATFORM_CROS_CHROMEOS_LIBCROS_SERVICE_H_
