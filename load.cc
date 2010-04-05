// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdio>
#include <dlfcn.h>
#include <string.h>

#include "chromeos_cros_api.h" // NOLINT
#include "chromeos_ime.h"  // NOLINT
#include "chromeos_keyboard.h"  // NOLINT
#include "chromeos_language.h"  // NOLINT
#include "chromeos_login.h"  // NOLINT
#include "chromeos_mount.h"  // NOLINT
#include "chromeos_network.h"  // NOLINT
#include "chromeos_power.h"  // NOLINT
#include "chromeos_synaptics.h"  // NOLINT
#include "chromeos_update.h"  // NOLINT

namespace chromeos {  // NOLINT //

static std::string error_string;

// Declare Function. These macros are used to define the imported functions
// from libcros. They will declare the proper type and define an exported
// variable to be used to call the function.
// |name| is the name of the function.
// |ret| is the return type.
// |arg[1-4]| are the types are the arguments.
// These are compile time declarations only.
// INIT_FUNC(name) needs to be called at runtime.
#define DECL_FUNC_0(name, ret) \
  typedef ret (*name##Type)(); \
  name##Type name = 0;

#define DECL_FUNC_1(name, ret, arg1) \
  typedef ret (*name##Type)(arg1); \
  name##Type name = 0;

#define DECL_FUNC_2(name, ret, arg1, arg2) \
  typedef ret (*name##Type)(arg1, arg2); \
  name##Type name = 0;

#define DECL_FUNC_3(name, ret, arg1, arg2, arg3) \
  typedef ret (*name##Type)(arg1, arg2, arg3); \
  name##Type name = 0;

#define DECL_FUNC_4(name, ret, arg1, arg2, arg3, arg4) \
  typedef ret (*name##Type)(arg1, arg2, arg3, arg4); \
  name##Type name = 0;

// Version
DECL_FUNC_1(CrosVersionCheck, bool, chromeos::CrosAPIVersion);

// Power
DECL_FUNC_2(MonitorPowerStatus, PowerStatusConnection, PowerMonitor, void*);
DECL_FUNC_1(DisconnectPowerStatus, void, PowerStatusConnection);
DECL_FUNC_1(RetrievePowerInformation, bool, PowerInformation*);

// IME
DECL_FUNC_2(MonitorLanguageStatus,
    LanguageStatusConnection*, LanguageStatusMonitorFunctions, void*);
DECL_FUNC_1(DisconnectLanguageStatus, void, LanguageStatusConnection*);
DECL_FUNC_1(GetSupportedLanguages,
    InputLanguageList*, LanguageStatusConnection*);
DECL_FUNC_1(GetActiveLanguages, InputLanguageList*, LanguageStatusConnection*);
DECL_FUNC_3(ChangeLanguage,
    void, LanguageStatusConnection*, LanguageCategory, const char*);
DECL_FUNC_4(SetLanguageActivated,
    bool, LanguageStatusConnection*, LanguageCategory, const char*, bool);
DECL_FUNC_3(SetImePropertyActivated,
    void, LanguageStatusConnection*, const char*, bool);
// DEPRECATED: TODO(satorux): Remove this when it's ready.
DECL_FUNC_3(ActivateLanguage,
    bool, LanguageStatusConnection*, LanguageCategory, const char*);
// DEPRECATED: TODO(satorux): Remove this when it's ready.
DECL_FUNC_3(DeactivateLanguage,
    bool, LanguageStatusConnection*, LanguageCategory, const char*);
// DEPRECATED: TODO(satorux): Remove this when it's ready.
DECL_FUNC_2(ActivateImeProperty, void, LanguageStatusConnection*, const char*);
// DEPRECATED: TODO(satorux): Remove this when it's ready.
DECL_FUNC_2(DeactivateImeProperty,
    void, LanguageStatusConnection*, const char*);
DECL_FUNC_4(GetImeConfig,
    bool,
    LanguageStatusConnection*,
    const char*,
    const char*,
    chromeos::ImeConfigValue*);
DECL_FUNC_4(SetImeConfig,
    bool,
    LanguageStatusConnection*, const char*, const char*, const ImeConfigValue&);
DECL_FUNC_1(LanguageStatusConnectionIsAlive, bool, LanguageStatusConnection*);
DECL_FUNC_2(MonitorImeStatus,
    ImeStatusConnection*, const ImeStatusMonitorFunctions&, void*);
DECL_FUNC_1(DisconnectImeStatus, void, ImeStatusConnection*);
DECL_FUNC_4(NotifyCandidateClicked, void, ImeStatusConnection*, int, int, int);
DECL_FUNC_0(GetCurrentKeyboardLayoutName, std::string);
DECL_FUNC_1(SetCurrentKeyboardLayoutByName, bool, const std::string&);
DECL_FUNC_1(GetKeyboardLayoutPerWindow, bool, bool*);
DECL_FUNC_1(SetKeyboardLayoutPerWindow, bool, bool);

// Mount
DECL_FUNC_2(MonitorMountStatus, MountStatusConnection, MountMonitor, void*);
DECL_FUNC_1(DisconnectMountStatus, void, MountStatusConnection);
DECL_FUNC_0(RetrieveMountInformation, MountStatus*);
DECL_FUNC_1(FreeMountStatus, void, MountStatus*);
DECL_FUNC_1(MountDevicePath, bool, const char*);

// Networking
DECL_FUNC_0(GetSystemInfo, SystemInfo*);
DECL_FUNC_1(RequestScan, void, ConnectionType);
DECL_FUNC_2(GetWifiService, ServiceInfo*, const char*, ConnectionSecurity);
DECL_FUNC_2(ConnectToNetwork, bool, const char*, const char*);
DECL_FUNC_1(FreeSystemInfo, void, SystemInfo*);
DECL_FUNC_1(FreeServiceInfo, void, ServiceInfo*);
DECL_FUNC_2(MonitorNetwork,
    MonitorNetworkConnection, MonitorNetworkCallback, void*);
DECL_FUNC_1(DisconnectMonitorNetwork, void, MonitorNetworkConnection);
DECL_FUNC_2(EnableNetworkDevice, bool, ConnectionType, bool);
DECL_FUNC_1(SetOfflineMode, bool, bool);
DECL_FUNC_1(ListIPConfigs, IPConfigStatus*, const char*);
DECL_FUNC_2(AddIPConfig, bool, const char*, IPConfigType);
DECL_FUNC_1(SaveIPConfig, bool, IPConfig*);
DECL_FUNC_1(RemoveIPConfig, bool, IPConfig*);
DECL_FUNC_1(FreeIPConfig, void, IPConfig*);
DECL_FUNC_1(FreeIPConfigStatus, void, IPConfigStatus*);

// Touchpad
DECL_FUNC_2(SetSynapticsParameter, void, SynapticsParameter, int);

// Login
DECL_FUNC_0(EmitLoginPromptReady, bool);
DECL_FUNC_2(StartSession, bool, const char*, const char*);
DECL_FUNC_1(StopSession, bool, const char*);

// Cryptohome
DECL_FUNC_2(CryptohomeCheckKey, bool, const char*, const char*);
DECL_FUNC_0(CryptohomeIsMounted, bool);
DECL_FUNC_2(CryptohomeMount, bool, const char*, const char*);
DECL_FUNC_0(CryptohomeUnmount, bool);

// Update library
DECL_FUNC_1(Update, bool, UpdateInformation*);
DECL_FUNC_1(CheckForUpdate, bool, UpdateInformation*);

char const * const kCrosDefaultPath = "/opt/google/chrome/chromeos/libcros.so";

// Initializes the variable by looking up the function by |name|.
// This macro uses the variable 'handle' and 'error_string'.
#define INIT_FUNC(name) \
  name = name##Type(::dlsym(handle, "ChromeOS"#name)); \
  if (!name) { \
    error_string += "Couldn't load: "#name","; \
  }

bool LoadLibcros(const char* path_to_libcros, std::string& error_string) {
  error_string = std::string();

  if (!path_to_libcros) {
    error_string = "path_to_libcros can't be NULL";
    return false;
  }

  void* handle = ::dlopen(path_to_libcros, RTLD_NOW);
  if (handle == NULL) {
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
        VersionFuncType(::dlsym(handle, "ChromeOSGetMinCrosVersion"));
    VersionFuncType GetCrosVersion =
        VersionFuncType(::dlsym(handle, "ChromeOSGetCrosVersion"));

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

  // Power
  INIT_FUNC(MonitorPowerStatus);
  INIT_FUNC(DisconnectPowerStatus);
  INIT_FUNC(RetrievePowerInformation);

  // IME
  INIT_FUNC(MonitorLanguageStatus);
  INIT_FUNC(DisconnectLanguageStatus);
  INIT_FUNC(GetSupportedLanguages);
  INIT_FUNC(GetActiveLanguages);
  INIT_FUNC(ChangeLanguage);
  INIT_FUNC(SetLanguageActivated);
  INIT_FUNC(SetImePropertyActivated);
  INIT_FUNC(ActivateLanguage);
  INIT_FUNC(DeactivateLanguage);
  INIT_FUNC(ActivateImeProperty);
  INIT_FUNC(DeactivateImeProperty);
  INIT_FUNC(GetImeConfig);
  INIT_FUNC(SetImeConfig);
  INIT_FUNC(LanguageStatusConnectionIsAlive);
  INIT_FUNC(MonitorImeStatus);
  INIT_FUNC(DisconnectImeStatus);
  INIT_FUNC(NotifyCandidateClicked);
  INIT_FUNC(GetCurrentKeyboardLayoutName);
  INIT_FUNC(SetCurrentKeyboardLayoutByName);
  INIT_FUNC(GetKeyboardLayoutPerWindow);
  INIT_FUNC(SetKeyboardLayoutPerWindow);

  // Mount
  INIT_FUNC(MonitorMountStatus);
  INIT_FUNC(DisconnectMountStatus);
  INIT_FUNC(RetrieveMountInformation);
  INIT_FUNC(FreeMountStatus);
  INIT_FUNC(MountDevicePath);

  // Networking
  INIT_FUNC(GetSystemInfo);
  INIT_FUNC(RequestScan);
  INIT_FUNC(GetWifiService);
  INIT_FUNC(ConnectToNetwork);
  INIT_FUNC(FreeSystemInfo);
  INIT_FUNC(FreeServiceInfo);
  INIT_FUNC(MonitorNetwork);
  INIT_FUNC(DisconnectMonitorNetwork);
  INIT_FUNC(EnableNetworkDevice);
  INIT_FUNC(SetOfflineMode);
  INIT_FUNC(ListIPConfigs);
  INIT_FUNC(AddIPConfig);
  INIT_FUNC(SaveIPConfig);
  INIT_FUNC(RemoveIPConfig);
  INIT_FUNC(FreeIPConfig);
  INIT_FUNC(FreeIPConfigStatus);

  // Touchpad
  INIT_FUNC(SetSynapticsParameter);

  // Login
  INIT_FUNC(EmitLoginPromptReady);
  INIT_FUNC(StartSession);
  INIT_FUNC(StopSession);

  // Cryptohome
  INIT_FUNC(CryptohomeCheckKey);
  INIT_FUNC(CryptohomeIsMounted);
  INIT_FUNC(CryptohomeMount);
  INIT_FUNC(CryptohomeUnmount);

  // Update
  INIT_FUNC(Update);
  INIT_FUNC(CheckForUpdate);

  return error_string.empty();
}

}  // namespace chromeos
