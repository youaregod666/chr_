# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

SOURCES=['chromeos_power.cc', 'chromeos_network.cc', 'marshal.cc', 
         'version_check.cc']

env = Environment(
    CPPPATH=[ '.', '../../common', '..'],
    CCFLAGS=['-m32', '-fno-exceptions'],
    LINKFLAGS=['-m32' ],
    LIBS = ['glog', 'chromeos'],
    LIBPATH=['../../common'],
)

# glib and dbug environment
env.ParseConfig('pkg-config --cflags --libs dbus-1 glib-2.0 dbus-glib-1')

env.SharedLibrary('cros', SOURCES)

# so test
env_so = Environment (
    CPPPATH=[ '.', '../../common', '..'],
    CCFLAGS=['-m32', '-fno-exceptions'],
    LINKFLAGS=['-m32' ],
    LIBS = ['glog', 'dl'],
    LIBPATH=['../../common'],
)
env_so.ParseConfig('pkg-config --cflags --libs gobject-2.0')
env_so.Program('monitor_power', ['monitor_power.cc', 'load.cc'])
env_so.Program('monitor_network', ['monitor_network.cc', 'load.cc'])

