// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>
#include <glib-object.h>

#include <iostream>  // NOLINT

#include <base/logging.h>

#include "chromeos_cros_api.h"  // NOLINT
#include "chromeos_update_engine.h"
#include "monitor_utils.h" //NOLINT

using std::cout;
using std::endl;

namespace {
  
void PrintUpdateProgress(const chromeos::UpdateProgress& progress) {
  const char* op = "";
  switch (progress.status_) {
    case chromeos::UPDATE_STATUS_IDLE:
      op = "IDLE";
      break;
    case chromeos::UPDATE_STATUS_CHECKING_FOR_UPDATE:
      op = "CHECKING_FOR_UPDATE";
      break;
    case chromeos::UPDATE_STATUS_UPDATE_AVAILABLE:
      op = "UPDATE_AVAILABLE";
      break;
    case chromeos::UPDATE_STATUS_DOWNLOADING:
      op = "DOWNLOADING";
      break;
    case chromeos::UPDATE_STATUS_VERIFYING:
      op = "VERIFYING";
      break;
    case chromeos::UPDATE_STATUS_FINALIZING:
      op = "FINALIZING";
      break;
    case chromeos::UPDATE_STATUS_UPDATED_NEED_REBOOT:
      op = "UPDATED_NEED_REBOOT";
      break;
  }
  
  cout << "status_: " << op << endl;
  cout << "download_progress_: " << progress.download_progress_ << endl;
  cout << "last_checked_time_: " << progress.last_checked_time_ << endl;
  cout << "new_version_: " << progress.new_version_ << endl;
  cout << "new_size_: " << progress.new_size_ << endl;
  cout << endl;
}

void TestUpdateMonitor(void* unused, const chromeos::UpdateProgress& progress) {
  cout << "Monitor got status:" << endl;
  PrintUpdateProgress(progress);
}

gboolean ExitTimerCallback(void* arg) {
  cout << "Exiting..." << endl;
  g_main_loop_quit(reinterpret_cast<GMainLoop*>(arg));
  return FALSE;  // Don't call this callback again
}

gboolean GetStatusTimerCallback(void* unused) {
  chromeos::UpdateProgress progress;
  bool success = chromeos::RetrieveUpdateProgress(&progress);
  if (!success) {
    cout << "ERROR: RetrieveUpdateProgress() failed." << endl;
    return FALSE;  // Don't call this callback again
  }
  cout << "Polled for status and got:" << endl;
  PrintUpdateProgress(progress);
  return TRUE;  // Keep calling back
}

}  // namespace {}

int main(int argc, const char** argv) {
  // Initialize the g_type systems an g_main event loop, normally this would be
  // done by chrome.
  ::g_type_init();
  GMainLoop* loop = ::g_main_loop_new(NULL, false);
  bool success = LoadCrosLibrary(argv);
  DCHECK(success) << "Failed to load cros .so";

  chromeos::UpdateStatusConnection connection =
      chromeos::MonitorUpdateStatus(&TestUpdateMonitor, NULL);

  // Callback to quit after 60 seconds:
  g_timeout_add(60 * 1000, &ExitTimerCallback, loop);
  
  // Poll for status every 5 seconds:
  g_timeout_add(5 * 1000, &GetStatusTimerCallback, NULL);

  ::g_main_loop_run(loop);

  // When we're done, we disconnect the power status.
  chromeos::DisconnectUpdateProgress(connection);

  return 0;
}
