// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_imageburn.h"

#include <base/memory/scoped_ptr.h>
#include <chromeos/dbus/abstract_dbus_service.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/dbus/service_constants.h>

#include "marshal.glibmarshal.h"

namespace chromeos {

class OpaqueBurnStatusConnection {
 public:
  typedef dbus::MonitorConnection<void (const char*, int64, int64)>*
      ConnectionUpdateType;
  typedef dbus::MonitorConnection<void (const char*, bool, const char*)>*
      ConnectionFinishedType;

  OpaqueBurnStatusConnection(const BurnMonitor& monitor,
                             const dbus::Proxy& burn_proxy,
                             void* object)
     : monitor_(monitor),
       object_(object),
       burn_proxy_(burn_proxy),
       updatedconnection_(NULL),
       finishedconnection_(NULL) {
  }

  void FireEvent(BurnEventType evt, const char* path, int64 amount_burnt,
                 int64 total_size, const char* error) {
    BurnStatus info;
    info.target_path = path;
    info.amount_burnt = amount_burnt;
    info.total_size = total_size;
    info.error = error;

    monitor_(object_, info, evt);
  }

  static void Updated(void* object, const char* target_path,
                      int64 amount_burnt, int64 total_size) {
    BurnStatusConnection self = static_cast<BurnStatusConnection>(object);
    self->FireEvent(BURN_UPDATED, target_path, amount_burnt, total_size, "");
  }

  static void Finished(void* object, const char* target_path, bool success,
                        const char* error ) {
    BurnStatusConnection self = static_cast<BurnStatusConnection>(object);
    if(success) {
      self->FireEvent(BURN_COMPLETE, target_path, 0, 0, "");
    } else {
      self->FireEvent(BURN_CANCELED, target_path, 0, 0, error);
    }
  }

  ConnectionUpdateType& updateconnection() {
    return updatedconnection_;
  }

  ConnectionFinishedType& finishedconnection() {
    return finishedconnection_;
  }

  void StartBurn(const char* from_path, const char* to_path) {
    glib::ScopedError error;

    if (!::dbus_g_proxy_call(burn_proxy_.gproxy(),
                             imageburn::kBurnImage,
                             &Resetter(&error).lvalue(),
                             G_TYPE_STRING, from_path,
                             G_TYPE_STRING, to_path,
                             G_TYPE_INVALID,
                             G_TYPE_INVALID)) {
      LOG(WARNING) << "Burn operation unable to start: "
          << (error->message ? error->message : "Unknown Error.");
      Finished(this, to_path, false, error->message);
    }
  }

 private:
  BurnMonitor monitor_;
  void* object_;
  dbus::Proxy burn_proxy_;
  ConnectionUpdateType updatedconnection_;
  ConnectionFinishedType finishedconnection_;
};

extern "C"
BurnStatusConnection ChromeOSMonitorBurnStatus(BurnMonitor monitor,
                                               void* object) {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy burn_proxy(bus,
                         imageburn::kImageBurnServiceName,
                         imageburn::kImageBurnServicePath,
                         imageburn::kImageBurnServiceInterface);
  BurnStatusConnection result =
      new OpaqueBurnStatusConnection(monitor, burn_proxy, object);

  typedef dbus::MonitorConnection<void (const char*, int64, int64)>
      ConnectionUpdateType;
  typedef dbus::MonitorConnection<void (const char*, bool, const char*)>
      ConnectionFinishedType;

  // Adding update signal and connection.
  ::dbus_g_object_register_marshaller(marshal_VOID__STRING_INT64_INT64,
                                      G_TYPE_NONE,
                                      G_TYPE_STRING,
                                      G_TYPE_INT64,
                                      G_TYPE_INT64,
                                      G_TYPE_INVALID);

  ::dbus_g_proxy_add_signal(burn_proxy.gproxy(),
                            imageburn::kSignalBurnUpdateName,
                            G_TYPE_STRING,
                            G_TYPE_INT64,
                            G_TYPE_INT64,
                            G_TYPE_INVALID);

  ConnectionUpdateType* updated = new ConnectionUpdateType(burn_proxy,
      imageburn::kSignalBurnUpdateName, &OpaqueBurnStatusConnection::Updated,
      result);

  ::dbus_g_proxy_connect_signal(burn_proxy.gproxy(),
                                imageburn::kSignalBurnUpdateName,
                                G_CALLBACK(&ConnectionUpdateType::Run),
                                updated, NULL);

  result->updateconnection() = updated;

  // Adding end signal and connection.
  ::dbus_g_object_register_marshaller(marshal_VOID__STRING_BOOLEAN_STRING,
                                      G_TYPE_NONE,
                                      G_TYPE_STRING,
                                      G_TYPE_BOOLEAN,
                                      G_TYPE_STRING,
                                      G_TYPE_INVALID);

  ::dbus_g_proxy_add_signal(burn_proxy.gproxy(),
                            imageburn::kSignalBurnFinishedName,
                            G_TYPE_STRING,
                            G_TYPE_BOOLEAN,
                            G_TYPE_STRING,
                            G_TYPE_INVALID);

  ConnectionFinishedType* finished = new ConnectionFinishedType(burn_proxy,
      imageburn::kSignalBurnFinishedName,
      &OpaqueBurnStatusConnection::Finished, result);

  ::dbus_g_proxy_connect_signal(burn_proxy.gproxy(),
                                imageburn::kSignalBurnFinishedName,
                                G_CALLBACK(&ConnectionFinishedType::Run),
                                finished, NULL);

  result->finishedconnection() = finished;

  return result;
}

extern "C"
void ChromeOSDisconnectBurnStatus(BurnStatusConnection connection) {
  dbus::Disconnect(connection->updateconnection());
  dbus::Disconnect(connection->finishedconnection());
  delete connection;
}

extern "C"
void ChromeOSStartBurn(const char* from_path, const char* to_path,
               BurnStatusConnection connection) {
  connection->StartBurn(from_path, to_path);
}

}  // namespace chromeos
