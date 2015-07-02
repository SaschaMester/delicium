// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ClearKeyPlayer responsible for playing media using Clear Key key system and
// the unprefixed version of EME.
function ClearKeyPlayer(video, testConfig) {
  this.video = video;
  this.testConfig = testConfig;
}

ClearKeyPlayer.prototype.init = function() {
  // Returns a promise.
  return PlayerUtils.initEMEPlayer(this);
};

ClearKeyPlayer.prototype.registerEventListeners = function() {
  // Returns a promise.
  return PlayerUtils.registerEMEEventListeners(this);
};

ClearKeyPlayer.prototype.onMessage = function(message) {
  Utils.timeLog('MediaKeySession onMessage', message);
  var initData =
      Utils.getInitDataFromMessage(message, this.testConfig.mediaType, true);
  var key = Utils.getDefaultKey(this.testConfig.forceInvalidResponse);
  var jwkSet = Utils.createJWKData(initData, key);
  message.target.update(jwkSet).catch(function(error) {
    // Ignore the error if a crash is expected. This ensures that the decoder
    // actually detects and reports the error.
    if (this.testConfig.keySystem != 'org.chromium.externalclearkey.crash') {
      Utils.failTest(error, KEY_ERROR);
    }
  });
};
