// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// An interactive program for testing the SIM lock/unlock API

#include <dlfcn.h>
#include <glib-object.h>
#include <map>
#include <vector>

#include <base/logging.h>
#include <base/string_util.h>
#include <base/values.h>

#include "chromeos_cros_api.h"  // NOLINT
#include "chromeos_network.h"  // NOLINT
#include "chromeos/dbus/dbus.h"  // NOLINT
#include "chromeos/glib/object.h"  // NOLINT
#include "monitor_utils.h" //NOLINT

GMainLoop* loop;

typedef std::vector<std::string> stringlist;

void set_up_for_next_command();

class DeviceHandler {
 public:
  DeviceHandler()
    : state_(kNeedManagerProps),
      device_index_(0),
      unlock_retries_left_(0),
      monitor_(NULL) {}

  static void GetManagerPropertiesCallback(void* object, const char* path,
                                           const Value* properties);
  static void GetDevicePropertiesCallback(void* object, const char* path,
                                          const Value* properties);
  enum State {kNeedManagerProps, kGettingDeviceProps};

  static bool RequirePin(DeviceHandler* device, stringlist& args);
  static bool EnterPin(DeviceHandler* device, stringlist& args);
  static bool UnblockPin(DeviceHandler* device, stringlist& args);
  static bool ChangePin(DeviceHandler* device, stringlist& args);

  static void PinMethodCallback(void* object, const char* path,
                                chromeos::NetworkMethodErrorType error,
                                const char* error_message);
  int unlock_retries_left() { return unlock_retries_left_; }
  std::string& lock_type() { return lock_type_; }

 private:
  void GetNextDeviceInfo();
  void FoundCellular(const DictionaryValue* dict);
  void ExtractSimLockStatus(const DictionaryValue *dict);
  static void HandlePropertyChange(void* object,
                                   const char* path,
                                   const char* key,
                                   const Value* value);

  enum State state_;
  stringlist devices_;
  stringlist::size_type device_index_;
  std::string cellular_;
  int unlock_retries_left_;
  std::string lock_type_;
  chromeos::PropertyChangeMonitor monitor_;
};

void DeviceHandler::GetManagerPropertiesCallback(void* object,
                                                 const char* path,
                                                 const Value* properties) {
  DeviceHandler* device = static_cast<DeviceHandler*>(object);
  DCHECK_EQ(properties->GetType(), Value::TYPE_DICTIONARY);
  const DictionaryValue* dict =
      static_cast<const DictionaryValue*>(properties);

  ListValue* list;
  if (!dict->GetListWithoutPathExpansion("EnabledTechnologies", &list)) {
    LOG(WARNING) << "Cannot determine enabled technologies";
    return;
  }
  Value* itemval;
  std::string liststr;
  size_t index = 0;
  bool found = false;
  while (list->Get(index, &itemval)) {
    if (!itemval->IsType(Value::TYPE_STRING)) {
      ++index;
      continue;
    }
    std::string tech;
    itemval->GetAsString(&tech);
    if (tech == "cellular")
      found = true;
    ++index;
  }
  if (!found) {
    LOG(WARNING) << "Cellular technology is not enabled";
    return;
  }
  if (!dict->GetListWithoutPathExpansion("Devices", &list)) {
    LOG(WARNING) << "No devices";
    return;
  }
  index = 0;
  while (list->Get(index, &itemval)) {
    if (!itemval->IsType(Value::TYPE_STRING)) {
      ++index;
      continue;
    }
    std::string device_path;
    itemval->GetAsString(&device_path);
    device->devices_.push_back(device_path);
    ++index;
  }
  device->state_ = kGettingDeviceProps;
  device->GetNextDeviceInfo();
}

void DeviceHandler::GetNextDeviceInfo() {
  if (device_index_ >= devices_.size()) {
    set_up_for_next_command();
    return;
  }
  chromeos::RequestNetworkDeviceInfo(devices_.at(device_index_).c_str(),
                                     GetDevicePropertiesCallback, this);
}

void DeviceHandler::ExtractSimLockStatus(const DictionaryValue *dict) {
  if (!dict->GetIntegerWithoutPathExpansion("RetriesLeft",
                                             &unlock_retries_left_))
    LOG(WARNING) << "No RetriesLeft property found";
  if (!dict->GetStringWithoutPathExpansion("LockType", &lock_type_))
    LOG(WARNING) << "No LockType property found";
  LOG(INFO) << "LockType: " << lock_type_
            << " RetriesLeft: " << unlock_retries_left_;
}

void DeviceHandler::FoundCellular(const DictionaryValue* dict) {
  cellular_ = devices_.at(device_index_);
  DictionaryValue* ldict;
  if (!dict->GetDictionaryWithoutPathExpansion("Cellular.SIMLockStatus",
                                               &ldict)) {
    LOG(WARNING) << "No Cellular.SIMLockStatus property found";
    return;
  }
  ExtractSimLockStatus(ldict);
  monitor_ = chromeos::MonitorNetworkDevice(
      HandlePropertyChange, cellular_.c_str(), this);
}

void DeviceHandler::GetDevicePropertiesCallback(void* object,
                                                const char* path,
                                                const Value* properties) {
  DeviceHandler* device = static_cast<DeviceHandler*>(object);
  DCHECK_EQ(properties->GetType(), Value::TYPE_DICTIONARY);
  const DictionaryValue* dict =
      static_cast<const DictionaryValue*>(properties);

  std::string devtype;
  if (!dict->GetStringWithoutPathExpansion("Type", &devtype)) {
    LOG(WARNING) << "Device Type property is missing";
    return;
  }
  if (devtype == "cellular")
    device->FoundCellular(dict);
  ++device->device_index_;
  device->GetNextDeviceInfo();
}

void DeviceHandler::HandlePropertyChange(void* object,
                                         const char* path,
                                         const char* key,
                                         const Value* value) {
  DeviceHandler* device = static_cast<DeviceHandler*>(object);
  if (strcmp(key, "Cellular.SIMLockStatus") != 0)
    return;
  const DictionaryValue* dict =
      static_cast<const DictionaryValue*>(value);
  device->ExtractSimLockStatus(dict);
}

gboolean request_network_info(void* data) {
  DeviceHandler* device = static_cast<DeviceHandler*>(data);
  // start an asynchronous request for network manager properties
  chromeos::RequestNetworkManagerInfo(
      DeviceHandler::GetManagerPropertiesCallback, device);
  return FALSE;
}

bool DeviceHandler::RequirePin(DeviceHandler* device, stringlist& args) {
  const char* pin = args[1].c_str();
  bool require;

  if (args[2] == "true")
    require = true;
  else if (args[2] == "false")
    require = false;
  else {
    printf("Usage: RequirePin <PIN> true|false\n");
    return false;
  }
  chromeos::RequestRequirePin(device->cellular_.c_str(), pin, require,
                              PinMethodCallback, device);
  return true;
}

bool DeviceHandler::EnterPin(DeviceHandler* device, stringlist& args) {
  chromeos::RequestEnterPin(device->cellular_.c_str(), args[1].c_str(),
                            PinMethodCallback, device);
  return true;
}
bool DeviceHandler::UnblockPin(DeviceHandler* device, stringlist& args) {
  chromeos::RequestUnblockPin(device->cellular_.c_str(),
                              args[1].c_str(), args[2].c_str(),
                              PinMethodCallback, device);
  return true;
}

bool DeviceHandler::ChangePin(DeviceHandler* device, stringlist& args) {
  chromeos::RequestChangePin(device->cellular_.c_str(),
                             args[1].c_str(), args[2].c_str(),
                             PinMethodCallback, device);
  return true;
}

void DeviceHandler::PinMethodCallback(void* object, const char* path,
                                      chromeos::NetworkMethodErrorType error,
                                      const char* error_message) {
//  DeviceHandler* device = static_cast<DeviceHandler*>(object);
  if (error == chromeos::NETWORK_METHOD_ERROR_NONE)
    printf("PIN operation succeeded\n");
  else
    printf("PIN operation failure: %s\n", error_message);
  set_up_for_next_command();
}

static bool DoQuit(DeviceHandler* device, stringlist& args) {
    ::g_main_loop_quit(loop);
    return true;
}

static bool ShowStatus(DeviceHandler* device, stringlist& args) {
  printf("LockType: %s RetriesLeft: %d\n",
         device->lock_type().c_str(),
         device->unlock_retries_left());
  return false;
}

typedef bool (*cmdfunc)(DeviceHandler* device, stringlist& args);

struct command {
  const char* name;
  size_t numargs;
  cmdfunc function;
  const char* usage;
} commands[] = {
  {"RequirePin", 2, DeviceHandler::RequirePin, "<PIN> true|false"},
  {"EnterPin",   1, DeviceHandler::EnterPin,   "<PIN>"},
  {"UnblockPin", 2, DeviceHandler::UnblockPin, "<PUK> <new PIN>"},
  {"ChangePin",  2, DeviceHandler::ChangePin, "<old PIN> <new PIN>"},
  {"status",     0, ShowStatus, ""},
  {"quit",       0, DoQuit, ""},
  {NULL},
};

gboolean prompt_for_input(gpointer data) {
  printf("Enter command: "); fflush(stdout);
  return FALSE;
}

void set_up_for_next_command() {
  ::g_idle_add(prompt_for_input, NULL);
}

gboolean read_and_process_command(GIOChannel* iochan,
                                  GIOCondition cond,
                                  gpointer data) {
  DeviceHandler* device = static_cast<DeviceHandler*>(data);
  gchar* line;
  gsize length;
  GError* gerror = NULL;

  ::g_io_channel_read_line(iochan, &line, &length, NULL, &gerror);
  if (length)
    line[length-1] = '\0';
  std::string argstring(line);
  ::g_free(line);
  stringlist args;
  SplitStringAlongWhitespace(argstring, &args);

  if (args.size() != 0) {
    struct command* cp = commands;
    while (cp->name != NULL) {
      if (args[0] == cp->name)
        break;
      ++cp;
    }
    if (cp->name == NULL) {
      printf("Unknown command \"%s\"\n", args[0].c_str());
    } else if (cp->numargs != args.size() - 1) {
      printf("\"%s\" command requires %d arguments\n",
              cp->name, cp->numargs);
      printf("Usage: %s %s\n", cp->name, cp->usage);
    } else {
      return cp->function(device, args);
    }
  }

  return FALSE;
}

gboolean docommand(GIOChannel* iochan, GIOCondition cond, gpointer data) {
  if (!read_and_process_command(iochan, cond, data))
    set_up_for_next_command();
  return TRUE;
}

int stdin_watch_id;

int main(int argc, const char** argv) {
  ::g_type_init();
  loop = ::g_main_loop_new(NULL, false);

  DCHECK(loop) << "Failed to create main loop";
  if (!LoadCrosLibrary(argv))
    LOG(ERROR) << "Failed to load cros .so";

  DeviceHandler* device = new DeviceHandler();

  // set up to poll stdin
  GIOChannel* iochan = ::g_io_channel_unix_new(0);
  stdin_watch_id = ::g_io_add_watch(iochan, G_IO_IN, docommand, device);
  ::g_io_channel_unref(iochan);
  ::g_idle_add(request_network_info, device);
  ::g_main_loop_run(loop);

  return 0;
}
