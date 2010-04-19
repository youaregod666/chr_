// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_H_
#define CHROMEOS_NETWORK_H_

#include <base/basictypes.h>

namespace chromeos { // NOLINT

// Connection enums (see flimflam/files/include/service.h)
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
  STATE_UNKNOWN       = 0,
  STATE_IDLE          = 1,
  STATE_CARRIER       = 2,
  STATE_ASSOCIATION   = 3,
  STATE_CONFIGURATION = 4,
  STATE_READY         = 5,
  STATE_DISCONNECT    = 6,
  STATE_FAILURE       = 7,
};

enum ConnectionError {
  ERROR_UNKNOWN        = 0,
  ERROR_OUT_OF_RANGE   = 1,
  ERROR_PIN_MISSING    = 2,
  ERROR_DHCP_FAILED    = 3,
  ERROR_CONNECT_FAILED = 4,
};

// ipconfig types (see flimflam/files/doc/ipconfig-api.txt
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
  int64 strength;
  bool favorite;
  bool auto_connect;
  const char* device_path;
};

struct SystemInfo {
  bool online; // if Manager.State == "online"
  int available_technologies; // bitwise OR of bit shifted by ConnectionType
  int enabled_technologies; // bitwise OR of bit shifted by ConnectionType
  int connected_technologies; // bitwise OR of bit shifted by ConnectionType
  ConnectionType default_technology;
  bool offline_mode;
  int service_size;
  ServiceInfo *services;
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
};

// Returns the system info, which includes the state of the system and a list of
// all of the available services that a user can connect to.
// The SystemInfo instance that is returned by this function MUST be
// deleted by calling FreeSystemInfo.
//
// Returns NULL on error.
extern SystemInfo* (*GetSystemInfo)();

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
class ManagerPropertyChangedHandler;
typedef ManagerPropertyChangedHandler* MonitorNetworkConnection;

// The expected callback signature that will be provided by the client who
// calls MonitorNetworkStatus. Callbacks are only called with |object| being
// the caller. The recipient of the callback should call GetSystemInfo to
// retrieve the current state of things.
typedef void(*MonitorNetworkCallback)(void* object);

// Sets up monitoring of the PropertyChanged signal on the Connman manager.
// The provided MonitorNetworkCallback will be called whenever that happens.
extern MonitorNetworkConnection (*MonitorNetwork)(MonitorNetworkCallback,
                                                  void*);

// Disconnects a MonitorNetworkConnection.
extern void (*DisconnectMonitorNetwork)(MonitorNetworkConnection connection);

// Enable or disable the specific network device for connection.
//
// Returns false on failure and true on success.
extern bool (*EnableNetworkDevice)(ConnectionType type, bool enable);

// Set offline mode. This will turn off all radios.
//
// Returns false on failure and true on success.
extern bool (*SetOfflineMode)(bool offline);

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

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_H_
