# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

SOURCES=['chromeos_ime.cc',
         'chromeos_language.cc',
         'chromeos_login.cc',
         'chromeos_mount.cc',
         'chromeos_network.cc',
         'chromeos_power.cc',
         'chromeos_synaptics.cc',
         'marshal.cc',
         'version_check.cc',
]

env = Environment(
    CPPPATH=['.', '..'],
    CCFLAGS=['-fno-exceptions', '-ggdb'],
    LINKFLAGS=['-fPIC'],
    LIBS = ['base', 'chromeos', 'rt', 'synaptics'],
)
for key in Split('CC CXX AR RANLIB LD NM CFLAGS CCFLAGS'):
  value = os.environ.get(key)
  if value != None:
    env[key] = value

# Fix issue with scons not passing pkg-config vars through the environment.
for key in Split('PKG_CONFIG_LIBDIR PKG_CONFIG_PATH'):
  if os.environ.has_key(key):
    env['ENV'][key] = os.environ[key]

# glib, dbus, and ibus environment
env.ParseConfig('pkg-config --cflags --libs dbus-1 glib-2.0 gudev-1.0 dbus-glib-1 ibus-1.0 libpcrecpp')

env.SharedLibrary('cros', SOURCES)

# so test
env_so = Environment (
    CPPPATH=[ '.', '..'],
    CCFLAGS=['-fno-exceptions', '-fPIC'],
    LIBS = ['base', 'dl', 'rt'],
)
for key in Split('CC CXX AR RANLIB LD NM CFLAGS CCFLAGS'):
  value = os.environ.get(key)
  if value != None:
    env_so[key] = value
env_so.ParseConfig('pkg-config --cflags --libs gobject-2.0')
env_so.Program('monitor_power', ['monitor_power.cc', 'load.cc'])
env_so.Program('monitor_language', ['monitor_language.cc', 'load.cc'])
env_so.Program('monitor_network', ['monitor_network.cc', 'load.cc'])
env_so.Program('monitor_mount', ['monitor_mount.cc', 'load.cc'])
env_so.Program('monitor_ime', ['monitor_ime.cc', 'load.cc'])
env_so.Program('login_driver', ['drive_login.cc', 'load.cc'])
