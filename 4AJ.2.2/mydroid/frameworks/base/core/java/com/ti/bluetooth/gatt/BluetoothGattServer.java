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

import java.nio.ByteBuffer;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.Log;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;

import android.os.ServiceManager;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;

import com.ti.bluetooth.gatt.GattReadResult;
import com.ti.bluetooth.gatt.GattStatus;
import com.ti.bluetooth.gatt.IBluetoothGattServer;
import com.ti.bluetooth.gatt.IBluetoothGattServerCallback;
import android.server.BluetoothGattServerService;

public class BluetoothGattServer {
    private static final String TAG = "BluetoothGattServer";
    private static final boolean DBG = true;

    /**
     * An intent with this action returns the Tx power level of a remote device.
     *
     * This intent is broadcast automatically after the connection of an LE device.
     * The remote device is available via
     * {@link BluetoothGattServer#TX_POWER_EXTRA_DEVICE} and the Tx power level
     * is available via {@link BluetoothGattServer#TX_POWER_EXTRA_LEVEL}.
     */
    public static final String TX_POWER_READ_ACTION =
                            "com.ti.bluetooth.gatt.txpower.action.TX_POWER_READ";
    /**
     * Contains a BluetoothDevice as a parcelable extra field.
     *
     * Used by {@link BluetoothGattServer#TX_POWER_READ_ACTION}.
     */
    public static final String TX_POWER_EXTRA_DEVICE =
                            "com.ti.bluetooth.gatt.txpower.extra.DEVICE";

    /**
     * Contains an integer as an extra field.
     *
     * Used by {@link BluetoothGattServer#TX_POWER_READ_ACTION}.
     */
    public static final String TX_POWER_EXTRA_LEVEL =
                            "com.ti.bluetooth.gatt.txpower.extra.LEVEL";
    // Unique profile id given at runtime by the low-level service
    private int mProfileId;

    // the current service index. incremented on each added GattService
    private int mCurrentServiceIdx = 0;

    private IBluetoothGattServer mService;

    // map binding handles to attribute callbacks
    private HashMap<Integer, GattAttributeCallback> mProfileCallbacks;

    private boolean mProfileInitialized = false;

    // map binding indication cookies to indication callbacks
    private HashMap<Integer, GattIndicationStatusCallback> mIndCookies;

    private Context mContext;

    // the start/stop callbacks in actual profile code
    private BluetoothGattServerCallback mCb;

    private String mName;

    /**
     * @param uniqueName
     * The uniqueName string must be unique throughout the system. It
     * is advised to use a combination of the package name and a unique name
     * for the profile (e.g. "com.ti.bluetooth.gatt.proximity")
     * @param context
     * @param cb
     */
    public BluetoothGattServer(String uniqueName, Context context, BluetoothGattServerCallback cb) {
        mContext = context;
        mCb = cb;
        mName = uniqueName;

        if (cb == null)
            throw new RuntimeException("GATT server callback must be implemented");

        IBinder b = ServiceManager.getService(BluetoothGattServerService.BLUETOOTH_GATTSERVER_SERVICE);
        if (b == null)
            throw new RuntimeException("Bluetooth GATT Server Service not available");

        mService = IBluetoothGattServer.Stub.asInterface(b);

        try {
            mService.addDynamicProfile(mName, mCallbackStub);
        } catch (RemoteException e) {
            e.printStackTrace();
            throw new RuntimeException("could not register profile with GATT server");
        }
    }


    /**
     * Remove this GATT server. The class can not be used after this function
     *is called and functions may throw exceptions.
     */
    public void destroy() {
        if (mService == null)
            return;

        try {
            mService.removeDynamicProfile(mName);
        } catch (RemoteException e) { e.printStackTrace(); }

        mService = null;
    }

    @Override
    protected void finalize() throws Throwable {
        try {
            destroy();
        } finally {
            super.finalize();
        }
    }

    IBluetoothGattServerCallback.Stub mCallbackStub = new IBluetoothGattServerCallback.Stub() {
        public void start(int profileId, IBinder srv) {
            mProfileId = profileId;
            mProfileCallbacks = new HashMap<Integer, GattAttributeCallback>();
            mIndCookies = new HashMap<Integer, GattIndicationStatusCallback>();

            if (DBG) Log.d(TAG, "start profileId " + mProfileId);

            // This function registers all services inside the profile
            mCb.registerProfile();

            // tell the low level service we finished init
            try {
                mService.endProfileInit(mProfileId);
            } catch (RemoteException e) {
                e.printStackTrace();
            }

            mProfileInitialized = true;
        }

        public void stop() {
            mCb.unregisterProfile();
            mProfileCallbacks = null;
            mIndCookies = null;
            mProfileInitialized = false;
        }

        public GattReadResult readCallback(int handle, BluetoothDevice device) {
            GattAttributeCallback cb = mProfileCallbacks.get(handle);
            if (cb == null) {
                Log.e(TAG, "no read CB for handle " + handle);
                return new GattReadResult(GattStatus.ERR_IO);
            }

            try {
                return cb.onRead(device, new GattAttribute(handle));
            } catch (Exception e) {
                e.printStackTrace();
                return new GattReadResult(GattStatus.ERR_IO);
            } catch (Throwable t) {
                t.printStackTrace();
                return new GattReadResult(GattStatus.ERR_IO);
            }
        }

        public int writeCallback(int handle, BluetoothDevice device, byte[] value) {
            GattAttributeCallback cb = mProfileCallbacks.get(handle);
            if (cb == null) {
                Log.e(TAG, "no write CB for handle " + handle);
                return GattStatus.ERR_IO.getValue();
            }

            GattStatus status = GattStatus.ERR_IO;
            try {
                status = cb.onWrite(device, new GattAttribute(handle), value);
            } catch (Exception e) {
                e.printStackTrace();
                return GattStatus.ERR_IO.getValue();
            } catch (Throwable t) {
                t.printStackTrace();
                return GattStatus.ERR_IO.getValue();
            }

            return status.getValue();
        }

        public void indicateResult(int cookie, int status) {
            GattIndicationStatusCallback cb = null;
            synchronized(mIndCookies) {
                cb = mIndCookies.remove(cookie);
                if (cb == null)
                    return;
            }

            try {
                cb.callStatus(GattStatus.getEnumValue(status));
            } catch (Exception e) {
                e.printStackTrace();
            } catch (Throwable t) {
                t.printStackTrace();
            }
        }
    };

    private static class RecordedAttribute {
        byte[] mUuid;
        int mReadReqs;
        int mWriteReqs;
        byte[] mValue;
        GattAttribute mAttrib;
        GattAttributeCallback mCb;

        /*
         * parameters given in read/write_reqs to the low level service as part
         * of an attribute definition.
         */
        private static final int ATT_NO_AUTH_REQ = 0;
        private static final int ATT_AUTHENTICATION = 1;
        private static final int ATT_AUTORIZATION = 2;
        private static final int ATT_NOT_PERMITTED = 3;

        /**
         * @param permissions
         *            attribute permissions
         * @param authPermissions
         *            attribute permissions requiring authentication
         * @return a readReqs element fit for the low level server
         */
        private int getReadReqs(byte permissions, byte authPermissions) {
            int readReqs = ATT_NOT_PERMITTED;

            if ((authPermissions & permissions & GATT_READ_PERM) != 0)
                readReqs = ATT_AUTHENTICATION;
            else if ((permissions & GATT_READ_PERM) != 0)
                readReqs = ATT_NO_AUTH_REQ;

            return readReqs;
        }

        /**
         * @param permissions
         *            attribute permissions
         * @param authPermissions
         *            attribute permissions requiring authentication
         * @return a writeReqs element fit for the low level server
         */
        private int getWriteReqs(byte permissions, byte authPermissions) {
            int writeReqs = ATT_NOT_PERMITTED;
            byte writePerms = GATT_WRITE_NO_RESP_PERM | GATT_WRITE_PERM;

            if ((authPermissions & permissions & writePerms) != 0)
                writeReqs = ATT_AUTHENTICATION;
            else if ((permissions & writePerms) != 0)
                writeReqs = ATT_NO_AUTH_REQ;

            return writeReqs;
        }

        /**
         * InitialValue may be null here. When null, it means the low level code
         * will register a read/write callback for this attribute.
         * The attrib argument can also be null, if the attribute handle
         * value is not required. The cb argument can be null as well
         */
        RecordedAttribute(GattAttribute attrib, byte[] uuid, byte permissions,
                byte authPermissions, byte[] initialValue, GattAttributeCallback cb) {
            mAttrib = attrib;
            mUuid = uuid;
            mValue = initialValue;
            mReadReqs = getReadReqs(permissions, authPermissions);
            mWriteReqs = getWriteReqs(permissions, authPermissions);
            mCb = cb;
        }
    };

    // /////////////////////////////////////////////////////////////////////////
    // User visible part
    // /////////////////////////////////////////////////////////////////////////

    /**
     * GATT service types.
     *
     * @see GattService
     */
    public static enum GattServiceType {
        GATT_PRIMARY_SVC_UUID(0x2800), GATT_SECONDARY_SVC_UUID(0x2801);

        int mValue;

        GattServiceType(int value) {
            mValue = value;
        }

        int getValue() {
            return mValue;
        }
    };

    // GATT permissions
    public static final byte GATT_READ_PERM = 0x02;
    public static final byte GATT_WRITE_NO_RESP_PERM = 0x04;
    public static final byte GATT_WRITE_PERM = 0x08;
    public static final byte GATT_NOTIFY_PERM = 0x10;
    public static final byte GATT_INDICATE_PERM = 0x20;

    /**
     * Class for representing a UUID value in GATT.
     *
     */
    public static class GattUuid {
        byte[] mValue;

        /**
         * @param value value we should convert to GattUuid
         * @throws IllegalArgumentException
         */
        public GattUuid(int value) throws IllegalArgumentException {
            if (value < 0 || value > 0xffff)
                throw new IllegalArgumentException("invalid uuid");

            mValue = new byte[] { (byte)value, (byte)(value >>> 8) };
        }

        /**
         * @param value
         * The value of the UUID in bytes. Note that GATT only
         * supports 16bit or 128bit UUIDs, so the length of the array
         * can be 2 or 16 bytes, accordingly. An exception will be
         * thrown otherwise.
         * @throws IllegalArgumentException
         */
        public GattUuid(byte[] value) throws IllegalArgumentException {
            if (value.length != 2 && value.length != 16)
                throw new IllegalArgumentException("invalid uuid length");

            mValue = value;
        }

        byte[] getValue() {
            return mValue;
        }

        boolean is16bit() {
            return mValue.length == 2;
        }

        /**
         * Compare two GATT UUIDs
         */
        @Override
        public boolean equals(Object o) {
            // Return true if the objects are identical.
            // (This is just an optimization, not required for correctness.)
            if (this == o)
                return true;

            // Return false if the other object has the wrong type.
            if (!(o instanceof GattUuid))
                return false;

            // Cast to the appropriate type.
            // This will succeed because of the instanceof, and lets us access
            // private fields.
            GattUuid lhs = (GattUuid)o;

            return Arrays.equals(mValue, lhs.mValue);
        }
    };

    /**
     * Class representing a single GATT attribute.
     *
     * Indications/notifications using a stand-alone GATT attribute are not
     * supported. To get this functionality, use {@link GattService#addChar}.
     */
    public static class GattAttribute {
        private int mHandle;

        GattAttribute() {
            mHandle = 0;
        }

        GattAttribute(int handle) {
            mHandle = handle;
        }

        /**
         * Low level function to get handle. Do not use unless you
         * specifically need the handle value of an attribute (for instance
         * to put as the value of another attribute).
         * If gotten during profile registration, it is only valid after
         * the current service has been registered via {@link GattService#register}.
         * @throws RuntimeException
         */
        public int getHandle() throws RuntimeException {
            if (mHandle == 0)
                throw new RuntimeException("invalid handle - not initialized");

            return mHandle;
        }

        /**
         * Compare between the contents of GATT attributes.
         *
         * Usually used inside read/write callbacks that are registered on
         * multiple attributes.
         */
        @Override
        public boolean equals(Object o) {
            // Return true if the objects are identical.
            // (This is just an optimization, not required for correctness.)
            if (this == o)
                return true;

            // Return false if the other object has the wrong type.
            if (!(o instanceof GattAttribute))
                return false;

            // Cast to the appropriate type.
            // This will succeed because of the instanceof, and lets us access
            // private fields.
            GattAttribute lhs = (GattAttribute)o;

            return mHandle == lhs.mHandle;
        }

        @Override
        public String toString() {
            return Integer.toString(mHandle);
        }
    }

    /**
     * Interface representing a profile-defined GATT attribute callback. The
     * functions are called when a remote device performs a read or write
     * operation on the attribute attached the this callback.
     *
     * Warning: The code running inside these callbacks is synchronized with
     * the Bluetooth subsystem. Therefore avoid issuing calls to blocking
     * BT APIs from callback code (e.g. {@link BluetoothAdapter#disable}). This
     * bug can also be hidden - a synchronization on a lock that's held by
     * other code making blocking BT calls.
     *
     * Also make sure you return as quickly as possible, as prolonged
     * operations can hurt user experience. The rule of thumb should be to
     * offload any lengthy non-essential operations.
     */
    public static interface GattAttributeCallback {
        /**
         * Callback for remote read requests. It is ok to return a zero-length
         * array in the result. This is equivalent to sending a zero-length
         * reply to the remote device.
         *
         * @return a class containing the result status and value read
         */
        GattReadResult onRead(BluetoothDevice device, GattAttribute attrib);

        /**
         * Callback for remote write requests. Note the length of the remote
         * array can be zero, or otherwise unexpected. Make sure to check the
         * length before traversing.
         *
         * @return result status for write operation
         */
        GattStatus onWrite(BluetoothDevice device, GattAttribute attrib,
                        byte[] value);
    }

    /**
     * Interface for the profile to receive indication confirmations from a
     * remote peer. The callback will be called only after it is given as an
     * argument to {@link GattChar#sendIndication}, and only when that call
     * succeeds (returns true).
     */
    public static interface GattIndicationStatusCallback {
        /**
         * @param status indication status. will be SUCCESS if all went well.
         */
        void callStatus(GattStatus status);
    }

    /**
     * Main class for creating a GATT service.
     *
     * Instantiating this class is the first step in creating a GATT service.
     * Characteristics and included services should be added, and then the
     * service should be registered using {@link GattService#register}.
     */
    public static class GattService {
        // GATT characteristic related UUIDs
        private static final int GATT_CHARAC_UUID = 0x2803;
        private static final int GATT_CLIENT_CHARAC_CFG_UUID = 0x2902;
        private static final int GATT_INCLUDED_SVC_UUID = 0x2902;

        // Auxiliary definition for char attribute type
        private final byte[] GATT_ARRAY_CHARAC_UUID = new byte[] {0x03, 0x28};

        private BluetoothGattServer mServer;

        private boolean mRegistered = false;

        // a unique index within the profile for this service. assigned
        // internally on creation
        private int mServiceIdx;

        private GattUuid mUuid;
        private String mName;
        private boolean mRegisterSdp;

        // offset to actual handle
        private int mHandleOffset;

        private ArrayList<RecordedAttribute> mAttributes =
                new ArrayList<RecordedAttribute>();

        /**
         * @param svcType
         * @param svcUuid The UUID of the service. Should be defined in the
         * bluetooth specification
         * @param registerSdp Should we register an SDP record for this service.
         * This should only be enabled if BR/EDR support is desired (and
         * supported by the profile specification).
         * @param name The name of the service in SDP
         * (e.g. "Phone Alert Status Profile"). If registerSdp is false, this
         * name is not used.
         */
        public GattService(BluetoothGattServer srv, GattServiceType svcType,
                        GattUuid svcUuid, boolean registerSdp, String name) {
            mServer = srv;
            mServiceIdx = mServer.mCurrentServiceIdx++;
            mHandleOffset = 0;
            mUuid = svcUuid;
            mName = name;
            mRegisterSdp = registerSdp;

            if (DBG) Log.d(TAG, "new GattService start");

            mAttributes.add(0, new RecordedAttribute(null,
                    new GattUuid(svcType.getValue()).getValue(),
                    GATT_READ_PERM, (byte) 0, svcUuid.getValue(), null));
        }

        /**
         * Complete the registration of a GATT service.
         *
         * This must be called after defining all characteristics and attributes
         * in a server.
         *
         * After the registration is completed, no new attributes or
         * characteristics can be added to the service. However from this
         * point on, the service can be added as an included service
         * (via {@link GattService#addIncludedService}).
         *
         * Handle values for the various attributes and characteristics in this
         * service (gotten with {@link GattAttribute#getHandle}) are only
         * valid after the service is registered.
         * @throws RuntimeException
         */
        synchronized public void register() throws RuntimeException {
            if (mRegistered)
                throw new RuntimeException(
                        "cannot change service after registration");

            mHandleOffset = 0;
            try {
                mHandleOffset = mServer.mService.addService(mServer.mProfileId,
                                    mServiceIdx, mUuid.getValue(),
                                    mAttributes.size()); // number of handles
            } catch (RemoteException e) { e.printStackTrace(); }

            playAttributes();

            if (mRegisterSdp) {
                try {
                    mServer.mService.registerSdpRecord(mServer.mProfileId, mServiceIdx,
                                    mHandleOffset, // the service handle
                                    mName != null ? mName : "");
                } catch (RemoteException e) { e.printStackTrace(); }
            }

            mRegistered = true;
        }

        /**
         * Add an included service to this service. Note the service being
         * included must already be registered via {@link GattService#register}.
         *
         * @param svc The service we wish to include
         * @throws IllegalArgumentException
         * @throws RuntimeException
         */
        synchronized public GattAttribute addIncludedService(GattService svc)
                throws IllegalArgumentException, RuntimeException {
            if (!svc.mRegistered)
                throw new IllegalArgumentException(
                        "included service must be registered");

            if (mRegistered)
                throw new RuntimeException(
                        "cannot change service after registration");

            int bufSize = 4 + (svc.mUuid.is16bit() ? 2 : 0);
            ByteBuffer incValue = ByteBuffer.allocate(bufSize);

            GattUuid beginUuid = new GattUuid(svc.mHandleOffset);
            GattUuid endUuid =
                    new GattUuid(svc.mHandleOffset + svc.mAttributes.size() - 1);
            incValue.put(beginUuid.getValue());
            incValue.put(endUuid.getValue());

            if (svc.mUuid.is16bit())
                incValue.put(svc.mUuid.getValue());

            GattAttribute attrib = new GattAttribute();

            // included services are just after the service definition,
            // before any other attributes - put in index 1 of the array
            mAttributes.add(1, new RecordedAttribute(attrib,
                    new GattUuid(GATT_INCLUDED_SVC_UUID).getValue(),
                    GATT_READ_PERM, (byte) 0, incValue.array(), null));

            return attrib;
        }

        private void checkNewAttribute(GattUuid uuid, byte permissions,
                byte authPermissions, GattAttributeCallback cb, byte[] value)
                        throws IllegalArgumentException{
            // bluez won't act nicely with defining a new CCC
            if (uuid.equals(new GattUuid(GATT_CLIENT_CHARAC_CFG_UUID)))
                throw new IllegalArgumentException(
                        "defining a new CCC is not supported");

            if (mRegistered)
                throw new IllegalArgumentException(
                        "cannot change service after registration");

            // authPermissions can't contain stuff not in permissions
            if ((authPermissions & ~permissions) != 0)
                throw new IllegalArgumentException("authPermissions can't "
                        + "contain bits not in permissions");

            byte rw_perm = GATT_READ_PERM | GATT_WRITE_PERM
                    | GATT_WRITE_NO_RESP_PERM;

            // a callback must be defined if read/write is requested
            if (value == null && cb == null && (permissions & rw_perm) != 0)
                throw new IllegalArgumentException("a callback/value must be "
                        + " provided if read/write permissions are required");
        }

        /**
         * Add a GATT characteristic.
         *
         * A client-configuration-characteristic (CCC) record will automatically
         * be created for this characteristic if GATT_NOTIFY_PERM or
         * GATT_INDICATE_PERM are specified in permissions. Note: The profile
         * cannot rely on constant attribute handle values. For instance it is
         * forbidden to record attribute handles in persistent storage and use
         * them afterwards. Instead, the profile must use the return value from
         * this function.
         *
         * @param uuid
         * the UUID of the new characteristic. Should be defined in
         * the Bluetooth spec.
         * @param permissions
         * what operations are allowed for this characteristic -
         * read/write/notify etc.
         * @param authPermissions
         * a subset of the characteristic's permissions that require
         * authentication
         * @param cb
         * a read/write callback. May be null if not required. The
         * callback will not be called unless appropriate permissions
         * are present. Note that a callback must be supplied if
         * read/write permissions are allowed.
         * @return a new GATT characteristic
         * @throws IllegalArgumentException
         */
        synchronized public GattChar addChar(GattUuid uuid, int permissions,
                int authPermissions, GattAttributeCallback cb)
                        throws IllegalArgumentException {
            if (DBG) Log.d(TAG, "addChar");

            checkNewAttribute(uuid, (byte)permissions, (byte)authPermissions,
                    cb, null);

            // char definition
            ByteBuffer charDefValue = ByteBuffer
                    .allocate(3 + uuid.getValue().length);
            charDefValue.put((byte)permissions);
            // char value - will be filled out in playAttributes()
            charDefValue.put(new byte[] {0,0});
            charDefValue.put(uuid.getValue());

            mAttributes.add(new RecordedAttribute(null,
                    new GattUuid(GATT_CHARAC_UUID).getValue(), GATT_READ_PERM,
                    (byte) 0, charDefValue.array(), null));

            GattAttribute valueAttrib = new GattAttribute();
            GattAttribute cccAttrib = null; // start out with a null CCC

            // char value - note we have no value since a cb is registered
            mAttributes.add(new RecordedAttribute(valueAttrib,
                    uuid.getValue(), (byte)permissions, (byte)authPermissions,
                    null, cb));

            // automatically create the CCC if notify/indicate permissions exist
            if ((permissions & (GATT_NOTIFY_PERM | GATT_INDICATE_PERM)) != 0) {
                // no authentication for write. bluez disregards the initial
                // value we give. it uses per-device values.
                cccAttrib = new GattAttribute();
                mAttributes.add(new RecordedAttribute(cccAttrib,
                        new GattUuid(GATT_CLIENT_CHARAC_CFG_UUID).getValue(),
                        (byte)(GATT_READ_PERM | GATT_WRITE_PERM), (byte)0,
                        new byte[] { 0, 0 }, null));
            }

            return new GattChar(mServer, valueAttrib, cccAttrib);
        }

        /**
         * Add static GATT attribute (low level API).
         *
         * This should typically be used to define extra characteristic
         * descriptor declarations - such as Extended Properties or User
         * Description.
         * Do not use this function unless you know what you're doing. This is
         * advanced low-level API to the attribute DB. Usually using
         * {@link #addChar} should be enough for implementing profiles. Do not
         * create CCC handles with this function. It is not supported and an
         * exception will be thrown.
         * This function allows the definition of an attribute whose value
         * does not change (usually read-only). To get read/write callbacks
         * use {@link GattService#addDynamicAttribute}.
         * @throws IllegalArgumentException
         */
        synchronized public GattAttribute addStaticAttribute(GattUuid uuid,
                int permissions, int authPermissions, byte[] value)
                        throws IllegalArgumentException {
            checkNewAttribute(uuid, (byte)permissions, (byte)authPermissions,
                    null, value);

            if (DBG) Log.d(TAG, "addStaticAttribute");

            GattAttribute attrib = new GattAttribute();
            mAttributes.add(new RecordedAttribute(attrib, uuid.getValue(),
                    (byte)permissions, (byte)authPermissions, value, null));

            return attrib;
        }

        /**
         * Add dynamic GATT attribute (low level API).
         *
         * This should typically be used to define extra characteristic
         * descriptor declarations - such as Extended Properties or User
         * Description.
         * Do not use this function unless you know what you're doing. This is
         * advanced low-level API to the attribute DB. Usually using
         * {@link #addChar} should be enough for implementing profiles. Do not
         * create CCC handles with this function. It is not supported and an
         * exception will be thrown.
         * This function allows the definition of an attribute whose value can
         * change via read/write callbacks. To set a static attribute
         * use {@link GattService#addStaticAttribute}.
         * @throws IllegalArgumentException
         */
        synchronized public GattAttribute addDynamicAttribute(GattUuid uuid,
                int permissions, int authPermissions, GattAttributeCallback cb)
                    throws IllegalArgumentException {
            checkNewAttribute(uuid, (byte)permissions, (byte)authPermissions,
                    cb, null);

            if (DBG) Log.d(TAG, "addDynamicAttribute");
            GattAttribute attrib = new GattAttribute();
            mAttributes.add(new RecordedAttribute(attrib, uuid.getValue(),
                    (byte)permissions, (byte)authPermissions, null, cb));

            return attrib;
        }

        // send attributes to lower service
        private void playAttributes() {
            if (DBG) Log.d(TAG, "play attribues");
            int handleValue = mHandleOffset;
            for (RecordedAttribute r : mAttributes) {
                // We allow null values but AIDL does not. Put a zero byte
                // whenever this happens
                byte[] value = r.mValue;
                if (value == null)
                    value = new byte[] { 0 };

                // incorporate the real value attribute inside the
                // characteristic definitions. we assume the next
                // characteristic is the correct one.
                if (Arrays.equals(r.mUuid, GATT_ARRAY_CHARAC_UUID) &&
                        r.mValue != null && r.mValue.length >= 3) {
                    byte[] realValue = new GattUuid(handleValue + 1).getValue();
                    r.mValue[1] = realValue[0];
                    r.mValue[2] = realValue[1];
                }

                // put the read/write cb on the handle
                if (r.mCb != null) {
                    if (DBG) Log.d(TAG, "add write cb to handle " + handleValue);
                    mServer.mProfileCallbacks.put(handleValue, r.mCb);
                }

                try {
                    mServer.mService.addAttribute(mServer.mProfileId, mServiceIdx, handleValue,
                        r.mUuid, r.mReadReqs, r.mWriteReqs, value, r.mCb != null);
                } catch (RemoteException e) { e.printStackTrace(); }

                if (r.mAttrib != null)
                    r.mAttrib.mHandle = handleValue;

                handleValue++;
            }
        }

    }

    /**
     * Class representing a GATT characteristic.
     *
     */
    public static class GattChar {
        private static final String BROADCAST_MAC_ADDR = "ff:ff:ff:ff:ff:ff";

        /* we can't send more than MIN_ATT_MTU (23) for the whole packet */
        private static final int MAX_IND_NOT_DATA_LEN = 20;

        private BluetoothGattServer mServer;

        // handle to the value attribute
        private GattAttribute mValueAttrib;

        // handle to the CCC (client configuration char). this can be null.
        private GattAttribute mCccAttrib;

        // the CCC will be 0 when not registered
        GattChar(BluetoothGattServer srv, GattAttribute handle, GattAttribute ccc) {
            mServer = srv;
            mValueAttrib = handle;
            mCccAttrib = ccc;
        }

        /**
         * Compare two GATT characteristics
         */
        @Override
        public boolean equals(Object o) {
            // Return true if the objects are identical.
            // (This is just an optimization, not required for correctness.)
            if (this == o)
                return true;

            // Return false if the other object has the wrong type.
            if (!(o instanceof GattChar))
                return false;

            // Cast to the appropriate type.
            // This will succeed because of the instanceof, and lets us access
            // private fields.
            GattChar lhs = (GattChar)o;

            return mValueAttrib.equals(lhs.mValueAttrib) &&
                    mCccAttrib.equals(lhs.mCccAttrib);
        }

        /**
         * Attribute representing the value handle for the characteristic.
         *
         * This will be the attribute in read/write callbacks registered on
         * this characteristic.
         */
        public GattAttribute getValueAttribute() {
            return mValueAttrib;
        }

        boolean checkBeforeTransmit(byte[] value) {
            if (value == null) {
                Log.e(TAG, "cannot send notification/indication with null value");
                return false;
            }

            if (value.length > MAX_IND_NOT_DATA_LEN) {
                Log.e(TAG, "ind/notif value too long");
                return false;
            }

            if (mCccAttrib == null) {
                Log.e(TAG, "no permissions to send notification/indication");
                return false;
            }

            if (!mServer.mProfileInitialized) {
                Log.e(TAG, "cannot notify/indicate before profile is registered");
                return false;
            }

            return true;
        }

        /**
         * Notify an attribute value to a remote device (or devices).
         *
         * Note that if the remote device being notified has not registered
         * itself for notification on the characteristic CCC handle, the
         * notification will not be sent. This is on par with the ATT
         * specification.
         *
         * @param device device we wish to notify. This parameter can be
         * null in case a broadcast to all devices is desired.
         * @param value value we wish to notify. This parameter is
         * mandatory and cannot be null.  Note the value length cannot be more
         * than 20 bytes - a limitation of ATT.
         * @return false when we failed sending the notification because
         * of internal reasons - invalid parameters or profile not initialized
         * yet.
         */
        public boolean sendNotification(BluetoothDevice device, byte[] value) {
            if (!checkBeforeTransmit(value))
                return false;

            String address = BROADCAST_MAC_ADDR;
            if (device != null)
                address = device.getAddress();

            if (DBG)
                Log.d(TAG, "sendNotification to " + address +
                        " length " + value.length);
            try {
                mServer.mService.sendNotification(mServer.mProfileId, address,
                        mValueAttrib.getHandle(), mCccAttrib.getHandle(),
                        value);
            } catch (RemoteException e) {
                e.printStackTrace();
                return false;
            }

            return true;
        }

        /**
         * Indicate an attribute value to a remote device.
         *
         * Note that if the remote device being indicated has not registered
         * itself for indication on the characteristic CCC handle, the
         * indication will not be sent. This is on par with the ATT
         * specification. Note the callback will not be called in case false
         * is returned.
         *
         * @param device device we wish to indicate. This parameter is
         * mandatory and cannot be null.
         * @param value value we wish to indicate. This parameter is
         * mandatory and cannot be null. Note the value length cannot be more
         * than 20 bytes - a limitation of ATT.
         * @param cb callback via which the user get the indication status in
         * case the function succeeds. This parameter is
         * mandatory and cannot be null.
         * @return true if an indication was queued. If the indication was
         * queued successfully, more information would be returned via the cb,
         * if one was provided.
         */
        synchronized public boolean sendIndication(BluetoothDevice device, byte[] value,
                    GattIndicationStatusCallback cb) {
            if (!checkBeforeTransmit(value))
                return false;

            if (device == null) {
                Log.e(TAG, "cannot send indication to null device");
                return false;
            }

            if (cb == null) {
                Log.e(TAG, "cannot send indication with null cb");
                return false;
            }

            if (DBG)
                Log.d(TAG, "sendIndication to " + device.getAddress() +
                        " length " + value.length);

            int cookie = 0;
            try {
                cookie = mServer.mService.sendIndication(mServer.mProfileId, device.getAddress(),
                        mValueAttrib.getHandle(), mCccAttrib.getHandle(), value);
            } catch (RemoteException e) {
                e.printStackTrace();
                return false;
            }

            if (cookie == 0) {
                Log.e(TAG, "failed sending indication profile " + mServer.mProfileId +
                        " handle " + mValueAttrib.getHandle());
                return false;
            }

            synchronized(mServer.mIndCookies) {
                mServer.mIndCookies.put(cookie, cb);
            }
            return true;
        }
    }

    public static interface BluetoothGattServerCallback {
        /**
         * Register GATT services and characteristics in a profile.
         *
         * Must be implemented by the profile.
         *
         * Register the GATT profile implemented by this class. This means defining
         * one or more GATT services with all their attributes and registering
         * them. See: {@link GattService}.
         *
         * This function must always define the same services, characteristics and
         * attributes, in the same order. Any change in the order or definitions may
         * break remote clients that rely on the attribute DB to be the same between
         * connections. This includes UUIDs used and permissions defined.
         *
         * The initial values given to static attributes can be changed, since this
         * does not affect the order.
         *
         * This function must be predictable, so it's best to keep general profile
         * initialization to the class constructor or the service onCreate call.
         * The profile must be ready to operate after this function is called. This
         * function should be very short.
         *
         * Do not perform other GATT operations (indications, notifications)
         * when inside this function. These will fail. Do not get handles for
         * attributes (via {@link GattAttribute#getHandle}) before the owning
         * service is registered.
         *
         * Do not rely on handle values for attributes to be constant in any way.
         * They can change between invocations of this function.
         */
        abstract void registerProfile();

        /**
         * Unregister a profile.
         *
         * Must be implemented by the profile.
         *
         * Remove the registered profile and clean up all state. GATT operations
         * (notifications, indications) will fail after this function returns.
         *
         * The profile service will generally be shutdown after this function
         * is called. Note that onDestroy() will still be called for the service,
         * so broadcast-receivers and such can be unregistered there. Of course,
         * make sure all still-registered components of the profile don't do
         * GATT operations.
         */
        abstract void unregisterProfile();
    }
}
