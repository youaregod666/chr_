// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TODO(satorux): Remove this file. DEPRECATED.

#include "network_proxy_resolver_handler.h"

#include <base/string_util.h>
#include <cros/chromeos_cros_api.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <glib.h>

namespace chromeos {

const char* kSignalPath = "/";

NetworkProxyResolverHandler::NetworkProxyResolverHandler()
    : handler_(NULL),
      object_(NULL) {
}

NetworkProxyResolverHandler::~NetworkProxyResolverHandler() {
  if (!all_requests_.empty()) {
    for (size_t i = all_requests_.size() - 1; i >= 0; --i) {
      LOG(WARNING) << "Pending request for " << all_requests_[i]->source_url;
      delete all_requests_[i];
    }
    all_requests_.clear();
  }
}

gboolean NetworkProxyResolverHandler::ResolveProxy(gchar* source_url,
                                                   gchar* signal_interface,
                                                   gchar* signal_name,
                                                   GError** error) {
  if (!handler_ || !object_) {
    g_set_error_literal(error, 0, 0, "No handler for proxy resolution");
    return false;
  }

  // Enqueue request and call the handler on it.
  all_requests_.push_back(new Request(source_url, signal_interface,
                                      signal_name));
  handler_(object_, source_url);
  return true;
}

void NetworkProxyResolverHandler::SetHandler(NetworkProxyResolver handler,
                                             void* object) {
  handler_ = handler;
  object_ = object;
}

bool NetworkProxyResolverHandler::NotifyProxyResolved(const char* source_url,
    const char* proxy_list, const char* resolved_error) {
  std::string error;
  Request* request = NULL;
  DBusMessage* signal = NULL;
  bool success = true;
  do {
    // Retrieve |Request| from all_requests_ with associated |source_url|.
    for (std::vector<Request*>::iterator iter = all_requests_.begin();
         iter != all_requests_.end(); ++iter) {
      if ((*iter)->source_url == source_url) {
        request = *iter;
        all_requests_.erase(iter);
        break;
      }
    }
    if (!request) {
      error = StringPrintf("Can't find url %s in list of requests", source_url);
    } else if (resolved_error && resolved_error[0]) {
      error = StringPrintf("Resolution error for url %s: %s", source_url,
                           resolved_error);
    }
    // Create proxy to send signal on.
    dbus::Proxy proxy(dbus::GetSystemBusConnection(), kSignalPath,
                      request->signal_interface.c_str());
    if (!proxy) {
      LOG(ERROR) << "Can't create proxy for interface "
                 << request->signal_interface << "; can't signal";
      success = false;
      break;
    }
    // Create signal with |signal_name| to be emitted on |signal_interface|.
    signal = ::dbus_message_new_signal(kSignalPath,
                                       request->signal_interface.c_str(),
                                       request->signal_name.c_str());
    if (!signal) {
      LOG(ERROR) << "Can't create signal " << request->signal_name
                 << " for " << source_url
                 << " on interface " << request->signal_interface;
      success = false;
      break;
    }
    // Append arguments, |proxy_list| and |error|, to signal.
    const char* error_str = error.c_str();
    if (!::dbus_message_append_args(signal,
                                    DBUS_TYPE_STRING, &source_url,
                                    DBUS_TYPE_STRING, &proxy_list,
                                    DBUS_TYPE_STRING, &error_str,
                                    DBUS_TYPE_INVALID)) {
      LOG(ERROR) << "Can't append args to signal " << request->signal_name
                 << " for " << source_url
                 << " on interface " << request->signal_interface;
      success = false;
      break;
    }
    // Send signal to requester on |signal_interface|.
    ::dbus_g_proxy_send(proxy.gproxy(), signal, NULL);
    LOG(INFO) << "Sent signal " << request->signal_name << " for " << source_url
              << " on interface " << request->signal_interface;
  } while (0);

  // Free request and signal.
  delete request;
  if (signal)
    ::dbus_message_unref(signal);

  return success;
}

}  // namespace chromeos
