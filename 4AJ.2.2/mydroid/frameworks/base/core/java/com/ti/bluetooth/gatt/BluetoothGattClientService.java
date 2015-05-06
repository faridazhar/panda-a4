/*
 * Copyright (C) 2012 Texas Instruments Corporation
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

import com.ti.bluetooth.gatt.BluetoothGattClient;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothUuid;
import com.ti.bluetooth.gatt.IBluetoothGattClient;
import com.ti.bluetooth.gatt.IBluetoothGattClientCallback;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.os.ParcelUuid;
import android.os.PowerManager;
import android.provider.Settings;
import android.util.Log;

import java.util.ArrayList;
import java.util.Arrays;

import java.io.FileDescriptor;
import java.io.PrintWriter;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Set;
import java.io.IOException;

public class BluetoothGattClientService extends IBluetoothGattClient.Stub {
    private static final String TAG = "BluetoothGattClientService";
    private static final boolean DBG = true;
    private static final boolean VERBOSE = false;

    private int mNativeData;

    public static final String BLUETOOTH_GATTCLIENT_SERVICE = "bluetooth_gattclient";

    private static final String BLUETOOTH_ADMIN_PERM = android.Manifest.permission.BLUETOOTH_ADMIN;
    private static final String BLUETOOTH_PERM = android.Manifest.permission.BLUETOOTH;

    private static final String BLUETOOTH_ENABLED = "bluetooth_enabled";

    private static final String PROPERTY_STATE = "State";

    private static final int MESSAGE_REFRESH_REMDEV_SERVICES = 1;

    private static final int REMDEV_MAX_SERVICE_REFRESH_ATTEMPTS = 5;


    private final Context mContext;
    private final IntentFilter mIntentFilter;
    private final BluetoothService mBluetoothService;
    private final BluetoothAdapter mAdapter;

    private int mClientCount;

    // Wake lock
    private PowerManager mPowerMgr;
    private PowerManager.WakeLock mWakeLock;

    private final HashMap<String, RemoteGattDevice> mDevices;
    private final HashMap<String, RemoteGattDevice> mPendingDevices;
    private final HashMap<String, RemoteGattDevice> mKnownDevices;

    /*
     * Represents a remote GATT Server device. This object is used by the server
     * to track connections, and manage remote services and characteristics.
     */
    private final class RemoteGattDevice {
        private BluetoothDevice mDevice;

        public static final int CONNECTION_STATE_NONE = 0;
        public static final int CONNECTION_STATE_PENDING = 1;
        public static final int CONNECTION_STATE_CONNECTED = 2;

        private int mServiceRefreshCount;

        private HashMap<Integer,IBluetoothGattClientCallback> mCallbacks;
        private ArrayList<RemoteGattService> mServices;

        /*
         * Represents a remote GATT Service
         */
        private final class RemoteGattService {
            private String mService;
            private boolean mWatched;
            private HashMap<String,String> mServiceProperties;
            private String[] mCharacteristics;

            RemoteGattService(String svc) {
                logv("RemoteGattService:RemoteGattService("+svc+")");
                mService = svc;
                mWatched = false;
                mServiceProperties = new HashMap<String,String>();
            }

            /*
             * Register a watcher for notification/indication
             */
            public synchronized void registerWatcher() {
                logv("RemoteGattService:registerWatcher()" + mService);
                if (mWatched == false) {
                    try {
                        registerWatcherNative(mService);
                        mWatched = true;
                    } catch (Exception e) {
                        Log.e(TAG,"registerWatcherNative error:"+e);
                    }
                } else {
                    logd(mService + " already watched");
                }
            }

            /*
             * Unregister a watcher for notification/indication
             */
            public synchronized void unregisterWatcher() {
                logv("RemoteGattService:unregisterWatcher() " + mService);
                mWatched = false;
            }

            /*
             * Update remote service properties.
             */
            public synchronized void updateProperties() {
                logv("RemoteGattService:updateProperties()");
                String[] properties = null;
                int proplength = 0;
                try {
                    properties = (String [])getServicePropertiesNative(mService);
                    proplength=properties.length;
                } catch (NullPointerException e) {
                    Log.d(TAG,"getServicePropertiesNative got NULL");
                    properties = null;
                } catch (Exception e) {
                    Log.e(TAG,"getServicePropertiesNative Exception:"+e);
                    return;
                }

                mServiceProperties = new HashMap<String,String>();

                try {
                    for (int i = 0; i < proplength; i++) {
                        String name = properties[i];
                        String newValue = null;
                        int len;
                        if (name == null) {
                            Log.e(TAG, "Error: Remote Device Property at index" + i + "is null");
                            continue;
                        }
                        if (name.equals("Characteristics")) {
                            StringBuilder str = new StringBuilder();
                            len = Integer.valueOf(properties[++i]);
                            for (int j = 0; j < len; j++) {
                                str.append(properties[++i]);
                                str.append(",");
                            }
                            if (len > 0) {
                                newValue = str.toString();
                            }
                        } else {
                            newValue = properties[++i];
                        }
                        mServiceProperties.put(name, newValue);
                        logd(mService + ":"+name+"="+newValue);
                    }
                } catch (Exception e) {
                    Log.e(TAG,"Exception:"+e);
                }
            }

            public synchronized HashMap<String,String> getProperties() {
                logv("RemoteGattService:getProperties()");
                return mServiceProperties;
            }

            public synchronized String[] getCharacteristics() {
                logv("RemoteGattService:getCharacteristics()");
                return mCharacteristics;
            }

            public synchronized void setCharacteristics(String[] chars) {
                logv("RemoteGattService:setCharacteristics()");
                mCharacteristics = chars;
            }

            @Override
            public String toString() {
                return mService;
            }
        }

        RemoteGattDevice(BluetoothDevice device,String[] services) {
            logv("RemoteGattDevice:RemoteGattDevice() device:"+device.toString());
            initRemoteDevice(device,services);
        }

        RemoteGattDevice(BluetoothDevice device,String[] services,IBluetoothGattClientCallback cb,int id) {
            logv("RemoteGattDevice:RemoteGattDevice() device:"+device.toString());
            initRemoteDevice(device,services);
            mCallbacks.put(id,cb);
        }

        public synchronized void updateServices(String[] services) {
            logv("RemoteGattDevice:updateServices() device:"+mDevice.toString());
            mServices = new ArrayList<RemoteGattService>();
            for (int i = 0; i < services.length; i++) {
                mServices.add(new RemoteGattService(services[i]));
            }
        }

        private synchronized void initRemoteDevice(BluetoothDevice device, String[] services) {
            logv("RemoteGattDevice:InitRemoteDevice() device:"+device.toString());
            resetServiceRefreshCount();
            mDevice = device;
            mServices = new ArrayList<RemoteGattService>();
            for (int i = 0; i < services.length; i++) {
                mServices.add(new RemoteGattService(services[i]));
            }
            mCallbacks = new HashMap<Integer,IBluetoothGattClientCallback>();
        }

        public void addCb(IBluetoothGattClientCallback cb,int id) {
            logv("RemoteGattDevice:addCb() device:"+mDevice.toString()+" cbID:"+id);
            if (mCallbacks != null) {
                logd("cblist is valid");
                if (!mCallbacks.containsKey(id)) {
                    logd("cblist does not contain id:"+id);
                    mCallbacks.put(id,cb);
                } else
                    Log.e(TAG,"mCallbacks contains id:"+id);
            } else
                Log.e(TAG,"mCallbacks invalid");
        }

        public void removeCb(int id) {
            logv("RemoteGattDevice:removeCb() device:"+mDevice.toString()+"cbID:"+id);
            if (mCallbacks != null) {
                mCallbacks.remove(id);
            }
        }

        public void clearCbList() {
            logv("RemoteGattDevice:clearCbList() device:"+mDevice.toString());
            mCallbacks.clear();
        }

        public IBluetoothGattClientCallback[] getCbList() {
            logv("RemoteGattDevice:getCbList() device:"+mDevice.toString());
            if (mCallbacks != null && mCallbacks.values() != null) {
                return mCallbacks.values().toArray(new IBluetoothGattClientCallback[0]);
            }
            return null;
        }

        public synchronized String[] getServices() {
            logv("RemoteGattDevice:getServices() device:"+mDevice.toString());
            String[] services = new String[mServices.size()];
            for (int i = 0; i < mServices.size(); i++)
                services[i] = mServices.get(i).toString();
            return services;
        }

        public synchronized HashMap<String,String> getServiceProperties(String svc) {
            logv("RemoteGattDevice:getServiceProperties() device:"+mDevice.toString());
            for (int i = 0; i < mServices.size(); i++)
                if (mServices.get(i).toString().equals(svc)) {
                    return mServices.get(i).getProperties();
                }
            return null;
        }

        public synchronized void registerWatchers() throws Exception {
            logv("RemoteGattDevice:registerWatchers() device:"+mDevice.toString());
            for (int i = 0; i<mServices.size(); i++) {
                logd("Registering "+mServices.get(i).toString());
                mServices.get(i).registerWatcher();
            }
        }

        public synchronized void unRegisterWatchers() throws Exception {
            logv("RemoteGattDevice:unRegisterWatchers() device:"+mDevice.toString());
            for (int i = 0; i<mServices.size(); i++) {
                logd("Unregistering "+mServices.get(i).toString());
                mServices.get(i).unregisterWatcher();
            }
        }

        private synchronized void updateServicesProperties() {
            logv("RemoteGattDevice:updateServiceProperties() device:"+mDevice.toString());
            for (int i=0; i < mServices.size(); i++) {
                mServices.get(i).updateProperties();
            }
        }

        public synchronized void onConnected() {
            logv("RemoteGattDevice:onConnected() device:"+mDevice.toString());
            try {
                updateServicesProperties();
            } catch (Exception e) {
                Log.e(TAG,"onConnected Exception:"+e);
            }
        }

        public synchronized void onDisconnected() {
            logv("RemoteGattDevice:onDisconnected() device:"+mDevice.toString());
        }

        @Override
        public String toString() {
            return mDevice.toString();
        }

        public void increaseServiceRefreshCount() {
            mServiceRefreshCount++;
        }

        public int getServiceRefreshCount() {
            return mServiceRefreshCount;
        }

        public void resetServiceRefreshCount() {
            mServiceRefreshCount = 0;
        }
    }

    private RemoteGattDevice findRemDev(String objPath) {
        logv("BluetoothGattClientService:findRemDev("+objPath+")");
        RemoteGattDevice remdev = null;
        int service_start = objPath.indexOf("service");
        if (service_start>0) {
            String devicepath = objPath.substring(0,service_start-1);
            String address = mBluetoothService.getAddressFromObjectPath(devicepath);
            logd("searching for "+address + "("+devicepath+")");
            remdev = mDevices.get(address);
        }
        return remdev;
    }

    private final Handler mHandler = new Handler() {
        @Override
            public void handleMessage(Message msg) {
                switch (msg.what) {
                    case MESSAGE_REFRESH_REMDEV_SERVICES:
                        String address = (String)msg.obj;
                        if (address == null) return;

                        RemoteGattDevice remdev = mDevices.get(address);
                        if (remdev == null) return;

                        logv("mHandler received MESSAGE_REFRESH_REMDEV_SERVICES for device" + address);
                        BluetoothDeviceProperties deviceProperties = mBluetoothService.getDeviceProperties();
                        deviceProperties.updateCache(address);
                        String Services = deviceProperties.getProperty(address, "Services");
                        logd("Device " + address + " services:"+Services);
                        String[] ServiceStr = new String[0];
                        if (Services != null) {
                            ServiceStr = Services.split (",");
                        } else {
                            int refreshCount = remdev.getServiceRefreshCount();

                            if (refreshCount < REMDEV_MAX_SERVICE_REFRESH_ATTEMPTS) {
                                remdev.increaseServiceRefreshCount();
                                Message message = mHandler.obtainMessage(MESSAGE_REFRESH_REMDEV_SERVICES);
                                message.obj = address;
                                mHandler.sendMessageDelayed(message,500);
                                logv("Sending MESSAGE_REFRESH_REMDEV_SERVICES to refresh services again");
                            } else {
                                logv("Sending MESSAGE_REFRESH_REMDEV_SERVICES maxed out service refresh attempts.");
                            }
                            return;
                        }

                        logv("mHandler received MESSAGE_REFRESH_REMDEV_SERVICES updating device services");
                        remdev.updateServices(ServiceStr);
                        mKnownDevices.put(address,remdev);
                        remdev.onConnected();
                        mDevices.put(address, remdev);
                        try {
                            remdev.registerWatchers();
                        } catch (Exception e) {
                            Log.e(TAG,"Exception:"+e);
                        }
                }
            }
    };

    private synchronized void onCharacteristicPropertyChanged(String deviceObjectPath, String[] propValues) {
        logv("BluetoothGattClientService:onCharacteristicPropertyChanged path="+deviceObjectPath+" prop="+propValues[0] +" value="+propValues[1]);
        RemoteGattDevice remdev = findRemDev(deviceObjectPath);
        if (remdev != null) {
            logd("device found");
            IBluetoothGattClientCallback[] rem_cb = remdev.getCbList();
            for (int i = 0; i < rem_cb.length; i++) {
                try {
                    rem_cb[i].propertyChanged(deviceObjectPath, propValues[0], propValues[1]);
                } catch (Exception e) {
                    Log.e(TAG,"cb.propertyChanged error:"+e);
                }
            }
        } else {
               logd("Could not find device");
        }
    }

    private synchronized void onCharValueChanged(String objectPath,byte[] buff) {
        logv("BluetoothGattClientService:onCharValueChanged path="+objectPath);
        String data = toHex(buff);
        logd("ValueChanged objPath:"+ objectPath+ " data:"+data);

        RemoteGattDevice remdev = findRemDev(objectPath);

        if (remdev != null) {
               logd("device found");
            IBluetoothGattClientCallback[] rem_cb = remdev.getCbList();
            for (int i = 0; i < rem_cb.length; i++) {
                try {
                    rem_cb[i].valueChanged(objectPath, buff);
                } catch (Exception e) {
                    Log.e(TAG,"cb.valueChanged error:"+e);
                }
            }
        } else {
               logd("Could not find device");
        }
    }

    private synchronized void onCharValueWriteReqComplete(String objectPath, byte result) {
        logv("BluetoothGattClientService:onCharValueWriteReqComplete objPath:"+ objectPath + " result:"+result);

        RemoteGattDevice remdev = findRemDev(objectPath);
        if (remdev != null) {
               logd("device found");
            IBluetoothGattClientCallback[] rem_cb = remdev.getCbList();
            for (int i = 0; i < rem_cb.length; i++) {
                try {
                    rem_cb[i].valueWriteReqComplete(objectPath, result);
                } catch (Exception e) {
                    Log.e(TAG,"valueWriteReqComplete error:"+e);
                }
            }
        }
    }

    private synchronized void onCharValueReadComplete(String objectPath, byte result, byte[] buff) {
        String data = toHex(buff);
        logd("BluetoothGattClientService:onCharValueReadComplete objPath:"+ objectPath + " status:"+result + " data:"+data);

           RemoteGattDevice remdev = findRemDev(objectPath);
           if (remdev != null) {
               logd("device found");
               IBluetoothGattClientCallback[] rem_cb = remdev.getCbList();
               for (int i = 0; i < rem_cb.length; i++) {
                   try {
                       rem_cb[i].valueReadComplete(objectPath, result, buff);
                   } catch (Exception e) {
                       Log.e(TAG,"valueReadComplete error:"+e);
                   }
               }
           }
    }


    private synchronized void addRemoteDevice(BluetoothDevice device)
    {
        RemoteGattDevice remdev;
        logv("BluetoothGattClientService:addRemoteDevice "+device.toString());

        BluetoothDeviceProperties deviceProperties = mBluetoothService.getDeviceProperties();
        deviceProperties.updateCache(device.toString());
        String Services = deviceProperties.getProperty(device.toString(), "Services");
        logd("Device "+device.toString() + " services:"+Services);
        String[] ServiceStr = new String[0];
        if (Services != null) {
            ServiceStr = Services.split (",");
        } else {
            Message message = mHandler.obtainMessage(MESSAGE_REFRESH_REMDEV_SERVICES);
            message.obj = device.toString();
            mHandler.sendMessageDelayed(message,1000);
            logv("Sending MESSAGE_REFRESH_REMDEV_SERVICES to refresh services again");
        }
        remdev = new RemoteGattDevice(device, ServiceStr);
        mKnownDevices.put(device.toString(),remdev);
        remdev.onConnected();
        mDevices.remove(device.toString());
        mDevices.put(device.toString(), remdev);
        try {
            remdev.registerWatchers();
        } catch (Exception e) {
            Log.e(TAG,"Exception:"+e);
        }
    }

    private synchronized void onConnected(BluetoothDevice device) {
        logv("BluetoothGattClientService:onConnected device="+device);
        RemoteGattDevice remdev = mPendingDevices.get(device.toString());
        if (remdev != null) {
            logd("Device "+device.toString() + " gatt connection complete");
            remdev.onConnected();

            mDevices.put(device.toString(), remdev);
            mKnownDevices.put(device.toString(),remdev);
            mPendingDevices.remove(device.toString());

            try {
                remdev.registerWatchers();
            } catch (Exception e) {
                Log.e(TAG,"Exception:"+e);
            }

            IBluetoothGattClientCallback[] rem_cb = remdev.getCbList();

            for (int i = 0; i < rem_cb.length; i++) {
                try {
                    rem_cb[i].clientConnected(device.toString());
                } catch (Exception e) {
                    Log.e(TAG,"clientConnected error:"+e);
                }
            }
        } else if (device.getBondState() == BluetoothDevice.BOND_BONDED) {
            logd("Device " + device.toString() + " is not connected to GATT. creating new");
            addRemoteDevice(device);
        } else {
            logd("Device " + device.toString() + " is not connected to GATT and not bonded - ignoring.");
        }
    }

    private synchronized void onDisconnected(BluetoothDevice device) {
        logv("BluetoothGattClientService:OnDisconnected device="+device);
        RemoteGattDevice remdev = mDevices.get(device.toString());
        if (remdev == null) {
            logd("device is not connected - checking the pending connections");
            remdev = mPendingDevices.get(device.toString());
        }

        if (remdev != null) {
            Disconnect(device,0);
            try {
                remdev.unRegisterWatchers();
            } catch (Exception e) {
                Log.e(TAG,"remdev.unRegisterWatchers exception:"+e);
            }

            try {
                mDevices.remove(device.toString());
            } catch (Exception e) {
                Log.e(TAG,"Error removing device from connected list:"+e);
            }

            remdev.onDisconnected();
            logd("Device "+ device.toString() + " gatt disconnection complete");
            IBluetoothGattClientCallback[] rem_cb = remdev.getCbList();

            for (int i = 0; i < rem_cb.length; i++) {
                try {
                    rem_cb[i].clientDisconnected(device.toString());
                } catch (Exception e) {
                    Log.e(TAG,"clientDisconnected error:"+e);
                }
            }
            remdev.clearCbList();
        } else {
            logd("Device " + device.toString() + " is not connected to GATT. Ignoring");
        }
    }

    private final BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            BluetoothDevice device =
                    intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
            if (action.equals(BluetoothAdapter.ACTION_STATE_CHANGED)) {
                int state = intent.getIntExtra(BluetoothAdapter.EXTRA_STATE,
                                               BluetoothAdapter.ERROR);
                switch (state) {
                case BluetoothAdapter.STATE_ON:
                    logd("BluetoothGattClientService::onReceive: BluetoothAdapter.STATE_ON");
                    onBluetoothEnable();
                    break;
                case BluetoothAdapter.STATE_TURNING_OFF:
                    logd("BluetoothGattClientService::onReceive: BluetoothAdapter.STATE_TURNING_OFF");
                    onBluetoothDisable();
                    break;
                }
            } else if (action.equals(BluetoothDevice.ACTION_BOND_STATE_CHANGED)) {
                int bondState = intent.getIntExtra(BluetoothDevice.EXTRA_BOND_STATE,
                                                   BluetoothDevice.ERROR);
                switch(bondState) {
                case BluetoothDevice.BOND_BONDED:
                    logd("BluetoothGattClientService::onReceive: BluetoothDevice.BOND_BONDED "+device.toString());
                    addRemoteDevice(device);
                    break;
                case BluetoothDevice.BOND_NONE:
                    if (mKnownDevices.get(device.toString()) != null) {
                        logd("BluetoothGattClientService::onReceive: BluetoothDevice.BOND_NONE "+device.toString()+" is no longer known");
                        mKnownDevices.remove(device.toString());
                    }
                    break;
                }
            } else if (action.equals(BluetoothDevice.ACTION_ACL_CONNECTED)) {
                synchronized (this) {
                    logd("BluetoothGattClientService::onReceive: BluetoothDevice.ACTION_ACL_CONNECTED:"+device.toString());
                    onConnected(device);
                }
            } else if (action.equals(BluetoothDevice.ACTION_ACL_DISCONNECTED)) {
                synchronized (this) {
                    logd("BluetoothGattClientService::onReceive: BluetoothDevice.ACTION_ACL_DISCONNECTED:"+device.toString());
                    onDisconnected(device);
                }
            }
        }
    };

    static { classInitNative(); }

    public BluetoothGattClientService(Context context, BluetoothService bluetoothService) {
        logv("BluetoothGattClientService:BluetoothGattClientService");

        mContext = context;
        mClientCount = 0;
        mBluetoothService = bluetoothService;

        if (mBluetoothService == null) {
            throw new RuntimeException("Platform does not support Bluetooth");
        }

        if (!initNative()) {
            throw new RuntimeException("Could not init BluetoothGattClient");
        }

        // get wake lock
        mPowerMgr = (PowerManager) mContext.getSystemService(Context.POWER_SERVICE);
        mWakeLock = mPowerMgr.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK,TAG);


        mAdapter = BluetoothAdapter.getDefaultAdapter();

        mIntentFilter = new IntentFilter(BluetoothAdapter.ACTION_STATE_CHANGED);
        mIntentFilter.addAction(BluetoothDevice.ACTION_BOND_STATE_CHANGED);
        mIntentFilter.addAction(BluetoothDevice.ACTION_ACL_CONNECTED);
        mIntentFilter.addAction(BluetoothDevice.ACTION_ACL_DISCONNECTED);
        mContext.registerReceiver(mReceiver, mIntentFilter);

        mDevices = new HashMap<String, RemoteGattDevice>();
        mPendingDevices = new HashMap<String, RemoteGattDevice>();
        mKnownDevices =  new HashMap<String, RemoteGattDevice>();

        if (mBluetoothService.isEnabled())
            onBluetoothEnable();
    }

    @Override
    protected void finalize() throws Throwable {
        logv("BluetoothGattClientService:finalize()");
        try {
            cleanupNative();
        } finally {
            super.finalize();
        }
    }

    private synchronized void onBluetoothEnable() {
        logv("BluetoothGattClientService:onBluetoothEnable");
    }

    private synchronized void callDisconnectCB(RemoteGattDevice remdev) {
        logv("BluetoothGattClientService:callDisconnectCB()");
        if (remdev != null) {
            remdev.onDisconnected();
            IBluetoothGattClientCallback[] rem_cb = remdev.getCbList();

               for (int i = 0; i < rem_cb.length; i++) {
                   logd("calling remdev cb "+i);
                   try {
                       rem_cb[i].clientDisconnected(remdev.toString());
                   } catch (Exception e) {
                       Log.e(TAG,"clientDisconnected error:"+e);
                   }
               }
            remdev.clearCbList();
        }
    }

    private synchronized void onBluetoothDisable() {
        logv("BluetoothGattClientService:onBluetoothDisable");
        /* Trying to disconnect all and remove from lists */

        checkPermissions();

        String[] addresses = mDevices.keySet().toArray(new String[0]);
            for (String address : addresses) {
                logd("Trying to disconnect from "+address);
                RemoteGattDevice remdev = mDevices.get(address);
                callDisconnectCB(remdev);
                mDevices.remove(address);
            }

        addresses = mPendingDevices.keySet().toArray(new String[0]);
            for (String address : addresses) {
                logd("Trying to disconnect from "+address);
                RemoteGattDevice remdev = mPendingDevices.get(address);
                callDisconnectCB(remdev);
                mPendingDevices.remove(address);
            }

        addresses = mKnownDevices.keySet().toArray(new String[0]);
        for (String address : addresses) {
            logd("Removing known device " + address);
            mKnownDevices.remove(address);
        }
    }

    /*
     * Utility for checking Bluetooth permissions
     */
    private void checkPermissions() {
        mContext.enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Requires BLUETOOTH Permission");
    }

    /*
     * Utility for logging
     */
    private static void logd(String msg) {
        if (DBG) Log.d(TAG, msg);
    }

    private static void logv(String msg) {
        if (VERBOSE) Log.i(TAG, msg);
    }

    /*
     * Utility for converting a byte array into a printable
     * string of bytes.
     */
    private static String toHex(byte[] digest) {
        StringBuilder sb = new StringBuilder();
        for (byte b : digest) {
            sb.append(String.format("%1$02X", b));
        }

        return sb.toString();
    }

    /*
     * Native function declarations
     */
    private native boolean initNative();
    private static native void classInitNative();
    private native void connectNative(String path);
    private native void disconnectNative(String path);
    private native void cleanupNative();
    private native boolean registerWatcherNative(String servicePath);
    private native boolean unregisterWatcherNative(String servicePath);
    private native Object[] getServicePropertiesNative(String objectPath);
    private native Object[] discoverServiceCharacteristicsNative(String objectPath);
    private native Object[] getCharacteristicPropertiesNative(String objectPath);
    private native boolean setCharacteristicPropertyStringNative(String objectPath, String key, String value);
    private native boolean setCharacteristicPropertyIntegerNative(String objectPath, String key, int value);
    private native boolean setCharacteristicPropertyBooleanNative(String objectPath, String key, int value);
    private native boolean writeValueRequestNative(String objectPath, byte[] value);
    private native boolean writeValueCommandNative(String objectPath, byte[] value);
    private native boolean readValueCommandNative(String objectPath,int offset);

    /*
     * Connect to a device
     */
    public synchronized int Connect(BluetoothDevice device, IBluetoothGattClientCallback cb) {
        logv("BluetoothGattClientService:Connect("+ device.toString()+")");
        checkPermissions();

        if (device.getBondState() == BluetoothDevice.BOND_NONE) {
            logd("Device not bonded.");
            return BluetoothGattClient.GATT_CLIENT_ERROR_NOT_BONDED;
        }

        RemoteGattDevice remdev = mDevices.get(device.toString());

        if (remdev == null) { /* Not yet connected - lets check if we're pending connection */
            logd("device "+device.toString()+" is not currently connected");
            remdev = mPendingDevices.get(device.toString());

            if (remdev == null) { /* Not yet pending connection - lets put it as pending and wait for connection */
                logd("No pending connection for "+device.toString());

                remdev = mKnownDevices.get(device.toString());

                if (remdev == null) { /* This device was not yet connected. Lets create a new one */
                    logd("device "+device.toString()+" is unkown. Creating new device");
                    BluetoothDeviceProperties deviceProperties = mBluetoothService.getDeviceProperties();
                    deviceProperties.updateCache(device.toString());
                    String Services = deviceProperties.getProperty(device.toString(), "Services");
                    String[] ServiceStr = new String[0];
                    if (Services != null)
                        ServiceStr = Services.split (",");

                    remdev = new RemoteGattDevice(device, ServiceStr,cb,++mClientCount);
                } else {
                    logd("device "+device.toString() + "is already known and will be used again.");
                    remdev.addCb(cb,++mClientCount); /* Need to add a new callback */
                }

                mPendingDevices.put(device.toString(), remdev);

                if (mBluetoothService.getAttDeviceConnectionState(device) == 0) {
                    logd("Device "+device.toString()+" is not yet connected - trying to invoke connection");
                    mBluetoothService.connectAttDevice(device);
                }

            } else {
                logd("Already trying to connected to "+device.toString());
                remdev.addCb(cb,++mClientCount);
            }
        } else {
            logd("device "+device.toString()+" is already connected");
            try {
                   remdev.addCb(cb,++mClientCount);
                   cb.clientConnected(device.toString());
            } catch (Exception e) {
                   Log.e(TAG,"Exception:"+e);
            }
        }

        return mClientCount;
    }

    /*
     * Cancel a connection to a device (currently not implemented)
     */
    public synchronized int CancelConnect(BluetoothDevice device,int id) {
        logv("BluetoothGattClientService:CancelConnect("+ device.toString()+")");
        checkPermissions();
        String path = mBluetoothService.getObjectPathFromAddress(device.toString());
        logd("Trying to cancel connect to :" + path);
        logd("not implemented yet");
        return 0;
    }

    /*
     * Disconnect from a device, or cancel a connection attempt
     */
    public synchronized int Disconnect(BluetoothDevice device,int id) {
        logv("BluetoothGattClientService:Disconnect(" + device.toString()+")");
        checkPermissions();

        RemoteGattDevice remdev = mDevices.get(device.toString());
        if (remdev == null) {
            logd("device is not connected. Trying pending");
            remdev = mPendingDevices.get(device.toString());
        }

        if (remdev != null) {
            logd("device "+device.toString()+ " is connected to "+remdev.getCbList().length+" devices");

            if (id != 0)
                remdev.removeCb(id);

            try {
                mPendingDevices.remove(device.toString());
            } catch (Exception e) {
                Log.e(TAG,"Error removing device from pending list:"+e);
            }
        } else {
            Log.e(TAG, "Error : failed to get remdev");
        }
        return 0;
    }

    /*
     * Get remote device GATT services list
     */
    public synchronized String[] getServiceList(BluetoothDevice device) {
        logv("BluetoothGattClientService:getServiceList(" + device.toString()+")");
        checkPermissions();

        RemoteGattDevice remdev = mDevices.get(device.toString());
        if (remdev != null) {
            String[] services = remdev.getServices();
            return services;
        }

        logd("Device " + device.toString() + " is not connected to GATT. Ignoring");
        return null;
        }

        /*
        * Get remote device GATT service properties
        */
        public synchronized String[] getServiceProperties(BluetoothDevice device, String service) {
            logv("BluetoothGattClientService:getServiceProperties device="+device+" service="+service);
            checkPermissions();

            RemoteGattDevice remdev = mDevices.get(device.toString());
            if (remdev != null) {
                String[] props = (String[])getServicePropertiesNative(service);
                return props;
            }
            return null;
        }

        /*
        * Get remote device GATT Service characteristic list
        */
        public synchronized String[] getCharacteristicList(BluetoothDevice device,
                                                                               String service) {
            logv("BluetoothGattClientService:getCharacteristicList device="+device+" service="+service);
            checkPermissions();

            RemoteGattDevice remdev = mDevices.get(device.toString());
            if (remdev != null) {
                String[] chars = (String[])discoverServiceCharacteristicsNative(service);
                return chars;
            }

            logd("Device " + device.toString() + " is not connected to GATT. Ignoring");
            return null;
        }

        /*
        * Get remote device GATT Service characteristic properties
        */
        public synchronized String[] getCharacteristicProperties(BluetoothDevice device,
                                                                               String characteristic) {
            logv("BluetoothGattClientService:getCharacteristicProperties device="+device+" char="+characteristic);
            checkPermissions();

            RemoteGattDevice remdev = mDevices.get(device.toString());
            if (remdev != null) {
                String[] chars = (String[])getCharacteristicPropertiesNative(characteristic);
                return chars;
            }

            logd("Device " + device.toString() + " is not connected to GATT. Ignoring");
            return null;
        }

        /*
        * enable/disable characteristic notifying
        */
        public synchronized boolean setNotifying(BluetoothDevice device,
                                                                 String characteristic,boolean notify) {
            logd("BluetoothGattClientService:setNotifying device="+device+" char="+characteristic+" notify="+notify);
            checkPermissions();

            RemoteGattDevice remdev = mDevices.get(device.toString());
            if (remdev != null) {
                return setCharacteristicPropertyBooleanNative(characteristic,
                                                              "Notify",
                                                              (notify ? 1:0));
            }

            return false;
        }

        /*
        * enable/disable characteristic indication
        */
        public synchronized boolean setIndicating(BluetoothDevice device,
                                                    String characteristic,boolean indicate) {
            logd("BluetoothGattClientService:setIndicating device="+device+" char="+characteristic+" indicate="+indicate);
            checkPermissions();

            RemoteGattDevice remdev = mDevices.get(device.toString());
            if (remdev != null) {
                return setCharacteristicPropertyBooleanNative(characteristic,
                                                              "Indicate",
                                                              (indicate ? 1:0));
            }

            return false;
        }

        /*
        * enable/disable characteristic broadcasting
        */
        public synchronized boolean setBroadcasting(BluetoothDevice device,
                                                    String characteristic,boolean broadcast) {
            logd("BluetoothGattClientService:setBroadcasting device="+device+" char="+characteristic+" broadcast="+broadcast);
            checkPermissions();

            RemoteGattDevice remdev = mDevices.get(device.toString());
            if (remdev != null) {
                return setCharacteristicPropertyBooleanNative(characteristic,
                                                                "Broadcast",
                                                                (broadcast ? 1:0));
            }

            return false;
        }

        /*
        * Read a characteristic value
        */
        public synchronized boolean Read(BluetoothDevice device,
                                                                String characteristic, int offset) {
            logd("BluetoothGattClientService:Read device="+device+" char="+characteristic+" offset="+offset);
            logd("char:"+characteristic);
            logd("offset:"+offset);
            checkPermissions();

            RemoteGattDevice remdev = mDevices.get(device.toString());
            if (remdev != null) {
                return readValueCommandNative(characteristic,offset);
            }

            logd("Device " + device.toString() + " is not connected to GATT. Ignoring");
            return false;
        }

        /*
        * Write a characteristic value
        */
        public synchronized boolean Write(BluetoothDevice device,
                                                          String characteristic,byte[]
                                                          value, boolean waitForResponse) {
            String data = toHex(value);
            logd("BluetoothGattClientService:Write device="+device+" char="+characteristic+" waitForRsp="+waitForResponse);
            logd("char:"+characteristic);
            logd("value:"+data);
            logd("Request:"+waitForResponse);
            checkPermissions();

            RemoteGattDevice remdev = mDevices.get(device.toString());
            if (remdev != null) {
                if (waitForResponse)
                    return writeValueRequestNative(characteristic,value);
                else
                    return writeValueCommandNative(characteristic,value);
            }

            logd("Device " + device.toString() + " is not connected to GATT. Ignoring");
            return false;
        }

        /*
        * Check if remote device is currently connected
        */
        public synchronized boolean isConnected(BluetoothDevice device) {
            checkPermissions();

            RemoteGattDevice remdev = mDevices.get(device.toString());
            logv("isConnected "+device.toString() +"="+(remdev == null ? "false" : "true"));
            return (remdev != null);
        }
}
