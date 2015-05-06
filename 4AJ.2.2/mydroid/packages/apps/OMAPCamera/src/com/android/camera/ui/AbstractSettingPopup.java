/*
 * Copyright (C) 2010 The Android Open Source Project
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

import java.util.ArrayList;
import android.content.Context;
import android.util.AttributeSet;
import android.view.ViewGroup;
import android.widget.TextView;

import com.ti.omap.android.camera.R;
import com.ti.omap.android.camera.ListPreference;

// A popup window that shows one or more camera settings.
abstract public class AbstractSettingPopup extends RotateLayout {
    protected ViewGroup mSettingList;
    protected TextView mTitle;

    public AbstractSettingPopup(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mTitle = (TextView) findViewById(R.id.title);
        mSettingList = (ViewGroup) findViewById(R.id.settingList);
    }

    public boolean findPref(String key){
        return false;
    }

    public void replace(String key, ListPreference pref,
            ArrayList<CharSequence[]> allEntries,
            ArrayList<CharSequence[]> allEntryValues) {
    }

    abstract public void reloadPreference();
}
