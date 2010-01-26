// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_update.h"

#include "chromeos/string.h"

#include <unistd.h>

#include <cstdlib>

namespace chromeos {

namespace {

// This is the "virtualized" destructor for UpdateInformation.
void Destruct(const UpdateInformation& x) {
  delete x.version_;
}

}  // namespace

extern "C"
bool ChromeOSUpdate(UpdateInformation* information) {
  // This code mochs in the UpdateInformation. To be filled in by Andrew.

  // simulate time to check.
  ::sleep(static_cast<unsigned>(std::rand()) % 30);

  information->destruct_ = &Destruct;

  static int call = 0;

  if (call == 0) {
      information->status_ = UPDATE_SUCCESSFUL;
      information->version_ = NewStringCopy("Updated to 2.0");
  }
  if (call == 1) {
    information->status_ = UPDATE_ALREADY_UP_TO_DATE;
    information->version_ = NewStringCopy("Already running 1.0");
  }
  if (call == 2) {
    information->status_ = UPDATE_ERROR;
    information->version_ = NewStringCopy("Aw, Snap! 1.5");
  }

  ++call; call %= 3;

  return information->status_ != UPDATE_ERROR;
}

extern "C"
bool ChromeOSCheckForUpdate(UpdateInformation* information) {
  // This code mochs in the UpdateInformation. To be filled in by Andrew.

  // simulate time to check.
  ::sleep(static_cast<unsigned>(std::rand()) % 30);

  information->destruct_ = &Destruct;

  static int call = 0;

  if (call == 0) {
      information->status_ = UPDATE_IS_AVAILABLE;
      information->version_ = NewStringCopy("2.0 is available");
  }
  if (call == 1) {
    information->status_ = UPDATE_ALREADY_UP_TO_DATE;
    information->version_ = NewStringCopy("Already running 1.1");
  }
  if (call == 2) {
    information->status_ = UPDATE_ERROR;
    information->version_ = NewStringCopy("Aw, Snap! 1.6");
  }

  ++call; call %= 3;

  return information->status_ != UPDATE_ERROR;
}

}  // namespace chromeos
