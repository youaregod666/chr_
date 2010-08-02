// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_system.h"

#include <base/command_line.h>
#include <base/file_path.h>
#include <base/file_util.h>
#include <base/logging.h>
#include "base/string_tokenizer.h"
#include "chromeos/string.h"

namespace chromeos {  // NOLINT

namespace { // NOLINT

// The filepath to the timezone file that symlinks to the actual timezone file.
static const char kTimezoneSymlink[] = "/var/lib/timezone/localtime";
static const char kTimezoneSymlink2[] = "/var/lib/timezone/localtime2";

// The directory that contains all the timezone files. So for timezone
// "US/Pacific", the actual timezone file is: "/usr/share/zoneinfo/US/Pacific"
static const char kTimezoneFilesDir[] = "/usr/share/zoneinfo/";

// The system command that returns the hardware class.
static const char kHardwareClassKey[] = "hardware_class";
static const char kHardwareClassTool[] = "/usr/bin/hardware_class";
static const char kUnknownHardwareClass[] = "unknown";

// File with machine hardware info and key/value delimiters.
static const char kMachineHardwareInfoFile[] = "/tmp/machine-info";
static const char kMachineHardwareInfoEq[] = "=";
static const char kMachineHardwareInfoDelim[] = " \n";

// File with machine OS info and key/value delimiters.
static const char kMachineOSInfoFile[] = "/etc/lsb-release";
static const char kMachineOSInfoEq[] = "=";
static const char kMachineOSInfoDelim[] = "\n";

}  // namespace

extern "C"
std::string ChromeOSGetTimezoneID() {
  // Look at kTimezoneSymlink, see which timezone we are symlinked to.
  char buf[256];
  const ssize_t len = readlink(kTimezoneSymlink, buf,
                               sizeof(buf)-1);
  if (len == -1) {
    LOG(ERROR) << "GetTimezoneID: Cannot read timezone symlink "
               << kTimezoneSymlink;
    return std::string();
  }

  std::string timezone(buf, len);
  // Remove kTimezoneFilesDir from the beginning.
  if (timezone.find(kTimezoneFilesDir) != 0) {
    LOG(ERROR) << "GetTimezoneID: Timezone symlink is wrong "
               << timezone;
    return std::string();
  }

  return timezone.substr(strlen(kTimezoneFilesDir));
}

extern "C"
void ChromeOSSetTimezoneID(const std::string& id) {
  // Change the kTimezoneSymlink symlink to the path for this timezone.
  // We want to do this in an atomic way. So we are going to create the symlink
  // at kTimezoneSymlink2 and then move it to kTimezoneSymlink

  FilePath timezone_symlink(kTimezoneSymlink);
  FilePath timezone_symlink2(kTimezoneSymlink2);
  FilePath timezone_file(kTimezoneFilesDir + id);

  // Make sure timezone_file exists.
  if (!file_util::PathExists(timezone_file)) {
    LOG(ERROR) << "SetTimezoneID: Cannot find timezone file "
               << timezone_file.value();
    return;
  }

  // Delete old symlink2 if it exists.
  file_util::Delete(timezone_symlink2, false);

  // Create new symlink2.
  if (symlink(timezone_file.value().c_str(),
              timezone_symlink2.value().c_str()) == -1) {
    LOG(ERROR) << "SetTimezoneID: Unable to create symlink "
               << timezone_symlink2.value() << " to " << timezone_file.value();
    return;
  }

  // Move symlink2 to symlink.
  if (!file_util::ReplaceFile(timezone_symlink2, timezone_symlink)) {
    LOG(ERROR) << "SetTimezoneID: Unable to move symlink "
               << timezone_symlink2.value() << " to "
               << timezone_symlink.value();
  }
}

// This will parse files with output in the format:
// <key><EQ><value><DELIM>[<key><EQ><value>][...]
// e.g. key1=value1 key2=value2
static bool ParseNVPairs(
    const std::string& in_string,
    const std::string& eq,
    const std::string& delim,
    std::vector<std::pair<std::string, std::string> >* nvpairs) {
  // Set up the pair tokenizer.
  StringTokenizer pair_toks(in_string, delim);
  pair_toks.set_quote_chars("\"");
  // Process token pairs.
  std::vector<std::pair<std::string, std::string> > new_pairs;
  while (pair_toks.GetNext()) {
    std::string pair(pair_toks.token());
    StringTokenizer keyvalue(pair, eq);
    std::string key,value;
    if (keyvalue.GetNext()) {
      key = keyvalue.token();
      if (keyvalue.GetNext()) {
        value = keyvalue.token();
      }
    }
    if (key.empty()) {
      LOG(WARNING) << "Invalid token pair: '" << pair << "'. Aborting.";
      return false;
    }
    new_pairs.push_back(std::make_pair(key, value));
  }
  nvpairs->insert(nvpairs->end(), new_pairs.begin(), new_pairs.end());
  return true;
}

// Similar to file_util::ReadFileToString, but executes a command using popen.
static bool ExecCmdToString(const std::string& command, std::string* contents) {
  FILE* fp = popen(command.c_str(), "r");
  if (!fp) {
    return false;
  }
  char buf[1 << 16];
  size_t len;
  while ((len = fread(buf, 1, sizeof(buf), fp)) > 0) {
    contents->append(buf, len);
  }
  pclose(fp);
  return true;
}

extern "C"
MachineInfo* ChromeOSGetMachineInfo() {
  MachineInfo* machine_info = new MachineInfo();
  if (!machine_info)
    return NULL;

  // Ensure the struct is initialized.
  machine_info->name_value_size = 0;
  machine_info->name_values = NULL;

  std::vector<std::pair<std::string, std::string> > nv_pairs;

  {
    // Get the hardware class.
    std::string hardware_class = kUnknownHardwareClass;
    if (ExecCmdToString(kHardwareClassTool, &hardware_class)) {
      TrimWhitespaceASCII(hardware_class, TRIM_ALL, &hardware_class);
    }
    nv_pairs.push_back(std::make_pair(std::string(kHardwareClassKey),
                                      hardware_class));
  }

  {
    // Get additional name value pairs from machine info files.
    FilePath output_file(kMachineHardwareInfoFile);
    std::string eq = kMachineHardwareInfoEq;
    std::string delim = kMachineHardwareInfoDelim;
    std::string output_string;
    if (file_util::ReadFileToString(output_file, &output_string)) {
      if (!ParseNVPairs(output_string, eq, delim, &nv_pairs))
        LOG(WARNING) << "Error parsing MachineInfo in: " + output_file.value();
    }
    output_file = FilePath(kMachineOSInfoFile);
    eq = kMachineOSInfoEq;
    delim = kMachineOSInfoDelim;
    output_string.clear();
    if (file_util::ReadFileToString(output_file, &output_string)) {
      if (!ParseNVPairs(output_string, eq, delim, &nv_pairs))
        LOG(WARNING) << "Error parsing MachineInfo in: " + output_file.value();
    }
  }

  // Set the MachineInfo struct.
  machine_info->name_value_size = nv_pairs.size();
  machine_info->name_values = new MachineInfo::NVPair[nv_pairs.size()];
  std::vector<std::pair<std::string, std::string> >::iterator iter;
  int idx = 0;
  for (iter = nv_pairs.begin(); iter != nv_pairs.end(); ++iter) {
    machine_info->name_values[idx].name = NewStringCopy(iter->first.c_str());
    machine_info->name_values[idx].value = NewStringCopy(iter->second.c_str());
    ++idx;
  }

  return machine_info;
}

extern "C"
void ChromeOSFreeMachineInfo(MachineInfo* info) {
  if (info) {
    for (int i=0; i<info->name_value_size; ++i) {
      delete info->name_values[i].name;
      delete info->name_values[i].value;
    }
    delete info->name_values;
    delete info;
  }
}


}  // namespace chromeos
