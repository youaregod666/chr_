// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>
#include <glib-object.h>
#include <stdlib.h>

#include <iostream>  // NOLINT
#include <vector>

#include <base/at_exit.h>
#include <base/crypto/rsa_private_key.h>
#include <base/crypto/signature_creator.h>
#include <base/crypto/signature_verifier.h>
#include <base/command_line.h>
#include <base/logging.h>
#include <base/file_path.h>
#include <base/file_util.h>
#include <base/nss_util.h>
#include <base/scoped_temp_dir.h>
#include <base/stringprintf.h>

#include "chromeos_cros_api.h"  // NOLINT
#include "chromeos_login.h"
#include "monitor_utils.h" //NOLINT

namespace {
static const char kEmit[] = "emit-login-prompt-ready";
static const char kStartSession[] = "start-session";
static const char kStopSession[] = "stop-session";
static const char kSetOwnerKey[] = "set-owner-key";
static const char kWhitelist[] = "whitelist";
static const char kUnwhitelist[] = "unwhitelist";
static const char kCheckWhitelist[] = "check-whitelist";
static const char kEnumerate[] = "enumerate-whitelisted";

class ClientLoop {
 public:
  ClientLoop()
      : loop_(NULL),
        connection_(NULL) { }

  virtual ~ClientLoop() {
    if (connection_) {
      chromeos::DisconnectSession(connection_);
    }
    if (loop_) {
      g_main_loop_unref(loop_);
    }
  }

  void Initialize() {
    loop_ = g_main_loop_new(NULL, TRUE);
    connection_ = chromeos::MonitorSession(
        reinterpret_cast<chromeos::SessionMonitor>(
            ClientLoop::CallbackThunk), this);
  }

  void Run() {
    g_main_loop_run(loop_);
  }

  chromeos::OwnershipEvent what_happened() { return what_happened_; }

 private:
  void Callback(const chromeos::OwnershipEvent& what_happened) {
    what_happened_ = what_happened;
    g_main_loop_quit(loop_);
  }

  static void CallbackThunk(ClientLoop* client_loop,
                            const chromeos::OwnershipEvent& call_status) {
    client_loop->Callback(call_status);
  }

  GMainLoop *loop_;
  chromeos::OwnershipEvent what_happened_;
  chromeos::SessionConnection connection_;
};

bool LoadPublicKey(FilePath file, std::vector<uint8>* key) {
  int64 file_size;
  if (!file_util::GetFileSize(file, &file_size)) {
    LOG(ERROR) << "Could not get size of " << file.value();
    return false;
  } else {
    LOG(INFO) << "Loaded key of " << file_size << " bytes";
  }
  int file_size_32 = static_cast<int32>(file_size);
  key->resize(file_size_32);

  int data_read = file_util::ReadFile(file,
                                      reinterpret_cast<char*>(&(key->at(0))),
                                      file_size_32);
  return file_size_32 == data_read;
}

// Man, this is ugly.  Better than trying to do it all programmatically, though.
bool GenerateOwnerKey(std::vector<uint8>* key) {
  ScopedTempDir tmpdir;
  FilePath randomness;
  FilePath scratch_file;
  FilePath cert_file;
  if (!tmpdir.CreateUniqueTempDir())
    return false;
  if (!file_util::CreateTemporaryFileInDir(tmpdir.path(), &randomness) ||
      !file_util::CreateTemporaryFileInDir(tmpdir.path(), &scratch_file) ||
      !file_util::CreateTemporaryFileInDir(tmpdir.path(), &cert_file)) {
    return false;
  }
  std::string command =
      base::StringPrintf("head -c 20 /dev/urandom > %s",
                         randomness.value().c_str());
  if (system(command.c_str()))  // get randomness.
    return false;

  command =
      base::StringPrintf("nsscertutil -d 'sql:/home/chronos/user/.pki/nssdb' "
                         "-S -x -n Fake -t 'C,,' -s CN=you -z %s",
                         randomness.value().c_str());
  LOG(INFO) << command;
  if (system(command.c_str()))
    return false;

  command =
      base::StringPrintf("nsspk12util -d 'sql:/home/chronos/user/.pki/nssdb' "
                         "-n Fake -W '' -o %s",
                         scratch_file.value().c_str());
  LOG(INFO) << command;
  if (system(command.c_str()))
    return false;

  command =
      base::StringPrintf("openssl pkcs12 -in %s -passin pass: -passout pass: "
                         "-nokeys"
                         "| openssl x509 -pubkey -noout -outform DER"
                         "| openssl rsa -outform DER -pubin -out %s",
                         scratch_file.value().c_str(),
                         cert_file.value().c_str());
  LOG(INFO) << command;
  if (system(command.c_str()))
    return false;

  return LoadPublicKey(cert_file, key);
}


bool Sign(std::string data, base::RSAPrivateKey* key, std::vector<uint8>* sig) {
  scoped_ptr<base::SignatureCreator> signer(
      base::SignatureCreator::Create(key));
  if (!signer->Update(reinterpret_cast<const uint8*>(data.c_str()),
                      data.length())) {
    return false;
  }
  return signer->Final(sig);
}

base::RSAPrivateKey* GetPrivateKey(FilePath file) {
  std::vector<uint8> pubkey;
  if (!LoadPublicKey(file, &pubkey))
    LOG(FATAL) << "Can't read public key off disk";

  base::EnsureNSSInit();
  base::OpenPersistentNSSDB();
  scoped_ptr<base::RSAPrivateKey> private_key;
  private_key.reset(base::RSAPrivateKey::FindFromPublicKeyInfo(pubkey));
  if (private_key.get())
    LOG(INFO) << "Re-read key data and reloaded private key";
  else
    LOG(FATAL) << "Can't get private key for public key I just created";
  return private_key.release();
}

}  // namespace

int main(int argc, const char** argv) {
  base::AtExitManager exit_manager;
  CommandLine::Init(argc, argv);
  CommandLine *cl = CommandLine::ForCurrentProcess();
  putenv(strdup("HOME=/home/chronos/user"));

  FilePath dir = file_util::GetHomeDir();
  LOG(INFO) << "Homedir is " << dir.value();

  // Initialize the g_type systems an g_main event loop, normally this would be
  // done by chrome.
  ::g_type_init();

  CHECK(LoadCrosLibrary(argv)) << "Failed to load cros .so";

  if (cl->HasSwitch(kEmit)) {
    if (chromeos::EmitLoginPromptReady())
      LOG(INFO) << "Emitted!";
    else
      LOG(FATAL) << "Emitting login-prompt-ready failed.";
  }
  if (cl->HasSwitch(kStartSession)) {
    if (chromeos::StartSession("foo@bar.com", ""))
      LOG(INFO) << "Started session!";
    else
      LOG(FATAL) << "Starting session failed.";
  }

  // Really have to do this after clearing owner key, starting BWSI session.
  // Note that Chrome will get the signal that this has been done and CHECK.
  if (cl->HasSwitch(kSetOwnerKey)) {
    std::vector<uint8> pubkey;
    if (!GenerateOwnerKey(&pubkey))
      LOG(FATAL) << "Couldn't generate fakey owner key";

    ClientLoop client_loop;
    client_loop.Initialize();

    if (!chromeos::SetOwnerKey(pubkey)) {
      LOG(FATAL) << "Could not send SetOwnerKey?";
    }
    client_loop.Run();
    LOG(INFO) << (client_loop.what_happened() == chromeos::SetKeySuccess ?
                  "Successfully set owner key" : "Didn't set owner key");
    exit(0);
  }
  if (cl->HasSwitch(kWhitelist)) {
    scoped_ptr<base::RSAPrivateKey> private_key(
        GetPrivateKey(FilePath(chromeos::kOwnerKeyFile)));

    std::string name = cl->GetSwitchValueASCII(kWhitelist);
    std::vector<uint8> sig;
    if (!Sign(name, private_key.get(), &sig))
      LOG(FATAL) << "Can't sign " << name;
    else
      LOG(INFO) << "Signature is " << sig.size();

    ClientLoop client_loop;
    client_loop.Initialize();

    if (!chromeos::Whitelist(name.c_str(), sig))
      LOG(FATAL) << "Could not send SetOwnerKey?";

    client_loop.Run();
    LOG(INFO) << (client_loop.what_happened() == chromeos::WhitelistOpSuccess ?
                  "Whitelisted " : "Failed to whitelist ") << name;
    exit(0);
  }
  if (cl->HasSwitch(kUnwhitelist)) {
    scoped_ptr<base::RSAPrivateKey> private_key(
        GetPrivateKey(FilePath(chromeos::kOwnerKeyFile)));

    std::string name = cl->GetSwitchValueASCII(kUnwhitelist);
    std::vector<uint8> sig;
    if (!Sign(name, private_key.get(), &sig))
      LOG(FATAL) << "Can't sign " << name;

    ClientLoop client_loop;
    client_loop.Initialize();

    if (!chromeos::Unwhitelist(name.c_str(), sig))
      LOG(FATAL) << "Could not send SetOwnerKey?";

    client_loop.Run();
    LOG(INFO) << (client_loop.what_happened() == chromeos::WhitelistOpSuccess ?
                  "Whitelisted " : "Failed to whitelist ") << name;
    exit(0);
  }
  if (cl->HasSwitch(kEnumerate)) {
    std::vector<std::string> whitelisted;
    if (!chromeos::EnumerateWhitelisted(&whitelisted)) {
      LOG(FATAL) << "Could not enumerate the whitelisted";
    }
    std::vector<std::string>::iterator it;

    for (it = whitelisted.begin(); it < whitelisted.end(); ++it)
      LOG(INFO) << *it << " is whitelisted";

    exit(0);
  }
  if (cl->HasSwitch(kCheckWhitelist)) {
    std::string name = cl->GetSwitchValueASCII(kCheckWhitelist);
    std::vector<uint8> sig;

    if (!chromeos::CheckWhitelist(name.c_str(), &sig))
      LOG(WARNING) << name << " not on whitelist.";
    else
      LOG(INFO) << name << " is on the whitelist.";

    exit(0);
  }

  return 0;
}
