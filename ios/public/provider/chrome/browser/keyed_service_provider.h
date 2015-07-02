// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_KEYED_SERVICE_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_KEYED_SERVICE_PROVIDER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"

enum class ServiceAccessType;

class KeyedServiceBaseFactory;
class ProfileOAuth2TokenServiceIOS;
class SigninManager;

namespace autofill {
class PersonalDataManager;
}

namespace bookmarks {
class BookmarkModel;
}

namespace sync_driver {
class SyncService;
}

namespace ios {

class ChromeBrowserState;
class KeyedServiceProvider;

// Registers and returns the global KeyedService provider.
void SetKeyedServiceProvider(KeyedServiceProvider* provider);
KeyedServiceProvider* GetKeyedServiceProvider();

// A class that provides access to KeyedService that do not have a pure iOS
// implementation yet.
class KeyedServiceProvider {
 public:
  KeyedServiceProvider();
  virtual ~KeyedServiceProvider();

  // Ensures that all KeyedService factories are instantiated. Must be called
  // before any BrowserState instance is created so that dependencies are
  // correct.
  virtual void AssertKeyedFactoriesBuilt() = 0;

  // Returns the bookmarks::BookmarkModel factory for dependencies.
  virtual KeyedServiceBaseFactory* GetBookmarkModelFactory() = 0;

  // Returns an instance of bookmarks::BookmarkModel tied to |browser_state|.
  virtual bookmarks::BookmarkModel* GetBookmarkModelForBrowserState(
      ChromeBrowserState* browser_state) = 0;

  // Returns the ProfileOAuth2TokenServiceIOS factory for dependencies.
  virtual KeyedServiceBaseFactory* GetProfileOAuth2TokenServiceIOSFactory() = 0;

  // Returns an instance of ProfileOAuth2TokenServiceIOS tied to
  // |browser_state|.
  virtual ProfileOAuth2TokenServiceIOS*
  GetProfileOAuth2TokenServiceIOSForBrowserState(
      ChromeBrowserState* browser_state) = 0;

  // Returns the SigninManager factory for dependencies.
  virtual KeyedServiceBaseFactory* GetSigninManagerFactory() = 0;

  // Returns an instance of SigninManager tied to |browser_state|.
  virtual SigninManager* GetSigninManagerForBrowserState(
      ChromeBrowserState* browser_state) = 0;

  // Returns the autofill::PersonalDataManager factory for dependencies.
  virtual KeyedServiceBaseFactory* GetPersonalDataManagerFactory() = 0;

  // Returns an instance of autofill::PersonalDataManager tied to
  // |browser_state|.
  virtual autofill::PersonalDataManager* GetPersonalDataManagerForBrowserState(
      ChromeBrowserState* browser_state) = 0;

  // Returns the sync_driver::SyncService factory for dependencies.
  virtual KeyedServiceBaseFactory* GetSyncServiceFactory() = 0;

  // Returns an instance of sync_driver::SyncService tied to |browser_state|.
  virtual sync_driver::SyncService* GetSyncServiceForBrowserState(
      ChromeBrowserState* browser_state) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(KeyedServiceProvider);
};

}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_KEYED_SERVICE_PROVIDER_H_
