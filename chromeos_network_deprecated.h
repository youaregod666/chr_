// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This code was originally used by Chrome but now only by entd.
// The code should be removed, or moved to entd.

#ifndef CHROMEOS_NETWORK_DEPRECATED_H_
#define CHROMEOS_NETWORK_DEPRECATED_H_

#include <base/basictypes.h>
#include <base/logging.h>

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

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_DEPRECATED_H_
