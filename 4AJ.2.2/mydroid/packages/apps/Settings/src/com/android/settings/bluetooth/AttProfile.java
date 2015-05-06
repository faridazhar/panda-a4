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

import android.bluetooth.BluetoothClass;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothAtt;
import android.bluetooth.BluetoothProfile;
import android.content.Context;
import android.content.BroadcastReceiver;
import android.content.Intent;
import android.content.IntentFilter;
import android.text.TextUtils;
import android.os.PowerManager;
import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;

import android.content.res.Resources;
import com.android.settings.R;

import java.util.List;
import android.util.Log;

/**
 * AttProfile handles Bluetooth ATT Server profile.
 */
final class AttProfile implements LocalBluetoothProfile {
    static final String TAG = "SettingsAttProfile";

    private BluetoothAtt mService = null;

    static final String NAME = "ATT";

    // Order of this profile in device profiles list
    private static final int ORDINAL = 5;

    private static final int CONNECT_POPUP_NOTIF_ID = 1;

    // These callbacks run on the main thread.
    private final class AttServiceListener
            implements BluetoothProfile.ServiceListener {

        public void onServiceConnected(int profile, BluetoothProfile proxy) {
            Log.d(TAG, "Att server connected");
            mService = (BluetoothAtt) proxy;
        }

        public void onServiceDisconnected(int profile) {
            mService = null;
        }
    }

    private BroadcastReceiver mPopupReceiver = new BroadcastReceiver() {

        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (action.equals(BluetoothAtt.ACTION_DEVICE_CONNECT_POPUP)) {
                // convert broadcast intent into activity intent (same action string)
                BluetoothDevice device =
                        intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
                Intent connectIntent = new Intent();
                connectIntent.setClass(context, BluetoothConnectDialog.class);
                connectIntent.putExtra(BluetoothDevice.EXTRA_DEVICE, device);
                connectIntent.setAction(BluetoothAtt.ACTION_DEVICE_CONNECT_POPUP);
                connectIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

                PowerManager powerManager =
                        (PowerManager)context.getSystemService(Context.POWER_SERVICE);
                String deviceAddress = device != null ? device.getAddress() : null;
                if (powerManager.isScreenOn() &&
                        LocalBluetoothPreferences.shouldShowDialogInForeground(context, deviceAddress)) {
                    // Since the screen is on and the BT-related activity is in the foreground,
                    // just open the dialog
                    context.startActivity(connectIntent);
                } else {
                    // Put up a notification that leads to the dialog
                    Resources res = context.getResources();
                    Notification.Builder builder = new Notification.Builder(context)
                            .setSmallIcon(android.R.drawable.stat_sys_data_bluetooth)
                            .setTicker("Bluetooth incoming connection");

                    PendingIntent pending = PendingIntent.getActivity(context, 0,
                            connectIntent, PendingIntent.FLAG_ONE_SHOT);

                    String name = intent.getStringExtra(BluetoothDevice.EXTRA_NAME);
                    if (TextUtils.isEmpty(name)) {
                        name = device != null ? device.getAliasName() :
                                context.getString(android.R.string.unknownName);
                    }

                    builder.setContentTitle("Incoming connection")
                            .setContentText("Touch to connect to " + name)
                            .setContentIntent(pending)
                            .setAutoCancel(true)
                            .setDefaults(Notification.DEFAULT_SOUND);

                    NotificationManager manager = (NotificationManager)
                            context.getSystemService(Context.NOTIFICATION_SERVICE);
                    manager.notify(CONNECT_POPUP_NOTIF_ID, builder.getNotification());
                }
            }
        }

    };

    AttProfile(Context context, LocalBluetoothAdapter adapter) {
        adapter.getProfileProxy(context, new AttServiceListener(),
                BluetoothProfile.ATT_SRV);

        context.registerReceiver(mPopupReceiver,
                         new IntentFilter(BluetoothAtt.ACTION_DEVICE_CONNECT_POPUP));
    }

    public boolean isConnectable() {
        return true;
    }

    public boolean isAutoConnectable() {
        return true;
    }

    public boolean connect(BluetoothDevice device) {
        if (mService == null)
            return false;

        return mService.connect(device);
    }

    /* only single-mode LE devices are supported for ATT server, so
     * no other service is exposed and it's ok to disconnect */
    public boolean disconnect(BluetoothDevice device) {
        if (mService == null)
            return false;

        return mService.disconnect(device);
    }

    public int getConnectionStatus(BluetoothDevice device) {
        if (mService == null)
            return BluetoothProfile.STATE_DISCONNECTED;

        return mService.getConnectionState(device);
    }

    public boolean isPreferred(BluetoothDevice device) {
        if (mService == null)
            return false;

        return mService.getPriority(device) > BluetoothProfile.PRIORITY_OFF;
    }

    public int getPreferred(BluetoothDevice device) {
        if (mService == null)
            return BluetoothProfile.PRIORITY_OFF;

        return mService.getPriority(device);
    }

    public void setPreferred(BluetoothDevice device, boolean preferred) {
        if (mService == null)
            return;

        if (preferred) {
            if (mService.getPriority(device) < BluetoothProfile.PRIORITY_ON) {
                mService.setPriority(device, BluetoothProfile.PRIORITY_ON);
            }
        } else {
            mService.setPriority(device, BluetoothProfile.PRIORITY_OFF);
        }
    }

    public boolean isProfileReady() {
        return mService != null;
    }

    public String toString() {
        return NAME;
    }

    public int getOrdinal() {
        return ORDINAL;
    }

    public int getNameResource(BluetoothDevice device) {
        return R.string.bluetooth_profile_att;
    }

    public int getSummaryResourceForDevice(BluetoothDevice device) {
        int state = BluetoothProfile.STATE_DISCONNECTED;
        if (mService != null)
            state = mService.getConnectionState(device);

        return Utils.getConnectionStateSummary(state);
    }

    public int getDrawableResource(BluetoothClass btClass) {
        return R.drawable.ic_sync_grey_holo;
    }
}
