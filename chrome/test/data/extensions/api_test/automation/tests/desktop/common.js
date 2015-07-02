// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var assertEq = chrome.test.assertEq;
var assertFalse = chrome.test.assertFalse;
var assertTrue = chrome.test.assertTrue;

var EventType = chrome.automation.EventType;
var RoleType = chrome.automation.RoleType;
var StateType = chrome.automation.StateType;

var rootNode = null;

function findAutomationNode(root, condition) {
  if (condition(root))
    return root;

  var children = root.children;
  for (var i = 0; i < children.length; i++) {
    var result = findAutomationNode(children[i], condition);
    if (result)
      return result;
  }
  return null;
}

function runWithDocument(docString, callback) {
  var url = 'data:text/html,<!doctype html>' + docString;
  var createParams = {
    active: true,
    url: url
  };
  chrome.tabs.create(createParams, function(tab) {
    chrome.tabs.onUpdated.addListener(function(tabId, changeInfo) {
      if (tabId == tab.id && changeInfo.status == 'complete') {
        if (callback)
          callback();
      }
    });
  });
}

function setupAndRunTests(allTests, opt_docString) {
  function runTestInternal() {
    chrome.test.runTests(allTests);
  }

  chrome.automation.getDesktop(function(rootNodeArg) {
    rootNode = rootNodeArg;

    // Only run the test when the window containing the new tab loads.
    rootNodeArg.addEventListener(
        chrome.automation.EventType.childrenChanged,
        function(evt) {
          var subroot = evt.target.firstChild;
          if (!opt_docString || !subroot)
            return;

          if (subroot.role == 'rootWebArea' &&
              subroot.docUrl.indexOf(opt_docString) != -1)
            runTestInternal();
        },
        true);
    if (opt_docString)
      runWithDocument(opt_docString, null);
    else
      runTestInternal();
  });
}
