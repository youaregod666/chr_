// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CRYPTOHOME_H_
#define CHROMEOS_CRYPTOHOME_H_

namespace chromeos { // NOLINT


extern bool (*CryptohomeCheckKey)(const char* user_email,
                                  const char* key);
extern bool (*CryptohomeIsMounted)();
extern bool (*CryptohomeMount)(const char* user_email,
                               const char* key);
extern bool (*CryptohomeUnmount)();

}  // namespace chromeos

#endif  // CHROMEOS_CRYPTOHOME_H_

