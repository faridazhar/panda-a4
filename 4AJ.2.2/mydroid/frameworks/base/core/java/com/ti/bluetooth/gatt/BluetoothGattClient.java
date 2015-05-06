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

package com.ti.bluetooth.gatt;

import java.io.Closeable;
import java.io.IOException;
import java.util.Iterator;
import java.util.UUID;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.concurrent.locks.ReentrantReadWriteLock;
import java.util.Random;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.RemoteException;
import android.os.IBinder;
import android.os.ServiceManager;
import android.os.ParcelUuid;
import android.os.RemoteException;
import android.os.Handler;
import android.util.Log;
import android.bluetooth.BluetoothDevice;
import android.server.BluetoothGattClientService;

import com.ti.bluetooth.gatt.IBluetoothGattClient;

/**
 * Represents a remote Bluetooth Gatt Client.
 *
 * <p>
 * This class provides interface for basic GATT Client API's to allow a user to
 * build his own LE profile, using simplified GATT Client objects.
 *
 * <p class="note">
 * <strong>Note:</strong> Requires the
 * {@link android.Manifest.permission#BLUETOOTH} permission.
 *
 * <p class="note">
 * <strong>Note:</strong> This is just a perliminary API.
 *
 */
public final class BluetoothGattClient extends IBluetoothGattClientCallback.Stub {
    private static final String TAG = "BluetoothGattClient";
    private static final boolean DBG = true;
    private static final boolean VERBOSE = false;

    private final BluetoothDevice mDevice; /* remote device */

    private IBluetoothGattClient mService;
    private final Context mContext;
    private BluetoothGattClientCB mClientCB;
    private ArrayList<GattService> mPrimaries = null;
    private HashMap <String, GattCharacteristic> mCharacteristics = null;

    public final static int GATT_CLIENT_ERROR_NOT_BONDED                = -1;
    public final static int GATT_CLIENT_ERROR_NATIVE_INIT_FAILED        = -2;
    public final static int GATT_CLIENT_ERROR_CONNECT_NATIVE            = -3;
    public final static int GATT_CLIENT_ERROR_DISCONNECT_NATIVE         = -4;

    private static Random mRandomGenerator;

    private int mId = 0;

    public interface BluetoothGattClientCB {
        abstract void onClientConnected(String addr);
        abstract void onClientDisconnected(String addr);
    }

    public interface GattCharCb {
        abstract void onValueChanged(GattCharacteristic obj, byte[] value);
        abstract void onPropertyChanged(GattCharacteristic obj, String property, String value);
        abstract void onWriteComplete(GattCharacteristic obj, byte status);
        abstract void onReadComplete(GattCharacteristic obj, byte status,byte[] value);
    }

    /*
     * IBluetoothGattClientCallback overrides
     */

    /**
     * Value changed callback function
     *
     * <p> This function implements the IBluetoothGattClientCallback valueChanged
     * callback function to get indication when the remote value is changed due to
     * a notification or indication from the server.
     *
     * @param path is the characteristic path for the notified/indicated characteristic.
     *
     * @param value is the new value of the notified/indicated characteristic.
     */
    @Override
    public void valueChanged(String path, byte[] value) {
        logv("Value changed on "+path);

        GattCharacteristic chr = mCharacteristics.get(path);
        if (chr != null)
            chr.notifyValueChanged(value);
    }

    @Override
    public void propertyChanged(String path,String property,String value) {
        logv("property changed on "+path);
        GattCharacteristic chr = mCharacteristics.get(path);
        if (chr != null) {
            if (property.equals("Indicate"))
                chr.setIndicating(value.equals("true"));
            else if (property.equals("Broadcast"))
                chr.setBroadcasting(value.equals("true"));
            else if (property.equals("Notify"))
                chr.setNotifying(value.equals("true"));
        }
    }

    /**
     * Client Connected callback function
     *
     * <p> This function implements the IBluetoothGattClientCallback clientConnected
     * callback function to get indication when the remote client is connected.
     *
     * @param addr is the address of the remote server
     *
     */
    @Override
    public void clientConnected(String addr) {
        logv("clientConnected "+addr);
        if (mClientCB != null)
            mClientCB.onClientConnected(addr);
    }

    /**
     * Client Disconnected callback function
     *
     * <p> This function implements the IBluetoothGattClientCallback clientDisconnected
     * callback function to get indication when the remote client is disconnected.
     *
     * @param addr is the address of the remote server
     *
     */
    @Override
    public void clientDisconnected(String addr) {
        logv("clientDisconnected "+addr);
        if (mClientCB != null)
            mClientCB.onClientDisconnected(addr);
    }

    /**
     * Characteristic value write request complete callback function
     *
     * <p> This function implements the IBluetoothGattClientCallback valueWriteReqComplete
     * callback function to get indication when the Write request is completed.
     *
     * @param path is the characteristic path.
     *
     * @param result is the Write request response error code.
     *
     */
    @Override
    public void valueWriteReqComplete(String path, byte result) {
        logv("valueWriteReqComplete path:"+path+" result:"+result);

        GattCharacteristic chr = mCharacteristics.get(path);
        if (chr != null)
            chr.notifyWriteComplete(result);
    }

    @Override
    public void valueReadComplete(String path, byte result,byte[] data) {
        logv("valueReadComplete path:"+path+" result:"+result+" data[0]:"+data[0]);
        GattCharacteristic chr = mCharacteristics.get(path);
        if (chr != null)
            chr.notifyReadComplete(result, data);
    }

    /**
     * Create a BluetoothGattClient proxy object.
     */
    public BluetoothGattClient(Context context, BluetoothDevice device,BluetoothGattClientCB clientCB) {
        mContext = context;
        mDevice = device;
        mClientCB = clientCB;

        IBinder b = ServiceManager.getService(BluetoothGattClientService.BLUETOOTH_GATTCLIENT_SERVICE);
        if (b != null) {
            mService = IBluetoothGattClient.Stub.asInterface(b);
        } else {
            Log.w(TAG, "Bluetooth Gatt Client service not available!");
            mService = null;
        }

        mCharacteristics = new HashMap <String, GattCharacteristic>();
    }

    /*
     * Add a characteristic to the known characteristics list
     */
    private void addChar(String path, GattCharacteristic chr) {
        mCharacteristics.put(path,chr);
    }

    /*
     * remove a characteristic from the known characteristics list
     */
    private void removeChar(String path) {
        mCharacteristics.remove(path);
    }

    /*
     * gat a characteristic from the known characteristics list
     */
    private GattCharacteristic getChar(String path) {
        return mCharacteristics.get(path);
    }

    /*
     * get the assigned device
     */
    public BluetoothDevice getDevice() {
        return mDevice;
    }

    /**
     * Represents a GATT Client Exception.
     *
     * <p>
     * This exception will be thrown on errors in the Gatt Client methods and
     * objects.
     *
     *
     */
    public class GattClientException extends IOException {
        public GattClientException() {
            super();
        }

        public GattClientException(IOException e) {
            super(e.toString(),e.getCause());
        }

        public GattClientException(String detailMessage) {
            super(detailMessage);
        }

        public GattClientException(String message, Throwable cause) {
            super(message, cause);
        }

        public GattClientException(Throwable cause) {
            super(cause == null ? null : cause.toString(), cause);
        }
    }

    /**
     * Represents a GATT Service.
     *
     * <p>
     * Gatt Service contains multiple GATT Caharacteristics, which can be
     * accessed using the {@link GattCharacteristic} object
     *
     * <p class="note">
     * <strong>Note:</strong> This is just a perliminary API.
     *
     */
    public class GattService {
        private static final String TAG = "GattService";
        private UUID mUuid;
        private BluetoothGattClient mGattClient;
        private boolean mValid;
        private ArrayList<GattCharacteristic> mCharacteristics;
        private Handler mHandler;
        private String mPath;
        private String[] mProperties = null;

        public static final int GATT_CHARATERISTIC_CHANGED_MSG = 100;

        /*
         * Utiltiy function to dump the current service + characteristics
         */
        public void dump() {
            try {
                logd("------------>8------------ GattService dump start ------------>8------------");
                logd(mUuid.toString());
                logd(mPath);
                logd(mGattClient.getDevice().toString());
                if (mCharacteristics != null && mCharacteristics.size() > 0) {
                    for (int i = 0;i < mCharacteristics.size(); i++)
                        mCharacteristics.get(i).dump();
                } else
                    logd("No charactersitics!");
                    logd("------------8<------------ GattService dump end   ------------8<------------");
            } catch (Exception e) {
                Log.e(TAG,"dump exception "+e);
            }
        }

        /**
         * GATT Service constructor
         *
         * @param gattClient
         *            reference to the parent BluetoothGattClient
         * @param uuid
         *            is the uuid of this service
         * @param startHandle
         *            The Starting handle for this service.
         * @param endHandle
         *            The Ending handle for this service.
         *
         */
        /* package */private GattService(BluetoothGattClient gattClient,
                String srvPath) throws GattClientException {
            logv("START OF GattService "+srvPath);
            checkDeviceConnected();

            mValid = true;
            mGattClient = gattClient;
            mPath = srvPath;
            try {
                mProperties = mService.getServiceProperties(mDevice,mPath);
            } catch (Exception e) {
                Log.e(TAG,"getServiceProperties failed:"+e);
            }
            parseProperties();
            mCharacteristics = null;
            logd("END OF GattService "+srvPath);
        }

        /*
         * Parse the native properties and extract usefull info
         */
        private void parseProperties() {
            mUuid = UUID.fromString("00000000-0000-1000-8000-00805f9b34fb");
            try {
                if (mProperties != null) {
                    for (int j = 0; j < mProperties.length; j++) {
                        logd("service props:"+mProperties[j]);
                        if (mProperties[j].equals("Characteristics")) {
                            int char_count = Integer.valueOf(mProperties[++j]);
                            for (int i = 0; i<char_count; i++) {
                                logd("parseProperties Characteristic("+i+"):"+mProperties[++j]);
                            }
                        } else if (mProperties[j].equals("UUID")) {
                            mUuid = UUID.fromString(mProperties[++j]);
                        } else {
                            Log.e(TAG,"parseProperties Invalid property:"+mProperties[j]+" ("+mPath+")");
                            break;
                        }
                    }
                }
            } catch (Exception e) {
                Log.e(TAG,"parseProperties exception:"+e);
            }
        }

        /**
         * Check if cache is still valid. If not, throw exception.
         *
         * @throws GattClientException
         *             in case the GATT Attribute cache is no longer valid.
         * @hide
         */
        private void checkValid() throws GattClientException {
            if (!mValid)
                throw new GattClientException("Invalid handle cache");
        }

        /**
         * Register a handler for GATT Server notification/indication
         *
         * @param handler
         *            Handler to receive the notification/indication
         * @throws GattclientException
         *             on error, for example Bluetooth not connected.
         *
         */
        public void registerValueChangedCB(Handler handler)
                throws GattClientException {
            checkValid();
            mHandler = handler;
        }

        /*
         * Internal utility to discover all service characteristics
         */
        private void discoverAllChars() {
            try {
                mCharacteristics = discoverAllCharacteristics(mPath);
                if (mCharacteristics != null) {
                    for (int i = 0;i < mCharacteristics.size(); i++) {
                        mCharacteristics.get(i).setParentService(this);
                    }
                }
            }catch (Exception e) {
                Log.e(TAG,"discoverAllCharacteristics :"+e);
            }
        }

        /**
         * Get characteristics for this service
         *
         * @return ArrayList<GattCharacteristic> List of characteristics.
         * @throws GattclientException
         *             on error, for example Bluetooth not connected.
         *
         */
        public ArrayList<GattCharacteristic> getCharacteristics()
                throws GattClientException {
            checkValid();
            if (mCharacteristics == null)
                discoverAllChars();

            return mCharacteristics;
        }

        /**
         * Get a list of characteristics with the provided UUID
         *
         * @param uuid
         *            The UUID of the requested characteristic.
         * @return ArrayList<GattCharacteristic> List of characteristics.
         * @throws GattclientException
         *             on error, for example Bluetooth not connected.
         *
         */
        public ArrayList<GattCharacteristic> getCharacteristics(UUID uuid)
                throws GattClientException {
            checkValid();

            if (mCharacteristics == null)
                discoverAllChars();

            if (mCharacteristics != null) {
                ArrayList<GattCharacteristic> gattCharList = new ArrayList<GattCharacteristic>();

                for (int i = 0; i<mCharacteristics.size(); i++) {
                    GattCharacteristic characteristic = mCharacteristics.get(i);
                    if (characteristic.getUUID().compareTo(uuid) == 0)
                        gattCharList.add(characteristic);
                }
                return gattCharList;
            }
            return null;
        }

        /**
         * Get the first characteristic with the provided UUID
         *
         * @param uuid
         *            The UUID of the requested characteristic.
         * @return GattCharacteristic The first characteristic with the
         *         requested UUID.
         * @throws GattclientException
         *             on error, for example Bluetooth not connected.
         *
         */
        public GattCharacteristic getCharacteristic(UUID uuid)
                throws GattClientException {
            checkValid();

            if (mCharacteristics == null)
                discoverAllChars();

            if (mCharacteristics != null) {
                for (int i = 0; i<mCharacteristics.size(); i++) {
                    GattCharacteristic characteristic = mCharacteristics.get(i);
                    if (characteristic.getUUID().compareTo(uuid) == 0)
                        return characteristic;
                }
            }
            return null;
        }

        /**
         * Invalidate the Attribute cache
         *
         * @throws GattclientException
         *             on error, for example Bluetooth not connected.
         *
         */
        /* packave */private void invalidate() throws GattClientException {
            checkValid();
            mValid = false;

            Iterator<GattCharacteristic> itr= mCharacteristics.iterator();
            while (itr.hasNext()) {
                itr.next().invalidate();
            }
        }

        /**
         * Check if the attribute cache is valid or not.
         *
         * @return boolean True if the attrib cache is valid, else false.
         * @throws GattclientException
         *             on error, for example Bluetooth not connected.
         *
         */
        /* package */private boolean isValid() {
            return mValid;
        }

        /**
         * Get the GATT service UUID
         *
         * @return uuid of the service
         *
         */
        public UUID getUUID() {
            return mUuid;
        }
    }

    /**
     * Represents a GATT Characteristic.
     *
     * <p>
     * You can read/write Gatt Caharacteristic values using the provided
     * methods.
     *
     * <p class="note">
     * <strong>Note:</strong> This is just a perliminary API.
     *
     */
    public class GattCharacteristic {
        private static final String TAG = "GattCharacteristic";
        private UUID mUuid;
        private String mPath;
        private GattService mGattService;
        private BluetoothGattClient mGattClient;
        private String[] mProperties;
        private boolean mValid;
        private boolean mNotifying;
        private boolean mIndicating;
        private boolean mBroadcasting;
        private boolean mWritableWithRsp;
        private boolean mWritableWitoutRsp;
        private boolean mWritableSigned;
        private boolean mWritableReliable;
        private boolean mReadable;
        private GattCharCb mCb = null;

        /*
         * Utiltiy function to dump the current characteristic
         */
        public void dump() {
            try {
                logd("------------>8------------ GattCharacteristic dump start ------------>8------------");
                logd(mUuid.toString());
                logd(mPath);
                logd(mGattClient.getDevice().toString());
                String props = "";
                if (mNotifying)
                    props += " Notifying";
                if (mIndicating)
                    props += " Indicating";
                if (mBroadcasting)
                    props += " Broadcasting";
                if (mWritableWithRsp)
                    props += " write_req";
                if (mWritableWitoutRsp)
                    props += " write_cmd";
                if (mWritableSigned)
                    props += " write_auth";
                if (mWritableReliable)
                    props += " write_rel";
                if (mReadable)
                    props += " readable";
                if (props.equals(""))
                    logd("no properties");
                else
                    logd(props);

                logd("------------8<------------ GattCharacteristic dump end   ------------8<------------");
            } catch (Exception e) {
                Log.e(TAG,"dump exception "+e);
            }
        }

        /**
         * Constructor for the GattCharacteristic object
         *
         * @param client
         *            The parent Client object
         * @param service
         *            The contianning gatt service
         * @param charPath
         *            Characterristic path
         *
         */
        /* package */public GattCharacteristic(BluetoothGattClient client, GattService service,
                String charPath) throws GattClientException{

            checkDeviceConnected();

            mPath = charPath;
            mGattService = service;
            mGattClient = client;
            mValid = true;
            mNotifying = false;
            mIndicating = false;
            mBroadcasting = false;
            mWritableWithRsp = false;
            mWritableWitoutRsp = false;
            mWritableSigned = false;
            mWritableReliable = false;
            mReadable = false;

            try {
                mProperties = mService.getCharacteristicProperties(mDevice,mPath);
                if (mProperties == null) {
                    Log.e(TAG,"Could not get characteristic properties for "+mPath);
                    throw new GattClientException("Could not get characteristic properties for "+mPath);
                }
            } catch (Exception e) {
                Log.e(TAG,"getcharacteristicProperties failed:"+e);
            }

            parseProperties();
        }

        /**
         * Set a callback for Value Changed, Read response and write
         * response
         *
         */
        public void setCallback(GattCharCb cb) {
            mCb = cb;
        }

        /*
         * Used internally to set notify according to the properyChanged event
         */
        private void setNotifying(boolean notify) {
  logd("setNotifying "+notify);
            mNotifying = notify;
            if (mNotifying != notify) {
                if (mCb != null)
                    mCb.onPropertyChanged(this, "Notify",notify ? "true":"false");
            }
        }

        /*
         * Used internally to set Indicate according to the properyChanged event
         */
        private void setIndicating(boolean indicate) {
            logd("setIndicating "+indicate);
            if (mIndicating != indicate) {
                mIndicating = indicate;
                if (mCb != null)
                    mCb.onPropertyChanged(this, "Indicate",indicate ? "true":"false");
            }
        }

        /*
         * Used internally to set broadcast according to the properyChanged event
         */
        private void setBroadcasting(boolean broadcast) {
            logd("setBroadcasting "+broadcast);
            if (mBroadcasting != broadcast) {
                mBroadcasting = broadcast;
                if (mCb != null)
                    mCb.onPropertyChanged(this, "Broadcast",broadcast ? "true":"false");
            }
        }

        /*
         * Used internally to signal a value changed event
         */
        private void notifyValueChanged(byte[] value) {
            logd("value has changed !");
            if (mCb != null)
                mCb.onValueChanged(this, value);
        }

        /*
         * Used internally to signal a write complete event
         */
        private void notifyWriteComplete(byte status) {
            if (mCb != null)
                mCb.onWriteComplete(this, status);
        }

        /*
         * Used internally to signal a read complete event
         */
        private void notifyReadComplete(byte status, byte[] value) {
            if (mCb != null)
                mCb.onReadComplete(this, status, value);
        }

        /*
         * parse native properties.
         */
        private void parseProperties() {
            try {
                if (mProperties != null) {
                    for (int i = 0;i< mProperties.length;i++) {
                        logv("characteristic prop("+i+"):"+mProperties[i]);
                    }
                    for (int j = 0; j < mProperties.length; j++) {
                        logv("characteristic props:"+mProperties[j]);
                        if (mProperties[j].equals("Name")) {
                            j++;
                        } else if (mProperties[j].equals("Description")) {
                            j++;
                        } else if (mProperties[j].equals("Broadcast")) {
                            mBroadcasting = mProperties[++j].equals("true");
                        } else if (mProperties[j].equals("Indicate")) {
                            mIndicating = mProperties[++j].equals("true");
                        } else if (mProperties[j].equals("Notify")) {
                            mNotifying = mProperties[++j].equals("true");
                        } else if (mProperties[j].equals("Readable")) {
                            mReadable = mProperties[++j].equals("true");
                        } else if (mProperties[j].equals("WriteMethods")) {
                            int method_count = Integer.valueOf(mProperties[++j]);
                            for (int i = 0; i < method_count; i++) {
                                ++j;
                                if (mProperties[j].equals("Write"))
                                    mWritableWithRsp = true;
                                else if (mProperties[j].equals("WriteWithoutResponse"))
                                    mWritableWitoutRsp = true;
                                else if (mProperties[j].equals("AuthenticatedSignedWrite"))
                                    mWritableSigned = true;
                                else if (mProperties[j].equals("ReliableWrite"))
                                    mWritableReliable = true;
                            }
                        } else if (mProperties[j].equals("UUID")) {
                            mUuid = UUID.fromString(mProperties[++j]);
                        } else if (mProperties[j].equals("Value")) {
                            j++;
                        } else {
                            Log.e(TAG,"parseProperties Invalid property:\""+mProperties[j]+"\" ("+mPath+")");
                            break;
                        }
                    }
                } else {
                    Log.e(TAG,"mProperties = NULL !");
                }
            } catch (Exception e) {
                Log.e(TAG,"parseProperties exception:"+e);
            }
        }

        /*
         * Set up the parent service.
         */
        private void setParentService(GattService svc) {
            mGattService = svc;
        }

        /**
         * Read the characteristic value
         *
         * @return true if the read can be performed, false otherwise.
         *         A onReadComplete on the callback to signal when the
         *         read value is available.
         * @throws GattclientException
         *         on error, for example Bluetooth not connected.
         *
         */
        public boolean readValue() throws GattClientException {
            logd("readValue");
            checkDeviceConnected();

            try {
                return readCharacteristicValue(mPath,0);
            } catch (Exception e) {
                Log.e(TAG,"readCharacteristicValue exception:"+e);
                throw new GattClientException(e);
            }
        }

        /**
         * Write the characteristic value (without a response)
         *
         * @param value A byte array with the content of the characteristic
         *        value.
         * @throws GattclientException
         *             on error, for example Bluetooth not connected.
         *
         */
        public void writeValueNoRsp(byte[] value) throws GattClientException {
            logd("writeValueNoRsp");
            checkDeviceConnected();

            try {
                writeCharacteristicValueCmd(mPath,value);
            } catch (Exception e) {
                Log.e(TAG,"writeCharacteristicValueCmd exception:"+e);
                throw new GattClientException(e);
            }
        }

        /**
         * Write the characteristic value (with a response)
         *
         * @param value A byte array with the content of the characteristic
         *        value.
         * @return boolean The result of the write request.
         * @throws GattclientException
         *             on error, for example Bluetooth not connected.
         *
         */
        public boolean writeValueWaitRsp(byte[] value) throws GattClientException {
            logd("writeValueWaitRsp");
            checkDeviceConnected();

            try {
                writeCharacteristicValueReq(mPath,value);
            } catch (Exception e) {
                Log.e(TAG,"writeCharacteristicValueReq exception:"+e);
                throw new GattClientException(e);
            }
            return false;
        }

        /**
         * Enable characteristic indication.
         *
         * <p>
         * Enable characterstic indication, by setting the indication bit in the
         * characteristic client configurationd descriptor. If this descriptor
         * is not available, a GattClientException will be thrown.
         *
         * <p>
         * Indication response to the server is handled by the lower level.
         *
         * <p>
         * Indication will be sent to the parent service registered handler.
         *
         * @throws GattclientException
         *             on error, for example Bluetooth not connected.
         *
         */
        public void enableIndication(boolean enable) throws GattClientException {
            logd("enableIndication");
            checkDeviceConnected();

            try {
                logd("Trying to set characteristic indication");
                mService.setIndicating(mDevice, mPath,enable);
            } catch (Exception e) {
                Log.e(TAG,"mService.setIndicating exception "+e);
                throw new GattClientException(e);
            }
        }

        /**
         * Enable characteristic notification.
         *
         * <p>
         * Enable characterstic notification, by setting the notification bit in
         * the characteristic client configurationd descriptor.
         *
         * <p>
         * Notification will be sent to the parent service registered handler.
         *
         * @throws GattclientException
         *             on error, for example Bluetooth not connected.
         *
         */
        public void enableNotification(boolean enable) throws GattClientException {
            logd("enableNotification");
            checkDeviceConnected();

            try {
                logd("Trying to set characteristic notification");
                mService.setNotifying(mDevice, mPath,enable);
            } catch (Exception e) {
                Log.e(TAG,"mService.setNotifying exception "+e);
                throw new GattClientException(e);
            }
        }

        /**
         * Enable characteristic broadcasting.
         *
         * <p>
         * Enable characterstic broadcasting, by setting the broadcasting bit in
         * the characteristic server configurationd descriptor. If this
         * descriptor is not available, a GattClientException will be thrown.
         *
         * <p>
         * Broadcasting information is available by using the observer role.
         *
         * @throws GattclientException
         *             on error, for example Bluetooth not connected.
         *
         */
        public void enableBroadcasting(boolean enable) throws GattClientException {
            logd("enableBroadcasting");
            checkDeviceConnected();

            try {
                logd("Trying to set characteristic broadcasting");
                mService.setBroadcasting(mDevice, mPath,enable);
            } catch (Exception e) {
                Log.e(TAG,"mService.setBroadcasting exception "+e);
                throw new GattClientException(e);
            }
        }

        /**
         * Check if characteristic is configured for broadcasting.
         *
         * @return false if not configured, true if configured correctly.
         */
        public boolean isBroadcasting() throws GattClientException {
            checkDeviceConnected();
            return mBroadcasting;
        }

        /**
         * Check if characteristic is configured for Notification.
         *
         * @return false if not configured, true if configured correctly.
         */
        public boolean isNotifying() throws GattClientException {
            checkDeviceConnected();
            return mNotifying;
        }

        /**
         * Check if characteristic is configured for Indicating.
         *
         * @return false if not configured, true if configured correctly.
         */
        public boolean isIndicating() throws GattClientException {
            checkDeviceConnected();
            return mIndicating;
        }

        /**
         * Check if characteristic value can be written with write_req
         *
         * @return false if not possible, true if possible
         */
        public boolean canWriteWithRsp() throws GattClientException {
            checkDeviceConnected();
            return mWritableWithRsp;
        }

        /**
         * Check if characteristic value can be written with write_cmd
         *
         * @return false if not possible, true if possible
         */
        public boolean canWriteWithoutRsp() throws GattClientException {
            checkDeviceConnected();
            return mWritableWitoutRsp;
        }

        /**
         * Check if characteristic value can be written with write_signed
         *
         * @return false if not possible, true if possible
         */
        public boolean canWriteAuth() throws GattClientException {
            checkDeviceConnected();
            return mWritableSigned;
        }

        /**
         * Check if characteristic value can be written with write_reliable
         *
         * @return false if not possible, true if possible
         */
        public boolean canWriteReliable() throws GattClientException {
            checkDeviceConnected();
            return mWritableReliable;
        }

        /**
         * Get the characteristic UUID.
         *
         * @return UUID the characteristic UUID.
         * @throws GattclientException
         *             on error, for example Bluetooth not connected.
         *
         */
        public UUID getUUID() {
            return mUuid;
        }

        /**
         * Invalidate the Attribute cache
         *
         * @throws GattclientException
         *             on error, for example Bluetooth not connected.
         *
         */
        /* package */private void invalidate() {
            mValid = false;
        }
    }

    /** @hide */
    @Override
    protected void finalize() throws Throwable {
        try {
            disconnect();
        } finally {
            super.finalize();
        }
    }

    /**
     * Connect to the remote BluetoothDevice GATT Server.
     *
     * @throws GattclientException
     *             on error, for example Bluetooth not connected.
     *
     */
    public int connect() throws GattClientException {
        if (mService == null) {
            throw new GattClientException("BluetoothGattClientService not available");
        }
        if (mDevice == null) {
            throw new GattClientException("Invalid device");
        }
        try {
            logd("Trying to connect to "+mDevice.toString());
            int ret = mService.Connect(mDevice,this);
            logd("mService.Connect returned "+ret);

            if (ret > 0) {
                mId = ret;
                return 0;
            } else {
                return ret;
            }
        } catch (Exception e) {
            Log.e(TAG,"connect exception "+e);
            throw new GattClientException(e);
        }
    }

    /**
     * Disconenct the GATT Connection to the service.
     *
     * @throws GattclientException
     *             on error, for example Bluetooth not connected.
     *
     */
    public synchronized void disconnect() throws GattClientException {
        if (mDevice == null)
            throw new GattClientException("BluetoothGattClientService not available");

        try {
            mService.Disconnect(mDevice,mId);
        } catch (Exception e) {
            Log.e(TAG,"disconnect exception "+e);
            throw new GattClientException(e);
        }
    }

    /**
     * Get the remote device this GATT Client connecting, or connected, to.
     *
     * @return remote device
     */
    public BluetoothDevice getRemoteDevice() {
        return mDevice;
    }

    private void doPrimaryServicesDiscovery() {
        logv("Creating mPrimaries");
        mPrimaries = new ArrayList<GattService>();

        String[] services = null;
        logd("Trying to get services");
        try {
            services = mService.getServiceList(mDevice);
        } catch (Exception e) {
            Log.e(TAG,"getServiceList failed:"+e);
        }

        if (services != null) {
            for (int i = 0; i < services.length; i++) {
                logd("discoverPrimaryService checking " + services[i]);
                try {
                    GattService svc = new GattService(this,services[i]);
                    mPrimaries.add(svc);
                } catch (Exception e) {
                    Log.e(TAG,"GattService("+services[i]+") Exception:"+e);
                }
            }
        }
    }

    /**
     * Discover all primary services of a connected GATT Server
     *
     * @return GattService List of services available on the remote GATT Server
     * @throws GattclientException
     *             on error, for example Bluetooth not connected.
     *
     */
    public GattService discoverPrimaryService(UUID uuid)
            throws GattClientException {

        if (mService == null) {
            throw new GattClientException("BluetoothGattClientService not available");
        }
        if (mDevice == null) {
            throw new GattClientException("Invalid device");
        }

        checkDeviceConnected();

        if (mPrimaries == null)
            doPrimaryServicesDiscovery();

        if (mPrimaries.size() > 0) {
            for (int i = 0;i < mPrimaries.size(); i++) {
                GattService svc = mPrimaries.get(i);
                if (svc != null && svc.getUUID().equals(uuid))
                    return svc;
            }
        }

        return null;
    }

    /**
     * Discover all primary services of a connected GATT Server
     *
     * @return ArrayList<GattService> List of services available on the remote
     *         GATT Server
     * @throws GattclientException
     *             on error, for example Bluetooth not connected.
     *
     */
    public ArrayList<GattService> discoverAllPrimaryServices()
            throws GattClientException {

        if (mService == null) {
            throw new GattClientException("BluetoothGattClientService not available");
        }
        if (mDevice == null) {
            throw new GattClientException("Invalid device");
        }

        checkDeviceConnected();

        if (mPrimaries == null)
            doPrimaryServicesDiscovery();

        return mPrimaries;
    }

    /**
     * Discover all characteristics, between the handle provided
     *
     * @param servicePath
     *            The service which holds the characteristics.
     * @return ArrayList<GattCaharacteristic> List of characteristics available
     *         on the remote GATT Server
     * @throws GattclientException
     *             on error, for example Bluetooth not connected.
     *
     */
    /* package */private ArrayList<GattCharacteristic> discoverAllCharacteristics(String svc)
            throws GattClientException {

        if (mService == null) {
            throw new GattClientException("BluetoothGattClientService not available");
        }
        if (mDevice == null) {
            throw new GattClientException("Invalid device");
        }

        checkDeviceConnected();

        ArrayList<GattCharacteristic> chars = new ArrayList<GattCharacteristic>();
        String[] services = null;
        String[] charslist = null;
        GattCharacteristic characteristic = null;
        try {
            logd("Trying to get characteristics");
            charslist = mService.getCharacteristicList(mDevice,svc);
        } catch (Exception e) {
            Log.e(TAG,"getCharacteristicList exception:"+e);
        }

        if (charslist != null) {
            for (int i = 0; i < charslist.length; i++) {
                logd("char:"+charslist[i]);
                try {
                    characteristic = getChar(charslist[i]);
                    if (characteristic == null) {
                        logd("char is new. Allocating new object");
                        characteristic = new GattCharacteristic(this,null,charslist[i]);
                        addChar(charslist[i],characteristic);
                    } else {
                        logd("char is already cached. Using cached");
                    }
                } catch (Exception e) {
                    Log.e(TAG,"GattCharacteristic exception "+e);
                }

                if (characteristic != null)
                    chars.add(characteristic);
            }
        }

        return chars;
    }

    /**
     * Read a characteristic value
     *
     * @param characteristicPath
     *            path of the characteristic
     * @return byte[] A byte array containing the characteristic value.
     * @throws GattclientException
     *             on error, for example Bluetooth not connected.
     *
     */
    /* package */private boolean readCharacteristicValue(String characteristicPath,int offset)
            throws GattClientException {
        if (mService == null) {
            throw new GattClientException("BluetoothGattClientService not available");
        }
        if (mDevice == null) {
            throw new GattClientException("Invalid device");
        }

        checkDeviceConnected();

        try {
            logd("Trying to read characteristic value");
            mService.Read(mDevice, characteristicPath,offset);
            return true;
        } catch (Exception e) {
            Log.e(TAG,"mService.Read exception "+e);
            throw new GattClientException(e);
        }
    }

    /**
     * Write the characteristic value (with a response)
     *
     * @param characteristicPath
     *            The characteristic path
     * @param byte[] A byte array with the content of the characteristic value.
     * @return boolean The result of the write request.
     * @throws GattclientException
     *             on error, for example Bluetooth not connected.
     *
     */
    /* package */private boolean writeCharacteristicValueReq(String characteristicPath,
            byte[] value) throws GattClientException {
        if (mService == null) {
            throw new GattClientException("BluetoothGattClientService not available");
        }
        if (mDevice == null) {
            throw new GattClientException("Invalid device");
        }

        checkDeviceConnected();

        try {
            logd("Trying to write characteristic value");
            mService.Write(mDevice, characteristicPath,value, true);
            return true;
        } catch (Exception e) {
            Log.e(TAG,"mService.Write exception "+e);
            throw new GattClientException(e);
        }
    }

    /**
     * Write the characteristic value (without a response)
     *
     * @param characteristicPath
     *            The characteristic path
     * @param byte[] A byte array with the content of the characteristic value.
     * @throws GattclientException
     *             on error, for example Bluetooth not connected.
     *
     */
    /* package */private void writeCharacteristicValueCmd(String characteristicPath,
            byte[] value) throws GattClientException {
        if (mService == null) {
            throw new GattClientException("BluetoothGattClientService not available");
        }
        if (mDevice == null) {
            throw new GattClientException("Invalid device");
        }

        checkDeviceConnected();

        try {
            logd("Trying to write characteristic value");
            mService.Write(mDevice, characteristicPath,value, false);
        } catch (Exception e) {
            Log.e(TAG,"mService.Write exception "+e);
            throw new GattClientException(e);
        }
    }

    /* Private utilities */
    private static void logd(String msg) {
        if(DBG) Log.d(TAG, msg);
    }

    private static void logv(String msg) {
        if(VERBOSE) Log.i(TAG, msg);
    }

    private synchronized void checkDeviceConnected() throws GattClientException {
        if (mService == null)
            throw new GattClientException("Could not connect to Gatt Client Service");

        try {
            if (mService.isConnected(mDevice) == false)
                throw new GattClientException("device "+mDevice.toString()+" is not connected");
        } catch (Exception e) {
            throw new GattClientException("checkDeviceConnected() exception:"+e);
        }
    }
}
