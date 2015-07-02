# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess

from catapult_base import support_binaries
from telemetry.core.platform import platform_backend


class DesktopPlatformBackend(platform_backend.PlatformBackend):

  # This is an abstract class. It is OK to have abstract methods.
  # pylint: disable=W0223

  def FlushSystemCacheForDirectory(self, directory):
    assert directory and os.path.exists(directory), \
        'Target directory %s must exist' % directory
    flush_command = support_binaries.FindPath('clear_system_cache',
                                              self.GetArchName(),
                                              self.GetOSName())
    assert flush_command, 'You must build clear_system_cache first'

    subprocess.check_call([flush_command, '--recurse', directory])

  def GetDeviceTypeName(self):
    return 'Desktop'
