// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_network.h"  // NOLINT

#include <algorithm>
#include <vector>
#include <cstring>

#include "marshal.glibmarshal.h"  // NOLINT
#include "base/string_util.h"
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
static const char* kTypeProperty = "Type";
static const char* kUnknownString = "UNKNOWN";
static const char* kIPConfigsProperty = "IPConfigs";
static const char* kDeviceProperty = "Device";
static const char* kActivationStateProperty = "Cellular.ActivationState";
static const char* kNetworkTechnologyProperty = "Cellular.NetworkTechnology";
static const char* kRoamingStateProperty = "Cellular.RoamingState";
static const char* kFavoriteProperty = "Favorite";
static const char* kAutoConnectProperty = "AutoConnect";
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
static const char* kActivationStateUnknown = "unknown";

// Connman error options.
static const char* kErrorOutOfRange = "out-of-range";
static const char* kErrorPinMissing = "pin-missing";
static const char* kErrorDhcpFailed = "dhcp-failed";
static const char* kErrorConnectFailed = "connect-failed";
static const char* kErrorBadPassphrase = "bad-passphrase";
static const char* kErrorBadWEPKey = "bad-wepkey";

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
  return STATE_UNKNOWN;
}

static NetworkTechnology ParseNetworkTechnology(
    const std::string& technology) {
  if (technology == kNetworkTechnology1Xrtt)
    return NETWORK_TECHNOLOGY_1XRTT;
  else if (technology == kNetworkTechnologyEvdo)
    return NETWORK_TECHNOLOGY_EVDO;
  else if (technology == kNetworkTechnologyGprs)
    return NETWORK_TECHNOLOGY_GPRS;
  else if (technology == kNetworkTechnologyEdge)
    return NETWORK_TECHNOLOGY_EDGE;
  else if (technology == kNetworkTechnologyUmts)
    return NETWORK_TECHNOLOGY_UMTS;
  else if (technology == kNetworkTechnologyHspa)
    return NETWORK_TECHNOLOGY_HSPA;
  else if (technology == kNetworkTechnologyHspaPlus)
    return NETWORK_TECHNOLOGY_HSPA_PLUS;
  else if (technology == kNetworkTechnologyLte)
    return NETWORK_TECHNOLOGY_LTE;
  else if (technology == kNetworkTechnologyLteAdvanced)
    return NETWORK_TECHNOLOGY_LTE_ADVANCED;
  return NETWORK_TECHNOLOGY_UNKNOWN;
}

static NetworkRoamingState ParseRoamingState(
    const std::string& roaming_state) {
  if (roaming_state == kRoamingStateHome)
    return ROAMING_STATE_HOME;
  else if (roaming_state == kRoamingStateRoaming)
    return ROAMING_STATE_ROAMING;
  else if (roaming_state == kRoamingStateUnknown)
    return ROAMING_STATE_UNKNOWN;
  return ROAMING_STATE_UNKNOWN;
}

static ActivationState ParseActivationState(
    const std::string& activation_state) {
  if (activation_state == kActivationStateActivated)
    return ACTIVATION_STATE_ACTIVATED;
  else if (activation_state == kActivationStateActivating)
    return ACTIVATION_STATE_ACTIVATING;
  else if (activation_state == kActivationStateNotActivated)
    return ACTIVATION_STATE_NOT_ACTIVATED;
  else if (activation_state == kActivationStateUnknown)
    return ACTIVATION_STATE_UNKNOWN;
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
  return ERROR_UNKNOWN;
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

  // Network technology
  default_string = kUnknownString;
  properties.Retrieve(kNetworkTechnologyProperty, &default_string);
  info->network_technology = ParseNetworkTechnology(default_string);

  // Roaming state
  default_string = kUnknownString;
  properties.Retrieve(kRoamingStateProperty, &default_string);
  info->roaming_state = ParseRoamingState(default_string);

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

  // AutoConnect
  default_bool = false;
  properties.Retrieve(kAutoConnectProperty, &default_bool);
  info->auto_connect = default_bool;

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
}

// Returns a ServiceInfo object populated with data from a
// given DBus object path.
//
// returns true on success.
bool ParseServiceInfo(const char* path, ServiceInfo *info) {
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

// Deletes all of the heap allocated members of a given ServiceInfo instance.
void DeleteServiceInfoProperties(ServiceInfo info) {
  if (info.service_path)
    delete info.service_path;
  if (info.name)
    delete info.name;
  if (info.passphrase)
    delete info.passphrase;
  if (info.device_path)
    delete info.device_path;
  if (info.identity)
    delete info.identity;
  if (info.cert_path)
    delete info.cert_path;

  info.service_path = NULL;
  info.name = NULL;
  info.passphrase = NULL;
  info.device_path = NULL;
  info.identity = NULL;
  info.cert_path = NULL;
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
  delete system;
}

extern "C"
void ChromeOSFreeServiceInfo(ServiceInfo* info) {
  if (info == NULL)
    return;
  DeleteServiceInfoProperties(*info);
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
                  const glib::Value* value) {
    PropertyChangedHandler* self =
        static_cast<PropertyChangedHandler*>(object);
    self->callback_(self->object_, self->path_.c_str(), property, value);
  }

  MonitorConnection& connection() {
    return connection_;
  }

 private:
  MonitorPropertyCallback callback_;
  std::string path_;
  void* object_;
  MonitorConnection connection_;

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
  delete status;

}

// BEGIN DEPRECATED
extern "C"
MonitorNetworkConnection ChromeOSMonitorNetwork(MonitorNetworkCallback callback,
                                                void* object) {
  // TODO(rtc): Figure out where the best place to init the marshaler is, also
  // it may need to be freed.
  dbus_g_object_register_marshaller(marshal_VOID__STRING_BOXED,
                                    G_TYPE_NONE,
                                    G_TYPE_STRING,
                                    G_TYPE_VALUE,
                                    G_TYPE_INVALID);
  dbus::Proxy proxy(dbus::GetSystemBusConnection(),
                    kConnmanServiceName,
                    "/",
                    kConnmanManagerInterface);
  MonitorNetworkConnection result =
      new ManagerPropertyChangedHandler(callback, object);
  result->connection() = dbus::Monitor(
      proxy, "PropertyChanged", &ManagerPropertyChangedHandler::Run, result);
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
  // TODO(rtc): Figure out where the best place to init the marshaler is, also
  // it may need to be freed.
  dbus_g_object_register_marshaller(marshal_VOID__STRING_BOXED,
                                    G_TYPE_NONE,
                                    G_TYPE_STRING,
                                    G_TYPE_VALUE,
                                    G_TYPE_INVALID);
  dbus::Proxy proxy(dbus::GetSystemBusConnection(),
                    kConnmanServiceName,
                    "/",
                    kConnmanManagerInterface);
  PropertyChangeMonitor monitor =
      new PropertyChangedHandler(callback, "/", object);
  monitor->connection() = dbus::Monitor(
      proxy, "PropertyChanged", &PropertyChangedHandler::Run, monitor);
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
  dbus::Proxy service_proxy(dbus::GetSystemBusConnection(),
                            kConnmanServiceName,
                            service_path,
                            kConnmanServiceInterface);
  PropertyChangeMonitor monitor =
      new PropertyChangedHandler(callback, service_path, object);
  monitor->connection() = dbus::Monitor(
      service_proxy, "PropertyChanged", &PropertyChangedHandler::Run, monitor);
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
  glib::Value value_passphrase("");
  glib::Value value_identity("");
  glib::Value value_cert_path("");

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
  if (!ParseServiceInfo(path, info)) {
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
                           &carrier,
                           G_TYPE_INVALID,
                           G_TYPE_INVALID)) {
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
  properties.Retrieve(kAvailableTechnologiesProperty, &available_val);
  gchar** available = static_cast<gchar**>(g_value_get_boxed(&available_val));
  while (*available) {
    system->available_technologies |= 1 << ParseType(*available);
    available++;
  }

  // EnabledTechnologies
  system->enabled_technologies = 0;
  glib::Value enabled_val;
  properties.Retrieve(kEnabledTechnologiesProperty, &enabled_val);
  gchar** enabled = static_cast<gchar**>(g_value_get_boxed(&enabled_val));
  while (*enabled) {
    system->enabled_technologies |= 1 << ParseType(*enabled);
    enabled++;
  }

  // ConnectedTechnologies
  system->connected_technologies = 0;
  glib::Value connected_val;
  properties.Retrieve(kConnectedTechnologiesProperty, &connected_val);
  gchar** connected = static_cast<gchar**>(g_value_get_boxed(&connected_val));
  while (*connected) {
    system->connected_technologies |= 1 << ParseType(*connected);
    connected++;
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
  properties.Retrieve(kServicesProperty, &services_val);
  GPtrArray* services =
      static_cast<GPtrArray*>(g_value_get_boxed(&services_val));
  std::vector<ServiceInfo> buffer;
  const char* path = NULL;
  for (size_t i = 0; i < services->len; i++) {
    path = static_cast<const char*>(g_ptr_array_index(services, i));
    ServiceInfo info = {};
    if (!ParseServiceInfo(path, &info))
      continue;
    buffer.push_back(info);
  }
  system->service_size = buffer.size();
  if (system->service_size == 0) {
    system->services = NULL;
  } else {
    system->services = new ServiceInfo[system->service_size];
  }
  std::copy(buffer.begin(), buffer.end(), system->services);

  // Profile
  glib::Value profile_val;
  properties.Retrieve(kActiveProfileProperty, &profile_val);
  const gchar* profile_path =
      static_cast<const gchar*>(g_value_get_boxed(&profile_val));

  dbus::Proxy profile_proxy(bus,
                            kConnmanServiceName,
                            profile_path,
                            kConnmanProfileInterface);

  glib::ScopedHashTable profile_properties;
  std::vector<ServiceInfo> remembered_buffer;
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
          remembered_buffer.push_back(info);
        }
        entries++;
      }
    }
  }
  system->remembered_service_size = remembered_buffer.size();
  if (system->remembered_service_size == 0) {
    system->remembered_services = NULL;
  } else {
    system->remembered_services =
        new ServiceInfo[system->remembered_service_size];
  }
  std::copy(remembered_buffer.begin(), remembered_buffer.end(),
            system->remembered_services);

  // Set the runtime size of the ServiceInfo struct for GetServiceInfo() calls.
  system->service_info_size = sizeof(ServiceInfo);

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
  return SetServiceStringProperty(service_path, kPassphraseProperty,
                                  passphrase);
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

}  // namespace chromeos
