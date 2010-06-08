// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SPEECH_SYNTHESIS_H_
#define CHROMEOS_SPEECH_SYNTHESIS_H_

namespace chromeos { // NOLINT

class OpaqueTTSInitConnection;
typedef OpaqueTTSInitConnection* TTSInitConnection;
typedef void(*InitStatusCallback)(bool success);

extern bool (*Speak)(const char* text);
extern bool (*SetSpeakProperties)(const char* props);
extern bool (*StopSpeaking)();
extern bool (*IsSpeaking) ();
extern void (*InitTts) (InitStatusCallback);

}  // namespace chromeos

#endif  // CHROMEOS_SPEECH_SYNTHESIS_H_
