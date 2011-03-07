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
#include "base/scoped_vector.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chromeos/dbus/dbus.h"  // NOLINT
#include "chromeos/glib/object.h"  // NOLINT
#include "chromeos/string.h"

// TODO(rtc): Unittest this code as soon as the tree is refactored.
namespace chromeos {  // NOLINT

namespace { // NOLINT

// Flimflam D-Bus service identifiers.
static const char* kFlimflamManagerInterface = "org.chromium.flimflam.Manager";
static const char* kFlimflamServiceInterface = "org.chromium.flimflam.Service";
static const char* kFlimflamServiceName = "org.chromium.flimflam";
static const char* kFlimflamIPConfigInterface = "org.chromium.flimflam.IPConfig";
static const char* kFlimflamDeviceInterface = "org.chromium.flimflam.Device";
static const char* kFlimflamProfileInterface = "org.chromium.flimflam.Profile";
static const char* kFlimflamNetworkInterface = "org.chromium.flimflam.Network";

// Flimflam function names.
static const char* kGetPropertiesFunction = "GetProperties";
static const char* kSetPropertyFunction = "SetProperty";
static const char* kClearPropertyFunction = "ClearProperty";
static const char* kConnectFunction = "Connect";
static const char* kDisconnectFunction = "Disconnect";
static const char* kRequestScanFunction = "RequestScan";
static const char* kGetWifiServiceFunction = "GetWifiService";
static const char* kEnableTechnologyFunction = "EnableTechnology";
static const char* kDisableTechnologyFunction = "DisableTechnology";
static const char* kAddIPConfigFunction = "AddIPConfig";
static const char* kRemoveConfigFunction = "Remove";
static const char* kGetEntryFunction = "GetEntry";
static const char* kDeleteEntryFunction = "DeleteEntry";
static const char* kActivateCellularModemFunction = "ActivateCellularModem";

// Flimflam property names.
static const char* kSecurityProperty = "Security";
static const char* kPassphraseProperty = "Passphrase";
static const char* kIdentityProperty = "Identity";
static const char* kCertPathProperty = "CertPath"; // DEPRECATED
static const char* kOfflineModeProperty = "OfflineMode";
static const char* kSignalStrengthProperty = "Strength";
static const char* kNameProperty = "Name";
static const char* kTypeProperty = "Type";
static const char* kUnknownString = "UNKNOWN";
static const char* kAutoConnectProperty = "AutoConnect";
static const char* kModeProperty = "Mode";
static const char* kActiveProfileProperty = "ActiveProfile";
static const char* kSSIDProperty = "SSID";
static const char* kDevicesProperty = "Devices";
static const char* kNetworksProperty = "Networks";
static const char* kConnectedProperty = "Connected";
static const char* kWifiChannelProperty = "WiFi.Channel";
static const char* kScanIntervalProperty = "ScanInterval";
static const char* kPoweredProperty = "Powered";

// Flimflam device info property names.
static const char* kIPConfigsProperty = "IPConfigs";
static const char* kCertpathSettingsPrefix = "SETTINGS:";

// Flimflam EAP service properties
static const char* kEAPEAPProperty = "EAP.EAP";
static const char* kEAPClientCertProperty = "EAP.ClientCert";
static const char* kEAPCertIDProperty = "EAP.CertID";
static const char* kEAPKeyIDProperty = "EAP.KeyID";
static const char* kEAPPINProperty = "EAP.PIN";

// Flimflam monitored properties
static const char* kMonitorPropertyChanged = "PropertyChanged";

// Flimflam type options.
static const char* kTypeWifi = "wifi";

// Flimflam mode options.
static const char* kModeManaged = "managed";

// Cashew D-Bus service identifiers.
static const char* kCashewServiceName = "org.chromium.Cashew";
static const char* kCashewServicePath = "/org/chromium/Cashew";
static const char* kCashewServiceInterface = "org.chromium.Cashew";

// Cashew function names.
static const char* kRequestDataPlanFunction = "RequestDataPlansUpdate";
static const char* kRetrieveDataPlanFunction = "GetDataPlans";

// Flimflam monitored properties
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

static CellularDataPlanType ParseCellularDataPlanType(const std::string& type) {
  if (type == kCellularDataPlanUnlimited)
    return CELLULAR_DATA_PLAN_UNLIMITED;
  if (type == kCellularDataPlanMeteredPaid)
    return CELLULAR_DATA_PLAN_METERED_PAID;
  if (type == kCellularDataPlanMeteredBase)
    return CELLULAR_DATA_PLAN_METERED_BASE;
  return CELLULAR_DATA_PLAN_UNKNOWN;
}

static Value* ConvertGlibValue(const glib::Value* gvalue);

static void AppendListElement(const GValue *gvalue, gpointer user_data) {
  ListValue* list = static_cast<ListValue*>(user_data);
  glib::Value glibvalue(*gvalue);
  Value* value = ConvertGlibValue(&glibvalue);
  list->Append(value);
}

static void AppendDictionaryElement(const GValue *keyvalue,
                                    const GValue *gvalue,
                                    gpointer user_data) {
  DictionaryValue* dict = static_cast<DictionaryValue*>(user_data);
  std::string key(g_value_get_string(gvalue));
  glib::Value glibvalue(*gvalue);
  Value* value = ConvertGlibValue(&glibvalue);
  dict->SetWithoutPathExpansion(key, value);
}

static Value* ConvertGlibValue(const glib::Value* gvalue) {
  if (G_VALUE_HOLDS_STRING(gvalue)) {
    return Value::CreateStringValue(g_value_get_string(gvalue));
  } else if (G_VALUE_HOLDS_BOOLEAN(gvalue)) {
    return Value::CreateBooleanValue(
        static_cast<bool>(g_value_get_boolean(gvalue)));
  } else if (G_VALUE_HOLDS_INT(gvalue)) {
    return Value::CreateIntegerValue(g_value_get_int(gvalue));
  } else if (G_VALUE_HOLDS_UINT(gvalue)) {
    return Value::CreateIntegerValue(
        static_cast<int>(g_value_get_uint(gvalue)));
  } else if (G_VALUE_HOLDS_UCHAR(gvalue)) {
    return Value::CreateIntegerValue(
        static_cast<int>(g_value_get_uchar(gvalue)));
  } else if (G_VALUE_HOLDS(gvalue, DBUS_TYPE_G_OBJECT_PATH)) {
    const char* path = static_cast<const char*>(::g_value_get_boxed(gvalue));
    return Value::CreateStringValue(path);
  } else if (G_VALUE_HOLDS(gvalue, G_TYPE_STRV)) {
    ListValue* list = new ListValue();
    for (GStrv strv = static_cast<GStrv>(::g_value_get_boxed(gvalue));
         *strv != NULL; ++strv) {
      list->Append(Value::CreateStringValue(*strv));
    }
    return list;
  } else if (::dbus_g_type_is_collection(G_VALUE_TYPE(gvalue))) {
    ListValue* list = new ListValue();
    ::dbus_g_type_collection_value_iterate(gvalue, AppendListElement, list);
    return list;
  } else if (::dbus_g_type_is_map(G_VALUE_TYPE(gvalue))) {
    DictionaryValue* dict = new DictionaryValue();
    ::dbus_g_type_map_value_iterate(gvalue, AppendDictionaryElement, dict);
    return dict;
  } else {
    LOG(ERROR) << "Unrecognized Glib value type: " << G_VALUE_TYPE(gvalue);
    return Value::CreateNullValue();
  }
}

static Value* ConvertGHashTable(GHashTable* ghash) {
  DictionaryValue* dict = new DictionaryValue();
  GHashTableIter iter;
  gpointer gkey, gvalue;
  g_hash_table_iter_init (&iter, ghash);
  while (g_hash_table_iter_next (&iter, &gkey, &gvalue))  {
    std::string key(static_cast<char*>(gkey));
    glib::Value glibvalue(*(static_cast<GValue*>(gvalue)));
    Value* value = ConvertGlibValue(&glibvalue);
    dict->SetWithoutPathExpansion(key, value);
  }
  return dict;
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


}  // namespace

// Register all dbus marshallers once.
// NOTE: This is also called from chromeos_network_deprecated.cc.
void RegisterNetworkMarshallers() {
  static bool registered(false);
  // TODO(rtc/stevenjb): Do these need to be freed?
  if (!registered) {
    ::dbus_g_object_register_marshaller(marshal_VOID__STRING_BOXED,
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
    scoped_ptr<Value> value(ConvertGlibValue(gvalue));
    self->callback_(self->object_, self->path_.c_str(), property, value.get());
  }

  const MonitorConnection& connection() const {
    return connection_;
  }

  void set_connection(const MonitorConnection& c) {
    connection_ = c;
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
                           kFlimflamServiceName,
                           path,
                           kFlimflamIPConfigInterface);
  glib::ScopedHashTable properties;
  if (!GetProperties(config_proxy, &properties))
    return false;
  ParseIPConfigProperties(properties, ipconfig);
  return true;
}

extern "C"
IPConfigStatus* ChromeOSListIPConfigs(const char* device_path) {
  if (!device_path || strlen(device_path) == 0)
    return NULL;

  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy device_proxy(bus,
                           kFlimflamServiceName,
                           device_path,
                           kFlimflamDeviceInterface);

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
                           kFlimflamServiceName,
                           device_path,
                           kFlimflamDeviceInterface);

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
                           kFlimflamServiceName,
                           config->path,
                           kFlimflamIPConfigInterface);
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
                           kFlimflamServiceName,
                           config->path,
                           kFlimflamIPConfigInterface);

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

extern "C"
PropertyChangeMonitor ChromeOSMonitorNetworkManager(
    MonitorPropertyCallback callback,
    void* object) {
  RegisterNetworkMarshallers();
  dbus::Proxy proxy(dbus::GetSystemBusConnection(),
                    kFlimflamServiceName,
                    "/",
                    kFlimflamManagerInterface);
  PropertyChangeMonitor monitor =
      new PropertyChangedHandler(callback, "/", object);
  monitor->set_connection(dbus::Monitor(proxy,
                                        kMonitorPropertyChanged,
                                        &PropertyChangedHandler::Run,
                                        monitor));

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
  RegisterNetworkMarshallers();
  dbus::Proxy service_proxy(dbus::GetSystemBusConnection(),
                            kFlimflamServiceName,
                            service_path,
                            kFlimflamServiceInterface);
  PropertyChangeMonitor monitor =
      new PropertyChangedHandler(callback, service_path, object);
  monitor->set_connection(dbus::Monitor(service_proxy,
                                        kMonitorPropertyChanged,
                                        &PropertyChangedHandler::Run,
                                        monitor));
  return monitor;
}

extern "C"
void ChromeOSDisconnectMonitorService(PropertyChangeMonitor connection) {
  dbus::Disconnect(connection->connection());
  delete connection;
}

extern "C"
bool ChromeOSActivateCellularModem(const char* service_path,
                                   const char* carrier) {
  if (carrier == NULL)
    carrier = "";

  dbus::Proxy service_proxy(dbus::GetSystemBusConnection(),
                            kFlimflamServiceName,
                            service_path,
                            kFlimflamServiceInterface);

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

namespace {
// TODO(stevenjb): This should soon be part of chromeos_network_deprecated.cc

class ScopedPtrGStrFreeV {
 public:
  inline void operator()(char** x) const {
    g_strfreev(x);
  }
};

static const char *map_oldprop_to_newprop(const char* oldprop) {
  if (strcmp(oldprop, "key_id") == 0)
    return kEAPKeyIDProperty;
  if (strcmp(oldprop, "cert_id") == 0)
    return kEAPCertIDProperty;
  if (strcmp(oldprop, "pin") == 0)
    return kEAPPINProperty;

  return NULL;
}

static bool set_certpath_properties(const char* certpath,
                                    dbus::Proxy* service_proxy) {
    // DEPRECATED
    if (::g_str_has_prefix(certpath, kCertpathSettingsPrefix)) {
    // Backwards-compatibility for "CertPath=SETTINGS:key_id=1,cert_id=2,..."
      glib::ScopedError error;
      char **settingsp;
      scoped_ptr_malloc<char *, ScopedPtrGStrFreeV> settings(
          ::g_strsplit_set(certpath +
                           strlen(kCertpathSettingsPrefix), ",=", 0));
      for (settingsp = settings.get(); *settingsp != NULL; settingsp += 2) {
        const char *key = map_oldprop_to_newprop(*settingsp);
        if (key == NULL) {
          LOG(WARNING) << "ConnectToNetwork, unknown key '" << key
                       << "' from certpath ";
          continue;
        }
        glib::Value value(*(settingsp + 1));
        if (!::dbus_g_proxy_call(service_proxy->gproxy(),
                                 kSetPropertyFunction,
                                 &Resetter(&error).lvalue(),
                                 G_TYPE_STRING,
                                 key,
                                 G_TYPE_VALUE,
                                 &value,
                                 G_TYPE_INVALID,
                                 G_TYPE_INVALID)) {
          LOG(WARNING) << "ConnectToNetwork failed on set '" << key
                       << "' (from certpath): "
                       << (error->message ? error->message : "Unknown Error.");
          return false;
        }
      }
      // Presume EAP-TLS if we're here
      glib::Value value("TLS");
      if (!::dbus_g_proxy_call(service_proxy->gproxy(),
                               kSetPropertyFunction,
                               &Resetter(&error).lvalue(),
                               G_TYPE_STRING,
                               kEAPEAPProperty,
                               G_TYPE_VALUE,
                               &value,
                               G_TYPE_INVALID,
                               G_TYPE_INVALID)) {
        LOG(WARNING) << "ConnectToNetwork failed on set EAP type'"
                     << "' (from certpath): "
                     << (error->message ? error->message : "Unknown Error.");
        return false;
      }
    } else {
      // Backwards-compatibility for "CertPath=/path/to/cert.pem"
      glib::Value value_certpath(certpath);
      glib::ScopedError error;
      if (!::dbus_g_proxy_call(service_proxy->gproxy(),
                               kSetPropertyFunction,
                               &Resetter(&error).lvalue(),
                               G_TYPE_STRING,
                               kEAPClientCertProperty,
                               G_TYPE_VALUE,
                               &value_certpath,
                               G_TYPE_INVALID,
                               G_TYPE_INVALID)) {
        LOG(WARNING) << "ConnectToNetwork failed on set certpath: "
                     << (error->message ? error->message : "Unknown Error.");
        return false;
      }
    }
    return true;
}



}  // namespace


extern "C"
bool ChromeOSConnectToNetworkWithCertInfo(const char* service_path,
                                          const char* passphrase,
                                          const char* identity,
                                          const char* certpath) {
  dbus::Proxy service_proxy(dbus::GetSystemBusConnection(),
                            kFlimflamServiceName,
                            service_path,
                            kFlimflamServiceInterface);

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
    if (!set_certpath_properties(certpath, &service_proxy))
      return false;
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
                            kFlimflamServiceName,
                            service_path,
                            kFlimflamServiceInterface);

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
                            kFlimflamServiceName,
                            "/",
                            kFlimflamManagerInterface);

  glib::ScopedHashTable properties;
  if (!GetProperties(manager_proxy, &properties)) {
    return false;
  }

  glib::Value profile_val;
  properties.Retrieve(kActiveProfileProperty, &profile_val);
  const gchar* profile_path =
      static_cast<const gchar*>(g_value_get_boxed(&profile_val));

  dbus::Proxy profile_proxy(bus,
                            kFlimflamServiceName,
                            profile_path,
                            kFlimflamProfileInterface);

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
bool ChromeOSSetOfflineMode(bool offline) {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy manager_proxy(bus,
                            kFlimflamServiceName,
                            "/",
                            kFlimflamManagerInterface);

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
                            kFlimflamServiceName,
                            service_path,
                            kFlimflamServiceInterface);

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
                            kFlimflamServiceName,
                            service_path,
                            kFlimflamServiceInterface);

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
                            kFlimflamServiceName,
                            service_path,
                            kFlimflamServiceInterface);

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
                            kFlimflamServiceName,
                            path,
                            kFlimflamNetworkInterface);
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
                              kFlimflamServiceName,
                              "/",
                              kFlimflamManagerInterface);

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
                             kFlimflamServiceName,
                             device_path,
                             kFlimflamDeviceInterface);

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

//////////////////////////////////////////////////////////////////////////////
// Flimflam asynchronous interface code

namespace {

struct FlimflamCallbackData {
  FlimflamCallbackData(const char* interface,
                       const char* service_path) {
    DCHECK(interface);
    interface_name = std::string(interface);
    proxy = new dbus::Proxy(dbus::GetSystemBusConnection(),
                            kFlimflamServiceName,
                            service_path,
                            interface);
  }
  virtual ~FlimflamCallbackData() {
    delete proxy;
  }
  // Owned by the callback, deleteted in the destructor:
  dbus::Proxy* proxy;
  std::string interface_name;  // Store for error reporting.
};

// DBus will always call the Delete function passed to it by
// dbus_g_proxy_begin_call, whether DBus calls the callback or not.
void DeleteFlimflamCallbackData(void* user_data) {
  FlimflamCallbackData* cb_data = static_cast<FlimflamCallbackData*>(user_data);
  delete cb_data;  // virtual destructor.
}

// Generic hander for logging errors from messages with no return value.
void FlimflamNotifyHandleError(DBusGProxy* gproxy,
                               DBusGProxyCall* call_id,
                               void* user_data) {
  glib::ScopedError error;
  if (!::dbus_g_proxy_end_call(gproxy,
                               call_id,
                               &Resetter(&error).lvalue(),
                               G_TYPE_INVALID)) {
    FlimflamCallbackData* cb_data =
        static_cast<FlimflamCallbackData*>(user_data);
    LOG(WARNING) << "DBus Error: " << cb_data->interface_name << ": "
                 << (error->message ? error->message : "Unknown Error.");
  }
}

struct GetPropertiesCallbackData : public FlimflamCallbackData {
  GetPropertiesCallbackData(const char* interface,
                            const char* service_path,
                            const char* cb_path,
                            NetworkPropertiesCallback cb,
                            void* obj) :
      FlimflamCallbackData(interface, service_path),
      callback(cb),
      object(obj) {
    callback_path = NewStringCopy(cb_path);
  }
  virtual ~GetPropertiesCallbackData() {
    delete callback_path;
  }

  // Owned by the caller (i.e. Chrome), do not destroy them:
  NetworkPropertiesCallback callback;
  void* object;
  // Owned by the callback, deleteted in the destructor:
  const char* callback_path;
};

void GetPropertiesNotify(DBusGProxy* gproxy,
                         DBusGProxyCall* call_id,
                         void* user_data) {
  GetPropertiesCallbackData* cb_data =
      static_cast<GetPropertiesCallbackData*>(user_data);
  DCHECK(cb_data);
  glib::ScopedError error;
  glib::ScopedHashTable properties;
  if (!::dbus_g_proxy_end_call(
          gproxy,
          call_id,
          &Resetter(&error).lvalue(),
          ::dbus_g_type_get_map("GHashTable", G_TYPE_STRING, G_TYPE_VALUE),
          &Resetter(&properties).lvalue(),
          G_TYPE_INVALID)) {
    LOG(WARNING) << "GetPropertiesNotify for path: '"
                 << cb_data->callback_path << "' error: "
                 << (error->message ? error->message : "Unknown Error.");
    cb_data->callback(cb_data->object, cb_data->callback_path, NULL);
  } else {
    scoped_ptr<Value> value(ConvertGHashTable(properties.get()));
    cb_data->callback(cb_data->object, cb_data->callback_path, value.get());
  }
}

void GetPropertiesAsync(const char* interface,
                        const char* service_path,
                        NetworkPropertiesCallback callback,
                        void* object) {
  DCHECK(interface && service_path  && callback);
  GetPropertiesCallbackData* cb_data = new GetPropertiesCallbackData(
      interface, service_path, service_path, callback, object);
  DBusGProxyCall* call_id = ::dbus_g_proxy_begin_call(
      cb_data->proxy->gproxy(),
      kGetPropertiesFunction,
      &GetPropertiesNotify,
      cb_data,
      &DeleteFlimflamCallbackData,
      G_TYPE_INVALID);
  if (!call_id) {
    LOG(ERROR) << "NULL call_id for: " << interface << " : " << service_path;
    callback(object, service_path, NULL);
    delete cb_data;
  }
}

void GetEntryAsync(const char* interface,
                   const char* profile_path,
                   const char* entry_path,
                   NetworkPropertiesCallback callback,
                   void* object) {
  DCHECK(interface && profile_path  && entry_path && callback);
  GetPropertiesCallbackData* cb_data = new GetPropertiesCallbackData(
      interface, profile_path, entry_path, callback, object);
  DBusGProxyCall* call_id = ::dbus_g_proxy_begin_call(
      cb_data->proxy->gproxy(),
      kGetEntryFunction,
      &GetPropertiesNotify,
      cb_data,
      &DeleteFlimflamCallbackData,
      G_TYPE_STRING,
      entry_path,
      G_TYPE_INVALID);
  if (!call_id) {
    LOG(ERROR) << "NULL call_id for: "
               << interface << " : " << profile_path << " : " << entry_path;
    callback(object, entry_path, NULL);
    delete cb_data;
  }
}

void GetWifiNotify(DBusGProxy* gproxy,
                   DBusGProxyCall* call_id,
                   void* user_data) {
  GetPropertiesCallbackData* cb_data =
      static_cast<GetPropertiesCallbackData*>(user_data);
  DCHECK(cb_data);
  glib::ScopedError error;
  char* service_path;
  if (!::dbus_g_proxy_end_call(
          gproxy,
          call_id,
          &Resetter(&error).lvalue(),
          DBUS_TYPE_G_OBJECT_PATH,
          &service_path,
          G_TYPE_INVALID)) {
    LOG(WARNING) << "GetWifiNotify for path: '"
                 << cb_data->callback_path << "' error: "
                 << (error->message ? error->message : "Unknown Error.");
    cb_data->callback(cb_data->object, cb_data->callback_path, NULL);
  } else {
    // Now request the properties for the service.
    GetPropertiesAsync(kFlimflamServiceInterface,
                       service_path,
                       cb_data->callback,
                       cb_data->object);
  }
}

struct NetworkActionCallbackData : public FlimflamCallbackData {
  NetworkActionCallbackData(const char* interface,
                            const char* service_path,
                            const char* cb_path,
                            NetworkActionCallback cb,
                            void* obj) :
      FlimflamCallbackData(interface, service_path),
      callback(cb),
      object(obj) {
    callback_path = NewStringCopy(cb_path);
  }
  virtual ~NetworkActionCallbackData() {
    delete callback_path;
  }

  // Owned by the caller (i.e. Chrome), do not destroy them:
  NetworkActionCallback callback;
  void* object;
  // Owned by the callback, deleteted in the destructor:
  const char* callback_path;
};

void NetworkServiceConnectNotify(DBusGProxy* gproxy,
                                 DBusGProxyCall* call_id,
                                 void* user_data) {
  NetworkActionCallbackData* cb_data =
      static_cast<NetworkActionCallbackData*>(user_data);
  DCHECK(cb_data);
  glib::ScopedError error;
  if (!::dbus_g_proxy_end_call(
          gproxy,
          call_id,
          &Resetter(&error).lvalue(),
          G_TYPE_INVALID)) {
    NetworkMethodErrorType etype;
    if (error->domain == DBUS_GERROR &&
        error->code == DBUS_GERROR_REMOTE_EXCEPTION) {
      etype = NETWORK_METHOD_ERROR_REMOTE;
    } else {
      LOG(WARNING) << "NetworkServiceConnectNotify for path: '"
                   << cb_data->callback_path << "' error: "
                   << (error->message ? error->message : "Unknown Error.");
      etype = NETWORK_METHOD_ERROR_LOCAL;
    }
    cb_data->callback(cb_data->object, cb_data->callback_path,
                      etype, error->message);
  } else {
    cb_data->callback(cb_data->object, cb_data->callback_path,
                      NETWORK_METHOD_ERROR_NONE, NULL);
  }
}

void NetworkServiceConnectAsync(
    const char* service_path,
    NetworkActionCallback callback,
    void* object) {
  DCHECK(service_path && callback);
  NetworkActionCallbackData* cb_data = new NetworkActionCallbackData(
      kFlimflamServiceInterface, service_path, service_path, callback, object);

  DBusGProxyCall* call_id = ::dbus_g_proxy_begin_call(
      cb_data->proxy->gproxy(),
      kConnectFunction,
      &NetworkServiceConnectNotify,
      cb_data,
      &DeleteFlimflamCallbackData,
      G_TYPE_INVALID);
  if (!call_id) {
    LOG(ERROR) << "NULL call_id for: " << kFlimflamServiceInterface
               << " : " << service_path;
    callback(object, service_path, NETWORK_METHOD_ERROR_LOCAL,
             "dbus: NULL call_id");
    delete cb_data;
  }
}


}  // namespace

extern "C"
void ChromeOSRequestNetworkServiceConnect(
    const char* service_path,
    NetworkActionCallback callback,
    void* object) {
  NetworkServiceConnectAsync(service_path, callback, object);
}


extern "C"
void ChromeOSRequestNetworkManagerInfo(
    NetworkPropertiesCallback callback,
    void* object) {
  GetPropertiesAsync(kFlimflamManagerInterface, "/", callback, object);
}

extern "C"
void ChromeOSRequestNetworkServiceInfo(
    const char* service_path,
    NetworkPropertiesCallback callback,
    void* object) {
  GetPropertiesAsync(kFlimflamServiceInterface, service_path, callback, object);
}

extern "C"
void ChromeOSRequestNetworkDeviceInfo(
    const char* device_path,
    NetworkPropertiesCallback callback,
    void* object) {
  GetPropertiesAsync(kFlimflamDeviceInterface, device_path, callback, object);
}

extern "C"
void ChromeOSRequestNetworkProfile(
    const char* profile_path,
    NetworkPropertiesCallback callback,
    void* object) {
  GetPropertiesAsync(kFlimflamProfileInterface, profile_path, callback, object);
}

extern "C"
void ChromeOSRequestNetworkProfileEntry(
    const char* profile_path,
    const char* entry_service_path,
    NetworkPropertiesCallback callback,
    void* object) {
  GetEntryAsync(kFlimflamProfileInterface, profile_path, entry_service_path,
                callback, object);
}

extern "C"
void ChromeOSRequestHiddenWifiNetwork(
    const char* ssid,
    const char* security,
    NetworkPropertiesCallback callback,
    void* object) {
  DCHECK(ssid && callback);
  GetPropertiesCallbackData* cb_data = new GetPropertiesCallbackData(
      kFlimflamManagerInterface, "/", ssid, callback, object);

  glib::ScopedHashTable scoped_properties =
      glib::ScopedHashTable(::g_hash_table_new_full(
          ::g_str_hash, ::g_str_equal, ::g_free, NULL));

  glib::Value value_mode(kModeManaged);
  glib::Value value_type(kTypeWifi);
  glib::Value value_ssid(ssid);
  glib::Value value_security(security);
  ::GHashTable* properties = scoped_properties.get();
  ::g_hash_table_insert(properties, ::g_strdup(kModeProperty), &value_mode);
  ::g_hash_table_insert(properties, ::g_strdup(kTypeProperty), &value_type);
  ::g_hash_table_insert(properties, ::g_strdup(kSSIDProperty), &value_ssid);
  ::g_hash_table_insert(properties, ::g_strdup(kSecurityProperty),
                        &value_security);

  // flimflam.Manger.GetWifiService() will apply the property changes in
  // |properties| and return a new or existing service to GetWifiNotify.
  // GetWifiNotify will then call GetPropertiesAsync, triggering a second
  // asynchronous call to GetPropertiesNotify which will then call |callback|.
  DBusGProxyCall* call_id = ::dbus_g_proxy_begin_call(
      cb_data->proxy->gproxy(),
      kGetWifiServiceFunction,
      &GetWifiNotify,
      cb_data,
      &DeleteFlimflamCallbackData,
      ::dbus_g_type_get_map("GHashTable", G_TYPE_STRING, G_TYPE_VALUE),
      properties,
      G_TYPE_INVALID);
  if (!call_id) {
    LOG(ERROR) << "NULL call_id for: " << kGetWifiServiceFunction;
    delete cb_data;
    callback(object, ssid, NULL);
  }
}

extern "C"
void ChromeOSRequestNetworkScan(const char* network_type) {
  FlimflamCallbackData* cb_data =
      new FlimflamCallbackData(kFlimflamManagerInterface, "/");
  DBusGProxyCall* call_id = ::dbus_g_proxy_begin_call(
      cb_data->proxy->gproxy(),
      kRequestScanFunction,
      &FlimflamNotifyHandleError,
      cb_data,
      &DeleteFlimflamCallbackData,
      G_TYPE_STRING,
      network_type,
      G_TYPE_INVALID);
  if (!call_id) {
    LOG(ERROR) << "NULL call_id for: " << kRequestScanFunction;
    delete cb_data;
  }
}

extern "C"
void ChromeOSRequestNetworkDeviceEnable(const char* network_type, bool enable) {
  FlimflamCallbackData* cb_data =
      new FlimflamCallbackData(kFlimflamManagerInterface, "/");
  DBusGProxyCall* call_id = ::dbus_g_proxy_begin_call(
      cb_data->proxy->gproxy(),
      enable ? kEnableTechnologyFunction : kDisableTechnologyFunction,
      &FlimflamNotifyHandleError,
      cb_data,
      &DeleteFlimflamCallbackData,
      G_TYPE_STRING,
      network_type,
      G_TYPE_INVALID);
  if (!call_id) {
    LOG(ERROR) << "NULL call_id for: " << kRequestScanFunction;
    delete cb_data;
  }
}

//////////////////////////////////////////////////////////////////////////////

static glib::Value *ConvertToGlibValue(const ::Value* value) {
  switch (value->GetType()) {
    case ::Value::TYPE_BOOLEAN: {
      bool out;
      if (value->GetAsBoolean(&out))
        return new glib::Value(out);
      break;
    }
    case ::Value::TYPE_INTEGER: {
      int out;
      if (value->GetAsInteger(&out)) {
        // Converting to a 32-bit signed int type in particular, since
        // that's what flimflam expects in its DBus API
        return new glib::Value(out);
      break;
      }
    }
    case ::Value::TYPE_STRING: {
      std::string out;
      if (value->GetAsString(&out))
        return new glib::Value(out);
      break;
    }
    default:
      // Other Value types - LIST, NULL, REAL, BINARY, and DICTIONARY -
      // aren't passed through this mechanism, and so we're not going to
      // bother to try converting them.
      // If we get here, it's a programming error, so complain.
      LOG(ERROR) << "Unconverted Value of type: " << value->GetType();
      return NULL;
  }
  LOG(ERROR) << "Value conversion failed, type: " << value->GetType();
  return NULL;
}


extern "C"
void ChromeOSSetNetworkServiceProperty(const char* service_path,
                                       const char* property,
                                       const ::Value* setting) {
  FlimflamCallbackData* cb_data =
      new FlimflamCallbackData(kFlimflamServiceInterface, service_path);

  // DEPRECATED
  // Backwards-compatibility for "CertPath=SETTINGS:key_id=1,cert_id=2,..."
  if (strcmp(property, "CertPath") == 0) {
    std::string certpath;
    if (setting->GetAsString(&certpath)) {
      // Synchronous call for backwards compatibility.
      // TODO(njw): remove once CertPath is deprecated in favor of
      // explicit EAP.* properties.
      set_certpath_properties(certpath.c_str(), cb_data->proxy);
    }
    delete cb_data;
    return;
  }

  // Start the DBus call. FlimflamNotifyHandleError will get called when
  // it completes and log any errors.
  scoped_ptr<glib::Value> gsetting(ConvertToGlibValue(setting));
  DBusGProxyCall* call_id = ::dbus_g_proxy_begin_call(
      cb_data->proxy->gproxy(),
      kSetPropertyFunction,
      &FlimflamNotifyHandleError,
      cb_data,
      &DeleteFlimflamCallbackData,
      G_TYPE_STRING,
      property,
      G_TYPE_VALUE,
      gsetting.get(),
      G_TYPE_INVALID);
  if (!call_id) {
    LOG(ERROR) << "NULL call_id for: " << kRequestScanFunction;
    delete cb_data;
  }
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
  RegisterNetworkMarshallers();

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
  ::dbus_g_proxy_call_no_reply(proxy.gproxy(),
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
