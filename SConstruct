# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

SOURCES=['chromeos_power.cc', 'chromeos_language.cc', 'chromeos_mount.cc',
         'chromeos_network.cc', 'chromeos_ime.cc', 'marshal.cc',
         'version_check.cc', 'chromeos_synaptics.cc']

env = Environment(
    CPPPATH=[ '.', '..', '../../common',
              '../../third_party/synaptics',
              '../../third_party/chrome/files'],
    CCFLAGS=['-m32', '-fno-exceptions', '-ggdb'],
    LINKFLAGS=['-m32' ],
    LIBS = ['base', 'chromeos', 'rt', 'synaptics'],
    LIBPATH=['../../common', '../../third_party/synaptics',
             '../../third_party/chrome'],
)

# glib, dbus, and ibus environment
env.ParseConfig('pkg-config --cflags --libs dbus-1 glib-2.0 gudev-1.0 dbus-glib-1 ibus-1.0 libpcrecpp')

env.SharedLibrary('cros', SOURCES)

# so test
env_so = Environment (
    CPPPATH=[ '.', '../../common', '..', '../../third_party/chrome/files'],
    CCFLAGS=['-m32', '-fno-exceptions', '-ggdb'],
    LINKFLAGS=['-m32' ],
    LIBS = ['base', 'dl', 'rt'],
    LIBPATH=['../../common', '../../third_party/chrome'],
)
env_so.ParseConfig('pkg-config --cflags --libs gobject-2.0')
env_so.Program('monitor_power', ['monitor_power.cc', 'load.cc'])
env_so.Program('monitor_language', ['monitor_language.cc', 'load.cc'])
env_so.Program('monitor_network', ['monitor_network.cc', 'load.cc'])
env_so.Program('monitor_mount', ['monitor_mount.cc', 'load.cc'])
env_so.Program('monitor_ime', ['monitor_ime.cc', 'load.cc'])
