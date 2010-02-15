// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The header files provides APIs for monitoring and controlling IME
// status. The APIs encapsulate the APIs of IBus, the underlying IME
// framework.

#ifndef CHROMEOS_IME_H_
#define CHROMEOS_IME_H_

#include <base/basictypes.h>

#include <sstream>
#include <string>
#include <vector>

namespace chromeos {

// The struct represents the IME lookup table (list of candidates).
// Used for ImeUpdateLookupTableMonitorFunction.
struct ImeLookupTable {
  ImeLookupTable()
      : visible(false),
        cursor_absolute_index(0),
        page_size(0) {
  }

  // Returns a string representation of the class. Used for debugging.
  // The function has to be defined here rather than in the .cc file.  If
  // it's defined in the .cc file, the code will be part of libcros.so,
  // which cannot be accessed from clients directly. libcros.so is loaded
  // by dlopen() so all functions are unbound unless explicitly bound by
  // dlsym().
  std::string ToString() const {
    std::stringstream stream;
    stream << "visible: " << visible << "\n";
    stream << "cursor_absolute_index: " << cursor_absolute_index << "\n";
    stream << "page_size: " << page_size << "\n";
    stream << "candidates:";
    for (size_t i = 0; i < candidates.size(); ++i) {
      stream << " " << candidates[i];
    }
    return stream.str();
  }

  // True if the lookup table is visible.
  bool visible;

  // Zero-origin index of the current cursor position in the all
  // candidates. If the cursor is pointing to the third candidate in the
  // second page when the page size is 10, the value will be 12 as it's
  // 13th candidate.
  int cursor_absolute_index;

  // Page size is the max number of candidates shown in a page. Usually
  // it's about 10, depending on the backend conversion engine.
  int page_size;

  // Candidate strings in UTF-8 for all pages.
  std::vector<std::string> candidates;
};

// Callback function type for handling IBus's |HideAuxiliaryText| signal.
typedef void (*ImeHideAuxiliaryTextMonitorFunction)(void* ime_library);

// Callback function type for handling IBus's |HideLookupTable| signal.
typedef void (*ImeHideLookupTableMonitorFunction)(void* ime_library);

// Callback function type for handling IBus's |SetCandidateText| signal.
typedef void (*ImeSetCursorLocationMonitorFunction)(
    void* ime_library,
    int x, int y, int width, int height);

// Callback function type for handling IBus's |UpdateAuxiliaryText| signal.
typedef void (*ImeUpdateAuxiliaryTextMonitorFunction)(void* ime_library,
                                                      const std::string& text,
                                                      bool visible);

// Callback function type for handling IBus's |UpdateLookupTable| signal.
typedef void (*ImeUpdateLookupTableMonitorFunction)(
    void* ime_library,
    const ImeLookupTable& table);

// A set of function pointers used for monitoring the IME status.
struct ImeStatusMonitorFunctions {
  ImeStatusMonitorFunctions()
      : hide_auxiliary_text(NULL),
        hide_lookup_table(NULL),
        set_cursor_location(NULL),
        update_auxiliary_text(NULL),
        update_lookup_table(NULL) {
  }

  ImeHideAuxiliaryTextMonitorFunction hide_auxiliary_text;
  ImeHideLookupTableMonitorFunction hide_lookup_table;
  ImeSetCursorLocationMonitorFunction set_cursor_location;
  ImeUpdateAuxiliaryTextMonitorFunction update_auxiliary_text;
  ImeUpdateLookupTableMonitorFunction update_lookup_table;
};

// Establishes IBus connection to the ibus-daemon.
//
// Returns an ImeStatusConnection object that is used for maintaining and
// monitoring an IBus connection. The implementation details of
// ImeStatusConnection is not exposed.
//
// Function pointers in |monitor_functions| are registered in the returned
// ImeStatusConnection object. These functions will be called, unless the
// pointers are NULL, when certain signals are received from
// ibus-daemon.
//
// The client can pass a pointer to an abitrary object as
// |ime_library|. The pointer passed as |ime_library| will be passed to
// the registered callback functions as the first parameter.
class ImeStatusConnection;
extern ImeStatusConnection* (*MonitorImeStatus)(
    const ImeStatusMonitorFunctions& monitor_functions,
    void* ime_library);

// Disconnects the IME status connection, as well as the underlying IBus
// connection.
extern void (*DisconnectImeStatus)(ImeStatusConnection* connection);

// Notifies that a candidate is clicked. |CandidateClicked| signal will be
// sent to the ibus-daemon.
//
// - |index| Index in the Lookup table. The semantics is same with
//   |cursor_absolute_index|.
// - |button| GdkEventButton::button (1: left button, etc.)
// - |state|  GdkEventButton::state (key modifier flags)
extern void (*NotifyCandidateClicked)(ImeStatusConnection* connection,
                                      int index, int button, int flags);

}  // namespace chromeos

#endif  // CHROMEOS_POWER_H_
