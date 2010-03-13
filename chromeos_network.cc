// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_network.h"  // NOLINT

#include <algorithm>
#include <vector>
#include <cstring>

#include "marshal.h"  // NOLINT
#include "base/string_util.h"
#include "chromeos/dbus/dbus.h"  // NOLINT
#include "chromeos/glib/object.h"  // NOLINT
#include "chromeos/string.h"

// TODO(rtc): Unittest this code as soon as the tree is refactored.
namespace chromeos {  // NOLINT

namespace { // NOLINT

// Connman D-Bus service identifiers.
static const char* kConnmanManagerInterface = "org.moblin.connman.Manager";
static const char* kConnmanServiceInterface = "org.moblin.connman.Service";
static const char* kConnmanServiceName = "org.moblin.connman";
static const char* kConnmanIPConfigInterface = "org.moblin.connman.IPConfig";
static const char* kConnmanDeviceInterface = "org.moblin.connman.Device";

// Connman function names.
static const char* kGetPropertiesFunction = "GetProperties";
static const char* kSetPropertyFunction = "SetProperty";
static const char* kConnectFunction = "Connect";
static const char* kGetWifiServiceFunction = "GetWifiService";
static const char* kEnableTechnologyFunction = "EnableTechnology";
static const char* kDisableTechnologyFunction = "DisableTechnology";
static const char* kAddIPConfigFunction = "AddIPConfig";
static const char* kRemoveConfigFunction = "Remove";

// Connman property names.
static const char* kSecurityProperty = "Security";
static const char* kPassphraseProperty = "Passphrase";
static const char* kPassphraseRequiredProperty = "PassphraseRequired";
static const char* kServicesProperty = "Services";
static const char* kEnabledTechnologiesProperty = "EnabledTechnologies";
static const char* kOfflineModeProperty = "OfflineMode";
static const char* kSignalStrengthProperty = "Strength";
static const char* kNameProperty = "Name";
static const char* kStateProperty = "State";
static const char* kTypeProperty = "Type";
static const char* kUnknownString = "UNKNOWN";
static const char* kIPConfigsProperty = "IPConfigs";
static const char* kDeviceProperty = "Device";
static const char* kFavoriteProperty = "Favorite";
static const char* kAutoConnectProperty = "AutoConnect";
static const char* kModeProperty = "Mode";
static const char* kErrorProperty = "Error";

// Connman type options.
static const char* kTypeEthernet = "ethernet";
static const char* kTypeWifi = "wifi";
static const char* kTypeWimax = "wimax";
static const char* kTypeBluetooth = "bluetooth";
static const char* kTypeCellular = "cellular";

// Connman mode options.
static const char* kModeManaged = "managed";
static const char* kModeAdhoc = "adhoc";

// Connman security options.
static const char* kSecurityWpa = "wpa";
static const char* kSecurityWep = "wep";
static const char* kSecurityRsn = "rsn";
static const char* kSecurityNone = "none";

// Connman state options.
static const char* kStateIdle = "idle";
static const char* kStateCarrier = "carrier";
static const char* kStateAssociation = "association";
static const char* kStateConfiguration = "configuration";
static const char* kStateReady = "ready";
static const char* kStateDisconnect = "disconnect";
static const char* kStateFailure = "failure";

// Connman error options.
static const char* kErrorOutOfRange = "out-of-range";
static const char* kErrorPinMissing = "pin-missing";
static const char* kErrorDhcpFailed = "dhcp-failed";
static const char* kErrorConnectFailed = "connect-failed";

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
  return kUnknownString;
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

static ConnectionError ParseError(const std::string& error) {
  if (error == kErrorOutOfRange)
    return ERROR_OUT_OF_RANGE;
  if (error == kErrorPinMissing)
    return ERROR_PIN_MISSING;
  if (error == kErrorDhcpFailed)
    return ERROR_DHCP_FAILED;
  if (error == kErrorConnectFailed)
    return ERROR_CONNECT_FAILED;
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
    LOG(WARNING) << "GetProperties failed: "
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
    info->device_path = NewStringCopy(kUnknownString);
  }
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

// Creates a new ServiceStatus instance populated with the contents from
// a vector of ServiceInfo.
ServiceStatus* CopyFromVector(const std::vector<ServiceInfo>& services) {
  ServiceStatus* result = new ServiceStatus();
  if (services.size() == 0) {
    result->services = NULL;
  } else {
    result->services = new ServiceInfo[services.size()];
  }
  result->size = services.size();
  std::copy(services.begin(), services.end(), result->services);
  return result;
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

  info.service_path = NULL;
  info.name = NULL;
  info.passphrase = NULL;
  info.device_path = NULL;
}

ServiceStatus* GetServiceStatus(const GPtrArray* array) {
  std::vector<ServiceInfo> buffer;
  // TODO(rtc/seanparent): Think about using std::transform instead of this
  // loop. For now I think the loop will be generally more readable than
  // std::transform().
  const char* path = NULL;
  for (size_t i = 0; i < array->len; i++) {
    path = static_cast<const char*>(g_ptr_array_index(array, i));
    ServiceInfo info = {};
    if (!ParseServiceInfo(path, &info))
      continue;
    buffer.push_back(info);
  }
  return CopyFromVector(buffer);
}

}  // namespace

extern "C"
void ChromeOSFreeServiceStatus(ServiceStatus* status) {
  if (status == NULL)
    return;
  std::for_each(status->services,
                status->services + status->size,
                &DeleteServiceInfoProperties);
  delete [] status->services;
  delete status;
}

extern "C"
void ChromeOSFreeServiceInfo(ServiceInfo* info) {
  if (info == NULL)
    return;
  DeleteServiceInfoProperties(*info);
}

class OpaqueNetworkStatusConnection {
 public:
  typedef dbus::MonitorConnection<void(const char*, const glib::Value*)>*
      ConnectionType;

  OpaqueNetworkStatusConnection(const dbus::Proxy& proxy,
                                const NetworkMonitor& monitor,
                                void* object)
     : proxy_(proxy),
       monitor_(monitor),
       object_(object),
       connection_(NULL) {
  }

  static void Run(void* object,
                  const char* property,
                  const glib::Value* value) {
    OpaqueNetworkStatusConnection* self =
        static_cast<OpaqueNetworkStatusConnection*>(object);
    if (strcmp("Services", property) != 0) {
      return;
    }

    ::GPtrArray *services =
        static_cast< ::GPtrArray *>(::g_value_get_boxed(value));
    if (services->len == 0) {
      LOG(INFO) << "Signal sent without path.";
      return;
    }
    ServiceStatus* status = GetServiceStatus(services);
    self->monitor_(self->object_, *status);
    ChromeOSFreeServiceStatus(status);
  }

  ConnectionType& connection() {
    return connection_;
  }

 private:
  dbus::Proxy proxy_;
  NetworkMonitor monitor_;
  void* object_;
  ConnectionType connection_;
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
  result->size = ips_value->len;
  if (result->size == 0) {
    result->ips = NULL;
  } else {
    result->ips = new IPConfig[result->size];
    for (size_t i = 0; i < ips_value->len; i++) {
      const char* path =
          static_cast<const char*>(g_ptr_array_index(ips_value, i));
      ParseIPConfig(path, &result->ips[i]);
    }
  }
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
  delete config.path;
  delete config.address;
  delete config.broadcast;
  delete config.netmask;
  delete config.gateway;
  delete config.domainname;
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

extern "C"
NetworkStatusConnection ChromeOSMonitorNetworkStatus(NetworkMonitor monitor,
                                                     void* object) {
  // TODO(rtc): Figure out where the best place to init the marshaler is, also
  // it may need to be freed.
  dbus_g_object_register_marshaller(marshal_VOID__STRING_BOXED,
                                    G_TYPE_NONE,
                                    G_TYPE_STRING,
                                    G_TYPE_VALUE,
                                    G_TYPE_INVALID);
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy proxy(bus, kConnmanServiceName, "/", kConnmanManagerInterface);
  NetworkStatusConnection result =
      new OpaqueNetworkStatusConnection(proxy, monitor, object);
  result->connection() = dbus::Monitor(
      proxy, "PropertyChanged", &OpaqueNetworkStatusConnection::Run, result);
  return result;
}

extern "C"
void ChromeOSDisconnectNetworkStatus(NetworkStatusConnection connection) {
  dbus::Disconnect(connection->connection());
  delete connection;
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

  glib::Value value_mode("managed");
  glib::Value value_type("wifi");
  glib::Value value_ssid(ssid);
  if (security == SECURITY_UNKNOWN)
    security = SECURITY_RSN;
  glib::Value value_security(SecurityToString(security));
  glib::Value value_passphrase("");

  ::GHashTable* properties = scoped_properties.get();
  ::g_hash_table_insert(properties, ::g_strdup("Mode"), &value_mode);
  ::g_hash_table_insert(properties, ::g_strdup("Type"), &value_type);
  ::g_hash_table_insert(properties, ::g_strdup("SSID"), &value_ssid);
  ::g_hash_table_insert(properties, ::g_strdup("Security"), &value_security);
  ::g_hash_table_insert(properties, ::g_strdup("Passphrase"),
                        &value_passphrase);

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
bool ChromeOSConnectToNetwork(const char* service_path,
                              const char* passphrase) {
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
ServiceStatus* ChromeOSGetAvailableNetworks() {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy manager_proxy(bus,
                            kConnmanServiceName,
                            "/",
                            kConnmanManagerInterface);

  glib::ScopedHashTable properties;
  if (!GetProperties(manager_proxy, &properties)) {
    return NULL;
  }

  GHashTable* table = properties.get();
  gpointer ptr = g_hash_table_lookup(table, kServicesProperty);
  if (ptr == NULL)
    return NULL;

  std::vector<ServiceInfo> buffer;
  // TODO(seanparent): See if there is a cleaner way to implement this.
  GPtrArray* service_value =
      static_cast<GPtrArray*>(g_value_get_boxed(static_cast<GValue*>(ptr)));
  return GetServiceStatus(service_value);
}

extern "C"
int ChromeOSGetEnabledNetworkDevices() {
  int devices = 0;

  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy manager_proxy(bus,
                            kConnmanServiceName,
                            "/",
                            kConnmanManagerInterface);

  glib::ScopedHashTable properties;
  if (!GetProperties(manager_proxy, &properties)) {
    return devices;
  }

  bool offline_mode = false;
  properties.Retrieve(kOfflineModeProperty, &offline_mode);
  if (offline_mode)
    return -1;

  GHashTable* table = properties.get();
  gpointer ptr = g_hash_table_lookup(table, kEnabledTechnologiesProperty);
  if (ptr == NULL)
    return devices;

  gchar** value =
      static_cast<gchar**>(g_value_get_boxed(static_cast<GValue*>(ptr)));
  while (*value) {
    // Bitwise OR with a bit left-shifted by the enum ConnectionType value.
    devices |= 1 << ParseType(*value);
    value++;
  }
  return devices;
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

}  // namespace chromeos
