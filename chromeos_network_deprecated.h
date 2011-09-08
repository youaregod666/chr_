// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_DEPRECATED_H_
#define CHROMEOS_NETWORK_DEPRECATED_H_

#include <base/basictypes.h>
#include <base/logging.h>

class Value;

namespace chromeos { // NOLINT

typedef void (*NetworkPropertiesCallback)(void* object,
                                          const char* path,
                                          const Value* properties);

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

// Device Info for cellular devices.
struct DeviceInfo {
  const char* carrier;
  const char* MEID;
  const char* IMEI;
  const char* IMSI;
  const char* ESN;
  const char* MDN;
  const char* MIN;
  const char* model_id;
  const char* manufacturer;
  const char* firmware_revision;
  const char* hardware_revision;
  const char* last_update;
  int PRL_version;
  const char* path;
  const char* name;
  ConnectionType type;
  bool scanning;
};

// Carrier Info for cellular services.
struct CarrierInfo {
  const char* operator_name;
  const char* operator_code;
  const char* payment_url;
};

struct ServiceInfo {
  const char* service_path;
  const char* name;
  ConnectionType type;
  ConnectionMode mode;
  ConnectionSecurity security;
  ConnectionState state;
  ConnectionError error;
  bool passphrase_required;
  const char* passphrase;
  const char* identity;
  const char* cert_path; // DEPRECATED - use EAP fields below
  int64 strength;
  bool favorite;
  bool auto_connect;
  const char* device_path;
  const char* activation_state_dont_use;  // DEPRECATED - use the enum below
  ActivationState activation_state;
  NetworkTechnology network_technology;
  NetworkRoamingState roaming_state;
  bool restricted_pool;  // DEPRECATED - use connectivity_state
  CarrierInfo* carrier_info;  // NULL unless TYPE_CELLULAR
  DeviceInfo* device_info;  // may point to a member of SystemInfo.devices
  bool is_active;
  bool connectable;
  ConnectivityState connectivity_state;
  // EAP fields (plus identity from above)
  const char *eap;
  const char *inner_eap;
  const char *anonymous_identity;
  const char *client_cert;
  const char *cert_id;
  const char *private_key;
  const char *private_key_passwd;
  const char *key_id;
  const char *ca_cert;
  const char *ca_cert_id;
  const char *pin;
  const char *password;
};

struct SystemInfo {
  bool online; // if Manager.State == "online"
  int available_technologies; // bitwise OR of bit shifted by ConnectionType
  int enabled_technologies; // bitwise OR of bit shifted by ConnectionType
  int connected_technologies; // bitwise OR of bit shifted by ConnectionType
  ConnectionType default_technology;
  bool offline_mode;
  int service_size;
  ServiceInfo *services; // Do not access this directly, use GetServiceInfo().
  int remembered_service_size;
  ServiceInfo *remembered_services; // Use GetRememberedServiceInfo().
  int service_info_size; // Size of the ServiceInfo stuct.
  int device_size;
  DeviceInfo* devices;
  int device_info_size;  // Size of the DeviceInfo struct.
  // Client needs to call this method to get each ServiceInfo object.
  ServiceInfo* GetServiceInfo(int index) {
    size_t ptr = reinterpret_cast<size_t>(services);
    return reinterpret_cast<ServiceInfo*>(ptr + index * service_info_size);
  }
  ServiceInfo* GetRememberedServiceInfo(int index) {
    size_t ptr = reinterpret_cast<size_t>(remembered_services);
    return reinterpret_cast<ServiceInfo*>(ptr + index * service_info_size);
  }
  DeviceInfo* GetDeviceInfo(int index) {
    size_t ptr = reinterpret_cast<size_t>(devices);
    return reinterpret_cast<DeviceInfo*>(ptr + index * device_info_size);
  }
};

// Requests a scan of services of |type|.
// If |type| is TYPE_UNKNOWN (0), it will scan for all types.
extern void (*RequestScan)(ConnectionType type);

// Gets a ServiceInfo for a wifi service with |ssid| and |security|.
// If an open network is not found, then it will create a hidden network and
// return the ServiceInfo for that.
// The ServiceInfo instance that is returned by this function MUST be
// deleted with by calling FreeServiceInfo.
//
// Returns NULL on error.
extern ServiceInfo* (*GetWifiService)(const char* ssid,
                                      ConnectionSecurity security);

// Set up the configuration for a wifi service with |ssid| and the
// provided security parameters. If the ssid is currently known and
// visible, the configuration will be applied to the existing service;
// otherwise, the configuration will be saved for use when the network
// is found.
//
// Returns false on failure and true on success.
extern bool (*ConfigureWifiService)(const char* ssid,
                                    ConnectionSecurity security,
                                    const char* passphrase,
                                    const char* identity,
                                    const char* certpath);

// Returns the system info, which includes the state of the system and a list of
// all of the available services that a user can connect to.
// The SystemInfo instance that is returned by this function MUST be
// deleted by calling FreeSystemInfo.
//
// Returns NULL on error.
extern SystemInfo* (*GetSystemInfo)();

// Deletes a SystemInfo type that was allocated in the ChromeOS dll. We need
// to do this to safely pass data over the dll boundary between our .so and
// Chrome.
extern void (*FreeSystemInfo)(SystemInfo* system);

// Deletes a ServiceInfo type that was allocated in the ChromeOS dll. We need
// to do this to safely pass data over the dll boundary between our .so and
// Chrome.
extern void (*FreeServiceInfo)(ServiceInfo* info);

// An internal listener to a d-bus signal. When notifications are received
// they are rebroadcasted in non-glib form.
// MonitorNetworkConnection is deprecated: use PropertyChangeMonitor.
class ManagerPropertyChangedHandler;
typedef ManagerPropertyChangedHandler* MonitorNetworkConnection;

// The expected callback signature that will be provided by the client who
// calls MonitorNetwork. Callbacks are only called with |object| being
// the caller. The recipient of the callback should call GetSystemInfo to
// retrieve the current state of things.
// MonitorNetworkCallback is deprecated: Use MonitorPropertyCallback.
typedef void(*MonitorNetworkCallback)(void* object);

// Sets up monitoring of the PropertyChanged signal on the flimflam manager.
// The provided MonitorNetworkCallback will be called whenever that happens.
// MonitorNetwork is deprecated: use MonitorNetworkManager.
extern MonitorNetworkConnection (*MonitorNetwork)(
    MonitorNetworkCallback callback,
    void* object);

// Disconnects a MonitorNetworkConnection.
// DisconnectMonitorNetwork is deprecated: use DisconnectPropertyChangeMonitor.
extern void (*DisconnectMonitorNetwork)(MonitorNetworkConnection connection);

// Returns false on failure and true on success.
extern bool (*EnableNetworkDevice)(ConnectionType type, bool enable);

// Request a wifi service not in the network list (i.e. hidden).
// Get a service path for a wifi service not in the network list (i.e. hidden).
extern void (*RequestWifiServicePath)(const char* ssid,
                                      ConnectionSecurity security,
                                      NetworkPropertiesCallback callback,
                                      void* object);

// Save the IP config data
extern bool (*SaveIPConfig)(IPConfig* config);

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_DEPRECATED_H_
