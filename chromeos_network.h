// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_H_
#define CHROMEOS_NETWORK_H_

#include <base/basictypes.h>
#include <base/logging.h>
#include <base/time.h>

#include <glib-object.h>

namespace chromeos { // NOLINT

// ipconfig types (see flimflam/files/doc/ipconfig-api.txt)
enum IPConfigType {
  IPCONFIG_TYPE_UNKNOWN,
  IPCONFIG_TYPE_IPV4,
  IPCONFIG_TYPE_IPV6,
  IPCONFIG_TYPE_DHCP,
  IPCONFIG_TYPE_BOOTP, //Not Used
  IPCONFIG_TYPE_ZEROCONF,
  IPCONFIG_TYPE_DHCP6,
  IPCONFIG_TYPE_PPP,
};

enum CellularDataPlanType {
  CELLULAR_DATA_PLAN_UNKNOWN = 0,
  CELLULAR_DATA_PLAN_UNLIMITED = 1,
  CELLULAR_DATA_PLAN_METERED_PAID = 2,
  CELLULAR_DATA_PLAN_METERED_BASE = 3
};

struct CellularDataPlanInfo {
  const char *plan_name;
  CellularDataPlanType plan_type;
  int64 update_time;
  int64 plan_start_time;
  int64 plan_end_time;
  int64 plan_data_bytes;
  int64 data_bytes_used;
};


struct CellularDataPlanList {
  struct CellularDataPlanInfo *plans;
  size_t plans_size;     // number of plans

  size_t data_plan_size; // size of a data plan structure

  // Client needs to call this method to get each CellularDataPlan object.
  CellularDataPlanInfo* GetCellularDataPlan(size_t index) {
    DCHECK(index < plans_size);
    size_t ptr = reinterpret_cast<size_t>(plans);
    return
        reinterpret_cast<CellularDataPlanInfo*>(ptr + index * data_plan_size);
  }
  const CellularDataPlanInfo* GetCellularDataPlan(size_t index) const {
    DCHECK(index < plans_size);
    size_t ptr = reinterpret_cast<size_t>(plans);
    return reinterpret_cast<const CellularDataPlanInfo*>(
        ptr + index * data_plan_size);
  }

};

struct SMS {
  base::Time timestamp;
  const char *number;
  const char *text;
  const char *smsc;  // optional; NULL if not present in message
  int32 validity;  // optional; -1 if not present in message
  int32 msgclass;  // optional; -1 if not present in message
};

struct IPConfig {
  const char* path;
  IPConfigType type;
  const char* address;
  int32 mtu;
  const char* netmask;
  const char* broadcast;
  const char* peer_address;
  const char* gateway;
  const char* domainname;
  const char* name_servers;
};

struct IPConfigStatus {
  IPConfig* ips;
  int size;
  // This is usually a MAC address. If no hardware address is present,
  // this field will be an empty string.
  const char *hardware_address;
};

// Represents a single network object (i.e. access point) from the list
// networks DBus API.
struct DeviceNetworkInfo {
  const char* device_path;   // Path to the device that owns this network
  const char* network_path;  // Path to this network object itself
  const char* address;       // Mac address, format 11:22:33:44:55:66
  const char* name;          // SSID for wifi
  int strength;              // Signal strengh
  int channel;               // Wifi channel number
  bool connected;            // True if this network is connected
  int age_seconds;           // Time in seconds since this network was seen
};

struct DeviceNetworkList {
  size_t network_size;
  DeviceNetworkInfo* networks;
};

// Activates the cellular modem specified by |service_path| with carrier
// specified by |carrier|.
// |carrier| is NULL or an empty string, this will activate with the currently
// active carrier.
//
// Returns false on failure and true on success.
extern bool (*ActivateCellularModem)(const char* service_path,
                                     const char* carrier);


// Set a property of a service to the provided value
//
// Success is indicated by the receipt of a matching PropertyChanged signal.
extern void (*SetNetworkServicePropertyGValue)(const char* service_path,
                                               const char* property,
                                               const GValue* gvalue);

// Clear a property of a service
extern void (*ClearNetworkServiceProperty)(const char* service_path,
                                           const char* property);

// Set a property of a device to the provided value
//
// Success is indicated by the receipt of a matching PropertyChanged signal.
extern void (*SetNetworkDevicePropertyGValue)(const char* device_path,
                                              const char* property,
                                              const GValue* gvalue);

// Set a property of an ip config to the provided value
//
// Success is indicated by the receipt of a matching PropertyChanged signal.
extern void (*SetNetworkIPConfigPropertyGValue)(const char* ipconfig_path,
                                                const char* property,
                                                const GValue* gvalue);

// Delete a remembered service from a profile.
extern void (*DeleteServiceFromProfile)(const char* profile_path,
                                        const char* service_path);

// Request an update of the data plans. A callback will be received by any
// object that invoked MonitorCellularDataPlan when up to date data is ready.
extern void (*RequestCellularDataPlanUpdate)(const char* modem_service_path);

// An internal listener to a d-bus signal. When notifications are received
// they are rebroadcasted in non-glib form.
class PropertyChangedGValueMonitor;
typedef PropertyChangedGValueMonitor* NetworkPropertiesMonitor;

// Callback for MonitorNetwork*Properties.
typedef void (*MonitorPropertyGValueCallback)(void* object,
                                              const char* path,
                                              const char* key,
                                              const GValue* value);

// Sets up monitoring of the PropertyChanged signal on the flimflam manager.
// The provided |callback| will be called whenever a manager property changes.
// |object| will be supplied as the first argument to the callback.
extern NetworkPropertiesMonitor (*MonitorNetworkManagerProperties)(
    MonitorPropertyGValueCallback callback,
    void* object);

// Similar to MonitorNetworkManagerProperties for a specified network service.
extern NetworkPropertiesMonitor (*MonitorNetworkServiceProperties)(
    MonitorPropertyGValueCallback callback,
    const char* service_path,
    void* object);

// Similar to MonitorNetworkManagerProperties for a specified network device.
extern NetworkPropertiesMonitor (*MonitorNetworkDeviceProperties)(
    MonitorPropertyGValueCallback callback,
    const char* device_path,
    void* object);

// Disconnects a PropertyChangeMonitor.
extern void (*DisconnectNetworkPropertiesMonitor)(
    NetworkPropertiesMonitor monitor);

// An internal listener to a d-bus signal for plan data.
class DataPlanUpdateHandler;
typedef DataPlanUpdateHandler* DataPlanUpdateMonitor;

// Callback for MonitorCellularDataPlan.
// NOTE: callback function must copy 'dataplan' if it wants it to persist.
typedef void (*MonitorDataPlanCallback)(void* object,
                                        const char* modem_service_path,
                                        const CellularDataPlanList* dataplan);

// Sets up monitoring of the cellular data plan updates from Cashew.
extern DataPlanUpdateMonitor (*MonitorCellularDataPlan)(
    MonitorDataPlanCallback callback,
    void* object);

// Disconnects a DataPlanUpdateMonitor.
extern void (*DisconnectDataPlanUpdateMonitor)(
    DataPlanUpdateMonitor monitor);


class SMSHandler;
typedef SMSHandler* SMSMonitor;

typedef void (*MonitorSMSCallback)(void* object,
                                   const char* modem_device_path,
                                   const SMS* message);

extern SMSMonitor (*MonitorSMS)(const char* modem_device_path,
                                MonitorSMSCallback callback,
                                void* object);

extern void (*DisconnectSMSMonitor)(SMSMonitor monitor);

//////////////////////////////////////////////////////////////////////////////
// Asynchronous flimflam interfaces.

// Callback for asynchronous getters.
typedef void (*NetworkPropertiesGValueCallback)(
    void* object,
    const char* path,
    GHashTable* properties);

// Describes whether there is an error and whether the error came from
// the local system or from the server implementing the connect
// method.
enum NetworkMethodErrorType {
  NETWORK_METHOD_ERROR_NONE = 0,
  NETWORK_METHOD_ERROR_LOCAL = 1,
  NETWORK_METHOD_ERROR_REMOTE = 2,
};

// Callback for methods that initiate an action and return no data.
typedef void (*NetworkActionCallback)(void* object,
                                      const char* path,
                                      NetworkMethodErrorType error,
                                      const char* error_message);

// Connect to the service with the |service_path|.
//
// Service parameters such as authentication must already be configured.
//
// Note, a successful invocation of the callback only indicates that
// the connection process has started. You will have to query the
// connection state to determine if the connection was established
// successfully.
extern void (*RequestNetworkServiceConnect)(const char* service_path,
                                            NetworkActionCallback callback,
                                            void* object);

extern void (*RequestNetworkManagerProperties)(
    NetworkPropertiesGValueCallback callback,
    void* object);

extern void (*RequestNetworkServiceProperties)(
    const char* service_path,
    NetworkPropertiesGValueCallback callback,
    void* object);

// Retrieve the latest info for a particular device.
extern void (*RequestNetworkDeviceProperties)(
    const char* device_path,
    NetworkPropertiesGValueCallback callback,
    void* object);

// Retrieve the list of remembered services for a profile.
extern void (*RequestNetworkProfileProperties)(
    const char* profile_path,
    NetworkPropertiesGValueCallback callback,
    void* object);

// Retrieve the latest info for a profile service entry.
extern void (*RequestNetworkProfileEntryProperties)(
    const char* profile_path,
    const char* profile_entry_path,
    NetworkPropertiesGValueCallback callback,
    void* object);

// Request a wifi service not in the network list (i.e. hidden).
extern void (*RequestHiddenWifiNetworkProperties)(
    const char* ssid,
    const char* security,
    NetworkPropertiesGValueCallback callback,
    void* object);

// Request a new VPN service.
extern void (*RequestVirtualNetworkProperties)(
    const char* service_name,
    const char* server_hostname,
    const char* provider_type,
    NetworkPropertiesGValueCallback callback,
    void* object);

// Asynchronous disconnect from network service.
extern void (*RequestNetworkServiceDisconnect)(const char* service_path);

// Remove an exisiting network service (e.g. after forgetting a VPN).
extern void (*RequestRemoveNetworkService)(const char* service_path);

// Requests a scan of services of |type|.
// |type| should be is a string recognized by flimflam's Manager API.
extern void (*RequestNetworkScan)(const char* network_type);

// Request enabling or disabling a device.
extern void (*RequestNetworkDeviceEnable)(const char* network_type,
                                          bool enable);

// Enable or disable PIN protection for a SIM card.
extern void (*RequestRequirePin)(const char* device_path,
                                 const char* pin,
                                 bool enable,
                                 NetworkActionCallback callback,
                                 void* object);

// Enter a PIN to unlock a SIM card.
extern void (*RequestEnterPin)(const char* device_path,
                               const char* pin,
                               NetworkActionCallback callback,
                               void* object);

// Enter a PUK to unlock a SIM card whose PIN has been entered
// incorrectly too many times. A new |pin| must be supplied
// along with the |unblock_code| (PUK).
extern void (*RequestUnblockPin)(const char* device_path,
                                 const char* unblock_code,
                                 const char* pin,
                                 NetworkActionCallback callback,
                                 void* object);

// Change the PIN used to unlock a SIM card.
extern void (*RequestChangePin)(const char* device_path,
                                const char* old_pin,
                                const char* new_pin,
                                NetworkActionCallback callback,
                                void* object);

// Proposes to trigger a scan transaction. For cellular networks scan result
// is set in the property Cellular.FoundNetworks.
extern void (*ProposeScan)(const char* device_path);

// Initiate registration on the network specified by network_id, which is in the
// form MCCMNC. If the network ID is the empty string, then switch back to
// automatic registration mode before initiating registration.
extern void (*RequestCellularRegister)(const char* device_path,
                                       const char* network_id,
                                       NetworkActionCallback callback,
                                       void* object);

//////////////////////////////////////////////////////////////////////////////
// Enable or disable the specific network device for connection.
//
// Set offline mode. This will turn off all radios.
//
// Returns false on failure and true on success.
extern bool (*SetOfflineMode)(bool offline);

// Gets a list of all the IPConfigs using a given device path
extern IPConfigStatus* (*ListIPConfigs)(const char* device_path);

// Add a IPConfig of the given type to the device
extern bool (*AddIPConfig)(const char* device_path, IPConfigType type);

// Remove an existing IP Config
extern bool (*RemoveIPConfig)(IPConfig* config);

// Free out a full status
extern void (*FreeIPConfigStatus)(IPConfigStatus* status);

// Retrieves the list of visible network objects. This structure is not cached
// within the DLL, and is fetched via d-bus on each call.
// Ownership of the DeviceNetworkList is returned to the caller; use
// FreeDeviceNetworkList to release it.
extern DeviceNetworkList* (*GetDeviceNetworkList)();

// Deletes a DeviceNetworkList type that was allocated in the ChromeOS dll. We
// need to do this to safely pass data over the dll boundary between our .so
// and Chrome.
extern void (*FreeDeviceNetworkList)(DeviceNetworkList* network_list);

// Configures the network service specified by |properties|.
// |identifier| can be the service path, guid, or any other identifier
// specified by the calling code; it is ignored by libcros and flimflam,
// except to pass it back in |callback| as |path|.
extern void (*ConfigureService)(const char* identifier,
                                const GHashTable* properties,
                                NetworkActionCallback callback,
                                void* object);

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_H_
