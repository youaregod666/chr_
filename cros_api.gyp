# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    { 'target_name': 'cros_api',
      'type': '<(library)',
      'dependencies': [
        '../../build/linux/system.gyp:glib',
      ],
      'sources': [
        'chromeos_network.h',
        'load.cc',
      ],
      'include_dirs': [
        '.',
        '../..',
      ],
    },
  ],
 }
