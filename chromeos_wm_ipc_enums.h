// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_WM_IPC_ENUMS_H_
#define CHROMEOS_WM_IPC_ENUMS_H_

// This file defines enums that are shared between Chrome and the Chrome OS
// window manager.

namespace chromeos {

// A window's _CHROME_WINDOW_TYPE property contains a sequence of 32-bit
// integer values that inform the window manager how the window should be
// treated.  This enum lists the possible values for the first element in a
// property.  If other elements are required after the window type, they
// are listed here; for example, param[0] indicates the second value that
// should appear in the property, param[1] the third, and so on.
//
// Do not re-use values -- this list is shared between multiple processes.
enum WmIpcWindowType {
  // A non-Chrome window, or one that doesn't need to be handled in any
  // special way by the window manager.
  WM_IPC_WINDOW_UNKNOWN = 0,

  // A top-level Chrome window.
  //   param[0]: The number of tabs currently in this Chrome window.
  //   param[1]: The index of the currently selected tab in this
  //             Chrome window.
  WM_IPC_WINDOW_CHROME_TOPLEVEL = 1,

  // TODO: Delete these.
  DEPRECATED_WM_IPC_WINDOW_CHROME_TAB_SUMMARY = 2,
  DEPRECATED_WM_IPC_WINDOW_CHROME_FLOATING_TAB = 3,

  // The contents of a popup window.
  //   param[0]: X ID of associated titlebar, which must be mapped before
  //             its content.
  //   param[1]: Initial state for panel (0 is collapsed, 1 is expanded).
  WM_IPC_WINDOW_CHROME_PANEL_CONTENT = 4,

  // A small window placed above the panel's contents containing its title
  // and a close button.
  WM_IPC_WINDOW_CHROME_PANEL_TITLEBAR = 5,

  // TODO: Delete this.
  DEPRECATED_WM_IPC_WINDOW_CREATE_BROWSER_WINDOW = 6,

  // A Chrome info bubble (e.g. the bookmark bubble).  These are
  // transient RGBA windows; we skip the usual transient behavior of
  // centering them over their owner and omit drawing a drop shadow.
  WM_IPC_WINDOW_CHROME_INFO_BUBBLE = 7,

  // A window showing a view of a tab within a Chrome window.
  //   param[0]: X ID of toplevel window that owns it.
  //   param[1]: Index of this tab in the toplevel window that owns it.
  WM_IPC_WINDOW_CHROME_TAB_SNAPSHOT = 8,

  // The following types are used for the windows that represent a user that
  // has already logged into the system.
  //
  // Visually the BORDER contains the IMAGE and CONTROLS windows, the LABEL
  // and UNSELECTED_LABEL are placed beneath the BORDER. The LABEL window is
  // onscreen when the user is selected, otherwise the UNSELECTED_LABEL is
  // on screen. The GUEST window is used when the user clicks on the entry
  // that represents the 'guest' user.
  //
  // The following parameters are set for these windows (except GUEST and
  // BACKGROUND):
  //   param[0]: Visual index of the user the window corresponds to.
  //             For example, all windows with an index of 0 occur first,
  //             followed by windows with an index of 1...
  //
  // The following additional params are set on the first BORDER window
  // (BORDER window whose param[0] == 0).
  //   param[1]: Total number of users.
  //   param[2]: Size of the unselected image.
  //   param[3]: Gap between image and controls.
  //
  // The following param is set on the BACKGROUND window:
  //   param[0]: Whether Chrome has finished painting the background
  //             (1 means "yes").
  WM_IPC_WINDOW_LOGIN_BORDER = 9,
  WM_IPC_WINDOW_LOGIN_IMAGE = 10,
  WM_IPC_WINDOW_LOGIN_CONTROLS = 11,
  WM_IPC_WINDOW_LOGIN_LABEL = 12,
  WM_IPC_WINDOW_LOGIN_UNSELECTED_LABEL = 13,
  WM_IPC_WINDOW_LOGIN_GUEST = 14,
  WM_IPC_WINDOW_LOGIN_BACKGROUND = 15,

  // NEXT VALUE TO USE: 16
};

// Messages are sent via ClientMessage events that have 'message_type' set
// to _CHROME_WM_MESSAGE, 'format' set to 32 (that is, 32-bit values), and
// l[0] set to a value from the WmIpcMessageType enum.  The remaining four
// values in the 'l' array contain data appropriate to the type of message
// being sent.
//
// Message names should take the form:
//   WM_IPC_MESSAGE_<recipient>_<description>
//
// Do not re-use values -- this list is shared between multiple processes.
enum WmIpcMessageType {
  WM_IPC_MESSAGE_UNKNOWN = 0,

  // TODO: Delete these.
  DEPRECATED_WM_IPC_MESSAGE_CHROME_NOTIFY_FLOATING_TAB_OVER_TAB_SUMMARY = 1,
  DEPRECATED_WM_IPC_MESSAGE_CHROME_NOTIFY_FLOATING_TAB_OVER_TOPLEVEL = 2,
  DEPRECATED_WM_IPC_MESSAGE_CHROME_SET_TAB_SUMMARY_VISIBILITY = 3,

  // Tell the WM to collapse or expand a panel.
  //   param[0]: X ID of the panel window.
  //   param[1]: Desired state (0 means collapsed, 1 means expanded).
  WM_IPC_MESSAGE_WM_SET_PANEL_STATE = 4,

  // Notify Chrome that the panel state has changed.  Sent to the panel
  // window.
  //   param[0]: new state (0 means collapsed, 1 means expanded).
  // TODO: Deprecate this; Chrome can just watch for changes to the
  // _CHROME_STATE property to get the same information.
  WM_IPC_MESSAGE_CHROME_NOTIFY_PANEL_STATE = 5,

  // TODO: Delete this.
  DEPRECATED_WM_IPC_MESSAGE_WM_MOVE_FLOATING_TAB = 6,

  // Notify the WM that a panel has been dragged.
  //   param[0]: X ID of the panel's content window.
  //   param[1]: X coordinate to which the upper-right corner of the
  //             panel's titlebar window was dragged.
  //   param[2]: Y coordinate to which the upper-right corner of the
  //             panel's titlebar window was dragged.
  // Note: The point given is actually that of one pixel to the right
  // of the upper-right corner of the titlebar window.  For example, a
  // no-op move message for a 10-pixel wide titlebar whose upper-left
  // point is at (0, 0) would contain the X and Y paremeters (10, 0):
  // in other words, the position of the titlebar's upper-left point
  // plus its width.  This is intended to make both the Chrome and WM
  // side of things simpler and to avoid some easy-to-make off-by-one
  // errors.
  WM_IPC_MESSAGE_WM_NOTIFY_PANEL_DRAGGED = 7,

  // Notify the WM that the panel drag is complete (that is, the mouse
  // button has been released).
  //   param[0]: X ID of the panel's content window.
  WM_IPC_MESSAGE_WM_NOTIFY_PANEL_DRAG_COMPLETE = 8,

  // TODO: Delete this.
  WM_IPC_MESSAGE_DEPRECATED_WM_FOCUS_WINDOW = 9,

  // Notify Chrome that the layout mode (for example, overview or
  // active) has changed.
  //   param[0]: New mode (0 means active, 1 means overview).
  //   param[1]: Was the mode cancelled? (0 means no, 1 means yes).
  WM_IPC_MESSAGE_CHROME_NOTIFY_LAYOUT_MODE = 10,

  // TODO: Delete this.
  DEPRECATED_WM_IPC_MESSAGE_WM_SWITCH_TO_OVERVIEW_MODE = 11,

  // Let the WM know which version of this file Chrome is using.  It's
  // difficult to make changes synchronously to Chrome and the WM (our
  // build scripts can use a locally-built Chromium, the latest one
  // from the buildbot, or an older hardcoded version), so it's useful
  // to be able to maintain compatibility in the WM with versions of
  // Chrome that exhibit older behavior.
  //
  // Chrome should send a message to the WM at (the WM's) startup
  // containing the version number from the list below describing the
  // behavior that it implements.  For backwards compatibility, the WM
  // assumes version 0 if it doesn't receive a message.  Here are the
  // changes that have been made in successive versions of the protocol:
  //
  // 1: WM_NOTIFY_PANEL_DRAGGED contains the position of the
  //    upper-right, rather than upper-left, corner of of the titlebar
  //    window
  //
  //   param[0]: Version of this protocol currently supported by Chrome.
  WM_IPC_MESSAGE_WM_NOTIFY_IPC_VERSION = 12,

  // Notify Chrome when a tab has been selected in the overview.  Sent to the
  // toplevel window associated with the magnified tab.
  //   param[0]: Tab index of the newly-selected tab.
  WM_IPC_MESSAGE_CHROME_NOTIFY_TAB_SELECT = 13,

  // Tell the window manager to hide the login windows.
  WM_IPC_MESSAGE_WM_HIDE_LOGIN = 14,

  // Set whether login is enabled.  If true, the user can click on any of the
  // login windows to select one, if false clicks on unselected windows are
  // ignored.  This is used when the user attempts a login to make sure the
  // user doesn't select another user.
  //   param[0]: True to enable, false to disable.
  WM_IPC_MESSAGE_WM_SET_LOGIN_STATE = 15,

  // Notify chrome when the guest entry is selected and the guest window
  // hasn't been created yet.
  WM_IPC_MESSAGE_CHROME_CREATE_GUEST_WINDOW = 16,

  // Notify Chrome when a system key of interest is clicked, so volume up/down
  // and mute can be handled (chrome can add visual feedback).  This message
  // could be extended for other special purpose keys (maybe multimedia keys
  // like play/pause/ff/rr). See WmIpcSystemKey enum for param[0] values.
  //
  // TODO(davej): Eventually this message should be deprecated in favor of
  // Chrome handling these sorts of keypresses internally.
  WM_IPC_MESSAGE_CHROME_NOTIFY_SYSKEY_PRESSED = 17,

  // NEXT VALUE TO USE: 18
};

// A parameter of WM_IPC_MESSAGE_CHROME_NOTIFY_SYSKEY_PRESSED message
// denoting which key is pressed.
enum WmIpcSystemKey {
  WM_IPC_SYSTEM_KEY_VOLUME_MUTE = 0,
  WM_IPC_SYSTEM_KEY_VOLUME_DOWN = 1,
  WM_IPC_SYSTEM_KEY_VOLUME_UP = 2
};

}  // namespace chromeos

#endif  // CHROMEOS_WM_IPC_ENUMS_H_
