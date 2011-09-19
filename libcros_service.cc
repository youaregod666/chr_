// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TODO(satorux): Remove this file.

#include "libcros_service.h"

#include <base/string_util.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/dbus/service_constants.h>
#include <cros/chromeos_cros_api.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <glib.h>

#include "libcros_servicer.h"
#include "network_proxy_resolver_handler.h"

namespace chromeos {

#include "bindings_server.h"

LibCrosService::LibCrosService()
    : libcros_servicer_(NULL) {
}

LibCrosService::~LibCrosService() {
  Cleanup();
}

const char *LibCrosService::service_name() const {
  return kLibCrosServiceName;
}

const char *LibCrosService::service_path() const {
  return kLibCrosServicePath;
}

const char *LibCrosService::service_interface() const {
  return kLibCrosServiceInterface;
}

GObject *LibCrosService::service_object() const {
  return G_OBJECT(libcros_servicer_);
}

bool LibCrosService::Initialize() {
  // Installs the type-info for the service with dbus.
  dbus_g_object_type_install_info(libcros_servicer_get_type(),
                                  &dbus_glib_libcros_servicer_object_info);

  // Create handlers for each type of service.
  network_proxy_resolver_handler_.reset(new NetworkProxyResolverHandler);

  if (Reset()) {
    LOG(INFO) << "LibCrosService initialized";
    return true;
  }
  LOG(ERROR) << "Unable to initialize LibCrosService";
  return false;
}

bool LibCrosService::Reset() {
  Cleanup();

  libcros_servicer_ = reinterpret_cast<LibCrosServicer*>(
      g_object_new(libcros_servicer_get_type(), NULL));
  if (!libcros_servicer_) {
    LOG(ERROR) << "Error constructing LibCrosServicer object";
    return false;
  }
  // Allow references to this instance.
  libcros_servicer_->service = this;

  return true;
}

bool LibCrosService::Run() {
  LOG(FATAL) << "LibCrosService shouldn't run its own loop; "
             << "it should simply run in Chrome's UI loop";
  return false;
}

bool LibCrosService::Shutdown() {
  LOG(FATAL) << "LibCrosService shouldn't have its own loop to shutdown; "
             << "it should have simply run in Chrome's UI loop";
  return false;
}

//--------------------------- Network Proxy Resolver ---------------------------

gboolean LibCrosService::ResolveNetworkProxy(gchar* source_url,
                                             gchar* signal_interface,
                                             gchar* signal_name,
                                             GError** error) {
  return network_proxy_resolver_handler_->ResolveProxy(source_url,
                                                       signal_interface,
                                                       signal_name, error);
}

void LibCrosService::SetNetworkProxyResolver(
    chromeos::NetworkProxyResolver handler, void* object) {
  network_proxy_resolver_handler_->SetHandler(handler, object);
}

bool LibCrosService::NotifyNetworkProxyResolved(const char* source_url,
                                                const char* proxy_list,
                                                const char* resolved_error) {
  return network_proxy_resolver_handler_->NotifyProxyResolved(source_url,
      proxy_list, resolved_error);
}

//--------------------------------- private ------------------------------------

void LibCrosService::Cleanup() {
  if (libcros_servicer_) {
    g_object_unref(libcros_servicer_);
    libcros_servicer_ = NULL;
  }
}

}  // namespace chromeos
