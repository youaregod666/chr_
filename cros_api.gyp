# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    { 'target_name': 'cros_api',
      'type': '<(library)',
      'sources': [
        'chromeos_power.h',
        'chromeos_network.h',
        'chromeos_input_method.h',
        'chromeos_input_method_ui.h',
        'chromeos_ime.h',  # TODO(satorux): Remove this once it's ready.
        'chromeos_language.h',  # TODO(satorux): Remove this once it's ready.
        'chromeos_syslogs.h',
        'load.cc',
      ],
      'include_dirs': [
        '.',
        '../..',
      ],
    },
  ],
 }
