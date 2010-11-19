// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_network.h"  // NOLINT

#include <algorithm>
#include <cstring>
#include <list>
#include <set>
#include <vector>

#include "marshal.glibmarshal.h"  // NOLINT
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chromeos/dbus/dbus.h"  // NOLINT
#include "chromeos/glib/object.h"  // NOLINT
#include "chromeos/string.h"

// TODO(rtc): Unittest this code as soon as the tree is refactored.
namespace chromeos {  // NOLINT

namespace { // NOLINT

// Connman D-Bus service identifiers.
static const char* kConnmanManagerInterface = "org.chromium.flimflam.Manager";
static const char* kConnmanServiceInterface = "org.chromium.flimflam.Service";
static const char* kConnmanServiceName = "org.chromium.flimflam";
static const char* kConnmanIPConfigInterface = "org.chromium.flimflam.IPConfig";
static const char* kConnmanDeviceInterface = "org.chromium.flimflam.Device";
static const char* kConnmanProfileInterface = "org.chromium.flimflam.Profile";
static const char* kConnmanNetworkInterface = "org.chromium.flimflam.Network";

// Connman function names.
static const char* kGetPropertiesFunction = "GetProperties";
static const char* kSetPropertyFunction = "SetProperty";
static const char* kClearPropertyFunction = "ClearProperty";
static const char* kConnectFunction = "Connect";
static const char* kDisconnectFunction = "Disconnect";
static const char* kRequestScanFunction = "RequestScan";
static const char* kGetWifiServiceFunction = "GetWifiService";
static const char* kConfigureWifiServiceFunction = "ConfigureWifiService";
static const char* kEnableTechnologyFunction = "EnableTechnology";
static const char* kDisableTechnologyFunction = "DisableTechnology";
static const char* kAddIPConfigFunction = "AddIPConfig";
static const char* kRemoveConfigFunction = "Remove";
static const char* kGetEntryFunction = "GetEntry";
static const char* kDeleteEntryFunction = "DeleteEntry";
static const char* kActivateCellularModemFunction = "ActivateCellularModem";

// Connman property names.
static const char* kSecurityProperty = "Security";
static const char* kPassphraseProperty = "Passphrase";
static const char* kIdentityProperty = "Identity";
static const char* kCertPathProperty = "CertPath";
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
static const char* kDevicesProperty = "Devices";
static const char* kNetworksProperty = "Networks";
static const char* kConnectedProperty = "Connected";
static const char* kWifiChannelProperty = "WiFi.Channel";
static const char* kScanIntervalProperty = "ScanInterval";
static const char* kPoweredProperty = "Powered";

// Connman device info property names.
static const char* kIPConfigsProperty = "IPConfigs";
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

// Connman monitored properties
static const char* kMonitorPropertyChanged = "PropertyChanged";

// Connman network state
static const char* kOnline = "online";

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

// Connman state options.
static const char* kStateIdle = "idle";
static const char* kStateCarrier = "carrier";
static const char* kStateAssociation = "association";
static const char* kStateConfiguration = "configuration";
static const char* kStateReady = "ready";
static const char* kStateDisconnect = "disconnect";
static const char* kStateFailure = "failure";
static const char* kStateActivationFailure = "activation-failure";

// Connman connectivity state options.
static const char* kConnStateUnrestricted = "unrestricted";
static const char* kConnStateRestricted = "restricted";
static const char* kConnStateNone = "none";

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

// Cashew D-Bus service identifiers.
static const char* kCashewServiceName = "org.chromium.Cashew";
static const char* kCashewServicePath = "/org/chromium/Cashew";
static const char* kCashewServiceInterface = "org.chromium.Cashew";

// Cashew function names.
static const char* kRequestDataPlanFunction = "RequestDataPlansUpdate";
static const char* kRetrieveDataPlanFunction = "GetDataPlans";

// Connman monitored properties
static const char* kMonitorDataPlanUpdate = "DataPlansUpdate";

// Cashew data plan properties
static const char* kCellularPlanNameProperty = "CellularPlanName";
static const char* kCellularPlanTypeProperty = "CellularPlanType";
static const char* kCellularPlanUpdateTimeProperty = "CellularPlanUpdateTime";
static const char* kCellularPlanStartProperty = "CellularPlanStart";
static const char* kCellularPlanEndProperty = "CellularPlanEnd";
static const char* kCellularPlanDataBytesProperty = "CellularPlanDataBytes";
static const char* kCellularDataBytesUsedProperty = "CellularDataBytesUsed";

// Cashew Data Plan types
static const char* kCellularDataPlanUnlimited = "UNLIMITED";
static const char* kCellularDataPlanMeteredPaid = "METERED_PAID";
static const char* kCellularDataPlanMeteredBase = "METERED_BASE";

// IPConfig property names.
static const char* kMethodProperty = "Method";
static const char* kAddressProperty = "Address";
static const char* kMtuProperty = "Mtu";
static const char* kPrefixlenProperty = "Prefixlen";
static const char* kBroadcastProperty = "Broadcast";
static const char* kPeerAddressProperty = "PeerAddress";
static const char* kGatewayProperty = "Gateway";
static const char* kDomainNameProperty = "DomainName";
static const char* kNameServersProperty = "NameServers";

// IPConfig type options.
static const char* kTypeIPv4 = "ipv4";
static const char* kTypeIPv6 = "ipv6";
static const char* kTypeDHCP = "dhcp";
static const char* kTypeBOOTP = "bootp";
static const char* kTypeZeroConf = "zeroconf";
static const char* kTypeDHCP6 = "dhcp6";
static const char* kTypePPP = "ppp";

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
/*
static const char* ModeToString(ConnectionMode mode) {
  switch (mode) {
    case MODE_UNKNOWN:
      break;
    case MODE_MANAGED:
      return kModeManaged;
    case MODE_ADHOC:
      return kModeAdhoc;
  }
  return kUnknownString;
}
*/
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

static CellularDataPlanType ParseCellularDataPlanType(const std::string& type) {
  if (type == kCellularDataPlanUnlimited)
    return CELLULAR_DATA_PLAN_UNLIMITED;
  if (type == kCellularDataPlanMeteredPaid)
    return CELLULAR_DATA_PLAN_METERED_PAID;
  if (type == kCellularDataPlanMeteredBase)
    return CELLULAR_DATA_PLAN_METERED_BASE;
  return CELLULAR_DATA_PLAN_UNKNOWN;
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
                           &Resetter(result).lvalue(), G_TYPE_INVALID)) {
    LOG(WARNING) << "GetProperties on path '" << proxy.path() << "' failed: "
                 << (error->message ? error->message : "Unknown Error.");
    return false;
  }
  return true;
}

// Invokes the given method on the proxy and stores the result
// in the ScopedHashTable. The hash table will map strings to glib values.
bool GetEntry(const dbus::Proxy& proxy, gchar* entry,
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
                           &Resetter(result).lvalue(), G_TYPE_INVALID)) {
    LOG(WARNING) << "GetEntry failed: "
                 << (error->message ? error->message : "Unknown Error.");
    return false;
  }
  return true;
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
  properties.Retrieve(kIdentityProperty, &default_string);
  info->identity = NewStringCopy(default_string);

  // Certificate path
  default_string = "";
  properties.Retrieve(kCertPathProperty, &default_string);
  info->cert_path = NewStringCopy(default_string);

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

// Deletes all of the heap allocated members of a given
// CellularDataPlan instance.
static void DeleteDataPlanProperties(CellularDataPlanInfo &plan) {
  delete plan.plan_name;
}

static void ParseCellularDataPlan(const glib::ScopedHashTable& properties,
                           CellularDataPlanInfo* plan) {
  DCHECK(plan);
  // Plan Name
  const char* default_string = kUnknownString;
  properties.Retrieve(kCellularPlanNameProperty, &default_string);
  plan->plan_name = NewStringCopy(default_string);
  // Plan Type
  default_string = kUnknownString;
  properties.Retrieve(kCellularPlanTypeProperty, &default_string);
  plan->plan_type = ParseCellularDataPlanType(default_string);
  // Plan Update Time
  int64 default_time = 0;
  properties.Retrieve(kCellularPlanUpdateTimeProperty, &default_time);
  plan->update_time = default_time;
  // Plan Start Time
  default_time = 0;
  properties.Retrieve(kCellularPlanStartProperty, &default_time);
  plan->plan_start_time = default_time;
  // Plan End Time
  default_time = 0;
  properties.Retrieve(kCellularPlanEndProperty, &default_time);
  plan->plan_end_time = default_time;
  // Plan Data Bytes
  int64 default_bytes = 0;
  properties.Retrieve(kCellularPlanDataBytesProperty, &default_bytes);
  plan->plan_data_bytes = default_bytes;
  // Plan Bytes Used
  default_bytes = 0;
  properties.Retrieve(kCellularDataBytesUsedProperty, &default_bytes);
  plan->data_bytes_used = default_bytes;
}

static CellularDataPlanList* ParseCellularDataPlanList(
    const GPtrArray* properties_array)
{
  DCHECK(properties_array);

  CellularDataPlanList* data_plan_list = new CellularDataPlanList;
  if (data_plan_list == NULL)
    return NULL;

  data_plan_list->plans = NULL;
  data_plan_list->data_plan_size = sizeof(CellularDataPlanInfo);
  data_plan_list->plans_size = properties_array->len;
  if (properties_array->len > 0)
    data_plan_list->plans = new CellularDataPlanInfo[properties_array->len];
  for (guint i = 0; i < properties_array->len; ++i) {
    GHashTable* properties =
        static_cast<GHashTable*>(g_ptr_array_index(properties_array, i));
    // Convert to ScopedHashTable so that we can use Retrieve utility method.
    // We need to inc wrapped GHashTable's refcount or else ScopedHashTable's
    // dtor will dec it and cause premature deletion.
    glib::ScopedHashTable scoped_properties(g_hash_table_ref(properties));
    ParseCellularDataPlan(scoped_properties, &data_plan_list->plans[i]);
  }
  return data_plan_list;
}

// Register all dbus marshallers once.
void RegisterMarshallers() {
  static bool registered(false);
  // TODO(rtc/stevenjb): Do these need to be freed?
  if (!registered) {
    dbus_g_object_register_marshaller(marshal_VOID__STRING_BOXED,
                                      G_TYPE_NONE,
                                      G_TYPE_STRING,
                                      G_TYPE_VALUE,
                                      G_TYPE_INVALID);
    // NOTE: We have a second marshaller type that is also VOID__STRING_BOXED,
    // except it takes a GPtrArray BOXED type instead of G_TYPE_VALYE.
    // Because both map to marshal_VOID__STRING_BOXED, I am assuming that
    // we don't need to register both. -stevenjb
    registered = true;
  }
}

}  // namespace

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

extern "C"
void AppendElement(const GValue *value, gpointer user_data) {
  ListValue* list = static_cast<ListValue*>(user_data);
  if (G_VALUE_HOLDS(value, DBUS_TYPE_G_OBJECT_PATH)) {
    const char* path = static_cast<const char*>(::g_value_get_boxed(value));
    list->Append(Value::CreateStringValue(path));
  } else
    list->Append(Value::CreateNullValue());
}

class PropertyChangedHandler {
 public:
  typedef dbus::MonitorConnection<void(const char*, const glib::Value*)>*
      MonitorConnection;

  PropertyChangedHandler(const MonitorPropertyCallback& callback,
                         const char* path,
                         void* object)
     : callback_(callback),
       path_(path),
       object_(object),
       connection_(NULL) {
  }

  static void Run(void* object,
                  const char* property,
                  const glib::Value* gvalue) {
    PropertyChangedHandler* self =
        static_cast<PropertyChangedHandler*>(object);
    // Convert a glib::Value into a Value as defined in base/values.h
    scoped_ptr<Value> value(ConvertValue(gvalue));
    self->callback_(self->object_, self->path_.c_str(), property, value.get());
  }

  MonitorConnection& connection() {
    return connection_;
  }

 private:
  MonitorPropertyCallback callback_;
  std::string path_;
  void* object_;
  MonitorConnection connection_;

  static Value* ConvertValue(const glib::Value* gvalue) {
    if (G_VALUE_HOLDS_STRING(gvalue))
      return Value::CreateStringValue(g_value_get_string(gvalue));
    else if (G_VALUE_HOLDS_BOOLEAN(gvalue))
      return Value::CreateBooleanValue(
          static_cast<bool>(g_value_get_boolean(gvalue)));
    else if (G_VALUE_HOLDS_INT(gvalue))
      return Value::CreateIntegerValue(g_value_get_int(gvalue));
    else if (G_VALUE_HOLDS_UINT(gvalue))
      return Value::CreateIntegerValue(
          static_cast<int>(g_value_get_uint(gvalue)));
    else if (G_VALUE_HOLDS_UCHAR(gvalue))
      return Value::CreateIntegerValue(
          static_cast<int>(g_value_get_uchar(gvalue)));
    else if (G_VALUE_HOLDS(gvalue, G_TYPE_STRV)) {
      ListValue* list = new ListValue();
      GStrv strv = static_cast<GStrv>(::g_value_get_boxed(gvalue));
      while (*strv != NULL) {
        list->Append(Value::CreateStringValue(*strv));
        ++strv;
      }
      return list;
    } else if (dbus_g_type_is_collection(G_VALUE_TYPE(gvalue))) {
      ListValue* list = new ListValue();
      dbus_g_type_collection_value_iterate(gvalue, AppendElement, list);
      return list;
    }
    return Value::CreateNullValue();
  }

  DISALLOW_COPY_AND_ASSIGN(PropertyChangedHandler);
};

IPConfigType ParseIPConfigType(const std::string& type) {
  if (type == kTypeIPv4)
    return IPCONFIG_TYPE_IPV4;
  if (type == kTypeIPv6)
    return IPCONFIG_TYPE_IPV6;
  if (type == kTypeDHCP)
    return IPCONFIG_TYPE_DHCP;
  if (type == kTypeBOOTP)
    return IPCONFIG_TYPE_BOOTP;
  if (type == kTypeZeroConf)
    return IPCONFIG_TYPE_ZEROCONF;
  if (type == kTypeDHCP6)
    return IPCONFIG_TYPE_DHCP6;
  if (type == kTypePPP)
    return IPCONFIG_TYPE_PPP;
  return IPCONFIG_TYPE_UNKNOWN;
}

// Converts a prefix length to a netmask. (for ipv4)
// e.g. a netmask of 255.255.255.0 has a prefixlen of 24
std::string PrefixlenToNetmask(int32 prefixlen) {
  std::string netmask;
  for (int i = 0; i < 4; i++) {
    int len = 8;
    if (prefixlen >= 8) {
      prefixlen -= 8;
    } else {
      len = prefixlen;
      prefixlen = 0;
    }
    if (i > 0)
      netmask += ".";
    int num = len == 0 ? 0 : ((2L << (len - 1)) - 1) << (8 - len);
    netmask += StringPrintf("%d", num);
  }
  return netmask;
}

// Populates an instance of a IPConfig with the properties from an IPConfig
void ParseIPConfigProperties(const glib::ScopedHashTable& properties,
                            IPConfig* ipconfig) {
  const char* default_string = kUnknownString;
  properties.Retrieve(kMethodProperty, &default_string);
  ipconfig->type = ParseIPConfigType(default_string);

  default_string = "";
  properties.Retrieve(kAddressProperty, &default_string);
  ipconfig->address = NewStringCopy(default_string);

  int32 default_int32 = 0;
  properties.Retrieve(kMtuProperty, &default_int32);
  ipconfig->mtu = default_int32;

  default_int32 = 0;
  properties.Retrieve(kPrefixlenProperty, &default_int32);
  ipconfig->netmask = NewStringCopy(PrefixlenToNetmask(default_int32).c_str());

  default_string = "";
  properties.Retrieve(kBroadcastProperty, &default_string);
  ipconfig->broadcast = NewStringCopy(default_string);

  default_string = "";
  properties.Retrieve(kPeerAddressProperty, &default_string);
  ipconfig->peer_address = NewStringCopy(default_string);

  default_string = "";
  properties.Retrieve(kGatewayProperty, &default_string);
  ipconfig->gateway = NewStringCopy(default_string);

  default_string = "";
  properties.Retrieve(kDomainNameProperty, &default_string);
  ipconfig->domainname = NewStringCopy(default_string);

  glib::Value val;
  std::string value = "";
  // store nameservers as a comma delimited list
  if (properties.Retrieve(kNameServersProperty, &val)) {
    const char** nameservers =
        static_cast<const char**>(g_value_get_boxed (&val));
    if (*nameservers) {
      value += *nameservers;
      nameservers++;
      while (*nameservers) {
        value += ",";
        value += *nameservers;
        nameservers++;
      }
    }
  }
  ipconfig->name_servers = NewStringCopy(value.c_str());
}

// Returns a IPConfig object populated with data from a
// given DBus object path.
//
// returns true on success.
bool ParseIPConfig(const char* path, IPConfig *ipconfig) {
  ipconfig->path = NewStringCopy(path);

  dbus::Proxy config_proxy(dbus::GetSystemBusConnection(),
                           kConnmanServiceName,
                           path,
                           kConnmanIPConfigInterface);
  glib::ScopedHashTable properties;
  if (!GetProperties(config_proxy, &properties))
    return false;
  ParseIPConfigProperties(properties, ipconfig);
  return true;
}

extern "C"
IPConfigStatus* ChromeOSListIPConfigs(const char* device_path) {
  if (!device_path)
    return NULL;

  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy device_proxy(bus,
                           kConnmanServiceName,
                           device_path,
                           kConnmanDeviceInterface);

  glib::ScopedHashTable properties;
  if (!GetProperties(device_proxy, &properties)) {
    return NULL;
  }

  GHashTable* table = properties.get();
  gpointer ptr = g_hash_table_lookup(table, kIPConfigsProperty);
  if (ptr == NULL)
    return NULL;

  GPtrArray* ips_value =
      static_cast<GPtrArray*>(g_value_get_boxed(static_cast<GValue*>(ptr)));

  IPConfigStatus* result = new IPConfigStatus();
  std::vector<IPConfig> buffer;
  const char* path = NULL;
  for (size_t i = 0; i < ips_value->len; i++) {
    path = static_cast<const char*>(g_ptr_array_index(ips_value, i));
    if (!path) {
      LOG(WARNING) << "Found NULL ip for device " << device_path;
      continue;
    }
    IPConfig ipconfig = {};
    if (!ParseIPConfig(path, &ipconfig))
      continue;
    buffer.push_back(ipconfig);
  }
  result->size = buffer.size();
  if (result->size == 0) {
    result->ips = NULL;
  } else {
    result->ips = new IPConfig[result->size];
  }
  std::copy(buffer.begin(), buffer.end(), result->ips);

  // Store the hardware address as well.
  const char* hardware_address = "";
  properties.Retrieve(kAddressProperty, &hardware_address);
  result->hardware_address = NewStringCopy(hardware_address);

  return result;
}

extern "C"
bool ChromeOSAddIPConfig(const char* device_path, IPConfigType type) {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy device_proxy(bus,
                           kConnmanServiceName,
                           device_path,
                           kConnmanDeviceInterface);

  glib::ScopedError error;
  ::DBusGProxy obj;
  const char* type_str = NULL;
  switch(type) {
    case IPCONFIG_TYPE_IPV4:
      type_str = kTypeIPv4;
      break;
    case IPCONFIG_TYPE_IPV6:
      type_str = kTypeIPv6;
      break;
    case IPCONFIG_TYPE_DHCP:
      type_str = kTypeDHCP;
      break;
    case IPCONFIG_TYPE_BOOTP:
      type_str = kTypeBOOTP;
      break;
    case IPCONFIG_TYPE_ZEROCONF:
      type_str = kTypeZeroConf;
      break;
    case IPCONFIG_TYPE_DHCP6:
      type_str = kTypeDHCP6;
      break;
    case IPCONFIG_TYPE_PPP:
      type_str = kTypePPP;
      break;
    default:
      return false;
  };

  if (!::dbus_g_proxy_call(device_proxy.gproxy(),
                           kAddIPConfigFunction,
                           &Resetter(&error).lvalue(),
                           G_TYPE_STRING,
                           type_str,
                           G_TYPE_INVALID,
                           DBUS_TYPE_G_PROXY,
                           &obj,
                           G_TYPE_INVALID)) {
    LOG(WARNING) <<"Add IPConfig failed: "
        << (error->message ? error->message : "Unknown Error.");
    return false;
  }
  return true;
}

extern "C"
bool ChromeOSSaveIPConfig(IPConfig* config) {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy device_proxy(bus,
                           kConnmanServiceName,
                           config->path,
                           kConnmanIPConfigInterface);
/*
  TODO(chocobo): Save all the values

  glib::Value value_set;

  glib::ScopedError error;

  if (!::dbus_g_proxy_call(device_proxy.gproxy(),
                           kSetPropertyFunction,
                           &Resetter(&error).lvalue(),
                           G_TYPE_STRING,
                           key,
                           G_TYPE_VALUE,
                           &value_set,
                           G_TYPE_INVALID,
                           G_TYPE_INVALID)) {
    LOG(WARNING) <<"Set IPConfig Property failed: "
        << (error->message ? error->message : "Unknown Error.");
    return false;
  }
*/
  return true;
}

extern "C"
bool ChromeOSRemoveIPConfig(IPConfig* config) {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy config_proxy(bus,
                           kConnmanServiceName,
                           config->path,
                           kConnmanIPConfigInterface);

  glib::ScopedError error;

  if (!::dbus_g_proxy_call(config_proxy.gproxy(),
                           kRemoveConfigFunction,
                           &Resetter(&error).lvalue(),
                           G_TYPE_INVALID,
                           G_TYPE_INVALID)) {
    LOG(WARNING) <<"Set IPConfig Property failed: "
        << (error->message ? error->message : "Unknown Error.");
    return false;
  }
  return true;
}

void DeleteIPConfigProperties(IPConfig config) {
  if (config.path)
    delete config.path;
  if (config.address)
    delete config.address;
  if (config.broadcast)
    delete config.broadcast;
  if (config.netmask)
    delete config.netmask;
  if (config.gateway)
    delete config.gateway;
  if (config.domainname)
    delete config.domainname;
  if (config.name_servers)
    delete config.name_servers;
}

extern "C"
void ChromeOSFreeIPConfig(IPConfig* config) {
  DeleteIPConfigProperties(*config);
  delete config;
}

extern "C"
void ChromeOSFreeIPConfigStatus(IPConfigStatus* status) {
  std::for_each(status->ips,
                status->ips + status->size,
                &DeleteIPConfigProperties);
  delete [] status->ips;
  delete status->hardware_address;
  delete status;

}

// BEGIN DEPRECATED
extern "C"
MonitorNetworkConnection ChromeOSMonitorNetwork(
    MonitorNetworkCallback callback, void* object) {
  RegisterMarshallers();
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
// END DEPRECATED

extern "C"
PropertyChangeMonitor ChromeOSMonitorNetworkManager(
    MonitorPropertyCallback callback,
    void* object) {
  RegisterMarshallers();
  dbus::Proxy proxy(dbus::GetSystemBusConnection(),
                    kConnmanServiceName,
                    "/",
                    kConnmanManagerInterface);
  PropertyChangeMonitor monitor =
      new PropertyChangedHandler(callback, "/", object);
  monitor->connection() = dbus::Monitor(proxy,
                                        kMonitorPropertyChanged,
                                        &PropertyChangedHandler::Run,
                                        monitor);
  return monitor;
}

extern "C"
void ChromeOSDisconnectPropertyChangeMonitor(
    PropertyChangeMonitor connection) {
  dbus::Disconnect(connection->connection());
  delete connection;
}

extern "C"
PropertyChangeMonitor ChromeOSMonitorNetworkService(
    MonitorPropertyCallback callback,
    const char* service_path,
    void* object) {
  RegisterMarshallers();
  dbus::Proxy service_proxy(dbus::GetSystemBusConnection(),
                            kConnmanServiceName,
                            service_path,
                            kConnmanServiceInterface);
  PropertyChangeMonitor monitor =
      new PropertyChangedHandler(callback, service_path, object);
  monitor->connection() = dbus::Monitor(service_proxy,
                                        kMonitorPropertyChanged,
                                        &PropertyChangedHandler::Run,
                                        monitor);
  return monitor;
}

extern "C"
void ChromeOSDisconnectMonitorService(PropertyChangeMonitor connection) {
  dbus::Disconnect(connection->connection());
  delete connection;
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

extern "C"
bool ChromeOSActivateCellularModem(const char* service_path,
                                   const char* carrier) {
  if (carrier == NULL)
    carrier = "";

  dbus::Proxy service_proxy(dbus::GetSystemBusConnection(),
                            kConnmanServiceName,
                            service_path,
                            kConnmanServiceInterface);

  // Now try activating.
  glib::ScopedError error;
  if (!::dbus_g_proxy_call(service_proxy.gproxy(),
                           kActivateCellularModemFunction,
                           &Resetter(&error).lvalue(),
                           G_TYPE_STRING,
                           carrier,
                           G_TYPE_INVALID,
                           G_TYPE_INVALID)) {
    if (std::string("In progress") == error->message) {
      LOG(WARNING) <<
          "ChromeOSActivateCellularModem: already started activation";
      return true;
    }
    LOG(WARNING) << "ChromeOSActivateCellularModem failed: "
        << (error->message ? error->message : "Unknown Error.");
    return false;
  }
  return true;
}

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
  ::g_hash_table_insert(properties, ::g_strdup(kIdentityProperty),
                        &value_identity);
  ::g_hash_table_insert(properties, ::g_strdup(kCertPathProperty),
                        &value_cert_path);


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
bool ChromeOSConnectToNetworkWithCertInfo(const char* service_path,
                                          const char* passphrase,
                                          const char* identity,
                                          const char* certpath) {
  dbus::Proxy service_proxy(dbus::GetSystemBusConnection(),
                            kConnmanServiceName,
                            service_path,
                            kConnmanServiceInterface);

  // Set passphrase if non-null.
  if (passphrase) {
    glib::Value value_passphrase(passphrase);
    glib::ScopedError error;
    if (!::dbus_g_proxy_call(service_proxy.gproxy(),
                             kSetPropertyFunction,
                             &Resetter(&error).lvalue(),
                             G_TYPE_STRING,
                             kPassphraseProperty,
                             G_TYPE_VALUE,
                             &value_passphrase,
                             G_TYPE_INVALID,
                             G_TYPE_INVALID)) {
      LOG(WARNING) << "ConnectToNetwork failed on set passphrase: "
          << (error->message ? error->message : "Unknown Error.");
      return false;
    }
  }

  // Set identity if non-null.
  if (identity) {
    glib::Value value_identity(identity);
    glib::ScopedError error;
    if (!::dbus_g_proxy_call(service_proxy.gproxy(),
                             kSetPropertyFunction,
                             &Resetter(&error).lvalue(),
                             G_TYPE_STRING,
                             kIdentityProperty,
                             G_TYPE_VALUE,
                             &value_identity,
                             G_TYPE_INVALID,
                             G_TYPE_INVALID)) {
      LOG(WARNING) << "ConnectToNetwork failed on set identity: "
          << (error->message ? error->message : "Unknown Error.");
      return false;
    }
  }

  // Set certificate path if non-null.
  if (certpath) {
    glib::Value value_certpath(certpath);
    glib::ScopedError error;
    if (!::dbus_g_proxy_call(service_proxy.gproxy(),
                             kSetPropertyFunction,
                             &Resetter(&error).lvalue(),
                             G_TYPE_STRING,
                             kCertPathProperty,
                             G_TYPE_VALUE,
                             &value_certpath,
                             G_TYPE_INVALID,
                             G_TYPE_INVALID)) {
      LOG(WARNING) << "ConnectToNetwork failed on set certpath: "
          << (error->message ? error->message : "Unknown Error.");
      return false;
    }
  }

  // Now try connecting.
  glib::ScopedError error;
  if (!::dbus_g_proxy_call(service_proxy.gproxy(),
                           kConnectFunction,
                           &Resetter(&error).lvalue(),
                           G_TYPE_INVALID,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << "ConnectToNetwork failed: "
        << (error->message ? error->message : "Unknown Error.");
    return false;
  }
  return true;
}


extern "C"
bool ChromeOSConnectToNetwork(const char* service_path,
                              const char* passphrase) {

  return ChromeOSConnectToNetworkWithCertInfo(service_path, passphrase,
                                              NULL, NULL);
}

extern "C"
bool ChromeOSDisconnectFromNetwork(const char* service_path) {
  dbus::Proxy service_proxy(dbus::GetSystemBusConnection(),
                            kConnmanServiceName,
                            service_path,
                            kConnmanServiceInterface);

  // Now try disconnecting.
  glib::ScopedError error;
  if (!::dbus_g_proxy_call(service_proxy.gproxy(),
                           kDisconnectFunction,
                           &Resetter(&error).lvalue(),
                           G_TYPE_INVALID,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << "DisconnectFromNetwork failed: "
        << (error->message ? error->message : "Unknown Error.");
    return false;
  }
  return true;
}

extern "C"
bool ChromeOSDeleteRememberedService(const char* service_path) {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy manager_proxy(bus,
                            kConnmanServiceName,
                            "/",
                            kConnmanManagerInterface);

  glib::ScopedHashTable properties;
  if (!GetProperties(manager_proxy, &properties)) {
    return false;
  }

  glib::Value profile_val;
  properties.Retrieve(kActiveProfileProperty, &profile_val);
  const gchar* profile_path =
      static_cast<const gchar*>(g_value_get_boxed(&profile_val));

  dbus::Proxy profile_proxy(bus,
                            kConnmanServiceName,
                            profile_path,
                            kConnmanProfileInterface);

  glib::ScopedError error;
  if (!::dbus_g_proxy_call(profile_proxy.gproxy(),
                           kDeleteEntryFunction,
                           &Resetter(&error).lvalue(),
                           G_TYPE_STRING,
                           service_path,
                           G_TYPE_INVALID,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << "DeleteEntry failed: "
                 << (error->message ? error->message : "Unknown Error.");
    return false;
  }
  return true;
}

extern "C"
SystemInfo* ChromeOSGetSystemInfo() {
  // TODO(chocobo): need to revisit the overhead of fetching the SystemInfo
  // object as one indivisible unit of data
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
          if (GetEntry(profile_proxy, *entries, &entry_properties)) {
            ServiceInfo info = {};
            info.service_path = NewStringCopy(*entries);
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
bool ChromeOSSetOfflineMode(bool offline) {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy manager_proxy(bus,
                            kConnmanServiceName,
                            "/",
                            kConnmanManagerInterface);

  glib::Value value_offline(offline);

  glib::ScopedError error;
  if (!::dbus_g_proxy_call(manager_proxy.gproxy(),
                           kSetPropertyFunction,
                           &Resetter(&error).lvalue(),
                           G_TYPE_STRING,
                           kOfflineModeProperty,
                           G_TYPE_VALUE,
                           &value_offline,
                           G_TYPE_INVALID,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << "SetOfflineMode failed: "
        << (error->message ? error->message : "Unknown Error.");
    return false;
  }

  return true;
}

extern "C"
bool ChromeOSSetAutoConnect(const char* service_path, bool auto_connect) {
  dbus::Proxy service_proxy(dbus::GetSystemBusConnection(),
                            kConnmanServiceName,
                            service_path,
                            kConnmanServiceInterface);

  glib::Value value_auto_connect(auto_connect);

  glib::ScopedError error;
  if (!::dbus_g_proxy_call(service_proxy.gproxy(),
                           kSetPropertyFunction,
                           &Resetter(&error).lvalue(),
                           G_TYPE_STRING,
                           kAutoConnectProperty,
                           G_TYPE_VALUE,
                           &value_auto_connect,
                           G_TYPE_INVALID,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << "SetAutoConnect failed: "
                 << (error->message ? error->message : "Unknown Error.");
    return false;
  }

  return true;
}

static bool ClearServiceProperty(const char* service_path,
                                 const char* property) {
  dbus::Proxy service_proxy(dbus::GetSystemBusConnection(),
                            kConnmanServiceName,
                            service_path,
                            kConnmanServiceInterface);

  glib::ScopedError error;
  if (!::dbus_g_proxy_call(service_proxy.gproxy(),
                           kClearPropertyFunction,
                           &Resetter(&error).lvalue(),
                           G_TYPE_STRING,
                           property,
                           G_TYPE_INVALID,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << "Clearing property " << property << " failed: "
                 << (error->message ? error->message : "Unknown Error.");
    return false;
  }

  return true;
}

static bool SetServiceStringProperty(const char* service_path,
                                     const char* property, const char* value) {
  dbus::Proxy service_proxy(dbus::GetSystemBusConnection(),
                            kConnmanServiceName,
                            service_path,
                            kConnmanServiceInterface);

  glib::Value value_string(value);
  glib::ScopedError error;
  if (!::dbus_g_proxy_call(service_proxy.gproxy(),
                           kSetPropertyFunction,
                           &Resetter(&error).lvalue(),
                           G_TYPE_STRING,
                           property,
                           G_TYPE_VALUE,
                           &value_string,
                           G_TYPE_INVALID,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << "Setting property " << property << " failed: "
                 << (error->message ? error->message : "Unknown Error.");
    return false;
  }

  return true;

}


extern "C"
bool ChromeOSSetPassphrase(const char* service_path, const char* passphrase) {
  if (passphrase && strlen(passphrase) > 0) {
    return SetServiceStringProperty(service_path, kPassphraseProperty,
                                    passphrase);
  } else {
    return ClearServiceProperty(service_path, kPassphraseProperty);
  }
}

extern "C"
bool ChromeOSSetIdentity(const char* service_path, const char* identity) {
  return SetServiceStringProperty(service_path, kIdentityProperty, identity);
}

extern "C"
bool ChromeOSSetCertPath(const char* service_path, const char* cert_path) {
  return SetServiceStringProperty(service_path, kCertPathProperty, cert_path);
}

namespace {
// Returns a DeviceNetworkInfo object populated with data from a
// given DBus object path.
//
// returns true on success.
bool ParseDeviceNetworkInfo(dbus::BusConnection bus,
                            const char* path,
                            DeviceNetworkInfo* info) {
  dbus::Proxy network_proxy(bus,
                            kConnmanServiceName,
                            path,
                            kConnmanNetworkInterface);
  glib::ScopedHashTable properties;
  if (!GetProperties(network_proxy, &properties))
    return false;
  info->network_path = NewStringCopy(path);

  // Address (mandatory)
  const char* default_string = kUnknownString;
  if (!properties.Retrieve(kAddressProperty, &default_string))
    return false;
  info->address = NewStringCopy(default_string);
  // CAREFULL: Do not return false after here, without freeing strings.

  // Name
  default_string = kUnknownString;
  properties.Retrieve(kNameProperty, &default_string);
  info->name = NewStringCopy(default_string);

  // Strength
  uint8 default_uint8 = 0;
  properties.Retrieve(kSignalStrengthProperty, &default_uint8);
  info->strength = default_uint8;

  // kWifi.Channel
  // (This is a uint16, stored in GValue as a uint;
  // see http://dbus.freedesktop.org/doc/dbus-tutorial.html)
  uint default_uint = 0;
  properties.Retrieve(kWifiChannelProperty, &default_uint);
  info->channel = default_uint;

  // Connected
  bool default_bool = false;
  properties.Retrieve(kConnectedProperty, &default_bool);
  info->connected = default_bool;

  return true;
}
}  // namespace

extern "C"
DeviceNetworkList* ChromeOSGetDeviceNetworkList() {
  glib::Value devices_val;
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  {
    dbus::Proxy manager_proxy(bus,
                              kConnmanServiceName,
                              "/",
                              kConnmanManagerInterface);

    glib::ScopedHashTable properties;
    if (!GetProperties(manager_proxy, &properties)) {
      LOG(WARNING) << "Couldn't read managers's properties";
      return NULL;
    }

    // Get the Devices property from the manager
    if (!properties.Retrieve(kDevicesProperty, &devices_val)) {
      LOG(WARNING) << kDevicesProperty << " property not found";
      return NULL;
    }
  }
  GPtrArray* devices = static_cast<GPtrArray*>(g_value_get_boxed(&devices_val));
  std::vector<DeviceNetworkInfo> buffer;
  bool found_at_least_one_device = false;
  for (size_t i = 0; i < devices->len; i++) {
    const char* device_path =
        static_cast<const char*>(g_ptr_array_index(devices, i));
    dbus::Proxy device_proxy(bus,
                             kConnmanServiceName,
                             device_path,
                             kConnmanDeviceInterface);

    glib::ScopedHashTable properties;
    if (!GetProperties(device_proxy, &properties)) {
      LOG(WARNING) << "Couldn't read device's properties";
      continue;
    }

    glib::Value networks_val;
    if (!properties.Retrieve(kNetworksProperty, &networks_val))
      continue;  // Some devices do not list networks, e.g. ethernet.
    GPtrArray* networks =
        static_cast<GPtrArray*>(g_value_get_boxed(&networks_val));
    DCHECK(networks);

    bool device_powered = false;
    if (properties.Retrieve(kPoweredProperty, &device_powered)
        && !device_powered) {
      continue;  // Skip devices that are not powered up.
    }
    uint scan_interval = 0;
    properties.Retrieve(kScanIntervalProperty, &scan_interval);

    found_at_least_one_device = true;
    for (size_t j = 0; j < networks->len; j++) {
      const char* network_path =
          static_cast<const char*>(g_ptr_array_index(networks, j));
      DeviceNetworkInfo info = {};
      if (!ParseDeviceNetworkInfo(bus, network_path, &info))
        continue;
      info.device_path = NewStringCopy(device_path);
      // Using the scan interval as a proxy for approximate age.
      // TODO(joth): Replace with actual age, when available from dbus.
      info.age_seconds = scan_interval;
      buffer.push_back(info);
    }
  }
  if (!found_at_least_one_device) {
    DCHECK_EQ(0u, buffer.size());
    return NULL;  // No powered device found that has a 'Networks' array.
  }
  DeviceNetworkList* network_list = new DeviceNetworkList();
  network_list->network_size = buffer.size();
  network_list->networks = network_list->network_size == 0 ? NULL :
                           new DeviceNetworkInfo[network_list->network_size];
  std::copy(buffer.begin(), buffer.end(), network_list->networks);
  return network_list;
}

extern "C"
void ChromeOSFreeDeviceNetworkList(DeviceNetworkList* list) {
  delete [] list->networks;
  delete list;
}

// Cashew services

extern "C"
void ChromeOSFreeCellularDataPlanList(CellularDataPlanList* data_plan_list) {
  if (data_plan_list == NULL)
    return;
  if (data_plan_list->plans_size > 0) {
    std::for_each(data_plan_list->plans,
                  data_plan_list->plans + data_plan_list->plans_size,
                  &DeleteDataPlanProperties);
    delete [] data_plan_list->plans;
  }
  delete data_plan_list;
}

// NOTE: We can't use dbus::MonitorConnection here because there is no
// type_to_gtypeid<GHashTable>, and MonitorConnection only takes
// up to 3 arguments, so we can't pass each argument either.
// Instead, make dbus calls directly.

class DataPlanUpdateHandler {
 public:
  DataPlanUpdateHandler(const MonitorDataPlanCallback& callback,
                         void* object)
     : callback_(callback),
       object_(object) {
    proxy_ = dbus::Proxy(dbus::GetSystemBusConnection(),
                         kCashewServiceName,
                         kCashewServicePath,
                         kCashewServiceInterface);

  ::GType g_type_map = ::dbus_g_type_get_map("GHashTable",
                                             G_TYPE_STRING,
                                             G_TYPE_VALUE);
  ::GType g_type_map_array = ::dbus_g_type_get_collection("GPtrArray",
                                                          g_type_map);
  ::dbus_g_proxy_add_signal(proxy_.gproxy(), kMonitorDataPlanUpdate,
                            G_TYPE_STRING,
                            g_type_map_array,
                            G_TYPE_INVALID);
  ::dbus_g_proxy_connect_signal(proxy_.gproxy(), kMonitorDataPlanUpdate,
                                G_CALLBACK(&DataPlanUpdateHandler::Run),
                                this,
                                NULL);

  }
  ~DataPlanUpdateHandler() {
    ::dbus_g_proxy_disconnect_signal(proxy_.gproxy(),
                                     kMonitorDataPlanUpdate,
                                     G_CALLBACK(&DataPlanUpdateHandler::Run),
                                     this);
  }

  static void Run(::DBusGProxy*,
                  const char* modem_service_path,
                  GPtrArray* properties_array,
                  void* object) {
    DCHECK(object);
    if (!modem_service_path || !properties_array) {
        return;
    }
    DataPlanUpdateHandler* self =
        static_cast<DataPlanUpdateHandler*>(object);

    CellularDataPlanList * data_plan_list =
        ParseCellularDataPlanList(properties_array);

    if (data_plan_list != NULL) {
      // NOTE: callback_ needs to copy 'data_plan_list'.
      self->callback_(self->object_, modem_service_path, data_plan_list);
      ChromeOSFreeCellularDataPlanList(data_plan_list);
    }
  }

 private:
  MonitorDataPlanCallback callback_;
  void* object_;
  dbus::Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(DataPlanUpdateHandler);
};


extern "C"
DataPlanUpdateMonitor ChromeOSMonitorCellularDataPlan(
    MonitorDataPlanCallback callback,
    void* object) {
  RegisterMarshallers();

  DataPlanUpdateMonitor monitor =
      new DataPlanUpdateHandler(callback, object);

  return monitor;
}

extern "C"
void ChromeOSDisconnectDataPlanUpdateMonitor(
    DataPlanUpdateMonitor connection) {
  delete connection;
}

extern "C"
void ChromeOSRequestCellularDataPlanUpdate(
    const char* modem_service_path) {
  dbus::Proxy proxy(dbus::GetSystemBusConnection(),
                    kCashewServiceName,
                    kCashewServicePath,
                    kCashewServiceInterface);
  dbus_g_proxy_call_no_reply(proxy.gproxy(),
                             kRequestDataPlanFunction,
                             // Inputs:
                             G_TYPE_STRING,
                             modem_service_path,
                             G_TYPE_INVALID);
}

extern "C"
CellularDataPlanList* ChromeOSRetrieveCellularDataPlans(
    const char* modem_service_path) {

  if (!modem_service_path)
    return NULL;

  dbus::Proxy proxy(dbus::GetSystemBusConnection(),
                    kCashewServiceName,
                    kCashewServicePath,
                    kCashewServiceInterface);

  glib::ScopedError error;
  // NOTE: We can't use ScopedPtrArray for GHashTable ptrs because it assumes
  // that the GPtrArray it wraps doesn't have a custom GDestroyNotify function
  // and so performs an unwanted g_free call on each ptr before calling
  // g_ptr_array_free.
  GPtrArray* properties_array = NULL;

  ::GType g_type_map = ::dbus_g_type_get_map("GHashTable",
                                             G_TYPE_STRING,
                                             G_TYPE_VALUE);
  ::GType g_type_map_array = ::dbus_g_type_get_collection("GPtrArray",
                                                          g_type_map);
  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           kRetrieveDataPlanFunction,
                           &Resetter(&error).lvalue(),
                           // Inputs:
                           G_TYPE_STRING,
                           modem_service_path,
                           G_TYPE_INVALID,
                           // Outputs:
                           g_type_map_array,
                           &properties_array,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << "RetrieveDataPlans on path '" << proxy.path() << "' failed: "
                 << (error->message ? error->message : "Unknown Error.");
    DCHECK(!properties_array);
    return NULL;
  }
  DCHECK(properties_array);

  CellularDataPlanList *data_plan_list =
      ParseCellularDataPlanList(properties_array);
  g_ptr_array_unref(properties_array);
  return data_plan_list;
}

}  // namespace chromeos
