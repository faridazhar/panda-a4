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
import com.ti.bluetooth.gatt.IBluetoothGattClientCallback;
import android.bluetooth.BluetoothDevice;

/** @hide */
interface IBluetoothGattClient
{
    int Connect(in BluetoothDevice device, IBluetoothGattClientCallback cb);
    int CancelConnect(in BluetoothDevice device,in int id);
    int Disconnect(in BluetoothDevice device,in int id);
    String[] getServiceList(in BluetoothDevice device);
    String[] getServiceProperties(in BluetoothDevice device, String service);
    String[] getCharacteristicList(in BluetoothDevice device, String Service);
    String[] getCharacteristicProperties(in BluetoothDevice device, String characteristic);
    boolean setNotifying(in BluetoothDevice device, String Characteristic,boolean notify);
    boolean setIndicating(in BluetoothDevice device, String Characteristic,boolean notify);
    boolean setBroadcasting(in BluetoothDevice device, String Characteristic,boolean broadcast);
    boolean Read(in BluetoothDevice device, String Characteristic, int offset);
    boolean Write(in BluetoothDevice device, String Characteristic,in byte[] value, boolean waitForResponse);
    boolean isConnected(in BluetoothDevice device);
}
