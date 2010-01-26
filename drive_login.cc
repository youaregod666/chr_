// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>
#include <glib-object.h>

#include <iostream>  // NOLINT

#include <base/logging.h>

#include "chromeos_cros_api.h"  // NOLINT
#include "chromeos_login.h"
#include "monitor_utils.h" //NOLINT

int main(int argc, const char** argv) {
  // Initialize the g_type systems an g_main event loop, normally this would be
  // done by chrome.
  ::g_type_init();
  GMainLoop* loop = ::g_main_loop_new(NULL, false);
  DCHECK(loop);

  DCHECK(LoadCrosLibrary(argv)) << "Failed to load cros .so";

  if (chromeos::EmitLoginPromptReady())
    LOG(INFO) << "Emitted!";
  else
    LOG(FATAL) << "Emitting login-prompt-ready failed.";

  if (chromeos::StartSession("foo@bar.com", ""))
    LOG(INFO) << "Started session!";
  else
    LOG(FATAL) << "Starting session failed.";

  return 0;
}
