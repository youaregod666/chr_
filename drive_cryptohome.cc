// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>
#include <glib-object.h>

#include <iostream>  // NOLINT

#include <base/command_line.h>
#include <base/logging.h>
#include <base/string_util.h>

#include "chromeos_cryptohome.h"
#include "monitor_utils.h" //NOLINT

static const char kDoUnmount[] = "do-unmount";
static const char kTpmStatus[] = "tpm-status";

int main(int argc, const char** argv) {
  CommandLine::Init(argc, argv);
  logging::InitLogging(NULL,
                       logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
                       logging::DONT_LOCK_LOG_FILE,
                       logging::APPEND_TO_OLD_LOG_FILE);
  CommandLine *cl = CommandLine::ForCurrentProcess();
  std::vector<std::wstring> loose_wide_args = cl->GetLooseValues();

  // Initialize the g_type systems an g_main event loop, normally this would be
  // done by chrome.
  ::g_type_init();
  GMainLoop* loop = ::g_main_loop_new(NULL, false);
  CHECK(loop);

  CHECK(LoadCrosLibrary(argv)) << "Failed to load cros .so";

  if (cl->HasSwitch(kTpmStatus)) {
    LOG(INFO) << "TPM Enabled: " << chromeos::CryptohomeTpmIsEnabled();
    LOG(INFO) << "TPM Ready: " << chromeos::CryptohomeTpmIsReady();
    std::string tpm_password;
    chromeos::CryptohomeTpmGetPassword(&tpm_password);
    LOG(INFO) << "TPM Password: " << tpm_password;
  }

  std::string name = WideToASCII(loose_wide_args[0]);
  std::string hash = WideToASCII(loose_wide_args[1]);
  LOG(INFO) << "Trying " << name << " " << hash;
  CHECK(chromeos::CryptohomeCheckKey(name.c_str(), hash.c_str())) <<
      "Credentials are no good on this device";
  CHECK(chromeos::CryptohomeMount(name.c_str(), hash.c_str())) <<
      "Cannot mount cryptohome for " << name;
  CHECK(chromeos::CryptohomeIsMounted()) <<
      "Cryptohome was mounted, but is now gone???";
  if (cl->HasSwitch(kDoUnmount)) {
    CHECK(chromeos::CryptohomeUnmount()) << "Cryptohome cannot be unmounted???";
  }
  LOG(INFO) << "All calls succeeded for " << name << ", " << hash;
  return 0;
}
