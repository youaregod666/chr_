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

const int kCryptohomeMountErrorNone = 0;
const int kCryptohomeMountErrorFatal = 1 << 0;
const int kCryptohomeMountErrorKeyFailure = 1 << 1;
const int kCryptohomeMountErrorMountPointBusy = 1 << 2;
const int kCryptohomeMountErrorNoSuchFile = 1 << 3;

extern bool (*CryptohomeCheckKey)(const char* user_email,
                                  const char* key);
extern bool (*CryptohomeMigrateKey)(const char* user_email,
                                    const char* from_key,
                                    const char* to_key);
extern bool (*CryptohomeRemove)(const char* user_email);
extern CryptohomeBlob (*CryptohomeGetSystemSalt)();
extern bool (*CryptohomeIsMounted)();
extern bool (*CryptohomeMount)(const char* user_email,
                               const char* key);
extern bool (*CryptohomeMountAllowFail)(const char* user_email,
                                        const char* key,
                                        int* mount_error);
extern bool (*CryptohomeMountGuest)(int* mount_error);
extern bool (*CryptohomeUnmount)();
extern bool (*CryptohomeTpmIsReady)();
extern bool (*CryptohomeTpmIsEnabled)();
extern bool (*CryptohomeTpmGetPassword)(std::string* password);

}  // namespace chromeos

#endif  // CHROMEOS_CRYPTOHOME_H_
