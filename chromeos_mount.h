// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_MOUNT_H_
#define CHROMEOS_MOUNT_H_
#include <string>
#include <base/basictypes.h>

namespace chromeos { //NOLINT

enum DeviceType {
  FLASH,
  HDD,
  OPTICAL,
  UNDEFINED
};


class DiskInfo {
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
};

// TODO(tbarzic): merge DiskInfoAdvanced with DinskInfo.
class DiskInfoAdvanced : public DiskInfo {
 public:
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

enum MountEventType {
  DISK_ADDED,
  DISK_REMOVED,
  DISK_CHANGED,
  DEVICE_ADDED,
  DEVICE_REMOVED,
  DEVICE_SCANNED
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

// Processes a callback from a d-bus signal by finding the path of the
// DeviceKit Disks service that changed and sending the details along to the
// next handler in the chain as an instance of MountStatus.
extern MountEventConnection (*MonitorMountEvents)(MountEventMonitor monitor,
                                                  void*);

// Disconnects a listener from the mounting events.
extern void (*DisconnectMountEventMonitor)(MountEventConnection connection);

// Callback for asynchronous mount/unmount requests.
typedef void (*MountRequestCallback)(void* object,
                                     const char* path,
                                     const char* mount_path,
                                     MountMethodErrorType error,
                                     const char* error_message);

// Callback for disk information retreival calls.
typedef void (*GetDiskPropertiesCallback)(void* object,
                                          const char* service_path,
                                          const DiskInfo* disk,
                                          MountMethodErrorType error,
                                          const char* error_message);

// Callback for disk information retreival calls.
typedef void (*RequestMountInfoCallback)(void* object,
                                         const char** devices,
                                         size_t devices_len,
                                         MountMethodErrorType error,
                                         const char* error_message);

// Initiates mount operation for a given |device_path|. When the operation
// completes, the callback will be invoked with appropriate |error| parameter
// indicating operation's outcome.
extern void (*MountRemovableDevice)(const char* device_path,
                                    MountRequestCallback callback,
                                    void* object);

// Initiates unmount operation for a given |device_path|. When the operation
// completes, the callback will be invoked with appropriate |error| parameter
// indicating operation's outcome.
extern void (*UnmountRemovableDevice)(const char* device_path,
                                     MountRequestCallback callback,
                                     void* object);

// Initiates retrieval of information about given |service_path| representing
// disk drive.
extern void (*GetDiskProperties)(const char* service_path,
                                 GetDiskPropertiesCallback callback,
                                 void* object);

// Initiates retrieval of information about all mounted disks. Please note that
// |callback| will be called once for each disk found on the system.
// This routine will skip all drives that are mounted form the device that
// system was booted from.
extern void (*RequestMountInfo)(RequestMountInfoCallback callback,
                                void* object);

// Mounts a given device path. If successful, a DISK_CHANGED event will fire
// after the call. Returns false on failure.
extern bool (*MountDevicePath)(const char* device_path);

// Unmounts a given device path. If successful, a DISK_CHANGED event will fire
// after the call. Returns false on failure.
extern bool (*UnmountDevicePath)(const char* device_path);

// Obsolete methods, kept here just as sacrifice to ChromeOS build gods.
// This block will be removed in the next iteration.
struct DiskStatus {
  const char* path;
  const char* mountpath;
  const char* systempath;
  bool isparent;
  bool hasmedia;
};

struct MountStatus {
  DiskStatus *disks;
  int size;
};
class OpaqueMountStatusConnection;
typedef OpaqueMountStatusConnection* MountStatusConnection;
extern MountStatus* (*RetrieveMountInformation)();
extern void (*FreeMountStatus)(MountStatus* status);
extern bool (*IsBootDevicePath)(const char* device_path);
typedef void(*MountMonitor)(void*,
                            const MountStatus&,
                            MountEventType,
                            const char*);
extern MountStatusConnection (*MonitorMountStatus)(MountMonitor monitor, void*);
extern void (*DisconnectMountStatus)(MountStatusConnection connection);

}  // namespace chromeos

#endif  // CHROMEOS_MOUNT_H_
