// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Intent;
import android.net.Uri;
import android.text.TextUtils;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.IntentHandler.ExternalAppId;
import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.appmenu.AppMenuPropertiesDelegate;
import org.chromium.chrome.browser.appmenu.ChromeAppMenuPropertiesDelegate;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanel.StateChangeReason;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerDocument;
import org.chromium.chrome.browser.document.BrandColorUtils;
import org.chromium.chrome.browser.tabmodel.SingleTabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.toolbar.ToolbarControlContainer;
import org.chromium.chrome.browser.widget.findinpage.FindToolbarManager;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationEntry;

/**
 * The activity for custom tabs. It will be launched on top of a client's task.
 */
public class CustomTabActivity extends ChromeActivity {
    private static CustomTabContentHandler sActiveContentHandler;

    private CustomTab mTab;
    private FindToolbarManager mFindToolbarManager;
    private CustomTabIntentDataProvider mIntentDataProvider;
    private long mSessionId;
    private CustomTabContentHandler mCustomTabContentHandler;

    // This is to give the right package name while using the client's resources during an
    // overridePendingTransition call.
    // TODO(ianwen, yusufo): Figure out a solution to extract external resources without having to
    // change the package name.
    private boolean mShouldOverridePackage;

    private boolean mRecordedStartupUma;

    /**
     * Sets the currently active {@link CustomTabContentHandler} in focus.
     * @param contentHandler {@link CustomTabContentHandler} to set.
     */
    public static void setActiveContentHandler(CustomTabContentHandler contentHandler) {
        sActiveContentHandler = contentHandler;
    }

    /**
     * Used to check whether an incoming intent can be handled by the
     * current {@link CustomTabContentHandler}.
     * @return Whether the active {@link CustomTabContentHandler} has handled the intent.
     */
    public static boolean handleInActiveContentIfNeeded(Intent intent) {
        if (sActiveContentHandler == null) return false;

        long intentSessionId = intent.getLongExtra(
                CustomTabIntentDataProvider.EXTRA_CUSTOM_TABS_SESSION_ID,
                CustomTabIntentDataProvider.INVALID_SESSION_ID);
        if (intentSessionId == CustomTabIntentDataProvider.INVALID_SESSION_ID) return false;

        if (sActiveContentHandler.getSessionId() != intentSessionId) return false;
        String url = IntentHandler.getUrlFromIntent(intent);
        if (TextUtils.isEmpty(url)) return false;
        sActiveContentHandler.loadUrlAndTrackFromTimestamp(new LoadUrlParams(url),
                IntentHandler.getTimestampFromIntent(intent));
        return true;
    }

    @Override
    public void onStart() {
        super.onStart();
        CustomTabsConnection.getInstance(getApplication())
                .keepAliveForSessionId(mIntentDataProvider.getSessionId(),
                        mIntentDataProvider.getKeepAliveServiceIntent());
    }

    @Override
    public void onStop() {
        super.onStop();
        CustomTabsConnection.getInstance(getApplication())
                .dontKeepAliveForSessionId(mIntentDataProvider.getSessionId());
    }

    @Override
    public void preInflationStartup() {
        super.preInflationStartup();
        setTabModelSelector(new SingleTabModelSelector(this, false, true) {
            @Override
            public Tab openNewTab(LoadUrlParams loadUrlParams, TabLaunchType type, Tab parent,
                    boolean incognito) {
                // A custom tab either loads a url or starts an external activity to handle the url.
                // It never opens a new tab/chrome activity.
                mTab.loadUrl(loadUrlParams);
                return mTab;
            }
        });
        mIntentDataProvider = new CustomTabIntentDataProvider(getIntent(), this);
    }

    @Override
    public void postInflationStartup() {
        super.postInflationStartup();
        getToolbarManager().setCloseButtonIcon(mIntentDataProvider.getCloseButtonIconResId());
        getToolbarManager().setShowTitle(mIntentDataProvider.getTitleVisibilityState()
                == CustomTabIntentDataProvider.CUSTOM_TAB_SHOW_PAGE_TITLE);
        getToolbarManager().updatePrimaryColor(mIntentDataProvider.getToolbarColor());
        setStatusBarColor(mIntentDataProvider.getToolbarColor());
        if (mIntentDataProvider.shouldShowActionButton()) {
            getToolbarManager().addCustomActionButton(mIntentDataProvider.getActionButtonIcon(),
                    new OnClickListener() {
                        @Override
                        public void onClick(View v) {
                            mIntentDataProvider.sendButtonPendingIntentWithUrl(
                                    getApplicationContext(), mTab.getUrl());
                            RecordUserAction.record("CustomTabsCustomActionButtonClick");
                        }
                    });
        }
    }

    @Override
    public void finishNativeInitialization() {
        String url = IntentHandler.getUrlFromIntent(getIntent());
        String referrer = IntentHandler.getReferrerUrlIncludingExtraHeaders(getIntent(), this);
        mSessionId = mIntentDataProvider.getSessionId();
        // If extra headers have been passed, cancel any current prerender, as
        // prerendering doesn't support extra headers.
        if (IntentHandler.getExtraHeadersFromIntent(getIntent()) != null) {
            CustomTabsConnection.getInstance(getApplication())
                    .takePrerenderedUrl(mSessionId, "", null);
        }

        mTab = new CustomTab(
                this, getWindowAndroid(), mSessionId, url, referrer, Tab.INVALID_TAB_ID);
        getTabModelSelector().setTab(mTab);

        ToolbarControlContainer controlContainer = (ToolbarControlContainer) findViewById(
                R.id.control_container);
        LayoutManagerDocument layoutDriver = new LayoutManagerDocument(getCompositorViewHolder());
        initializeCompositorContent(layoutDriver, findViewById(R.id.url_bar),
                (ViewGroup) findViewById(android.R.id.content), controlContainer);
        mFindToolbarManager = new FindToolbarManager(this, getTabModelSelector(),
                getToolbarManager().getContextualMenuBar().getCustomSelectionActionModeCallback());
        getToolbarManager().initializeWithNative(getTabModelSelector(), getFullscreenManager(),
                mFindToolbarManager, null, layoutDriver, null, null, null,
                new OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        CustomTabActivity.this.finish();
                    }
                });

        mTab.setFullscreenManager(getFullscreenManager());
        loadUrlInCurrentTab(new LoadUrlParams(url),
                IntentHandler.getTimestampFromIntent(getIntent()));
        mCustomTabContentHandler = new CustomTabContentHandler() {
            @Override
            public void loadUrlAndTrackFromTimestamp(LoadUrlParams params, long timestamp) {
                loadUrlInCurrentTab(params, timestamp);
            }

            @Override
            public long getSessionId() {
                return mSessionId;
            }
        };
        super.finishNativeInitialization();
    }

    @Override
    public void onStartWithNative() {
        super.onStartWithNative();
        setActiveContentHandler(mCustomTabContentHandler);

        if (!mRecordedStartupUma) {
            mRecordedStartupUma = true;
            ExternalAppId externalId =
                    IntentHandler.determineExternalIntentSource(getPackageName(), getIntent());
            RecordHistogram.recordEnumeratedHistogram("CustomTabs.ClientAppId",
                    externalId.ordinal(), ExternalAppId.INDEX_BOUNDARY.ordinal());
        }
    }

    @Override
    public void onStopWithNative() {
        super.onStopWithNative();
        setActiveContentHandler(null);
    }

    /**
     * Loads the current tab with the given load params. Unlike
     * {@link CustomTab#loadUrlAndTrackFromTimestamp(LoadUrlParams, long)}, this method takes client
     * referrer and extra headers into account.
     */
    private void loadUrlInCurrentTab(LoadUrlParams params, long timeStamp) {
        Intent intent = getIntent();
        IntentHandler.addReferrerAndHeaders(params, intent, this);
        mTab.loadUrlAndTrackFromTimestamp(params, timeStamp);
    }

    /**
     * Calculate the proper color for status bar and update it. Only works on L and future versions.
     */
    private void setStatusBarColor(int color) {
        // If the client did not specify the toolbar color, we do not change the status bar color.
        if (color == getResources().getColor(R.color.default_primary_color)) return;

        ApiCompatibilityUtils.setStatusBarColor(getWindow(),
                BrandColorUtils.computeStatusBarColor(color));
    }

    @Override
    public boolean createContextualSearchTab(ContentViewCore searchContentViewCore) {
        if (mTab == null) return false;
        NavigationEntry entry =
                searchContentViewCore.getWebContents().getNavigationController().getPendingEntry();
        String url = entry != null
                ? entry.getUrl() : searchContentViewCore.getWebContents().getUrl();
        mTab.loadUrl(new LoadUrlParams(url));
        return false;
    }

    @Override
    public SingleTabModelSelector getTabModelSelector() {
        return (SingleTabModelSelector) super.getTabModelSelector();
    }

    @Override
    protected ChromeAppMenuPropertiesDelegate createAppMenuPropertiesDelegate() {
        return new CustomTabAppMenuPropertiesDelegate(this, mIntentDataProvider.getMenuTitles());
    }

    @Override
    protected int getAppMenuLayoutId() {
        return R.menu.custom_tabs_menu;
    }

    @Override
    protected int getControlContainerLayoutId() {
        return R.layout.custom_tabs_control_container;
    }

    @Override
    protected int getControlContainerHeightResource() {
        return R.dimen.custom_tabs_control_container_height;
    }

    @Override
    public String getPackageName() {
        if (mShouldOverridePackage) return mIntentDataProvider.getClientPackageName();
        return super.getPackageName();
    }

    @Override
    public void finish() {
        super.finish();
        if (mIntentDataProvider.shouldAnimateOnFinish()) {
            mShouldOverridePackage = true;
            overridePendingTransition(mIntentDataProvider.getAnimationEnterRes(),
                    mIntentDataProvider.getAnimationExitRes());
            mShouldOverridePackage = false;
        }
    }

    @Override
    protected boolean handleBackPressed() {
        if (mTab == null) return false;
        if (mTab.canGoBack()) {
            mTab.goBack();
        } else {
            finish();
        }
        return true;
    }

    @Override
    public boolean shouldShowAppMenu() {
        return mTab != null && getToolbarManager().isInitialized();
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        int menuIndex = getAppMenuPropertiesDelegate().getIndexOfMenuItem(item);
        if (menuIndex >= 0) {
            mIntentDataProvider.clickMenuItemWithUrl(getApplicationContext(), menuIndex,
                    getTabModelSelector().getCurrentTab().getUrl());
            RecordUserAction.record("CustomTabsMenuCustomMenuItem");
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    @Override
    public boolean onMenuOrKeyboardAction(int id, boolean fromMenu) {
        if (id == R.id.show_menu) {
            if (shouldShowAppMenu()) {
                getAppMenuHandler().showAppMenu(getToolbarManager().getMenuAnchor(), true, false);
                return true;
            }
        } else if (id == R.id.open_in_chrome_id) {
            String url = getTabModelSelector().getCurrentTab().getUrl();
            Intent chromeIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
            chromeIntent.setPackage(getApplicationContext().getPackageName());
            chromeIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            startActivity(chromeIntent);
            RecordUserAction.record("CustomTabsMenuOpenInChrome");
            return true;
        } else if (id == R.id.find_in_page_id) {
            mFindToolbarManager.showToolbar();
            if (getContextualSearchManager() != null) {
                getContextualSearchManager().hideContextualSearch(StateChangeReason.UNKNOWN);
            }
            if (fromMenu) {
                RecordUserAction.record("MobileMenuFindInPage");
            } else {
                RecordUserAction.record("MobileShortcutFindInPage");
            }
            return true;
        }
        return super.onMenuOrKeyboardAction(id, fromMenu);
    }

    /**
     * @return The {@link AppMenuPropertiesDelegate} associated with this activity. For test
     *         purposes only.
     */
    @VisibleForTesting
    @Override
    public CustomTabAppMenuPropertiesDelegate getAppMenuPropertiesDelegate() {
        return (CustomTabAppMenuPropertiesDelegate) super.getAppMenuPropertiesDelegate();
    }

    /**
     * @return The {@link CustomTabIntentDataProvider} for this {@link CustomTabActivity}. For test
     *         purposes only.
     */
    @VisibleForTesting
    CustomTabIntentDataProvider getIntentDataProvider() {
        return mIntentDataProvider;
    }
}
