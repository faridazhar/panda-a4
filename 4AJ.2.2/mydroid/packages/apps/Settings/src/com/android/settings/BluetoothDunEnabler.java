/*
 * Copyright (C) 2010 The Android Open Source Project
 * Copyright (C) 2012 Texas Instruments, Inc.
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

package com.android.settings;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothProfile;
import android.content.BroadcastReceiver;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.SystemClock;
import android.os.SystemProperties;
import android.preference.CheckBoxPreference;
import android.preference.Preference;
import android.provider.Settings;
import android.util.Log;
import com.android.settings.R;
import com.android.settings.bluetooth.LocalBluetoothAdapter;
import com.android.settings.bluetooth.LocalBluetoothManager;

import com.ti.bluetooth.dun.BluetoothDun;

/**
 * BluetoothDunEnabler is a helper to enable Bluetooth dial-up networking.
 */
public class BluetoothDunEnabler extends SettingsPreferenceFragment implements
        Preference.OnPreferenceChangeListener {

    // debug data
    private static final String LOG_TAG = "BluetoothDunEnabler";
    private static final boolean DBG = true;

    private BluetoothDun mBluetoothDun;
    private final ContentResolver mContentResolver;
    private final CheckBoxPreference mCheckBoxPref;

    public BluetoothDunEnabler(Context context, CheckBoxPreference checkBoxPreference) {
        mContentResolver = context.getContentResolver();
        mCheckBoxPref = checkBoxPreference;

        BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
        if (adapter != null) {
            adapter.getProfileProxy(context, mProfileServiceListener, BluetoothProfile.DUN);
        }
    }

    private BluetoothProfile.ServiceListener mProfileServiceListener =
        new BluetoothProfile.ServiceListener() {
        public void onServiceConnected(int profile, BluetoothProfile proxy) {
        if (DBG) Log.d(LOG_TAG, "onServiceConnected(): Bluetooth DUN proxy " + proxy);
            mBluetoothDun = (BluetoothDun) proxy;
        }
        public void onServiceDisconnected(int profile) {
            mBluetoothDun = null;
        }
    };

    public void resume() {
        boolean checked = Settings.Secure.getInt(mContentResolver,
                    Settings.Secure.ENABLE_BLUETOOTH_DUN_SETTING, 0) == 0 ? false : true;
        mCheckBoxPref.setChecked(checked);
        mCheckBoxPref.setOnPreferenceChangeListener(this);
    }

    public void pause() {
        mCheckBoxPref.setOnPreferenceChangeListener(null);
    }

    public boolean onPreferenceChange(Preference preference, Object objValue) {
        final boolean enable = (Boolean)objValue;
        if (DBG) Log.d(LOG_TAG, "onPreferenceChange(): Bluetooth DUN enable = " + enable);
        mCheckBoxPref.setChecked(enable);
        try {
            Settings.Secure.putInt(mContentResolver,
                    Settings.Secure.ENABLE_BLUETOOTH_DUN_SETTING, enable ? 1 : 0);
        } catch (NumberFormatException e) {
            Log.e(LOG_TAG, "Could not persist Bluetooth DUN setting", e);
        }
        if (mBluetoothDun != null) {
            mBluetoothDun.enableBluetoothDun(enable);
        }
        return true;
    }

}
