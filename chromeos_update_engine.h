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
// Any state can transition to REPORTING_ERROR_EVENT and then on to IDLE.

enum UpdateStatusOperation {
  UPDATE_STATUS_ERROR = -1,
  UPDATE_STATUS_IDLE = 0,
  UPDATE_STATUS_CHECKING_FOR_UPDATE,
  UPDATE_STATUS_UPDATE_AVAILABLE,
  UPDATE_STATUS_DOWNLOADING,
  UPDATE_STATUS_VERIFYING,
  UPDATE_STATUS_FINALIZING,
  UPDATE_STATUS_UPDATED_NEED_REBOOT,
  UPDATE_STATUS_REPORTING_ERROR_EVENT
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

// Tell UpdateEngine daemon to reboot the system if an update has been
// downloaded and installed. Returns true on success.
extern bool (*RebootIfUpdated)();

// Asynchronous API.

// Asynchronously poll UpdateEngine once for its state and call |callback|
// with the result.
extern void (*RequestUpdateStatus)(UpdateMonitor callback, void* user_data);

// Asynchronously tell UpdateEngine daemon to check for an update.
// If |callback| is non NULL, call with the result when the request completes.
enum UpdateResult {
  UPDATE_RESULT_SUCCESS,
  UPDATE_RESULT_FAILED,
  UPDATE_RESULT_DBUS_FAILED
};
typedef void (*UpdateCallback)(void* user_data,
                               UpdateResult result,
                               const char* error_msg);
extern void (*RequestUpdateCheck)(UpdateCallback callback, void* user_data);
// Set the release track (channel) asynchronously. |track| should look like
// "beta-channel" or "dev-channel".
extern void (*SetUpdateTrack)(const std::string& track);
// Request the track and call |callback| with the track when complete.
// On error, call |callback| with a NULL string.
typedef void (*UpdateTrackCallback)(void* user_data, const char* track);
extern void (*RequestUpdateTrack)(UpdateTrackCallback callback,
                                  void* user_data);

}  // namespace chromeos

#endif  // CHROMEOS_UPDATE_ENGINE_H_
