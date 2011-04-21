// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>
#include <glib-object.h>
#include <map>
#include <vector>

#include <base/logging.h>
#include <base/time.h>
#include <base/values.h>

#include "chromeos_cros_api.h"  // NOLINT
#include "chromeos_network.h"  // NOLINT
#include "chromeos/dbus/dbus.h"  // NOLINT
#include "chromeos/glib/object.h"  // NOLINT
#include "monitor_utils.h" //NOLINT

static void PrintProperty(const char* path,
                          const char* key,
                          const Value* value) {
    std::string prelude("PropertyChanged [");
    prelude += path;
    prelude += "] ";
    prelude += key;
    prelude += " : ";
    if (value->IsType(Value::TYPE_STRING)) {
      std::string strval;
      value->GetAsString(&strval);
      LOG(INFO) << prelude << "\"" << strval << "\"";
    } else if (value->IsType(Value::TYPE_BOOLEAN)) {
      bool boolval;
      value->GetAsBoolean(&boolval);
      LOG(INFO) << prelude << boolval;
    }  else if (value->IsType(Value::TYPE_INTEGER)) {
      int intval;
      value->GetAsInteger(&intval);
      LOG(INFO) << prelude << intval;
    } else if (value->IsType(Value::TYPE_LIST)) {
      const ListValue* list = static_cast<const ListValue*>(value);
      Value *itemval;
      std::string liststr;
      size_t index = 0;
      while (list->Get(index, &itemval)) {
        if (!itemval->IsType(Value::TYPE_STRING)) {
          ++index;
          continue;
        }
        std::string itemstr;
        itemval->GetAsString(&itemstr);
        liststr += itemstr;
        ++index;
        if (index < list->GetSize())
          liststr += ", ";
      }
      LOG(INFO) << prelude << "\"" << liststr << "\"";
    } else if (value->IsType(Value::TYPE_DICTIONARY)) {
      const DictionaryValue* dict = static_cast<const DictionaryValue*>(value);
      std::string items;
      std::string itemval;
      size_t n = 0;
      DictionaryValue::key_iterator iter = dict->begin_keys();
      while (iter != dict->end_keys()) {
        std::string key = *iter;
        items += "{'" + key + "': '";
        if (dict->GetStringWithoutPathExpansion(key, &itemval))
          items += itemval + "'}";
        else
          items += "<not-a-string>'}";
        if (n < dict->size())
          items += ", ";
        ++iter;
        ++n;
      }
      LOG(INFO) << prelude << items;
    } else
      LOG(INFO) << prelude << "<type " << value->GetType() << ">";
}

class CallbackMonitorNetwork {
 public:
  static void Run(void *object,
                  const char *path,
                  const char *key,
                  const Value* value) {
    PrintProperty(path, key, value);
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

  chromeos::PropertyChangeMonitor device_mon;
  if (strncmp(path, "/service/", 9) == 0) {
    LOG(INFO) << "Requesting properties messages on service '" << path << "'";
    device_mon = chromeos::MonitorNetworkService(&CallbackMonitorNetwork::Run,
                                                 path, NULL);
    if (clear) {
      LOG(INFO) << "Clearing property '" << property << "' on '" << path << "'";
      chromeos::ClearNetworkServiceProperty(path, property);
    } else {
      LOG(INFO) << "Setting property '" << property << "' on '" << path << "'";
      chromeos::SetNetworkServiceProperty(path, property,
                                          Value::CreateStringValue(value));
    }
  } else if (strncmp(path, "/device/", 8) == 0) {
    LOG(INFO) << "Requesting properties messages on device '" << path << "'";
    device_mon = chromeos::MonitorNetworkDevice(&CallbackMonitorNetwork::Run,
                                                path, NULL);
    if (clear) {
      LOG(INFO) << "Clearing property '" << property << "' on '" << path << "'";
      chromeos::ClearNetworkDeviceProperty(path, property);
    } else {
      LOG(INFO) << "Setting property '" << property << "' on '" << path << "'";
      chromeos::SetNetworkDeviceProperty(path, property,
                                         Value::CreateStringValue(value));
    }
  } else {
    LOG(INFO) << "Don't know what to do with path '" << path << "' "
              << "neither a device nor a service";
        return 1;
  }

  LOG(INFO) << "Starting g_main_loop.";

  ::g_main_loop_run(loop);

  LOG(INFO) << "Shutting down.";

  chromeos::DisconnectPropertyChangeMonitor(device_mon);
  return 0;
}
