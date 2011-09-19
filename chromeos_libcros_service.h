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

// Starts LibCrosService running in chrome executable as a DBus service.
// Returns the services that is used for all subsequent interactions with
// any functionalities exposed by chrome for chromeos.
// E.g. The update engine can send dbus requests to the service to resolve
// network proxy for a URL and subscribe to signal where the proxy list will be
// returned.
extern LibCrosServiceConnection (*StartLibCrosService)();

// Stops |connection| which was started via StartLibCrosService.
extern void (*StopLibCrosService)(LibCrosServiceConnection connection);

// Network Proxy Resolver

// The expected callback signature provided by chrome which will
// be invoked by LibCrosService::ResolveNetworkProxy.
// |object| given when calling SetNetworkProxyResolverHandler will be passed
// when calling callback.
typedef void (*NetworkProxyResolver)(void* object,
                                     const char* source_url);

// Sets handler for NetworkProxyResolver in LibCrosService in |connection|.
// |handler| is the callback function for method ResolveNetworkProxy in
// LibCrosService invoked via dbus request.
extern void (*SetNetworkProxyResolver)(NetworkProxyResolver handler,
                                       void* handler_object,
                                       LibCrosServiceConnection connection);

// Notifies clients of LibCrosService in |connection| that network proxy
// resolution for |source_url| has completed via emission of signal
// "proxy_resolved".
// |proxy_list| contains result of proxy resolution.
// |error| contains the error if there's proxy resolution fails.
extern bool (*NotifyNetworkProxyResolved)(const char* source_url,
                                          const char* proxy_list,
                                          const char* error,
                                          LibCrosServiceConnection connection);

} // namespace chromeos

#endif  // PLATFORM_CROS_CHROMEOS_LIBCROS_SERVICE_H_
