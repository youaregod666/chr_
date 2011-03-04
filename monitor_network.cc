// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>
#include <glib-object.h>
#include <map>
#include <vector>

#include <base/logging.h>
#include <base/values.h>

#include "chromeos_cros_api.h"  // NOLINT
#include "chromeos_network.h"  // NOLINT
// TODO(stevenjb): Remove testing of deprecated code; add new tests.
#include "chromeos_network_deprecahted.h"  // NOLINT
#include "chromeos/dbus/dbus.h"  // NOLINT
#include "chromeos/glib/object.h"  // NOLINT
#include "monitor_utils.h" //NOLINT


void DumpServices(const chromeos::SystemInfo* info);
void DumpDataPlans(const char* service_path,
                   const chromeos::CellularDataPlanList* data_plan_list);
void PrintProperty(const char* path,
                   const char* key,
                   const Value* value);

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
                  const Value* value) {
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

void PrintProperty(const char* path,
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
    } else
      LOG(INFO) << prelude << "<type " << value->GetType() << ">";
}

void DumpDeviceInfo(const chromeos::DeviceInfo& device,
                    chromeos::ConnectionType type) {
  LOG(INFO) << "      Name:" << device.name
            << ", Type:" << device.type
            << ", Scanning: " << device.scanning;
  if (type == chromeos::TYPE_CELLULAR) {
    LOG(INFO) << "      Carrier:" << device.carrier;
    LOG(INFO) << "      MEID=" << device.MEID
              << ", IMEI=" << device.IMEI
              << ", IMSI=" << device.IMSI
              << ", ESN=" << device.ESN
              << ", MDN=" << device.MDN
              << ", MIN=" << device.MIN;
    LOG(INFO) << "      ModelID=" << device.model_id
              << ", Manufacturer=" << device.manufacturer;
    LOG(INFO) << "      Firmware=" << device.firmware_revision
              << ", Hardware=" << device.hardware_revision;
    LOG(INFO) << "      Last Update=" << device.last_update
              << ", PRL Version=" << device.PRL_version;
  }
}

void DumpCarrierInfo(const chromeos::CarrierInfo& carrier) {
  LOG(INFO) << "      Operator:" << carrier.operator_name
            << ", Code=" << carrier.operator_code;
  LOG(INFO) << "      Payment URL:" << carrier.payment_url;
}

void DumpService(const chromeos::ServiceInfo& info) {
  const char* passphrase;
  if (info.passphrase != NULL && strlen(info.passphrase) != 0)
    passphrase = "******";
  else
    passphrase = "\"\"";

  LOG(INFO) << "  \"" << info.name << "\"";
  LOG(INFO) << "    Service=" << info.service_path
            << ", Name=" << info.name;
  LOG(INFO) << "    Type=" << info.type
            << ", Active=" << info.is_active;
  LOG(INFO) << "    Mode=" << info.mode
            << ", Security=" << info.security
            << ", State=" << info.state
            << ", Error=" << info.error;
  LOG(INFO) << "    PassphraseRequired=" << info.passphrase_required
            << ", Passphrase=" << passphrase;
  LOG(INFO) << "    Identity=" << info.identity
            << ", CertPath=" << info.cert_path;
  LOG(INFO) << "    Strength=" << info.strength
            << ", Favorite=" << info.favorite
            << ", AutoConnect=" << info.auto_connect;
  if (info.device_path)
    LOG(INFO) << "    Device=" << info.device_path;
  if (info.device_info)
    DumpDeviceInfo(*info.device_info, info.type);
  if (info.type == chromeos::TYPE_CELLULAR)
    LOG(INFO) << "    Activation State=" << info.activation_state
            << ", Technology=" << info.network_technology
            << ", RoamingState=" << info.roaming_state
            << ", ConnectivityState=" << info.connectivity_state
            << ", (RestrictedPool=" << info.restricted_pool << ")";
  if (info.carrier_info)
    DumpCarrierInfo(*info.carrier_info);
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
  LOG(INFO) << "Removing services.";
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
  for (unsigned int i = 0; i < data_plan_list->plans_size; i++) {
    const chromeos::CellularDataPlanInfo* data =
        data_plan_list->GetCellularDataPlan(i);
    LOG(INFO) << "Plan Name: " << data->plan_name
              << ", Type=" << data->plan_type
              << ", Update Time=" << data->update_time
              << ", Start Time=" << data->plan_start_time
              << ", End Time=" << data->plan_end_time
              << ", Data Bytes=" << data->plan_data_bytes
              << ", Bytes Used=" << data->data_bytes_used;
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

  LOG(INFO) << "Calling chromeos::GetSystemInfo()";
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
    chromeos::CellularDataPlanList* data_plan_list;
    data_plan_list = chromeos::RetrieveCellularDataPlans(sinfo->service_path);

    if (data_plan_list) {
      DumpDataPlans(sinfo->service_path, data_plan_list);
      chromeos::FreeCellularDataPlanList(data_plan_list);
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
