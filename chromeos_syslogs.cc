// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chromeos_syslogs.h" // NOLINT

namespace chromeos { // NOLINT

namespace {
const char* kSysLogsDir = "/usr/bin/syslogs/scripts/";
const char* kTempFileTemplate = "/tmp/SYS_LOGS_XXXXXX";
const char* kAppendRedirector = " >> ";
const char* kWhiteSpace = " \t\n\r";
const char* kMultilineQuote = "\"\"\"";
const char* kNewLineChars = "\r\n";

std::string Trim(const std::string& s) {
  size_t first_non_ws = s.find_first_not_of(kWhiteSpace);
  size_t last_non_ws = s.find_last_not_of(kWhiteSpace);

  if (last_non_ws <= first_non_ws)
    return std::string("");
  return s.substr(first_non_ws, (last_non_ws - first_non_ws));
}

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

std::string ReadValue(std::string *data) {
  size_t first_multi = data->find(kMultilineQuote);

  // if multiline value
  if (first_multi != std::string::npos) {
    data->erase(first_multi, 3);
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
LogDictionaryType* ChromeOSGetSystemLogs(char** const tmpfilename) {
  LogDictionaryType* logs = new LogDictionaryType();

 // get list of scripts in the scripts dir
  DIR* scripts_dir = opendir(kSysLogsDir);
  if (!scripts_dir) return NULL;

  // save current dir then switch to the logs dir
  char* current_dir = get_current_dir_name();
  if (!current_dir) return NULL;

  if (!*tmpfilename) {
    // TODO(rkc): Change this to use mkstemp
    *tmpfilename = new char[strlen(kTempFileTemplate) + 1];
    strcpy(*tmpfilename, kTempFileTemplate);
    mktemp(*tmpfilename);
    if (!*tmpfilename) {
      closedir(scripts_dir);
      delete current_dir;
      return NULL;
    }
  }

  // delete the file if it exists
  remove(*tmpfilename);

  dirent* dir_entry = NULL;
  while (dir_entry = readdir(scripts_dir)) {
    if (dir_entry->d_type == DT_REG) {
      // create the fill command
      char* cmd = new char[strlen(kSysLogsDir) + strlen(dir_entry->d_name) +
                           strlen(kAppendRedirector) +
                           strlen(*tmpfilename) + 1];
      strcpy(cmd, kSysLogsDir); strcat(cmd, dir_entry->d_name);
      strcat(cmd, kAppendRedirector); strcat(cmd, *tmpfilename);

      chdir(kSysLogsDir);
      int ret = system(cmd);
      if (!ret) {
      	closedir(scripts_dir);
	delete current_dir;
	delete cmd;
	return NULL;
      }
      delete cmd;
    }
  }

  // Read in the tmp file
  FILE* tmp = fopen(*tmpfilename, "rt");
  if (!tmp) {
    closedir(scripts_dir);
    delete current_dir;
    return NULL;
  }

  // TODO(rkc): make this faster
  int c = 0;
  std::string data;
  while ((c = fgetc(tmp)) != EOF) {
    data.append(1, (char)c);
  }

  // Then parse the return data into a dictionary
  while (data.length() > 0) {
    std::string key = Trim(ReadKey(&data));
    if (key != "") {
      std::string value = Trim(ReadValue(&data));
      (*logs)[key] = value;
    }
    else {
      // no more keys, we're done
      break;
    }
  }


  // Cleanup:
  chdir(current_dir);
  delete current_dir;
  closedir(scripts_dir);

  return logs;
}


} // namespace chromeos
