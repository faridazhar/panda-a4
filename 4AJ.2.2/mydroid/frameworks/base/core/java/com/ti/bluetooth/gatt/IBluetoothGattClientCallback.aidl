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


/**
 * These callbacks should be implemented by a class wishing to get Gatt Server
 * event notifications
 * @hide
 */

oneway interface IBluetoothGattClientCallback
{
    void valueChanged(in String path, in byte[] value);
    void propertyChanged(in String path, in String property, in String value);
    void clientConnected(in String path);
    void clientDisconnected(in String path);
    void valueWriteReqComplete(in String path, in byte status);
    void valueReadComplete(in String path, in byte status, in byte[] value);
}
