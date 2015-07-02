// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enhanced_bookmarks/android/enhanced_bookmarks_bridge.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/enhanced_bookmarks/enhanced_bookmark_model_factory.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/android/bookmark_id.h"
#include "components/bookmarks/common/android/bookmark_type.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "jni/EnhancedBookmarksBridge_jni.h"
#include "ui/gfx/android/java_bitmap.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaGlobalRef;
using bookmarks::android::JavaBookmarkIdCreateBookmarkId;
using bookmarks::android::JavaBookmarkIdGetId;
using bookmarks::android::JavaBookmarkIdGetType;
using bookmarks::BookmarkNode;
using bookmarks::BookmarkType;
using content::WebContents;
using content::BrowserThread;

namespace enhanced_bookmarks {
namespace android {

EnhancedBookmarksBridge::EnhancedBookmarksBridge(JNIEnv* env,
    jobject obj,
    Profile* profile) : weak_java_ref_(env, obj) {
  profile_ = profile;
  enhanced_bookmark_model_ =
      EnhancedBookmarkModelFactory::GetForBrowserContext(profile_);
  enhanced_bookmark_model_->SetVersionSuffix(chrome::VersionInfo().OSType());
}

void EnhancedBookmarksBridge::Destroy(JNIEnv*, jobject) {
  delete this;
}

ScopedJavaLocalRef<jobject> EnhancedBookmarksBridge::AddFolder(JNIEnv* env,
    jobject obj,
    jobject j_parent_id_obj,
    jint index,
    jstring j_title) {
  DCHECK(enhanced_bookmark_model_->loaded());
  long bookmark_id = JavaBookmarkIdGetId(env, j_parent_id_obj);
  const BookmarkNode* parent = bookmarks::GetBookmarkNodeByID(
      enhanced_bookmark_model_->bookmark_model(),
      static_cast<int64>(bookmark_id));
  const BookmarkNode* new_node = enhanced_bookmark_model_->AddFolder(
      parent, index, base::android::ConvertJavaStringToUTF16(env, j_title));
  if (!new_node) {
    NOTREACHED();
    return ScopedJavaLocalRef<jobject>();
  }
  ScopedJavaLocalRef<jobject> new_java_obj = JavaBookmarkIdCreateBookmarkId(
      env, new_node->id(), BookmarkType::BOOKMARK_TYPE_NORMAL);
  return new_java_obj;
}

void EnhancedBookmarksBridge::MoveBookmark(JNIEnv* env,
                                           jobject obj,
                                           jobject j_bookmark_id_obj,
                                           jobject j_parent_id_obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(enhanced_bookmark_model_->loaded());

  long bookmark_id = JavaBookmarkIdGetId(env, j_bookmark_id_obj);
  const BookmarkNode* node = bookmarks::GetBookmarkNodeByID(
      enhanced_bookmark_model_->bookmark_model(),
      static_cast<int64>(bookmark_id));
  if (!IsEditable(node)) {
    NOTREACHED();
    return;
  }
  bookmark_id = JavaBookmarkIdGetId(env, j_parent_id_obj);
  const BookmarkNode* new_parent_node = bookmarks::GetBookmarkNodeByID(
      enhanced_bookmark_model_->bookmark_model(),
      static_cast<int64>(bookmark_id));
  enhanced_bookmark_model_->Move(node, new_parent_node,
                                 new_parent_node->child_count());
}

ScopedJavaLocalRef<jobject> EnhancedBookmarksBridge::AddBookmark(
    JNIEnv* env,
    jobject obj,
    jobject j_parent_id_obj,
    jint index,
    jstring j_title,
    jstring j_url) {
  DCHECK(enhanced_bookmark_model_->loaded());
  long bookmark_id = JavaBookmarkIdGetId(env, j_parent_id_obj);
  const BookmarkNode* parent = bookmarks::GetBookmarkNodeByID(
      enhanced_bookmark_model_->bookmark_model(),
      static_cast<int64>(bookmark_id));

  const BookmarkNode* new_node = enhanced_bookmark_model_->AddURL(
      parent,
      index,
      base::android::ConvertJavaStringToUTF16(env, j_title),
      GURL(base::android::ConvertJavaStringToUTF16(env, j_url)),
      base::Time::Now());
  if (!new_node) {
    NOTREACHED();
    return ScopedJavaLocalRef<jobject>();
  }
  ScopedJavaLocalRef<jobject> new_java_obj = JavaBookmarkIdCreateBookmarkId(
      env, new_node->id(), BookmarkType::BOOKMARK_TYPE_NORMAL);
  return new_java_obj;
}

bool EnhancedBookmarksBridge::IsEditable(const BookmarkNode* node) const {
  if (!node || (node->type() != BookmarkNode::FOLDER &&
      node->type() != BookmarkNode::URL)) {
    return false;
  }
  return profile_->GetPrefs()->GetBoolean(
        bookmarks::prefs::kEditBookmarksEnabled);
}

static jlong Init(JNIEnv* env, jobject obj, jobject j_profile) {
  return reinterpret_cast<jlong>(new EnhancedBookmarksBridge(
        env, obj, ProfileAndroid::FromProfileAndroid(j_profile)));
}

bool RegisterEnhancedBookmarksBridge(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android
}  // namespace enhanced_bookmarks
