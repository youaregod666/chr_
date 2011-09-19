// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TODO(satorux): Remove this file. DEPRECATED.

#include "libcros_servicer.h"

namespace chromeos {

// Register with the glib type system.
// This macro automatically defines a number of functions and variables
// which are required to make libcros_servicer functional as a GObject:
// - libcros_servicer_parent_class
// - libcros_servicer_get_type()
// - dbus_glib_libcros_servicer_object_info
// It also ensures that the structs are setup so that the initialization
// functions are called in the appropriate way by g_object_new().
G_DEFINE_TYPE(LibCrosServicer, libcros_servicer, G_TYPE_OBJECT);

GObject* libcros_servicer_constructor(GType gtype,
                                      guint n_properties,
                                      GObjectConstructParam *properties) {
  GObject* obj;
  GObjectClass* parent_class;
  // Instantiate using the parent class, then extend for local properties.
  parent_class = G_OBJECT_CLASS(libcros_servicer_parent_class);
  obj = parent_class->constructor(gtype, n_properties, properties);

  LibCrosServicer* libcros_servicer = reinterpret_cast<LibCrosServicer*>(obj);
  libcros_servicer->service = NULL;

  // We don't have any thing we care to expose to the glib class system.
  return obj;
}

void libcros_servicer_class_init(LibCrosServicerClass *real_class) {
  // Called once to configure the "class" structure.
  GObjectClass* gobject_class = G_OBJECT_CLASS(real_class);
  gobject_class->constructor = libcros_servicer_constructor;
}

void libcros_servicer_init(LibCrosServicer *self) {}

gboolean libcros_servicer_resolve_network_proxy(LibCrosServicer* self,
                                                gchar* source_url,
                                                gchar* signal_interface,
                                                gchar* signal_name,
                                                GError** error) {
  if (!self->service)
    return false;
  return self->service->ResolveNetworkProxy(source_url, signal_interface,
                                            signal_name, error);
}

}  // namespace chromeos
