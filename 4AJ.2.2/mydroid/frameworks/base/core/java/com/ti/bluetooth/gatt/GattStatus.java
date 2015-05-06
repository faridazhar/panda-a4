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
 * Enum representing a GATT status code.
 *
 * The only success value is SUCCESS. User defined error codes start from 0x80
 * and up.
 */
public enum GattStatus {
    /**
     * Success status.
     */
    SUCCESS(0x00),

    // These are errors
    INVALID_HANDLE(0x01),
    READ_NOT_PERM(0x02),
    WRITE_NOT_PERM(0x03),
    INVALID_PDU(0x04),
    AUTHENTICATION(0x05),
    REQ_NOT_SUPP(0x06),
    INVALID_OFFSET(0x07),
    AUTHORIZATION(0x08),
    PREP_QUEUE_FULL(0x09),
    ATTR_NOT_FOUND(0x0A),
    ATTR_NOT_LONG(0x0B),
    INSUFF_ENCR_KEY_SIZE(0x0C),
    INVAL_ATTR_VALUE_LEN(0x0D),
    UNLIKELY(0x0E),
    INSUFF_ENC(0x0F),
    UNSUPP_GRP_TYPE(0x10),
    INSUFF_RESOURCES(0x11),

    // These are bluez defined error codes
    /**
     * Application defined error code.
     */
    ERR_IO(0x80),
    /**
     * Application defined error code.
     */
    TIMEOUT(0x81),
    /**
     * Application defined error code.
     */
    COMMAND_ABORTED(0x82);

    int mValue;

    GattStatus(int value) {
        mValue = value;
    }

    public int getValue() {
        return mValue;
    }


    /**
     * Try to find an enumeration element with this value. Do not call this
     * function often. It is expensive.
     */
    public static GattStatus getEnumValue(int value) {
        for (GattStatus s : values()) {
            if (s.getValue() == value)
                return s;
        }

        // default error value
        return ERR_IO;
    }
}
