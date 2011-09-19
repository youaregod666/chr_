// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/dbus/abstract_dbus_service.h>
#include <chromeos/dbus/service_constants.h>

#include "libcros_service.h"

namespace chromeos {

// The expected callback signature provided by chrome which will
// be invoked by LibCrosService::ResolveNetworkProxy.
// |object| given when calling SetNetworkProxyResolverHandler will be passed
// when calling callback.
typedef void (*NetworkProxyResolver)(void* object,
                                     const char* source_url);

extern "C"
LibCrosServiceConnection ChromeOSStartLibCrosService() {
  LibCrosService* service = new LibCrosService;
  if (service->Initialize() &&
      service->Register(chromeos::dbus::GetSystemBusConnection())) {
    LOG(INFO) << "StartLibCrosService completed successfully.";
    return service;
  }
  delete service;
  LOG(ERROR) << "Error starting LibCrosService as service.";
  return NULL;
}

extern "C"
void ChromeOSStopLibCrosService(LibCrosServiceConnection connection) {
  delete connection;
}

extern "C"
void ChromeOSSetNetworkProxyResolver(NetworkProxyResolver handler, void* object,
                                     LibCrosServiceConnection connection) {
  connection->SetNetworkProxyResolver(handler, object);
}

extern "C"
bool ChromeOSNotifyNetworkProxyResolved(const char* source_url,
                                        const char* proxy_list,
                                        const char* resolved_error,
                                        LibCrosServiceConnection connection) {
  return connection->NotifyNetworkProxyResolved(source_url, proxy_list,
                                                resolved_error);
}

}  // namespace chromeos
