// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>
#include <glib-object.h>
#include <map>
#include <vector>

#include <base/logging.h>

#include "chromeos_cros_api.h"  // NOLINT
#include "chromeos_network.h"  // NOLINT
#include "chromeos/dbus/dbus.h"  // NOLINT
#include "chromeos/glib/object.h"  // NOLINT
#include "monitor_utils.h" //NOLINT


void DumpServices(const chromeos::SystemInfo* info);
void DumpDataPlans(const char* service_path,
                   const chromeos::CellularDataPlanList* data_plan_list);
void PrintProperty(const char* path,
                   const char* key,
                   const chromeos::glib::Value* value);

// Callback is an example of how to use the network monitoring functionality.
class CallbackMonitorNetwork {
 public:
  // You can store whatever state is needed in the function object.
  explicit CallbackMonitorNetwork() :
    count_(0) {
  }

  // Note, you MUST copy the service status struct since it will be freed
  // the moment this function returns.
  //
  // DO NOT DO THIS
  // Struct my_status = status;
  //
  // DO THIS INSTEAD
  // Struct my_status = {};
  // my_status = MakeACopyOf(status);
  // ...
  static void Run(void* object,
                  const char* path,
                  const char* key,
                  const chromeos::glib::Value* value) {
    PrintProperty(path, key, value);
    if (strcmp(key, "Services") == 0) {
      chromeos::SystemInfo* info = chromeos::GetSystemInfo();
      DumpServices(info);
      chromeos::FreeSystemInfo(info);
    }
    CallbackMonitorNetwork* self = static_cast<CallbackMonitorNetwork*>(object);
    ++self->count_;
  }

 private:
  int count_;
};

// CallbackMonitorDataPlan is an example of how to use the cellular
// data plan monitoring functionality.
class CallbackMonitorDataPlan {
 public:
  explicit CallbackMonitorDataPlan() :
    count_(0) {
  }

  static void Run(void* object,
                  const char* path,
                  const chromeos::CellularDataPlanList* data) {
    DumpDataPlans(path, data);

    CallbackMonitorDataPlan* self =
        static_cast<CallbackMonitorDataPlan*>(object);
    ++self->count_;
  }

 private:
  int count_;
};

struct ServiceMonitor {
  ServiceMonitor() : monitor(NULL), callback(NULL), last_scangen(0) { }
  ~ServiceMonitor() { delete callback; }
  chromeos::PropertyChangeMonitor monitor;
  CallbackMonitorNetwork *callback;
  int last_scangen;
  static int scangen;
};

int ServiceMonitor::scangen;

typedef std::map<std::string, ServiceMonitor*> MonitorMap;
MonitorMap monitor_map;

extern "C"
void PrintCollectionElement(const GValue *value, gpointer user_data) {
  if (G_VALUE_HOLDS(value, DBUS_TYPE_G_OBJECT_PATH)) {
    const char* path = static_cast<const char*>(::g_value_get_boxed(value));
    LOG(INFO) << "  path: " << path;
  } else
    LOG(INFO) << "  <type " << G_VALUE_TYPE_NAME(value) << ">";
}

void PrintProperty(const char* path,
                   const char* key,
                   const chromeos::glib::Value* value) {
    std::string prelude("PropertyChanged [");
    prelude += path;
    prelude += "] ";
    prelude += key;
    prelude += " : ";
    if (G_VALUE_HOLDS_STRING(value))
      LOG(INFO) << prelude << "\"" << ::g_value_get_string(value) << "\"";
    else if (G_VALUE_HOLDS_BOOLEAN(value))
      LOG(INFO) << prelude << static_cast<bool>(::g_value_get_boolean(value));
    else if (G_VALUE_HOLDS_UCHAR(value))
      LOG(INFO) << prelude << static_cast<int>(::g_value_get_uchar(value));
    else if (G_VALUE_HOLDS_UINT(value))
      LOG(INFO) << prelude << ::g_value_get_uint(value);
    else if (G_VALUE_HOLDS_INT(value))
      LOG(INFO) << prelude << ::g_value_get_int(value);
    else if (G_VALUE_HOLDS(value, G_TYPE_STRV)) {
      std::string values;
      GStrv strv = static_cast<GStrv>(::g_value_get_boxed(value));
      while (*strv != NULL) {
        values += *strv;
        ++strv;
        if (*strv != NULL)
          values += ", ";
      }
      LOG(INFO) << prelude << "\"" << values << "\"";
    } else if (dbus_g_type_is_collection(G_VALUE_TYPE(value))) {
      LOG(INFO) << prelude;
      dbus_g_type_collection_value_iterate(value, PrintCollectionElement, NULL);
    } else
      LOG(INFO) << prelude << "<type " << G_VALUE_TYPE_NAME(value) << ">";
}

// Dumps the contents of a single service to the logs.
void DumpService(const chromeos::ServiceInfo& info) {
  const char* passphrase;
  if (info.passphrase != NULL && strlen(info.passphrase) != 0)
    passphrase = "******";
  else
    passphrase = "\"\"";

  LOG(INFO) << "  \"" << info.name << "\"";
  LOG(INFO) << "    Service=" << info.service_path;
  LOG(INFO) << "    Device=" << info.device_path;
  LOG(INFO) << "    Type=" << info.type <<
               ", Mode=" << info.mode <<
               ", Security=" << info.security <<
               ", State=" << info.state <<
               ", Technology=" << info.network_technology;
  LOG(INFO) << "    RoamingState=" << info.roaming_state <<
               ", Error=" << info.error <<
               ", PassphraseRequired=" << info.passphrase_required <<
               ", Passphrase=" << passphrase;
  LOG(INFO) << "    Strength=" << info.strength <<
               ", Favorite=" << info.favorite <<
               ", AutoConnect=" << info.auto_connect;
}

// Dumps the contents of ServiceStatus to the log.
void DumpServices(const chromeos::SystemInfo* info) {
  if (info == NULL)
    return;

  LOG(INFO) << "Network status:";
  ++ServiceMonitor::scangen;
  for (int i = 0; i < info->service_size; i++) {
    chromeos::ServiceInfo *sinfo = &info->services[i];
    DumpService(*sinfo);

    ServiceMonitor *servmon = monitor_map[sinfo->service_path];
    if (servmon == NULL) {
      LOG(INFO) << "New service " << sinfo->service_path;
      servmon = new ServiceMonitor;
      servmon->callback = new CallbackMonitorNetwork();
      monitor_map[sinfo->service_path] = servmon;
    }
    servmon->last_scangen = ServiceMonitor::scangen;
    // For any service that has just entered the ready (i..e, connected)
    // state, start monitoring it for property changes. For any service
    // that has just left the ready state, stop monitoring it. Also
    // start monitoring if there is just one service.
    if (sinfo->state == chromeos::STATE_READY ||
        sinfo->type == chromeos::TYPE_CELLULAR) {
      if (servmon->monitor == NULL) {
        LOG(INFO) << "Start monitoring service " << sinfo->service_path;
        servmon->monitor = chromeos::MonitorNetworkService(
            &CallbackMonitorNetwork::Run,
            sinfo->service_path,
            servmon->callback);
      }
    } else if (servmon->monitor != NULL) {
        LOG(INFO) << "Stop monitoring service " << sinfo->service_path;
        chromeos::DisconnectPropertyChangeMonitor(servmon->monitor);
        servmon->monitor = NULL;
    }
  }
  // Go through monitor_map and remove mappings for
  // services that are no longer in the service list.
  MonitorMap::iterator it;
  for (it = monitor_map.begin(); it != monitor_map.end(); ++it) {
    ServiceMonitor* servmon = it->second;
    if (servmon->last_scangen != ServiceMonitor::scangen)  {
      if (servmon->monitor != NULL) {
        LOG(INFO) << "Service " << it->first << " gone, stop monitoring";
        chromeos::DisconnectPropertyChangeMonitor(servmon->monitor);
      } else
        LOG(INFO) << "Service " << it->first << " no longer present";
      monitor_map.erase(it);
      delete servmon;
    }
  }
}

void DumpDataPlans(const char* modem_service_path,
                  const chromeos::CellularDataPlanList* data_plan_list) {
  LOG(INFO) << "Data Plans for: '" << modem_service_path;
  chromeos::CellularDataPlanList::const_iterator iter;
  for (iter = data_plan_list->begin(); iter != data_plan_list->end(); ++iter) {
    const chromeos::CellularDataPlan& data = *iter;
    LOG(INFO) << "Plan Name: " << data.plan_name
              << ", Type=" << data.plan_type
              << ", Update Time=" << data.update_time
              << ", Start Time=" << data.plan_start_time
              << ", End Time=" << data.plan_end_time
              << ", Data Bytes=" << data.plan_data_bytes
              << ", Bytes Used=" << data.data_bytes_used;
  }
}

// A simple example program demonstrating how to use the ChromeOS network API.
int main(int argc, const char** argv) {
  ::g_type_init();
  GMainLoop* loop = ::g_main_loop_new(NULL, false);

  DCHECK(loop) << "Failed to create main loop";
  if (!LoadCrosLibrary(argv))
    LOG(INFO) << "Failed to load cros .so";

  // Synchronous request of network info.

  chromeos::SystemInfo* network_info = chromeos::GetSystemInfo();
  DCHECK(network_info) << "Unable to get SystemInfo";

  LOG(INFO) << "Enabled network devices:";
  int technologies = network_info->enabled_technologies;
  if (technologies & (1 << chromeos::TYPE_ETHERNET))
    LOG(INFO) << "  ethernet";
  if (technologies & (1 << chromeos::TYPE_WIFI))
    LOG(INFO) << "  wifi";
  if (technologies & (1 << chromeos::TYPE_WIMAX))
    LOG(INFO) << "  wimax";
  if (technologies & (1 << chromeos::TYPE_BLUETOOTH))
    LOG(INFO) << "  bluetooth";
  if (technologies & (1 << chromeos::TYPE_CELLULAR))
    LOG(INFO) << "  cellular";

  DumpServices(network_info);

  // Synchronous request of data plans.

  LOG(INFO) << "Retrieving Cellular Data Plans:";
  for (int i = 0; i < network_info->service_size; ++i) {
    chromeos::ServiceInfo *sinfo = &network_info->services[i];
    if (sinfo->type != chromeos::TYPE_CELLULAR) {
      continue;
    }
    LOG(INFO) << "  Retrieving Data Plans for: " << sinfo->service_path;
    chromeos::CellularDataPlanList data_plan_list;
    bool res = chromeos::RetrieveCellularDataPlans(sinfo->service_path,
                                                   &data_plan_list);
    if (res) {
      DumpDataPlans(sinfo->service_path, &data_plan_list);
    } else {
      LOG(WARNING) << "  RetrieveCellularDataPlans failed for: "
          << sinfo->service_path;
    }
  }

  // Asynchronous network monitoring.

  LOG(INFO) << "Starting Monitor Network:";
  CallbackMonitorNetwork callback_network;
  chromeos::PropertyChangeMonitor connection_network =
      chromeos::MonitorNetworkManager(&CallbackMonitorNetwork::Run,
                                      &callback_network);

  // Asynchronous data plan monitoring.

  LOG(INFO) << "Starting Monitor Data Plan:";
  CallbackMonitorDataPlan callback_dataplan;
  chromeos::DataPlanUpdateMonitor connection_dataplan =
      chromeos::MonitorCellularDataPlan(&CallbackMonitorDataPlan::Run,
                                        &callback_dataplan);
  LOG(INFO) << "Requesting Cellular Data Plan Updates:";
  for (int i = 0; i < network_info->service_size; i++) {
    chromeos::ServiceInfo *sinfo = &network_info->services[i];
    if (sinfo->type == chromeos::TYPE_CELLULAR) {
      LOG(INFO) << "  Requesting Data Plan Update for: " << sinfo->service_path;
      chromeos::RequestCellularDataPlanUpdate(sinfo->service_path);
    }
  }

  LOG(INFO) << "Starting g_main_loop.";

  ::g_main_loop_run(loop);

  LOG(INFO) << "Shutting down.";

  chromeos::FreeSystemInfo(network_info);
  chromeos::DisconnectPropertyChangeMonitor(connection_network);
  chromeos::DisconnectDataPlanUpdateMonitor(connection_dataplan);

  return 0;
}
