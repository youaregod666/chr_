// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TODO(satorux): Remove this file.

#ifndef PLATFORM_CROS_LIBCROS_SERVICE_H_
#define PLATFORM_CROS_LIBCROS_SERVICE_H_
#pragma once

#include <base/basictypes.h>
#include <chromeos/dbus/abstract_dbus_service.h>
#include <cros/chromeos_libcros_service.h>

namespace chromeos {

struct LibCrosServicer;
class NetworkProxyResolverHandler;

// Provides a wrapper for exporting LibCrosServiceInterface to DBus.
// ::g_type_init() must be called before this class is used.
class LibCrosService : public dbus::AbstractDbusService {
 public:
  LibCrosService();
  virtual ~LibCrosService();

  // chromeos::dbus::AbstractDbusService implementation.
  virtual bool Initialize();
  virtual bool Reset();
  virtual bool Run();
  virtual bool Shutdown();
  virtual const char *service_name() const;
  virtual const char *service_path() const;
  virtual const char *service_interface() const;
  virtual GObject *service_object() const;

  // Network proxy resolver.
  // Methods via dbus invocation.
  gboolean ResolveNetworkProxy(gchar* source_url, gchar* signal_interface,
                               gchar* signal_name, GError** error);
  // Methods via direct invocation.
  void SetNetworkProxyResolver(chromeos::NetworkProxyResolver handler,
                               void* object);
  bool NotifyNetworkProxyResolved(const char* source_url,
                                  const char* proxy_list,
                                  const char* resolved_error);

 protected:
  virtual GMainLoop *main_loop() {
    LOG(FATAL) << "LibCrosService should not have its main loop; "
               << "it should use Chrome's UI loop";
    return NULL;
  }

 private:
  void Cleanup();

  LibCrosServicer* libcros_servicer_;

  scoped_ptr<NetworkProxyResolverHandler> network_proxy_resolver_handler_;
};

}  // namespace chromeos

#endif  // PLATFORM_CROS_LIBCROS_SERVICE_H_
