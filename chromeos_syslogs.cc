// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_syslogs.h" // NOLINT

#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <base/string_util.h>
#include <base/file_path.h>
#include <base/file_util.h>
#include <base/logging.h>

namespace chromeos { // NOLINT

namespace {
const char kSysLogsDir[] = "/usr/share/userfeedback/scripts/";
const char kAppendRedirector[] = " >> ";
const char kMultilineQuote[] = "\"\"\"";
const char kNewLineChars[] = "\r\n";

// Reads a key from the input string erasing the read values + delimiters read
// from the initial string
std::string ReadKey(std::string* data) {
  size_t equal_sign = data->find("=");
  if (equal_sign == std::string::npos)
    return std::string("");
  std::string key = data->substr(0, equal_sign);
  data->erase(0, equal_sign);
  if (data->size() > 0) {
    // erase the equal to sign also
    data->erase(0,1);
    return key;
  }
  return std::string();
}

// Reads a value from the input string; erasing the read values from
// the initial string; detects if the value is multiline and reads
// accordingly
std::string ReadValue(std::string* data) {
  // Fast forward past leading whitespaces
  TrimWhitespaceASCII(*data, TRIM_LEADING, data);

  // If multiline value
  if (StartsWithASCII(*data, std::string(kMultilineQuote), false)) {
    data->erase(0, strlen(kMultilineQuote));
    size_t next_multi = data->find(kMultilineQuote);
    if (next_multi == std::string::npos) {
      // Error condition, clear data to stop further processing
      data->erase();
      return std::string();
    }
    std::string value = data->substr(0, next_multi);
    data->erase(0, next_multi + 3);
    return value;
  } else { // single line value
    size_t endl_pos = data->find_first_of(kNewLineChars);
    // if we don't find a new line, we just return the rest of the data
    std::string value = data->substr(0, endl_pos);
    data->erase(0, endl_pos);
    return value;
  }
}
} // namespace


extern "C"
LogDictionaryType* ChromeOSGetSystemLogs(FilePath* temp_filename) {
  // Save current dir then switch to the logs dir
  FilePath old_dir;
  FilePath scripts_dir(kSysLogsDir);

  if (!file_util::GetCurrentDirectory(&old_dir))
    return NULL;
  if (!file_util::SetCurrentDirectory(scripts_dir))
    return NULL;

  // Create the temp file, logs will go here
  if (!file_util::CreateTemporaryFile(temp_filename))
    return NULL;

  // Open scripts directory for listing
  dirent* dir_entry = NULL;
  DIR* dir = opendir(scripts_dir.value().c_str());
  if (!dir)
    return NULL;

  while (dir_entry = readdir(dir)) {
    if (dir_entry->d_type == DT_REG) {
      std::string cmd =
          scripts_dir.Append(std::string(dir_entry->d_name)).value() +
                             std::string(kAppendRedirector) +
                             temp_filename->value();
      // Ignore the return value - if the script execution didn't work
      // sterr won't go into the output file anyway
      system(cmd.c_str());
    }
  }
  closedir(dir);

  // Read logs from the temp file
  std::string data;
  if (!file_util::ReadFileToString(FilePath(*temp_filename), &data)) {
    file_util::SetCurrentDirectory(old_dir);
    return NULL;
  }

  // Parse the return data into a dictionary
  LogDictionaryType* logs = new LogDictionaryType();
  while (data.length() > 0) {
    std::string key = ReadKey(&data);
    TrimWhitespaceASCII(key, TRIM_ALL, &key);
    if (!key.empty()) {
      std::string value = ReadValue(&data);
      TrimWhitespaceASCII(value, TRIM_ALL, &value);
      (*logs)[key] = value;
    }
    else {
      // no more keys, we're done
      break;
    }
  }

  // Cleanup:
  if (!file_util::SetCurrentDirectory(old_dir)) {
    delete logs;
    return NULL;
  }
  return logs;
}

} // namespace chromeos
