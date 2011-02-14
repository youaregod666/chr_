// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
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

// Register a handler that will be called when the system resumes from sleeping.
extern ResumeConnection (*MonitorResume)(ResumeMonitor monitor, void* object);

// Unregister the handler. Takes the ResumeConnection got from MonitorResume().
extern void (*DisconnectResume)(ResumeConnection connection);

}  // namespace chromeos

#endif  // CHROMEOS_RESUME_H_
