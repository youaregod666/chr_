// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_system.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
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

// Command to get machine hardware info and key/value delimiters.
static const char kMachineHardwareInfoTool[] = "cat /tmp/machine-info";
static const char kMachineHardwareInfoEq[] = "=";
static const char kMachineHardwareInfoDelim[] = " \n";

// Command to get machine OS info and key/value delimiters.
static const char kMachineOSInfoTool[] = "cat /etc/lsb-release";
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

void ChromeOSSystem::AddNVPair(const std::string& key,
                               const std::string& value) {
  nv_pairs_.push_back(std::make_pair(key,value));
}

bool ChromeOSSystem::ParseNVPairs(const std::string& in_string,
                                  const std::string& eq,
                                  const std::string& delim) {
  // Set up the pair tokenizer.
  StringTokenizer pair_toks(in_string, delim);
  pair_toks.set_quote_chars("\"");
  // Process token pairs.
  NameValuePairs new_pairs;
  while (pair_toks.GetNext()) {
    std::string pair(pair_toks.token());
    if (pair.find(eq) == 0) {
      LOG(WARNING) << "Empty key: '" << pair << "'. Aborting.";
      return false;
    }
    StringTokenizer keyvalue(pair, eq);
    std::string key,value;
    if (keyvalue.GetNext()) {
      key = keyvalue.token();
      if (keyvalue.GetNext()) {
        value = keyvalue.token();
        if (keyvalue.GetNext()) {
          LOG(WARNING) << "Multiple key tokens: '" << pair << "'. Aborting.";
          return false;
        }
      }
    }
    if (key.empty()) {
      LOG(WARNING) << "Invalid token pair: '" << pair << "'. Aborting.";
      return false;
    }
    new_pairs.push_back(std::make_pair(key, value));
  }
  nv_pairs_.insert(nv_pairs_.end(), new_pairs.begin(), new_pairs.end());
  return true;
}

bool ChromeOSSystem::ExecCmdToString(const std::string& command,
                                     std::string* contents) {
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

bool ChromeOSSystem::GetSingleValueFromTool(const std::string& tool,
                                            const std::string& key) {
  std::string output_string;
  if (tool.empty() || !ExecCmdToString(tool, &output_string)) {
    LOG(WARNING) << "Error excuting: " << tool;
    return false;
  }
  TrimWhitespaceASCII(output_string, TRIM_ALL, &output_string);
  nv_pairs_.push_back(std::make_pair(key, output_string));
  return true;
}

bool ChromeOSSystem::ParseNVPairsFromTool(const std::string& tool,
                                          const std::string& eq,
                                          const std::string& delim) {
  std::string output_string;
  if (tool.empty() || !ExecCmdToString(tool, &output_string)) {
    LOG(WARNING) << "Error excuting: " << tool;
    return false;
  }
  if (!ParseNVPairs(output_string, eq, delim)) {
    LOG(WARNING) << "Error parsing values while excuting: " << tool;
    return false;
  }
  return true;
}

void ChromeOSSystem::SetMachineInfo(MachineInfo* machine_info) {
  // Set the MachineInfo struct.
  machine_info->name_value_size = nv_pairs_.size();
  machine_info->name_values = new MachineInfo::NVPair[nv_pairs_.size()];
  int idx = 0;
  for (NameValuePairs::iterator iter = nv_pairs_.begin();
       iter != nv_pairs_.end(); ++iter) {
    machine_info->name_values[idx].name = NewStringCopy(iter->first.c_str());
    machine_info->name_values[idx].value = NewStringCopy(iter->second.c_str());
    ++idx;
  }
}

extern "C"
MachineInfo* ChromeOSGetMachineInfo() {
  MachineInfo* machine_info = new MachineInfo();
  if (!machine_info)
    return NULL;

  ChromeOSSystem system;
  if (!system.GetSingleValueFromTool(kHardwareClassTool, kHardwareClassKey)) {
    // Use kUnknownHardwareClass if the hardware class command fails.
    system.AddNVPair(kHardwareClassKey, kUnknownHardwareClass);
  }
  system.ParseNVPairsFromTool(kMachineHardwareInfoTool,
                              kMachineHardwareInfoEq,
                              kMachineHardwareInfoDelim);
  system.ParseNVPairsFromTool(kMachineOSInfoTool,
                              kMachineOSInfoEq,
                              kMachineOSInfoDelim);
  system.SetMachineInfo(machine_info);
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
