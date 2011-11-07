// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_MOUNT_H_
#define CHROMEOS_MOUNT_H_
#include <string>
#include <vector>
#include <base/basictypes.h>

namespace chromeos { //NOLINT

enum DeviceType {
  FLASH,
  HDD,
  OPTICAL,
  UNDEFINED
};

enum MountType {
  MOUNT_TYPE_INVALID,
  MOUNT_TYPE_DEVICE,
  MOUNT_TYPE_ARCHIVE,
  MOUNT_TYPE_NETWORK_STORAGE
};

enum MountError {
  MOUNT_ERROR_NONE = 0,
  MOUNT_ERROR_UNKNOWN = 1,
  MOUNT_ERROR_INTERNAL = 2,
  MOUNT_ERROR_UNKNOWN_FILESYSTEM = 101,
  MOUNT_ERROR_UNSUPORTED_FILESYSTEM = 102,
  MOUNT_ERROR_INVALID_ARCHIVE = 201,
  MOUNT_ERROR_LIBRARY_NOT_LOADED = 501,
  MOUNT_ERROR_PATH_UNMOUNTED = 901
  // TODO(tbarzic): Add more error codes as they get added to cros-disks and
  // consider doing explicit translation from cros-disks error_types.
};

// TODO(tbarzic): Remove DiskInfoAdvanced.
class DiskInfoAdvanced {
 public:
  // DBus service path.
  virtual const char* path() const = 0;
  // Disk mount path.
  virtual const char* mount_path() const = 0;
  // Disk system path.
  virtual const char* system_path() const = 0;
  // Is disk into a drive (i.e. /dev/sdb vs, /dev/sdb1).
  virtual bool is_drive() const = 0;
  // Does the disk have media content.
  virtual bool has_media() const = 0;
  // Is the disk on deveice we booted the machien from.
  virtual bool on_boot_device() const = 0;
  // Disk file path (e.g /dev/sdb).
  virtual const char* file_path() const = 0;
  // Disk label.
  virtual const char* label() const = 0;
  // Disk model
  virtual const char* drive_label() const = 0;
  // Partition table path of the device, if device is partition.
  virtual const char* partition_slave() const = 0;
  // Device type. Not working well, yet.
  virtual DeviceType device_type() const = 0;
  // Total size of the disk.
  virtual uint64 size() const = 0;
  // Is the device read-only.
  virtual bool is_read_only() const = 0;
};

class DiskInfo : public DiskInfoAdvanced {
 public:
  virtual bool is_hidden() const = 0;
};

enum MountEventType {
  DISK_ADDED,
  DISK_REMOVED,
  DISK_CHANGED,
  DEVICE_ADDED,
  DEVICE_REMOVED,
  DEVICE_SCANNED,
  FORMATTING_FINISHED,
};

// Describes whether there is an error and whether the error came from
// the local system or from the server implementing the connect
// method.
enum MountMethodErrorType {
  MOUNT_METHOD_ERROR_NONE = 0,
  MOUNT_METHOD_ERROR_LOCAL = 1,
  MOUNT_METHOD_ERROR_REMOTE = 2,
};

// An internal listener to a d-bus signal. When notifications are received
// they are rebroadcasted in non-glib form.
class OpaqueMountEventConnection;
typedef OpaqueMountEventConnection* MountEventConnection;

// The expected callback signature that will be provided by the client who
// calls MonitorMountEvents.
typedef void(*MountEventMonitor)(void*,
                                 MountEventType,
                                 const char*);

typedef void(*MountCompletedMonitor)(void*,  // Callback data passed by client.
                                     MountError,  // Error code.
                                     const char*,  // Source path.
                                     MountType,  // Type of the mount.
                                     const char*);  // Mount path.

// Callback for asynchronous unmount requests.
typedef void (*UnmountRequestCallback)(void* object,
                                       const char* path,
                                       MountMethodErrorType error,
                                       const char* error_message);

// Callback for disk information retreival calls.
typedef void (*GetDiskPropertiesCallback)(void* object,
                                          const char* service_path,
                                          const DiskInfo* disk,
                                          MountMethodErrorType error,
                                          const char* error_message);

// Callback for disk formatting request.
typedef void (*FormatRequestCallback)(void* object,
                                      const char* device_path,
                                      bool success,
                                      MountMethodErrorType error,
                                      const char* error_message);

// Callback for disk information retreival calls.
typedef void (*RequestMountInfoCallback)(void* object,
                                         const char** devices,
                                         size_t devices_len,
                                         MountMethodErrorType error,
                                         const char* error_message);

typedef std::vector<std::pair<const char*, const char*> > MountPathOptions;
}  // namespace chromeos

#endif  // CHROMEOS_MOUNT_H_
