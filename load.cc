// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>

#include "chromeos_cros_api.h" // NOLINT
#include "chromeos_ime.h"  // NOLINT
#include "chromeos_language.h"  // NOLINT
#include "chromeos_mount.h"  // NOLINT
#include "chromeos_network.h"  // NOLINT
#include "chromeos_power.h"  // NOLINT
#include "chromeos_synaptics.h"  // NOLINT
#include "chromeos_update.h"  // NOLINT

namespace chromeos {  // NOLINT

typedef bool (*CrosVersionCheckType)(chromeos::CrosAPIVersion);
typedef PowerStatusConnection (*MonitorPowerStatusType)(PowerMonitor, void*);
typedef void (*DisconnectPowerStatusType)(PowerStatusConnection);
typedef bool (*RetrievePowerInformationType)(PowerInformation* information);
typedef LanguageStatusConnection* (*MonitorLanguageStatusType)(
    LanguageStatusMonitorFunctions, void*);
typedef void (*DisconnectLanguageStatusType)(LanguageStatusConnection*);
typedef InputLanguageList* (*GetLanguagesType)(LanguageStatusConnection*);
typedef void (*ChangeLanguageType)(
    LanguageStatusConnection*, LanguageCategory, const char*);
typedef bool (*ActivateLanguageType)(
    LanguageStatusConnection*, LanguageCategory, const char*);
typedef bool (*DeactivateLanguageType)(
    LanguageStatusConnection*, LanguageCategory, const char*);
typedef void (*ActivateImePropertyType)(
    LanguageStatusConnection*, const char*);
typedef void (*DeactivateImePropertyType)(
    LanguageStatusConnection*, const char*);
typedef bool (*GetImeConfigType)(LanguageStatusConnection*,
                                 const char*,
                                 const char*,
                                 ImeConfigValue*);
typedef bool (*SetImeConfigType)(LanguageStatusConnection*,
                                 const char*,
                                 const char*,
                                 const ImeConfigValue&);
typedef ImeStatusConnection* (*MonitorImeStatusType)(
    const ImeStatusMonitorFunctions&, void*);
typedef void (*DisconnectImeStatusType)(ImeStatusConnection*);
typedef void (*NotifyCandidateClickedType)(ImeStatusConnection*,
                                           int, int, int);
typedef MountStatusConnection (*MonitorMountStatusType)(MountMonitor, void*);
typedef void (*DisconnectMountStatusType)(MountStatusConnection);
typedef MountStatus* (*RetrieveMountInformationType)();
typedef void (*FreeMountStatusType)(MountStatus*);
typedef bool (*ConnectToWifiNetworkType)(const char*,
                                         const char*,
                                         const char*);
typedef ServiceStatus* (*GetAvailableNetworksType)();
typedef void (*FreeServiceStatusType)(ServiceStatus*);
typedef NetworkStatusConnection
    (*MonitorNetworkStatusType)(NetworkMonitor, void*);
typedef void (*DisconnectNetworkStatusType)(NetworkStatusConnection);
typedef int (*GetEnabledNetworkDevicesType)();
typedef bool (*EnableNetworkDeviceType)(ConnectionType type, bool enable);
typedef bool (*SetOfflineModeType)(bool offline);
typedef void (*SetSynapticsParameterType)(SynapticsParameter param, int value);
typedef bool (*EmitLoginPromptReadyType)();
typedef bool (*StartSessionType)(const char*, const char*);
typedef bool (*StopSessionType)(const char*);

// Update library
typedef bool (*UpdateType)(UpdateInformation*);
typedef bool (*CheckForUpdateType)(UpdateInformation*);

CrosVersionCheckType CrosVersionCheck = 0;

MonitorPowerStatusType MonitorPowerStatus = 0;
DisconnectPowerStatusType DisconnectPowerStatus = 0;
RetrievePowerInformationType RetrievePowerInformation = 0;

MonitorLanguageStatusType MonitorLanguageStatus = 0;
DisconnectLanguageStatusType DisconnectLanguageStatus = 0;
GetLanguagesType GetActiveLanguages = 0;
GetLanguagesType GetSupportedLanguages = 0;
ChangeLanguageType ChangeLanguage = 0;
ActivateLanguageType ActivateLanguage = 0;
DeactivateLanguageType DeactivateLanguage = 0;
ActivateImePropertyType ActivateImeProperty = 0;
DeactivateImePropertyType DeactivateImeProperty = 0;
GetImeConfigType GetImeConfig = 0;
SetImeConfigType SetImeConfig = 0;

MonitorImeStatusType MonitorImeStatus = 0;
DisconnectImeStatusType DisconnectImeStatus = 0;
NotifyCandidateClickedType NotifyCandidateClicked = 0;

MonitorMountStatusType MonitorMountStatus = 0;
DisconnectMountStatusType DisconnectMountStatus = 0;
RetrieveMountInformationType RetrieveMountInformation = 0;
FreeMountStatusType FreeMountStatus = 0;

ConnectToWifiNetworkType ConnectToWifiNetwork = 0;
GetAvailableNetworksType GetAvailableNetworks = 0;
FreeServiceStatusType FreeServiceStatus = 0;
MonitorNetworkStatusType MonitorNetworkStatus = 0;
DisconnectNetworkStatusType DisconnectNetworkStatus = 0;
GetEnabledNetworkDevicesType GetEnabledNetworkDevices = 0;
EnableNetworkDeviceType EnableNetworkDevice = 0;
SetOfflineModeType SetOfflineMode = 0;

SetSynapticsParameterType SetSynapticsParameter = 0;

EmitLoginPromptReadyType EmitLoginPromptReady = 0;
StartSessionType StartSession = 0;
StopSessionType StopSession = 0;

// Update Library
UpdateType Update = 0;
CheckForUpdateType CheckForUpdate = 0;

char const * const kCrosDefaultPath = "/opt/google/chrome/chromeos/libcros.so";

// TODO(rtc/davemoore/seanparent): Give the caller some mechanism to help them
// understand why this call failed.
bool LoadCros(const char* path_to_libcros) {
  if (!path_to_libcros)
    return false;

  void* handle = ::dlopen(path_to_libcros, RTLD_NOW);
  if (handle == NULL) {
    return false;
  }

  CrosVersionCheck =
      CrosVersionCheckType(::dlsym(handle, "ChromeOSCrosVersionCheck"));

  if (!CrosVersionCheck)
    return false;

  if (!CrosVersionCheck(chromeos::kCrosAPIVersion))
    return false;

  MonitorPowerStatus = MonitorPowerStatusType(
      ::dlsym(handle, "ChromeOSMonitorPowerStatus"));

  DisconnectPowerStatus = DisconnectPowerStatusType(
      ::dlsym(handle, "ChromeOSDisconnectPowerStatus"));

  RetrievePowerInformation = RetrievePowerInformationType(
      ::dlsym(handle, "ChromeOSRetrievePowerInformation"));

  MonitorLanguageStatus = MonitorLanguageStatusType(
      ::dlsym(handle, "ChromeOSMonitorLanguageStatus"));
  DisconnectLanguageStatus = DisconnectLanguageStatusType(
      ::dlsym(handle, "ChromeOSDisconnectLanguageStatus"));
  GetActiveLanguages = GetLanguagesType(
      ::dlsym(handle, "ChromeOSGetActiveLanguages"));
  GetSupportedLanguages = GetLanguagesType(
      ::dlsym(handle, "ChromeOSGetSupportedLanguages"));
  ChangeLanguage = ChangeLanguageType(
      ::dlsym(handle, "ChromeOSChangeLanguage"));
  ActivateLanguage = ActivateLanguageType(
      ::dlsym(handle, "ChromeOSActivateLanguage"));
  DeactivateLanguage = DeactivateLanguageType(
      ::dlsym(handle, "ChromeOSDeactivateLanguage"));
  ActivateImeProperty = ActivateImePropertyType(
      ::dlsym(handle, "ChromeOSActivateImeProperty"));
  DeactivateImeProperty = DeactivateImePropertyType(
      ::dlsym(handle, "ChromeOSDeactivateImeProperty"));
  GetImeConfig = GetImeConfigType(
      ::dlsym(handle, "ChromeOSGetImeConfig"));
  SetImeConfig = SetImeConfigType(
      ::dlsym(handle, "ChromeOSSetImeConfig"));

  MonitorImeStatus = MonitorImeStatusType(
      ::dlsym(handle, "ChromeOSMonitorImeStatus"));
  DisconnectImeStatus = DisconnectImeStatusType(
      ::dlsym(handle, "ChromeOSDisconnectImeStatus"));
  NotifyCandidateClicked = NotifyCandidateClickedType(
      ::dlsym(handle, "ChromeOSNotifyCandidateClicked"));

  MonitorMountStatus = MonitorMountStatusType(
      ::dlsym(handle, "ChromeOSMonitorMountStatus"));

  FreeMountStatus = FreeMountStatusType(
      ::dlsym(handle, "ChromeOSFreeMountStatus"));

  DisconnectMountStatus = DisconnectMountStatusType(
      ::dlsym(handle, "ChromeOSDisconnectMountStatus"));

  RetrieveMountInformation = RetrieveMountInformationType(
      ::dlsym(handle, "ChromeOSRetrieveMountInformation"));

  ConnectToWifiNetwork = ConnectToWifiNetworkType(
      ::dlsym(handle, "ChromeOSConnectToWifiNetwork"));

  GetAvailableNetworks = GetAvailableNetworksType(
      ::dlsym(handle, "ChromeOSGetAvailableNetworks"));

  FreeServiceStatus = FreeServiceStatusType(
      ::dlsym(handle, "ChromeOSFreeServiceStatus"));

  MonitorNetworkStatus =
      MonitorNetworkStatusType(::dlsym(handle, "ChromeOSMonitorNetworkStatus"));

  DisconnectNetworkStatus = DisconnectNetworkStatusType(
      ::dlsym(handle, "ChromeOSDisconnectNetworkStatus"));

  GetEnabledNetworkDevices = GetEnabledNetworkDevicesType(
      ::dlsym(handle, "ChromeOSGetEnabledNetworkDevices"));

  EnableNetworkDevice = EnableNetworkDeviceType(
      ::dlsym(handle, "ChromeOSEnableNetworkDevice"));

  SetOfflineMode = SetOfflineModeType(
      ::dlsym(handle, "ChromeOSSetOfflineMode"));

  SetSynapticsParameter = SetSynapticsParameterType(
      ::dlsym(handle, "ChromeOSSetSynapticsParameter"));

  EmitLoginPromptReady = EmitLoginPromptReadyType(
      ::dlsym(handle, "ChromeOSEmitLoginPromptReady"));
  StartSession = StartSessionType(
      ::dlsym(handle, "ChromeOSStartSession"));
  StopSession = StopSessionType(
      ::dlsym(handle, "ChromeOSStopSession"));

  // Update Library
  Update = UpdateType(
      ::dlsym(handle, "ChromeOSUpdate"));
  CheckForUpdate = CheckForUpdateType(
      ::dlsym(handle, "ChromeOSCheckForUpdate"));

  return MonitorPowerStatus
      && DisconnectPowerStatus
      && RetrievePowerInformation
      && MonitorLanguageStatus
      && DisconnectLanguageStatus
      && GetActiveLanguages
      && GetSupportedLanguages
      && ChangeLanguage
      && ActivateLanguage
      && DeactivateLanguage
      && ActivateImeProperty
      && DeactivateImeProperty
      && GetImeConfig
      && SetImeConfig
      && MonitorImeStatus
      && DisconnectImeStatus
      && NotifyCandidateClicked
      && MonitorMountStatus
      && FreeMountStatus
      && DisconnectMountStatus
      && RetrieveMountInformation
      && ConnectToWifiNetwork
      && GetAvailableNetworks
      && FreeServiceStatus
      && MonitorNetworkStatus
      && DisconnectNetworkStatus
      && GetEnabledNetworkDevices
      && EnableNetworkDevice
      && SetOfflineMode
      && SetSynapticsParameter
      && EmitLoginPromptReady
      && StartSession
      && StopSession
      && Update
      && CheckForUpdate;
}

}  // namespace chromeos
