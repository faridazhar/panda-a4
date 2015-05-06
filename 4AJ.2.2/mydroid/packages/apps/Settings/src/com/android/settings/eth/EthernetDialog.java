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

package com.android.settings.eth;

import com.android.settings.R;

import android.app.AlertDialog;
import android.content.Context;
import android.net.eth.EthernetConfiguration;
import android.net.eth.TIEthernetManager;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.ListView;
import android.widget.Button;
import android.widget.TextView;
import android.widget.ImageView;

import com.android.settings.wifi.WifiConfigController;

public class EthernetDialog extends AlertDialog {

    public static final String TAG = "EthernetDialog";

    private EthernetConfiguration ec;
    private View mView;
    private String ifaceName;
    private TIEthernetManager mEthManager;
    private WifiConfigController mController;

    public EthernetDialog(Context context, String ifaceName) {
        super(context);

        this.ifaceName = ifaceName;
        this.mEthManager = (TIEthernetManager) context.getSystemService(Context.ETHERNET_SERVICE);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        mView = getLayoutInflater().inflate(R.layout.wifi_dialog, null);
        setView(mView);
        setInverseBackgroundForced(true);

        super.onCreate(savedInstanceState);

        ec = mEthManager.getConfiguredIface(ifaceName);
        Log.e(TAG, "EthernetDialog.onCreate " + (ec == null ? "null" : ec.getIfaceName()));
    }
}
