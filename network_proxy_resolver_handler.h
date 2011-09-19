// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TODO(satorux): Remove this file. DEPRECATED.

#ifndef PLATFORM_CROS_NETWORK_PROXY_RESOLVER_HANDLER_H_
#define PLATFORM_CROS_NETWORK_PROXY_RESOLVER_HANDLER_H_
#pragma once

#include <vector>

#include <base/basictypes.h>
#include <chromeos/dbus/dbus.h>
#include <cros/chromeos_libcros_service.h>

namespace chromeos {

class NetworkProxyResolverHandler {
 public:
  NetworkProxyResolverHandler();
  virtual ~NetworkProxyResolverHandler();

  // Methods via dbus invocation.
  gboolean ResolveProxy(gchar* source_url, gchar* signal_interface,
                        gchar* signal_name, GError** error);

  // Methods via direct invocation.
  void SetHandler(NetworkProxyResolver handler, void* object);
  bool NotifyProxyResolved(const char* source_url, const char* proxy_list,
                           const char* resolved_error);

 private:
  // Structure to store information for one proxy resolution.
  struct Request {
    Request(const char* in_source_url, const char* in_signal_interface,
            const char* in_signal_name)
        : source_url(in_source_url),
          signal_interface(in_signal_interface),
          signal_name(in_signal_name) {}
    ~Request() {}

    const std::string source_url;
    const std::string signal_interface;
    const std::string signal_name;
  };

  NetworkProxyResolver handler_;
  void* object_;
  std::vector<Request*> all_requests_;
};

}  // namespace chromeos

#endif  // PLATFORM_CROS_NETWORK_PROXY_RESOLVER_HANDLER_H_
