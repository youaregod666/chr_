// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_RESUME_H_
#define CHROMEOS_RESUME_H_

namespace chromeos {

// Signature of function invoked to handle system resume. Argument
// is the object passed to MonitorResume().
typedef void (*ResumeMonitor)(void*);

class OpaqueResumeConnection;
typedef OpaqueResumeConnection* ResumeConnection;

}  // namespace chromeos

#endif  // CHROMEOS_RESUME_H_
