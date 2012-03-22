// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdio>
#include <dlfcn.h>
#include <string.h>
#include <vector>

#include "base/basictypes.h"
#include "chromeos_cros_api.h" // NOLINT
#include "chromeos_network.h"  // NOLINT

namespace chromeos {  // NOLINT //

namespace {
// If set, points to a function provided by Chrome to add a histogram.
LibcrosTimeHistogramFunc addHistogram = NULL;
// Pointer to libcros (from ::dlopen call).
void* dll_handle = NULL;
}

class TimerInst {
 public:
  TimerInst(const char* name) {
    if (addHistogram) {
      name_ = std::string("Cros.") + name;
      start_ = base::TimeTicks::Now();
    }
  }
  ~TimerInst() {
    if (addHistogram) {
      base::TimeDelta delta = base::TimeTicks::Now() - start_;
      addHistogram(name_.c_str(), delta);
    }
  }

 private:
  std::string name_;
  base::TimeTicks start_;
};

// Declare Function. These macros are used to define the imported functions
// from libcros. They will declare the proper type and define an exported
// variable to be used to call the function.
// |name| is the name of the function.
// |ret| is the return type.
// |arg[1-5]| are the types are the arguments.
// These are compile time declarations only.
// INIT_FUNC(name) needs to be called at runtime.
#define DECL_FUNC_0(name, ret)                                          \
  typedef ret (*name##Type)();                                          \
  name##Type name = 0;                                                  \
  ret WrapChromeOS##name() {                                            \
    static name##Type func = name##Type(::dlsym(dll_handle, "ChromeOS"#name)); \
    TimerInst timer(#name); return func(); }

#define DECL_FUNC_1(name, ret, arg1)                                    \
  typedef ret (*name##Type)(arg1);                                      \
  name##Type name = 0;                                                  \
  ret WrapChromeOS##name(arg1 a1) {                                     \
    static name##Type func = name##Type(::dlsym(dll_handle, "ChromeOS"#name)); \
    TimerInst timer(#name); return func(a1); }

#define DECL_FUNC_2(name, ret, arg1, arg2)                              \
  typedef ret (*name##Type)(arg1, arg2);                                \
  name##Type name = 0;                                                  \
  ret WrapChromeOS##name(arg1 a1, arg2 a2) {                            \
    static name##Type func = name##Type(::dlsym(dll_handle, "ChromeOS"#name)); \
    TimerInst timer(#name); return func(a1, a2); }

#define DECL_FUNC_3(name, ret, arg1, arg2, arg3)                        \
  typedef ret (*name##Type)(arg1, arg2, arg3);                          \
  name##Type name = 0;                                                  \
  ret WrapChromeOS##name(arg1 a1, arg2 a2, arg3 a3) {                   \
    static name##Type func = name##Type(::dlsym(dll_handle, "ChromeOS"#name)); \
    TimerInst timer(#name); return func(a1, a2, a3); }

#define DECL_FUNC_4(name, ret, arg1, arg2, arg3, arg4)                  \
  typedef ret (*name##Type)(arg1, arg2, arg3, arg4);                    \
  name##Type name = 0;                                                  \
  ret WrapChromeOS##name(arg1 a1, arg2 a2, arg3 a3, arg4 a4) {          \
    static name##Type func = name##Type(::dlsym(dll_handle, "ChromeOS"#name)); \
    TimerInst timer(#name); return func(a1, a2, a3, a4); }

#define DECL_FUNC_5(name, ret, arg1, arg2, arg3, arg4, arg5)            \
  typedef ret (*name##Type)(arg1, arg2, arg3, arg4, arg5);              \
  name##Type name = 0;                                                  \
  extern "C" ret ChromeOS##name(arg1, arg2, arg3, arg4, arg5);          \
  ret WrapChromeOS##name(                                               \
      arg1 a1, arg2 a2, arg3 a3, arg4 a4, arg5 a5) {                    \
    static name##Type func = name##Type(::dlsym(dll_handle, "ChromeOS"#name)); \
    TimerInst timer(#name); return func(a1, a2, a3, a4, a5); }

#define DECL_FUNC_6(name, ret, arg1, arg2, arg3, arg4, arg5, arg6)      \
  typedef ret (*name##Type)(arg1, arg2, arg3, arg4, arg5, arg6);        \
  name##Type name = 0;                                                  \
  extern "C" ret ChromeOS##name(arg1, arg2, arg3, arg4, arg5, arg6);    \
  ret WrapChromeOS##name(                                               \
      arg1 a1, arg2 a2, arg3 a3, arg4 a4, arg5 a5, arg6 a6) {           \
    static name##Type func = name##Type(::dlsym(dll_handle, "ChromeOS"#name)); \
    TimerInst timer(#name); return func(a1, a2, a3, a4, a5, a6); }

// Version
DECL_FUNC_1(CrosVersionCheck, bool, chromeos::CrosAPIVersion);

// Networking
DECL_FUNC_2(ActivateCellularModem, bool, const char*, const char*);
DECL_FUNC_3(SetNetworkServicePropertyGValue, void, const char*, const char*,
            const GValue*);
DECL_FUNC_2(ClearNetworkServiceProperty, void, const char*, const char*);
DECL_FUNC_3(SetNetworkDevicePropertyGValue, void, const char*, const char*,
            const GValue*);
DECL_FUNC_3(SetNetworkIPConfigPropertyGValue, void, const char*, const char*,
            const GValue*);
DECL_FUNC_2(SetNetworkManagerPropertyGValue, void, const char*, const GValue*);
DECL_FUNC_2(DeleteServiceFromProfile, void, const char*, const char*);
DECL_FUNC_1(RequestCellularDataPlanUpdate, void, const char*);
DECL_FUNC_2(MonitorNetworkManagerProperties, NetworkPropertiesMonitor,
            MonitorPropertyGValueCallback, void*);
DECL_FUNC_3(MonitorNetworkServiceProperties, NetworkPropertiesMonitor,
            MonitorPropertyGValueCallback, const char*, void*);
DECL_FUNC_3(MonitorNetworkDeviceProperties, NetworkPropertiesMonitor,
            MonitorPropertyGValueCallback, const char*, void*);
DECL_FUNC_1(DisconnectNetworkPropertiesMonitor, void, NetworkPropertiesMonitor);
DECL_FUNC_2(MonitorCellularDataPlan, DataPlanUpdateMonitor,
            MonitorDataPlanCallback, void*);
DECL_FUNC_1(DisconnectDataPlanUpdateMonitor, void, DataPlanUpdateMonitor);
DECL_FUNC_3(MonitorSMS, SMSMonitor, const char*, MonitorSMSCallback, void*);
DECL_FUNC_1(DisconnectSMSMonitor, void, SMSMonitor);
DECL_FUNC_3(RequestNetworkServiceConnect, void, const char*,
            NetworkActionCallback, void *);
DECL_FUNC_2(RequestNetworkManagerProperties, void,
            NetworkPropertiesGValueCallback, void*);
DECL_FUNC_3(RequestNetworkServiceProperties, void, const char*,
            NetworkPropertiesGValueCallback, void*);
DECL_FUNC_3(RequestNetworkDeviceProperties, void, const char*,
            NetworkPropertiesGValueCallback, void*);
DECL_FUNC_3(RequestNetworkProfileProperties, void, const char*,
            NetworkPropertiesGValueCallback, void*);
DECL_FUNC_4(RequestNetworkProfileEntryProperties, void, const char*,
            const char*, NetworkPropertiesGValueCallback, void*);
DECL_FUNC_4(RequestHiddenWifiNetworkProperties, void, const char*, const char*,
            NetworkPropertiesGValueCallback, void*);
DECL_FUNC_5(RequestVirtualNetworkProperties, void, const char*, const char*,
            const char*, NetworkPropertiesGValueCallback, void*);
DECL_FUNC_1(RequestRemoveNetworkService, void, const char*);
DECL_FUNC_1(RequestNetworkServiceDisconnect, void, const char*);
DECL_FUNC_1(RequestNetworkScan, void, const char*);
DECL_FUNC_2(RequestNetworkDeviceEnable, void, const char*, bool);
DECL_FUNC_5(RequestRequirePin, void, const char*, const char*, bool,
            NetworkActionCallback, void*);
DECL_FUNC_4(RequestEnterPin, void, const char*, const char*,
            NetworkActionCallback, void*);
DECL_FUNC_5(RequestUnblockPin, void, const char*, const char*, const char*,
            NetworkActionCallback, void*);
DECL_FUNC_5(RequestChangePin, void, const char*, const char*, const char*,
            NetworkActionCallback, void*);
DECL_FUNC_1(ProposeScan, void, const char*);
DECL_FUNC_4(RequestCellularRegister, void, const char*, const char*,
            NetworkActionCallback, void*);
DECL_FUNC_1(SetOfflineMode, bool, bool);
DECL_FUNC_1(ListIPConfigs, IPConfigStatus*, const char*);
DECL_FUNC_2(AddIPConfig, bool, const char*, IPConfigType);
DECL_FUNC_1(RemoveIPConfig, bool, IPConfig*);
DECL_FUNC_1(FreeIPConfigStatus, void, IPConfigStatus*);
DECL_FUNC_0(GetDeviceNetworkList, DeviceNetworkList*);
DECL_FUNC_1(FreeDeviceNetworkList, void, DeviceNetworkList*);
DECL_FUNC_4(ConfigureService, void, const char*, const GHashTable*,
            NetworkActionCallback, void*);

char const * const kCrosDefaultPath = "/opt/google/chrome/chromeos/libcros.so";

// Initializes the variable by looking up the function by |name|.
// This macro uses the variable 'handle' and 'error_string'.
#define INIT_FUNC(name) \
  name = WrapChromeOS##name; \
  if (!::dlsym(dll_handle, "ChromeOS"#name)) { \
    error_string += "Couldn't load: "#name","; \
  }

bool LoadLibcros(const char* path_to_libcros, std::string& error_string) {
  error_string.clear();

  if (!path_to_libcros) {
    error_string = "path_to_libcros can't be NULL";
    return false;
  }

  dll_handle = ::dlopen(path_to_libcros, RTLD_NOW);
  if (dll_handle == NULL) {
    error_string = "Couldn't load libcros from: ";
    error_string += path_to_libcros;
    error_string += " error: ";
    error_string += dlerror();
    return false;
  }

  INIT_FUNC(CrosVersionCheck);
  if (!CrosVersionCheck) {
    // error_string will already be set.
    return false;
  }

  if (!CrosVersionCheck(chromeos::kCrosAPIVersion)) {
    const int buf_size = sizeof(int)*8+1;
    char buf[buf_size];
    typedef int (*VersionFuncType)();

    // These weren't exported from older copies of the library. It's not an
    // error so we don't use INIT_FUNC()
    VersionFuncType GetMinCrosVersion =
        VersionFuncType(::dlsym(dll_handle, "ChromeOSGetMinCrosVersion"));
    VersionFuncType GetCrosVersion =
        VersionFuncType(::dlsym(dll_handle, "ChromeOSGetCrosVersion"));

    error_string = "Incompatible libcros version. Client: ";
    snprintf(buf, buf_size, "%d", chromeos::kCrosAPIVersion);
    error_string += buf;
    if (GetMinCrosVersion && GetCrosVersion) {
      snprintf(buf, buf_size, "%d", GetMinCrosVersion());
      error_string += " Min: ";
      error_string += buf;
      snprintf(buf, buf_size, "%d", GetCrosVersion());
      error_string += " Max: ";
      error_string += buf;
    }
    return false;
  }

  // Networking
  INIT_FUNC(ActivateCellularModem);
  INIT_FUNC(SetNetworkServicePropertyGValue);
  INIT_FUNC(ClearNetworkServiceProperty);
  INIT_FUNC(SetNetworkDevicePropertyGValue);
  INIT_FUNC(SetNetworkIPConfigPropertyGValue);
  INIT_FUNC(SetNetworkManagerPropertyGValue);
  INIT_FUNC(DeleteServiceFromProfile);
  INIT_FUNC(RequestCellularDataPlanUpdate);
  INIT_FUNC(MonitorNetworkManagerProperties);
  INIT_FUNC(MonitorNetworkServiceProperties);
  INIT_FUNC(MonitorNetworkDeviceProperties);
  INIT_FUNC(DisconnectNetworkPropertiesMonitor);
  INIT_FUNC(MonitorCellularDataPlan);
  INIT_FUNC(DisconnectDataPlanUpdateMonitor);
  INIT_FUNC(MonitorSMS);
  INIT_FUNC(DisconnectSMSMonitor);
  INIT_FUNC(RequestNetworkServiceConnect);
  INIT_FUNC(RequestNetworkManagerProperties);
  INIT_FUNC(RequestNetworkServiceProperties);
  INIT_FUNC(RequestNetworkDeviceProperties);
  INIT_FUNC(RequestNetworkProfileProperties);
  INIT_FUNC(RequestNetworkProfileEntryProperties);
  INIT_FUNC(RequestHiddenWifiNetworkProperties);
  INIT_FUNC(RequestVirtualNetworkProperties);
  INIT_FUNC(RequestRemoveNetworkService);
  INIT_FUNC(RequestNetworkServiceDisconnect);
  INIT_FUNC(RequestNetworkScan);
  INIT_FUNC(RequestNetworkDeviceEnable);
  INIT_FUNC(RequestRequirePin);
  INIT_FUNC(RequestEnterPin);
  INIT_FUNC(RequestUnblockPin);
  INIT_FUNC(RequestChangePin);
  INIT_FUNC(ProposeScan);
  INIT_FUNC(RequestCellularRegister);
  INIT_FUNC(SetOfflineMode);
  INIT_FUNC(ListIPConfigs);
  INIT_FUNC(AddIPConfig);
  INIT_FUNC(RemoveIPConfig);
  INIT_FUNC(FreeIPConfigStatus);
  INIT_FUNC(GetDeviceNetworkList);
  INIT_FUNC(FreeDeviceNetworkList);
  INIT_FUNC(ConfigureService);

  return error_string.empty();
}

void SetLibcrosTimeHistogramFunction(LibcrosTimeHistogramFunc func) {
  addHistogram = func;
}

}  // namespace chromeos
