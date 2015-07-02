// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/precache/precache_launcher.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/precache/precache_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/precache/content/precache_manager.h"
#include "jni/PrecacheLauncher_jni.h"

using base::android::AttachCurrentThread;
using precache::PrecacheManager;

namespace history {
class HistoryService;
}

namespace {

// Get the profile that should be used for precaching.
Profile* GetProfile() {
  Profile* profile = g_browser_process->profile_manager()->GetLastUsedProfile()
      ->GetOriginalProfile();
  DCHECK(profile);
  DCHECK(g_browser_process->profile_manager()->IsValidProfile(profile));
  return profile;
}

// Get the PrecacheManager for the given |profile|.
PrecacheManager* GetPrecacheManager(Profile* profile) {
  PrecacheManager* precache_manager =
      precache::PrecacheManagerFactory::GetForBrowserContext(profile);
  DCHECK(precache_manager);
  return precache_manager;
}

}  // namespace

PrecacheLauncher::PrecacheLauncher(JNIEnv* env, jobject obj)
    : weak_java_precache_launcher_(env, obj), weak_factory_(this) {}

PrecacheLauncher::~PrecacheLauncher() {}

void PrecacheLauncher::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

void PrecacheLauncher::Start(JNIEnv* env, jobject obj) {
  // TODO(bengr): Add integration tests for the whole feature.
  Profile* profile = GetProfile();

  PrecacheManager* precache_manager = GetPrecacheManager(profile);
  history::HistoryService* hs = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::IMPLICIT_ACCESS);

  if (precache_manager == nullptr || hs == nullptr ||
      !precache_manager->IsPrecachingAllowed()) {
    Java_PrecacheLauncher_onPrecacheCompletedCallback(
        env, weak_java_precache_launcher_.get(env).obj());
    return;
  }

  precache_manager->StartPrecaching(
      base::Bind(&PrecacheLauncher::OnPrecacheCompleted,
                 weak_factory_.GetWeakPtr()),
      *hs);
}

void PrecacheLauncher::Cancel(JNIEnv* env, jobject obj) {
  Profile* profile = GetProfile();
  PrecacheManager* precache_manager = GetPrecacheManager(profile);

  precache_manager->CancelPrecaching();
}

void PrecacheLauncher::OnPrecacheCompleted() {
  JNIEnv* env = AttachCurrentThread();
  Java_PrecacheLauncher_onPrecacheCompletedCallback(
      env, weak_java_precache_launcher_.get(env).obj());
}

static jlong Init(JNIEnv* env, jobject obj) {
  return reinterpret_cast<intptr_t>(new PrecacheLauncher(env, obj));
}

static jboolean IsPrecachingEnabled(JNIEnv* env, jclass clazz) {
  return PrecacheManager::IsPrecachingEnabled();
}

bool RegisterPrecacheLauncher(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
