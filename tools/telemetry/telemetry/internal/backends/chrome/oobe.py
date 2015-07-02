# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from telemetry.core import exceptions
from telemetry.core import util
from telemetry.internal.browser import web_contents


class Oobe(web_contents.WebContents):
  def __init__(self, inspector_backend):
    super(Oobe, self).__init__(inspector_backend)

  def _GaiaIFrameContext(self):
    max_context_id = self.EnableAllContexts()
    logging.debug('%d contexts in Gaia page' % max_context_id)
    for gaia_iframe_context in range(max_context_id + 1):
      try:
        if self.EvaluateJavaScriptInContext(
            "document.readyState == 'complete' && "
            "document.getElementById('Email') != null",
            gaia_iframe_context):
          return gaia_iframe_context
      except exceptions.EvaluateException:
        pass
    return None

  def _GaiaWebviewContext(self):
    webview_contexts = self.GetWebviewContexts()
    if webview_contexts:
      return webview_contexts[0]
    return None

  def _ExecuteOobeApi(self, api, *args):
    logging.info('Invoking %s' % api)
    self.WaitForJavaScriptExpression("typeof Oobe == 'function'", 20)

    if self.EvaluateJavaScript("typeof %s == 'undefined'" % api):
      raise exceptions.LoginException('%s js api missing' % api)

    js = api + '(' + ("'%s'," * len(args)).rstrip(',') + ');'
    self.ExecuteJavaScript(js % args)

  def NavigateGuestLogin(self):
    """Logs in as guest."""
    self._ExecuteOobeApi('Oobe.guestLoginForTesting')

  def NavigateFakeLogin(self, username, password):
    """Fake user login."""
    self._ExecuteOobeApi('Oobe.loginForTesting', username, password)

  def NavigateEnterpriseEnrollment(self, username, password):
    """Enterprise enrolls using the GAIA webview or IFrame, whichever
    is present."""
    self._ExecuteOobeApi('Oobe.skipToLoginForTesting')
    self._ExecuteOobeApi('Oobe.switchToEnterpriseEnrollmentForTesting')
    if self._GaiaIFrameContext() is None:
      self._NavigateWebViewLogin(username, password, wait_for_close=False)
    else:
      self._NavigateIFrameLogin(username, password)

    self.WaitForJavaScriptExpression('Oobe.isEnrollmentSuccessfulForTest()', 30)
    self._ExecuteOobeApi('Oobe.enterpriseEnrollmentDone')

  def NavigateGaiaLogin(self, username, password):
    """Logs in using the GAIA webview or IFrame, whichever is
    present."""
    self._ExecuteOobeApi('Oobe.skipToLoginForTesting')
    def _GetGaiaFunction():
      self._ExecuteOobeApi('Oobe.showAddUserForTesting')
      if self._GaiaIFrameContext() is not None:
        return Oobe._NavigateIFrameLogin
      elif self._GaiaWebviewContext():
        return Oobe._NavigateWebViewLogin
      return None
    util.WaitFor(_GetGaiaFunction, 20)(self, username, password)

  def _NavigateIFrameLogin(self, username, password):
    """Logs into the IFrame-based GAIA screen"""
    gaia_iframe_context = util.WaitFor(self._GaiaIFrameContext, timeout=30)

    self.ExecuteJavaScriptInContext("""
        document.getElementById('Email').value='%s';
        document.getElementById('Passwd').value='%s';
        document.getElementById('signIn').click();"""
            % (username, password),
        gaia_iframe_context)

  def _NavigateWebViewLogin(self, username, password, wait_for_close=True):
    """Logs into the webview-based GAIA screen"""
    self._NavigateWebViewEntry('identifierId', username)
    self._GaiaWebviewContext().WaitForJavaScriptExpression(
        "document.getElementById('identifierId') == null", 20)
    self._NavigateWebViewEntry('password', password)
    if wait_for_close:
      util.WaitFor(lambda: not self._GaiaWebviewContext(), 20)

  def _NavigateWebViewEntry(self, field, value):
    self._WaitForField(field)
    self._WaitForField('next')
    gaia_webview_context = self._GaiaWebviewContext()
    gaia_webview_context.EvaluateJavaScript("""
       document.getElementById('%s').value='%s';
       document.getElementById('next').click()"""
           % (field, value))

  def _WaitForField(self, field_id):
    gaia_webview_context = util.WaitFor(self._GaiaWebviewContext, 5)
    util.WaitFor(gaia_webview_context.HasReachedQuiescence, 20)
    gaia_webview_context.WaitForJavaScriptExpression(
        "document.getElementById('%s') != null" % field_id, 20)
