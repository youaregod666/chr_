// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UPDATE_ENGINE_H_
#define CHROMEOS_UPDATE_ENGINE_H_

#include <string>

#include "base/basictypes.h"

namespace chromeos {

// Edges for state machine
//    IDLE->CHECKING_FOR_UPDATE
//    CHECKING_FOR_UPDATE->IDLE
//    CHECKING_FOR_UPDATE->UPDATE_AVAILABLE
//    ...
//    FINALIZING->UPDATE_NEED_REBOOT

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
  UpdateProgress() : status_(UPDATE_STATUS_IDLE), download_progress_(0.0),
      last_checked_time_(0), new_version_(0), new_size_(0), destruct_(0) {
  }
  ~UpdateProgress() {
      if (destruct_) destruct_(*this);
  }

  UpdateStatusOperation status_;
  double download_progress_;  // 0.0 - 1.0
  int64_t last_checked_time_;  // as reported by std::time()
  const char* new_version_;
  int64_t new_size_;  // valid during DOWNLOADING, in bytes

// private:
  void (*destruct_)(const UpdateProgress&);
  DISALLOW_COPY_AND_ASSIGN(UpdateProgress);
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
