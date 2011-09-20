// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_network.h"  // NOLINT
#include "chromeos_network_deprecated.h"  // NOLINT

#include <algorithm>
#include <cstring>
#include <list>
#include <set>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/string_util.h"
#include "base/time.h"
#include "chromeos/dbus/dbus.h"  // NOLINT
#include "chromeos/dbus/service_constants.h"  // NOLINT
#include "chromeos/glib/object.h"  // NOLINT
#include "chromeos/string.h"

class Value;

namespace chromeos {  // NOLINT

namespace {  // NOLINT

// Connman function names.
static const char* kConfigureWifiServiceFunction = "ConfigureWifiService";

// Connman property names.
static const char* kLastDeviceUpdateProperty = "Cellular.LastDeviceUpdate";
static const char* kCertpathSettingsPrefix = "SETTINGS:";

// Connman EAP service properties
static const char* kEAPIdentityProperty = "EAP.Identity";
static const char* kEAPInnerEAPProperty = "EAP.InnerEAP";
static const char* kEAPAnonymousIdentityProperty = "EAP.AnonymousIdentity";
static const char* kEAPPrivateKeyProperty = "EAP.PrivateKey";
static const char* kEAPPrivateKeyPasswordProperty = "EAP.PrivateKeyPassword";
static const char* kEAPCACertProperty = "EAP.CACert";
static const char* kEAPCACertIDProperty = "EAP.CACertID";
static const char* kEAPPasswordProperty = "EAP.Password";

// Connman connectivity state options.
static const char* kConnStateUnrestricted = "unrestricted";
static const char* kConnStateRestricted = "restricted";
static const char* kConnStateNone = "none";

}  // namespace

namespace {  // NOLINT

static ConnectionType ParseType(const std::string& type) {
  if (type == flimflam::kTypeEthernet)
    return TYPE_ETHERNET;
  if (type == flimflam::kTypeWifi)
    return TYPE_WIFI;
  if (type == flimflam::kTypeWimax)
    return TYPE_WIMAX;
  if (type == flimflam::kTypeBluetooth)
    return TYPE_BLUETOOTH;
  if (type == flimflam::kTypeCellular)
    return TYPE_CELLULAR;
  return TYPE_UNKNOWN;
}

static ConnectionMode ParseMode(const std::string& mode) {
  if (mode == flimflam::kModeManaged)
    return MODE_MANAGED;
  if (mode == flimflam::kModeAdhoc)
    return MODE_ADHOC;
  return MODE_UNKNOWN;
}

static ConnectionSecurity ParseSecurity(const std::string& security) {
  if (security == flimflam::kSecurity8021x)
    return SECURITY_8021X;
  if (security == flimflam::kSecurityRsn)
    return SECURITY_RSN;
  if (security == flimflam::kSecurityWpa)
    return SECURITY_WPA;
  if (security == flimflam::kSecurityWep)
    return SECURITY_WEP;
  if (security == flimflam::kSecurityNone)
    return SECURITY_NONE;
  return SECURITY_UNKNOWN;
}

static const char* SecurityToString(ConnectionSecurity security) {
  switch (security) {
    case SECURITY_UNKNOWN:
      break;
    case SECURITY_8021X:
      return flimflam::kSecurity8021x;
    case SECURITY_RSN:
      return flimflam::kSecurityRsn;
    case SECURITY_WPA:
      return flimflam::kSecurityWpa;
    case SECURITY_WEP:
      return flimflam::kSecurityWep;
    case SECURITY_NONE:
      return flimflam::kSecurityNone;
  }
  return flimflam::kUnknownString;
}

static ConnectionState ParseState(const std::string& state) {
  if (state == flimflam::kStateIdle)
    return STATE_IDLE;
  if (state == flimflam::kStateCarrier)
    return STATE_CARRIER;
  if (state == flimflam::kStateAssociation)
    return STATE_ASSOCIATION;
  if (state == flimflam::kStateConfiguration)
    return STATE_CONFIGURATION;
  if (state == flimflam::kStateReady)
    return STATE_READY;
  if (state == flimflam::kStateDisconnect)
    return STATE_DISCONNECT;
  if (state == flimflam::kStateFailure)
    return STATE_FAILURE;
  if (state == flimflam::kStateActivationFailure)
    return STATE_ACTIVATION_FAILURE;
  return STATE_UNKNOWN;
}

static ConnectivityState ParseConnectivityState(const std::string& state) {
  if (state == kConnStateUnrestricted)
    return CONN_STATE_UNRESTRICTED;
  if (state == kConnStateRestricted)
    return CONN_STATE_RESTRICTED;
  if (state == kConnStateNone)
    return CONN_STATE_NONE;
  return CONN_STATE_UNKNOWN;
}

static NetworkTechnology ParseNetworkTechnology(
    const std::string& technology) {
  if (technology == flimflam::kNetworkTechnology1Xrtt)
    return NETWORK_TECHNOLOGY_1XRTT;
  if (technology == flimflam::kNetworkTechnologyEvdo)
    return NETWORK_TECHNOLOGY_EVDO;
  if (technology == flimflam::kNetworkTechnologyGprs)
    return NETWORK_TECHNOLOGY_GPRS;
  if (technology == flimflam::kNetworkTechnologyEdge)
    return NETWORK_TECHNOLOGY_EDGE;
  if (technology == flimflam::kNetworkTechnologyUmts)
    return NETWORK_TECHNOLOGY_UMTS;
  if (technology == flimflam::kNetworkTechnologyHspa)
    return NETWORK_TECHNOLOGY_HSPA;
  if (technology == flimflam::kNetworkTechnologyHspaPlus)
    return NETWORK_TECHNOLOGY_HSPA_PLUS;
  if (technology == flimflam::kNetworkTechnologyLte)
    return NETWORK_TECHNOLOGY_LTE;
  if (technology == flimflam::kNetworkTechnologyLteAdvanced)
    return NETWORK_TECHNOLOGY_LTE_ADVANCED;
  return NETWORK_TECHNOLOGY_UNKNOWN;
}

static NetworkRoamingState ParseRoamingState(
    const std::string& roaming_state) {
  if (roaming_state == flimflam::kRoamingStateHome)
    return ROAMING_STATE_HOME;
  if (roaming_state == flimflam::kRoamingStateRoaming)
    return ROAMING_STATE_ROAMING;
  if (roaming_state == flimflam::kRoamingStateUnknown)
    return ROAMING_STATE_UNKNOWN;
  return ROAMING_STATE_UNKNOWN;
}

static ActivationState ParseActivationState(
    const std::string& activation_state) {
  if (activation_state == flimflam::kActivationStateActivated)
    return ACTIVATION_STATE_ACTIVATED;
  if (activation_state == flimflam::kActivationStateActivating)
    return ACTIVATION_STATE_ACTIVATING;
  if (activation_state == flimflam::kActivationStateNotActivated)
    return ACTIVATION_STATE_NOT_ACTIVATED;
  if (activation_state == flimflam::kActivationStateUnknown)
    return ACTIVATION_STATE_UNKNOWN;
  if (activation_state == flimflam::kActivationStatePartiallyActivated)
    return ACTIVATION_STATE_PARTIALLY_ACTIVATED;
  return ACTIVATION_STATE_UNKNOWN;
}

static ConnectionError ParseError(const std::string& error) {
  if (error == flimflam::kErrorOutOfRange)
    return ERROR_OUT_OF_RANGE;
  if (error == flimflam::kErrorPinMissing)
    return ERROR_PIN_MISSING;
  if (error == flimflam::kErrorDhcpFailed)
    return ERROR_DHCP_FAILED;
  if (error == flimflam::kErrorConnectFailed)
    return ERROR_CONNECT_FAILED;
  if (error == flimflam::kErrorBadPassphrase)
    return ERROR_BAD_PASSPHRASE;
  if (error == flimflam::kErrorBadWEPKey)
    return ERROR_BAD_WEPKEY;
  if (error == flimflam::kErrorActivationFailed)
    return ERROR_ACTIVATION_FAILED;
  if (error == flimflam::kErrorNeedEvdo)
    return ERROR_NEED_EVDO;
  if (error == flimflam::kErrorNeedHomeNetwork)
    return ERROR_NEED_HOME_NETWORK;
  if (error == flimflam::kErrorOtaspFailed)
    return ERROR_OTASP_FAILED;
  if (error == flimflam::kErrorAaaFailed)
    return ERROR_AAA_FAILED;
  return ERROR_UNKNOWN;
}

void ParseDeviceProperties(const glib::ScopedHashTable& properties,
                           DeviceInfo* info) {
  // Name
  const char* default_string = flimflam::kUnknownString;
  properties.Retrieve(flimflam::kNameProperty, &default_string);
  info->name = NewStringCopy(default_string);
  // Type
  default_string = flimflam::kUnknownString;
  properties.Retrieve(flimflam::kTypeProperty, &default_string);
  info->type = ParseType(default_string);
  // Scanning
  bool default_bool = false;
  properties.Retrieve(flimflam::kScanningProperty, &default_bool);
  info->scanning = default_bool;
}

void ParseCellularDeviceProperties(const glib::ScopedHashTable& properties,
                                   DeviceInfo* info) {
  // Carrier
  const char* default_string = flimflam::kUnknownString;
  properties.Retrieve(flimflam::kCarrierProperty, &default_string);
  info->carrier = NewStringCopy(default_string);
  // MEID
  default_string = flimflam::kUnknownString;
  properties.Retrieve(flimflam::kMeidProperty, &default_string);
  info->MEID = NewStringCopy(default_string);
  // IMEI
  default_string = flimflam::kUnknownString;
  properties.Retrieve(flimflam::kImeiProperty, &default_string);
  info->IMEI = NewStringCopy(default_string);
  // IMSI
  default_string = flimflam::kUnknownString;
  properties.Retrieve(flimflam::kImsiProperty, &default_string);
  info->IMSI = NewStringCopy(default_string);
  // ESN
  default_string = flimflam::kUnknownString;
  properties.Retrieve(flimflam::kEsnProperty, &default_string);
  info->ESN = NewStringCopy(default_string);
  // MDN
  default_string = flimflam::kUnknownString;
  properties.Retrieve(flimflam::kMdnProperty, &default_string);
  info->MDN = NewStringCopy(default_string);
  // MIN
  default_string = flimflam::kUnknownString;
  properties.Retrieve(flimflam::kMinProperty, &default_string);
  info->MIN = NewStringCopy(default_string);
  // ModelID
  default_string = flimflam::kUnknownString;
  properties.Retrieve(flimflam::kModelIDProperty, &default_string);
  info->model_id = NewStringCopy(default_string);
  // Manufacturer
  default_string = flimflam::kUnknownString;
  properties.Retrieve(flimflam::kManufacturerProperty, &default_string);
  info->manufacturer = NewStringCopy(default_string);
  // FirmwareRevision
  default_string = flimflam::kUnknownString;
  properties.Retrieve(flimflam::kFirmwareRevisionProperty, &default_string);
  info->firmware_revision = NewStringCopy(default_string);
  // HardwareRevision
  default_string = flimflam::kUnknownString;
  properties.Retrieve(flimflam::kHardwareRevisionProperty, &default_string);
  info->hardware_revision = NewStringCopy(default_string);
  // LastDeviceUpdate
  default_string = flimflam::kUnknownString;
  properties.Retrieve(kLastDeviceUpdateProperty, &default_string);
  info->last_update = NewStringCopy(default_string);
  // PRLVersion
  unsigned int default_uint = 0;
  properties.Retrieve(flimflam::kPRLVersionProperty, &default_uint);
  info->PRL_version = default_uint;
}

// Invokes the given method on the proxy and stores the result
// in the ScopedHashTable. The hash table will map strings to glib values.
bool GetProperties(const dbus::Proxy& proxy, glib::ScopedHashTable* result) {
  glib::ScopedError error;
  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           flimflam::kGetPropertiesFunction,
                           &Resetter(&error).lvalue(),
                           G_TYPE_INVALID,
                           ::dbus_g_type_get_map("GHashTable", G_TYPE_STRING,
                                                 G_TYPE_VALUE),
                           &Resetter(result).lvalue(),
                           G_TYPE_INVALID)) {
    LOG(WARNING) << "GetProperties on path '" << proxy.path() << "' failed: "
                 << (error->message ? error->message : "Unknown Error.");
    return false;
  }
  return true;
}

// Invokes the given method on the proxy and stores the result
// in the ScopedHashTable. The hash table will map strings to glib values.
bool GetEntry(const dbus::Proxy& proxy, const char* entry,
              glib::ScopedHashTable* result) {
  glib::ScopedError error;
  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           flimflam::kGetEntryFunction,
                           &Resetter(&error).lvalue(),
                           G_TYPE_STRING,
                           entry,
                           G_TYPE_INVALID,
                           ::dbus_g_type_get_map("GHashTable", G_TYPE_STRING,
                                                 G_TYPE_VALUE),
                           &Resetter(result).lvalue(),
                           G_TYPE_INVALID)) {
    LOG(WARNING) << "GetEntry failed: "
                 << (error->message ? error->message : "Unknown Error.");
    return false;
  }
  return true;
}

// Returns a DeviceInfo object populated with data from a
// given DBus object path.
//
// returns true on success.
bool GetDeviceInfo(const char* device_path, ConnectionType type,
                   DeviceInfo *info) {
  dbus::Proxy device_proxy(dbus::GetSystemBusConnection(),
                           flimflam::kFlimflamServiceName,
                           device_path,
                           flimflam::kFlimflamDeviceInterface);

  glib::ScopedHashTable device_properties;
  if (!GetProperties(device_proxy, &device_properties)) {
    LOG(WARNING) << "Couldn't read device's properties";
    return false;
  }
  info->path = NewStringCopy(device_path);
  ParseDeviceProperties(device_properties, info);
  if (type == TYPE_CELLULAR) {
    ParseCellularDeviceProperties(device_properties, info);
  } else {
    info->carrier = NULL;
    info->MEID = NULL;
    info->IMEI = NULL;
    info->IMSI = NULL;
    info->ESN = NULL;
    info->MDN = NULL;
    info->MIN = NULL;
    info->model_id = NULL;
    info->manufacturer = NULL;
    info->firmware_revision = NULL;
    info->hardware_revision = NULL;
    info->last_update = NULL;
    info->PRL_version = 0;
  }
  return true;
}

// Populates an instance of a ServiceInfo with the properties
// from a Connman service.
void ParseServiceProperties(const glib::ScopedHashTable& properties,
                            ServiceInfo* info) {
  // Name
  const char* default_string = flimflam::kUnknownString;
  properties.Retrieve(flimflam::kNameProperty, &default_string);
  info->name = NewStringCopy(default_string);

  // Type
  default_string = flimflam::kUnknownString;
  properties.Retrieve(flimflam::kTypeProperty, &default_string);
  info->type = ParseType(default_string);

  // Mode
  default_string = flimflam::kUnknownString;
  properties.Retrieve(flimflam::kModeProperty, &default_string);
  info->mode = ParseMode(default_string);

  // Security
  default_string = flimflam::kSecurityNone;
  properties.Retrieve(flimflam::kSecurityProperty, &default_string);
  info->security = ParseSecurity(default_string);

  // State
  default_string = flimflam::kUnknownString;
  properties.Retrieve(flimflam::kStateProperty, &default_string);
  info->state = ParseState(default_string);

  // Error
  default_string = flimflam::kUnknownString;
  properties.Retrieve(flimflam::kErrorProperty, &default_string);
  info->error = ParseError(default_string);

  // PassphraseRequired
  bool default_bool = false;
  properties.Retrieve(flimflam::kPassphraseRequiredProperty, &default_bool);
  info->passphrase_required = default_bool;

  // Passphrase
  default_string = "";
  properties.Retrieve(flimflam::kPassphraseProperty, &default_string);
  info->passphrase = NewStringCopy(default_string);

  // Identity
  default_string = "";
  properties.Retrieve(kEAPIdentityProperty, &default_string);
  info->identity = NewStringCopy(default_string);

  // Strength
  uint8 default_uint8 = 0;
  properties.Retrieve(flimflam::kSignalStrengthProperty, &default_uint8);
  info->strength = default_uint8;

  // Favorite
  default_bool = false;
  properties.Retrieve(flimflam::kFavoriteProperty, &default_bool);
  info->favorite = default_bool;

  // Connectable
  default_bool = true;
  properties.Retrieve(flimflam::kConnectableProperty, &default_bool);
  info->connectable = default_bool;

  // AutoConnect
  default_bool = false;
  properties.Retrieve(flimflam::kAutoConnectProperty, &default_bool);
  info->auto_connect = default_bool;

  // IsActive
  default_bool = false;
  properties.Retrieve(flimflam::kIsActiveProperty, &default_bool);
  info->is_active = default_bool;

  // Device
  glib::Value val;
  if (properties.Retrieve(flimflam::kDeviceProperty, &val)) {
    const gchar* path = static_cast<const gchar*>(g_value_get_boxed (&val));
    info->device_path = NewStringCopy(path);
  } else {
    info->device_path = NULL;
  }

  // ActivationState
  default_string = flimflam::kUnknownString;
  properties.Retrieve(flimflam::kActivationStateProperty, &default_string);
  info->activation_state = ParseActivationState(default_string);

  // Network technology
  default_string = flimflam::kUnknownString;
  properties.Retrieve(flimflam::kNetworkTechnologyProperty, &default_string);
  info->network_technology = ParseNetworkTechnology(default_string);

  // Roaming state
  default_string = flimflam::kUnknownString;
  properties.Retrieve(flimflam::kRoamingStateProperty, &default_string);
  info->roaming_state = ParseRoamingState(default_string);

  // Connectivity state
  default_string = flimflam::kUnknownString;
  properties.Retrieve(flimflam::kConnectivityStateProperty, &default_string);
  info->connectivity_state = ParseConnectivityState(default_string);

  // TODO(ers) restricted_pool is deprecated, remove once chrome is changed
  info->restricted_pool = (info->connectivity_state == CONN_STATE_RESTRICTED ||
                           info->connectivity_state == CONN_STATE_NONE);

  // CarrierInfo
  if (info->type == TYPE_CELLULAR) {
    info->carrier_info = new CarrierInfo;
    // Operator Name
    default_string = flimflam::kUnknownString;
    properties.Retrieve(flimflam::kOperatorNameProperty, &default_string);
    info->carrier_info->operator_name = NewStringCopy(default_string);
    // Operator Code
    default_string = flimflam::kUnknownString;
    properties.Retrieve(flimflam::kOperatorCodeProperty, &default_string);
    info->carrier_info->operator_code = NewStringCopy(default_string);
    // Payment URL
    default_string = flimflam::kUnknownString;
    properties.Retrieve(flimflam::kPaymentURLProperty, &default_string);
    info->carrier_info->payment_url = NewStringCopy(default_string);
  } else {
    info->carrier_info = NULL;
  }

  // EAP type
  default_string = "";
  properties.Retrieve(flimflam::kEAPEAPProperty, &default_string);
  info->eap = NewStringCopy(default_string);
  // Inner EAP type
  default_string = "";
  properties.Retrieve(kEAPInnerEAPProperty, &default_string);
  info->inner_eap = NewStringCopy(default_string);
  // Anonymous identity
  default_string = "";
  properties.Retrieve(kEAPAnonymousIdentityProperty, &default_string);
  info->anonymous_identity = NewStringCopy(default_string);
  // Client certificate
  default_string = "";
  properties.Retrieve(flimflam::kEAPClientCertProperty, &default_string);
  info->client_cert = NewStringCopy(default_string);
  // Certificate ID
  default_string = "";
  properties.Retrieve(flimflam::kEAPCertIDProperty, &default_string);
  info->cert_id = NewStringCopy(default_string);
  // Private key
  default_string = "";
  properties.Retrieve(kEAPPrivateKeyProperty, &default_string);
  info->private_key = NewStringCopy(default_string);
  // Private key password
  default_string = "";
  properties.Retrieve(kEAPPrivateKeyPasswordProperty, &default_string);
  info->private_key_passwd = NewStringCopy(default_string);
  // Private key ID
  default_string = "";
  properties.Retrieve(flimflam::kEAPKeyIDProperty, &default_string);
  info->key_id = NewStringCopy(default_string);
  // CA certificate
  default_string = "";
  properties.Retrieve(kEAPCACertProperty, &default_string);
  info->ca_cert = NewStringCopy(default_string);
  // CA certificate ID
  default_string = "";
  properties.Retrieve(kEAPCACertIDProperty, &default_string);
  info->ca_cert_id = NewStringCopy(default_string);
  // PKCS#11 PIN
  default_string = "";
  properties.Retrieve(flimflam::kEAPPINProperty, &default_string);
  info->pin = NewStringCopy(default_string);
  // EAP Password
  default_string = "";
  properties.Retrieve(kEAPPasswordProperty, &default_string);
  info->password = NewStringCopy(default_string);

  // DEPRECATED: Certificate path (backwards compat only)
  default_string = "";
  if (strcmp(info->eap, "TLS") == 0) {
    // "SETTINGS:cert_id=x,key_id=x,pin=x"
    std::string certpath("SETTINGS:");
    const char *comma = "";
    if (strlen(info->cert_id) != 0) {
      certpath += StringPrintf("%scert_id=%s", comma, info->cert_id);
      comma = ",";
    }
    if (strlen(info->key_id) != 0) {
      certpath += StringPrintf("%skey_id=%s", comma, info->key_id);
      comma = ",";
    }
    if (strlen(info->pin) != 0) {
      certpath += StringPrintf("%spin=%s", comma, info->pin);
      comma = ",";
    }
    info->cert_path = NewStringCopy(certpath.c_str());
  } else
    info->cert_path = NewStringCopy(default_string);

  // Device Info (initialize to NULL)
  info->device_info = NULL;
}

// Returns a ServiceInfo object populated with data from a
// given DBus object path.
//
// returns true on success.
bool GetServiceInfo(const char* path, ServiceInfo *info) {
  dbus::Proxy service_proxy(dbus::GetSystemBusConnection(),
                            flimflam::kFlimflamServiceName,
                            path,
                            flimflam::kFlimflamServiceInterface);
  glib::ScopedHashTable service_properties;
  if (!GetProperties(service_proxy, &service_properties))
    return false;
  info->service_path = NewStringCopy(path);
  ParseServiceProperties(service_properties, info);
  return true;
}

void DeleteCarrierInfo(CarrierInfo& carrier_info) {
  delete carrier_info.operator_name;
  delete carrier_info.operator_code;
  delete carrier_info.payment_url;
}

void DeleteDeviceInfo(DeviceInfo& device_info) {
  delete device_info.carrier;
  delete device_info.MEID;
  delete device_info.IMEI;
  delete device_info.IMSI;
  delete device_info.ESN;
  delete device_info.MDN;
  delete device_info.MIN;
  delete device_info.model_id;
  delete device_info.manufacturer;
  delete device_info.firmware_revision;
  delete device_info.hardware_revision;
  delete device_info.last_update;
  delete device_info.path;
  delete device_info.name;
}

// Deletes all of the heap allocated members of a given ServiceInfo instance.
void DeleteServiceInfoProperties(ServiceInfo& info) {
  delete info.service_path;
  delete info.name;
  delete info.passphrase;
  delete info.device_path;
  delete info.identity;
  delete info.cert_path;

  info.service_path = NULL;
  info.name = NULL;
  info.passphrase = NULL;
  info.device_path = NULL;
  info.identity = NULL;
  info.cert_path = NULL;

  if (info.carrier_info) {
    DeleteCarrierInfo(*info.carrier_info);
    delete info.carrier_info;
    info.carrier_info = NULL;
  }

  // Note: DeviceInfo is owned by SystemInfo.
}

}  // namespace

extern "C"
ServiceInfo* ChromeOSGetWifiService(const char* ssid,
                                    ConnectionSecurity security) {
  dbus::Proxy manager_proxy(dbus::GetSystemBusConnection(),
                            flimflam::kFlimflamServiceName,
                            flimflam::kFlimflamServicePath,
                            flimflam::kFlimflamManagerInterface);

  glib::ScopedHashTable scoped_properties =
      glib::ScopedHashTable(
          ::g_hash_table_new_full(::g_str_hash,
                                  ::g_str_equal,
                                  ::g_free,
                                  NULL));

  glib::Value value_mode(flimflam::kModeManaged);
  glib::Value value_type(flimflam::kTypeWifi);
  glib::Value value_ssid(ssid);
  if (security == SECURITY_UNKNOWN)
    security = SECURITY_RSN;
  glib::Value value_security(SecurityToString(security));

  ::GHashTable* properties = scoped_properties.get();
  ::g_hash_table_insert(properties, ::g_strdup(flimflam::kModeProperty),
                        &value_mode);
  ::g_hash_table_insert(properties, ::g_strdup(flimflam::kTypeProperty),
                        &value_type);
  ::g_hash_table_insert(properties, ::g_strdup(flimflam::kSSIDProperty),
                        &value_ssid);
  ::g_hash_table_insert(properties, ::g_strdup(flimflam::kSecurityProperty),
                        &value_security);


  glib::ScopedError error;
  char* path;
  if (!::dbus_g_proxy_call(manager_proxy.gproxy(),
                           flimflam::kGetWifiServiceFunction,
                           &Resetter(&error).lvalue(),
                           ::dbus_g_type_get_map("GHashTable",
                                                 G_TYPE_STRING,
                                                 G_TYPE_VALUE),
                           properties,
                           G_TYPE_INVALID,
                           DBUS_TYPE_G_OBJECT_PATH,
                           &path,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << "ChromeOSGetWifiService failed: "
        << (error->message ? error->message : "Unknown Error.");
    return NULL;
  }

  ServiceInfo* info = new ServiceInfo();
  if (!GetServiceInfo(path, info)) {
    LOG(WARNING) << "ChromeOSGetWifiService failed to parse ServiceInfo.";
    ::g_free(path);
    return NULL;
  }
  ::g_free(path);
  return info;
}

namespace {

class ScopedPtrGStrFreeV {
 public:
  inline void operator()(char** x) const {
    g_strfreev(x);
  }
};

static const char *map_oldprop_to_newprop(const char *oldprop)
{
  if (strcmp(oldprop, "key_id") == 0)
    return flimflam::kEAPKeyIDProperty;
  if (strcmp(oldprop, "cert_id") == 0)
    return flimflam::kEAPCertIDProperty;
  if (strcmp(oldprop, "pin") == 0)
    return flimflam::kEAPPINProperty;

  return NULL;
}

}  // namespace

extern "C"
bool ChromeOSConfigureWifiService(const char* ssid,
                                  ConnectionSecurity security,
                                  const char* passphrase,
                                  const char* identity,
                                  const char* certpath) {

  dbus::Proxy manager_proxy(dbus::GetSystemBusConnection(),
                            flimflam::kFlimflamServiceName,
                            flimflam::kFlimflamServicePath,
                            flimflam::kFlimflamManagerInterface);

  glib::ScopedHashTable scoped_properties =
      glib::ScopedHashTable(
          ::g_hash_table_new_full(::g_str_hash,
                                  ::g_str_equal,
                                  ::g_free,
                                  NULL));

  glib::Value value_mode(flimflam::kModeManaged);
  glib::Value value_type(flimflam::kTypeWifi);
  glib::Value value_ssid(ssid);
  if (security == SECURITY_UNKNOWN)
    security = SECURITY_RSN;
  glib::Value value_security(SecurityToString(security));
  glib::Value value_passphrase(passphrase);
  glib::Value value_identity(identity);
  glib::Value value_cert_path(certpath);

  ::GHashTable* properties = scoped_properties.get();
  ::g_hash_table_insert(properties, ::g_strdup(flimflam::kModeProperty),
                        &value_mode);
  ::g_hash_table_insert(properties, ::g_strdup(flimflam::kTypeProperty),
                        &value_type);
  ::g_hash_table_insert(properties, ::g_strdup(flimflam::kSSIDProperty),
                        &value_ssid);
  ::g_hash_table_insert(properties, ::g_strdup(flimflam::kSecurityProperty),
                        &value_security);
  ::g_hash_table_insert(properties, ::g_strdup(flimflam::kPassphraseProperty),
                        &value_passphrase);
  ::g_hash_table_insert(properties, ::g_strdup(kEAPIdentityProperty),
                        &value_identity);

  // DEPRECATED
  // Backwards-compatibility for "CertPath=SETTINGS:key_id=1,cert_id=2,..."
  ScopedVector<glib::Value> values;
  scoped_ptr_malloc<char *, ScopedPtrGStrFreeV> settings;
  if (::g_str_has_prefix(certpath, kCertpathSettingsPrefix)) {
      char **settingsp;
      settings.reset(::g_strsplit_set(
          certpath + strlen(kCertpathSettingsPrefix), ",=", 0));
      for (settingsp = settings.get(); *settingsp != NULL; settingsp += 2) {
        const char *key = map_oldprop_to_newprop(*settingsp);
        if (key == NULL) {
          LOG(WARNING) << "ConfigureWifiService, unknown key '" << key
                       << "' from certpath ";
          continue;
        }
        glib::Value *value = new glib::Value(*(settingsp + 1));
        values.push_back(value);
        ::g_hash_table_insert(properties, ::g_strdup(key), value);
      }
      // Presume EAP-TLS if we're here
      glib::Value *value = new glib::Value("TLS");
      values.push_back(value);
      ::g_hash_table_insert(properties,
                            ::g_strdup(flimflam::kEAPEAPProperty), value);
  } else {
    ::g_hash_table_insert(properties,
                          ::g_strdup(flimflam::kEAPClientCertProperty),
                          &value_cert_path);
  }


  glib::ScopedError error;
  if (!::dbus_g_proxy_call(manager_proxy.gproxy(),
                           kConfigureWifiServiceFunction,
                           &Resetter(&error).lvalue(),
                           ::dbus_g_type_get_map("GHashTable",
                                                 G_TYPE_STRING,
                                                 G_TYPE_VALUE),
                           properties,
                           G_TYPE_INVALID,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << "ChromeOSConfigureWifiService failed: "
        << (error->message ? error->message : "Unknown Error.");
    return false;
  }

  return true;
}

extern "C"
void ChromeOSFreeServiceInfo(ServiceInfo* info) {
  if (info == NULL)
    return;
  DeleteServiceInfoProperties(*info);
  delete info;
}

}  // namespace chromeos
