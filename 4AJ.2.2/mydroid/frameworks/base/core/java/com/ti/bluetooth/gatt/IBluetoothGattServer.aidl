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

import com.ti.bluetooth.gatt.IBluetoothGattServerCallback;

/**
 * System private API for talking with the Bluetooth Gatt Server service
 *
 * {@hide}
 */
interface IBluetoothGattServer {
    /*
     * The return value is the startHandle for this service (serviceIdx)
     */
    int addService(int profileIdx, int serviceIdx, in byte[] svcUuid,
        int attributeNum);

    /*
    * profileId is the id given to the profile by the start() method. The
    * service index is assigned by the profile and incremented on every new
    * service definition.
    */
    void addAttribute(int profileId, int serviceIdx, int handle,
            in byte[] uuid, int readReqs, int writeReqs,
            in byte[] value, boolean registerCb);

    /*
     * Can only be called after all service addributes have been added.
     */
    void registerSdpRecord(int profileIdx, int serviceIdx, int handle,
                String name);

    void endProfileInit(int profileId);

    /*
    * A broadcast address ("ff:ff:ff:ff:ff:ff") means to notify to all
    * registered devices.
    */
    void sendNotification(int profileId, String address, int handle,
                int ccc, in byte[] value);

    /*
    * A broadcast address ("ff:ff:ff:ff:ff:ff") means to indicate to all
    * registered devices. The return value is a unique cookie. It is received
    * again in the indication result and is used to relate indications to
    * results. A return value of 0 indicates failure.
    */
    int sendIndication(int profileId, String address, int handle,
                int ccc, in byte[] value);

    /*
     * Add/remove a dynamic profile. Will be started on BT-on.
     * The profileUniqueId string must be unique throughout the system. It
     * is advised to use a combination of the package name and a unique name
     * for the profile (e.g. "com.ti.bluetooth.gatt.proximity")
     */
    void addDynamicProfile(String profileUniqueName, in IBluetoothGattServerCallback cb);
    void removeDynamicProfile(String profileUniqueName);
}
