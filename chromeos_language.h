// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LANGUAGE_H_
#define CHROMEOS_LANGUAGE_H_

#include <string>
#include <vector>

#include <base/basictypes.h>

static const char kFallbackXKBId[] = "USA";
static const char kFallbackXKBDisplayName[] = "US";

namespace chromeos {

enum LanguageCategory {
  LANGUAGE_CATEGORY_XKB,
  LANGUAGE_CATEGORY_IME,
};

// A structure which represents an IME language or a XKB layout.
struct InputLanguage {
  InputLanguage(LanguageCategory category,
                const std::string& id,
                const std::string& display_name,
                const std::string& icon_path)
      : category(category),
        id(id),
        display_name(display_name),
        icon_path(icon_path) {
  }

  InputLanguage()
      : category(LANGUAGE_CATEGORY_XKB) {
  }

  // Languages are sorted by category, then by display_name, then by id.
  bool operator<(const InputLanguage& other) const {
    if (category == other.category) {
      if (display_name == other.display_name) {
        return id < other.id;
      }
      return display_name < other.display_name;
    }
    return category < other.category;
  }

  bool operator==(const InputLanguage& other) const {
    return (category == other.category) && (id == other.id);
  }

  LanguageCategory category;

  // An ID that identifies an IME engine or a XKB layout (e.g., "anthy",
  // "t:latn-post", "chewing").
  std::string id;
  // An IME or layout name which is used in the UI (e.g., "Anthy").
  std::string display_name;
  // Path to an icon (e.g., "/usr/share/ibus-chewing/icons/ibus-chewing.png").
  // Empty if it does not exist.
  std::string icon_path;
};
typedef std::vector<InputLanguage> InputLanguageList;

// Creates dummy InputLanguageList object. Usually, this function is called only
// on development enviromnent where libcros.so does not exist. So, obviously
// you can not move this function to libcros.so. This function is called by
// src/chrome/browser/chromeos/language_library.cc when EnsureLoaded() fails.
inline InputLanguageList* CreateFallbackInputLanguageList() {
  InputLanguageList* language_list = new InputLanguageList;
  language_list->push_back(
      InputLanguage(LANGUAGE_CATEGORY_XKB,
                    kFallbackXKBId,
                    kFallbackXKBDisplayName,
                    "" /* no icon */));
  return language_list;
}

class LanguageStatusConnection;
typedef void(*LanguageStatusMonitorFunction)(
    void* language_library, const InputLanguage& current_language);

// Establishes IBus connection to the ibus-daemon and DBus connection to
// the candidate window process. |monitor_function| will be called when IME
// language or XKB layout is changed.
extern LanguageStatusConnection* (*MonitorLanguageStatus)(
    LanguageStatusMonitorFunction monitor_funcion, void* language_library);

// Terminates IBus and DBus connections.
extern void (*DisconnectLanguageStatus)(LanguageStatusConnection* connection);

// Gets all IME languages and XKB layouts that are currently active. Caller
// has to delete the returned list. This function might return NULL on error.
extern InputLanguageList* (*GetLanguages)(LanguageStatusConnection* connection);

// Gets all IME languages and XKB layouts that are supported, including ones
// not active. Caller has to delete the returned list. This function might
// return NULL on error.
extern InputLanguageList* (*GetSupportedLanguages)(LanguageStatusConnection*
                                                   connection);

// Changes the current IME engine to |name| and enable IME (when |category| is
// LANGUAGE_CATEGORY_IME). Changes the current XKB layout to |name| and disable
// IME (when |category| is LANGUAGE_CATEGORY_XKB).
extern void (*ChangeLanguage)(LanguageStatusConnection* connection,
                              LanguageCategory category, const char* name);

// Activates the language specified by |category| and |name|. Returns true on
// success.
extern bool (*ActivateLanguage)(LanguageStatusConnection* connection,
                                LanguageCategory category, const char* name);

// Deactivates the language specified by |category| and |name|. Returns true
// on success.
extern bool (*DeactivateLanguage)(LanguageStatusConnection* connection,
                                  LanguageCategory category, const char* name);

}  // namespace chromeos

#endif  // CHROMEOS_POWER_H_
