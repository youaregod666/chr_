// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/types.h>
#include <unistd.h>

#include "chromeos_speech_synthesis.h" // NOLINT

#include <base/string_util.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/dbus/service_constants.h>
#include <chromeos/glib/object.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "marshal.glibmarshal.h"  // NOLINT

namespace chromeos {

class OpaqueTTSInitConnection {
 public:
  explicit OpaqueTTSInitConnection(const InitStatusCallback& callback)
      : init_callback_(callback) {
  }

  virtual ~OpaqueTTSInitConnection() {}

  void NotifySuccess(bool success) {
    if (init_callback_ != NULL) {
      init_callback_(success);
    }
  }

 private:
  InitStatusCallback init_callback_;

  DISALLOW_COPY_AND_ASSIGN(OpaqueTTSInitConnection);
};

// A message filter to receive signals.
DBusHandlerResult Filter(DBusConnection* connection,
                         DBusMessage* message,
                         void* object) {
  TTSInitConnection self = static_cast<TTSInitConnection>(object);
  if (dbus_message_is_signal(message, chromium::kChromiumInterface,
                             chromium::kTTSReadySignal)) {
    self->NotifySuccess(true);
    return DBUS_HANDLER_RESULT_HANDLED;
  } else if (dbus_message_is_signal(message, chromium::kChromiumInterface,
                                    chromium::kTTSFailedSignal)) {
    self->NotifySuccess(false);
    return DBUS_HANDLER_RESULT_HANDLED;
  } else {
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }
}

// Starts the speech synthesizer service registered under the name
// "org.chromium.SpeechSynthesizer", by sending a message over DBus.
bool StartTtsService() {
  bool ret = true;
  DBusMessage *message;
  dbus_uint32_t flag;
  dbus_bool_t result;
  DBusError error;
  dbus_error_init (&error);
  DBusConnection* connection = dbus_g_connection_get_connection(
      dbus::GetSystemBusConnection().g_connection());
  if (dbus_error_is_set(&error)) {
    DLOG(ERROR) << "Error getting dbus connection: " << error.message;
    return false;
  }
  message = dbus_message_new_method_call("org.freedesktop.DBus",
                                         "/org/freedesktop/DBus",
                                         "org.freedesktop.DBus",
                                         "StartServiceByName");
  if (!message) {
    DLOG(ERROR) << "Error creating DBus message.";
    return false;
  }
  dbus_message_set_no_reply(message, TRUE);
  dbus_message_append_args(message, DBUS_TYPE_STRING,
      &speech_synthesis::kSpeechSynthesizerServiceName,
      DBUS_TYPE_UINT32, &flag, DBUS_TYPE_INVALID);
  // TODO(chaitanyag): Ideally, we want to receive a reply from DBus indicating
  // whether it was able to start the service. |result| here just indicates
  // whether the message was sent successfully.
  result = dbus_connection_send(connection, message, NULL);
  if (result == TRUE) {
    DLOG(INFO) << "Successfully activating service " <<
        speech_synthesis::kSpeechSynthesizerServiceName;
  } else {
    DLOG(ERROR) << "Failed to activate service" <<
        speech_synthesis::kSpeechSynthesizerServiceName;
    ret = false;
  }
  dbus_message_unref(message);
  return ret;
}

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

#define SAFE_MESSAGE(e) (e.message ? e.message : "unknown error")

extern "C"
void
ChromeOSInitTts(InitStatusCallback callback) {
  const std::string filter = StringPrintf("type='signal', interface='%s'",
                                          chromium::kChromiumInterface);
  DBusError error;
  dbus_error_init(&error);
  DBusConnection* connection = ::dbus_g_connection_get_connection(
      dbus::GetSystemBusConnection().g_connection());
  dbus_bus_add_match(connection, filter.c_str(), &error);
  if (dbus_error_is_set(&error)) {
    DLOG(WARNING) << "Failed to add a filter:" << error.name << ", message="
                  << SAFE_MESSAGE(error);
    return;
  }
  TTSInitConnection result = new OpaqueTTSInitConnection(callback);
  CHECK(dbus_connection_add_filter(connection, &Filter, result, NULL));
  StartTtsService();
}

}  // namespace chromeos
