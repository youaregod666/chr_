// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>
#include <glib-object.h>
#include <map>
#include <vector>

#include <base/logging.h>
#include <base/time.h>
#include <base/values.h>

#include "chromeos_cros_api.h"  // NOLINT
#include "chromeos_network.h"  // NOLINT
#include "chromeos/dbus/dbus.h"  // NOLINT
#include "chromeos/glib/object.h"  // NOLINT
#include "monitor_utils.h" //NOLINT

class CallbackSMSMonitor {
 public:
  explicit CallbackSMSMonitor()
  {
  }

  static void Run(void *object,
                  const char* modem_device_path,
                  const chromeos::SMS* sms) {

    LOG(INFO) << "SMS callback.";
    LOG(INFO) << "SMS number: '" << sms->number << "'";
    LOG(INFO) << "SMS text: '" << sms->text << "'";
    struct timeval sms_tv = sms->timestamp.ToTimeVal();
    struct tm sms_tm;
    gmtime_r(&sms_tv.tv_sec, &sms_tm);
    char buf[64];
    strftime(buf, sizeof(buf), "%c", &sms_tm);
    LOG(INFO) << "SMS time: " << buf;
    if (sms->smsc)
      LOG(INFO) << "SMS SMSC: '" << sms->smsc << "'";
    if (sms->validity != -1)
      LOG(INFO) << "SMS validity: '" << sms->validity << "'";
    if (sms->msgclass != -1)
      LOG(INFO) << "SMS class: '" << sms->msgclass << "'";
  }

};

// A simple example program demonstrating how to use the ChromeOS SMS API.
int main(int argc, const char** argv) {
  ::g_type_init();
  GMainLoop* loop = ::g_main_loop_new(NULL, false);

  DCHECK(loop) << "Failed to create main loop";
  if (!LoadCrosLibrary(argv))
    LOG(INFO) << "Failed to load cros .so";

  if (argc != 2) {
    LOG(INFO) << "Program must be invoked with one modem device path argument.";
    return 1;
  }

  LOG(INFO) << "Requesting SMS messages on modem '" << argv[1] << "'";
  CallbackSMSMonitor callback_sms;
  chromeos::SMSMonitor smsmon =
      chromeos::MonitorSMS(argv[1],
                           &CallbackSMSMonitor::Run,
                           &callback_sms);

  LOG(INFO) << "Starting g_main_loop.";

  ::g_main_loop_run(loop);

  LOG(INFO) << "Shutting down.";

  chromeos::DisconnectSMSMonitor(smsmon);
  return 0;
}
