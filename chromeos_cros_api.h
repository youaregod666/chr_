// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CROS_API_VERSION_H_
#define CHROMEOS_CROS_API_VERSION_H_

#include <string>
#include "base/time.h"

// This file defines two version numbers for the CrosAPI.
//
// We have two components, the libcros.so (so) and the libcros API which is
// built into chrome (app). The version of the two components may not match.
// The so will support any app version in the range
// [kCrosAPIMinVersion, kCrosAPIVersion]. Currently the app will not support
// older versions of the so.

// The expected workflow is that the so is updated to be backward compatible
// by at least one version of the app. This allows the so to be updated
// then the app without breaking either build. Support for older than n-1
// versions should only be done if there isn't any advantage to breaking the
// older versions (for example, cleaning out otherwise dead code).

// Removing an API method is a bit tricky. It takes a few steps:
// 1) First remove all calls to that method in Chrome.
// 2) Deprecate the method but keep the method implementation. Remove the
//      binding of the method in load.cc and increment kCrosAPIVersion in this
//      file. Check this in.
// 3) Once ChromeOS looks good, update cros_deps/DEPS in Chrome source tree.
//      This new libcros.so will work with new Chrome (which does not bind with
//      the deprecated method) and it will still work with an older Chrome
//      (which does bind with the deprecated method but does not call it).
// 4) Wait until all versions of Chrome (i.e. the binary release of Chrome)
//      is using this new libcros.so.
// 5) Now, delete the method implementation. Increment kCrosAPIVersion and set
//      kCrosAPIMinVersion to what kCrosAPIVersion was. Check this in.
// 6) Once ChromeOS looks good, update cros_deps/DEPS in Chrome source tree.
//      This new libcros.so will work with both the new Chrome and the one
//      version back Chrome, since neither Chrome will be trying to bind to
//      this now deleted method.

// It is not recommended to change the signature of a method as that can never
// be backwards compatible. So just name it differently. Add the new method and
// remove the old method as described above.

// Current version numbers:
//  0:  Version number prior to the version scheme.
//  1:  Added CrosVersionCheck API.
//      Changed load to take file path instead of so handle.
//  2:  Changed the interface for network monitoring callbacks.
//  3:  Added support disconnecting the network monitor.
//  4:  Added Update API
//  5:  Added IPConfig code
//  6:  Deprecated GetIPConfigProperty and SetIPConfigProperty.
//  7:  Added a member to InputLanguageList struct (backward incompatible).
//  8:  Added LanguageStatusConnectionIsAlive API.
//  9:  Added ConnectToNetwork and refactored code (backward incompatible).
//  10: Added support for mounting/unmounting cryptohomes.
//  11: Added GetWifiService and FreeServiceInfo
//  12: Added RequestScan
//  13: Added SystemInfo replacing ServiceStatus. Reworked network monitoring.
//      Removed loading of deprecated methods: GetAvailableNetworks,
//      FreeServiceStatus, MonitorNetworkStatus, DisconnectNetworkStatus,
//      GetEnabledTechnologies
//  14: Removed deprecated method
//  15: Added SetLanguageActivated and SetImePropertyActivated
//  16: Added MountDevicePath function, removed automatic mounting
//  17: Added GetCurrentKeyboardLayoutName, SetCurrentKeyboardLayoutByName,
//      GetKeyboardLayoutPerWindow, SetKeyboardLayoutPerWindow
//  18: Added GetActiveInputMethods, GetSupportedInputMethods,
//      ChangeInputMethod, SetInputMethodActivated
//  19: Removed loading of deprecated methods: GetSupportedLanguages,
//      GetActiveLanguages, ChangeLanguage, SetLanguageActivated,
//      ActivateLanguage, DeactivateLanguage, ActivateImeProperty,
//      DeactivateImeProperty
//  20: Removed depreceted methods above.
//  21: Removed loading of a deprecated method: SetInputMethodActivated
//  22: Added ChromeOSConnectToNetworkWithCertInfo.
//  23: Added profile entries and SetAutoConnect and DeleteRememberedService.
//  24: Added MonitorInputMethodUiStatus and DisconnectInputMethodUiStatus.
//  25: Removed the depreceted method above.
//      Added MonitorInputMethodStatus, DisconnectInputMethodStatusConnection,
//      InputMethodStatusConnectionIsAlive.
//  26: Added speech synthesis library functions.
//  27: Added DisconnectFromNetwork and SetPassphrase
//  28: Removed loading of deprecated methods: MonitorLanguageStatus,
//      DisconnectLanguageStatus, LanguageStatusConnectionIsAlive,
//      MonitorImeStatus, DisconnectImeStatus.
//  29: Added SetIdentity and SetCertPath. Added identity and cert_path
//      fields to ServiceInfo. Not backwards-compatible.
//  30: Reused an obsoleted member variable |icon_path| in InputMethodDescriptor
//      class by renaming it to |keyboard_layout|. This change should be
//      backwards-compatible since the type of the variable is not changed and
//      the obsoleted variable hasn't been used for over a month.
//      ScreenLock API is also added in this version.
//  31: Added NotifyScreenUnlockRequested, and obsoleted NotifyScreenUnlocked.
//      Added new ScreenLockState argument to ScreenLockMonitor callback.
//  32: Added orientation information to InputMethodLookupTable.
//  33: Added NotifyScreenLockRequested.
//      Renamed ScreenLockState to ScreenLockEvent
//  34: Added GetSystemInfo. Logs returned by GetSystemInfo are consumed by
//      the bug report dialog when submitting user feedback to Google Feedback
//  35: Removed deprecated methods for input method (see v28).
//  36: Fix crashing but with device_path set to unknown.
//  37: Added CryptohomeGetSystemSalt, CryptohomeMigrateKey, and
//      CryptohomeRemove.
//  38: Added InitTts function to the speech synthesis library.
//  39: Added CryptohomeMountAllowFail and CryptohomeMountGuest
//  40: Fixed update_engine API to be ABI safe. API was not called in prior
//      versions of chrome so no backward compatibility provided.
//  41: Added chromeos_system timezone calls.
//  42: Added label information to InputMethodLookupTable.
//  43: Changed a string literal in chromeos_input_method.h.
//  44: Added SetActiveInputMethods() to chromeos_input_method.{cc,h}.
//  45: Added DeviceNetworkList + associated Get & Free functions.
//  46: Change the meaning of the last argument in MonitorInputMethodStatus().
//  47: Added RemapModifierKeys
//  48: Added UPDATE_STATUS_REPORTING_ERROR_EVENT.
//  49: Added RebootIfUpdated.
//  50: Added new flimflam error codes.
//  51: Added GetMachineInfo
//  52: Added ConfigureWifiService.
//  53: Added TPM APIs to Cryptohome.
//  54: Added GetAutoRepeatEnabled, SetAutoRepeatEnabled, GetAutoRepeatRate, and
//      SetAutoRepeatRate
//  55: Added GetServiceInfo and GetRememberedServiceInfo on SystemInfo struct.
//  56: Added RestartJob to chromeos_login.{cc,h}.
//  57: Added UnmountDevicePath to chromeos_mount.h
//  58: Added GetHardwareKeyboardLayoutName
//  59: Added ActivateCellularModem and activation_state
//      This is not backwards compatible, b/c we added a field to ServiceInfo.
//      Client will need to use GetServiceInfo() which was added in version 55.
//  60: Changed GetSystemLogs to accept a NULL parameter.
//  61: Added chromeos_imageburn.{cc,h} API for burning a recovery image
//  62: Added async API for setting the Owner's public key.
//  63: Added async APIs for managing user whitelist and system settings.
//  64: Added async APIs for cryptohome
//  65: Added more status APIs for cryptohome
//  66: Added annotation information to InputMethodLookupTable.
//  67: Added an un-signed API for enumerating the whitelisted users.
//  68: Added new generic touchpad api.
//  69: Added async API for removing a user's cryptohome
//  70: Added ibus connection change callback for UI.
//  71: Added new network service properties, and handlers for service property
//      change signals.
//  72: Added cellular data plan (CellularDataPlan) and monitoring.
//  73: Changed cryptohome Mount APIs to support better configuration
//  74: Added TpmCanAttemptOwnership/TpmClearStoredPassword APIs to cryptohome
//  75: Added DeviceInfo and CarrierInfo to ServiceInfo
//  76: Added EnableScreenLock to power library.
//  77: Added RequestShutdown to power library.
//  78: Changed ListIPConfigs to return the hardware address as well.
//  79: Added RequestRestart to power library.
//  80: Added a context parameter to GetSystemLogs
//  81: Added SetTrack and GetTrack to update engine library.
//  82: Add *Safe calls for the ownership API, which use memory safely.
//  83: Changed CellularDataPlan APIs, which use memory safely
//  84: Added Connectable property to network services.
//  85: Changed DeviceInfo in network library.
//  86: Deprecated restricted_pool and added connectivity_state in ServiceInfo
//      struct in network library
//  87: Added entd restarting ability.
//  88: Added MonitorBrightness and DisconnectBrightness.
//  89: Added GetCurrentInputMethod.
//  90: Added StopInputMethodProcess.
//  91: Added GetKeyboardOverlayID to input method library.
//  92: Added IsBootDevicePath.
//  93: Deprecated GetKeyboardLayoutPerWindow and SetKeyboardLayoutPerWindow.
//  94: Added LibCrosService service for network proxy resolution functionality.
//  95: Deprecated GetActiveInputMethods and SetActiveInputMethods.
//  96: Deleted GetKeyboardLayoutPerWindow and SetKeyboardLayoutPerWindow.
//  97: Added GetSupportedInputMethodDescriptors
//  98: Deprecated GetHardwareKeyboardLayoutName and
//      CreateFallbackInputMethodDescriptors
//  99: Deprecated GetSupportedInputMethods.
// 100: Deprecated GetImeConfig.
// 101: Added MonitorResume and DisconnectResume.
// 102: Moved chromeos_network:GetSystemInfo() into chromeos_network_deprecated
//      and added new asynchronous getters.
// 103: Deprecated GetCurrentKeyboardLayoutName.
// 104: Deprecated DisconnectInputMethodStatus and
//      InputMethodStatusConnectionIsAlive.
// 105: Introduce SetLibcrosHistogramFunction.
// 106: Removed GetActiveInputMethods, SetActiveInputMethods,
//      GetSupportedInputMethods, SetInputMethodActivated, GetImeConfig,
//      GetHardwareKeyboardLayoutName, GetCurrentKeyboardLayoutName,
//      DisconnectInputMethodStatus, and InputMethodStatusConnectionIsAlive.
// 107: Added RequestNetworkServiceConnect and SetNetworkServiceProperty.
// 108: Move types, RequestWifiServicePath, RequestScan, EnableNetworkDevice
//      to deprecated. Add RequestHiddenWifiNetwork, RequestNetworkScan,
//      RequestNetworkDeviceEnable.

namespace chromeos {  // NOLINT

enum CrosAPIVersion {
  kCrosAPIMinVersion = 104,
  kCrosAPIVersion = 108
};

// Default path to pass to LoadCros: "/opt/google/chrome/chromeos/libcros.so"
extern char const * const kCrosDefaultPath;

// |path_to_libcros| is the path to the libcros.so file.
// Returns true to indicate success.
// If returns false, |load_error| will contain a string describing the
// problem.
bool LoadLibcros(const char* path_to_libcros, std::string& load_error);

// Use to provide a function for passing histogram timing values to Chrome.
typedef void (*LibcrosTimeHistogramFunc)(
    const char* name, const base::TimeDelta& delta);
void SetLibcrosTimeHistogramFunction(LibcrosTimeHistogramFunc func);

}  // namespace chromeos

#endif /* CHROMEOS_CROS_API_VERSION_H_ */
