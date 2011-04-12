// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>
#include <glib-object.h>

#include <iostream>  // NOLINT

#include <base/command_line.h>
#include <base/logging.h>
#include <base/string_util.h>

#include "chromeos_cryptohome.h"
#include "monitor_utils.h" //NOLINT

static const char kDoUnmount[] = "do-unmount";
static const char kTpmStatusStl[] = "tpm-status-stl";
static const char kTpmStatus[] = "tpm-status";
static const char kCheckKey[] = "check-key";
static const char kTestMountStl[] = "test-mount-stl";
static const char kTestMount[] = "test-mount";
static const char kChangeKey[] = "change-key";
static const char kMountGuest[] = "mount-guest";
static const char kRemove[] = "remove";
static const char kStatus[] = "status";
static const char kStatusStl[] = "status-stl";
static const char kAsync[] = "async";
static const char kOwnTpm[] = "own-tpm";
static const char kClearPass[] = "clear-tpmpass";
static const char kGetSalt[] = "get-salt";

class ClientLoop {
 public:
  ClientLoop()
      : loop_(NULL),
      async_call_id_(0),
      return_status_(false),
      return_code_(0),
      connection_(NULL) { }

  virtual ~ClientLoop() {
    if (connection_) {
      chromeos::CryptohomeDisconnectSession(connection_);
    }
    if (loop_) {
      g_main_loop_unref(loop_);
    }
  }

  void Initialize() {
    loop_ = g_main_loop_new(NULL, TRUE);
    connection_ = chromeos::CryptohomeMonitorSession(
        reinterpret_cast<chromeos::CryptohomeSignalCallback>(
            ClientLoop::CallbackThunk), this);
  }

  void Run(int async_call_id) {
    async_call_id_ = async_call_id;
    g_main_loop_run(loop_);
  }

  bool get_return_status() {
    return return_status_;
  }

  int get_return_code() {
    return return_code_;
  }

 private:
  void Callback(const chromeos::CryptohomeAsyncCallStatus& call_status) {
    if (call_status.async_id == async_call_id_) {
      return_status_ = call_status.return_status;
      return_code_ = call_status.return_code;
      g_main_loop_quit(loop_);
    }
  }

  static void CallbackThunk(
      const chromeos::CryptohomeAsyncCallStatus& call_status,
      ClientLoop* client_loop) {
    client_loop->Callback(call_status);
  }

  GMainLoop *loop_;
  int async_call_id_;
  bool return_status_;
  int return_code_;
  void* connection_;
};

int main(int argc, const char** argv) {
  CommandLine::Init(argc, argv);
  CommandLine *cl = CommandLine::ForCurrentProcess();
  std::vector<std::string> loose_args = cl->args();

  // Initialize the g_type system
  ::g_type_init();

  CHECK(LoadCrosLibrary(argv)) << "Failed to load cros .so";

  if (cl->HasSwitch(kTpmStatus) || cl->HasSwitch(kTpmStatusStl)) {
    LOG(INFO) << "TPM Enabled: " << chromeos::CryptohomeTpmIsEnabled();
    LOG(INFO) << "TPM Ready: " << chromeos::CryptohomeTpmIsReady();
    LOG(INFO) << "TPM Owned: " << chromeos::CryptohomeTpmIsOwned();
    LOG(INFO) << "TPM Being Owned: " << chromeos::CryptohomeTpmIsBeingOwned();
    LOG(INFO) << "PKCS11 TPM Token Ready: "
              << chromeos::CryptohomePkcs11IsTpmTokenReady();
    if (cl->HasSwitch(kTpmStatus)) {
      char* tpm_password;
      chromeos::CryptohomeTpmGetPasswordSafe(&tpm_password);
      LOG(INFO) << "TPM Password: " << tpm_password;
      chromeos::CryptohomeFreeString(tpm_password);
    } else {
      std::string token_label;
      std::string token_user_pin;
      chromeos::CryptohomePkcs11GetTpmTokenInfo(&token_label, &token_user_pin);
      LOG(INFO) << "PKCS11 TPM Token Info: label: " << token_label << ", "
                << "user PIN: " << token_user_pin;
      std::string tpm_password;
      chromeos::CryptohomeTpmGetPassword(&tpm_password);
      LOG(INFO) << "TPM Password: " << tpm_password;
    }
  }

  if (cl->HasSwitch(kGetSalt)) {
    char* salt;
    int length;
    chromeos::CryptohomeGetSystemSaltSafe(&salt, &length);
    std::string salt_string;
    const char digits[] = "0123456789abcdef";
    for (int i = 0; i < length; i++) {
      salt_string += digits[(salt[i] >> 4) & 0xf];
      salt_string += digits[salt[i] & 0xf];
    }
    LOG(INFO) << "Salt: " << salt_string;
    chromeos::CryptohomeFreeBlob(salt);
  }

  if (cl->HasSwitch(kStatus)) {
    char* status;
    chromeos::CryptohomeGetStatusStringSafe(&status);
    LOG(INFO) << "Cryptohome Status: \n" << status;
    chromeos::CryptohomeFreeString(status);
  }
  if (cl->HasSwitch(kStatusStl)) {
    std::string status;
    chromeos::CryptohomeGetStatusString(&status);
    LOG(INFO) << "Cryptohome Status: \n" << status;
  }

  if (cl->HasSwitch(kOwnTpm)) {
    chromeos::CryptohomeTpmCanAttemptOwnership();
  }

  if (cl->HasSwitch(kClearPass)) {
    chromeos::CryptohomeTpmClearStoredPassword();
  }

  if (cl->HasSwitch(kCheckKey)) {
    std::string name = loose_args[0];
    std::string hash = loose_args[1];
    if (cl->HasSwitch(kAsync)) {
      ClientLoop client_loop;
      client_loop.Initialize();
      int async_id = chromeos::CryptohomeAsyncCheckKey(name.c_str(),
                                                       hash.c_str());
      if (async_id > 0) {
        client_loop.Run(async_id);
        if (client_loop.get_return_status()) {
          LOG(INFO) << "Credentials are good";
        } else {
          LOG(INFO) << "Credentials are no good on this device";
        }
      } else {
        LOG(ERROR) << "Failed to call AsyncCheckKey";
      }
    } else {
      LOG(INFO) << "Trying " << name << " " << hash;
      CHECK(chromeos::CryptohomeCheckKey(name.c_str(), hash.c_str()))
          << "Credentials are no good on this device";
    }
  }
  if (cl->HasSwitch(kTestMount) || cl->HasSwitch(kTestMountStl)) {
    std::string name = loose_args[0];
    std::string hash = loose_args[1];
    // TODO(fes): Differentiate the two calls
    std::vector<std::string> tracked_dirs;
    const char** tracked_dirs_array = NULL;
    if (loose_args.size() > 2) {
      tracked_dirs.assign(loose_args.begin() + 2, loose_args.end());
      if (tracked_dirs.size() != 0) {
        tracked_dirs_array = new const char*[tracked_dirs.size() + 1];
        unsigned int i;
        for (i = 0; i < tracked_dirs.size(); i++) {
          tracked_dirs_array[i] = tracked_dirs[i].c_str();
        }
        tracked_dirs_array[i] = NULL;
      }
    }
    if (cl->HasSwitch(kAsync)) {
      ClientLoop client_loop;
      client_loop.Initialize();
      do {
        int async_id;
        if (cl->HasSwitch(kTestMount)) {
          async_id = chromeos::CryptohomeAsyncMountSafe(
              name.c_str(),
              hash.c_str(),
              true,
              (tracked_dirs.size() != 0),
              tracked_dirs_array);
        } else {
          async_id = chromeos::CryptohomeAsyncMount(
              name.c_str(),
              hash.c_str(),
              true,
              (tracked_dirs.size() != 0),
              tracked_dirs);
        }
        if (async_id <= 0) {
          LOG(ERROR) << "Failed to call AsyncMount";
          break;
        }
        client_loop.Run(async_id);
        if (!client_loop.get_return_status()) {
          LOG(INFO) << "AsyncMount returned false";
          break;
        }
        LOG(INFO) << "AsyncMount success";
        if (cl->HasSwitch(kDoUnmount)) {
          CHECK(chromeos::CryptohomeUnmount())
              << "Cryptohome cannot be unmounted???";
        }
      } while(false);
    } else {
      int mount_error;
      if (cl->HasSwitch(kTestMount)) {
        CHECK(chromeos::CryptohomeMountSafe(name.c_str(), hash.c_str(),
                                            true,
                                            (tracked_dirs.size() != 0),
                                            tracked_dirs_array,
                                            &mount_error))
            << "Cannot mount cryptohome for " << name;
      } else {
        CHECK(chromeos::CryptohomeMount(name.c_str(), hash.c_str(),
                                        true,
                                        (tracked_dirs.size() != 0),
                                        tracked_dirs,
                                        &mount_error))
            << "Cannot mount cryptohome for " << name;
      }
      CHECK(chromeos::CryptohomeIsMounted())
          << "Cryptohome was mounted, but is now gone???";
      if (cl->HasSwitch(kDoUnmount)) {
        CHECK(chromeos::CryptohomeUnmount())
            << "Cryptohome cannot be unmounted???";
      }
    }
    if (tracked_dirs_array) {
      delete tracked_dirs_array;
    }
  }
  if (cl->HasSwitch(kChangeKey)) {
    std::string name = loose_args[0];
    std::string hash = loose_args[1];
    std::string new_hash = loose_args[2];
    if (cl->HasSwitch(kAsync)) {
      ClientLoop client_loop;
      client_loop.Initialize();
      do {
        int async_id = chromeos::CryptohomeAsyncMigrateKey(name.c_str(),
                                                           hash.c_str(),
                                                           new_hash.c_str());
        if (async_id <= 0) {
          LOG(ERROR) << "Failed to call AsyncMigrateKey";
          break;
        }
        client_loop.Run(async_id);
        if (!client_loop.get_return_status()) {
          LOG(INFO) << "AsyncMigrateKey returned false";
          break;
        }
        LOG(INFO) << "AsyncMigrateKey success";
      } while(false);
    } else {
      CHECK(chromeos::CryptohomeMigrateKey(name.c_str(), hash.c_str(),
                                           new_hash.c_str()))
          << "Cannot migrate key for " << name;
    }
  }
  if (cl->HasSwitch(kRemove)) {
    std::string name = loose_args[0];
    if (cl->HasSwitch(kAsync)) {
      ClientLoop client_loop;
      client_loop.Initialize();
      do {
        int async_id = chromeos::CryptohomeAsyncRemove(name.c_str());
        if (async_id <= 0) {
          LOG(ERROR) << "Failed to call AsyncRemove";
          break;
        }
        client_loop.Run(async_id);
        if (!client_loop.get_return_status()) {
          LOG(INFO) << "AsyncRemove returned false";
          break;
        }
        LOG(INFO) << "AsyncRemove success";
      } while(false);
    } else {
      CHECK(chromeos::CryptohomeRemove(name.c_str()))
          << "Cannot remove cryptohome for " << name;
    }
  }
  if (cl->HasSwitch(kMountGuest)) {
    if (cl->HasSwitch(kAsync)) {
      ClientLoop client_loop;
      client_loop.Initialize();
      do {
        int async_id = chromeos::CryptohomeAsyncMountGuest();
        if (async_id <= 0) {
          LOG(ERROR) << "Failed to call AsyncMountGuest";
          break;
        }
        client_loop.Run(async_id);
        if (!client_loop.get_return_status()) {
          LOG(INFO) << "AsyncMountGuest returned false";
          break;
        }
        LOG(INFO) << "AsyncMountGuest success";
        if (cl->HasSwitch(kDoUnmount)) {
          CHECK(chromeos::CryptohomeUnmount())
              << "Cryptohome cannot be unmounted???";
        }
      } while(false);
    } else {
      int mount_error;
      CHECK(chromeos::CryptohomeMountGuest(&mount_error))
          << "Cannot mount guest";
      if (cl->HasSwitch(kDoUnmount)) {
        CHECK(chromeos::CryptohomeUnmount())
            << "Cryptohome cannot be unmounted???";
      }
    }
  }
  return 0;
}
