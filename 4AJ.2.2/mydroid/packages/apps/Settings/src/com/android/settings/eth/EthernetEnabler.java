/*
 * Copyright (C) Texas Instruments - http://www.ti.com/
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

package com.android.settings.eth;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.eth.TIEthernetManager;
import android.provider.Settings;
import android.util.Log;
import android.widget.CompoundButton;
import android.widget.Switch;

import com.android.settings.R;

/**
 * EthernetEnabler is a helper to manage the Ethernet on/off checkbox
 * preference. It turns on/off Ethernet and ensures the summary of the
 * preference reflects the current state.
 */
public final class EthernetEnabler implements CompoundButton.OnCheckedChangeListener {

    public static final String TAG = "EthernetEnabler";

    private final Context mContext;
    private Switch mSwitch;

    private final IntentFilter mIntentFilter;
    private final TIEthernetManager manager;

    private final BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            mSwitch.setChecked(null != manager ? manager.getEthernetState() : false);
        }
    };

    public EthernetEnabler(Context context, Switch switch_) {
        mContext = context;
        mSwitch = switch_;

        manager = (TIEthernetManager) mContext.getSystemService(mContext.ETHERNET_SERVICE);
        if (manager == null) {
            // Ethernet is not supported ??
        }

        mIntentFilter = new IntentFilter(TIEthernetManager.ETH_GLOBAL_STATE_CHANGED_ACTION);
    }

    public void resume() {
        mContext.registerReceiver(mReceiver, mIntentFilter);
        mSwitch.setOnCheckedChangeListener(this);
    }

    public void pause() {
        mContext.unregisterReceiver(mReceiver);
        mSwitch.setOnCheckedChangeListener(null);
    }

    public void setSwitch(Switch switch_) {
        if (mSwitch == switch_) return;
        mSwitch.setOnCheckedChangeListener(null);
        mSwitch = switch_;
        mSwitch.setChecked(null != manager ? manager.getEthernetState() : false);
        mSwitch.setEnabled(null != manager ? true : false);
        mSwitch.setOnCheckedChangeListener(this);
    }

    public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
        if (null != manager) {
            if (manager.getEthernetState() != mSwitch.isChecked()) {
                manager.setEthernetState(isChecked);
            }
        }
    }
}
