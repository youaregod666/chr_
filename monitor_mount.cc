// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>
#include <glib-object.h>
#include <base/logging.h>

#include <iostream>  // NOLINT

#include "chromeos_cros_api.h"  // NOLINT
#include "chromeos_mount.h" // NOLINT
#include "monitor_utils.h" //NOLINT

// \file This is a simple console application which will monitor the mount
// status to std::cout and disconnect after it has reported DISK_ADDED event 50
// times.

void PrintDiskInfo(const chromeos::DiskInfo* info1) {
  const chromeos::DiskInfoAdvanced* info =
      reinterpret_cast<const chromeos::DiskInfoAdvanced*>(info1);
  std::cout << "    Device path: "
            << (info->path() ? info->path() : "" ) << std::endl;
  std::cout << "    Mount path: "
            << (info->mount_path() ? info->mount_path() : "" ) << std::endl;
  std::cout << "    System path: "
            << (info->system_path() ? info->system_path() : "" ) << std::endl;
  std::cout << "    File path: "
            << (info->file_path() ? info->file_path() : "" ) << std::endl;
  std::cout << "    Device label: "
            << (info->label() ? info->label() : "" ) << std::endl;
  std::cout << "    Drive label: "
            << (info->drive_label() ? info->drive_label() : "" ) << std::endl;
  std::cout << "    Parent path: "
            << (info->partition_slave() ? info->partition_slave() : "" )
            << std::endl;
  std::cout << "    Device type: " << info->device_type() << std::endl;
  std::cout << "    Is drive: " << (info->is_drive() ? "true" : "false" )
            << std::endl;
  std::cout << "    Has media: " << (info->has_media() ? "true" : "false" )
            << std::endl;
  std::cout << "    Is on boot device: "
            << (info->on_boot_device() ? "true" : "false" ) << std::endl;
  std::cout << "    Is read only: "
            << (info->is_read_only() ? "true" : "false" ) << std::endl;
  std::cout << "    Size: " << info->size() << std::endl;

}


void GetDiskPropertiesResponse(void* object,
                               const char* device_path,
                               const chromeos::DiskInfo* disk,
                               chromeos::MountMethodErrorType error,
                               const char* error_message) {

  std::cout << "-------------------------------------------------" << std::endl;
  if (error) {
    std::cout << "Getting disk info for"
              << (device_path ? device_path : "NULL") << " failed with:"
              << (error_message ? error_message : "Unknown error.")
              << std::endl;
  } else {
    std::cout << "Got disk info for "
              << (device_path ? device_path : "NULL") << ":" << std::endl;
    PrintDiskInfo(disk);
  }
  std::cout << "-------------------------------------------------" << std::endl;
}

// Callback is an example object which can be passed to MonitorMountStatus.
class Monitor {
 public:
  // You can store whatever state is needed in the function object.
  explicit Monitor(GMainLoop* loop) :
    count_(0),
    loop_(loop) {
  }

  static void OnMountEvent(void* object,
                           chromeos::MountEventType evt,
                           const char* path) {
    Monitor* self = static_cast<Monitor*>(object);

    switch (evt) {
      case (chromeos::DISK_ADDED):
        std::cout << "New disk detected: " << path << std::endl;
        chromeos::GetDiskProperties(path, &GetDiskPropertiesResponse, NULL);
        break;
      case (chromeos::DISK_REMOVED):
        std::cout << "Disk removed: " << path << std::endl;
        return;
      case (chromeos::DEVICE_ADDED):
        std::cout << "Device detected: " << path << std::endl;
        return;
      case (chromeos::DEVICE_REMOVED):
        std::cout << "Device removed: " << path << std::endl;
        return;
      case (chromeos::DEVICE_SCANNED):
        std::cout << "Device scanned: " << path << std::endl;
        return;
      default:
        return;
    }

    ++self->count_;
    if (self->count_ == 50)
      ::g_main_loop_quit(self->loop_);
  }

  static void OnMountCompletedEvent(void* object,
                                    chromeos::MountError error_code,
                                    const char* source_path,
                                    chromeos::MountType mount_type,
                                    const char* mount_path) {
    std::string type_str = "";

    switch (mount_type) {
      case (chromeos::MOUNT_TYPE_DEVICE):
        type_str = "Device";
        break;
      case (chromeos::MOUNT_TYPE_ARCHIVE):
        type_str = "Archive";
        break;
      case(chromeos::MOUNT_TYPE_NETWORK_STORAGE):
        type_str = "Network storage";
        break;
      default:
        type_str = "Unknown mount point type";
    }

    if (error_code) {
      std::cout << type_str << " could not be mounted: "
                << (source_path ? source_path : "NULL") << std::endl;
    } else {
      std::cout << type_str << " " << (source_path ? source_path : "NULL")
                << " has been mounted to: "
                << (mount_path ? mount_path : "NULL") << std::endl;
    }
  }

 private:
  int count_;
  GMainLoop* loop_;
};

void MountInfoResponse(void* object,
                       const char** devices,
                       size_t devices_len,
                       chromeos::MountMethodErrorType error,
                       const char* error_message) {
  if (error) {
    std::cout << "Unable to get device list: "
              << (error_message ? error_message : "Unknown error.")
              << std::endl;
  } else {
    for (size_t i = 0; i < devices_len; i++) {
      std::cout << "Requesting info for: " << devices[i] << std::endl;
      chromeos::GetDiskProperties(devices[i], &GetDiskPropertiesResponse, NULL);
    }
  }
}

int main(int argc, const char** argv) {
  // Initialize the g_type systems an g_main event loop, normally this would be
  // done by chrome.
  ::g_type_init();

  DCHECK(LoadCrosLibrary(argv)) << "Failed to load cros .so";

  GMainLoop* loop = ::g_main_loop_new(NULL, false);

  // Display information about the mount system
  chromeos::RequestMountInfo(&MountInfoResponse, NULL);

  Monitor monitor(loop);

  chromeos::MountEventConnection connection =
      chromeos::MonitorAllMountEvents(&Monitor::OnMountEvent,
                                      &Monitor::OnMountCompletedEvent,
                                      &monitor);


  ::g_main_loop_run(loop);

  // When we're done, we disconnect the mount status.
  chromeos::DisconnectMountEventMonitor(connection);

  return 0;
}
