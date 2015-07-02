// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENHANCED_BOOKMARKS_ANDROID_ENHANCED_BOOKMARKS_BRIDGE_H_
#define CHROME_BROWSER_ENHANCED_BOOKMARKS_ANDROID_ENHANCED_BOOKMARKS_BRIDGE_H_

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "chrome/browser/profiles/profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/enhanced_bookmarks/enhanced_bookmark_model.h"

namespace enhanced_bookmarks {

class BookmarkImageServiceAndroid;

namespace android {

class EnhancedBookmarksBridge {
 public:
  EnhancedBookmarksBridge(JNIEnv* env, jobject obj, Profile* profile);
  void Destroy(JNIEnv*, jobject);

  base::android::ScopedJavaLocalRef<jobject> AddFolder(JNIEnv* env,
                                                       jobject obj,
                                                       jobject j_parent_id_obj,
                                                       jint index,
                                                       jstring j_title);

  void MoveBookmark(JNIEnv* env,
                    jobject obj,
                    jobject j_bookmark_id_obj,
                    jobject j_parent_id_obj);

  base::android::ScopedJavaLocalRef<jobject> AddBookmark(
      JNIEnv* env,
      jobject obj,
      jobject j_parent_id_obj,
      jint index,
      jstring j_title,
      jstring j_url);

 private:
  bool IsEditable(const bookmarks::BookmarkNode* node) const;

  JavaObjectWeakGlobalRef weak_java_ref_;
  EnhancedBookmarkModel* enhanced_bookmark_model_;         // weak
  Profile* profile_;                       // weak
  DISALLOW_COPY_AND_ASSIGN(EnhancedBookmarksBridge);
};

bool RegisterEnhancedBookmarksBridge(JNIEnv* env);

}  // namespace android
}  // namespace enhanced_bookmarks

#endif  // CHROME_BROWSER_ENHANCED_BOOKMARKS_ANDROID_ENHANCED_BOOKMARKS_BRIDGE_H_
