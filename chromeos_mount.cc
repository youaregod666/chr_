// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_mount.h" // NOLINT

#include <base/logging.h>
#include <base/memory/scoped_vector.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <set>
#include <string>
#include <vector>

#include "chromeos/dbus/dbus.h"
#include "chromeos/glib/object.h"
#include "chromeos/string.h"
#include "marshal.glibmarshal.h"

namespace chromeos { // NOLINT

const char* kCrosDisksInterface = "org.chromium.CrosDisks";
const char* kCrosDisksPath = "/org/chromium/CrosDisks";

const char* kDefaultMountOptions[] = {
    "rw",
    "nodev",
    "noexec",
    "nosuid",
    "sync",
    NULL
};
const char* kDefaultUnmountOptions[] = {
    "force",
    NULL
};

// Relevant Device/Disk properties.
const char kDeviceIsDrive[] = "DeviceIsDrive";
const char kDevicePresentationHide[] = "DevicePresentationHide";
const char kDeviceMountPaths[] = "DeviceMountPaths";
const char kDeviceIsMediaAvailable[] = "DeviceIsMediaAvailable";
const char kDeviceIsOnBootDevice[] = "DeviceIsOnBootDevice";
const char kNativePath[] = "NativePath";
const char kDeviceFile[] = "DeviceFile";
const char kLabel[] = "IdLabel";
const char kDriveModel[] = "DriveModel";
const char kPartitionSlave[] = "PartitionSlave";
const char kDriveIsRotational[] = "DriveIsRotational";
const char kDeviceIsOpticalDisc[] = "DeviceIsOpticalDisc";
const char kDeviceSize[] = "DeviceSize";
const char kReadOnly[] = "DeviceIsReadOnly";

namespace {  // NOLINT

struct DiskInfoImpl : public DiskInfo {
 public:
  DiskInfoImpl(const char* path, const glib::ScopedHashTable& properties)
    : mount_path_(NULL),
      system_path_(NULL),
      is_drive_(false),
      has_media_(false),
      on_boot_device_(false),
      file_path_(NULL),
      label_(NULL),
      drive_model_(NULL),
      partition_slave_(NULL),
      device_type_(UNDEFINED),
      total_size_(0),
      is_read_only_(false),
      is_hidden_(true) {
    DCHECK(path);
    path_ = NewStringCopy(path);
    InitializeFromProperties(properties);
  }

  virtual ~DiskInfoImpl() {
    if (path_)
      delete path_;
    if (mount_path_)
      delete mount_path_;
    if (system_path_)
      delete system_path_;
    if (file_path_)
      delete file_path_;
    if (label_)
      delete label_;
    if (drive_model_)
      delete drive_model_;
    if (partition_slave_)
      delete partition_slave_;
  }
  // DiskInfo overrides:
  virtual const char* path() const { return path_; }
  virtual const char* mount_path() const { return mount_path_; }
  virtual const char* system_path() const { return system_path_; }
  virtual bool is_drive() const { return is_drive_; }
  virtual bool has_media() const { return has_media_; }
  virtual bool on_boot_device() const { return on_boot_device_; }

  // DiskInfoAdvanced overrides:
  virtual const char* file_path() const { return file_path_; }
  virtual const char* label() const { return label_; }
  virtual const char* drive_label() const { return drive_model_; }
  virtual const char* partition_slave() const { return partition_slave_; }
  virtual DeviceType device_type() const { return device_type_; }
  virtual uint64 size() const { return total_size_; }
  virtual bool is_read_only() const { return is_read_only_; }
  virtual bool is_hidden() const { return is_hidden_; }

 private:
  DeviceType GetDeviceType(bool is_optical, bool is_rotational) {
    if (is_optical)
      return OPTICAL;
    if (is_rotational)
      return HDD;
    return FLASH;
  }

  void InitializeFromProperties(const glib::ScopedHashTable& properties) {
    properties.Retrieve(kDeviceIsDrive, &is_drive_);
    properties.Retrieve(kReadOnly, &is_read_only_);
    properties.Retrieve(kDevicePresentationHide, &is_hidden_);
    properties.Retrieve(kDeviceIsMediaAvailable, &has_media_);
    properties.Retrieve(kDeviceIsOnBootDevice, &on_boot_device_);

    std::string path;
    if (properties.Retrieve(kNativePath, &path))
      system_path_ = NewStringCopy(path.c_str());

    std::string file_path_str;
    if (properties.Retrieve(kDeviceFile, &file_path_str))
      file_path_ = NewStringCopy(file_path_str.c_str());

    std::string drive_model_str;
    if (properties.Retrieve(kDriveModel, &drive_model_str))
      drive_model_ = NewStringCopy(drive_model_str.c_str());

    std::string  device_label_str;
    if (properties.Retrieve(kLabel, &device_label_str))
      label_ = NewStringCopy(device_label_str.c_str());

    glib::Value size_gval;
    if (properties.Retrieve(kDeviceSize, &size_gval))
      total_size_ = static_cast<uint64>(::g_value_get_uint64(&size_gval));

    glib::Value value;
    if (properties.Retrieve(kDeviceMountPaths, &value)) {
      char** paths = reinterpret_cast<char**>(g_value_get_boxed(&value));
      if (paths[0])
        mount_path_ = NewStringCopy(paths[0]);
    }

    bool is_rotational;
    bool is_optical;
    if (properties.Retrieve(kDriveIsRotational, &is_rotational))
      if (properties.Retrieve(kDeviceIsOpticalDisc, &is_optical))
        device_type_ = GetDeviceType(is_optical, is_rotational);
  }

  const char* path_;
  const char* mount_path_;
  const char* system_path_;
  bool is_drive_;
  bool has_media_;
  bool on_boot_device_;

  const char* file_path_;
  const char* label_;
  const char* drive_model_;
  const char* partition_slave_;
  DeviceType device_type_;
  uint64 total_size_;
  bool is_read_only_;
  bool is_hidden_;
};

struct MountCallbackData {
  MountCallbackData(const char* interface,
                    const char* device_path,
                    MountType type) :
      proxy(new dbus::Proxy(dbus::GetSystemBusConnection(),
                            kCrosDisksInterface,
                            kCrosDisksPath,
                            interface)),
      interface_name(interface),
      callback_device_path(device_path),
      mount_type(type) {}
  scoped_ptr<dbus::Proxy> proxy;
  std::string interface_name;  // Store for error reporting.
  std::string callback_device_path;
  MountType mount_type;
};

template<class T> struct MountRequestCallbackData : public MountCallbackData {
  MountRequestCallbackData(const char* interface,
                           const char* device_path,
                           MountType type,
                           T cb,
                           void* obj)
      : MountCallbackData(interface, device_path, type),
        callback(cb),
        object(obj) {}
  // Owned by the caller (i.e. Chrome), do not destroy them:
  T callback;
  void* object;
};

// DBus will always call the Delete function passed to it by
// dbus_g_proxy_begin_call, whether DBus calls the callback or not.
template<class T>
void DeleteMountCallbackData(void* user_data) {
  MountRequestCallbackData<T>* cb_data =
      reinterpret_cast<MountRequestCallbackData<T>*>(user_data);
  delete cb_data;
}

struct MountEventCallbackData {
  MountEventCallbackData(MountEventConnection connection,
                         MountEventType type)
      : event_connection(connection),
        event_type(type) {}

  MountEventConnection event_connection;
  MountEventType event_type;
};

void MountRequestNotify(DBusGProxy* gproxy,
                        DBusGProxyCall* call_id,
                        void* user_data) {
  MountRequestCallbackData<MountCompletedMonitor>* cb_data =
      reinterpret_cast<MountRequestCallbackData<MountCompletedMonitor>*>(
          user_data);
  DCHECK(cb_data);
  glib::ScopedError error;
  if (!::dbus_g_proxy_end_call(gproxy,
                               call_id,
                               &Resetter(&error).lvalue(),
                               G_TYPE_INVALID)) {
    LOG(WARNING) << "MountRequestNotify for path: '"
                 << cb_data->callback_device_path << "' error: "
                 << (error->message ? error->message : "Unknown Error.");

    cb_data->callback(cb_data->object,
                      MOUNT_ERROR_INTERNAL,
                      cb_data->callback_device_path.c_str(),
                      cb_data->mount_type,
                      NULL);
  }
}

void UnmountRequestNotify(DBusGProxy* gproxy,
                          DBusGProxyCall* call_id,
                          void* user_data) {
  MountRequestCallbackData<UnmountRequestCallback>* cb_data =
      reinterpret_cast<MountRequestCallbackData<UnmountRequestCallback>*>(
          user_data);
  DCHECK(cb_data);
  glib::ScopedError error;
  if (!::dbus_g_proxy_end_call(gproxy,
                               call_id,
                               &Resetter(&error).lvalue(),
                               G_TYPE_INVALID)) {
    MountMethodErrorType error_type;
    if (error->domain == DBUS_GERROR &&
        error->code == DBUS_GERROR_REMOTE_EXCEPTION) {
      error_type = MOUNT_METHOD_ERROR_REMOTE;
    } else {
      LOG(WARNING) << "UnmountRequestNotify for path: '"
                   << cb_data->callback_device_path << "' error: "
                   << (error->message ? error->message : "Unknown Error.");
      error_type = MOUNT_METHOD_ERROR_LOCAL;
    }
    cb_data->callback(cb_data->object,
                      cb_data->callback_device_path.c_str(),
                      error_type, error->message);
  } else {
    cb_data->callback(cb_data->object,
                      cb_data->callback_device_path.c_str(),
                      MOUNT_METHOD_ERROR_NONE, NULL);
  }
}

void MountPathAsync(const char* source_path,
                    MountType type,
                    const MountPathOptions& options,
                    MountCompletedMonitor callback,
                    void* object) {
  MountRequestCallbackData<MountCompletedMonitor>* cb_data =
      new MountRequestCallbackData<MountCompletedMonitor>(
          kCrosDisksInterface, source_path, type, callback, object);
  DBusGProxyCall* call_id =
      ::dbus_g_proxy_begin_call(cb_data->proxy->gproxy(),
                                "Mount",
                                &MountRequestNotify,
                                cb_data,
                                &DeleteMountCallbackData<MountCompletedMonitor>,
                                G_TYPE_STRING, source_path,
                                G_TYPE_STRING, "",  // auto detect filesystem.
                                G_TYPE_STRV, kDefaultMountOptions,
                                G_TYPE_INVALID);
  if (!call_id) {
    LOG(ERROR) << "MountPathAsync call failed";
    callback(object, MOUNT_ERROR_INTERNAL, source_path, type, NULL);
    delete cb_data;
  }
}

void UnmountPathAsync(const char* device_path,
                      UnmountRequestCallback callback,
                      void* object) {
  MountRequestCallbackData<UnmountRequestCallback>* cb_data =
      new MountRequestCallbackData<UnmountRequestCallback>(
          kCrosDisksInterface, device_path, MOUNT_TYPE_INVALID, callback,
          object);
  DBusGProxyCall* call_id =
      ::dbus_g_proxy_begin_call(
          cb_data->proxy->gproxy(),
          "Unmount",
          &UnmountRequestNotify,
          cb_data,
          &DeleteMountCallbackData<UnmountRequestCallback>,
          G_TYPE_STRING, device_path,
          G_TYPE_STRV, kDefaultUnmountOptions,
          G_TYPE_INVALID);
  if (!call_id) {
    LOG(ERROR) << "UnmountRemoveableDeviceAsync call failed";
    callback(object,
             device_path,
             MOUNT_METHOD_ERROR_LOCAL, NULL);
    delete cb_data;
  }
}

void GetDiskPropertiesNotify(DBusGProxy* gproxy,
                             DBusGProxyCall* call_id,
                             void* user_data) {
  MountRequestCallbackData<GetDiskPropertiesCallback>* cb_data =
      reinterpret_cast<MountRequestCallbackData<GetDiskPropertiesCallback>*>(
          user_data);
  DCHECK(cb_data);
  glib::ScopedError error;
  glib::ScopedHashTable properties;
  if (!::dbus_g_proxy_end_call(gproxy,
                               call_id,
                               &Resetter(&error).lvalue(),
                               ::dbus_g_type_get_map("GHashTable",
                                                     G_TYPE_STRING,
                                                     G_TYPE_VALUE),
                               &Resetter(&properties).lvalue(),
          G_TYPE_INVALID)) {
    MountMethodErrorType error_type;
    if (error->domain == DBUS_GERROR &&
        error->code == DBUS_GERROR_REMOTE_EXCEPTION) {
      error_type = MOUNT_METHOD_ERROR_REMOTE;
    } else {
      LOG(WARNING) << "GetDiskPropertiesNotify for path: '"
                   << cb_data->callback_device_path << "' error: "
                   << (error->message ? error->message : "Unknown Error.");
      error_type = MOUNT_METHOD_ERROR_LOCAL;
    }
    if (cb_data->callback) {
      cb_data->callback(cb_data->object,
                        cb_data->callback_device_path.c_str(),
                        NULL,
                        error_type, error->message);
    }
    return;
  }
  DiskInfoImpl disk(cb_data->callback_device_path.c_str(), properties);
  cb_data->callback(cb_data->object,
                    disk.path(),
                    &disk,
                    MOUNT_METHOD_ERROR_NONE, NULL);
}

void GetDiskPropertiesAsync(const char* device_path,
                            GetDiskPropertiesCallback callback,
                            void* object) {
  MountRequestCallbackData<GetDiskPropertiesCallback>* cb_data =
      new MountRequestCallbackData<GetDiskPropertiesCallback>(
          kCrosDisksInterface, device_path, MOUNT_TYPE_INVALID, callback,
          object);
  DBusGProxyCall* call_id =
      ::dbus_g_proxy_begin_call(cb_data->proxy->gproxy(),
          "GetDeviceProperties",
          &GetDiskPropertiesNotify,
          cb_data,
          &DeleteMountCallbackData<GetDiskPropertiesCallback>,
          G_TYPE_STRING, device_path,
          G_TYPE_INVALID);
  if (!call_id) {
    LOG(ERROR) << "GetDiskPropertiesAsync call failed for device "
               << device_path;
    cb_data->callback(cb_data->object,
                      device_path,
                      NULL,
                      MOUNT_METHOD_ERROR_LOCAL, NULL);
    delete cb_data;
  }
}

void FormatRequestNotify(DBusGProxy* gproxy,
                         DBusGProxyCall* call_id,
                         void* user_data) {
  MountRequestCallbackData<FormatRequestCallback>* cb_data =
      reinterpret_cast<MountRequestCallbackData<FormatRequestCallback>*>(
          user_data);
  DCHECK(cb_data);
  glib::ScopedError error;
  bool success = false;
  if (!::dbus_g_proxy_end_call(gproxy,
                               call_id,
                               &Resetter(&error).lvalue(),
                               G_TYPE_BOOLEAN, &success,
                               G_TYPE_INVALID)) {
    MountMethodErrorType error_type = MOUNT_METHOD_ERROR_LOCAL;
    if (error->domain == DBUS_GERROR &&
        error->code == DBUS_GERROR_REMOTE_EXCEPTION) {
      error_type = MOUNT_METHOD_ERROR_REMOTE;
    } else {
      LOG(WARNING) << "FormatRequestNotify for path: '"
                   << cb_data->callback_device_path << "' error: "
                   << (error->message ? error->message : "Unknown Error.");
      error_type = MOUNT_METHOD_ERROR_LOCAL;
    }
    cb_data->callback(cb_data->object,
                      cb_data->callback_device_path.c_str(),
                      false,
                      error_type, error->message);
  } else {
    cb_data->callback(cb_data->object,
                      cb_data->callback_device_path.c_str(),
                      success,
                      MOUNT_METHOD_ERROR_NONE, NULL);
  }
}

void FormatDeviceAsync(const char* device_path, const char* filesystem,
                       FormatRequestCallback callback,
                       void* object) {
  MountRequestCallbackData<FormatRequestCallback>* cb_data =
      new MountRequestCallbackData<FormatRequestCallback>(
          kCrosDisksInterface, device_path, MOUNT_TYPE_INVALID, callback,
          object);
  DBusGProxyCall* call_id =
      ::dbus_g_proxy_begin_call(cb_data->proxy->gproxy(),
                                "FormatDevice",
                                &FormatRequestNotify,
                                cb_data,
                                &DeleteMountCallbackData<FormatRequestCallback>,
                                G_TYPE_STRING, device_path,
                                G_TYPE_STRING, filesystem,
                                G_TYPE_INVALID);
  if (!call_id) {
    LOG(ERROR) << "MountRemoveableDeviceAsync call failed";
    callback(object,
             device_path,
             NULL,
             MOUNT_METHOD_ERROR_LOCAL, NULL);
    delete cb_data;
  }
}

void RequestMountInfoNotify(DBusGProxy* gproxy,
                            DBusGProxyCall* call_id,
                            void* user_data) {
  MountRequestCallbackData<RequestMountInfoCallback>* cb_data =
      reinterpret_cast<MountRequestCallbackData<
      RequestMountInfoCallback>*>(user_data);
  DCHECK(cb_data);
  glib::ScopedError error;
  glib::ScopedPtrArray<const char*> devices;
  ::GType g_type_array =
      ::dbus_g_type_get_collection("GPtrArray", G_TYPE_STRING);
  if (!::dbus_g_proxy_end_call(gproxy,
                               call_id,
                               &Resetter(&error).lvalue(),
                               g_type_array, &Resetter(&devices).lvalue(),
                               G_TYPE_INVALID)) {
    MountMethodErrorType error_type;
    if (error->domain == DBUS_GERROR &&
        error->code == DBUS_GERROR_REMOTE_EXCEPTION) {
      error_type = MOUNT_METHOD_ERROR_REMOTE;
    } else {
      LOG(WARNING) << "RequestMountInfoNotify failed: '"
                   << (error->message ? error->message : "Unknown Error.");
      error_type = MOUNT_METHOD_ERROR_LOCAL;
    }
    cb_data->callback(cb_data->object,
                      NULL,   // device paths
                      0,      // device path count
                      error_type, error->message);
    return;
  }

  // Pack and send results.
  scoped_array<char*> device_paths(new char*[devices.size()]);
  size_t removable_device_count = 0;
  for (glib::ScopedPtrArray<const char*>::iterator device_iter =
          devices.begin();
      device_iter < devices.end();
      ++device_iter) {
    device_paths[removable_device_count++] = NewStringCopy(*device_iter);
  }
  cb_data->callback(cb_data->object,
                    const_cast<const char**>(device_paths.get()),
                    removable_device_count,
                    MOUNT_METHOD_ERROR_NONE, NULL);
  for (size_t i = 0; i < removable_device_count; i++)
    delete device_paths[i++];
}

void RequestMountInfoAsync(RequestMountInfoCallback callback,
                           void* object) {
  MountRequestCallbackData<RequestMountInfoCallback>* cb_data =
      new MountRequestCallbackData<RequestMountInfoCallback>(
          kCrosDisksInterface, "", MOUNT_TYPE_INVALID, callback,
          object);
  DBusGProxyCall* call_id =
      ::dbus_g_proxy_begin_call(cb_data->proxy->gproxy(),
            "EnumerateAutoMountableDevices",
            RequestMountInfoNotify,
            cb_data,
            &DeleteMountCallbackData<RequestMountInfoCallback>,
            G_TYPE_INVALID);
  if (!call_id) {
    LOG(ERROR) << "RequestMountInfoAsync call failed";
    if (callback) {
      cb_data->callback(cb_data->object,
                        NULL,   // device path
                        0,      // device path count
                        MOUNT_METHOD_ERROR_LOCAL, NULL);
    }
    delete cb_data;
  }
}

}  // namespace

class OpaqueMountEventConnection {
 public:
  typedef dbus::MonitorConnection<void (const char*)> ConnectionType;
  typedef dbus::MonitorConnection<void (unsigned int, const char*,
                                        unsigned int, const char*)>
      MountCompletedConnectionType;

  OpaqueMountEventConnection(
      const MountEventMonitor& monitor,
      const MountCompletedMonitor& mount_completed_monitor,
      const dbus::Proxy& mount,
      void* object)
     : monitor_(monitor),
       mount_completed_monitor_(mount_completed_monitor),
       object_(object),
       mount_(mount) {
  }

  ~OpaqueMountEventConnection() {
    TearDownConnections();
  }

  void SetUpConnections() {
    static const struct {
      const char *signal_name;
      MountEventType event_type;
    } kSignalEventTuples[] = {
      { "DeviceAdded", DEVICE_ADDED },
      { "DeviceScanned", DEVICE_SCANNED },
      { "DeviceRemoved", DEVICE_REMOVED },
      { "DiskAdded", DISK_ADDED },
      { "DiskChanged", DISK_CHANGED },
      { "DiskRemoved", DISK_REMOVED },
      { "FormattingFinished", FORMATTING_FINISHED },
    };
    static const size_t kNumSignalEventTuples =
      sizeof(kSignalEventTuples) / sizeof(kSignalEventTuples[0]);

    callback_data_.reserve(kNumSignalEventTuples);
    connections_.reserve(kNumSignalEventTuples);

    for (size_t i = 0; i < kNumSignalEventTuples; ++i) {
      MountEventCallbackData* cb_data = new MountEventCallbackData(this,
          kSignalEventTuples[i].event_type);
      callback_data_.push_back(cb_data);

      ConnectionType* connection = dbus::Monitor(mount_,
          kSignalEventTuples[i].signal_name,
          &OpaqueMountEventConnection::MountEventCallback,
          cb_data);
      connections_.push_back(connection);
    }
    mount_completed_connection_ = dbus::Monitor(mount_,
                                               "MountCompleted",
                                               &MountCompletedEventCallback,
                                               this);
  }

  void TearDownConnections() {
    for (std::vector<ConnectionType*>::const_iterator
        connection_iter = connections_.begin();
        connection_iter != connections_.end();
        ++connection_iter) {
      // dbus::Disconnect also deletes the connection object.
      dbus::Disconnect(*connection_iter);
    }
    connections_.clear();
    callback_data_.reset();
    dbus::Disconnect(mount_completed_connection_);
  }

  void FireEvent(MountEventType evt, const char* path) {
    monitor_(object_, evt, path);
  }

  void FireMountCompletedEvent(MountError error_code,
                               const char* source_path,
                               MountType mount_type,
                               const char* mount_path) {
   LOG(WARNING) << "Mount complete" << error_code << " " << source_path;
   mount_completed_monitor_(object_, error_code, source_path, mount_type,
        mount_path);
  }

  static void MountEventCallback(void* data, const char* device) {
    MountEventCallbackData* cb_data =
        reinterpret_cast<MountEventCallbackData*>(data);
    cb_data->event_connection->FireEvent(cb_data->event_type, device);
  }

  static void MountCompletedEventCallback(void* data,
                                          unsigned int error_code,
                                          const char* source_path,
                                          unsigned int mount_type,
                                          const char* mount_path) {
    OpaqueMountEventConnection* self =
        reinterpret_cast<OpaqueMountEventConnection*>(data);
    self->FireMountCompletedEvent(static_cast<MountError>(error_code),
                                  source_path,
                                  static_cast<MountType>(mount_type),
                                  mount_path);
  }

 private:
  MountEventMonitor monitor_;
  MountCompletedMonitor mount_completed_monitor_;
  void* object_;
  dbus::Proxy mount_;
  ScopedVector<MountEventCallbackData> callback_data_;
  std::vector<ConnectionType*> connections_;
  MountCompletedConnectionType* mount_completed_connection_;
};

extern "C"
MountEventConnection ChromeOSMonitorAllMountEvents(
    MountEventMonitor monitor,
    MountCompletedMonitor mount_completed_monitor,
    void* object) {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy mount(bus,
                    kCrosDisksInterface,
                    kCrosDisksPath,
                    kCrosDisksInterface);
  ::dbus_g_object_register_marshaller(marshal_VOID__UINT_STRING_UINT_STRING,
                                      G_TYPE_NONE,
                                      G_TYPE_UINT,
                                      G_TYPE_STRING,
                                      G_TYPE_UINT,
                                      G_TYPE_STRING,
                                      G_TYPE_INVALID);
  MountEventConnection connection =
    new OpaqueMountEventConnection(monitor,
                                   mount_completed_monitor,
                                   mount,
                                   object);
  connection->SetUpConnections();
  return connection;
}

extern "C"
void ChromeOSMountSourcePath(const char* source_path,
                             MountType type,
                             const MountPathOptions& options,
                             MountCompletedMonitor callback,
                             void* object) {
  return MountPathAsync(source_path, type, options, callback, object);
}

extern "C"
void ChromeOSUnmountMountPoint(const char* device_path,
                               UnmountRequestCallback callback,
                               void* object) {
  return UnmountPathAsync(device_path, callback, object);
}

extern "C"
void ChromeOSGetDiskProperties(const char* device_path,
                               GetDiskPropertiesCallback callback,
                               void* object) {
  GetDiskPropertiesAsync(device_path, callback, object);
}

extern "C"
void ChromeOSRequestMountInfo(RequestMountInfoCallback callback,
                              void* object) {
  RequestMountInfoAsync(callback, object);
}

extern "C"
void ChromeOSDisconnectMountEventMonitor(MountEventConnection connection) {
  delete connection;
}

extern "C"
void ChromeOSFormatDevice(const char* device_path,
                          const char* filesystem,
                          FormatRequestCallback callback,
                          void* object) {
  FormatDeviceAsync(device_path, filesystem, callback, object);
}

}  //  namespace chromeos
