// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.shell;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.ContentViewRenderView;
import org.chromium.ui.base.WindowAndroid;

import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Basic sanity test for loading urls in ChromeShell.
 */
public class ChromeShellUrlTest extends ChromeShellTestBase {
    // URL used for base tests.
    private static final String URL = "data:text";

    @SmallTest
    @Feature({"Main"})
    public void testBaseStartup() throws InterruptedException {
        ChromeShellActivity activity = launchChromeShellWithUrl(URL);
        waitForActiveShellToBeDoneLoading();

        // Make sure the activity was created as expected.
        assertNotNull(activity);
    }

    @SmallTest
    @Feature({"Main"})
    public void testChromeUrlPageLoads() throws InterruptedException {
        // Test flags page because it will *probably* never get removed.
        String flagsUrl = "chrome://flags/";
        final ChromeShellActivity activity = launchChromeShellWithUrl(flagsUrl);
        waitForActiveShellToBeDoneLoading();

        // Make sure the activity was created as expected.
        assertNotNull(activity);

        // Ensure we have a valid ContentViewCore.
        final AtomicReference<ContentViewCore> contentViewCore =
                new AtomicReference<ContentViewCore>();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                contentViewCore.set(activity.getActiveContentViewCore());
            }
        });
        assertNotNull(contentViewCore.get());
        assertNotNull(contentViewCore.get().getContainerView());

        // Ensure the correct page has been loaded, ie. not interstitial, and title/url should
        // be sane.
        final AtomicBoolean isShowingInterstitialPage = new AtomicBoolean();
        final AtomicReference<String> url = new AtomicReference<String>();
        final AtomicReference<String> title = new AtomicReference<String>();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                isShowingInterstitialPage.set(contentViewCore.get().getWebContents()
                        .isShowingInterstitialPage());
                url.set(contentViewCore.get().getWebContents().getUrl());
                title.set(contentViewCore.get().getWebContents().getTitle());
            }
        });
        assertFalse("Showed interstitial page instead of welcome page",
                isShowingInterstitialPage.get());
        assertNotNull("URL was null", url.get());
        assertTrue("URL did not contain: " + flagsUrl + ". Was: " + url.get(),
                url.get().contains(flagsUrl));
        assertNotNull("Title was null", title.get());
    }

    /**
     * Tests that creating an extra ContentViewRenderView does not cause an assert because we would
     * initialize the compositor twice http://crbug.com/162312
     */
    @SmallTest
    @Feature({"Main"})
    public void testCompositorInit() throws InterruptedException {
        // Start the ChromeShell, this loads the native library and create an instance of
        // ContentViewRenderView.
        final ChromeShellActivity activity = launchChromeShellWithUrl(URL);
        waitForActiveShellToBeDoneLoading();

        // Now create a new ContentViewRenderView, it should not assert.
        try {
            runTestOnUiThread(new Runnable() {
                @Override
                public void run() {
                    WindowAndroid windowAndroid = new WindowAndroid(
                            getInstrumentation().getTargetContext().getApplicationContext());
                    ContentViewRenderView contentViewRenderView =
                            new ContentViewRenderView(getInstrumentation().getTargetContext());
                    contentViewRenderView.onNativeLibraryLoaded(windowAndroid);
                    contentViewRenderView.setCurrentContentViewCore(
                            activity.getActiveContentViewCore());
                }
            });
        } catch (Throwable e) {
            e.printStackTrace();
            fail("Could not create a ContentViewRenderView: " + e);
        }
    }
}
