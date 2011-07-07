// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UPDATE_H_
#define CHROMEOS_UPDATE_H_

#include <cstddef>

#include <base/basictypes.h>

namespace chromeos {

// NOTE: All functions in the file will be invoked on a thread other than the
// main thread in Chrome. The code must be thread safe and reentrant.

enum UpdateStatus {
  // An error occurred.
  UPDATE_ERROR,
  // An update is available.
  UPDATE_IS_AVAILABLE,
  // The upgrade happened successfully.
  UPDATE_SUCCESSFUL,
  // No need to upgrade, we are up to date.
  UPDATE_ALREADY_UP_TO_DATE,
};

// This simple class is DLL safe. We "virtualize" the destructor with
// a proc-pointer to avoid cross DLL dependencies on allocators (and vtables).

struct UpdateInformation {
  UpdateInformation() : status_(UPDATE_ERROR), version_(NULL), destruct_(NULL) {
  }
  ~UpdateInformation() {
    if (destruct_) destruct_(*this);
  }
  UpdateStatus status_;
  const char* version_;

// private:
  void (*destruct_)(const UpdateInformation&);
};

}  // namespace chromeos

#endif  // CHROMEOS_UPDATE_H_
