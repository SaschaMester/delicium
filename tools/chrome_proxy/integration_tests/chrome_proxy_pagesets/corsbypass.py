# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class CorsBypassPage(page_module.Page):

  def __init__(self, url, page_set):
    super(CorsBypassPage, self).__init__(url=url, page_set=page_set)


class CorsBypassPageSet(page_set_module.PageSet):

  """ Chrome proxy test sites """

  def __init__(self):
    super(CorsBypassPageSet, self).__init__()

    urls_list = [
      'http://www.gstatic.com/chrome/googlezip/cors/',
    ]

    for url in urls_list:
      self.AddUserStory(CorsBypassPage(url, self))
