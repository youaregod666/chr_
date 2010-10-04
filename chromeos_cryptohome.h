// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CRYPTOHOME_H_
#define CHROMEOS_CRYPTOHOME_H_

#include <string>
#include <vector>

namespace chromeos { // NOLINT

// Typedef CryptohomeBlob here so that we don't have to include additional
// headers.
typedef std::vector<unsigned char> CryptohomeBlob;

struct CryptohomeAsyncCallStatus {
  int async_id;
  bool return_status;
  int return_code;
};

// These constants must match the MountError enumeration in mount.h from
// cryptohome.
const int kCryptohomeMountErrorNone = 0;
const int kCryptohomeMountErrorFatal = 1 << 0;
const int kCryptohomeMountErrorKeyFailure = 1 << 1;
const int kCryptohomeMountErrorMountPointBusy = 1 << 2;
const int kCryptohomeMountErrorTpmCommError = 1 << 3;
const int kCryptohomeMountErrorTpmDefendLock = 1 << 4;
const int kCryptohomeMountErrorUserDoesNotExist = 1 << 5;
const int kCryptohomeMountErrorRecreated = 1 << 31;

extern bool (*CryptohomeCheckKey)(const char* user_email,
                                  const char* key);
extern int (*CryptohomeAsyncCheckKey)(const char* user_email,
                                      const char* key);
extern bool (*CryptohomeMigrateKey)(const char* user_email,
                                    const char* from_key,
                                    const char* to_key);
extern int (*CryptohomeAsyncMigrateKey)(const char* user_email,
                                        const char* from_key,
                                        const char* to_key);
extern bool (*CryptohomeRemove)(const char* user_email);
extern int (*CryptohomeAsyncRemove)(const char* user_email);
extern CryptohomeBlob (*CryptohomeGetSystemSalt)();
extern bool (*CryptohomeIsMounted)();
extern bool (*CryptohomeMount)(
    const char* user_email,
    const char* key,
    bool create_if_missing,
    bool replace_tracked_subdirectories,
    const std::vector<std::string>& tracked_subdirectories,
    int* mount_error);
extern bool (*CryptohomeMountAllowFail)(const char* user_email,
                                        const char* key,
                                        int* mount_error);
extern int (*CryptohomeAsyncMount)(
    const char* user_email,
    const char* key,
    bool create_if_missing,
    bool replace_tracked_subdirectories,
    const std::vector<std::string>& tracked_subdirectories);
extern bool (*CryptohomeMountGuest)(int* mount_error);
extern int (*CryptohomeAsyncMountGuest)();
extern bool (*CryptohomeUnmount)();
extern bool (*CryptohomeRemoveTrackedSubdirectories)();
extern int (*CryptohomeAsyncRemoveTrackedSubdirectories)();
extern bool (*CryptohomeTpmIsReady)();
extern bool (*CryptohomeTpmIsEnabled)();
extern bool (*CryptohomeTpmIsOwned)();
extern bool (*CryptohomeTpmIsBeingOwned)();
extern bool (*CryptohomeTpmGetPassword)(std::string* password);
extern void (*CryptohomeTpmCanAttemptOwnership)();
extern void (*CryptohomeTpmClearStoredPassword)();
extern bool (*CryptohomeGetStatusString)(std::string* status);

typedef void(*CryptohomeSignalCallback)(
    const CryptohomeAsyncCallStatus& call_status, void* callback_context);

extern void* (*CryptohomeMonitorSession)(
    CryptohomeSignalCallback monitor,
    void* monitor_context);
extern void (*CryptohomeDisconnectSession)(void* connection);

}  // namespace chromeos

#endif  // CHROMEOS_CRYPTOHOME_H_
