// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_mount.h" // NOLINT

#include <base/logging.h>

#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <vector>

#include "chromeos/dbus/dbus.h"
#include "chromeos/glib/object.h"
#include "chromeos/string.h"
#define G_UDEV_API_IS_SUBJECT_TO_CHANGE
#include <gudev/gudev.h>

namespace chromeos { // NOLINT

const char* kDeviceKitDisksInterface =
    "org.freedesktop.DeviceKit.Disks";
const char* kDeviceKitDeviceInterface =
    "org.freedesktop.DeviceKit.Disks.Device";
const char* kDeviceKitPropertiesInterface =
    "org.freedesktop.DBus.Properties";
const char* kDefaultMountOptions[] = {
    "rw",
    "nodev",
    "noexec",
    "nosuid",
    NULL
};

extern "C"
MountStatus* ChromeOSRetrieveMountInformation();

namespace {  // NOLINT

bool DeviceIsParent(const dbus::BusConnection& bus,
                    dbus::Proxy& proxy) {
  bool parent;
  if (!dbus::RetrieveProperty(proxy,
                              kDeviceKitDeviceInterface,
                              "device-is-drive",
                              &parent)) {
    // Since we should always be able to get this property, if we can't,
    // there is some problem, so we should return false.
    DLOG(ERROR) << "unable to determine if device is a drive";
    return false;
  }
  return parent;
}

bool DeviceIsHidden(const dbus::BusConnection& bus,
                    dbus::Proxy& proxy) {
  // Assume that all devices which are not on sda are removeable
  bool hidden;
  if (!dbus::RetrieveProperty(proxy,
                              kDeviceKitDeviceInterface,
                              "device-presentation-hide",
                              &hidden)) {
    // We should always be able to get this property.  If we can't, there is
    // some problem, so we should return true to have Chrome ignore this disk.
    DLOG(ERROR) << "unable to determine if device is hidden";
    return true;
  }
  return hidden;
}

// Creates a new MountStatus instance populated with the contents from
// a vector of DiskStatus.
MountStatus* CopyFromVector(const std::vector<DiskStatus>& services) {
  MountStatus* result = new MountStatus();
  if (services.size() == 0) {
    result->disks = NULL;
  } else {
    result->disks = new DiskStatus[services.size()];
  }
  result->size = services.size();
  std::copy(services.begin(), services.end(), result->disks);
  return result;
}

bool DeviceIsMounted(const dbus::BusConnection& bus,
                     const dbus::Proxy& proxy,
                     std::string& path,
                     bool* ismounted) {
  bool mounted = false;
  if (!dbus::RetrieveProperty(proxy,
                              kDeviceKitDeviceInterface,
                              "device-is-mounted",
                              &mounted)) {
    DLOG(WARNING) << "unable to determine if device is mounted, bailing";
    return false;
  }
  *ismounted = mounted;
  if (mounted) {
    glib::Value value;
    if (!dbus::RetrieveProperty(proxy,
                                kDeviceKitDeviceInterface,
                                "device-mount-paths",
                                &value)) {
      return false;
    }

    char** paths = static_cast<char**>(g_value_get_boxed(&value));
    // TODO(dhg): This is an array for a reason, try to use it.
    if (paths[0])
      path = paths[0];
  }
  return true;
}

bool MountRemoveableDevice(const dbus::BusConnection& bus, const char* path) {
  dbus::Proxy proxy(bus,
                    kDeviceKitDisksInterface,
                    path,
                    kDeviceKitDeviceInterface);
  glib::ScopedError error;
  char* val;
  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           "FilesystemMount",
                           &Resetter(&error).lvalue(),
                           G_TYPE_STRING, NULL,
                           G_TYPE_STRV, kDefaultMountOptions,
                           G_TYPE_INVALID,
                           G_TYPE_STRING,
                           &val, G_TYPE_INVALID)) {
    LOG(WARNING) << "Filesystem Mount failed: "
        << (error->message ? error->message : "Unknown Error.");
    return false;
  }

  g_free(val);
  return true;
}

bool UnmountRemoveableDevice(const dbus::BusConnection& bus, const char* path) {
  dbus::Proxy proxy(bus,
                    kDeviceKitDisksInterface,
                    path,
                    kDeviceKitDeviceInterface);
  glib::ScopedError error;
  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           "FilesystemUnmount",
                           &Resetter(&error).lvalue(),
                           G_TYPE_STRV, NULL,
                           G_TYPE_INVALID,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << "Filesystem Unmount failed: "
        << (error->message ? error->message : "Unknown Error.");
    return false;
  }

  return true;
}

}  // namespace


void DeleteDiskStatusProperties(DiskStatus status) {
  delete status.path;
  delete status.mountpath;
}

extern "C"
void ChromeOSFreeMountStatus(MountStatus* status) {
  if (status == NULL)
    return;
  std::for_each(status->disks,
                status->disks + status->size,
                &DeleteDiskStatusProperties);
  delete [] status->disks;
  delete status;
}

class OpaqueMountStatusConnection {
 public:
  typedef dbus::MonitorConnection<void (const char*)>* ConnectionType;

  OpaqueMountStatusConnection(const MountMonitor& monitor,
                              const dbus::Proxy& mount,
                              void* object)
     : gudev_client(NULL),
       monitor_(monitor),
       object_(object),
       mount_(mount),
       addconnection_(NULL),
       removeconnection_(NULL) {
  }

  void FireEvent(MountEventType evt, const char* path) {
    MountStatus* info;
    if ((info = ChromeOSRetrieveMountInformation()) != NULL) {
      if (evt == DEVICE_REMOVED ||
          evt == DEVICE_ADDED ||
          evt == DEVICE_SCANNED ||
          evt == DISK_REMOVED) {
        monitor_(object_, *info, evt, path);
      } else {
        for (int x = 0; x < info->size; x++) {
          // This ensures we only event on disk adds/changes
          // for things which are in the disk array
          if (strcmp(path, info->disks[x].path) == 0) {
            monitor_(object_, *info, evt, path);
            break;
          }
        }

      }
      ChromeOSFreeMountStatus(info);
    }
  }

  std::vector<std::string>::iterator FindDevicePath(std::string path) {
    for (std::vector<std::string>::iterator i = paths_.begin();
         i != paths_.end();
         ++i) {
      if (path.find(*i) != std::string::npos) {
        return i;
      }
    }
    return paths_.end();
  }

  static void Added(void* object, const char* device) {
    MountStatusConnection self = static_cast<MountStatusConnection>(object);
    self->FireEvent(DISK_ADDED, device);
  }

  static void Removed(void* object, const char* device) {
    MountStatusConnection self = static_cast<MountStatusConnection>(object);
    self->FireEvent(DISK_REMOVED, device);
  }

  static void Changed(void* object, const char* device) {
    MountStatusConnection self = static_cast<MountStatusConnection>(object);
    self->FireEvent(DISK_CHANGED, device);
  }

  static void OnUDevEvent (GUdevClient* client,
                           const char* action,
                           GUdevDevice* device,
                           gpointer object) {
    MountStatusConnection self = static_cast<MountStatusConnection>(object);
    std::vector<std::string>::iterator iter;
    // can be scsi or block
    const char* subsystem = g_udev_device_get_subsystem(device);
    if (subsystem == NULL || strcmp(subsystem, "scsi") != 0) {
      return;
    }
    if (strcmp(action, "add") == 0) {
      const char* device_path = g_udev_device_get_sysfs_path(device);
      iter = self->FindDevicePath(device_path);
      if (iter != self->paths_.end()) {
        self->FireEvent(DEVICE_SCANNED, iter->c_str());
      } else {
        std::string path = device_path;
        self->paths_.push_back(path);
        self->FireEvent(DEVICE_ADDED, device_path);
      }
    } else if (strcmp(action, "remove") == 0) {
      const char* device_path = g_udev_device_get_sysfs_path(device);
      iter = self->FindDevicePath(device_path);
      if (iter != self->paths_.end()) {
        self->paths_.erase(iter);
        self->FireEvent(DEVICE_REMOVED, device_path);
      }
    }
  }
  ConnectionType& addedconnection() {
    return addconnection_;
  }

  ConnectionType& removedconnection() {
    return removeconnection_;
  }

  ConnectionType& changedconnection() {
    return changedconnection_;
  }

  GUdevClient* gudev_client;

 private:
  MountMonitor monitor_;
  void* object_;
  std::vector<std::string> paths_;
  dbus::Proxy mount_;
  ConnectionType addconnection_;
  ConnectionType removeconnection_;
  ConnectionType changedconnection_;
};

extern "C"
MountStatusConnection ChromeOSMonitorMountStatus(MountMonitor monitor,
                                                 void* object) {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy mount(bus,
                    kDeviceKitDisksInterface,
                    "/org/freedesktop/DeviceKit/Disks",
                    kDeviceKitDisksInterface);
  MountStatusConnection result =
     new OpaqueMountStatusConnection(monitor, mount, object);
  ::dbus_g_proxy_add_signal(mount.gproxy(),
                            "DeviceAdded",
                            DBUS_TYPE_G_OBJECT_PATH,
                             G_TYPE_INVALID);
  ::dbus_g_proxy_add_signal(mount.gproxy(),
                            "DeviceRemoved",
                            DBUS_TYPE_G_OBJECT_PATH,
                            G_TYPE_INVALID);
  ::dbus_g_proxy_add_signal(mount.gproxy(),
                            "DeviceChanged",
                            DBUS_TYPE_G_OBJECT_PATH,
                            G_TYPE_INVALID);
  typedef dbus::MonitorConnection<void (const char*)> ConnectionType;

  ConnectionType* added = new ConnectionType(mount, "DeviceAdded",
      &OpaqueMountStatusConnection::Added, result);

  ::dbus_g_proxy_connect_signal(mount.gproxy(), "DeviceAdded",
                                G_CALLBACK(&ConnectionType::Run),
                                added, NULL);
  result->addedconnection() = added;


  ConnectionType* removed = new ConnectionType(mount, "DeviceRemoved",
      &OpaqueMountStatusConnection::Removed, result);

  ::dbus_g_proxy_connect_signal(mount.gproxy(), "DeviceRemoved",
                                G_CALLBACK(&ConnectionType::Run),
                                removed, NULL);
  result->removedconnection() = removed;


  ConnectionType* changed = new ConnectionType(mount, "DeviceChanged",
      &OpaqueMountStatusConnection::Changed, result);

  ::dbus_g_proxy_connect_signal(mount.gproxy(), "DeviceChanged",
                                G_CALLBACK(&ConnectionType::Run),
                                changed, NULL);
  result->changedconnection() = changed;

  // Listen to udev events

  const char *subsystems[] = {"scsi","block", NULL};
  result->gudev_client = g_udev_client_new(subsystems);
  g_signal_connect(result->gudev_client,
                   "uevent",
                   G_CALLBACK (&OpaqueMountStatusConnection::OnUDevEvent),
                   result);

  return result;
}

extern "C"
bool ChromeOSMountDevicePath(const char* device_path) {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  return MountRemoveableDevice(bus, device_path);
}

extern "C"
bool ChromeOSUnmountDevicePath(const char* device_path) {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  return UnmountRemoveableDevice(bus, device_path);
}

extern "C"
void ChromeOSDisconnectMountStatus(MountStatusConnection connection) {
  dbus::Disconnect(connection->addedconnection());
  dbus::Disconnect(connection->removedconnection());
  dbus::Disconnect(connection->changedconnection());
  delete connection;
}

extern "C"
MountStatus* ChromeOSRetrieveMountInformation() {
  typedef glib::ScopedPtrArray<const char*> ScopedPtrArray;
  typedef ScopedPtrArray::iterator iterator;

  ScopedPtrArray devices;

  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy mount(bus,
                    kDeviceKitDisksInterface,
                    "/org/freedesktop/DeviceKit/Disks",
                    kDeviceKitDisksInterface);
  if (!dbus::CallPtrArray(mount, "EnumerateDevices", &devices)) {
    DLOG(WARNING) << "Could not enumerate disk devices.";
    return NULL;
  }
  std::vector<DiskStatus> buffer;
  for (iterator currentpath = devices.begin();
       currentpath < devices.end();
       ++currentpath) {
    dbus::Proxy proxy(bus,
                      kDeviceKitDisksInterface,
                      *currentpath,
                      kDeviceKitPropertiesInterface);
    const bool hidden = DeviceIsHidden(bus, proxy);
    if (!hidden) {
      DiskStatus info = {};
      bool ismounted = false;
      std::string path;
      info.isparent = DeviceIsParent(bus, proxy);
      info.path = NewStringCopy(*currentpath);
      if (DeviceIsMounted(bus, proxy, path, &ismounted)) {
        if (ismounted) {
          info.mountpath = NewStringCopy(path.c_str());
        }
      }
      info.hasmedia = false;
      dbus::RetrieveProperty(proxy,
                             kDeviceKitDeviceInterface,
                             "device-is-media-available",
                             &info.hasmedia);
      if (dbus::RetrieveProperty(proxy,
                                 kDeviceKitDeviceInterface,
                                 "native-path",
                                 &path)) {
        info.systempath = NewStringCopy(path.c_str());
      }
      buffer.push_back(info);
    }
  }
  return CopyFromVector(buffer);
}

}  // namespace chromeos
