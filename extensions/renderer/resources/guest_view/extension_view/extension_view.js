// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements the ExtensionView <extensionview>.

var GuestViewContainer = require('guestViewContainer').GuestViewContainer;
var ExtensionViewConstants =
    require('extensionViewConstants').ExtensionViewConstants;
var ExtensionViewEvents = require('extensionViewEvents').ExtensionViewEvents;
var ExtensionViewInternal =
    require('extensionViewInternal').ExtensionViewInternal;

function ExtensionViewImpl(extensionviewElement) {
  GuestViewContainer.call(this, extensionviewElement, 'extensionview');

  // A queue of objects in the order they should be loaded.
  // Every load call will add the given src, as well as the resolve and reject
  // functions. Each src will be loaded in the order they were called.
  this.loadQueue = [];

  // The current src that is loading.
  // @type {Object<!string, function, function>}
  this.pendingLoad = null;

  new ExtensionViewEvents(this, this.viewInstanceId);
}

ExtensionViewImpl.prototype.__proto__ = GuestViewContainer.prototype;

ExtensionViewImpl.VIEW_TYPE = 'ExtensionView';

ExtensionViewImpl.setupElement = function(proto) {
  var apiMethods = ExtensionViewImpl.getApiMethods();

  GuestViewContainer.forwardApiMethods(proto, apiMethods);
};

ExtensionViewImpl.prototype.createGuest = function(callback) {
  this.guest.create(this.buildParams(), function() {
    this.attachWindow$();
    callback();
  }.bind(this));
};

ExtensionViewImpl.prototype.buildContainerParams = function() {
  var params = {};
  for (var i in this.attributes) {
    params[i] = this.attributes[i].getValue();
  }
  return params;
};

ExtensionViewImpl.prototype.onElementDetached = function() {
  this.guest.destroy();

  // Reset all attributes.
  for (var i in this.attributes) {
    this.attributes[i].setValueIgnoreMutation();
  }
};

// Updates src upon loadcommit.
ExtensionViewImpl.prototype.onLoadCommit = function(url) {
  this.attributes[ExtensionViewConstants.ATTRIBUTE_SRC].
      setValueIgnoreMutation(url);
};

// Loads the next pending src from |loadQueue| to the extensionview.
ExtensionViewImpl.prototype.loadNextSrc = function() {
  // If extensionview isn't currently loading a src, load the next src
  // in |loadQueue|. Otherwise, do nothing.
  if (!this.pendingLoad && this.loadQueue.length) {
    this.pendingLoad = this.loadQueue.shift();
    var src = this.pendingLoad.src;
    var resolve = this.pendingLoad.resolve;
    var reject = this.pendingLoad.reject;

    ExtensionViewInternal.parseSrc(src, function(isSrcValid, extensionId) {
      // Check if the src is valid.
      if (!isSrcValid) {
        reject('Failed to load: src is not valid.');
        return;
      }

      // Destroy the current guest and create a new one if extension ID
      // is different.
      //
      // This may happen if the extensionview is loads an extension page, and
      // is then intended to load a page served from a different extension in
      // the same part of the WebUI.
      //
      // The two calls may look like the following:
      //   extensionview.load('chrome-extension://firstId/page.html');
      //   extensionview.load('chrome-extension://secondId/page.html');
      // The second time load is called, we destroy the current guest since
      // we will be loading content from a different extension.
      if (extensionId !=
          this.attributes[ExtensionViewConstants.ATTRIBUTE_EXTENSION]
            .getValue()) {
        this.guest.destroy();

        // Update the extension and src attributes.
        this.attributes[ExtensionViewConstants.ATTRIBUTE_EXTENSION]
            .setValueIgnoreMutation(extensionId);
        this.attributes[ExtensionViewConstants.ATTRIBUTE_SRC]
            .setValueIgnoreMutation(src);

        this.createGuest(function() {
          if (this.guest.getId() <= 0) {
            reject('Failed to load: guest creation failed.');
          } else {
            resolve('Successful load.');
          }
        }.bind(this));
      } else {
        ExtensionViewInternal.loadSrc(this.guest.getId(), src,
            function(hasLoadSucceeded) {
          if (!hasLoadSucceeded) {
            reject('Failed to load.');
          } else {
            // Update the src attribute.
            this.attributes[ExtensionViewConstants.ATTRIBUTE_SRC]
                .setValueIgnoreMutation(src);
            resolve('Successful load.');
          }
        }.bind(this));
      }
    }.bind(this));
  }
};

GuestViewContainer.registerElement(ExtensionViewImpl);

// Exports.
exports.ExtensionViewImpl = ExtensionViewImpl;