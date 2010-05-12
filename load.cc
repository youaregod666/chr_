// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdio>
#include <dlfcn.h>
#include <string.h>

#include "chromeos_cros_api.h" // NOLINT
#include "chromeos_input_method.h"  // NOLINT
#include "chromeos_input_method_ui.h"  // NOLINT
#include "chromeos_keyboard.h"  // NOLINT
#include "chromeos_login.h"  // NOLINT
#include "chromeos_mount.h"  // NOLINT
#include "chromeos_network.h"  // NOLINT
#include "chromeos_power.h"  // NOLINT
#include "chromeos_screen_lock.h"  // NOLINT
#include "chromeos_speech_synthesis.h"  // NOLINT
#include "chromeos_synaptics.h"  // NOLINT
#include "chromeos_update.h"  // NOLINT
#include "chromeos_syslogs.h"  // NOLINT

namespace chromeos {  // NOLINT //

static std::string error_string;

// Declare Function. These macros are used to define the imported functions
// from libcros. They will declare the proper type and define an exported
// variable to be used to call the function.
// |name| is the name of the function.
// |ret| is the return type.
// |arg[1-5]| are the types are the arguments.
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

#define DECL_FUNC_5(name, ret, arg1, arg2, arg3, arg4, arg5) \
  typedef ret (*name##Type)(arg1, arg2, arg3, arg4, arg5); \
  name##Type name = 0;

// Version
DECL_FUNC_1(CrosVersionCheck, bool, chromeos::CrosAPIVersion);

// Power
DECL_FUNC_2(MonitorPowerStatus, PowerStatusConnection, PowerMonitor, void*);
DECL_FUNC_1(DisconnectPowerStatus, void, PowerStatusConnection);
DECL_FUNC_1(RetrievePowerInformation, bool, PowerInformation*);

// Input methods
DECL_FUNC_5(MonitorInputMethodStatus,
    InputMethodStatusConnection*,
    void*,
    chromeos::LanguageCurrentInputMethodMonitorFunction,
    chromeos::LanguageRegisterImePropertiesFunction,
    chromeos::LanguageUpdateImePropertyFunction,
    chromeos::LanguageFocusChangeMonitorFunction);
DECL_FUNC_1(DisconnectInputMethodStatus, void, InputMethodStatusConnection*);
DECL_FUNC_1(GetSupportedInputMethods,
    InputMethodDescriptors*, InputMethodStatusConnection*);
DECL_FUNC_1(GetActiveInputMethods,
    InputMethodDescriptors*, InputMethodStatusConnection*);
DECL_FUNC_2(ChangeInputMethod,
    bool, InputMethodStatusConnection*, const char*);
DECL_FUNC_3(SetImePropertyActivated,
    void, InputMethodStatusConnection*, const char*, bool);
DECL_FUNC_4(GetImeConfig,
    bool,
    InputMethodStatusConnection*,
    const char*,
    const char*,
    chromeos::ImeConfigValue*);
DECL_FUNC_4(SetImeConfig,
    bool,
    InputMethodStatusConnection*,
    const char*,
    const char*,
    const ImeConfigValue&);
DECL_FUNC_1(
    InputMethodStatusConnectionIsAlive, bool, InputMethodStatusConnection*);
DECL_FUNC_2(MonitorInputMethodUiStatus,
            InputMethodUiStatusConnection*,
            const InputMethodUiStatusMonitorFunctions&,
            void*);
DECL_FUNC_1(DisconnectInputMethodUiStatus,
            void,
            InputMethodUiStatusConnection*);
DECL_FUNC_4(NotifyCandidateClicked, void,
            InputMethodUiStatusConnection*, int, int, int);
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
DECL_FUNC_4(ConnectToNetworkWithCertInfo, bool, const char*, const char*,
            const char*, const char*);
DECL_FUNC_1(DisconnectFromNetwork, bool, const char*);
DECL_FUNC_1(DeleteRememberedService, bool, const char*);
DECL_FUNC_1(FreeSystemInfo, void, SystemInfo*);
DECL_FUNC_1(FreeServiceInfo, void, ServiceInfo*);
DECL_FUNC_2(MonitorNetwork,
    MonitorNetworkConnection, MonitorNetworkCallback, void*);
DECL_FUNC_1(DisconnectMonitorNetwork, void, MonitorNetworkConnection);
DECL_FUNC_2(EnableNetworkDevice, bool, ConnectionType, bool);
DECL_FUNC_1(SetOfflineMode, bool, bool);
DECL_FUNC_2(SetAutoConnect, bool, const char*, bool);
DECL_FUNC_2(SetPassphrase, bool, const char*, const char*);
DECL_FUNC_2(SetIdentity, bool, const char*, const char*);
DECL_FUNC_2(SetCertPath, bool, const char*, const char*);
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

// Screen Lock
DECL_FUNC_2(MonitorScreenLock,
            ScreenLockConnection, ScreenLockMonitor, void*);
DECL_FUNC_1(DisconnectScreenLock, void, ScreenLockConnection);
DECL_FUNC_0(NotifyScreenLockCompleted, void);
DECL_FUNC_0(NotifyScreenLockRequested, void);
DECL_FUNC_0(NotifyScreenUnlockRequested, void);
DECL_FUNC_0(NotifyScreenUnlocked, void);

// Cryptohome
DECL_FUNC_2(CryptohomeCheckKey, bool, const char*, const char*);
DECL_FUNC_0(CryptohomeIsMounted, bool);
DECL_FUNC_2(CryptohomeMount, bool, const char*, const char*);
DECL_FUNC_0(CryptohomeUnmount, bool);

// Update library
DECL_FUNC_1(Update, bool, UpdateInformation*);
DECL_FUNC_1(CheckForUpdate, bool, UpdateInformation*);

DECL_FUNC_1(Speak, bool, const char*);
DECL_FUNC_1(SetSpeakProperties, bool, const char*);
DECL_FUNC_0(StopSpeaking, bool);
DECL_FUNC_0(IsSpeaking, bool);

// Syslogs
DECL_FUNC_1(GetSystemLogs, LogDictionaryType*, char** const);



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

  // Input methods
  INIT_FUNC(MonitorInputMethodStatus);
  INIT_FUNC(DisconnectInputMethodStatus);
  INIT_FUNC(GetSupportedInputMethods);
  INIT_FUNC(GetActiveInputMethods);
  INIT_FUNC(ChangeInputMethod);
  INIT_FUNC(SetImePropertyActivated);
  INIT_FUNC(GetImeConfig);
  INIT_FUNC(SetImeConfig);
  INIT_FUNC(InputMethodStatusConnectionIsAlive);
  INIT_FUNC(MonitorInputMethodUiStatus);
  INIT_FUNC(DisconnectInputMethodUiStatus);
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
  INIT_FUNC(ConnectToNetworkWithCertInfo);
  INIT_FUNC(DisconnectFromNetwork);
  INIT_FUNC(DeleteRememberedService);
  INIT_FUNC(FreeSystemInfo);
  INIT_FUNC(FreeServiceInfo);
  INIT_FUNC(MonitorNetwork);
  INIT_FUNC(DisconnectMonitorNetwork);
  INIT_FUNC(EnableNetworkDevice);
  INIT_FUNC(SetOfflineMode);
  INIT_FUNC(SetAutoConnect);
  INIT_FUNC(SetPassphrase);
  INIT_FUNC(SetIdentity);
  INIT_FUNC(SetCertPath);
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

  // Screen Lock
  INIT_FUNC(MonitorScreenLock);
  INIT_FUNC(DisconnectScreenLock);
  INIT_FUNC(NotifyScreenLockCompleted);
  INIT_FUNC(NotifyScreenLockRequested);
  INIT_FUNC(NotifyScreenUnlockRequested);
  INIT_FUNC(NotifyScreenUnlocked);

  // Cryptohome
  INIT_FUNC(CryptohomeCheckKey);
  INIT_FUNC(CryptohomeIsMounted);
  INIT_FUNC(CryptohomeMount);
  INIT_FUNC(CryptohomeUnmount);

  // Update
  INIT_FUNC(Update);
  INIT_FUNC(CheckForUpdate);

  INIT_FUNC(Speak);
  INIT_FUNC(SetSpeakProperties);
  INIT_FUNC(StopSpeaking);
  INIT_FUNC(IsSpeaking);

  // Syslogs
  INIT_FUNC(GetSystemLogs);

  return error_string.empty();
}

}  // namespace chromeos
