// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.scene_layer;

import android.content.Context;

import org.chromium.base.JNINamespace;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutTab;
import org.chromium.ui.resources.ResourceManager;

/**
 * The Java component of what is basically a CC Layer that manages drawing the Tab Strip (which is
 * composed of {@link StripLayoutTab}s) to the screen.  This object keeps the layers up to date and
 * removes/creates children as necessary.  This object is built by its native counterpart.
 */
@JNINamespace("chrome::android")
public class TabStripSceneLayer extends SceneOverlayLayer {
    private long mNativePtr;
    private final float mDpToPx;
    private SceneLayer mChildSceneLayer;

    public TabStripSceneLayer(Context context) {
        mDpToPx = context.getResources().getDisplayMetrics().density;
    }

    @Override
    protected void initializeNative() {
        if (mNativePtr == 0) {
            mNativePtr = nativeInit();
        }
        assert mNativePtr != 0;
    }

    @Override
    public void setContentTree(SceneLayer contentTree) {
        nativeSetContentTree(mNativePtr, contentTree);
    }

    /**
     * Pushes all relevant {@link StripLayoutTab}s to the CC Layer tree.
     * This also pushes any other assets required to draw the Tab Strip.  This should only be called
     * when the Compositor has disabled ScheduleComposite calls as this will change the tree and
     * could subsequently cause unnecessary follow up renders.
     *
     * @param layoutHelper A layout helper for the tab strip.
     * @param layerTitleCache A layer title cache.
     * @param resourceManager A resource manager.
     * @param stripLayoutTabsToRender Array of strip layout tabs.
     * @param yOffset Current top controls offset in dp.
     */
    public void pushAndUpdateStrip(StripLayoutHelperManager layoutHelper,
            LayerTitleCache layerTitleCache, ResourceManager resourceManager,
            StripLayoutTab[] stripLayoutTabsToRender, float yOffset) {
        if (mNativePtr == 0) return;

        nativeBeginBuildingFrame(mNativePtr);
        pushButtonsAndBackground(layoutHelper, resourceManager, yOffset);
        pushStripTabs(layoutHelper, layerTitleCache, resourceManager, stripLayoutTabsToRender);
        nativeFinishBuildingFrame(mNativePtr);
    }

    private void pushButtonsAndBackground(StripLayoutHelperManager layoutHelper,
            ResourceManager resourceManager, float yOffset) {
        nativeUpdateTabStripLayer(mNativePtr, layoutHelper.getWidth() * mDpToPx,
                layoutHelper.getHeight() * mDpToPx, yOffset * mDpToPx,
                layoutHelper.getStripBrightness());

        CompositorButton newTabButton = layoutHelper.getNewTabButton();
        CompositorButton modelSelectorButton = layoutHelper.getModelSelectorButton();
        boolean newTabButtonVisible = newTabButton.isVisible();
        boolean modelSelectorButtonVisible = modelSelectorButton.isVisible();

        nativeUpdateNewTabButton(mNativePtr, newTabButton.getResourceId(),
                newTabButton.getX() * mDpToPx, newTabButton.getY() * mDpToPx,
                newTabButton.getWidth() * mDpToPx, newTabButton.getHeight() * mDpToPx,
                newTabButtonVisible, resourceManager);

        nativeUpdateModelSelectorButton(mNativePtr, modelSelectorButton.getResourceId(),
                modelSelectorButton.getX() * mDpToPx, modelSelectorButton.getY() * mDpToPx,
                modelSelectorButton.getWidth() * mDpToPx, modelSelectorButton.getHeight() * mDpToPx,
                modelSelectorButton.isIncognito(), modelSelectorButtonVisible, resourceManager);
    }

    private void pushStripTabs(StripLayoutHelperManager layoutHelper,
            LayerTitleCache layerTitleCache, ResourceManager resourceManager,
            StripLayoutTab[] stripTabs) {
        final int tabsCount = stripTabs != null ? stripTabs.length : 0;

        for (int i = 0; i < tabsCount; i++) {
            final StripLayoutTab st = stripTabs[i];
            nativePutStripTabLayer(mNativePtr, st.getId(), st.getCloseButton().getResourceId(),
                    st.getResourceId(i == tabsCount - 1), i == tabsCount - 1, st.getClosePressed(),
                    layoutHelper.getWidth() * mDpToPx, st.getDrawX() * mDpToPx,
                    st.getDrawY() * mDpToPx, st.getWidth() * mDpToPx, st.getHeight() * mDpToPx,
                    st.getContentOffsetX() * mDpToPx, st.getCloseButton().getOpacity(),
                    st.isLoading(), layoutHelper.getBorderOpacity(), layerTitleCache,
                    resourceManager);
        }
    }

    @Override
    public void destroy() {
        super.destroy();
        mNativePtr = 0;
    }

    private native long nativeInit();
    private native void nativeBeginBuildingFrame(long nativeTabStripSceneLayer);
    private native void nativeFinishBuildingFrame(long nativeTabStripSceneLayer);
    private native void nativeUpdateTabStripLayer(long nativeTabStripSceneLayer, float width,
            float height, float yOffset, float stripBrightness);
    private native void nativeUpdateNewTabButton(long nativeTabStripSceneLayer, int resourceId,
            float x, float y, float width, float height, boolean visible,
            ResourceManager resourceManager);
    private native void nativeUpdateModelSelectorButton(long nativeTabStripSceneLayer,
            int resourceId, float x, float y, float width, float height, boolean incognito,
            boolean visible, ResourceManager resourceManager);
    private native void nativePutStripTabLayer(long nativeTabStripSceneLayer, int id,
            int closeResourceId, int handleResourceId, boolean foreground, boolean closePressed,
            float toolbarWidth, float x, float y, float width, float height, float contentOffsetX,
            float closeButtonAlpha, boolean isLoading, float borderOpacity,
            LayerTitleCache layerTitleCache, ResourceManager resourceManager);
    private native void nativeSetContentTree(long nativeTabStripSceneLayer, SceneLayer contentTree);
}
