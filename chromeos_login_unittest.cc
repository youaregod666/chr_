// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_login.h"
#include "chromeos_login_helpers.h"

#include "gtest/gtest.h"

namespace chromeos {

TEST(ChromeOSLoginApiTest, CryptoBlob) {
  const char expected[] = "hello";
  GArray* ary = g_array_sized_new(FALSE, FALSE, 1, strlen(expected));
  g_array_append_vals(ary, expected, strlen(expected));
  CryptoBlob* blob = ChromeOSLoginHelpers::CreateCryptoBlob(ary);

  for (int i = 0; i < blob->length; ++i)
    EXPECT_EQ(blob->data[i], expected[i]);
  EXPECT_EQ(blob->length, strlen(expected));

  g_array_free(ary, TRUE);
  ChromeOSLoginHelpers::FreeCryptoBlob(blob);
}

TEST(ChromeOSLoginApiTest, Property) {
  const char name[] = "name";
  const char val[] = "val";
  const char expected[] = "hello";
  GArray* ary = g_array_sized_new(FALSE, FALSE, 1, strlen(expected));
  g_array_append_vals(ary, expected, strlen(expected));
  Property* prop = ChromeOSLoginHelpers::CreateProperty(name, val, ary);

  for (int i = 0; i < prop->signature->length; ++i)
    EXPECT_EQ(prop->signature->data[i], expected[i]);
  EXPECT_EQ(prop->signature->length, strlen(expected));
  EXPECT_EQ(name, std::string(prop->name));
  EXPECT_EQ(val, std::string(prop->value));

  g_array_free(ary, TRUE);
  ChromeOSLoginHelpers::FreeProperty(prop);
}

TEST(ChromeOSLoginApiTest, UserList) {
  // NULL-terminated array of NULL-terminated strings to mimic a GLib
  // "vector" of strings.
  const char* names[4] = { "who", "what", "where", NULL };
  UserList* users = ChromeOSLoginHelpers::CreateUserList(names);
  int count;
  for (count = 0; names[count] != NULL; count++)
    EXPECT_EQ(names[count], std::string(users->users[count]));
  EXPECT_EQ(count, users->num_users);
  ChromeOSLoginHelpers::FreeUserList(users);
}

}  // namespace chromeos
