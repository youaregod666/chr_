// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_CROS_LIBCROS_SERVICER_H_
#define PLATFORM_CROS_LIBCROS_SERVICER_H_
#pragma once

#include "libcros_service.h"

// Helpers for using GObjects until we can get a C++ wrapper going.
namespace chromeos {

struct LibCrosServicer {
  GObject parent_instance;
  LibCrosService *service;  // pointer to implementing service.
};

struct LibCrosServicerClass {
  GObjectClass parent_class;
};

// libcros_servicer_get_type() is defined in interface.cc by the
// G_DEFINE_TYPE() macro.  This macro defines a number of other GLib
// class system specific functions and variables discussed in
// libcros_servicer.cc.
GType libcros_servicer_get_type();  // defined by G_DEFINE_TYPE

// Interface function prototypes which wrap service.
gboolean libcros_servicer_resolve_network_proxy(LibCrosServicer *self,
                                                gchar* source_url,
                                                gchar* signal_interface,
                                                gchar* signal_name,
                                                GError **error);

}  // namespace chromeos

#endif  // PLATFORM_CROS_LIBCROS_SERVICER_H_

