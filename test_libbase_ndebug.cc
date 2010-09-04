// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"

// We don't install this program, or even run it -- it's just here to give this
// package a target that will fail to compile if NDEBUG is being defined now
// but wasn't when libbase was compiled, or vice versa.  See InitLogging() in
// libbase/logging.h for more details.
int main(int argc, char** argv) {
  logging::InitLogging(NULL,
                       logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
                       logging::DONT_LOCK_LOG_FILE,
                       logging::APPEND_TO_OLD_LOG_FILE);
  return 0;
}
