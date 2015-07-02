// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Build;
import android.preference.PreferenceManager;
import android.test.suitebuilder.annotation.MediumTest;
import android.view.View;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.document.DocumentActivity;
import org.chromium.chrome.browser.tab.TabIdManager;
import org.chromium.chrome.test.MultiActivityTestBase;
import org.chromium.chrome.test.util.ActivityUtils;
import org.chromium.chrome.test.util.DisableInTabbedMode;
import org.chromium.chrome.test.util.browser.TabLoadObserver;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content_public.common.ScreenOrientationValues;

import java.lang.ref.WeakReference;
import java.util.List;

/**
 * Tests that WebappActivities are launched correctly.
 *
 * This test seems a little wonky because WebappActivities launched differently, depending on what
 * OS the user is on.  Pre-L, WebappActivities were manually instanced and assigned by the
 * WebappManager.  On L and above, WebappActivities are automatically instanced by Android and the
 * FLAG_ACTIVITY_NEW_DOCUMENT mechanism.  Moreover, we don't have access to the task list pre-L so
 * we have to assume that any non-running WebappActivities are not listed in Android's Overview.
 */
public class WebappModeTest extends MultiActivityTestBase {
    private static final String WEBAPP_1_ID = "webapp_id_1";
    private static final String WEBAPP_1_URL =
            UrlUtils.encodeHtmlDataUri("<html><body bgcolor='#011684'>Webapp 1</body></html>");
    private static final String WEBAPP_1_TITLE = "Web app #1";

    private static final String WEBAPP_2_ID = "webapp_id_2";
    private static final String WEBAPP_2_URL =
            UrlUtils.encodeHtmlDataUri("<html><body bgcolor='#840116'>Webapp 2</body></html>");
    private static final String WEBAPP_2_TITLE = "Web app #2";

    private static final String WEBAPP_ICON = "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAACXB"
            + "IWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH3wQIFB4cxOfiSQAAABl0RVh0Q29tbWVudABDcmVhdGVkIHdpdG"
            + "ggR0lNUFeBDhcAAAAMSURBVAjXY2AUawEAALcAnI/TkI8AAAAASUVORK5CYII=";

    private boolean isNumberOfRunningActivitiesCorrect(final int numActivities) throws Exception {
        return CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Context context = getInstrumentation().getTargetContext();
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP
                        && MultiActivityTestBase.getNumChromeTasks(context) != numActivities) {
                    return false;
                }

                int count = 0;
                List<WeakReference<Activity>> activities = ApplicationStatus.getRunningActivities();
                for (WeakReference<Activity> activity : activities) {
                    if (activity.get() instanceof WebappActivity) count++;
                }
                return count == numActivities;
            }
        });
    }

    private void fireWebappIntent(String id, String url, String title, String icon,
            boolean addMac) throws Exception {
        Intent intent = new Intent();
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setPackage(getInstrumentation().getTargetContext().getPackageName());
        intent.setAction(ChromeLauncherActivity.ACTION_START_WEBAPP);
        intent.putExtra(ShortcutHelper.EXTRA_ID, id);
        intent.putExtra(ShortcutHelper.EXTRA_URL, url);
        intent.putExtra(ShortcutHelper.EXTRA_TITLE, title);
        intent.putExtra(ShortcutHelper.EXTRA_ICON, icon);
        intent.putExtra(ShortcutHelper.EXTRA_ORIENTATION, ScreenOrientationValues.PORTRAIT);
        if (addMac) {
            // Needed for security reasons.  If the MAC is excluded, the URL of the webapp is opened
            // in a browser window, instead.
            String mac = ShortcutHelper.getEncodedMac(getInstrumentation().getTargetContext(), url);
            intent.putExtra(ShortcutHelper.EXTRA_MAC, mac);
        }

        getInstrumentation().getTargetContext().startActivity(intent);
        getInstrumentation().waitForIdleSync();
        MultiActivityTestBase.waitUntilChromeInForeground();
    }

    /**
     * Tests that WebappActivities are started properly.
     */
    @MediumTest
    public void testWebappLaunches() throws Exception {
        // Start the WebappActivity.  We can't use ActivityUtils.waitForActivity() because
        // of the way WebappActivity is instanced on pre-L devices.
        fireWebappIntent(WEBAPP_1_ID, WEBAPP_1_URL, WEBAPP_1_TITLE, WEBAPP_ICON, true);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Activity lastActivity = ApplicationStatus.getLastTrackedFocusedActivity();
                return lastActivity instanceof WebappActivity
                        && lastActivity.findViewById(android.R.id.content).hasWindowFocus();
            }
        }));
        assertTrue(isNumberOfRunningActivitiesCorrect(1));
        final Activity firstActivity = ApplicationStatus.getLastTrackedFocusedActivity();

        // Firing a different Intent should start a new WebappActivity instance.
        fireWebappIntent(WEBAPP_2_ID, WEBAPP_2_URL, WEBAPP_2_TITLE, WEBAPP_ICON, true);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Activity lastActivity = ApplicationStatus.getLastTrackedFocusedActivity();
                return lastActivity instanceof WebappActivity && lastActivity != firstActivity
                        && lastActivity.findViewById(android.R.id.content).hasWindowFocus();
            }
        }));
        assertTrue(isNumberOfRunningActivitiesCorrect(2));

        // Firing the first Intent should bring back the first WebappActivity instance.
        fireWebappIntent(WEBAPP_1_ID, WEBAPP_1_URL, WEBAPP_1_TITLE, WEBAPP_ICON, true);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Activity lastActivity = ApplicationStatus.getLastTrackedFocusedActivity();
                return lastActivity instanceof WebappActivity && lastActivity == firstActivity
                        && lastActivity.findViewById(android.R.id.content).hasWindowFocus();
            }
        }));
        assertTrue(isNumberOfRunningActivitiesCorrect(2));
    }

    /**
     * Tests that the WebappActivity actually gets a legitimate Tab ID instead of 0.
     */
    @MediumTest
    public void testWebappTabIdsProperlyAssigned() throws Exception {
        Context context = getInstrumentation().getTargetContext();
        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(context);
        SharedPreferences.Editor editor = prefs.edit();
        editor.putInt(TabIdManager.PREF_NEXT_ID, 11684);
        editor.apply();

        // Start the WebappActivity.  We can't use ActivityUtils.waitForActivity() because
        // of the way WebappActivity is instanced on pre-L devices.
        fireWebappIntent(WEBAPP_1_ID, WEBAPP_1_URL, WEBAPP_1_TITLE, WEBAPP_ICON, true);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Activity lastActivity = ApplicationStatus.getLastTrackedFocusedActivity();
                return lastActivity instanceof WebappActivity
                        && lastActivity.findViewById(android.R.id.content).hasWindowFocus();
            }
        }));
        assertTrue(isNumberOfRunningActivitiesCorrect(1));
        final WebappActivity webappActivity =
                (WebappActivity) ApplicationStatus.getLastTrackedFocusedActivity();

        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return webappActivity.getActivityTab() != null;
            }
        }));

        assertTrue("Smaller Tab ID was used", 11684 <= webappActivity.getActivityTab().getId());
    }

    /**
     * Tests that a WebappActivity can be brought forward by calling
     * WebContentsDelegateAndroid.activateContents().
     */
    @MediumTest
    public void testActivateContents() throws Exception {
        final Context context = getInstrumentation().getTargetContext();

        // Start the WebappActivity.
        fireWebappIntent(WEBAPP_1_ID, WEBAPP_1_URL, WEBAPP_1_TITLE, WEBAPP_ICON, true);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Activity lastActivity = ApplicationStatus.getLastTrackedFocusedActivity();
                View rootView = lastActivity.findViewById(android.R.id.content);
                return lastActivity instanceof WebappActivity && rootView.hasWindowFocus();
            }
        }));
        assertTrue(isNumberOfRunningActivitiesCorrect(1));

        // Return home.
        final WebappActivity activity =
                (WebappActivity) ApplicationStatus.getLastTrackedFocusedActivity();
        MultiActivityTestBase.launchHomescreenIntent(context);
        getInstrumentation().waitForIdleSync();

        // Bring it back via the Tab.
        activity.getActivityTab().getChromeWebContentsDelegateAndroid().activateContents();
        getInstrumentation().waitForIdleSync();
        MultiActivityTestBase.waitUntilChromeInForeground();
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return activity == ApplicationStatus.getLastTrackedFocusedActivity()
                        && activity.hasWindowFocus();
            }
        }));
        assertTrue(isNumberOfRunningActivitiesCorrect(1));
    }

    /**
     * Ensure WebappActivities can't be launched without proper security checks.
     */
    @MediumTest
    public void testWebappRequiresValidMac() throws Exception {
        // Try to start a WebappActivity.  Fail because the Intent is insecure.
        fireWebappIntent(WEBAPP_1_ID, WEBAPP_1_URL, WEBAPP_1_TITLE, WEBAPP_ICON, false);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Activity lastActivity = ApplicationStatus.getLastTrackedFocusedActivity();
                if (!lastActivity.findViewById(android.R.id.content).hasWindowFocus()) return false;
                return lastActivity instanceof ChromeTabbedActivity
                        || lastActivity instanceof DocumentActivity;
            }
        }));
        final Activity firstActivity = ApplicationStatus.getLastTrackedFocusedActivity();

        // Firing a correct Intent should start a new WebappActivity instance.
        fireWebappIntent(WEBAPP_2_ID, WEBAPP_2_URL, WEBAPP_2_TITLE, WEBAPP_ICON, true);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Activity lastActivity = ApplicationStatus.getLastTrackedFocusedActivity();
                return lastActivity instanceof WebappActivity && lastActivity != firstActivity
                        && lastActivity.findViewById(android.R.id.content).hasWindowFocus();
            }
        }));
    }

    /**
     * Tests that WebappActivities handle window.open() properly in document mode.
     */
    @DisableInTabbedMode
    @MediumTest
    public void testWebappHandlesWindowOpenInDocumentMode() throws Exception {
        triggerWindowOpenAndWaitForLoad(DocumentActivity.class);
    }

    /**
     * Tests that WebappActivities handle window.open() properly in tabbed mode.
     */
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_DOCUMENT_MODE)
    @MediumTest
    public void testWebappHandlesWindowOpenInTabbedMode() throws Exception {
        triggerWindowOpenAndWaitForLoad(ChromeTabbedActivity.class);
    }

    private <T extends ChromeActivity> void triggerWindowOpenAndWaitForLoad(Class<T> classToWaitFor)
            throws Exception {
        // Start the WebappActivity.  We can't use ActivityUtils.waitForActivity() because
        // of the way WebappActivity is instanced on pre-L devices.
        fireWebappIntent(WEBAPP_1_ID, WEBAPP_1_URL, WEBAPP_1_TITLE, WEBAPP_ICON, true);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Activity lastActivity = ApplicationStatus.getLastTrackedFocusedActivity();
                return lastActivity instanceof WebappActivity
                        && lastActivity.findViewById(android.R.id.content).hasWindowFocus();
            }
        }));
        assertTrue(isNumberOfRunningActivitiesCorrect(1));
        final WebappActivity webappActivity =
                (WebappActivity) ApplicationStatus.getLastTrackedFocusedActivity();

        // Load up the test page.
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return webappActivity.getActivityTab() != null;
            }
        }));
        assertTrue(CriteriaHelper.pollForCriteria(
                new TabLoadObserver(webappActivity.getActivityTab(), ONCLICK_LINK)));

        // Do a plain click to make the link open in the main browser via a window.open().
        // If the window is opened successfully, javascript on the first page triggers and changes
        // its URL as a signal for this test.
        Runnable fgTrigger = new Runnable() {
            @Override
            public void run() {
                try {
                    DOMUtils.clickNode(null, webappActivity.getCurrentContentViewCore(), "body");
                } catch (Exception e) {
                }
            }
        };
        ChromeActivity secondActivity = ActivityUtils.waitForActivity(
                getInstrumentation(), classToWaitFor, fgTrigger);
        waitForFullLoad(secondActivity, "Page 4");
        assertEquals("New WebContents was not created",
                SUCCESS_URL, webappActivity.getActivityTab().getUrl());
        assertNotSame("Wrong Activity in foreground",
                webappActivity, ApplicationStatus.getLastTrackedFocusedActivity());
    }
}
