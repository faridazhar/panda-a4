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

import android.os.IBinder;
import android.bluetooth.BluetoothDevice;
import com.ti.bluetooth.gatt.GattReadResult;

/**
 * System private API for talking with the Bluetooth Gatt Server service
 *
 * {@hide}
 */
interface IBluetoothGattServerCallback {
    /*
     * dryRun means the profile is being installed, and will be shutdown
     * immediately after started
     */
    void start(int profileId, in IBinder srv);
    void stop();

    /*
    * The return value contains a value buffer and a status to return to the
    * remote peer.
    */
    GattReadResult readCallback(int handle, in BluetoothDevice device);

    /*
    * The return value indicates what to return to the remote peer
    */
    int writeCallback(int handle, in BluetoothDevice device, in byte[] value);

    /*
    * cookie is the value given in the return value of the related
    * indicateAttribute() call to the low level service.
    */
    void indicateResult(int cookie, int status);
}
