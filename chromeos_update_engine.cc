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
void Destruct(const UpdateProgress& x) {
  delete x.new_version_;
}

// Returns -1 on error.
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
    if (status == -1) {
      LOG(ERROR) << "Error parsing status: " << current_operation;
      return;
    }
    UpdateProgress information;
    information.status_ = status;
    information.download_progress_ = progress;
    information.last_checked_time_ = last_checked_time;
    information.new_version_ = NewStringCopy(new_version);
    information.new_size_ = new_size;
    information.destruct_ = &Destruct;
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
  if (status == -1) {
    LOG(ERROR) << "Error parsing status: " << current_op;
    return false;
  }
  if (information->destruct_) information->destruct_(*information);
  information->status_ = status;
  information->download_progress_ = progress;
  information->last_checked_time_ = last_checked_time;
  information->new_version_ = NewStringCopy(new_version);
  information->new_size_ = new_size;
  information->destruct_ = &Destruct;
  return true;
}

extern "C"
bool ChromeOSInitiateUpdateCheck() {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy update_proxy(bus,
                           kUpdateEngineServiceName,
                           kUpdateEngineServicePath,
                           kUpdateEngineServiceInterface);
  GError* error = NULL;

  gboolean rc = org_chromium_UpdateEngineInterface_check_for_update(
      update_proxy.gproxy(), &error);
  LOG_IF(ERROR, rc == FALSE) << "Error checking for update: "
                             << GetGErrorMessage(error);
  return rc == TRUE;
}

}  // namespace chromeos
