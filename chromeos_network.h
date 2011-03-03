// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_H_
#define CHROMEOS_NETWORK_H_

#include <base/basictypes.h>
#include <base/logging.h>

class Value;

namespace chromeos { // NOLINT


// Connection enums (see flimflam/include/service.h)
enum ConnectionType {
  TYPE_UNKNOWN   = 0,
  TYPE_ETHERNET  = 1,
  TYPE_WIFI      = 2,
  TYPE_WIMAX     = 3,
  TYPE_BLUETOOTH = 4,
  TYPE_CELLULAR  = 5,
};

enum ConnectionMode {
  MODE_UNKNOWN = 0,
  MODE_MANAGED = 1,
  MODE_ADHOC   = 2,
};

enum ConnectionSecurity {
  SECURITY_UNKNOWN = 0,
  SECURITY_NONE    = 1,
  SECURITY_WEP     = 2,
  SECURITY_WPA     = 3,
  SECURITY_RSN     = 4,
  SECURITY_8021X   = 5,
};

enum ConnectionState {
  STATE_UNKNOWN            = 0,
  STATE_IDLE               = 1,
  STATE_CARRIER            = 2,
  STATE_ASSOCIATION        = 3,
  STATE_CONFIGURATION      = 4,
  STATE_READY              = 5,
  STATE_DISCONNECT         = 6,
  STATE_FAILURE            = 7,
  STATE_ACTIVATION_FAILURE = 8
};

enum ConnectivityState {
  CONN_STATE_UNKNOWN      = 0,
  CONN_STATE_UNRESTRICTED = 1,
  CONN_STATE_RESTRICTED   = 2,
  CONN_STATE_NONE         = 3
};

// Network enums (see flimflam/include/network.h)
enum NetworkTechnology {
  NETWORK_TECHNOLOGY_UNKNOWN      = 0,
  NETWORK_TECHNOLOGY_1XRTT        = 1,
  NETWORK_TECHNOLOGY_EVDO         = 2,
  NETWORK_TECHNOLOGY_GPRS         = 3,
  NETWORK_TECHNOLOGY_EDGE         = 4,
  NETWORK_TECHNOLOGY_UMTS         = 5,
  NETWORK_TECHNOLOGY_HSPA         = 6,
  NETWORK_TECHNOLOGY_HSPA_PLUS    = 7,
  NETWORK_TECHNOLOGY_LTE          = 8,
  NETWORK_TECHNOLOGY_LTE_ADVANCED = 9,
};

enum ActivationState {
  ACTIVATION_STATE_UNKNOWN             = 0,
  ACTIVATION_STATE_ACTIVATED           = 1,
  ACTIVATION_STATE_ACTIVATING          = 2,
  ACTIVATION_STATE_NOT_ACTIVATED       = 3,
  ACTIVATION_STATE_PARTIALLY_ACTIVATED = 4,
};

enum NetworkRoamingState {
  ROAMING_STATE_UNKNOWN = 0,
  ROAMING_STATE_HOME    = 1,
  ROAMING_STATE_ROAMING = 2,
};

// connection errors (see flimflam/include/service.h)
enum ConnectionError {
  ERROR_UNKNOWN           = 0,
  ERROR_OUT_OF_RANGE      = 1,
  ERROR_PIN_MISSING       = 2,
  ERROR_DHCP_FAILED       = 3,
  ERROR_CONNECT_FAILED    = 4,
  ERROR_BAD_PASSPHRASE    = 5,
  ERROR_BAD_WEPKEY        = 6,
  ERROR_ACTIVATION_FAILED = 7,
  ERROR_NEED_EVDO         = 8,
  ERROR_NEED_HOME_NETWORK = 9,
  ERROR_OTASP_FAILED      = 10,
  ERROR_AAA_FAILED        = 11,
};

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

// Requests a scan of services of |type|.
// If |type| is TYPE_UNKNOWN (0), it will scan for all types.
extern void (*RequestScan)(ConnectionType type);

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
extern void (*SetNetworkServiceProperty)(const char* service_path,
                                         const char* property,
                                         const ::Value* setting);

// Connects to the network with the |service_path|.
//
// Set |passphrase| to NULL if the network doesn't require authentication.
// Set |identity| and |certpath| to NULL if the network doesn't require
// certificate-based authentication.
//
// returns false on failure and true on success.
//
// Note, a successful call to this function only indicates that the
// connection process has started. You will have to query the connection state
// to determine if the connection was established successfully.
extern bool (*ConnectToNetworkWithCertInfo)(const char* service_path,
                                            const char* passphrase,
                                            const char* identity,
                                            const char* certpath);

// Connects to the network with the |service_path|.
//
// Set |passphrase| to NULL if the network doesn't require authentication.
// returns false on failure and true on success.
//
// Note, a successful call to this function only indicates that the
// connection process has started. You will have to query the connection state
// to determine if the connection was established successfully.
extern bool (*ConnectToNetwork)(const char* service_path,
                                const char* passphrase);

// Disconnects from the network with the |service_path|.
extern bool (*DisconnectFromNetwork)(const char* service_path);

// This will delete this service from the list of remembered service.
//
// Returns false on failure and true on success.
extern bool (*DeleteRememberedService)(const char* service_path);

// Get the list of data plans for the cellular network corresponding to path.
// The CellularDataPlansList instance that is returned by this function MUST be
// deleted by calling FreeCellularDataPlanList
//
// Returns NULL on error or no data available.
extern CellularDataPlanList* (*RetrieveCellularDataPlans)(
    const char* modem_service_path);

// Request an update of the data plans. A callback will be received by any
// object that invoked MonitorCellularDataPlan when up to date data is ready.
extern void (*RequestCellularDataPlanUpdate)(const char* modem_service_path);

// Deletes a CellularDataPlanList that was allocated in the ChromeOS
// dll. We need to do this to safely pass data over the dll boundary
// between our .so and Chrome.
extern void (*FreeCellularDataPlanList)(CellularDataPlanList* list);

// An internal listener to a d-bus signal. When notifications are received
// they are rebroadcasted in non-glib form.
class PropertyChangedHandler;
typedef PropertyChangedHandler* PropertyChangeMonitor;

// Callback for MonitorNetworkManager.
typedef void (*MonitorPropertyCallback)(void* object,
                                        const char* path,
                                        const char* key,
                                        const Value* value);

// Sets up monitoring of the PropertyChanged signal on the flimflam manager.
// The provided MonitorPropertyCallback will be called whenever a manager
// property changes. |object| will be supplied as the first argument to the
// callback.
extern PropertyChangeMonitor (*MonitorNetworkManager)(
    MonitorPropertyCallback callback,
    void* object);

// Disconnects a PropertyChangeMonitor.
extern void (*DisconnectPropertyChangeMonitor)(
    PropertyChangeMonitor monitor);

// Sets up monitoring of the PropertyChanged signal on the specified service.
// The provided MonitorPropertyCallback will be called whenever a service
// property changes. |object| will be supplied as the first argument to the
// callback.
extern PropertyChangeMonitor (*MonitorNetworkService)(
    MonitorPropertyCallback callback,
    const char* service_path,
    void* object);


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


//////////////////////////////////////////////////////////////////////////////
// Asynchronous flimflam interfaces.

// Callback for asynchronous getters.
typedef void (*NetworkPropertiesCallback)(void* object,
                                          const char* path,
                                          const Value* properties);

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

extern void (*RequestNetworkManagerInfo)(NetworkPropertiesCallback callback,
                                         void* object);

// Retrieve the latest info for a particular service.
extern void (*RequestNetworkServiceInfo)(const char* service_path,
                                         NetworkPropertiesCallback callback,
                                         void* object);

// Retrieve the latest info for a particular device.
extern void (*RequestNetworkDeviceInfo)(const char* device_path,
                                        NetworkPropertiesCallback callback,
                                        void* object);

// Retrieve the list of remembered services for a profile.
extern void (*RequestNetworkProfile)(const char* profile_path,
                                     NetworkPropertiesCallback callback,
                                     void* object);

// Retrieve the latest info for a profile service entry.
extern void (*RequestNetworkProfileEntry)(const char* profile_path,
                                          const char* profile_entry_path,
                                          NetworkPropertiesCallback callback,
                                          void* object);

// Get a service path for a wifi service not in the network list (i.e. hidden).
extern void (*RequestWifiServicePath)(const char* ssid,
                                      ConnectionSecurity security,
                                      NetworkPropertiesCallback callback,
                                      void* object);

//////////////////////////////////////////////////////////////////////////////
// Enable or disable the specific network device for connection.
//
// Returns false on failure and true on success.
extern bool (*EnableNetworkDevice)(ConnectionType type, bool enable);

// Set offline mode. This will turn off all radios.
//
// Returns false on failure and true on success.
extern bool (*SetOfflineMode)(bool offline);

// Set auto_connect for service.
//
// Returns true on success.
extern bool (*SetAutoConnect)(const char* service_path, bool auto_connect);

// Set passphrase for service.
//
// Returns true on success.
extern bool (*SetPassphrase)(const char* service_path, const char* passphrase);

// Set security identity for service.
//
// Returns true on success.
extern bool (*SetIdentity)(const char* service_path, const char* identity);

// Set certificate id/path for service.
//
// Returns true on success.
extern bool (*SetCertPath)(const char* service_path, const char* cert_path);

// Gets a list of all the IPConfigs using a given device path
extern IPConfigStatus* (*ListIPConfigs)(const char* device_path);

// Add a IPConfig of the given type to the device
extern bool (*AddIPConfig)(const char* device_path, IPConfigType type);

// Save the IP config data
extern bool (*SaveIPConfig)(IPConfig* config);

// Remove an existing IP Config
extern bool (*RemoveIPConfig)(IPConfig* config);

// Free a found IPConfig
extern void (*FreeIPConfig)(IPConfig* config);

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

}  // namespace chromeos

// TODO(stevenjb): Remove this after R11 -> stable to force Chrome to use the
// new interfaces or explicitly include chromeos_network_deprecated.h.
#include "chromeos_network_deprecated.h"

#endif  // CHROMEOS_NETWORK_H_
