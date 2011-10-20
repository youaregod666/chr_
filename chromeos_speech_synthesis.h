// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SPEECH_SYNTHESIS_H_
#define CHROMEOS_SPEECH_SYNTHESIS_H_

namespace chromeos { // NOLINT

class OpaqueTTSInitConnection;
typedef OpaqueTTSInitConnection* TTSInitConnection;
typedef void(*InitStatusCallback)(bool success);

}  // namespace chromeos

#endif  // CHROMEOS_SPEECH_SYNTHESIS_H_
