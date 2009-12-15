// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>

#include "chromeos_cros_api.h" // NOLINT
#include "chromeos_language.h"  // NOLINT
#include "chromeos_mount.h"  // NOLINT
#include "chromeos_network.h"  // NOLINT
#include "chromeos_power.h"  // NOLINT
#include "chromeos_synaptics.h"  // NOLINT

namespace chromeos {  // NOLINT

typedef bool (*CrosVersionCheckType)(chromeos::CrosAPIVersion);
typedef PowerStatusConnection (*MonitorPowerStatusType)(PowerMonitor, void*);
typedef void (*DisconnectPowerStatusType)(PowerStatusConnection);
typedef bool (*RetrievePowerInformationType)(PowerInformation* information);
typedef LanguageStatusConnection* (*MonitorLanguageStatusType)(
    LanguageStatusMonitorFunction, void*);
typedef void (*DisconnectLanguageStatusType)(LanguageStatusConnection*);
typedef InputLanguageList* (*GetLanguagesType)(LanguageStatusConnection*);
typedef void (*ChangeLanguageType)(
    LanguageStatusConnection*, LanguageCategory, const char*);
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

CrosVersionCheckType CrosVersionCheck = 0;

MonitorPowerStatusType MonitorPowerStatus = 0;
DisconnectPowerStatusType DisconnectPowerStatus = 0;
RetrievePowerInformationType RetrievePowerInformation = 0;

MonitorLanguageStatusType MonitorLanguageStatus = 0;
DisconnectLanguageStatusType DisconnectLanguageStatus = 0;
GetLanguagesType GetLanguages = 0;
ChangeLanguageType ChangeLanguage = 0;

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
  GetLanguages = GetLanguagesType(
      ::dlsym(handle, "ChromeOSGetLanguages"));
  ChangeLanguage = ChangeLanguageType(
      ::dlsym(handle, "ChromeOSChangeLanguage"));

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

  return MonitorPowerStatus
      && DisconnectPowerStatus
      && RetrievePowerInformation
      && MonitorLanguageStatus
      && DisconnectLanguageStatus
      && GetLanguages
      && ChangeLanguage
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
      && SetSynapticsParameter;
}

}  // namespace chromeos
