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

  // Deprecated.
  void DoBurn(const char* from_path, const char* to_path,
              const char** devices_to_unmount) {
    LOG(ERROR) << "Deprecated method call";
    NOTREACHED();
  }

 private:
  BurnMonitor monitor_;
  void* object_;
  dbus::Proxy burn_proxy_;
  ConnectionUpdateType updatedconnection_;
  ConnectionFinishedType finishedconnection_;
};

struct BurnCallbackData {
  BurnCallbackData(const char* target_path,
                   BurnMonitor cb,
                   void* obj)
    : proxy(new dbus::Proxy(dbus::GetSystemBusConnection(),
            imageburn::kImageBurnServiceName,
            imageburn::kImageBurnServicePath,
            imageburn::kImageBurnServiceInterface)),
      callback(cb),
      object(obj),
      callback_target_path(target_path) {
  }

  scoped_ptr<dbus::Proxy> proxy;
  BurnMonitor callback;
  void* object;
  std::string callback_target_path;
};

void DeleteBurnCallbackData(void* user_data) {
  BurnCallbackData* cb_data = reinterpret_cast<BurnCallbackData*>(user_data);
  delete cb_data;
}

void OnStartBurnFailed(const char* target_path, const char* error,
    BurnMonitor callback, void* object) {
  BurnStatus info;
  info.target_path = target_path;
  info.amount_burnt = 0;
  info.total_size = 0;
  callback(object, info, BURN_CANCELED);
}

void BurnImageRequestNotify(DBusGProxy* gproxy,
                            DBusGProxyCall* call_id,
                            void* user_data) {
  BurnCallbackData* cb_data =
      reinterpret_cast<BurnCallbackData*>(user_data);
  DCHECK(cb_data);
  glib::ScopedError error;
  if (!::dbus_g_proxy_end_call(gproxy,
                               call_id,
                               &Resetter(&error).lvalue(),
                               G_TYPE_INVALID)) {
    LOG(WARNING) << "BurnImageNotify for path: '"
                << cb_data->callback_target_path << "' error: "
                << (error->message ? error->message : "Unknown Error.");
    std::string err = "Image burn failed: ";
    err = err.append(error->message ? error->message : "Unknown Error");
    OnStartBurnFailed(cb_data->callback_target_path.c_str(), err.c_str(),
        cb_data->callback, cb_data->object);
  } else {
    // Nothing. Image burn service will send status update messages.
  }
}

void DoBurnAsync(const char* from_path, const char* to_path,
    BurnMonitor callback, void* object) {
  scoped_ptr<BurnCallbackData> cb_data(
     new  BurnCallbackData(to_path, callback, object));
  //We need this temp because cb_data is being released in the argument list.
  dbus::Proxy* proxy = cb_data->proxy.get();
  glib::ScopedError error;
  DBusGProxyCall* call_id =
      ::dbus_g_proxy_begin_call(proxy->gproxy(),
                                imageburn::kBurnImage,
                                &BurnImageRequestNotify,
                                cb_data.release(),
                                &DeleteBurnCallbackData,
                                G_TYPE_STRING, from_path,
                                G_TYPE_STRING, to_path,
                                G_TYPE_INVALID);
  if(!call_id) {
    LOG(ERROR) << "StartBurn failed";
    OnStartBurnFailed(to_path, "StartBurn failed", callback, object);
  }
}

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

// TODO(satorux): Remove this. DEPRECATED.
extern "C"
void ChromeOSStartBurn(const char* from_path, const char* to_path,
               BurnStatusConnection connection) {
}

extern "C"
void ChromeOSRequestBurn(const char* from_path, const char* to_path,
               BurnMonitor callback, void* user_data) {
  DoBurnAsync(from_path, to_path, callback, user_data);
}

}  // namespace chromeos
