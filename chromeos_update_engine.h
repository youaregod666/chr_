// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UPDATE_ENGINE_H_
#define CHROMEOS_UPDATE_ENGINE_H_

#include <inttypes.h>
#include <string>

namespace chromeos {

enum UpdateStatusOperation {
  UPDATE_STATUS_ERROR = -1,
  UPDATE_STATUS_IDLE = 0,
  UPDATE_STATUS_CHECKING_FOR_UPDATE,
  UPDATE_STATUS_UPDATE_AVAILABLE,
  UPDATE_STATUS_DOWNLOADING,
  UPDATE_STATUS_VERIFYING,
  UPDATE_STATUS_FINALIZING,
  UPDATE_STATUS_UPDATED_NEED_REBOOT
};

struct UpdateProgress {
  UpdateStatusOperation status_;
  double download_progress_;
  int64_t last_checked_time_;
  std::string new_version_;
  int64_t new_size_;
};

class OpaqueUpdateStatusConnection;
typedef OpaqueUpdateStatusConnection* UpdateStatusConnection;
typedef void(*UpdateMonitor)(void*, const UpdateProgress&);

// Register an UpdateMonitor callback.
extern UpdateStatusConnection (*MonitorUpdateStatus)(UpdateMonitor monitor,
                                                     void*);
// Unregister an UpdateMonitor callback.
extern void (*DisconnectUpdateProgress)(UpdateStatusConnection connection);

// Poll for the status once. Returns true on success.
extern bool (*RetrieveUpdateProgress)(UpdateProgress* information);
// Tell UpdateEngine daemon to check for an update. Returns true on success.
extern bool (*InitiateUpdateCheck)();

}  // namespace chromeos

#endif  // CHROMEOS_UPDATE_ENGINE_H_
