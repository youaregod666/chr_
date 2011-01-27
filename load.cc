// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdio>
#include <dlfcn.h>
#include <string.h>
#include <vector>

#include <base/basictypes.h>

#include "chromeos_brightness.h" // NOLINT
#include "chromeos_cros_api.h" // NOLINT
#include "chromeos_cryptohome.h" // NOLINT
#include "chromeos_imageburn.h"  //NOLINT
#include "chromeos_input_method.h"  // NOLINT
#include "chromeos_input_method_ui.h"  // NOLINT
#include "chromeos_keyboard.h"  // NOLINT
#include "chromeos_libcros_service.h"  // NOLINT
#include "chromeos_login.h"  // NOLINT
#include "chromeos_mount.h"  // NOLINT
#include "chromeos_network.h"  // NOLINT
#include "chromeos_power.h"  // NOLINT
#include "chromeos_screen_lock.h"  // NOLINT
#include "chromeos_speech_synthesis.h"  // NOLINT
#include "chromeos_synaptics.h"  // NOLINT
#include "chromeos_touchpad.h"  // NOLINT
#include "chromeos_update.h"  // NOLINT
#include "chromeos_update_engine.h"  // NOLINT
#include "chromeos_syslogs.h"  // NOLINT
#include "chromeos_system.h"  // NOLINT

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

#define DECL_FUNC_6(name, ret, arg1, arg2, arg3, arg4, arg5, arg6) \
  typedef ret (*name##Type)(arg1, arg2, arg3, arg4, arg5, arg6); \
  name##Type name = 0;

// Version
DECL_FUNC_1(CrosVersionCheck, bool, chromeos::CrosAPIVersion);

// Power
DECL_FUNC_2(MonitorPowerStatus, PowerStatusConnection, PowerMonitor, void*);
DECL_FUNC_1(DisconnectPowerStatus, void, PowerStatusConnection);
DECL_FUNC_1(RetrievePowerInformation, bool, PowerInformation*);
DECL_FUNC_1(EnableScreenLock, void, bool);
DECL_FUNC_0(RequestRestart, void);
DECL_FUNC_0(RequestShutdown, void);

// Input methods
DECL_FUNC_5(MonitorInputMethodStatus,
    InputMethodStatusConnection*,
    void*,
    chromeos::LanguageCurrentInputMethodMonitorFunction,
    chromeos::LanguageRegisterImePropertiesFunction,
    chromeos::LanguageUpdateImePropertyFunction,
    chromeos::LanguageConnectionChangeMonitorFunction);
DECL_FUNC_1(DisconnectInputMethodStatus, void, InputMethodStatusConnection*);
DECL_FUNC_1(StopInputMethodProcess, bool, InputMethodStatusConnection*);
DECL_FUNC_1(GetSupportedInputMethods,
    InputMethodDescriptors*, InputMethodStatusConnection*);
DECL_FUNC_1(GetActiveInputMethods,
    InputMethodDescriptors*, InputMethodStatusConnection*);
DECL_FUNC_2(SetActiveInputMethods,
    bool,
    InputMethodStatusConnection*,
    const ImeConfigValue&);
DECL_FUNC_2(ChangeInputMethod,
    bool, InputMethodStatusConnection*, const char*);
DECL_FUNC_1(GetCurrentInputMethod,
    InputMethodDescriptor*, InputMethodStatusConnection*);
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
DECL_FUNC_1(GetKeyboardOverlayId, std::string, const std::string&);
DECL_FUNC_2(MonitorInputMethodUiStatus,
            InputMethodUiStatusConnection*,
            const InputMethodUiStatusMonitorFunctions&,
            void*);
DECL_FUNC_1(DisconnectInputMethodUiStatus,
            void,
            InputMethodUiStatusConnection*);
DECL_FUNC_4(NotifyCandidateClicked, void,
            InputMethodUiStatusConnection*, int, int, int);
DECL_FUNC_2(MonitorInputMethodConnection,
            void,
            InputMethodUiStatusConnection*,
            InputMethodConnectionChangeMonitorFunction);

// Keyboard
DECL_FUNC_0(GetHardwareKeyboardLayoutName, std::string);
DECL_FUNC_0(GetCurrentKeyboardLayoutName, std::string);
DECL_FUNC_1(SetCurrentKeyboardLayoutByName, bool, const std::string&);
DECL_FUNC_1(RemapModifierKeys, bool, const ModifierMap&);
DECL_FUNC_1(GetAutoRepeatEnabled, bool, bool*);
DECL_FUNC_1(SetAutoRepeatEnabled, bool, bool);
DECL_FUNC_1(GetAutoRepeatRate, bool, chromeos::AutoRepeatRate*);
DECL_FUNC_1(SetAutoRepeatRate, bool, const chromeos::AutoRepeatRate&);

// Mount
DECL_FUNC_2(MonitorMountStatus, MountStatusConnection, MountMonitor, void*);
DECL_FUNC_1(DisconnectMountStatus, void, MountStatusConnection);
DECL_FUNC_0(RetrieveMountInformation, MountStatus*);
DECL_FUNC_1(FreeMountStatus, void, MountStatus*);
DECL_FUNC_1(MountDevicePath, bool, const char*);
DECL_FUNC_1(UnmountDevicePath, bool, const char*);
DECL_FUNC_1(IsBootDevicePath, bool, const char*);

// Networking
DECL_FUNC_0(GetSystemInfo, SystemInfo*);
DECL_FUNC_1(RequestScan, void, ConnectionType);
DECL_FUNC_2(GetWifiService, ServiceInfo*, const char*, ConnectionSecurity);
DECL_FUNC_2(ActivateCellularModem, bool, const char*, const char*);
DECL_FUNC_5(ConfigureWifiService, bool, const char*, ConnectionSecurity,
            const char*, const char*, const char*);
DECL_FUNC_2(ConnectToNetwork, bool, const char*, const char*);
DECL_FUNC_4(ConnectToNetworkWithCertInfo, bool, const char*, const char*,
            const char*, const char*);
DECL_FUNC_1(DisconnectFromNetwork, bool, const char*);
DECL_FUNC_1(DeleteRememberedService, bool, const char*);
DECL_FUNC_1(FreeSystemInfo, void, SystemInfo*);
DECL_FUNC_1(FreeServiceInfo, void, ServiceInfo*);
// MonitorNetwork is deprecated: use MonitorNetworkManager
DECL_FUNC_2(MonitorNetwork,
            MonitorNetworkConnection, MonitorNetworkCallback, void*);
// DisconnectMonitorNetwork is deprecated: use DisconnectPropertyChangeMonitor
DECL_FUNC_1(DisconnectMonitorNetwork, void, MonitorNetworkConnection);
DECL_FUNC_2(MonitorNetworkManager, PropertyChangeMonitor,
            MonitorPropertyCallback, void*);
DECL_FUNC_1(DisconnectPropertyChangeMonitor, void, PropertyChangeMonitor);
DECL_FUNC_3(MonitorNetworkService, PropertyChangeMonitor,
            MonitorPropertyCallback, const char*, void*);
DECL_FUNC_2(MonitorCellularDataPlan, DataPlanUpdateMonitor,
            MonitorDataPlanCallback, void*);
DECL_FUNC_1(DisconnectDataPlanUpdateMonitor, void, DataPlanUpdateMonitor);
DECL_FUNC_1(RetrieveCellularDataPlans, CellularDataPlanList*, const char*);
DECL_FUNC_1(RequestCellularDataPlanUpdate, void, const char*);
DECL_FUNC_1(FreeCellularDataPlanList, void, CellularDataPlanList*);
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
DECL_FUNC_0(GetDeviceNetworkList, DeviceNetworkList*);
DECL_FUNC_1(FreeDeviceNetworkList, void, DeviceNetworkList*);

// Synaptics
DECL_FUNC_2(SetSynapticsParameter, void, SynapticsParameter, int);

// Touchpad
DECL_FUNC_1(SetTouchpadSensitivity, void, int);
DECL_FUNC_1(SetTouchpadTapToClick, void, bool);

// Login
DECL_FUNC_2(CheckWhitelist, bool, const char*, std::vector<uint8>*);
DECL_FUNC_2(CheckWhitelistSafe, bool, const char*, CryptoBlob**);
DECL_FUNC_0(EmitLoginPromptReady, bool);
DECL_FUNC_1(EnumerateWhitelisted, bool, std::vector<std::string>*);
DECL_FUNC_1(EnumerateWhitelistedSafe, bool, UserList**);
DECL_FUNC_2(CreateCryptoBlob, CryptoBlob*, const uint8*, const int);
DECL_FUNC_4(CreateProperty,
            Property*,
            const char*,
            const char*,
            const uint8*,
            const int);
DECL_FUNC_1(CreateUserList, UserList*, char**);
DECL_FUNC_1(FreeCryptoBlob, void, CryptoBlob*);
DECL_FUNC_1(FreeProperty, void, Property*);
DECL_FUNC_1(FreeUserList, void, UserList*);
DECL_FUNC_2(RestartJob, bool, int, const char*);
DECL_FUNC_0(RestartEntd, bool);
DECL_FUNC_3(RetrieveProperty,
            bool,
            const char*,
            std::string*,
            std::vector<uint8>*);
DECL_FUNC_2(RetrievePropertySafe, bool, const char*, Property**);
DECL_FUNC_1(SetOwnerKey, bool, const std::vector<uint8>&);
DECL_FUNC_1(SetOwnerKeySafe, bool, const CryptoBlob*);
DECL_FUNC_2(StartSession, bool, const char*, const char*);
DECL_FUNC_1(StopSession, bool, const char*);
DECL_FUNC_3(StoreProperty,
            bool,
            const char*,
            const char*,
            const std::vector<uint8>&);
DECL_FUNC_1(StorePropertySafe, bool, const Property*);
DECL_FUNC_2(Unwhitelist, bool, const char*, const std::vector<uint8>&);
DECL_FUNC_2(UnwhitelistSafe, bool, const char*, const CryptoBlob*);
DECL_FUNC_2(Whitelist, bool, const char*, const std::vector<uint8>&);
DECL_FUNC_2(WhitelistSafe, bool, const char*, const CryptoBlob*);
DECL_FUNC_2(MonitorSession, SessionConnection, SessionMonitor, void*);
DECL_FUNC_1(DisconnectSession, void, SessionConnection);

// Screen Lock
DECL_FUNC_2(MonitorScreenLock,
            ScreenLockConnection, ScreenLockMonitor, void*);
DECL_FUNC_1(DisconnectScreenLock, void, ScreenLockConnection);
DECL_FUNC_0(NotifyScreenLockCompleted, void);
DECL_FUNC_0(NotifyScreenLockRequested, void);
DECL_FUNC_0(NotifyScreenUnlockRequested, void);
DECL_FUNC_0(NotifyScreenUnlockCompleted, void);

// Cryptohome
DECL_FUNC_2(CryptohomeCheckKey, bool, const char*, const char*);
DECL_FUNC_2(CryptohomeAsyncCheckKey, int, const char*, const char*);
DECL_FUNC_3(CryptohomeMigrateKey, bool, const char*, const char*, const char*);
DECL_FUNC_3(CryptohomeAsyncMigrateKey,
            int,
            const char*,
            const char*,
            const char*);
DECL_FUNC_1(CryptohomeRemove, bool, const char*);
DECL_FUNC_1(CryptohomeAsyncRemove, int, const char*);
DECL_FUNC_0(CryptohomeGetSystemSalt, CryptohomeBlob);
DECL_FUNC_2(CryptohomeGetSystemSaltSafe, bool, char**, int*);
DECL_FUNC_0(CryptohomeIsMounted, bool);
DECL_FUNC_3(CryptohomeMountAllowFail, bool, const char*, const char*, int*);
DECL_FUNC_6(CryptohomeMount, bool, const char*, const char*, bool, bool,
            const std::vector<std::string>&, int*);
DECL_FUNC_6(CryptohomeMountSafe, bool, const char*, const char*, bool, bool,
            const char**, int*);
DECL_FUNC_5(CryptohomeAsyncMount, int, const char*, const char*, bool, bool,
            const std::vector<std::string>&);
DECL_FUNC_5(CryptohomeAsyncMountSafe, int, const char*, const char*, bool, bool,
            const char**);
DECL_FUNC_1(CryptohomeMountGuest, bool, int*);
DECL_FUNC_0(CryptohomeAsyncMountGuest, int);
DECL_FUNC_0(CryptohomeUnmount, bool);
DECL_FUNC_0(CryptohomeRemoveTrackedSubdirectories, bool);
DECL_FUNC_0(CryptohomeAsyncRemoveTrackedSubdirectories, int);
DECL_FUNC_0(CryptohomeTpmIsReady, bool);
DECL_FUNC_0(CryptohomeTpmIsEnabled, bool);
DECL_FUNC_0(CryptohomeTpmIsOwned, bool);
DECL_FUNC_0(CryptohomeTpmIsBeingOwned, bool);
DECL_FUNC_1(CryptohomeTpmGetPassword, bool, std::string*);
DECL_FUNC_1(CryptohomeTpmGetPasswordSafe, bool, char**);
DECL_FUNC_0(CryptohomeTpmCanAttemptOwnership, void);
DECL_FUNC_0(CryptohomeTpmClearStoredPassword, void);
DECL_FUNC_1(CryptohomeGetStatusString, bool, std::string*);
DECL_FUNC_1(CryptohomeGetStatusStringSafe, bool, char**);
DECL_FUNC_1(CryptohomeFreeString, void, char*);
DECL_FUNC_1(CryptohomeFreeBlob, void, char*);
DECL_FUNC_2(CryptohomeMonitorSession, void*, CryptohomeSignalCallback, void*);
DECL_FUNC_1(CryptohomeDisconnectSession, void, void*);

// Imageburn
DECL_FUNC_2(MonitorBurnStatus, BurnStatusConnection, BurnMonitor, void*);
DECL_FUNC_1(DisconnectBurnStatus, void, BurnStatusConnection);
DECL_FUNC_3(StartBurn, void, const char*, const char*, BurnStatusConnection);

// Update library
DECL_FUNC_1(Update, bool, UpdateInformation*);
DECL_FUNC_1(CheckForUpdate, bool, UpdateInformation*);

// Update Engine (replacing Update library)
DECL_FUNC_2(MonitorUpdateStatus, UpdateStatusConnection, UpdateMonitor, void*);
DECL_FUNC_1(DisconnectUpdateProgress, void, UpdateStatusConnection);
DECL_FUNC_1(RetrieveUpdateProgress, bool, UpdateProgress*);
DECL_FUNC_0(InitiateUpdateCheck, bool);
DECL_FUNC_0(RebootIfUpdated, bool);
DECL_FUNC_1(SetTrack, bool, const std::string&);
DECL_FUNC_0(GetTrack, std::string);

DECL_FUNC_1(Speak, bool, const char*);
DECL_FUNC_1(SetSpeakProperties, bool, const char*);
DECL_FUNC_0(StopSpeaking, bool);
DECL_FUNC_0(IsSpeaking, bool);
DECL_FUNC_1(InitTts, void, InitStatusCallback);

// Syslogs
DECL_FUNC_2(GetSystemLogs, LogDictionaryType*, FilePath*, const std::string&);

// System
DECL_FUNC_0(GetTimezoneID, std::string);
DECL_FUNC_1(SetTimezoneID, void, const std::string& id);
DECL_FUNC_0(GetMachineInfo, MachineInfo*);
DECL_FUNC_1(FreeMachineInfo, void, MachineInfo*);

// Brightness
DECL_FUNC_2(MonitorBrightness,
            BrightnessConnection,
            BrightnessMonitorFunction,
            void*);
DECL_FUNC_1(DisconnectBrightness, void, BrightnessConnection);

// LibCros Service
DECL_FUNC_0(StartLibCrosService, LibCrosServiceConnection);
DECL_FUNC_1(StopLibCrosService, void, LibCrosServiceConnection);
DECL_FUNC_3(SetNetworkProxyResolver, void,
            NetworkProxyResolver, void*, LibCrosServiceConnection);
DECL_FUNC_4(NotifyNetworkProxyResolved, bool, const char*, const char*,
            const char*, LibCrosServiceConnection);

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
  INIT_FUNC(EnableScreenLock);
  INIT_FUNC(RequestRestart);
  INIT_FUNC(RequestShutdown);

  // Input methods
  INIT_FUNC(MonitorInputMethodStatus);
  INIT_FUNC(DisconnectInputMethodStatus);
  INIT_FUNC(StopInputMethodProcess);
  INIT_FUNC(GetSupportedInputMethods);
  INIT_FUNC(GetActiveInputMethods);
  INIT_FUNC(SetActiveInputMethods);
  INIT_FUNC(ChangeInputMethod);
  INIT_FUNC(GetCurrentInputMethod);
  INIT_FUNC(SetImePropertyActivated);
  INIT_FUNC(GetImeConfig);
  INIT_FUNC(SetImeConfig);
  INIT_FUNC(InputMethodStatusConnectionIsAlive);
  INIT_FUNC(GetKeyboardOverlayId);
  INIT_FUNC(MonitorInputMethodUiStatus);
  INIT_FUNC(DisconnectInputMethodUiStatus);
  INIT_FUNC(NotifyCandidateClicked);
  INIT_FUNC(MonitorInputMethodConnection);

  // Keyboard
  INIT_FUNC(GetHardwareKeyboardLayoutName);
  INIT_FUNC(GetCurrentKeyboardLayoutName);
  INIT_FUNC(SetCurrentKeyboardLayoutByName);
  INIT_FUNC(RemapModifierKeys);
  INIT_FUNC(GetAutoRepeatEnabled);
  INIT_FUNC(SetAutoRepeatEnabled);
  INIT_FUNC(GetAutoRepeatRate);
  INIT_FUNC(SetAutoRepeatRate);

  // Mount
  INIT_FUNC(MonitorMountStatus);
  INIT_FUNC(DisconnectMountStatus);
  INIT_FUNC(RetrieveMountInformation);
  INIT_FUNC(FreeMountStatus);
  INIT_FUNC(MountDevicePath);
  INIT_FUNC(UnmountDevicePath);
  INIT_FUNC(IsBootDevicePath);

  // Networking
  INIT_FUNC(GetSystemInfo);
  INIT_FUNC(RequestScan);
  INIT_FUNC(GetWifiService);
  INIT_FUNC(ActivateCellularModem);
  INIT_FUNC(ConfigureWifiService);
  INIT_FUNC(ConnectToNetwork);
  INIT_FUNC(ConnectToNetworkWithCertInfo);
  INIT_FUNC(DisconnectFromNetwork);
  INIT_FUNC(DeleteRememberedService);
  INIT_FUNC(FreeSystemInfo);
  INIT_FUNC(FreeServiceInfo);
  // MonitorNetwork is deprecated: use MonitorNetworkManager
  INIT_FUNC(MonitorNetwork);
  // DisconnectMonitorNetwork is deprecated:
  // use DisconnectPropertyChangeMonitor
  INIT_FUNC(DisconnectMonitorNetwork);
  INIT_FUNC(MonitorNetworkManager);
  INIT_FUNC(DisconnectPropertyChangeMonitor);
  INIT_FUNC(MonitorNetworkService);
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
  INIT_FUNC(GetDeviceNetworkList);
  INIT_FUNC(FreeDeviceNetworkList);
  INIT_FUNC(MonitorCellularDataPlan);
  INIT_FUNC(DisconnectDataPlanUpdateMonitor);
  INIT_FUNC(RetrieveCellularDataPlans);
  INIT_FUNC(RequestCellularDataPlanUpdate);
  INIT_FUNC(FreeCellularDataPlanList);

  // Synaptics
  INIT_FUNC(SetSynapticsParameter);

  // Touchpad
  INIT_FUNC(SetTouchpadSensitivity);
  INIT_FUNC(SetTouchpadTapToClick);

  // Login
  INIT_FUNC(CheckWhitelist);
  INIT_FUNC(CheckWhitelistSafe);
  INIT_FUNC(EmitLoginPromptReady);
  INIT_FUNC(EnumerateWhitelisted);
  INIT_FUNC(EnumerateWhitelistedSafe);
  INIT_FUNC(CreateCryptoBlob);
  INIT_FUNC(CreateProperty);
  INIT_FUNC(CreateUserList);
  INIT_FUNC(FreeCryptoBlob);
  INIT_FUNC(FreeProperty);
  INIT_FUNC(FreeUserList);
  INIT_FUNC(RestartJob);
  INIT_FUNC(RestartEntd);
  INIT_FUNC(RetrieveProperty);
  INIT_FUNC(RetrievePropertySafe);
  INIT_FUNC(SetOwnerKey);
  INIT_FUNC(SetOwnerKeySafe);
  INIT_FUNC(StartSession);
  INIT_FUNC(StopSession);
  INIT_FUNC(StoreProperty);
  INIT_FUNC(StorePropertySafe);
  INIT_FUNC(Unwhitelist);
  INIT_FUNC(UnwhitelistSafe);
  INIT_FUNC(Whitelist);
  INIT_FUNC(WhitelistSafe);
  INIT_FUNC(MonitorSession);
  INIT_FUNC(DisconnectSession);

  // Screen Lock
  INIT_FUNC(MonitorScreenLock);
  INIT_FUNC(DisconnectScreenLock);
  INIT_FUNC(NotifyScreenLockCompleted);
  INIT_FUNC(NotifyScreenLockRequested);
  INIT_FUNC(NotifyScreenUnlockRequested);
  INIT_FUNC(NotifyScreenUnlockCompleted);

  // Cryptohome
  INIT_FUNC(CryptohomeCheckKey);
  INIT_FUNC(CryptohomeAsyncCheckKey);
  INIT_FUNC(CryptohomeMigrateKey);
  INIT_FUNC(CryptohomeAsyncMigrateKey);
  INIT_FUNC(CryptohomeRemove);
  INIT_FUNC(CryptohomeAsyncRemove);
  INIT_FUNC(CryptohomeGetSystemSalt);
  INIT_FUNC(CryptohomeGetSystemSaltSafe);
  INIT_FUNC(CryptohomeIsMounted);
  INIT_FUNC(CryptohomeMountAllowFail);
  INIT_FUNC(CryptohomeMount);
  INIT_FUNC(CryptohomeMountSafe);
  INIT_FUNC(CryptohomeAsyncMount);
  INIT_FUNC(CryptohomeAsyncMountSafe);
  INIT_FUNC(CryptohomeMountGuest);
  INIT_FUNC(CryptohomeAsyncMountGuest);
  INIT_FUNC(CryptohomeUnmount);
  INIT_FUNC(CryptohomeRemoveTrackedSubdirectories);
  INIT_FUNC(CryptohomeAsyncRemoveTrackedSubdirectories);
  INIT_FUNC(CryptohomeTpmIsReady);
  INIT_FUNC(CryptohomeTpmIsEnabled);
  INIT_FUNC(CryptohomeTpmIsOwned);
  INIT_FUNC(CryptohomeTpmIsBeingOwned);
  INIT_FUNC(CryptohomeTpmGetPassword);
  INIT_FUNC(CryptohomeTpmGetPasswordSafe);
  INIT_FUNC(CryptohomeTpmCanAttemptOwnership);
  INIT_FUNC(CryptohomeTpmClearStoredPassword);
  INIT_FUNC(CryptohomeGetStatusString);
  INIT_FUNC(CryptohomeGetStatusStringSafe);
  INIT_FUNC(CryptohomeFreeString);
  INIT_FUNC(CryptohomeFreeBlob);
  INIT_FUNC(CryptohomeMonitorSession);
  INIT_FUNC(CryptohomeDisconnectSession);

  // Imageburn
  INIT_FUNC(MonitorBurnStatus);
  INIT_FUNC(DisconnectBurnStatus);
  INIT_FUNC(StartBurn);

  // Update
  INIT_FUNC(Update);
  INIT_FUNC(CheckForUpdate);

  // Update Engine
  INIT_FUNC(MonitorUpdateStatus);
  INIT_FUNC(DisconnectUpdateProgress);
  INIT_FUNC(RetrieveUpdateProgress);
  INIT_FUNC(InitiateUpdateCheck);
  INIT_FUNC(RebootIfUpdated);
  INIT_FUNC(SetTrack);
  INIT_FUNC(GetTrack);

  // Speech Synthesis
  INIT_FUNC(Speak);
  INIT_FUNC(SetSpeakProperties);
  INIT_FUNC(StopSpeaking);
  INIT_FUNC(IsSpeaking);
  INIT_FUNC(InitTts);

  // Syslogs
  INIT_FUNC(GetSystemLogs);

  // System
  INIT_FUNC(GetTimezoneID);
  INIT_FUNC(SetTimezoneID);
  INIT_FUNC(GetMachineInfo);
  INIT_FUNC(FreeMachineInfo);

  // Brightness
  INIT_FUNC(MonitorBrightness);
  INIT_FUNC(DisconnectBrightness);

  // LibCros Service
  INIT_FUNC(StartLibCrosService);
  INIT_FUNC(StopLibCrosService);
  INIT_FUNC(SetNetworkProxyResolver);
  INIT_FUNC(NotifyNetworkProxyResolved);

  return error_string.empty();
}

}  // namespace chromeos
