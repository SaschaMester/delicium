// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.ComponentName;
import android.content.Intent;
import android.net.Uri;

import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

/**
 * Base class for all instrumentation tests that require a {@link CustomTabActivity}.
 */
public abstract class CustomTabActivityTestBase extends
        ChromeActivityTestCaseBase<CustomTabActivity> {

    public CustomTabActivityTestBase() {
        super(CustomTabActivity.class);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
    }

    @Override
    protected void startActivityCompletely(Intent intent) {
        Instrumentation.ActivityMonitor monitor = getInstrumentation().addMonitor(
                CustomTabActivity.class.getName(), null, false);
        Activity activity = getInstrumentation().startActivitySync(intent);
        assertNotNull("Main activity did not start", activity);
        CustomTabActivity customTabActivity =
                (CustomTabActivity) monitor.waitForActivityWithTimeout(
                ACTIVITY_START_TIMEOUT_MS);
        assertNotNull("CustomTabActivity did not start", customTabActivity);
        setActivity(customTabActivity);
    }

    /**
     * Start a {@link CustomTabActivity} with given {@link Intent}, and wait till a tab is
     * initialized.
     */
    protected void startCustomTabActivityWithIntent(Intent intent) throws InterruptedException {
        startActivityCompletely(intent);
        assertTrue("Tab never selected/initialized.",
                CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return getActivity().getActivityTab() != null;
                    }
                }));
        Tab tab = getActivity().getActivityTab();

        assertTrue("Deferred startup never completed",
                CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return DeferredStartupHandler.getInstance().isDeferredStartupComplete();
                    }
                }));

        assertNotNull(tab);
        assertNotNull(tab.getView());
    }

    /**
     * Creates the simplest intent that is sufficient to let {@link ChromeLauncherActivity} launch
     * the {@link CustomTabActivity}.
     */
    protected Intent createMinimalCustomTabIntent(String url) {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setComponent(new ComponentName(getInstrumentation().getTargetContext(),
                ChromeLauncherActivity.class));
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_CUSTOM_TABS_SESSION_ID, -1);
        return intent;
    }
}
