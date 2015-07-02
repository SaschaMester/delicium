// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.ntp.NewTabPage;

/**
 * Defines the data that is exposed to properly render the Toolbar.
 */
public interface ToolbarDataProvider {
    /**
     * @return The tab that contains the information currently displayed in the toolbar.
     */
    Tab getTab();

    /**
     * @return The NewTabPage shown for the current Tab or null if one is not being shown.
     */
    NewTabPage getNewTabPageForCurrentTab();

    /**
     * @return Whether the toolbar is currently being displayed for incognito.
     */
    boolean isIncognito();

    /**
     * @return The load progress for the current tab (0 being no load progress, 100 being
     *         fully loaded).
     */
    @VisibleForTesting
    int getLoadProgress();

    /**
     * @return The chip text from the search URL.
     */
    String getCorpusChipText();

    /**
     * @return The formatted text (URL or search terms) for display.
     */
    String getText();

    /**
     * @return Whether the text to display is a search query replacing the URL.
     */
    boolean wouldReplaceURL();

    /**
     * @return The primary color to use for the background drawable.
     */
    int getPrimaryColor();

    /**
     * @return Whether the current primary color is a brand color.
     */
    boolean isUsingBrandColor();
}
