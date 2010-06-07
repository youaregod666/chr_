// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unistd.h>

#include "chromeos_speech_synthesis.h" // NOLINT

#include <chromeos/dbus/dbus.h>
#include <chromeos/dbus/service_constants.h>
#include <chromeos/glib/object.h>

#include "marshal.glibmarshal.h"  // NOLINT

namespace chromeos {

extern "C"
bool ChromeOSSpeak(const char* text) {
  g_type_init();
  chromeos::dbus::BusConnection bus = chromeos::dbus::GetSystemBusConnection();
  chromeos::dbus::Proxy tts_proxy(bus,
      speech_synthesis::kSpeechSynthesizerServiceName,
      speech_synthesis::kSpeechSynthesizerServicePath,
      speech_synthesis::kSpeechSynthesizerInterface);
  DCHECK(tts_proxy.gproxy()) << "Failed to acquire proxy";
  gboolean done = false;
  chromeos::glib::ScopedError error;
  if (!::dbus_g_proxy_call(tts_proxy.gproxy(),
                           "Speak",
                           &Resetter(&error).lvalue(),
                           G_TYPE_STRING,
                           text,
                           G_TYPE_INVALID,
                           G_TYPE_BOOLEAN,
                           &done,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << "Speak" << " failed: "
                 << (error->message ? error->message : "Unknown Error.");
    return false;
  }
  return true;
}

extern "C"
bool ChromeOSSetSpeakProperties(const char* props) {
  g_type_init();
  chromeos::dbus::BusConnection bus = chromeos::dbus::GetSystemBusConnection();
  chromeos::dbus::Proxy tts_proxy(bus,
      speech_synthesis::kSpeechSynthesizerServiceName,
      speech_synthesis::kSpeechSynthesizerServicePath,
      speech_synthesis::kSpeechSynthesizerInterface);
  DCHECK(tts_proxy.gproxy()) << "Failed to acquire proxy";
  gboolean done = false;
  chromeos::glib::ScopedError error;
  if (!::dbus_g_proxy_call(tts_proxy.gproxy(),
                           "SetProperties",
                           &Resetter(&error).lvalue(),
                           G_TYPE_STRING,
                           props,
                           G_TYPE_INVALID,
                           G_TYPE_BOOLEAN,
                           &done,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << "SetProperties" << " failed: "
                 << (error->message ? error->message : "Unknown Error.");
    return false;
  }
  return true;
}

extern "C"
bool ChromeOSStopSpeaking() {
  g_type_init();
  chromeos::dbus::BusConnection bus = chromeos::dbus::GetSystemBusConnection();
  chromeos::dbus::Proxy tts_proxy(bus,
      speech_synthesis::kSpeechSynthesizerServiceName,
      speech_synthesis::kSpeechSynthesizerServicePath,
      speech_synthesis::kSpeechSynthesizerInterface);
  DCHECK(tts_proxy.gproxy()) << "Failed to acquire proxy";
  gboolean done = false;
  chromeos::glib::ScopedError error;
  if (!::dbus_g_proxy_call(tts_proxy.gproxy(),
                           "Stop",
                           &Resetter(&error).lvalue(),
                           G_TYPE_INVALID,
                           G_TYPE_BOOLEAN,
                           &done,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << "Stop" << " failed: "
                 << (error->message ? error->message : "Unknown Error.");
    return false;
  }
  return true;
}

extern "C"
bool ChromeOSIsSpeaking() {
  g_type_init();
  chromeos::dbus::BusConnection bus = chromeos::dbus::GetSystemBusConnection();
  chromeos::dbus::Proxy tts_proxy(bus,
      speech_synthesis::kSpeechSynthesizerServiceName,
      speech_synthesis::kSpeechSynthesizerServicePath,
      speech_synthesis::kSpeechSynthesizerInterface);
  DCHECK(tts_proxy.gproxy()) << "Failed to acquire proxy";
  gboolean done = false;
  chromeos::glib::ScopedError error;
  if (!::dbus_g_proxy_call(tts_proxy.gproxy(),
                           "IsSpeaking",
                           &Resetter(&error).lvalue(),
                           G_TYPE_INVALID,
                           G_TYPE_BOOLEAN,
                           &done,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << "Speak" << " failed: "
                 << (error->message ? error->message : "Unknown Error.");
    return false;
  }
  return done;
}

}  // namespace chromeos
