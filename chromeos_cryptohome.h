// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
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

extern int (*CryptohomeAsyncCheckKey)(const char* user_email,
                                      const char* key);
extern int (*CryptohomeAsyncMigrateKey)(const char* user_email,
                                        const char* from_key,
                                        const char* to_key);
extern int (*CryptohomeAsyncRemove)(const char* user_email);
extern bool (*CryptohomeGetSystemSaltSafe)(char** salt, int* length);
extern bool (*CryptohomeIsMounted)();
extern int (*CryptohomeAsyncMountSafe)(
    const char* user_email,
    const char* key,
    bool create_if_missing,
    bool replace_tracked_subdirectories,
    const char** tracked_subdirectories);
extern int (*CryptohomeAsyncMountGuest)();
extern bool (*CryptohomeTpmIsReady)();
extern bool (*CryptohomeTpmIsEnabled)();
extern bool (*CryptohomeTpmIsOwned)();
extern bool (*CryptohomeTpmIsBeingOwned)();
extern bool (*CryptohomeTpmGetPasswordSafe)(char** password);
extern void (*CryptohomeTpmCanAttemptOwnership)();
extern void (*CryptohomeTpmClearStoredPassword)();
extern void (*CryptohomePkcs11GetTpmTokenInfo)(std::string* label,
                                               std::string* user_pin);
extern bool (*CryptohomePkcs11IsTpmTokenReady)();
extern bool (*CryptohomeInstallAttributesGet)(const char* name, char** value);
extern bool (*CryptohomeInstallAttributesSet)(const char* name,
                                              const char* value);
extern bool (*CryptohomeInstallAttributesFinalize)();
extern bool (*CryptohomeInstallAttributesIsReady)();
extern bool (*CryptohomeInstallAttributesIsInvalid)();
extern bool (*CryptohomeInstallAttributesIsFirstInstall)();

extern void (*CryptohomeFreeString)(char* value);

typedef void(*CryptohomeSignalCallback)(
    const CryptohomeAsyncCallStatus& call_status, void* callback_context);

extern void* (*CryptohomeMonitorSession)(
    CryptohomeSignalCallback monitor,
    void* monitor_context);

}  // namespace chromeos

#endif  // CHROMEOS_CRYPTOHOME_H_
