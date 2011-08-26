// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>
#include <glib-object.h>
#include <map>
#include <string>
#include <vector>

#include <base/logging.h>
#include <base/time.h>

#include "chromeos_cros_api.h"  // NOLINT
#include "chromeos_network.h"  // NOLINT
#include "chromeos/dbus/dbus.h"  // NOLINT
#include "chromeos/glib/object.h"  // NOLINT
#include "monitor_utils.h" //NOLINT

namespace {

void PrintValue(const std::string& prelude, const GValue* gvalue);

void PrintListElement(const GValue *gvalue, gpointer user_data) {
  std::string prelude("  ");
  PrintValue(prelude, gvalue);
}

void PrintMapElement(const GValue *keyvalue,
                     const GValue *gvalue,
                     gpointer user_data) {
  std::string prelude("  ");
  const char* key = g_value_get_string(gvalue);
  if (key)
    prelude += key;
  prelude += " : ";
  PrintValue(prelude, gvalue);
}

void PrintValue(const std::string& prelude, const GValue* gvalue) {
  if (G_VALUE_HOLDS_STRING(gvalue)) {
    std::string strval(g_value_get_string(gvalue));
    LOG(INFO) << prelude << "\"" << strval << "\"";
  } else if (G_VALUE_HOLDS_BOOLEAN(gvalue)) {
    bool boolval(static_cast<bool>(g_value_get_boolean(gvalue)));
    LOG(INFO) << prelude << boolval;
  } else if (G_VALUE_HOLDS_INT(gvalue)) {
    int intval(g_value_get_int(gvalue));
    LOG(INFO) << prelude << intval;
  } else if (dbus_g_type_is_collection(G_VALUE_TYPE(gvalue))) {
    LOG(INFO) << prelude << " : List [";
    dbus_g_type_collection_value_iterate(gvalue, PrintListElement, NULL);
    LOG(INFO) << "]";
  } else if (::dbus_g_type_is_map(G_VALUE_TYPE(gvalue))) {
    LOG(INFO) << prelude << " : Map [";
    ::dbus_g_type_map_value_iterate(gvalue, PrintMapElement, NULL);
    LOG(INFO) << "]";
  } else {
    LOG(INFO) << prelude << "<type "
              << g_type_name(G_VALUE_TYPE(gvalue)) << ">";
  }
}

void PrintProperty(const char* path,
                   const char* key,
                   const GValue* gvalue) {
    std::string prelude("PropertyChanged [");
    prelude += path;
    prelude += "] ";
    prelude += key;
    prelude += " : ";
    PrintValue(prelude, gvalue);
}

GValue* ConvertToGValue(const char* c) {
  GValue* gvalue = new GValue();
  g_value_init(gvalue, G_TYPE_STRING);
  g_value_set_string(gvalue, c);
  return gvalue;
}

}  // namespace

class CallbackMonitorNetwork {
 public:
  static void Run(void *object,
                  const char *path,
                  const char *key,
                  const GValue* gvalue) {
    PrintProperty(path, key, gvalue);
  }
};

// A simple program exercising the {Set,Clear}{Device,Service}Property methods.
int main(int argc, const char** argv) {
  ::g_type_init();
  GMainLoop* loop = ::g_main_loop_new(NULL, false);

  DCHECK(loop) << "Failed to create main loop";
  if (!LoadCrosLibrary(argv))
    LOG(INFO) << "Failed to load cros .so";

  if (argc != 4) {
    LOG(INFO) << "Usage: " << argv[0] << " <path> <property> <string-value>";
    LOG(INFO) << "       " << argv[0] << " -c <path> <property>";
    return 1;
  }

  bool clear;
  const char *path, *property, *value;
  if (strcmp(argv[1], "-c") == 0) {
    clear = true;
    path = argv[2];
    property = argv[3];
    value = NULL;
  } else {
    clear = false;
    path = argv[1];
    property = argv[2];
    value = argv[3];
  }

  chromeos::NetworkPropertiesMonitor device_mon;
  if (strncmp(path, "/service/", 9) == 0) {
    LOG(INFO) << "Requesting properties messages on service '" << path << "'";
    device_mon = chromeos::MonitorNetworkServiceProperties(
        &CallbackMonitorNetwork::Run, path, NULL);
    if (clear) {
      LOG(INFO) << "Clearing property '" << property << "' on '" << path << "'";
      chromeos::ClearNetworkServiceProperty(path, property);
    } else {
      LOG(INFO) << "Setting property '" << property << "' on '" << path << "'";
      scoped_ptr<GValue> gvalue(ConvertToGValue(value));
      chromeos::SetNetworkServicePropertyGValue(path, property, gvalue.get());
    }
  } else if (strncmp(path, "/device/", 8) == 0) {
    LOG(INFO) << "Requesting properties messages on device '" << path << "'";
    device_mon = chromeos::MonitorNetworkDeviceProperties(
        &CallbackMonitorNetwork::Run, path, NULL);
    if (clear) {
      LOG(INFO) << "Clearing property '" << property << "' on '" << path << "'";
      chromeos::ClearNetworkDeviceProperty(path, property);
    } else {
      LOG(INFO) << "Setting property '" << property << "' on '" << path << "'";
      scoped_ptr<GValue> gvalue(ConvertToGValue(value));
      chromeos::SetNetworkDevicePropertyGValue(path, property, gvalue.get());
    }
  } else {
    LOG(INFO) << "Don't know what to do with path '" << path << "' "
              << "neither a device nor a service";
        return 1;
  }

  LOG(INFO) << "Starting g_main_loop.";

  ::g_main_loop_run(loop);

  LOG(INFO) << "Shutting down.";

  chromeos::DisconnectNetworkPropertiesMonitor(device_mon);
  return 0;
}
