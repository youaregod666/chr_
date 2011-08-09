// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>

#include <base/basictypes.h>
#include <base/command_line.h>
#include <base/string_split.h>
#include <base/string_util.h>
#include <chromeos/dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <glib.h>

#include "bindings_client.h"

// This is modified from image_burner_tester.cc.

namespace chromeos {

static const char* kTestSignalInterface =
    "org.chromium.TestLibCrosServiceInterface";
static const char* kTestSignalNameNetworkProxyResolved =
    "test_network_proxy_resolved";

class TestClient {
 public:
  TestClient(dbus::Proxy request_proxy, GMainLoop* loop)
      : request_proxy_(request_proxy),
        loop_(loop),
        num_signals_(0) {
  }

  ~TestClient() {
    // Remove filter from connection.
    DBusConnection* conn = ::dbus_g_connection_get_connection(
        dbus::GetSystemBusConnection().g_connection());
    ::dbus_connection_remove_filter(conn, &TestClient::FilterMessage, this);
  }

  bool Initialize() {
    // Add filter for signal(s).
    const std::string filter = StringPrintf("type='signal', interface='%s'",
                                            kTestSignalInterface);
    DBusConnection* connection = ::dbus_g_connection_get_connection(
        dbus::GetSystemBusConnection().g_connection());
    if (!connection) {
      std::cout << "Can't get system bus connection" << std::endl;
      return false;
    }
    DBusError error;
    ::dbus_error_init(&error);
    ::dbus_bus_add_match(connection, filter.c_str(), &error);
    if (::dbus_error_is_set(&error)) {
      std::cout << "Failed to add match: " << error.name << ", message="
                << (error.message ? error.message : "unknown error")
                << std::endl;
      return false;
    }
    if (!::dbus_connection_add_filter(connection, &TestClient::FilterMessage,
                                      this, NULL)) {
      std::cout << "Failed to add filter." << std::endl;
      return false;
    }
    return true;
  }

  void ResolveNetworkProxy(const char* source_url) {
    glib::ScopedError error;
    std::cout << "ResolveNetworkProxy: start" << std::endl;
    if (!::dbus_g_proxy_call(request_proxy_.gproxy(),
                            "ResolveNetworkProxy",
                             &Resetter(&error).lvalue(),
                             G_TYPE_STRING, source_url,
                             G_TYPE_STRING, kTestSignalInterface,
                             G_TYPE_STRING, kTestSignalNameNetworkProxyResolved,
                             G_TYPE_INVALID, G_TYPE_INVALID)) {
      std::cout << "Resolving failed: " << error->message << std::endl;
    } else {
      ++num_signals_;
      std::cout << "Resolving started for " << source_url << "..." << std::endl;
    }
    std::cout << "ResolveNetworkProxy: end++++++++++++++++\n" << std::endl;
  }

  void ReceivedSignal() {
    --num_signals_;
  }

  void RunLoopIfNecessary() {
    if (num_signals_)
      ::g_main_loop_run(loop_);
  }

  void QuitLoopIfNecessary() {
    if (!num_signals_)
      ::g_main_loop_quit(loop_);
  }

 private:
  static DBusHandlerResult FilterMessage(DBusConnection* connection,
                                         DBusMessage* message,
                                         void* object) {
    if (dbus_message_is_signal(message, kTestSignalInterface,
                               kTestSignalNameNetworkProxyResolved)) {
      std::cout << "Filter: received signal "
                << kTestSignalNameNetworkProxyResolved << std::endl;
      // Retreive arguments from message.
      char* source_url = NULL;
      char* proxy_list = NULL;
      char* error = NULL;
      DBusError arg_error;
      ::dbus_error_init(&arg_error);
      if (!::dbus_message_get_args(message, &arg_error,
                                   DBUS_TYPE_STRING, &source_url,
                                   DBUS_TYPE_STRING, &proxy_list,
                                   DBUS_TYPE_STRING, &error,
                                   DBUS_TYPE_INVALID)) {
        std::cout << "Error getting args" << std::endl;
      } else {
        std::cout << "[" << source_url << "] [" << proxy_list << "] [" << error
                  << "]\n" << std::endl;
      }
      TestClient* self = static_cast<TestClient*>(object);
      self->ReceivedSignal();
      self->QuitLoopIfNecessary();
      return DBUS_HANDLER_RESULT_HANDLED;
    }
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  dbus::Proxy request_proxy_;
  GMainLoop *loop_;
  int num_signals_;  // Number of signals to wait for.
};

}  // namespace chromeos

int main(int argc, char* argv[]) {
  ::g_type_init();
  GMainLoop* loop = ::g_main_loop_new(NULL, true);

  CommandLine::Init(argc, argv);
  CommandLine *command_line = CommandLine::ForCurrentProcess();

  const char* kDefaultServiceName = "org.chromium.LibCrosService";
  const char* kDefaultObjectPath = "/org/chromium/LibCrosService";
  const char* kDefaultInterfaceName = "org.chromium.LibCrosServiceInterface";

  // Use the custom service name if specified by --service_name.
  std::string service_name = command_line->GetSwitchValueASCII("service_name");
  if (service_name.empty())
    service_name = kDefaultServiceName;

  // Use the custom object path if specified by --object_path.
  std::string object_path = command_line->GetSwitchValueASCII("object_path");
  if (object_path.empty())
    object_path = kDefaultObjectPath;

  // Use the custom interface name if specified by --interface_name.
  std::string interface_name =
      command_line->GetSwitchValueASCII("interface_name");
  if (interface_name.empty())
    interface_name = kDefaultInterfaceName;

  chromeos::dbus::Proxy request_proxy(chromeos::dbus::GetSystemBusConnection(),
                                      service_name.c_str(),
                                      object_path.c_str(),
                                      interface_name.c_str());
  if (!request_proxy) {
    std::cout << "Can't create proxy for LibCrosService" << std::endl;
    return -1;
  }

  chromeos::TestClient test(request_proxy, loop);
  if (!test.Initialize())
    return -1;

  std::string comma_separated_urls = command_line->GetSwitchValueASCII("urls");
  if (!comma_separated_urls.empty()) {
    // Resolve the custom URLs if specified by --urls.
    std::vector<std::string> urls;
    base::SplitString(comma_separated_urls, ',', &urls);
    for (size_t i = 0; i < urls.size(); ++i)
      test.ResolveNetworkProxy(urls[i].c_str());
  } else {
    // Otherwise, just resolve the preset URLs.
    test.ResolveNetworkProxy("http://maps.google.com");
    test.ResolveNetworkProxy("http://www.youtube.com");
    test.ResolveNetworkProxy("http://www.gmail.com");
    test.ResolveNetworkProxy("http://127.0.0.1");
  }

  // Run glib loop if there're signal(s) to wait for.
  test.RunLoopIfNecessary();

  return 0;
}
