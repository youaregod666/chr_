// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_update_engine.h"
#include <cstring>
#include "chromeos/dbus/dbus.h"
#include "chromeos/string.h"
#include "marshal.glibmarshal.h"

extern "C" {
#include "chromeos/update_engine/update_engine.dbusclient.h"
}

namespace chromeos {

namespace {

const char* const kUpdateEngineServiceName = "org.chromium.UpdateEngine";
const char* const kUpdateEngineServicePath =
    "/org/chromium/UpdateEngine";
const char* const kUpdateEngineServiceInterface =
    "org.chromium.UpdateEngineInterface";

// This is the "virtualized" destructor for UpdateProgress.
void DestroyUpdateProgress(const UpdateProgress& x) {
  delete x.new_version_;
}

// Returns UPDATE_STATUS_ERROR on error.
UpdateStatusOperation UpdateStatusFromString(const char* str) {
  const char* const kPrefix = "UPDATE_STATUS_";
  if (strncmp(str, kPrefix, strlen(kPrefix)))
    return UPDATE_STATUS_ERROR;
  const char* const main_str = (str + strlen(kPrefix));
  if (!strcmp(main_str, "IDLE"))
    return UPDATE_STATUS_IDLE;
  if (!strcmp(main_str, "CHECKING_FOR_UPDATE"))
    return UPDATE_STATUS_CHECKING_FOR_UPDATE;
  if (!strcmp(main_str, "UPDATE_AVAILABLE"))
    return UPDATE_STATUS_UPDATE_AVAILABLE;
  if (!strcmp(main_str, "DOWNLOADING"))
    return UPDATE_STATUS_DOWNLOADING;
  if (!strcmp(main_str, "VERIFYING"))
    return UPDATE_STATUS_VERIFYING;
  if (!strcmp(main_str, "FINALIZING"))
    return UPDATE_STATUS_FINALIZING;
  if (!strcmp(main_str, "UPDATED_NEED_REBOOT"))
    return UPDATE_STATUS_UPDATED_NEED_REBOOT;
  if (!strcmp(main_str, "REPORTING_ERROR_EVENT"))
    return UPDATE_STATUS_REPORTING_ERROR_EVENT;
  return UPDATE_STATUS_ERROR;
}

const char* GetGErrorMessage(const GError* error) {
  if (!error)
    return "Unknown error.";
  return error->message;
}

}  // namespace {}

class OpaqueUpdateStatusConnection {
 public:
  OpaqueUpdateStatusConnection(UpdateMonitor monitor, void* monitor_data)
      : proxy_(dbus::GetSystemBusConnection(),
               kUpdateEngineServiceName,
               kUpdateEngineServicePath,
               kUpdateEngineServiceInterface),
        monitor_(monitor),
        monitor_data_(monitor_data) {
    if (!marshaller_registered_) {
      dbus_g_object_register_marshaller(
          marshal_VOID__INT64_DOUBLE_STRING_STRING_INT64,
          G_TYPE_NONE,
          G_TYPE_INT64,
          G_TYPE_DOUBLE,
          G_TYPE_STRING,
          G_TYPE_STRING,
          G_TYPE_INT64,
          G_TYPE_INVALID);
    }
    const char* const kStatusUpdate = "StatusUpdate";
    dbus_g_proxy_add_signal(proxy_.gproxy(),
                            "StatusUpdate",
                            G_TYPE_INT64,
                            G_TYPE_DOUBLE,
                            G_TYPE_STRING,
                            G_TYPE_STRING,
                            G_TYPE_INT64,
                            G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(proxy_.gproxy(),
                                kStatusUpdate,
                                G_CALLBACK(StaticSignalHandler),
                                this,
                                NULL);
  }
  static void StaticSignalHandler(DBusGProxy* proxy,
                                  int64_t last_checked_time,
                                  double progress,
                                  gchar* current_operation,
                                  gchar* new_version,
                                  int64_t new_size,
                                  void* user_data) {
    OpaqueUpdateStatusConnection* self =
        reinterpret_cast<OpaqueUpdateStatusConnection*>(user_data);
    self->SignalHandler(proxy,
                        last_checked_time,
                        progress,
                        current_operation,
                        new_version,
                        new_size);
  }
  void SignalHandler(DBusGProxy* proxy,
                     int64_t last_checked_time,
                     double progress,
                     gchar* current_operation,
                     gchar* new_version,
                     int64_t new_size) {
    UpdateStatusOperation status = UpdateStatusFromString(current_operation);
    if (status == UPDATE_STATUS_ERROR) {
      LOG(ERROR) << "Error parsing status: " << current_operation;
      return;
    }
    UpdateProgress information;
    information.status_ = status;
    information.download_progress_ = progress;
    information.last_checked_time_ = last_checked_time;
    information.new_version_ = NewStringCopy(new_version);
    information.new_size_ = new_size;
    information.destruct_ = &DestroyUpdateProgress;
    monitor_(monitor_data_, information);
  }
 private:
  dbus::Proxy proxy_;
  static bool marshaller_registered_;
  UpdateMonitor monitor_;
  void* monitor_data_;
};
bool OpaqueUpdateStatusConnection::marshaller_registered_ = false;

// Register an UpdateMonitor callback.
extern "C"
UpdateStatusConnection ChromeOSMonitorUpdateStatus(UpdateMonitor monitor,
                                                   void* data) {
  return new OpaqueUpdateStatusConnection(monitor, data);
}
// Unregister an UpdateMonitor callback.
extern "C"
void ChromeOSDisconnectUpdateProgress(UpdateStatusConnection connection) {
  delete connection;
}

// TODO(satorux): Remove this. DEPRECATED.
extern "C"
bool ChromeOSRetrieveUpdateProgress(UpdateProgress* information) {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy update_proxy(bus,
                           kUpdateEngineServiceName,
                           kUpdateEngineServicePath,
                           kUpdateEngineServiceInterface);
  GError* error = NULL;

  gint64 last_checked_time = 0;
  gdouble progress = 0.0;
  char* current_op = NULL;
  char* new_version = NULL;
  gint64 new_size = 0;
  gboolean rc = org_chromium_UpdateEngineInterface_get_status(
      update_proxy.gproxy(),
      &last_checked_time,
      &progress,
      &current_op,
      &new_version,
      &new_size,
      &error);
  if (rc == FALSE) {
    LOG(ERROR) << "Error getting status: " << GetGErrorMessage(error);
    return false;
  }

  UpdateStatusOperation status = UpdateStatusFromString(current_op);
  if (status == UPDATE_STATUS_ERROR) {
    LOG(ERROR) << "Error parsing status: " << current_op;
    g_free(current_op);
    g_free(new_version);
    return false;
  }
  if (information->destruct_) information->destruct_(*information);
  information->status_ = status;
  information->download_progress_ = progress;
  information->last_checked_time_ = last_checked_time;
  information->new_version_ = NewStringCopy(new_version);
  information->new_size_ = new_size;
  information->destruct_ = &DestroyUpdateProgress;
  g_free(current_op);
  g_free(new_version);
  return true;
}

// TODO(satorux): Remove this. DEPRECATED.
extern "C"
bool ChromeOSInitiateUpdateCheck() {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy update_proxy(bus,
                           kUpdateEngineServiceName,
                           kUpdateEngineServicePath,
                           kUpdateEngineServiceInterface);
  GError* error = NULL;

  gboolean rc = org_chromium_UpdateEngineInterface_attempt_update(
      update_proxy.gproxy(), "", "", &error);
  LOG_IF(ERROR, rc == FALSE) << "Error checking for update: "
                             << GetGErrorMessage(error);
  return rc == TRUE;
}

extern "C"
bool ChromeOSRebootIfUpdated() {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy update_proxy(bus,
                           kUpdateEngineServiceName,
                           kUpdateEngineServicePath,
                           kUpdateEngineServiceInterface);
  GError* error = NULL;

  gboolean rc = org_chromium_UpdateEngineInterface_reboot_if_needed(
      update_proxy.gproxy(), &error);
  LOG_IF(ERROR, rc == FALSE) << "Error requesting a reboot: "
                             << GetGErrorMessage(error);
  return rc == TRUE;
}

// TODO(satorux): Remove this. DEPRECATED.
extern "C"
bool ChromeOSSetTrack(const std::string& track) {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy update_proxy(bus,
                           kUpdateEngineServiceName,
                           kUpdateEngineServicePath,
                           kUpdateEngineServiceInterface);
  GError* error = NULL;
  gboolean rc =
      org_chromium_UpdateEngineInterface_set_track(update_proxy.gproxy(),
                                                   track.c_str(),
                                                   &error);
  LOG_IF(ERROR, rc == FALSE) << "Error setting track: "
                             << GetGErrorMessage(error);
  return rc == TRUE;
}

// TODO(satorux): Remove this. DEPRECATED.
extern "C"
std::string ChromeOSGetTrack() {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy update_proxy(bus,
                           kUpdateEngineServiceName,
                           kUpdateEngineServicePath,
                           kUpdateEngineServiceInterface);
  char* track = NULL;
  GError* error = NULL;
  gboolean rc =
      org_chromium_UpdateEngineInterface_get_track(update_proxy.gproxy(),
                                                   &track,
                                                   &error);
  LOG_IF(ERROR, rc == FALSE) << "Error getting track: "
                             << GetGErrorMessage(error);
  if (!rc || track == NULL) {
    return "";
  }
  std::string output = track;
  g_free(track);
  return output;
}

// Asynchronous API.

namespace {

struct UpdateEngineCallbackData {
  UpdateEngineCallbackData()
      : proxy(new dbus::Proxy(dbus::GetSystemBusConnection(),
                              kUpdateEngineServiceName,
                              kUpdateEngineServicePath,
                              kUpdateEngineServiceInterface)) {}
  scoped_ptr<dbus::Proxy> proxy;
};

struct GetStatusCallbackData : public UpdateEngineCallbackData {
  GetStatusCallbackData(UpdateMonitor callback, void* user_data)
      : UpdateEngineCallbackData(),
        callback_(callback),
        user_data_(user_data) {}
  UpdateMonitor callback_;
  void* user_data_;
};

struct AttemptUpdateCallbackData : public UpdateEngineCallbackData {
  AttemptUpdateCallbackData(UpdateCallback cb, void* data)
      : UpdateEngineCallbackData(),
        callback(cb),
        user_data(data) {}
  UpdateCallback callback;
  void* user_data;
};

struct SetTrackCallbackData : public UpdateEngineCallbackData {
};

struct GetTrackCallbackData : public UpdateEngineCallbackData {
  GetTrackCallbackData(UpdateTrackCallback cb, void* data)
      : UpdateEngineCallbackData(),
        callback(cb),
        user_data(data) {}
  UpdateTrackCallback callback;
  void* user_data;
};

// Note: org_chromium_*Interface functions wrap the DBus calls and provide
// their own callback and data, which in turn calls these callbacks.

void GetStatusNotify(DBusGProxy* proxy,
                     int64_t last_checked_time,
                     double progress,
                     gchar* current_operation,
                     gchar* new_version,
                     int64_t new_size,
                     GError* error,
                     void* user_data) {
  GetStatusCallbackData* cb_data =
      static_cast<GetStatusCallbackData*>(user_data);
  if (error) {
    LOG(WARNING) << "GetStatus DBus error: " << GetGErrorMessage(error);
    delete cb_data;
    return;
  }
  UpdateStatusOperation status = UpdateStatusFromString(current_operation);
  if (status == UPDATE_STATUS_ERROR) {
    LOG(ERROR) << "Error parsing status: " << current_operation;
    delete cb_data;
    return;
  }
  UpdateProgress information;
  information.status_ = status;
  information.download_progress_ = progress;
  information.last_checked_time_ = last_checked_time;
  information.new_version_ = NewStringCopy(new_version);
  information.new_size_ = new_size;
  information.destruct_ = &DestroyUpdateProgress;
  if (cb_data->callback_)
    cb_data->callback_(cb_data->user_data_, information);
  // UpdateProgress destructor cleans up the new_version_ string.
  delete cb_data;
}

void AttemptUpdateNotify(DBusGProxy* gproxy,
                         GError* error,
                         void* user_data) {
  AttemptUpdateCallbackData* cb_data =
      static_cast<AttemptUpdateCallbackData*>(user_data);
  if (error) {
    const char* msg = GetGErrorMessage(error);
    LOG(WARNING) << "AttemptUpdate DBus Error: " << msg;
    if (cb_data->callback)
      cb_data->callback(cb_data->user_data, UPDATE_RESULT_FAILED, msg);
  } else {
    if (cb_data->callback)
      cb_data->callback(cb_data->user_data, UPDATE_RESULT_SUCCESS, NULL);
  }
  delete cb_data;
}

void SetTrackNotify(DBusGProxy* gproxy, GError* error, void* user_data) {
  SetTrackCallbackData* cb_data =
      static_cast<SetTrackCallbackData*>(user_data);
  if (error) {
    const char* msg = GetGErrorMessage(error);
    LOG(WARNING) << "SetTrack DBus Error: " << msg;
  }
  delete cb_data;
}

void GetTrackNotify(DBusGProxy* gproxy,
                    char* track,
                    GError* error,
                    void* user_data) {
  GetTrackCallbackData* cb_data =
      static_cast<GetTrackCallbackData*>(user_data);
  if (error) {
    const char* msg = GetGErrorMessage(error);
    LOG(WARNING) << "GetTrack DBus Error: " << msg;
    if (cb_data->callback)
      cb_data->callback(cb_data->user_data, NULL);
  } else {
    if (cb_data->callback)
      cb_data->callback(cb_data->user_data, track);
  }
  delete cb_data;
}

}  // namespace

extern "C"
void ChromeOSRequestUpdateStatus(UpdateMonitor callback,
                                 void* user_data) {
  GetStatusCallbackData* cb_data =
      new GetStatusCallbackData(callback, user_data);
  DBusGProxyCall* call_id =
      org_chromium_UpdateEngineInterface_get_status_async(
          cb_data->proxy->gproxy(), &GetStatusNotify, cb_data);
  if (!call_id) {
    LOG(ERROR) << "NULL call_id";
    delete cb_data;
  }
}

extern "C"
void ChromeOSRequestUpdateCheck(UpdateCallback callback, void* user_data) {
  AttemptUpdateCallbackData* cb_data =
      new AttemptUpdateCallbackData(callback, user_data);
  DBusGProxyCall* call_id =
      org_chromium_UpdateEngineInterface_attempt_update_async(
          cb_data->proxy->gproxy(), "", "", &AttemptUpdateNotify, cb_data);
  if (!call_id) {
    LOG(ERROR) << "NULL call_id";
    if (callback)
      callback(user_data, UPDATE_RESULT_DBUS_FAILED, NULL);
    delete cb_data;
  }
}

extern "C"
void ChromeOSSetUpdateTrack(const std::string& track) {
  SetTrackCallbackData* cb_data = new SetTrackCallbackData();
  DBusGProxyCall* call_id =
      org_chromium_UpdateEngineInterface_set_track_async(
          cb_data->proxy->gproxy(), track.c_str(), &SetTrackNotify, cb_data);
  if (!call_id) {
    LOG(ERROR) << "NULL call_id";
    delete cb_data;
  }
}

extern "C"
void ChromeOSRequestUpdateTrack(UpdateTrackCallback callback, void* user_data) {
  GetTrackCallbackData* cb_data = new GetTrackCallbackData(callback, user_data);
  DBusGProxyCall* call_id =
      org_chromium_UpdateEngineInterface_get_track_async(
          cb_data->proxy->gproxy(), &GetTrackNotify, cb_data);
  if (!call_id) {
    LOG(ERROR) << "NULL call_id";
    if (callback)
      callback(user_data, NULL);
    delete cb_data;
  }
}


}  // namespace chromeos
