// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_update.h"

#include "chromeos/string.h"

#include <unistd.h>

#include <cstdlib>
#include <string>
#include <base/scoped_ptr.h>

#include <glib.h>

using std::string;

namespace chromeos {

namespace {

// This is the "virtualized" destructor for UpdateInformation.
void Destruct(const UpdateInformation& x) {
  delete x.version_;
}

// Searches for a line "key=some value" for a given key 'key'
// and returns the value part of the line
string ValueForKey(const char* const haystack, const string& key) {
  string use_key = key + "=";

  const char* needle = NULL;

  // See if haystack starts with key
  if (!strncmp(haystack, use_key.data(), use_key.size())) {
    needle = haystack;
  } else {
    use_key = string("\n") + use_key;
    needle = strstr(haystack, use_key.c_str());
  }
  if (!needle)
    return "";
  needle += use_key.size();
  const char* end = needle + 1;
  while (*end != '\0' && *end != '\n') {
    end++;
  }
  return string(needle, end - needle);
}

}  // namespace

extern "C"
bool ChromeOSUpdate(UpdateInformation* information) {
  scoped_array<char*> envp(new char*[2]);
  envp[0] = strdup("PATH=/bin:/sbin:/usr/bin:/usr/sbin");
  envp[1] = NULL;
  
  scoped_array<char*> argv(new char*[3]);
  argv[0] = strdup("/opt/google/memento_updater/suid_exec");
  argv[1] = strdup("/opt/google/memento_updater/memento_updater.sh");
  argv[2] = NULL;

  gint exit_status = 0;
  gchar* child_stdout = NULL;
  GError* err = NULL;

  gboolean success = g_spawn_sync(NULL,  // Use parent's working dir
                                  argv.get(),
                                  envp.get(),
                                  G_SPAWN_STDERR_TO_DEV_NULL,  // no flags
                                  NULL,  // no setup function
                                  NULL,  // no setup function data
                                  &child_stdout,
                                  NULL,  // stderr pointer
                                  &exit_status,
                                  &err);

  if (!success || exit_status != 0) {
    information->status_ = UPDATE_ERROR;
    if (!success) {
      information->version_ = NewStringCopy(err->message);
    } else {
      information->version_ = NewStringCopy("Nonzero return code");
    }
    return false;
  }

  if (child_stdout && !strcmp(child_stdout, "UPDATED")) {
    information->status_ = UPDATE_SUCCESSFUL;
    information->version_ = NewStringCopy("Updated to new version");
    return true;
  } else {
    information->status_ = UPDATE_ERROR;
    information->version_ = NewStringCopy("didn't update");
    return false;
  }
  // TODO(adlr): handle up to date
  //information->status_ = UPDATE_ALREADY_UP_TO_DATE;
  //information->version_ = NewStringCopy("Didn't update");
}

extern "C"
bool ChromeOSCheckForUpdate(UpdateInformation* information) {
  scoped_array<char*> envp(new char*[2]);
  envp[0] = strdup("PATH=/bin:/sbin:/usr/bin:/usr/sbin");
  envp[1] = NULL;
  
  scoped_array<char*> argv(new char*[3]);
  argv[0] = strdup("/opt/google/memento_updater/suid_exec");
  argv[1] = strdup("/opt/google/memento_updater/ping_omaha.sh");
  argv[2] = NULL;

  gchar* child_stdout = NULL;
  gint exit_status = 0;
  GError* err = NULL;

  gboolean success = g_spawn_sync(NULL,  // Use parent's working dir
                                  argv.get(),
                                  envp.get(),
                                  G_SPAWN_STDERR_TO_DEV_NULL,  // flags
                                  NULL,  // no setup function
                                  NULL,  // no setup function data
                                  &child_stdout,
                                  NULL,  // stderr pointer
                                  &exit_status,
                                  &err);

  if (!success || exit_status != 0) {
    information->status_ = UPDATE_ERROR;
    if (!success) {
      information->version_ = NewStringCopy(err->message);
    } else {
      information->version_ = NewStringCopy("Nonzero return code");
    }
    return false;
  }

  if (strlen(child_stdout) > 0) {
    string new_version = ValueForKey(child_stdout, "NEW_VERSION");

    information->status_ = UPDATE_IS_AVAILABLE;
    information->version_ = NewStringCopy(new_version.c_str());
  } else {
    information->status_ = UPDATE_ALREADY_UP_TO_DATE;
    information->version_ = NewStringCopy("No new version");
  }
  return true;
}

}  // namespace chromeos
