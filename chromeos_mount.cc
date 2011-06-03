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

struct DiskInfoImpl : public DiskInfoAdvanced {
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
      is_read_only_(false) {
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

    bool hidden;
    if (properties.Retrieve(kDevicePresentationHide, &hidden) &&
       !hidden) {

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

      glib::Value partition_slave_gval;
      if (properties.Retrieve(kPartitionSlave, &partition_slave_gval)) {
        char* partition_slave_path =
            reinterpret_cast<char*>(g_value_get_boxed(&partition_slave_gval));
        partition_slave_ = NewStringCopy(partition_slave_path);
      }

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
      if (properties.Retrieve(kDriveIsRotational,&is_rotational))
        if (properties.Retrieve(kDeviceIsOpticalDisc, &is_optical))
          device_type_ = GetDeviceType(is_optical, is_rotational);
    }
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
};

struct MountCallbackData {
  MountCallbackData(const char* interface,
                    const char* device_path) :
      proxy(new dbus::Proxy(dbus::GetSystemBusConnection(),
                            kCrosDisksInterface,
                            kCrosDisksPath,
                            interface)),
      interface_name(interface),
      callback_device_path(device_path) {}
  scoped_ptr<dbus::Proxy> proxy;
  std::string interface_name;  // Store for error reporting.
  std::string callback_device_path;
};

template<class T> struct MountRequestCallbackData : public MountCallbackData {
  MountRequestCallbackData(const char* interface,
                           const char* device_path,
                           T cb,
                           void* obj)
      : MountCallbackData(interface, device_path),
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
  MountRequestCallbackData<MountRequestCallback>* cb_data =
      reinterpret_cast<MountRequestCallbackData<MountRequestCallback>*>(
          user_data);
  DCHECK(cb_data);
  glib::ScopedError error;
  char* mount_point = NULL;
  if (!::dbus_g_proxy_end_call(gproxy,
                               call_id,
                               &Resetter(&error).lvalue(),
                               G_TYPE_STRING, &mount_point,
                               G_TYPE_INVALID)) {
    MountMethodErrorType etype;
    if (error->domain == DBUS_GERROR &&
        error->code == DBUS_GERROR_REMOTE_EXCEPTION) {
      etype = MOUNT_METHOD_ERROR_REMOTE;
    } else {
      LOG(WARNING) << "MountRequestNotify for path: '"
                   << cb_data->callback_device_path << "' error: "
                   << (error->message ? error->message : "Unknown Error.");
      etype = MOUNT_METHOD_ERROR_LOCAL;
    }
    cb_data->callback(cb_data->object,
                      cb_data->callback_device_path.c_str(),
                      NULL,
                      etype, error->message);
  } else {
    cb_data->callback(cb_data->object,
                      cb_data->callback_device_path.c_str(),
                      mount_point,
                      MOUNT_METHOD_ERROR_NONE, NULL);
  }
  if (mount_point)
    g_free(mount_point);
}


void UnmountRequestNotify(DBusGProxy* gproxy,
                          DBusGProxyCall* call_id,
                          void* user_data) {
  MountRequestCallbackData<MountRequestCallback>* cb_data =
      reinterpret_cast<MountRequestCallbackData<MountRequestCallback>*>(
          user_data);
  DCHECK(cb_data);
  glib::ScopedError error;
  if (!::dbus_g_proxy_end_call(gproxy,
                               call_id,
                               &Resetter(&error).lvalue(),
                               G_TYPE_INVALID)) {
    MountMethodErrorType etype;
    if (error->domain == DBUS_GERROR &&
        error->code == DBUS_GERROR_REMOTE_EXCEPTION) {
      etype = MOUNT_METHOD_ERROR_REMOTE;
    } else {
      LOG(WARNING) << "UnmountRequestNotify for path: '"
                   << cb_data->callback_device_path << "' error: "
                   << (error->message ? error->message : "Unknown Error.");
      etype = MOUNT_METHOD_ERROR_LOCAL;
    }
    cb_data->callback(cb_data->object,
                      cb_data->callback_device_path.c_str(),
                      NULL,
                      etype, error->message);
  } else {
    cb_data->callback(cb_data->object,
                      cb_data->callback_device_path.c_str(),
                      NULL,
                      MOUNT_METHOD_ERROR_NONE, NULL);
  }
}

void MountRemoveableDeviceAsync(const char* device_path,
                                MountRequestCallback callback,
                                void* object) {
  MountRequestCallbackData<MountRequestCallback>* cb_data =
      new MountRequestCallbackData<MountRequestCallback>(
          kCrosDisksInterface, device_path, callback, object);
  DBusGProxyCall* call_id =
      ::dbus_g_proxy_begin_call(cb_data->proxy->gproxy(),
                                "FilesystemMount",
                                &MountRequestNotify,
                                cb_data,
                                &DeleteMountCallbackData<MountRequestCallback>,
                                G_TYPE_STRING, device_path,
                                G_TYPE_STRING, "",  // auto detect filesystem
                                G_TYPE_STRV, kDefaultMountOptions,
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

void UnmountRemoveableDeviceAsync(const char* device_path,
                                  MountRequestCallback callback,
                                  void* object) {
  MountRequestCallbackData<MountRequestCallback>* cb_data =
      new MountRequestCallbackData<MountRequestCallback>(
          kCrosDisksInterface, device_path, callback, object);
  DBusGProxyCall* call_id =
      ::dbus_g_proxy_begin_call(cb_data->proxy->gproxy(),
                                "FilesystemUnmount",
                                &UnmountRequestNotify,
                                cb_data,
                                &DeleteMountCallbackData<MountRequestCallback>,
                                G_TYPE_STRING, device_path,
                                G_TYPE_STRV, kDefaultUnmountOptions,
                                G_TYPE_INVALID);
  if (!call_id) {
    LOG(ERROR) << "UnmountRemoveableDeviceAsync call failed";
    callback(object,
             device_path,
             NULL,
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
    MountMethodErrorType etype;
    if (error->domain == DBUS_GERROR &&
        error->code == DBUS_GERROR_REMOTE_EXCEPTION) {
      etype = MOUNT_METHOD_ERROR_REMOTE;
    } else {
      LOG(WARNING) << "GetDiskPropertiesNotify for path: '"
                   << cb_data->callback_device_path << "' error: "
                   << (error->message ? error->message : "Unknown Error.");
      etype = MOUNT_METHOD_ERROR_LOCAL;
    }
    if (cb_data->callback) {
      cb_data->callback(cb_data->object,
                        cb_data->callback_device_path.c_str(),
                        NULL,
                        etype, error->message);
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
          kCrosDisksInterface, device_path, callback, object);
  DBusGProxyCall* call_id =
      ::dbus_g_proxy_begin_call(cb_data->proxy->gproxy(),
          "GetDeviceProperties",
          GetDiskPropertiesNotify,
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
    MountMethodErrorType etype;
    if (error->domain == DBUS_GERROR &&
        error->code == DBUS_GERROR_REMOTE_EXCEPTION) {
      etype = MOUNT_METHOD_ERROR_REMOTE;
    } else {
      LOG(WARNING) << "RequestMountInfoNotify failed: '"
                   << (error->message ? error->message : "Unknown Error.");
      etype = MOUNT_METHOD_ERROR_LOCAL;
    }
    cb_data->callback(cb_data->object,
                      NULL,   // device paths
                      0,      // device path count
                      etype, error->message);
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
          kCrosDisksInterface, "",
          callback, object);
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
                        NULL,   // disk
                        MOUNT_METHOD_ERROR_LOCAL, NULL);
    }
    delete cb_data;
  }
}

}  // namespace

class OpaqueMountEventConnection {
 public:
  typedef dbus::MonitorConnection<void (const char*)> ConnectionType;

  OpaqueMountEventConnection(const MountEventMonitor& monitor,
                             const dbus::Proxy& mount,
                             void* object)
     : monitor_(monitor),
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
    };
    static const size_t kNumSignalEventTuples =
      sizeof(kSignalEventTuples) / sizeof(kSignalEventTuples[0]);

    callback_data_.reserve(kNumSignalEventTuples);
    connections_.reserve(kNumSignalEventTuples);

    for (size_t i = 0; i < kNumSignalEventTuples; ++i) {
      MountEventCallbackData* cb_data = new MountEventCallbackData(this,
          kSignalEventTuples[i].event_type);
      callback_data_.push_back(cb_data);

      ConnectionType* connection = new ConnectionType(mount_,
          kSignalEventTuples[i].signal_name,
          &OpaqueMountEventConnection::MountEventCallback,
          cb_data);
      connections_.push_back(connection);

      ::dbus_g_proxy_add_signal(mount_.gproxy(),
          kSignalEventTuples[i].signal_name,
          G_TYPE_STRING,
          G_TYPE_INVALID);

      ::dbus_g_proxy_connect_signal(mount_.gproxy(),
          kSignalEventTuples[i].signal_name,
          G_CALLBACK(&ConnectionType::Run),
          connection, NULL);
    }
  }

  void TearDownConnections() {
    for (std::vector<ConnectionType*>::const_iterator
        connection_iter = connections_.begin();
        connection_iter != connections_.end();
        ++connection_iter) {
      dbus::Disconnect(*connection_iter);
    }
    connections_.reset();
    callback_data_.reset();
  }

  void FireEvent(MountEventType evt, const char* path) {
    monitor_(object_, evt, path);
  }

  static void MountEventCallback(void* data, const char* device) {
    MountEventCallbackData* cb_data =
        reinterpret_cast<MountEventCallbackData*>(data);
    cb_data->event_connection->FireEvent(cb_data->event_type, device);
  }

 private:
  MountEventMonitor monitor_;
  void* object_;
  dbus::Proxy mount_;
  ScopedVector<MountEventCallbackData> callback_data_;
  ScopedVector<ConnectionType> connections_;
};

extern "C"
MountEventConnection ChromeOSMonitorMountEvents(MountEventMonitor monitor,
                                                void* object) {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy mount(bus,
                    kCrosDisksInterface,
                    kCrosDisksPath,
                    kCrosDisksInterface);
  MountEventConnection connection =
    new OpaqueMountEventConnection(monitor, mount, object);
  connection->SetUpConnections();
  return connection;
}

extern "C"
void ChromeOSMountRemovableDevice(const char* device_path,
                                  MountRequestCallback callback,
                                  void* object) {
  return MountRemoveableDeviceAsync(device_path, callback, object);
}

extern "C"
void ChromeOSUnmountRemovableDevice(const char* device_path,
                                    MountRequestCallback callback,
                                    void* object) {
  return UnmountRemoveableDeviceAsync(device_path, callback, object);
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


// TODO(zelidrag): Remove everything from here intil the rest of the file.
class OpaqueMountStatusConnection {
};
bool MountRemoveableDevice(const dbus::BusConnection& bus, const char* path) {
  return false;
}
bool UnmountRemoveableDevice(const dbus::BusConnection& bus, const char* path) {
  return false;
}
extern "C"
bool ChromeOSIsBootDevicePath(const char* device_path) {
  return false;
}
extern "C"
MountStatus* ChromeOSRetrieveMountInformation() {
  return new MountStatus();
}
extern "C"
void ChromeOSDisconnectMountStatus(MountStatusConnection connection) {
}
extern "C"
bool ChromeOSMountDevicePath(const char* device_path) {
  return false;
}
extern "C"
bool ChromeOSUnmountDevicePath(const char* device_path) {
  return false;
}
extern "C"
MountStatusConnection ChromeOSMonitorMountStatus(MountMonitor monitor,
                                                 void* object) {
  return new OpaqueMountStatusConnection();
}
extern "C"
void ChromeOSFreeMountStatus(MountStatus* status) {
}

} //  namespace chromeos
