/*
 * Copyright (C) 2011 The Android Open Source Project
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

package com.ti.bluetooth.dun;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothProfile;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.res.Resources.NotFoundException;
import android.os.IBinder;
import android.os.ServiceManager;
import android.provider.Settings;
import android.server.BluetoothService;
import android.util.Log;

import java.util.ArrayList;
import java.util.List;

import com.ti.bluetooth.dun.BluetoothDun;


/**
 * This handles the DUN profile. All calls into this are made
 * from Bluetooth Service.
 */
public class BluetoothDunProfileHandler {
    private static final String TAG = "BluetoothDunProfileHandler";
    private static final boolean DBG = true;

    private static final String BLUETOOTH_ADMIN_PERM = android.Manifest.permission.BLUETOOTH_ADMIN;
    static final String BLUETOOTH_PERM = android.Manifest.permission.BLUETOOTH;

    public static BluetoothDunProfileHandler sInstance;
    private boolean mDunEnabled;
    private Context mContext;
    private BluetoothService mBluetoothService;
    private BluetoothDevice mDunDevice;
    private int mState;

    public BluetoothDunProfileHandler(Context context, BluetoothService service) {
        mContext = context;
        mBluetoothService = service;
        mDunDevice = null;
        mDunEnabled = false;
        mState = BluetoothDun.STATE_DISCONNECTED;
    }

    public static BluetoothDunProfileHandler getInstance(Context context,
            BluetoothService service) {
        if (sInstance == null) sInstance = new BluetoothDunProfileHandler(context, service);
        return sInstance;
    }

    public boolean isDunEnabled() {
        debugLog("isDunEnabled(): " + mDunEnabled);
        return mDunEnabled;
    }

    private BroadcastReceiver mDunReceiver = null;

    public boolean enableBluetoothDun(boolean value) {
        debugLog("enableBluetoothDun(): " + value);
        if (!value && mDunDevice != null) {
            debugLog("enableBluetoothDun(): disconnecting DUN device");
            disconnectDunDevice();
        }

        if (mBluetoothService.getBluetoothState() != BluetoothAdapter.STATE_ON && value) {
            debugLog("enableBluetoothDun(): creating Receiver, waiting for Bluetooth On");
            IntentFilter filter = new IntentFilter();
            filter.addAction(BluetoothAdapter.ACTION_STATE_CHANGED);
            mDunReceiver = new BroadcastReceiver() {
                @Override
                public void onReceive(Context context, Intent intent) {
                    if (intent.getIntExtra(BluetoothAdapter.EXTRA_STATE, BluetoothAdapter.STATE_OFF)
                            == BluetoothAdapter.STATE_ON) {
                        debugLog("enableBluetoothDun(): onReceive(): Bluetooth is On");
                        mContext.unregisterReceiver(mDunReceiver);
                        if (Settings.Secure.getInt(mContext.getContentResolver(),
                                Settings.Secure.ENABLE_BLUETOOTH_DUN_SETTING, 0) != 0) {
                            if (!mBluetoothService.enableBluetoothDunNative(true)) {
                                errorLog("enableBluetoothDunNative(true) failed");
                            } else {
                                mDunEnabled = true;
                            }
                       }
                    }
                }
            };
            mContext.registerReceiver(mDunReceiver, filter);
            return false;
        } else {
            mDunEnabled = value;
        }

        if (!mBluetoothService.enableBluetoothDunNative(value)) {
            errorLog("enableBluetoothDunNative() failed");
            return false;
        } else {
            return true;
        }
    }

    public int getDunDeviceConnectionState(BluetoothDevice device) {
        debugLog("getDunDeviceConnectionState(): device " + device + ", mDunDevice " +
                mDunDevice + " state " + mState);
        if (device != null) {
            if (device.equals(mDunDevice)) {
                return mState;
            }
        }
        return BluetoothDun.STATE_DISCONNECTED;
    }

    public boolean setDunDevicePriority(BluetoothDevice device, int priority) {
        debugLog("setDunDevicePriority(): device " + device + ", priority " + priority);
        if (!BluetoothAdapter.checkBluetoothAddress(device.getAddress())) {
            return false;
        }
        return Settings.Secure.putInt(mContext.getContentResolver(),
                Settings.Secure.getBluetoothDunDevicePriorityKey(device.getAddress()),
                priority);
    }

    public int getDunDevicePriority(BluetoothDevice device) {
        debugLog("getDunDevicePriority(): device " + device);
        return Settings.Secure.getInt(mContext.getContentResolver(),
                Settings.Secure.getBluetoothDunDevicePriorityKey(device.getAddress()),
                BluetoothProfile.PRIORITY_UNDEFINED);
    }

    public void setInitialDunDevicePriority(BluetoothDevice device, int state) {
        debugLog("setInitialDunDevicePriority(): device " + device + ", state " + state);
        switch (state) {
            case BluetoothDevice.BOND_BONDED:
                if (getDunDevicePriority(device) == BluetoothProfile.PRIORITY_UNDEFINED) {
                    setDunDevicePriority(device, BluetoothProfile.PRIORITY_ON);
                }
                break;
            case BluetoothDevice.BOND_NONE:
                setDunDevicePriority(device, BluetoothProfile.PRIORITY_UNDEFINED);
                break;
        }
    }

    public List<BluetoothDevice> getConnectedDunDevices() {
        List<BluetoothDevice> devices = new ArrayList<BluetoothDevice>();

        if (getDunDeviceConnectionState(mDunDevice) == BluetoothDun.STATE_CONNECTED) {
            devices.add(mDunDevice);
        }
        return devices;
    }

    public List<BluetoothDevice> getDunDevicesMatchingConnectionStates(int[] states) {
        List<BluetoothDevice> devices = new ArrayList<BluetoothDevice>();

        int dunDeviceState = getDunDeviceConnectionState(mDunDevice);
        for (int state : states) {
            if (state == dunDeviceState) {
                devices.add(mDunDevice);
                break;
            }
        }
        return devices;
    }

    public boolean disconnectDunDevice() {
        debugLog("disconnect DUN");

        if (mDunDevice == null) {
            errorLog("DUN device does not exist");
            return false;
        }

        if (mState != BluetoothDun.STATE_CONNECTED) {
            debugLog("already disconnected from DUN");
            return false;
        }

        handleDunDeviceStateChange(mDunDevice, BluetoothDun.STATE_DISCONNECTING);
        if (!mBluetoothService.disconnectDunDeviceNative()) {
            // Restore prev state, this shouldn't happen
            handleDunDeviceStateChange(mDunDevice, BluetoothDun.STATE_CONNECTED);
            errorLog("disconnectDunDeviceNative() failed");
            return false;
        }
        return true;
    }

    public void handleDunDeviceStateChange(BluetoothDevice device, int state) {
        int prevState;
        mContext.enforceCallingOrSelfPermission(BLUETOOTH_ADMIN_PERM, "Need BLUETOOTH_ADMIN permission");

        debugLog("handleDunDeviceStateChange(): device " + device + ", state " + state);

        if (device == null) {
            errorLog("Null DUN device");
            return;
        } else if (mDunDevice == null) {
            mDunDevice = device;
            prevState = BluetoothDun.STATE_DISCONNECTED;
        } else if (!mDunDevice.equals(device)) {
            errorLog("No record for this DUN device: " + device + " mDunDevice: " + mDunDevice);
            return;
        } else {
            prevState = mState;
        }
        mState = state;

        if (prevState == state) return;

        if (state == BluetoothDun.STATE_DISCONNECTED) {
            mDunDevice = null;
        }

        Intent intent = new Intent(BluetoothDun.ACTION_CONNECTION_STATE_CHANGED);
        intent.putExtra(BluetoothDevice.EXTRA_DEVICE, device);
        intent.putExtra(BluetoothDun.EXTRA_PREVIOUS_STATE, prevState);
        intent.putExtra(BluetoothDun.EXTRA_STATE, state);
        mContext.sendBroadcast(intent, BLUETOOTH_PERM);

        debugLog("DUN Device state : device: " + device + " state: " + prevState + " -> " + state);
        mBluetoothService.sendConnectionStateChange(device, BluetoothProfile.DUN, state, prevState);
    }

    private static void debugLog(String msg) {
        if (DBG) Log.d(TAG, msg);
    }

    private static void errorLog(String msg) {
        Log.e(TAG, msg);
    }
}
