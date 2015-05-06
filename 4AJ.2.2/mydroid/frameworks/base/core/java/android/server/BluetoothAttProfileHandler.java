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

package android.server;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothDeviceProfileState;
import android.bluetooth.BluetoothAtt;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.BluetoothProfileState;
import android.content.Context;
import android.content.Intent;
import android.os.Handler;
import android.os.Message;
import android.provider.Settings;
import android.util.Log;
import android.content.BroadcastReceiver;
import android.content.IntentFilter;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Set;
import java.util.HashSet;

/**
 * This handles all the operations on the ATT profile.
 * All functions are called by BluetoothService, as Bluetooth Service
 * is the Service handler for the ATT profile.
 */
final class BluetoothAttProfileHandler {
    private static final String TAG = "BluetoothAttProfileHandler";
    private static final boolean DBG = true;

    // allow for 5 minutes between popups to connect to paired but non-
    // preferred devices
    private static final int FOUND_DEVICE_POPUP_TIMEOUT_MSEC = 5 * 60 * 1000;

    // give a grace-period for the reconnection of a non-preferred device
    // (in case the connection was lost)
    private static final int NON_PREFERRED_DEV_DISCONNECT_TIMEOUT_MSEC = 60 * 1000;

    public static BluetoothAttProfileHandler sInstance;
    private Context mContext;
    private BluetoothService mBluetoothService;
    private final HashMap<String, Integer> mAttDevices;
    private HashMap<String, Long> mRecentPopups;
    private HashMap<String, Long> mRecentDisconnects;

    // only use background scan when there are bonded and disconnected
    // LE devices
    private Set<String> mBondedDisconnctedDevs;

    private boolean mWhiteListScanning = false;

    private BluetoothAttProfileHandler(Context context, BluetoothService service) {
        mContext = context;
        mBluetoothService = service;
        mAttDevices = new HashMap<String, Integer>();
        mRecentPopups = new HashMap<String, Long>();
        mRecentDisconnects = new HashMap<String, Long>();
        mBondedDisconnctedDevs = new HashSet<String>();

        IntentFilter devFilter = new IntentFilter();
        devFilter.addAction(BluetoothDevice.ACTION_ACL_CONNECTED);
        devFilter.addAction(BluetoothDevice.ACTION_ACL_DISCONNECTED);
        context.registerReceiver(mAclDevStateReceiver, devFilter);

        IntentFilter stateFilter = new IntentFilter();
        stateFilter.addAction(BluetoothAdapter.ACTION_STATE_CHANGED);
        stateFilter.addAction(BluetoothDevice.ACTION_BOND_STATE_CHANGED);
        stateFilter.addAction(BluetoothDevice.ACTION_FOUND);
        context.registerReceiver(mBtStateReceiver, stateFilter);
    }

    static synchronized BluetoothAttProfileHandler getInstance(Context context,
            BluetoothService service) {
        if (sInstance == null) sInstance = new BluetoothAttProfileHandler(context, service);
        return sInstance;
    }

    private void broadcastConnectionStateChange(Context context, BluetoothDevice device,
                                                    int prevState, int state) {
        Log.d(TAG, "LE device " + device.getAddress() + " state: "
                                                + prevState + " -> " + state);
        Intent i = new Intent(BluetoothAtt.ACTION_CONNECTION_STATE_CHANGED);
        i.putExtra(BluetoothDevice.EXTRA_DEVICE, device);
        i.putExtra(BluetoothProfile.EXTRA_PREVIOUS_STATE, prevState);
        i.putExtra(BluetoothProfile.EXTRA_STATE, state);
        i.addFlags(Intent.FLAG_RECEIVER_REGISTERED_ONLY_BEFORE_BOOT);
        context.sendBroadcast(i, BluetoothService.BLUETOOTH_PERM);
    }

    private void broadcastPopup(Context context, BluetoothDevice device) {
        Intent i = new Intent(BluetoothAtt.ACTION_DEVICE_CONNECT_POPUP);
        i.putExtra(BluetoothDevice.EXTRA_DEVICE, device);
        i.addFlags(Intent.FLAG_RECEIVER_REGISTERED_ONLY_BEFORE_BOOT);
        context.sendBroadcast(i, BluetoothService.BLUETOOTH_PERM);
    }

    private boolean isRecentPopup(String addr) {
        if (mRecentPopups.get(addr) == null)
            return false;

        long lastPopup = mRecentPopups.get(addr);
        long now = System.currentTimeMillis();
        return lastPopup > (now - FOUND_DEVICE_POPUP_TIMEOUT_MSEC);
    }

    private void setRecentPopup(String addr) {
        mRecentPopups.put(addr, System.currentTimeMillis());
    }

    private boolean isRecentDisconnect(String addr) {
        if (mRecentDisconnects.get(addr) == null)
            return false;

        long lastDisconnect = mRecentDisconnects.get(addr);
        long now = System.currentTimeMillis();
        return lastDisconnect > (now - NON_PREFERRED_DEV_DISCONNECT_TIMEOUT_MSEC);
    }

    private void setRecentDisconnect(String addr) {
        mRecentDisconnects.put(addr, System.currentTimeMillis());
    }

    // Receiver for remote BT device connection state
    private BroadcastReceiver mAclDevStateReceiver = new BroadcastReceiver() {

        @Override
        public void onReceive(Context context, Intent intent) {
            BluetoothDevice device =
                        intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
            if (device == null || device.getAddress() == null)
                return;

            String addr = device.getAddress();
            String type = mBluetoothService.getDeviceProperty(addr, "Type");
            // we don't care about BR/EDR devices
            if (type == null || Integer.parseInt(type) == 0)
                return;

            int prevState = getAttDeviceConnectionState(device);
            int state = BluetoothProfile.STATE_DISCONNECTED;

            if (intent.getAction().equals(BluetoothDevice.ACTION_ACL_CONNECTED)) {
                state = BluetoothProfile.STATE_CONNECTED;
            } else if (intent.getAction().equals(BluetoothDevice.ACTION_ACL_DISCONNECTED)) {
                state = BluetoothProfile.STATE_DISCONNECTED;
            }

            if (prevState == state)
                return;

            setAttDeviceConnectionState(context, device, state);

            boolean whitelistScanChange = false;
            if (device.getBondState() == BluetoothDevice.BOND_BONDED) {
                boolean prevNoBondedDisconnected = mBondedDisconnctedDevs.isEmpty();

                if (state == BluetoothProfile.STATE_CONNECTED)
                    mBondedDisconnctedDevs.remove(device.getAddress());
                else
                    mBondedDisconnctedDevs.add(device.getAddress());

                boolean noBondedDisconncted = mBondedDisconnctedDevs.isEmpty();

                // only change the scanning state if the state of the list changed
                whitelistScanChange = (prevNoBondedDisconnected != noBondedDisconncted);
            }

            boolean remoteDisconnect = false;
            if (state == BluetoothProfile.STATE_DISCONNECTED &&
                        prevState != BluetoothProfile.STATE_DISCONNECTING) {
                /* give non-preferred devices a grace period for auto-reconnections */
                setRecentDisconnect(addr);
                remoteDisconnect = true;
                whitelistScanChange = true;
            }

            /* if the remote side disconnected, start a reconnection scan */
            if (whitelistScanChange)
                restartWhitelistScan(remoteDisconnect);
        }
    };

    private BroadcastReceiver mBtStateReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (action.equals(BluetoothAdapter.ACTION_STATE_CHANGED)) {
                int state = intent.getIntExtra(BluetoothAdapter.EXTRA_STATE,
                                                BluetoothAdapter.STATE_OFF);
                if (state == BluetoothAdapter.STATE_ON) {
                    refreshWhitelist();
                } else if (state == BluetoothAdapter.STATE_OFF) {
                    // disconnect all devices on BT-off
                    markAllDevicesDisconnected();
                }
            } else if (action.equals(BluetoothDevice.ACTION_BOND_STATE_CHANGED)) {
                BluetoothDevice device =
                        intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
                if (device == null)
                    return;

                /* update devices in white-list according to bond state */
                int state = intent.getIntExtra(BluetoothDevice.EXTRA_BOND_STATE,
                                                    BluetoothDevice.BOND_NONE);
                if (state == BluetoothDevice.BOND_BONDED ||
                            state == BluetoothDevice.BOND_NONE) {
                    Log.d(TAG, "changing whitelist status for device " +
                               device.getAddress());
                    refreshWhitelist();
                }
            } else if (action.equals(BluetoothDevice.ACTION_FOUND)) {
                BluetoothDevice device =
                        intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
                if (device == null)
                    return;

                // non-bonded devices are not worth our attention
                if (device.getBondState() != BluetoothDevice.BOND_BONDED)
                    return;

                // don't do anything if the device is not disconnected
                if (getAttDeviceConnectionState(device) !=
                                            BluetoothProfile.STATE_DISCONNECTED)
                    return;

                // connect automatically to a device with PRIORITY_ON, or non-
                // preferred devices that have recently disconnected
                boolean recentDisconnect = isRecentDisconnect(device.getAddress());
                int prio = getAttPriority(device);
                if (prio >= BluetoothProfile.PRIORITY_ON || recentDisconnect) {
                    Log.d(TAG, "auto-reconnect to ATT device " + device.getAddress() +
                               "prio: " + prio + " recentDisconnect: " + recentDisconnect);
                    connectAttDevice(device);
                } else { // else popup a dialog to the user asking for connection
                    String addr = device.getAddress();
                    Log.d(TAG, "non-preferred device seen: " + addr);
                    // allow the user to cool down between popups
                    if (isRecentPopup(addr)) {
                        Log.d(TAG, "not popping up - recent popup detected");
                        return;
                    }

                    setRecentPopup(addr);
                    Log.d(TAG, "popping up!");
                    broadcastPopup(context, device);
                }
            }
        }
    };

    private void markAllDevicesDisconnected() {
        synchronized (mAttDevices) {
            for (String address: mAttDevices.keySet()) {
                BluetoothDevice device =
                    BluetoothAdapter.getDefaultAdapter().getRemoteDevice(address);
                setAttDeviceConnectionState(mContext, device,
                                BluetoothProfile.STATE_DISCONNECTED);
            }
        }
    }

    synchronized private boolean addWhitelist(BluetoothDevice device) {
        String objpath = mBluetoothService.getObjectPathFromAddress(device.getAddress());
        if (objpath == null)
            return false;

        String type = mBluetoothService.getDeviceProperty(device.getAddress(), "Type");
        // we don't care about BR/EDR devices
        if (type == null || Integer.parseInt(type) == 0)
            return false;

        Log.d(TAG, "adding device " + objpath + " to whitelist");

        return mBluetoothService.setDevicePropertyBooleanSyncNative(objpath,
                                                             "WhiteList", 1);
    }

    synchronized private void refreshWhitelist() {
        stopWhitelistScan();

        mBluetoothService.clearLeWhiteListNative();
        addBondedDevicesToWhiteList();

        startWhitelistScan(false);
    }


    synchronized private void stopWhitelistScan() {
        if (!mWhiteListScanning) {
            Log.d(TAG, "no need to stop whitelist scan - already stopped");
            return;
        }

        boolean success = mBluetoothService.stopWhiteListScanNative();
        Log.d(TAG, "stop whitelist scan success: " + success);

        mWhiteListScanning = false;
    }

    synchronized private void startWhitelistScan(boolean reconnectScan) {
        // only use whitelist when we have bonded disconncted devices
        if (mBondedDisconnctedDevs.isEmpty()) {
            Log.d(TAG, "no bonded disconnected devs - no need to scan");
            return;
        }

        if (mWhiteListScanning) {
            Log.d(TAG, "no need to start whitelist scan - already started");
            return;
        }

        boolean success =
            mBluetoothService.startWhiteListScanNative(reconnectScan);
        Log.d(TAG, "start whitelist scan. reconnectScan: " + reconnectScan +
                   " success: " + success);

        mWhiteListScanning = true;
    }

    private void restartWhitelistScan(boolean reconnectScan) {
        stopWhitelistScan();
        startWhitelistScan(reconnectScan);
    }

    private void addBondedDevicesToWhiteList() {
        Set<BluetoothDevice> bonded =
                    BluetoothAdapter.getDefaultAdapter().getBondedDevices();

        mBondedDisconnctedDevs.clear();

        if (bonded.size() == 0)
            return;

        for (BluetoothDevice device : bonded) {
            if (!addWhitelist(device))
                continue;

            if (getAttDeviceConnectionState(device) != BluetoothProfile.STATE_CONNECTED)
                mBondedDisconnctedDevs.add(device.getAddress());
        }
    }

    boolean connectAttDevice(BluetoothDevice device) {
        if (device == null)
                return false;

        boolean connectResult;

        // If the user wants to connect to the device, allow further popups
        // and reconnects
        mRecentPopups.remove(device.getAddress());
        mRecentDisconnects.remove(device.getAddress());

        synchronized (mAttDevices) {
        int connState = getAttDeviceConnectionState(device);
            if (connState != BluetoothProfile.STATE_DISCONNECTED) {
                Log.d(TAG, "Trying to connect to non-disconnected device " +
                                                        device.getAddress());
                /* it's ok to connect to an already connected device */
                return connState == BluetoothProfile.STATE_CONNECTED;
            }

            connectResult =
                mBluetoothService.connectAttDeviceNative(device.getAddress());
            if (connectResult) {
                setAttDeviceConnectionState(mContext, device,
                                BluetoothProfile.STATE_CONNECTING);
            }
        }

        return connectResult;
    }

    boolean disconnectAttDevice(BluetoothDevice device) {
        if (device == null)
                return false;

        String devpath =
            mBluetoothService.getObjectPathFromAddress(device.getAddress());
        if (devpath == null)
            return false;

        boolean discResult;

        /* mark the device as disconnecting so we know not to reconnect */
        synchronized (mAttDevices) {
        int connState = getAttDeviceConnectionState(device);
            if (connState != BluetoothProfile.STATE_CONNECTED) {
                    Log.d(TAG, "Trying to disconnect non-connected device " +
                            device.getAddress());
                /* it's ok to disconnect from an already connected device */
                return connState == BluetoothProfile.STATE_DISCONNECTED;
            }

            discResult = mBluetoothService.disconnectDeviceNative(devpath);
            if (discResult) {
                setAttDeviceConnectionState(mContext, device,
                                BluetoothProfile.STATE_DISCONNECTING);
            }
        }

        return discResult;
    }

    private void setAttDeviceConnectionState(Context context,
                        BluetoothDevice device, int state) {
            synchronized (mAttDevices) {
                int prevState = getAttDeviceConnectionState(device);
                mAttDevices.put(device.getAddress(), state);
                broadcastConnectionStateChange(context, device, prevState, state);
                mBluetoothService.sendConnectionStateChange(device,
                                    BluetoothProfile.ATT_SRV, state, prevState);
            }
    }

    int getAttDeviceConnectionState(BluetoothDevice device) {
        if (mAttDevices.get(device.getAddress()) == null) {
            return BluetoothProfile.STATE_DISCONNECTED;
        }
        return mAttDevices.get(device.getAddress());
    }

    int getAttPriority(BluetoothDevice device) {
        return Settings.Secure.getInt(mContext.getContentResolver(),
                Settings.Secure.getBluetoothAttPriorityKey(device.getAddress()),
                BluetoothProfile.PRIORITY_UNDEFINED);
    }

    boolean setAttPriority(BluetoothDevice device, int priority) {
        if (!BluetoothAdapter.checkBluetoothAddress(device.getAddress())) {
            return false;
        }
        return Settings.Secure.putInt(mContext.getContentResolver(),
                Settings.Secure.getBluetoothAttPriorityKey(device.getAddress()),
                priority);
    }

    void setInitialAttPriority(BluetoothDevice device, int state) {
        switch (state) {
            case BluetoothDevice.BOND_BONDED:
                if (getAttPriority(device) == BluetoothProfile.PRIORITY_UNDEFINED) {
                    setAttPriority(device, BluetoothProfile.PRIORITY_ON);
                }
                break;
            case BluetoothDevice.BOND_NONE:
                setAttPriority(device, BluetoothProfile.PRIORITY_UNDEFINED);
                break;
        }
    }

    private static void debugLog(String msg) {
        if (DBG) Log.d(TAG, msg);
    }

    private static void errorLog(String msg) {
        Log.e(TAG, msg);
    }
}
