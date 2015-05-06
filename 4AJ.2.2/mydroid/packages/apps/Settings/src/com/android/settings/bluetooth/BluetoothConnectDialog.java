/*
 * Copyright 2012 Texas Instruments Israel Ltd.
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

package com.android.settings.bluetooth;

import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothAtt;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Bundle;
import android.text.Editable;
import android.text.InputFilter;
import android.text.InputType;
import android.text.Spanned;
import android.text.InputFilter.LengthFilter;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.EditText;
import android.widget.TextView;

import com.android.internal.app.AlertActivity;
import com.android.internal.app.AlertController;
import com.android.settings.R;
import android.view.KeyEvent;

import java.util.List;

/**
 * BluetoothConnectDialog asks the user to decide if it wants to connect to
 * a remote bluetooth device, set it as preferred or unpair from it.
 * It is an activity that appears as a dialog.
 */
public final class BluetoothConnectDialog extends AlertActivity implements
                                            DialogInterface.OnClickListener {
    private static final String TAG = "BluetoothConnectDialog";

    private BluetoothDevice mDevice;
    private CachedBluetoothDevice mCachedBtDevice;
    private LocalBluetoothProfile mAttProfile;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        Intent intent = getIntent();
        if (!intent.getAction().equals(BluetoothAtt.ACTION_DEVICE_CONNECT_POPUP))
        {
            Log.e(TAG, "Error: this activity may be started only with intent " +
                  BluetoothAtt.ACTION_DEVICE_CONNECT_POPUP);
            finish();
            return;
        }

        LocalBluetoothManager manager = LocalBluetoothManager.getInstance(this);
        if (manager == null) {
            Log.e(TAG, "Error: BluetoothAdapter not supported by system");
            finish();
            return;
        }
        CachedBluetoothDeviceManager deviceManager = manager.getCachedDeviceManager();

        mDevice = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
        mCachedBtDevice = deviceManager.findDevice(mDevice);
        if (mCachedBtDevice == null) {
            Log.e(TAG, "Error: cannot find CachedBluetoothDevice for " +
                  mDevice.getAddress());
            finish();
            return;
        }

        List<LocalBluetoothProfile> profiles = mCachedBtDevice.getProfiles();
        for (LocalBluetoothProfile profile : profiles) {
            if (profile.toString().equals(AttProfile.NAME)) {
                mAttProfile = profile;
                break;
            }
        }

        if (mAttProfile == null) {
            Log.e(TAG, "Error: cannot find ATT profile in device " +
                  mDevice.getAddress());
            finish();
            return;
        }

        createConnectDialog(deviceManager);
    }

    private View createView(CachedBluetoothDeviceManager deviceManager) {
        View view = getLayoutInflater().inflate(R.layout.bluetooth_pin_confirm, null);
        String name = deviceManager.getName(mDevice);
        TextView messageView = (TextView) view.findViewById(R.id.message);

        messageView.setText("Connection request from " + name);
        return view;
    }

    private void createConnectDialog(CachedBluetoothDeviceManager deviceManager) {
        final AlertController.AlertParams p = mAlertParams;
        p.mIconId = android.R.drawable.ic_dialog_info;
        p.mTitle = "Connect to ATT client device";
        p.mView = createView(deviceManager);
        p.mPositiveButtonText = "Always connect";
        p.mPositiveButtonListener = this;
        p.mNeutralButtonText = "Connect once";
        p.mNeutralButtonListener = this;
        p.mNegativeButtonText = "Unpair";
        p.mNegativeButtonListener = this;
        setupAlert();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
    }

    private void onConnectAlways() {
        Log.d(TAG, "connect always " + mDevice.getAddress());
        mAttProfile.setPreferred(mDevice, true);
        mAttProfile.connect(mDevice);
        mCachedBtDevice.refresh();
    }

    private void onConnectOnce() {
        Log.d(TAG, "connect once " + mDevice.getAddress());
        mAttProfile.connect(mDevice);
        mCachedBtDevice.refresh();
    }

    private void onUnpair() {
        Log.d(TAG, "unpair " + mDevice.getAddress());
        mCachedBtDevice.unpair();
        mCachedBtDevice.refresh();
    }

    public void onClick(DialogInterface dialog, int which) {
        switch (which) {
            case BUTTON_POSITIVE:
                onConnectAlways();
                break;
            case BUTTON_NEGATIVE:
                onUnpair();
            case BUTTON_NEUTRAL:
            default:
                onConnectOnce();
                break;
        }
    }

}
