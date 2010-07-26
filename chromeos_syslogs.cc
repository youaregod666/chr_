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
const char kSysLogsScript[] =
    "/usr/share/userfeedback/scripts/sysinfo_script_runner";
const char kMultilineQuote[] = "\"\"\"";
const char kNewLineChars[] = "\r\n";
const char kInvalidLogEntry[] = "<invalid characters in log entry>";

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
  // Trim the leading spaces and tabs. In order to use a multi-line
  // value, you have to place the multi-line quote on the same line as
  // the equal sign.
  //
  // Why not use TrimWhitespace? Consider the following input:
  //
  // KEY1=
  // KEY2=VALUE
  //
  // If we use TrimWhitespace, we will incorrectly trim the new line
  // and assume that KEY1's value is "KEY2=VALUE" rather than empty.
  TrimString(*data, " \t", data);

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
  // Create the temp file, logs will go here.
  if (!file_util::CreateTemporaryFile(temp_filename)) {
    return NULL;
  }

  std::string cmd = std::string(kSysLogsScript) + " >> " +
      temp_filename->value();

  // Ignore the return value - if the script execution didn't work
  // stderr won't go into the output file anyway.
  system(cmd.c_str());

  // Read logs from the temp file
  std::string data;
  if (!file_util::ReadFileToString(FilePath(*temp_filename), &data)) {
    return NULL;
  }

  // Parse the return data into a dictionary
  LogDictionaryType* logs = new LogDictionaryType();
  while (data.length() > 0) {
    std::string key = ReadKey(&data);
    TrimWhitespaceASCII(key, TRIM_ALL, &key);
    if (!key.empty()) {
      std::string value = ReadValue(&data);
      if (IsStringUTF8(value)) {
        TrimWhitespaceASCII(value, TRIM_ALL, &value);
        (*logs)[key] = value;
      } else {
        LOG(WARNING) << "Invalid characters in system log entry: " << key;
        (*logs)[key] = kInvalidLogEntry;
      }
    } else {
      // no more keys, we're done
      break;
    }
  }

  return logs;
}

} // namespace chromeos
