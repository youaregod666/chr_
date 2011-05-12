// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_network.h"  // NOLINT
#include "chromeos_network_deprecated.h"  // NOLINT

#include <algorithm>
#include <cstring>
#include <list>
#include <set>
#include <vector>

#include "marshal.glibmarshal.h"  // NOLINT
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/string_util.h"
#include "base/time.h"
#include "chromeos/dbus/dbus.h"  // NOLINT
#include "chromeos/glib/object.h"  // NOLINT
#include "chromeos/string.h"

// This code only exists to preserve backwards compatability with older
// versions of Chrome, and should be removed once R10 is fully deprecated
// and kCrosAPIMinVersion in chromeos_cros_api.h is updated appropriately.
// All of the code here was transplanted from chromeos_network.cc.

namespace chromeos {  // NOLINT

namespace {  // NOLINT

// Connman D-Bus service identifiers.
static const char* kConnmanManagerInterface = "org.chromium.flimflam.Manager";
static const char* kConnmanServiceInterface = "org.chromium.flimflam.Service";
static const char* kConnmanServiceName = "org.chromium.flimflam";
static const char* kConnmanDeviceInterface = "org.chromium.flimflam.Device";
static const char* kConnmanProfileInterface = "org.chromium.flimflam.Profile";

// Connman function names.
static const char* kGetPropertiesFunction = "GetProperties";
static const char* kRequestScanFunction = "RequestScan";
static const char* kConfigureWifiServiceFunction = "ConfigureWifiService";
static const char* kGetWifiServiceFunction = "GetWifiService";
static const char* kEnableTechnologyFunction = "EnableTechnology";
static const char* kDisableTechnologyFunction = "DisableTechnology";
static const char* kGetEntryFunction = "GetEntry";

// Connman property names.
static const char* kSecurityProperty = "Security";
static const char* kPassphraseProperty = "Passphrase";
static const char* kPassphraseRequiredProperty = "PassphraseRequired";
static const char* kServicesProperty = "Services";
static const char* kAvailableTechnologiesProperty = "AvailableTechnologies";
static const char* kEnabledTechnologiesProperty = "EnabledTechnologies";
static const char* kConnectedTechnologiesProperty = "ConnectedTechnologies";
static const char* kDefaultTechnologyProperty = "DefaultTechnology";
static const char* kOfflineModeProperty = "OfflineMode";
static const char* kSignalStrengthProperty = "Strength";
static const char* kNameProperty = "Name";
static const char* kStateProperty = "State";
static const char* kConnectivityStateProperty = "ConnectivityState";
static const char* kTypeProperty = "Type";
static const char* kUnknownString = "UNKNOWN";
static const char* kDeviceProperty = "Device";
static const char* kActivationStateProperty = "Cellular.ActivationState";
static const char* kNetworkTechnologyProperty = "Cellular.NetworkTechnology";
static const char* kRoamingStateProperty = "Cellular.RoamingState";
static const char* kOperatorNameProperty = "Cellular.OperatorName";
static const char* kOperatorCodeProperty = "Cellular.OperatorCode";
static const char* kPaymentURLProperty = "Cellular.OlpUrl";
static const char* kFavoriteProperty = "Favorite";
static const char* kConnectableProperty = "Connectable";
static const char* kAutoConnectProperty = "AutoConnect";
static const char* kIsActiveProperty = "IsActive";
static const char* kModeProperty = "Mode";
static const char* kErrorProperty = "Error";
static const char* kActiveProfileProperty = "ActiveProfile";
static const char* kEntriesProperty = "Entries";
static const char* kSSIDProperty = "SSID";
static const char* kScanningProperty = "Scanning";
static const char* kCarrierProperty = "Cellular.Carrier";
static const char* kMeidProperty = "Cellular.MEID";
static const char* kImeiProperty = "Cellular.IMEI";
static const char* kImsiProperty = "Cellular.IMSI";
static const char* kEsnProperty = "Cellular.ESN";
static const char* kMdnProperty = "Cellular.MDN";
static const char* kMinProperty = "Cellular.MIN";
static const char* kModelIDProperty = "Cellular.ModelID";
static const char* kManufacturerProperty = "Cellular.Manufacturer";
static const char* kFirmwareRevisionProperty = "Cellular.FirmwareRevision";
static const char* kHardwareRevisionProperty = "Cellular.HardwareRevision";
static const char* kLastDeviceUpdateProperty = "Cellular.LastDeviceUpdate";
static const char* kPRLVersionProperty = "Cellular.PRLVersion"; // (INT16)
static const char* kCertpathSettingsPrefix = "SETTINGS:";

// Connman EAP service properties
static const char* kEAPIdentityProperty = "EAP.Identity";
static const char* kEAPEAPProperty = "EAP.EAP";
static const char* kEAPInnerEAPProperty = "EAP.InnerEAP";
static const char* kEAPAnonymousIdentityProperty = "EAP.AnonymousIdentity";
static const char* kEAPClientCertProperty = "EAP.ClientCert";
static const char* kEAPCertIDProperty = "EAP.CertID";
static const char* kEAPPrivateKeyProperty = "EAP.PrivateKey";
static const char* kEAPPrivateKeyPasswordProperty = "EAP.PrivateKeyPassword";
static const char* kEAPKeyIDProperty = "EAP.KeyID";
static const char* kEAPCACertProperty = "EAP.CACert";
static const char* kEAPCACertIDProperty = "EAP.CACertID";
static const char* kEAPPINProperty = "EAP.PIN";
static const char* kEAPPasswordProperty = "EAP.Password";

// Connman network state
static const char* kOnline = "online";

// Connman monitored properties
static const char* kMonitorPropertyChanged = "PropertyChanged";

// Connman type options.
static const char* kTypeEthernet = "ethernet";
static const char* kTypeWifi = "wifi";
static const char* kTypeWimax = "wimax";
static const char* kTypeBluetooth = "bluetooth";
static const char* kTypeCellular = "cellular";
static const char* kTypeUnknown = "";

// Connman mode options.
static const char* kModeManaged = "managed";
static const char* kModeAdhoc = "adhoc";

// Connman security options.
static const char* kSecurityWpa = "wpa";
static const char* kSecurityWep = "wep";
static const char* kSecurityRsn = "rsn";
static const char* kSecurity8021x = "802_1x";
static const char* kSecurityNone = "none";

// Connman connectivity state options.
static const char* kConnStateUnrestricted = "unrestricted";
static const char* kConnStateRestricted = "restricted";
static const char* kConnStateNone = "none";

// Connman state options.
static const char* kStateIdle = "idle";
static const char* kStateCarrier = "carrier";
static const char* kStateAssociation = "association";
static const char* kStateConfiguration = "configuration";
static const char* kStateReady = "ready";
static const char* kStateDisconnect = "disconnect";
static const char* kStateFailure = "failure";
static const char* kStateActivationFailure = "activation-failure";

// Connman network technology options.
static const char* kNetworkTechnology1Xrtt = "1xRTT";
static const char* kNetworkTechnologyEvdo = "EVDO";
static const char* kNetworkTechnologyGprs = "GPRS";
static const char* kNetworkTechnologyEdge = "EDGE";
static const char* kNetworkTechnologyUmts = "UMTS";
static const char* kNetworkTechnologyHspa = "HSPA";
static const char* kNetworkTechnologyHspaPlus = "HSPA+";
static const char* kNetworkTechnologyLte = "LTE";
static const char* kNetworkTechnologyLteAdvanced = "LTE Advanced";


// Connman roaming state options
static const char* kRoamingStateHome = "home";
static const char* kRoamingStateRoaming = "roaming";
static const char* kRoamingStateUnknown = "unknown";

// Connman activation state options
static const char* kActivationStateActivated = "activated";
static const char* kActivationStateActivating = "activating";
static const char* kActivationStateNotActivated = "not-activated";
static const char* kActivationStatePartiallyActivated = "partially-activated";
static const char* kActivationStateUnknown = "unknown";

// Connman error options.
static const char* kErrorOutOfRange = "out-of-range";
static const char* kErrorPinMissing = "pin-missing";
static const char* kErrorDhcpFailed = "dhcp-failed";
static const char* kErrorConnectFailed = "connect-failed";
static const char* kErrorBadPassphrase = "bad-passphrase";
static const char* kErrorBadWEPKey = "bad-wepkey";
static const char* kErrorActivationFailed = "activation-failed";
static const char* kErrorNeedEvdo = "need-evdo";
static const char* kErrorNeedHomeNetwork = "need-home-network";
static const char* kErrorOtaspFailed = "otasp-failed";
static const char* kErrorAaaFailed = "aaa-failed";


}  // namespace

namespace {  // NOLINT

static ConnectionType ParseType(const std::string& type) {
  if (type == kTypeEthernet)
    return TYPE_ETHERNET;
  if (type == kTypeWifi)
    return TYPE_WIFI;
  if (type == kTypeWimax)
    return TYPE_WIMAX;
  if (type == kTypeBluetooth)
    return TYPE_BLUETOOTH;
  if (type == kTypeCellular)
    return TYPE_CELLULAR;
  return TYPE_UNKNOWN;
}

static const char* TypeToString(ConnectionType type) {
  switch (type) {
    case TYPE_UNKNOWN:
      break;
    case TYPE_ETHERNET:
      return kTypeEthernet;
    case TYPE_WIFI:
      return kTypeWifi;
    case TYPE_WIMAX:
      return kTypeWimax;
    case TYPE_BLUETOOTH:
      return kTypeBluetooth;
    case TYPE_CELLULAR:
      return kTypeCellular;
  }
  return kTypeUnknown;
}

static ConnectionMode ParseMode(const std::string& mode) {
  if (mode == kModeManaged)
    return MODE_MANAGED;
  if (mode == kModeAdhoc)
    return MODE_ADHOC;
  return MODE_UNKNOWN;
}

static ConnectionSecurity ParseSecurity(const std::string& security) {
  if (security == kSecurity8021x)
    return SECURITY_8021X;
  if (security == kSecurityRsn)
    return SECURITY_RSN;
  if (security == kSecurityWpa)
    return SECURITY_WPA;
  if (security == kSecurityWep)
    return SECURITY_WEP;
  if (security == kSecurityNone)
    return SECURITY_NONE;
  return SECURITY_UNKNOWN;
}

static const char* SecurityToString(ConnectionSecurity security) {
  switch (security) {
    case SECURITY_UNKNOWN:
      break;
    case SECURITY_8021X:
      return kSecurity8021x;
    case SECURITY_RSN:
      return kSecurityRsn;
    case SECURITY_WPA:
      return kSecurityWpa;
    case SECURITY_WEP:
      return kSecurityWep;
    case SECURITY_NONE:
      return kSecurityNone;
  }
  return kUnknownString;
}

static ConnectionState ParseState(const std::string& state) {
  if (state == kStateIdle)
    return STATE_IDLE;
  if (state == kStateCarrier)
    return STATE_CARRIER;
  if (state == kStateAssociation)
    return STATE_ASSOCIATION;
  if (state == kStateConfiguration)
    return STATE_CONFIGURATION;
  if (state == kStateReady)
    return STATE_READY;
  if (state == kStateDisconnect)
    return STATE_DISCONNECT;
  if (state == kStateFailure)
    return STATE_FAILURE;
  if (state == kStateActivationFailure)
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
  if (technology == kNetworkTechnology1Xrtt)
    return NETWORK_TECHNOLOGY_1XRTT;
  if (technology == kNetworkTechnologyEvdo)
    return NETWORK_TECHNOLOGY_EVDO;
  if (technology == kNetworkTechnologyGprs)
    return NETWORK_TECHNOLOGY_GPRS;
  if (technology == kNetworkTechnologyEdge)
    return NETWORK_TECHNOLOGY_EDGE;
  if (technology == kNetworkTechnologyUmts)
    return NETWORK_TECHNOLOGY_UMTS;
  if (technology == kNetworkTechnologyHspa)
    return NETWORK_TECHNOLOGY_HSPA;
  if (technology == kNetworkTechnologyHspaPlus)
    return NETWORK_TECHNOLOGY_HSPA_PLUS;
  if (technology == kNetworkTechnologyLte)
    return NETWORK_TECHNOLOGY_LTE;
  if (technology == kNetworkTechnologyLteAdvanced)
    return NETWORK_TECHNOLOGY_LTE_ADVANCED;
  return NETWORK_TECHNOLOGY_UNKNOWN;
}

static NetworkRoamingState ParseRoamingState(
    const std::string& roaming_state) {
  if (roaming_state == kRoamingStateHome)
    return ROAMING_STATE_HOME;
  if (roaming_state == kRoamingStateRoaming)
    return ROAMING_STATE_ROAMING;
  if (roaming_state == kRoamingStateUnknown)
    return ROAMING_STATE_UNKNOWN;
  return ROAMING_STATE_UNKNOWN;
}

static ActivationState ParseActivationState(
    const std::string& activation_state) {
  if (activation_state == kActivationStateActivated)
    return ACTIVATION_STATE_ACTIVATED;
  if (activation_state == kActivationStateActivating)
    return ACTIVATION_STATE_ACTIVATING;
  if (activation_state == kActivationStateNotActivated)
    return ACTIVATION_STATE_NOT_ACTIVATED;
  if (activation_state == kActivationStateUnknown)
    return ACTIVATION_STATE_UNKNOWN;
  if (activation_state == kActivationStatePartiallyActivated)
    return ACTIVATION_STATE_PARTIALLY_ACTIVATED;
  return ACTIVATION_STATE_UNKNOWN;
}

static ConnectionError ParseError(const std::string& error) {
  if (error == kErrorOutOfRange)
    return ERROR_OUT_OF_RANGE;
  if (error == kErrorPinMissing)
    return ERROR_PIN_MISSING;
  if (error == kErrorDhcpFailed)
    return ERROR_DHCP_FAILED;
  if (error == kErrorConnectFailed)
    return ERROR_CONNECT_FAILED;
  if (error == kErrorBadPassphrase)
    return ERROR_BAD_PASSPHRASE;
  if (error == kErrorBadWEPKey)
    return ERROR_BAD_WEPKEY;
  if (error == kErrorActivationFailed)
    return ERROR_ACTIVATION_FAILED;
  if (error == kErrorNeedEvdo)
    return ERROR_NEED_EVDO;
  if (error == kErrorNeedHomeNetwork)
    return ERROR_NEED_HOME_NETWORK;
  if (error == kErrorOtaspFailed)
    return ERROR_OTASP_FAILED;
  if (error == kErrorAaaFailed)
    return ERROR_AAA_FAILED;
  return ERROR_UNKNOWN;
}

void ParseDeviceProperties(const glib::ScopedHashTable& properties,
                           DeviceInfo* info) {
  // Name
  const char* default_string = kUnknownString;
  properties.Retrieve(kNameProperty, &default_string);
  info->name = NewStringCopy(default_string);
  // Type
  default_string = kUnknownString;
  properties.Retrieve(kTypeProperty, &default_string);
  info->type = ParseType(default_string);
  // Scanning
  bool default_bool = false;
  properties.Retrieve(kScanningProperty, &default_bool);
  info->scanning = default_bool;
}

void ParseCellularDeviceProperties(const glib::ScopedHashTable& properties,
                                   DeviceInfo* info) {
  // Carrier
  const char* default_string = kUnknownString;
  properties.Retrieve(kCarrierProperty, &default_string);
  info->carrier = NewStringCopy(default_string);
  // MEID
  default_string = kUnknownString;
  properties.Retrieve(kMeidProperty, &default_string);
  info->MEID = NewStringCopy(default_string);
  // IMEI
  default_string = kUnknownString;
  properties.Retrieve(kImeiProperty, &default_string);
  info->IMEI = NewStringCopy(default_string);
  // IMSI
  default_string = kUnknownString;
  properties.Retrieve(kImsiProperty, &default_string);
  info->IMSI = NewStringCopy(default_string);
  // ESN
  default_string = kUnknownString;
  properties.Retrieve(kEsnProperty, &default_string);
  info->ESN = NewStringCopy(default_string);
  // MDN
  default_string = kUnknownString;
  properties.Retrieve(kMdnProperty, &default_string);
  info->MDN = NewStringCopy(default_string);
  // MIN
  default_string = kUnknownString;
  properties.Retrieve(kMinProperty, &default_string);
  info->MIN = NewStringCopy(default_string);
  // ModelID
  default_string = kUnknownString;
  properties.Retrieve(kModelIDProperty, &default_string);
  info->model_id = NewStringCopy(default_string);
  // Manufacturer
  default_string = kUnknownString;
  properties.Retrieve(kManufacturerProperty, &default_string);
  info->manufacturer = NewStringCopy(default_string);
  // FirmwareRevision
  default_string = kUnknownString;
  properties.Retrieve(kFirmwareRevisionProperty, &default_string);
  info->firmware_revision = NewStringCopy(default_string);
  // HardwareRevision
  default_string = kUnknownString;
  properties.Retrieve(kHardwareRevisionProperty, &default_string);
  info->hardware_revision = NewStringCopy(default_string);
  // LastDeviceUpdate
  default_string = kUnknownString;
  properties.Retrieve(kLastDeviceUpdateProperty, &default_string);
  info->last_update = NewStringCopy(default_string);
  // PRLVersion
  unsigned int default_uint = 0;
  properties.Retrieve(kPRLVersionProperty, &default_uint);
  info->PRL_version = default_uint;
}

// Invokes the given method on the proxy and stores the result
// in the ScopedHashTable. The hash table will map strings to glib values.
bool GetProperties(const dbus::Proxy& proxy, glib::ScopedHashTable* result) {
  glib::ScopedError error;
  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           kGetPropertiesFunction,
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
                           kGetEntryFunction,
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
                           kConnmanServiceName,
                           device_path,
                           kConnmanDeviceInterface);

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
  const char* default_string = kUnknownString;
  properties.Retrieve(kNameProperty, &default_string);
  info->name = NewStringCopy(default_string);

  // Type
  default_string = kUnknownString;
  properties.Retrieve(kTypeProperty, &default_string);
  info->type = ParseType(default_string);

  // Mode
  default_string = kUnknownString;
  properties.Retrieve(kModeProperty, &default_string);
  info->mode = ParseMode(default_string);

  // Security
  default_string = kSecurityNone;
  properties.Retrieve(kSecurityProperty, &default_string);
  info->security = ParseSecurity(default_string);

  // State
  default_string = kUnknownString;
  properties.Retrieve(kStateProperty, &default_string);
  info->state = ParseState(default_string);

  // Error
  default_string = kUnknownString;
  properties.Retrieve(kErrorProperty, &default_string);
  info->error = ParseError(default_string);

  // PassphraseRequired
  bool default_bool = false;
  properties.Retrieve(kPassphraseRequiredProperty, &default_bool);
  info->passphrase_required = default_bool;

  // Passphrase
  default_string = "";
  properties.Retrieve(kPassphraseProperty, &default_string);
  info->passphrase = NewStringCopy(default_string);

  // Identity
  default_string = "";
  properties.Retrieve(kEAPIdentityProperty, &default_string);
  info->identity = NewStringCopy(default_string);

  // Strength
  uint8 default_uint8 = 0;
  properties.Retrieve(kSignalStrengthProperty, &default_uint8);
  info->strength = default_uint8;

  // Favorite
  default_bool = false;
  properties.Retrieve(kFavoriteProperty, &default_bool);
  info->favorite = default_bool;

  // Connectable
  default_bool = true;
  properties.Retrieve(kConnectableProperty, &default_bool);
  info->connectable = default_bool;

  // AutoConnect
  default_bool = false;
  properties.Retrieve(kAutoConnectProperty, &default_bool);
  info->auto_connect = default_bool;

  // IsActive
  default_bool = false;
  properties.Retrieve(kIsActiveProperty, &default_bool);
  info->is_active = default_bool;

  // Device
  glib::Value val;
  if (properties.Retrieve(kDeviceProperty, &val)) {
    const gchar* path = static_cast<const gchar*>(g_value_get_boxed (&val));
    info->device_path = NewStringCopy(path);
  } else {
    info->device_path = NULL;
  }

  // ActivationState
  default_string = kUnknownString;
  properties.Retrieve(kActivationStateProperty, &default_string);
  info->activation_state = ParseActivationState(default_string);

  // Network technology
  default_string = kUnknownString;
  properties.Retrieve(kNetworkTechnologyProperty, &default_string);
  info->network_technology = ParseNetworkTechnology(default_string);

  // Roaming state
  default_string = kUnknownString;
  properties.Retrieve(kRoamingStateProperty, &default_string);
  info->roaming_state = ParseRoamingState(default_string);

  // Connectivity state
  default_string = kUnknownString;
  properties.Retrieve(kConnectivityStateProperty, &default_string);
  info->connectivity_state = ParseConnectivityState(default_string);

  // TODO(ers) restricted_pool is deprecated, remove once chrome is changed
  info->restricted_pool = (info->connectivity_state == CONN_STATE_RESTRICTED ||
                           info->connectivity_state == CONN_STATE_NONE);

  // CarrierInfo
  if (info->type == TYPE_CELLULAR) {
    info->carrier_info = new CarrierInfo;
    // Operator Name
    default_string = kUnknownString;
    properties.Retrieve(kOperatorNameProperty, &default_string);
    info->carrier_info->operator_name = NewStringCopy(default_string);
    // Operator Code
    default_string = kUnknownString;
    properties.Retrieve(kOperatorCodeProperty, &default_string);
    info->carrier_info->operator_code = NewStringCopy(default_string);
    // Payment URL
    default_string = kUnknownString;
    properties.Retrieve(kPaymentURLProperty, &default_string);
    info->carrier_info->payment_url = NewStringCopy(default_string);
  } else {
    info->carrier_info = NULL;
  }

  // EAP type
  default_string = "";
  properties.Retrieve(kEAPEAPProperty, &default_string);
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
  properties.Retrieve(kEAPClientCertProperty, &default_string);
  info->client_cert = NewStringCopy(default_string);
  // Certificate ID
  default_string = "";
  properties.Retrieve(kEAPCertIDProperty, &default_string);
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
  properties.Retrieve(kEAPKeyIDProperty, &default_string);
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
  properties.Retrieve(kEAPPINProperty, &default_string);
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
                            kConnmanServiceName,
                            path,
                            kConnmanServiceInterface);
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

}

extern "C"
void ChromeOSRequestScan(ConnectionType type) {
  dbus::Proxy manager_proxy(dbus::GetSystemBusConnection(),
                            kConnmanServiceName,
                            "/",
                            kConnmanManagerInterface);
  gchar* device = ::g_strdup(TypeToString(type));
  glib::ScopedError error;
  if (!::dbus_g_proxy_call(manager_proxy.gproxy(),
                           kRequestScanFunction,
                           &Resetter(&error).lvalue(),
                           G_TYPE_STRING,
                           device,
                           G_TYPE_INVALID,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << "ChromeOSRequestScan failed: "
        << (error->message ? error->message : "Unknown Error.");
  }
  ::g_free(device);
}

extern "C"
ServiceInfo* ChromeOSGetWifiService(const char* ssid,
                                    ConnectionSecurity security) {
  dbus::Proxy manager_proxy(dbus::GetSystemBusConnection(),
                            kConnmanServiceName,
                            "/",
                            kConnmanManagerInterface);

  glib::ScopedHashTable scoped_properties =
      glib::ScopedHashTable(
          ::g_hash_table_new_full(::g_str_hash,
                                  ::g_str_equal,
                                  ::g_free,
                                  NULL));

  glib::Value value_mode(kModeManaged);
  glib::Value value_type(kTypeWifi);
  glib::Value value_ssid(ssid);
  if (security == SECURITY_UNKNOWN)
    security = SECURITY_RSN;
  glib::Value value_security(SecurityToString(security));

  ::GHashTable* properties = scoped_properties.get();
  ::g_hash_table_insert(properties, ::g_strdup(kModeProperty), &value_mode);
  ::g_hash_table_insert(properties, ::g_strdup(kTypeProperty), &value_type);
  ::g_hash_table_insert(properties, ::g_strdup(kSSIDProperty), &value_ssid);
  ::g_hash_table_insert(properties, ::g_strdup(kSecurityProperty),
                        &value_security);


  glib::ScopedError error;
  char* path;
  if (!::dbus_g_proxy_call(manager_proxy.gproxy(),
                           kGetWifiServiceFunction,
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
    return kEAPKeyIDProperty;
  if (strcmp(oldprop, "cert_id") == 0)
    return kEAPCertIDProperty;
  if (strcmp(oldprop, "pin") == 0)
    return kEAPPINProperty;

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
                            kConnmanServiceName,
                            "/",
                            kConnmanManagerInterface);

  glib::ScopedHashTable scoped_properties =
      glib::ScopedHashTable(
          ::g_hash_table_new_full(::g_str_hash,
                                  ::g_str_equal,
                                  ::g_free,
                                  NULL));

  glib::Value value_mode(kModeManaged);
  glib::Value value_type(kTypeWifi);
  glib::Value value_ssid(ssid);
  if (security == SECURITY_UNKNOWN)
    security = SECURITY_RSN;
  glib::Value value_security(SecurityToString(security));
  glib::Value value_passphrase(passphrase);
  glib::Value value_identity(identity);
  glib::Value value_cert_path(certpath);

  ::GHashTable* properties = scoped_properties.get();
  ::g_hash_table_insert(properties, ::g_strdup(kModeProperty), &value_mode);
  ::g_hash_table_insert(properties, ::g_strdup(kTypeProperty), &value_type);
  ::g_hash_table_insert(properties, ::g_strdup(kSSIDProperty), &value_ssid);
  ::g_hash_table_insert(properties, ::g_strdup(kSecurityProperty),
                        &value_security);
  ::g_hash_table_insert(properties, ::g_strdup(kPassphraseProperty),
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
      ::g_hash_table_insert(properties, ::g_strdup(kEAPEAPProperty), value);
  } else {
    ::g_hash_table_insert(properties, ::g_strdup(kEAPClientCertProperty),
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
SystemInfo* ChromeOSGetSystemInfo() {
  // TODO(chocobo): need to revisit the overhead of fetching the SystemInfo
  // object as one indivisible unit of data
  base::Time t0 = base::Time::Now();
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy manager_proxy(bus,
                            kConnmanServiceName,
                            "/",
                            kConnmanManagerInterface);

  glib::ScopedHashTable properties;
  if (!GetProperties(manager_proxy, &properties)) {
    return NULL;
  }

  SystemInfo* system = new SystemInfo();

  // Online (State == "online")
  const char* state = kUnknownString;
  properties.Retrieve(kStateProperty, &state);
  system->online = strcmp(kOnline, state) == 0;

  // AvailableTechnologies
  system->available_technologies = 0;
  glib::Value available_val;
  if (properties.Retrieve(kAvailableTechnologiesProperty, &available_val)) {
    gchar** available = static_cast<gchar**>(g_value_get_boxed(&available_val));
    while (*available) {
      system->available_technologies |= 1 << ParseType(*available);
      available++;
    }
  } else {
    LOG(WARNING) << "Missing property: " << kAvailableTechnologiesProperty;
  }

  // EnabledTechnologies
  system->enabled_technologies = 0;
  glib::Value enabled_val;
  if (properties.Retrieve(kEnabledTechnologiesProperty, &enabled_val)) {
    gchar** enabled = static_cast<gchar**>(g_value_get_boxed(&enabled_val));
    while (*enabled) {
      system->enabled_technologies |= 1 << ParseType(*enabled);
      enabled++;
    }
  } else {
    LOG(WARNING) << "Missing property: " << kEnabledTechnologiesProperty;
  }

  // ConnectedTechnologies
  system->connected_technologies = 0;
  glib::Value connected_val;
  if (properties.Retrieve(kConnectedTechnologiesProperty, &connected_val)) {
    gchar** connected = static_cast<gchar**>(g_value_get_boxed(&connected_val));
    while (*connected) {
      system->connected_technologies |= 1 << ParseType(*connected);
      connected++;
    }
  } else {
    LOG(WARNING) << "Missing property: " << kConnectedTechnologiesProperty;
  }

  // DefaultTechnology
  const char* default_technology = kTypeUnknown;
  properties.Retrieve(kDefaultTechnologyProperty, &default_technology);
  system->default_technology = ParseType(default_technology);

  // OfflineMode
  bool offline_mode = false;
  properties.Retrieve(kOfflineModeProperty, &offline_mode);
  system->offline_mode = offline_mode;

  // Services
  glib::Value services_val;
  std::vector<ServiceInfo> service_info_list;
  typedef std::set<std::pair<std::string, ConnectionType> > DeviceSet;
  DeviceSet device_set;
  if (properties.Retrieve(kServicesProperty, &services_val)) {
    GPtrArray* services =
        static_cast<GPtrArray*>(g_value_get_boxed(&services_val));
    for (size_t i = 0; i < services->len; i++) {
      const char* service_path =
          static_cast<const char*>(g_ptr_array_index(services, i));
      ServiceInfo info;
      if (!GetServiceInfo(service_path, &info))
        continue;
      // Push info onto the list of services.
      service_info_list.push_back(info);
      // Add an entry to the list of device paths.
      if (info.device_path) {
        device_set.insert(
            std::make_pair(std::string(info.device_path), info.type));
      }
    }
  } else {
    LOG(WARNING) << "Missing property: " << kServicesProperty;
  }

  // Copy service_info_list -> system->services.
  system->service_size = service_info_list.size();
  if (system->service_size == 0) {
    system->services = NULL;
  } else {
    system->services = new ServiceInfo[system->service_size];
    std::copy(service_info_list.begin(),
              service_info_list.end(),
              system->services);
  }

  // Devices
  typedef std::list<DeviceInfo> DeviceInfoList;
  DeviceInfoList device_info_list;
  for(DeviceSet::iterator iter = device_set.begin();
      iter != device_set.end(); ++iter) {
    const std::string& path = (*iter).first;
    ConnectionType type = (*iter).second;
    DeviceInfo device_info;
    if (GetDeviceInfo(path.c_str(), type, &device_info)) {
      device_info_list.push_back(device_info);
    } else {
      LOG(WARNING) << "No device info for:" << path;
    }
  }
  system->device_size = device_info_list.size();
  if (system->device_size == 0) {
    system->devices = NULL;
  } else {
    system->devices = new DeviceInfo[system->device_size];
    int index = 0;
    for(DeviceInfoList::iterator iter = device_info_list.begin();
        iter != device_info_list.end(); ++iter) {
      DeviceInfo* device = &system->devices[index++];  // pointer into array
      *device = *iter;  // copy DeviceInfo into array
      // Now iterate through services and set device_info if the path matches.
      for (int i=0; i < system->service_size; ++i) {
        ServiceInfo& service = system->services[i];
        if (service.device_path &&
            strcmp(service.device_path, device->path) == 0) {
          service.device_info = device;
        }
      }
    }
  }

  // Profile
  glib::Value profile_val;
  std::vector<ServiceInfo> remembered_service_info_list;
  if (properties.Retrieve(kActiveProfileProperty, &profile_val)) {
    const gchar* profile_path =
        static_cast<const gchar*>(g_value_get_boxed(&profile_val));

    dbus::Proxy profile_proxy(bus,
                              kConnmanServiceName,
                              profile_path,
                              kConnmanProfileInterface);

    glib::ScopedHashTable profile_properties;
    if (GetProperties(profile_proxy, &profile_properties)) {
      glib::Value entries_val;
      if (profile_properties.Retrieve(kEntriesProperty, &entries_val)) {
        gchar** entries = static_cast<gchar**>(g_value_get_boxed(&entries_val));
        while (*entries) {
          glib::ScopedHashTable entry_properties;
          char* service_path = static_cast<char*>(*entries);
          if (GetEntry(profile_proxy, service_path, &entry_properties)) {
            ServiceInfo info = {};
            info.service_path = NewStringCopy(service_path);
            ParseServiceProperties(entry_properties, &info);
            remembered_service_info_list.push_back(info);
          }
          entries++;
        }
      }
    }
  } else {
    LOG(WARNING) << "Missing property: " << kActiveProfileProperty;
  }

  // Copy remembered_service_info_list -> system->remembered_services.
  system->remembered_service_size = remembered_service_info_list.size();
  if (system->remembered_service_size == 0) {
    system->remembered_services = NULL;
  } else {
    system->remembered_services =
        new ServiceInfo[system->remembered_service_size];
    std::copy(remembered_service_info_list.begin(),
              remembered_service_info_list.end(),
              system->remembered_services);
  }

  // Set the runtime size of the ServiceInfo struct for GetServiceInfo() calls.
  system->service_info_size = sizeof(ServiceInfo);
  system->device_info_size = sizeof(DeviceInfo);

  base::TimeDelta dt = (base::Time::Now() - t0);
  LOG(INFO) << "SystemInfo: " << dt.InMilliseconds() << " ms.";

  return system;
}

extern "C"
bool ChromeOSEnableNetworkDevice(ConnectionType type, bool enable) {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy manager_proxy(bus,
                            kConnmanServiceName,
                            "/",
                            kConnmanManagerInterface);
  if (type == TYPE_UNKNOWN) {
    LOG(WARNING) << "EnableNetworkDevice called with an unknown type: " << type;
    return false;
  }

  gchar* device = ::g_strdup(TypeToString(type));
  glib::ScopedError error;
  if (!::dbus_g_proxy_call(manager_proxy.gproxy(),
                           enable ? kEnableTechnologyFunction :
                                    kDisableTechnologyFunction,
                           &Resetter(&error).lvalue(),
                           G_TYPE_STRING,
                           device,
                           G_TYPE_INVALID,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << "EnableNetworkDevice failed: "
        << (error->message ? error->message : "Unknown Error.");
    ::g_free(device);
    return false;
  }
  ::g_free(device);
  return true;
}

extern "C"
void ChromeOSFreeSystemInfo(SystemInfo* system) {
  if (system == NULL)
    return;
  if (system->service_size > 0) {
    std::for_each(system->services,
                  system->services + system->service_size,
                  &DeleteServiceInfoProperties);
    delete [] system->services;
  }
  if (system->remembered_service_size > 0) {
    std::for_each(system->remembered_services,
                  system->remembered_services + system->remembered_service_size,
                  &DeleteServiceInfoProperties);
    delete [] system->remembered_services;
  }
  if (system->device_size > 0) {
    std::for_each(system->devices,
                  system->devices + system->device_size,
                  &DeleteDeviceInfo);
    delete [] system->devices;
  }
  delete system;
}

extern "C"
void ChromeOSFreeServiceInfo(ServiceInfo* info) {
  if (info == NULL)
    return;
  DeleteServiceInfoProperties(*info);
  delete info;
}

// TODO(ers) ManagerPropertyChangedHandler is deprecated
class ManagerPropertyChangedHandler {
 public:
  typedef dbus::MonitorConnection<void(const char*, const glib::Value*)>*
      MonitorConnection;

  ManagerPropertyChangedHandler(const MonitorNetworkCallback& callback,
                                void* object)
     : callback_(callback),
       object_(object),
       connection_(NULL) {
  }

  static void Run(void* object,
                  const char* property,
                  const glib::Value* value) {
    ManagerPropertyChangedHandler* self =
        static_cast<ManagerPropertyChangedHandler*>(object);
    self->callback_(self->object_);
  }

  MonitorConnection& connection() {
    return connection_;
  }

 private:
  MonitorNetworkCallback callback_;
  void* object_;
  MonitorConnection connection_;
};

// HACK! We only want to register network marshallers once, but do not want
// to expose RegisterNetworkMarshallers() outside of libcros. See depercation
// note at top of file.
extern void RegisterNetworkMarshallers();

extern "C"
MonitorNetworkConnection ChromeOSMonitorNetwork(
    MonitorNetworkCallback callback, void* object) {
  RegisterNetworkMarshallers();
  dbus::Proxy proxy(dbus::GetSystemBusConnection(),
                    kConnmanServiceName,
                    "/",
                    kConnmanManagerInterface);
  MonitorNetworkConnection result =
      new ManagerPropertyChangedHandler(callback, object);
  result->connection() = dbus::Monitor(
      proxy, kMonitorPropertyChanged,
      &ManagerPropertyChangedHandler::Run, result);
  return result;
}

extern "C"
void ChromeOSDisconnectMonitorNetwork(MonitorNetworkConnection connection) {
  dbus::Disconnect(connection->connection());
  delete connection;
}

extern "C"
void ChromeOSRequestHiddenWifiNetwork(const char* ssid,
                                      const char* security,
                                      NetworkPropertiesCallback callback,
                                      void* object);

extern "C"
void ChromeOSRequestWifiServicePath(
    const char* ssid,
    ConnectionSecurity security,
    NetworkPropertiesCallback callback,
    void* object) {
  ChromeOSRequestHiddenWifiNetwork(
      ssid, SecurityToString(security), callback, object);
}

}  // namespace chromeos
