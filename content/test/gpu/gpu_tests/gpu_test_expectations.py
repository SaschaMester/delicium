# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import test_expectations

# Valid expectation conditions are:
#
# Operating systems:
#     win, xp, vista, win7, mac, leopard, snowleopard, lion, mountainlion,
#     mavericks, yosemite, linux, chromeos, android
#
# GPU vendors:
#     amd, arm, broadcom, hisilicon, intel, imagination, nvidia, qualcomm,
#     vivante
#
# Browser types:
#     android-webview-shell, android-content-shell, debug
#
# ANGLE renderer:
#     d3d9, d3d11, opengl
#
# Specific GPUs can be listed as a tuple with vendor name and device ID.
# Examples: ('nvidia', 0x1234), ('arm', 'Mali-T604')
# Device IDs must be paired with a GPU vendor.
#
# Sample usage in SetExpectations in subclasses:
#   self.Fail('gl-enable-vertex-attrib.html',
#       ['mac', 'amd', ('nvidia', 0x1234)], bug=123)

ANGLE_MODIFIERS = ['d3d9', 'd3d11', 'opengl']

BROWSER_TYPE_MODIFIERS = [
    'android-webview-shell', 'android-content-shell', 'debug' ]

class _FlakyExpectation(object):
  def __init__(self, expectation, max_num_retries):
    self.expectation = expectation
    self.max_num_retries = max_num_retries

class _FakeSharedPageState(object):
  def __init__(self, browser):
    self.browser = browser

class GpuTestExpectations(test_expectations.TestExpectations):
  def __init__(self):
    self._flaky_expectations = []
    super(GpuTestExpectations, self).__init__()

  def Flaky(self, url_pattern, conditions=None, bug=None, max_num_retries=2):
    expectation = _FlakyExpectation(self.CreateExpectation(
      'pass', url_pattern, conditions, bug), max_num_retries)
    self._flaky_expectations.append(expectation)

  def GetFlakyRetriesForPage(self, page, browser):
    for fe in self._flaky_expectations:
      e = fe.expectation
      if self.ExpectationAppliesToPage(e, _FakeSharedPageState(browser), page):
        return fe.max_num_retries
    return 0

  def IsValidUserDefinedCondition(self, condition):
    # Add support for d3d9, d3d11 and opengl-specific expectations.
    if condition in ANGLE_MODIFIERS:
      return True
    # Add support for browser-type-specific expectations.
    if condition in BROWSER_TYPE_MODIFIERS:
      return True
    return super(GpuTestExpectations,
        self).IsValidUserDefinedCondition(condition)

  def ModifiersApply(self, shared_page_state, expectation):
    if not super(GpuTestExpectations, self).ModifiersApply(
        shared_page_state, expectation):
      return False

    # We'll only get here if the OS and GPU matched the expectation.

    # TODO(kbr): refactor _Expectation to be a public class so that
    # the GPU-specific properties can be moved into a subclass, and
    # run the unit tests from this directory on the CQ and the bots.
    # crbug.com/495868 crbug.com/495870

    # Check for presence of Android WebView.
    browser = shared_page_state.browser
    browser_expectations = [x for x in expectation.user_defined_conditions
                            if x in BROWSER_TYPE_MODIFIERS]
    browser_matches = ((not browser_expectations) or
                       browser.browser_type in browser_expectations)
    if not browser_matches:
      return False
    angle_renderer = ''
    gpu_info = None
    if browser.supports_system_info:
      gpu_info = browser.GetSystemInfo().gpu
    if gpu_info and gpu_info.aux_attributes:
      gl_renderer = gpu_info.aux_attributes.get('gl_renderer')
      if gl_renderer:
        if 'Direct3D11' in gl_renderer:
          angle_renderer = 'd3d11'
        elif 'Direct3D9' in gl_renderer:
          angle_renderer = 'd3d9'
        elif 'OpenGL' in gl_renderer:
          angle_renderer = 'opengl'
    angle_expectations = [x for x in expectation.user_defined_conditions
                        if x in ANGLE_MODIFIERS]
    angle_matches = ((not angle_expectations) or
                   angle_renderer in angle_expectations)
    return angle_matches
