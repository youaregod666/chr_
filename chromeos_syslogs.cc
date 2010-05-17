// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <iostream>

#include <base/string_util.h>
#include <base/file_path.h>
#include <base/file_util.h>
#include <base/logging.h>


#include "chromeos_syslogs.h" // NOLINT

namespace chromeos { // NOLINT

namespace {
const char* kSysLogsDir = "/usr/share/userfeedback/scripts/";
const char* kAppendRedirector = " >> ";
const char* kMultilineQuote = "\"\"\"";
const char* kNewLineChars = "\r\n";

// reads a key from the input string; erasing the read values + delimiters read
// from the initial string 
std::string ReadKey(std::string *data) {
  size_t equal_sign = data->find("=");
  if (equal_sign == std::string::npos)
    return std::string("");
  std::string key = data->substr(0, equal_sign);
  data->erase(0, equal_sign);
  if (data->length() > 0) {
    // erase the equal to sign also
    data->erase(0,1);
    return key;
  }
  return std::string("");
}

// reads a value from the input string; erasing the read values from
// the initial string; detects if the value is multiline and reads
// accordingly
std::string ReadValue(std::string *data) {
  // fast forward past leading whitespaces
  TrimWhitespaceASCII(*data, TRIM_LEADING, data);

  // if multiline value
  if (StartsWithASCII(*data, std::string(kMultilineQuote), false)) {
    data->erase(0, strlen(kMultilineQuote));
    size_t next_multi = data->find(kMultilineQuote);
    if (next_multi == std::string::npos) {
      // error condition, clear data to stop further processing
      data->erase();
      return std::string("");
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
LogDictionaryType* ChromeOSGetSystemLogs(FilePath* tmpfilename) {
  LogDictionaryType* logs = new LogDictionaryType();

  std::cerr << "1" << std::endl;
  // Open scripts directory for listing
  FilePath scripts_dir(kSysLogsDir);
  file_util::FileEnumerator scripts_dir_list(scripts_dir, false,
                                  file_util::FileEnumerator::FILES);
  std::cerr << "1.2" << std::endl;

  // save current dir then switch to the logs dir
  FilePath old_dir;
  if (!file_util::GetCurrentDirectory(&old_dir)) {
    return NULL;
  }
  std::cerr << "1.3" << std::endl;
  std::cerr << old_dir.value() << std::endl;
  std::cerr << "1.5" << std::endl;
  std::cerr << scripts_dir.value() << std::endl;
  if (!file_util::SetCurrentDirectory(scripts_dir)) {
    return NULL;
  }
  std::cerr << "1.6" << std::endl;

  if (!file_util::CreateTemporaryFile(tmpfilename)) {
    return NULL;
  }
  std::cerr << "2" << std::endl;
  std::cerr << scripts_dir.value() << std::endl;
  std::cerr << "2.1" << std::endl;

  dirent* dir_entry = NULL; 
  DIR* dir = opendir(scripts_dir.value().c_str());
  while (dir_entry = readdir(dir)) {
    std::cerr << "2.5" << std::endl;
    if (dir_entry->d_type == DT_REG) {
      std::string cmd = scripts_dir.Append(std::string(dir_entry->d_name)).value() +
                        std::string(kAppendRedirector) +
                        tmpfilename->value();
      std::cerr << "2.6" << std::endl;
      std::cerr << cmd << std::endl;
      std::cerr << "3" << std::endl;
      // Ignore the return value - if the script execution didn't work
      // sterr won't go into the output file anyway
      system(cmd.c_str());
      std::cerr << "4" << std::endl;
    }
  }

  closedir(dir);
  std::cerr << "5" << std::endl;
  std::string data;
  if (!file_util::ReadFileToString(FilePath(*tmpfilename), &data)) {
    return NULL;
  }
  std::cerr << "6" << std::endl;
  std::cerr << data << std::endl;
  std::cerr << "7" << std::endl;

  // Parse the return data into a dictionary
  while (data.length() > 0) {
    std::string key = ReadKey(&data);
    TrimWhitespaceASCII(key, TRIM_ALL, &key);
    if (key != "") {
      std::string value = ReadValue(&data);
      TrimWhitespaceASCII(value, TRIM_ALL, &value);
      (*logs)[key] = value;
    }
    else {
      // no more keys, we're done
      break;
    }
  }
  std::cerr << "8" << std::endl;

  // Cleanup:
  if (!file_util::SetCurrentDirectory(scripts_dir)) {
    return NULL;
  }
  std::cerr << "9" << std::endl;
  for (chromeos::LogDictionaryType::const_iterator i = logs->begin();
              i != logs->end(); i++)
    std::cerr << "Key: " << i->first << " and value: " << i->second << std::endl;
  std::cerr << "0" << std::endl;
  return logs;
}


} // namespace chromeos
