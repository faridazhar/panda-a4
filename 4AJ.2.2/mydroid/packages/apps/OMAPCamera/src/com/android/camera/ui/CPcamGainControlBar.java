/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.ti.omap.android.camera.ui;

import android.content.Context;
import android.content.res.Configuration;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;

import com.ti.omap.android.camera.R;
import com.ti.omap.android.camera.Util;
import android.util.Log;
/**
 * A view that contains camera Gain control and its layout.
 */
public class CPcamGainControlBar extends CPcamGainControl {
    @SuppressWarnings("unused")
    private static final String TAG = "GainControlBar";
    private static final int THRESHOLD_FIRST_MOVE = Util.dpToPixel(10); // pixels
    // Space between indicator icon and the Gain-in/out icon.
    private static final int ICON_SPACING = Util.dpToPixel(12);

    private View mBar;
    private boolean mStartChanging;
    private int mSliderPosition = 0;
    private int mSliderLength;
    // The width of the Gain control bar (including the '+', '-' icons and the
    // slider bar) for phone in portrait orientation, or the height of that
    // for phone in landscape orientation.
    private int mSize;
    // The width of the '+' icon (the same as '-' icon) for phone in portrait
    // orientation, or the height of that for phone in landscape orientation.
    private int mIconSize;
    // mIconSize + padding
    private int mTotalIconSize;

    public CPcamGainControlBar(Context context, AttributeSet attrs) {
        super(context, attrs);
        mBar = new View(context);
        mBar.setBackgroundResource(R.drawable.zoom_slider_bar);
        addView(mBar);
    }

    @Override
    public void setActivated(boolean activated) {
        super.setActivated(activated);
        mBar.setActivated(activated);
    }

    private int getSliderPosition(int offset) {
        // Calculate the absolute offset of the slider in the Gain control bar.
        // For left-hand users, as the device is rotated for 180 degree for
        // landscape mode, the Gain-in bottom should be on the top, so the
        // position should be reversed.
        int pos; // the relative position in the Gain slider bar
        if (getResources().getConfiguration().orientation
                == Configuration.ORIENTATION_LANDSCAPE) {
            if (mOrientation == 180) {
                pos = offset - mTotalIconSize;
            } else {
                pos = mSize - mTotalIconSize - offset;
            }
        } else {
            if (mOrientation == 90) {
                pos = mSize - mTotalIconSize - offset;
            } else {
                pos = offset - mTotalIconSize;
            }
        }
        if (pos < 0) pos = 0;
        if (pos > mSliderLength) pos = mSliderLength;
        return pos;
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        if (getResources().getConfiguration().orientation
                == Configuration.ORIENTATION_LANDSCAPE) {
            mSize = h;
            mIconSize = mGainIn.getMeasuredHeight();
        } else {
            mSize = w;
            mIconSize = mGainIn.getMeasuredWidth();
        }
        mTotalIconSize = mIconSize + ICON_SPACING;
        mSliderLength = mSize  - (2 * mTotalIconSize);
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent event) {
        if (!isEnabled() || (mSize == 0)) return false;
        int action = event.getAction();

        switch (action) {
            case MotionEvent.ACTION_OUTSIDE:
            case MotionEvent.ACTION_CANCEL:
            case MotionEvent.ACTION_UP:
                setActivated(false);
                closeGainControl();
                break;

            case MotionEvent.ACTION_DOWN:
                setActivated(true);
                mStartChanging = false;
            case MotionEvent.ACTION_MOVE:
                boolean isLandscape = (getResources().getConfiguration().orientation
                        == Configuration.ORIENTATION_LANDSCAPE);
                int pos = getSliderPosition((int)
                        (isLandscape ? event.getY() : event.getX()));
                if (!mStartChanging) {
                    // Make sure the movement is large enough before we start
                    // changing the Gain.
                    int delta = mSliderPosition - pos;
                    if ((delta > THRESHOLD_FIRST_MOVE) ||
                            (delta < -THRESHOLD_FIRST_MOVE)) {
                        mStartChanging = true;
                    }
                }
                if (mStartChanging) {
                    performGain(1.0d * pos / mSliderLength);
                    mSliderPosition = pos;
                }
                requestLayout();
        }
        return true;
    }

    @Override
    public void setOrientation(int orientation, boolean animation) {
        // layout for the left-hand camera control
        if ((orientation == 180) || (mOrientation == 180)) requestLayout();
        super.setOrientation(orientation, animation);
    }

    @Override
    protected void onLayout(
            boolean changed, int left, int top, int right, int bottom) {
        boolean isLandscape = (getResources().getConfiguration().orientation
                == Configuration.ORIENTATION_LANDSCAPE);
        if (mGainMax == 0) return;
        int size = 0;

        if (isLandscape) {
            size = right - left;
            mBar.layout(0, mTotalIconSize, size, mSize - mTotalIconSize);
        } else {
            size = bottom - top;
            mBar.layout(mTotalIconSize, 0, mSize - mTotalIconSize, size);
        }
        // For left-hand users, as the device is rotated for 180 degree,
        // the Gain-in button should be on the top.
        int pos; // slider position
        int sliderPosition;
        if (mSliderPosition != -1) { // -1 means invalid
            sliderPosition = mSliderPosition;
        } else {
            sliderPosition = (int) ((double) mSliderLength * mGainIndex / mGainMax);
        }

        if (isLandscape) {
            if (mOrientation == 180) {
                mGainOut.layout(0, 0, size, mIconSize);
                mGainIn.layout(0, mSize - mIconSize, size, mSize);
                pos = mBar.getTop() + sliderPosition;
            } else {
                mGainIn.layout(0, 0, size, mIconSize);
                mGainOut.layout(0, mSize - mIconSize, size, mSize);
                pos = mBar.getBottom() - sliderPosition;
            }
            int sliderHeight = mGainSlider.getMeasuredHeight();
            mGainSlider.layout(0, (pos - sliderHeight / 2), size,
                    (pos + sliderHeight / 2));
        } else {
            if (mOrientation == 90) {
                mGainIn.layout(0, 0, mIconSize, size);
                mGainOut.layout(mSize - mIconSize, 0, mSize, size);
                pos = mBar.getRight() - sliderPosition;
            } else {
                mGainOut.layout(0, 0, mIconSize, size);
                mGainIn.layout(mSize - mIconSize, 0, mSize, size);
                pos = mBar.getLeft() + sliderPosition;
            }
            int sliderWidth = mGainSlider.getMeasuredWidth();
            mGainSlider.layout((pos - sliderWidth / 2), 0,
                    (pos + sliderWidth / 2), size);
        }
    }

    @Override
    public void setGainIndex(int index) {
        super.setGainIndex(index);
        mSliderPosition = -1; // -1 means invalid
    }
}
