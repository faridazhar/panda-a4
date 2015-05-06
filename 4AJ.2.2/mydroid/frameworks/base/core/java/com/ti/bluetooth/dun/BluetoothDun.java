/*
 * Copyright (C) 2008 The Android Open Source Project
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

import android.annotation.SdkConstant;
import android.annotation.SdkConstant.SdkConstantType;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.IBluetooth;
import android.content.Context;
import android.os.IBinder;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.server.BluetoothService;
import android.util.Log;

import java.util.ArrayList;
import java.util.List;


/**
 * This class provides the APIs to control the Bluetooth Pan
 * Profile.
 *
 *<p>BluetoothDun is a proxy object for controlling the Bluetooth
 * Service via IPC. Use {@link BluetoothAdapter#getProfileProxy} to get
 * the BluetoothDun proxy object.
 *
 *<p>Each method is protected with its appropriate permission.
 *@hide
 */
public final class BluetoothDun implements BluetoothProfile {
    private static final String TAG = "BluetoothDun";
    private static final boolean DBG = true;

    /**
     * Intent used to broadcast the change in connection state of the Pan
     * profile.
     *
     * <p>This intent will have 4 extras:
     * <ul>
     *   <li> {@link #EXTRA_STATE} - The current state of the profile. </li>
     *   <li> {@link #EXTRA_PREVIOUS_STATE}- The previous state of the profile.</li>
     *   <li> {@link BluetoothDevice#EXTRA_DEVICE} - The remote device. </li>
     * </ul>
     *
     * <p>{@link #EXTRA_STATE} or {@link #EXTRA_PREVIOUS_STATE} can be any of
     * {@link #STATE_DISCONNECTED}, {@link #STATE_CONNECTING},
     * {@link #STATE_CONNECTED}, {@link #STATE_DISCONNECTING}.
     *
     * <p>Requires {@link android.Manifest.permission#BLUETOOTH} permission to
     * receive.
     */
    @SdkConstant(SdkConstantType.BROADCAST_INTENT_ACTION)
    public static final String ACTION_CONNECTION_STATE_CHANGED =
        "android.com.ti.bluetooth.dun.profile.action.CONNECTION_STATE_CHANGED";

    /**
     * Return codes for the connect and disconnect Bluez / Dbus calls.
     * @hide
     */
    public static final int DUN_DISCONNECT_FAILED_NOT_CONNECTED = 1000;

    /**
     * @hide
     */
    public static final int DUN_OPERATION_GENERIC_FAILURE = 1001;

    /**
     * @hide
     */
    public static final int DUN_OPERATION_SUCCESS = 1002;

    private ServiceListener mServiceListener;
    private BluetoothAdapter mAdapter;
    private IBluetooth mService;

    /**
     * Create a BluetoothDun proxy object for interacting with the local
     * Bluetooth Service which handles the DUN profile
     *
     */
    public BluetoothDun(Context context, ServiceListener l) {
        IBinder b = ServiceManager.getService(BluetoothAdapter.BLUETOOTH_SERVICE);
        mServiceListener = l;
        mAdapter = BluetoothAdapter.getDefaultAdapter();
        if (b != null) {
            mService = IBluetooth.Stub.asInterface(b);
            if (mServiceListener != null) {
                mServiceListener.onServiceConnected(BluetoothProfile.DUN, this);
            }
        } else {
            Log.w(TAG, "Bluetooth Service not available!");
            mService = null;
        }
    }

    public void close() {
        mServiceListener = null;
    }

    /**
     * Check whether Bluetooth DUN is enabled by user
     *
     * <p> This API will return persistently strored 'enabled' status of
     * Bluetooth DUN with no regard to Bluetooth enabled status.
     *
     * <p>Requires {@link android.Manifest.permission#BLUETOOTH_ADMIN}
     * permission.
     *
     * @return true is enabled,
     *             false otherwise
     * @hide
     */
    public boolean isDunEnabled() {
        if (DBG) log("isDunEnabled");
        if (mService == null) {
            Log.w(TAG, "Proxy not attached to service");
        } else {
            try {
                return mService.isDunEnabled();
            } catch (RemoteException e) {
                Log.e(TAG, "Stack:" + Log.getStackTraceString(new Throwable()));
            }
        }
        return false;
    }

    /**
     * Enable or disable Bluetooth DUN
     *
     * <p> This API will enable or disable Bluetooth DUN and store the status
     * persistently with no regard to Bluetooth enabled status.
     *
     * <p>Requires {@link android.Manifest.permission#BLUETOOTH_ADMIN}
     * permission.
     *
     * @return none
     * @hide
     */
    public void enableBluetoothDun(boolean value) {
        if (DBG) log("enableBluetoothDun(" + value + ")");
        if (mService == null) {
            Log.w(TAG, "Proxy not attached to service");
        } else {
            try {
                mService.enableBluetoothDun(value);
            } catch (RemoteException e) {
                Log.e(TAG, "Stack:" + Log.getStackTraceString(new Throwable()));
            }
        }
    }

    /**
     * Initiate disconnection from a profile
     *
     * <p> This API will return false in scenarios like the profile on the
     * Bluetooth device is not in connected state etc. When this API returns,
     * true, it is guaranteed that the connection state change
     * intent will be broadcasted with the state. Users can get the
     * disconnection state of the profile from this intent.
     *
     * <p> If the disconnection is initiated by a remote device, the state
     * will transition from {@link #STATE_CONNECTED} to
     * {@link #STATE_DISCONNECTED}. If the disconnect is initiated by the
     * host (local) device the state will transition from
     * {@link #STATE_CONNECTED} to state {@link #STATE_DISCONNECTING} to
     * state {@link #STATE_DISCONNECTED}. The transition to
     * {@link #STATE_DISCONNECTING} can be used to distinguish between the
     * two scenarios.
     *
     * <p>Requires {@link android.Manifest.permission#BLUETOOTH_ADMIN}
     * permission.
     *
     * @return false on immediate error,
     *               true otherwise
     * @hide
     */
    public boolean disconnect() {
        if (DBG) log("disconnect");
        if (mService == null) {
            Log.w(TAG, "Proxy not attached to service");
        } else if (isBtEnabled()) {
            try {
                return mService.disconnectDunDevice();
            } catch (RemoteException e) {
                Log.e(TAG, "Stack:" + Log.getStackTraceString(new Throwable()));
            }
        }
        return false;
    }

    /**
     * {@inheritDoc}
     */
    public List<BluetoothDevice> getConnectedDevices() {
        if (DBG) log("getConnectedDevices()");
        if (mService == null) {
            Log.w(TAG, "Proxy not attached to service");
        } else if (isBtEnabled()) {
            try {
                return mService.getConnectedDunDevices();
            } catch (RemoteException e) {
                Log.e(TAG, "Stack:" + Log.getStackTraceString(new Throwable()));
            }
        }
        return new ArrayList<BluetoothDevice>();
    }

    /**
     * {@inheritDoc}
     */
    public List<BluetoothDevice> getDevicesMatchingConnectionStates(int[] states) {
        if (DBG) log("getDevicesMatchingStates()");
        if (mService == null) {
            Log.w(TAG, "Proxy not attached to service");
        } else if (isBtEnabled()) {
            try {
                return mService.getDunDevicesMatchingConnectionStates(states);
            } catch (RemoteException e) {
                Log.e(TAG, "Stack:" + Log.getStackTraceString(new Throwable()));
            }
        }
        return new ArrayList<BluetoothDevice>();
    }

    /**
     * {@inheritDoc}
     */
    public int getConnectionState(BluetoothDevice device) {
        if (DBG) log("getState(" + device + ")");
        if (mService == null) {
            Log.w(TAG, "Proxy not attached to service");
        } else if (isValidDevice(device)) {
            try {
                if (isDunEnabled()) {
                    return mService.getDunDeviceConnectionState(device);
                }
            } catch (RemoteException e) {
                Log.e(TAG, "Stack:" + Log.getStackTraceString(new Throwable()));
            }
        }
        return BluetoothProfile.STATE_DISCONNECTED;
    }

    /**
     * Set priority of the profile for the requested device
     *
     * <p> The device should already be paired.
     *  Priority can be one of {@link #PRIORITY_ON} or
     * {@link #PRIORITY_OFF},
     *
     * <p>Requires {@link android.Manifest.permission#BLUETOOTH_ADMIN}
     * permission.
     *
     * @param device Paired bluetooth device
     * @param priority
     * @return true if priority is set, false on error
     * @hide
     */
    public boolean setPriority(BluetoothDevice device, int priority) {
        if (DBG) log("setPriority(" + device + ", " + priority + ")");
        if (mService == null) {
            Log.w(TAG, "Proxy not attached to service");
        } else if (isValidDevice(device)) {
            if (priority != BluetoothProfile.PRIORITY_OFF &&
                priority != BluetoothProfile.PRIORITY_ON) {
              return false;
            }
            try {
                return mService.setDunDevicePriority(device, priority);
            } catch (RemoteException e) {
                Log.e(TAG, "Stack:" + Log.getStackTraceString(new Throwable()));
            }
        }
        return false;
    }

    /**
     * Get the priority of the profile for the requested device.
     *
     * <p> The priority can be any of:
     * {@link #PRIORITY_AUTO_CONNECT}, {@link #PRIORITY_OFF},
     * {@link #PRIORITY_ON}, {@link #PRIORITY_UNDEFINED}
     *
     * <p>Requires {@link android.Manifest.permission#BLUETOOTH} permission.
     *
     * @param device Bluetooth device
     * @return priority of the device
     * @hide
     */
    public int getPriority(BluetoothDevice device) {
        if (DBG) log("getPriority(" + device + ")");
        if (mService == null) {
            Log.w(TAG, "Proxy not attached to service");
        } else if (isValidDevice(device)) {
            try {
                return mService.getDunDevicePriority(device);
            } catch (RemoteException e) {
                Log.e(TAG, "Stack:" + Log.getStackTraceString(new Throwable()));
            }
        }
        return BluetoothProfile.PRIORITY_OFF;
    }

    private boolean isBtEnabled () {
       if (mAdapter.getState() == BluetoothAdapter.STATE_ON) return true;
       return false;
    }

    private boolean isValidDevice(BluetoothDevice device) {
       if (device == null) return false;

       if (BluetoothAdapter.checkBluetoothAddress(device.getAddress())) return true;
       return false;
    }

    private static void log(String msg) {
      Log.d(TAG, msg);
    }
}
