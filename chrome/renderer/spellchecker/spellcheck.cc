// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/spellchecker/spellcheck.h"

#include <algorithm>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/common/spellcheck_common.h"
#include "chrome/common/spellcheck_messages.h"
#include "chrome/common/spellcheck_result.h"
#include "chrome/renderer/spellchecker/spellcheck_provider.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/render_view_visitor.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebTextCheckingCompletion.h"
#include "third_party/WebKit/public/web/WebTextCheckingResult.h"
#include "third_party/WebKit/public/web/WebTextDecorationType.h"
#include "third_party/WebKit/public/web/WebView.h"

using blink::WebVector;
using blink::WebString;
using blink::WebTextCheckingResult;
using blink::WebTextDecorationType;

namespace {

class UpdateSpellcheckEnabled : public content::RenderViewVisitor {
 public:
  explicit UpdateSpellcheckEnabled(bool enabled) : enabled_(enabled) {}
  bool Visit(content::RenderView* render_view) override;

 private:
  bool enabled_;  // New spellcheck-enabled state.
  DISALLOW_COPY_AND_ASSIGN(UpdateSpellcheckEnabled);
};

bool UpdateSpellcheckEnabled::Visit(content::RenderView* render_view) {
  SpellCheckProvider* provider = SpellCheckProvider::Get(render_view);
  DCHECK(provider);
  provider->EnableSpellcheck(enabled_);
  return true;
}

class DocumentMarkersCollector : public content::RenderViewVisitor {
 public:
  DocumentMarkersCollector() {}
  ~DocumentMarkersCollector() override {}
  const std::vector<uint32>& markers() const { return markers_; }
  bool Visit(content::RenderView* render_view) override;

 private:
  std::vector<uint32> markers_;
  DISALLOW_COPY_AND_ASSIGN(DocumentMarkersCollector);
};

bool DocumentMarkersCollector::Visit(content::RenderView* render_view) {
  if (!render_view || !render_view->GetWebView())
    return true;
  WebVector<uint32> markers;
  render_view->GetWebView()->spellingMarkers(&markers);
  for (size_t i = 0; i < markers.size(); ++i)
    markers_.push_back(markers[i]);
  // Visit all render views.
  return true;
}

class DocumentMarkersRemover : public content::RenderViewVisitor {
 public:
  explicit DocumentMarkersRemover(const std::set<std::string>& words);
  ~DocumentMarkersRemover() override {}
  bool Visit(content::RenderView* render_view) override;

 private:
  WebVector<WebString> words_;
  DISALLOW_COPY_AND_ASSIGN(DocumentMarkersRemover);
};

DocumentMarkersRemover::DocumentMarkersRemover(
    const std::set<std::string>& words)
    : words_(words.size()) {
  std::transform(words.begin(), words.end(), words_.begin(),
                 [](const std::string& w) { return WebString::fromUTF8(w); });
}

bool DocumentMarkersRemover::Visit(content::RenderView* render_view) {
  if (render_view && render_view->GetWebView())
    render_view->GetWebView()->removeSpellingMarkersUnderWords(words_);
  return true;
}

bool IsApostrophe(base::char16 c) {
  const base::char16 kApostrophe = 0x27;
  const base::char16 kRightSingleQuotationMark = 0x2019;
  return c == kApostrophe || c == kRightSingleQuotationMark;
}

// Makes sure that the apostrophes in the |spelling_suggestion| are the same
// type as in the |misspelled_word| and in the same order. Ignore differences in
// the number of apostrophes.
void PreserveOriginalApostropheTypes(const base::string16& misspelled_word,
                                     base::string16* spelling_suggestion) {
  auto it = spelling_suggestion->begin();
  for (const base::char16& c : misspelled_word) {
    if (IsApostrophe(c)) {
      it = std::find_if(it, spelling_suggestion->end(), IsApostrophe);
      if (it == spelling_suggestion->end())
        return;

      *it++ = c;
    }
  }
}

}  // namespace

class SpellCheck::SpellcheckRequest {
 public:
  SpellcheckRequest(const base::string16& text,
                    blink::WebTextCheckingCompletion* completion)
      : text_(text), completion_(completion) {
    DCHECK(completion);
  }
  ~SpellcheckRequest() {}

  base::string16 text() { return text_; }
  blink::WebTextCheckingCompletion* completion() { return completion_; }

 private:
  base::string16 text_;  // Text to be checked in this task.

  // The interface to send the misspelled ranges to WebKit.
  blink::WebTextCheckingCompletion* completion_;

  DISALLOW_COPY_AND_ASSIGN(SpellcheckRequest);
};


// Initializes SpellCheck object.
// spellcheck_enabled_ currently MUST be set to true, due to peculiarities of
// the initialization sequence.
// Since it defaults to true, newly created SpellCheckProviders will enable
// spellchecking. After the first word is typed, the provider requests a check,
// which in turn triggers the delayed initialization sequence in SpellCheck.
// This does send a message to the browser side, which triggers the creation
// of the SpellcheckService. That does create the observer for the preference
// responsible for enabling/disabling checking, which allows subsequent changes
// to that preference to be sent to all SpellCheckProviders.
// Setting |spellcheck_enabled_| to false by default prevents that mechanism,
// and as such the SpellCheckProviders will never be notified of different
// values.
// TODO(groby): Simplify this.
SpellCheck::SpellCheck()
    : auto_spell_correct_turned_on_(false),
      spellcheck_enabled_(true) {
}

SpellCheck::~SpellCheck() {
}

bool SpellCheck::OnControlMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SpellCheck, message)
    IPC_MESSAGE_HANDLER(SpellCheckMsg_Init, OnInit)
    IPC_MESSAGE_HANDLER(SpellCheckMsg_CustomDictionaryChanged,
                        OnCustomDictionaryChanged)
    IPC_MESSAGE_HANDLER(SpellCheckMsg_EnableAutoSpellCorrect,
                        OnEnableAutoSpellCorrect)
    IPC_MESSAGE_HANDLER(SpellCheckMsg_EnableSpellCheck, OnEnableSpellCheck)
    IPC_MESSAGE_HANDLER(SpellCheckMsg_RequestDocumentMarkers,
                        OnRequestDocumentMarkers)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void SpellCheck::OnInit(IPC::PlatformFileForTransit bdict_file,
                        const std::set<std::string>& custom_words,
                        const std::string& language,
                        bool auto_spell_correct) {
  Init(IPC::PlatformFileForTransitToFile(bdict_file), custom_words, language);
  auto_spell_correct_turned_on_ = auto_spell_correct;
#if !defined(OS_MACOSX)
  PostDelayedSpellCheckTask(pending_request_param_.release());
#endif
}

void SpellCheck::OnCustomDictionaryChanged(
    const std::set<std::string>& words_added,
    const std::set<std::string>& words_removed) {
  custom_dictionary_.OnCustomDictionaryChanged(words_added, words_removed);
  if (words_added.empty())
    return;
  DocumentMarkersRemover markersRemover(words_added);
  content::RenderView::ForEach(&markersRemover);
}

void SpellCheck::OnEnableAutoSpellCorrect(bool enable) {
  auto_spell_correct_turned_on_ = enable;
}

void SpellCheck::OnEnableSpellCheck(bool enable) {
  spellcheck_enabled_ = enable;
  UpdateSpellcheckEnabled updater(enable);
  content::RenderView::ForEach(&updater);
}

void SpellCheck::OnRequestDocumentMarkers() {
  DocumentMarkersCollector collector;
  content::RenderView::ForEach(&collector);
  content::RenderThread::Get()->Send(
      new SpellCheckHostMsg_RespondDocumentMarkers(collector.markers()));
}

// TODO(groby): Make sure we always have a spelling engine, even before Init()
// is called.
void SpellCheck::Init(base::File file,
                      const std::set<std::string>& custom_words,
                      const std::string& language) {
  spellcheck_.Init(file.Pass(), language);
  custom_dictionary_.Init(custom_words);
}

bool SpellCheck::SpellCheckWord(
    const base::char16* in_word,
    int in_word_len,
    int tag,
    int* misspelling_start,
    int* misspelling_len,
    std::vector<base::string16>* optional_suggestions) {
  DCHECK(in_word_len >= 0);
  DCHECK(misspelling_start && misspelling_len) << "Out vars must be given.";

  // Do nothing if we need to delay initialization. (Rather than blocking,
  // report the word as correctly spelled.)
  if (InitializeIfNeeded())
    return true;

  return spellcheck_.SpellCheckWord(in_word, in_word_len,
                                    tag,
                                    misspelling_start, misspelling_len,
                                    optional_suggestions);
}

bool SpellCheck::SpellCheckParagraph(
    const base::string16& text,
    WebVector<WebTextCheckingResult>* results) {
#if !defined(OS_MACOSX)
  // Mac has its own spell checker, so this method will not be used.
  DCHECK(results);
  std::vector<WebTextCheckingResult> textcheck_results;
  size_t length = text.length();
  size_t offset = 0;

  // Spellcheck::SpellCheckWord() automatically breaks text into words and
  // checks the spellings of the extracted words. This function sets the
  // position and length of the first misspelled word and returns false when
  // the text includes misspelled words. Therefore, we just repeat calling the
  // function until it returns true to check the whole text.
  int misspelling_start = 0;
  int misspelling_length = 0;
  while (offset <= length) {
    if (SpellCheckWord(&text[offset],
                       length - offset,
                       0,
                       &misspelling_start,
                       &misspelling_length,
                       NULL)) {
      results->assign(textcheck_results);
      return true;
    }

    if (!custom_dictionary_.SpellCheckWord(
            text, misspelling_start + offset, misspelling_length)) {
      base::string16 replacement;
      textcheck_results.push_back(WebTextCheckingResult(
          blink::WebTextDecorationTypeSpelling,
          misspelling_start + offset,
          misspelling_length,
          replacement));
    }
    offset += misspelling_start + misspelling_length;
  }
  results->assign(textcheck_results);
  return false;
#else
  // This function is only invoked for spell checker functionality that runs
  // on the render thread. OSX builds don't have that.
  NOTREACHED();
  return true;
#endif
}

base::string16 SpellCheck::GetAutoCorrectionWord(const base::string16& word,
                                                 int tag) {
  base::string16 autocorrect_word;
  if (!auto_spell_correct_turned_on_)
    return autocorrect_word;  // Return the empty string.

  int word_length = static_cast<int>(word.size());
  if (word_length < 2 ||
      word_length > chrome::spellcheck_common::kMaxAutoCorrectWordSize)
    return autocorrect_word;

  if (InitializeIfNeeded())
    return autocorrect_word;

  base::char16 misspelled_word[
      chrome::spellcheck_common::kMaxAutoCorrectWordSize + 1];
  const base::char16* word_char = word.c_str();
  for (int i = 0; i <= chrome::spellcheck_common::kMaxAutoCorrectWordSize;
       ++i) {
    if (i >= word_length)
      misspelled_word[i] = 0;
    else
      misspelled_word[i] = word_char[i];
  }

  // Swap adjacent characters and spellcheck.
  int misspelling_start, misspelling_len;
  for (int i = 0; i < word_length - 1; i++) {
    // Swap.
    std::swap(misspelled_word[i], misspelled_word[i + 1]);

    // Check spelling.
    misspelling_start = misspelling_len = 0;
    SpellCheckWord(misspelled_word, word_length, tag, &misspelling_start,
        &misspelling_len, NULL);

    // Make decision: if only one swap produced a valid word, then we want to
    // return it. If we found two or more, we don't do autocorrection.
    if (misspelling_len == 0) {
      if (autocorrect_word.empty()) {
        autocorrect_word.assign(misspelled_word);
      } else {
        autocorrect_word.clear();
        break;
      }
    }

    // Restore the swapped characters.
    std::swap(misspelled_word[i], misspelled_word[i + 1]);
  }
  return autocorrect_word;
}

#if !defined(OS_MACOSX)  // OSX uses its own spell checker
void SpellCheck::RequestTextChecking(
    const base::string16& text,
    blink::WebTextCheckingCompletion* completion) {
  // Clean up the previous request before starting a new request.
  if (pending_request_param_.get())
    pending_request_param_->completion()->didCancelCheckingText();

  pending_request_param_.reset(new SpellcheckRequest(
      text, completion));
  // We will check this text after we finish loading the hunspell dictionary.
  if (InitializeIfNeeded())
    return;

  PostDelayedSpellCheckTask(pending_request_param_.release());
}
#endif

bool SpellCheck::InitializeIfNeeded() {
  return spellcheck_.InitializeIfNeeded();
}

#if !defined(OS_MACOSX) // OSX doesn't have |pending_request_param_|
void SpellCheck::PostDelayedSpellCheckTask(SpellcheckRequest* request) {
  if (!request)
    return;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&SpellCheck::PerformSpellCheck, AsWeakPtr(),
                            base::Owned(request)));
}
#endif

#if !defined(OS_MACOSX)  // Mac uses its native engine instead.
void SpellCheck::PerformSpellCheck(SpellcheckRequest* param) {
  DCHECK(param);

  if (!spellcheck_.IsEnabled()) {
    param->completion()->didCancelCheckingText();
  } else {
    WebVector<blink::WebTextCheckingResult> results;
    SpellCheckParagraph(param->text(), &results);
    param->completion()->didFinishCheckingText(results);
  }
}
#endif

void SpellCheck::CreateTextCheckingResults(
    ResultFilter filter,
    int line_offset,
    const base::string16& line_text,
    const std::vector<SpellCheckResult>& spellcheck_results,
    WebVector<WebTextCheckingResult>* textcheck_results) {
  std::vector<WebTextCheckingResult> results;
  for (const SpellCheckResult& spellcheck_result : spellcheck_results) {
    base::string16 replacement = spellcheck_result.replacement;
    SpellCheckResult::Decoration decoration = spellcheck_result.decoration;
    if (filter == USE_NATIVE_CHECKER) {
      DCHECK(!line_text.empty());
      DCHECK_LE(static_cast<size_t>(spellcheck_result.location),
                line_text.length());
      DCHECK_LE(static_cast<size_t>(spellcheck_result.location +
                                    spellcheck_result.length),
                line_text.length());

      const base::string16& misspelled_word = line_text.substr(
          spellcheck_result.location, spellcheck_result.length);

      // Ignore words in custom dictionary.
      if (custom_dictionary_.SpellCheckWord(misspelled_word, 0,
                                            misspelled_word.length())) {
        continue;
      }

      // Use the same types of appostrophes as in the mispelled word.
      PreserveOriginalApostropheTypes(misspelled_word, &replacement);

      // Ignore misspellings due the typographical apostrophe.
      if (misspelled_word == replacement)
        continue;

      // Double-check misspelled words with out spellchecker and attach grammar
      // markers to them if our spellchecker tells us they are correct words,
      // i.e. they are probably contextually-misspelled words.
      int unused_misspelling_start = 0;
      int unused_misspelling_length = 0;
      if (decoration == SpellCheckResult::SPELLING &&
          SpellCheckWord(misspelled_word.c_str(), misspelled_word.length(), 0,
                         &unused_misspelling_start, &unused_misspelling_length,
                         nullptr)) {
        decoration = SpellCheckResult::GRAMMAR;
      }
    } else {
      DCHECK(line_text.empty());
    }

    results.push_back(WebTextCheckingResult(
        static_cast<WebTextDecorationType>(decoration),
        line_offset + spellcheck_result.location, spellcheck_result.length,
        replacement, spellcheck_result.hash));
  }

  textcheck_results->assign(results);
}
