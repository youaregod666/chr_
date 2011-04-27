// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TODO(satorux): Remove thie file.

#ifndef CHROMEOS_SYSTEM_H_
#define CHROMEOS_SYSTEM_H_

#include <string>
#include <vector>

namespace chromeos {

struct MachineInfo {
  MachineInfo() : name_value_size(0), name_values(NULL) {}
  struct NVPair {
    char* name;
    char* value;
  };
  int name_value_size;
  NVPair* name_values;
};

// Implementation class. Defined here to be accessable by tests.
class ChromeOSSystem {
 public:
  typedef std::vector<std::pair<std::string, std::string> > NameValuePairs;

  void AddNVPair(const std::string& key, const std::string& value);

  // Execute tool and insert (key, <output>) into nv_pairs_.
  bool GetSingleValueFromTool(const std::string& tool,
                              const std::string& key);
  // Execute tool, parse the output using ParseNVPairs, and insert the results
  // into nv_pairs_.
  bool ParseNVPairsFromTool(const std::string& tool,
                            const std::string& eq, const std::string& delim);
  // Fills in machine_info from nv_pairs_.
  void SetMachineInfo(MachineInfo* machine_info);

  const NameValuePairs& nv_pairs() const { return nv_pairs_; }

 private:
  // This will parse strings with output in the format:
  // <key><EQ><value><DELIM>[<key><EQ><value>][...]
  // e.g. ParseNVPairs("key1=value1 key2=value2", "=", " ")
  bool ParseNVPairs(const std::string& in_string,
                    const std::string& eq,
                    const std::string& delim);
// Similar to file_util::ReadFileToString, but executes a command instead.
  bool ExecCmdToString(const std::string& command, std::string* contents);

  NameValuePairs nv_pairs_;
};

}  // namespace chromeos

#endif  // CHROMEOS_SYSTEM_H_
