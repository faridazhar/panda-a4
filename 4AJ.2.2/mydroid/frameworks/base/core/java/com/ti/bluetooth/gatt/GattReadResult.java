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

import android.os.Parcel;
import android.os.Parcelable;

/**
 * Class representing a result from a profile read callback.
 *
 * In essence this class contains a status code (in the form of a
 * {@link GattStatus}) and a buffer with the returned read data.
 */
public class GattReadResult implements Parcelable {
    byte[] mValue;
    int mStatus;

    GattReadResult(Parcel in) {
        mStatus = in.readInt();

        boolean hasValue = (in.readInt() != 0);
        if (hasValue)
            mValue = in.createByteArray();
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(Parcel dest, int flags) {
        dest.writeInt(mStatus);

        int hasValue = (mValue != null) ? 1 : 0;
        dest.writeInt(hasValue);

        if (hasValue != 0)
            dest.writeByteArray(mValue);
    }

    public static final Parcelable.Creator<GattReadResult> CREATOR
                                = new Parcelable.Creator<GattReadResult>() {
        public GattReadResult createFromParcel(Parcel in) {
            return new GattReadResult(in);
        }

        public GattReadResult[] newArray(int size) {
            return new GattReadResult[size];
        }
    };

    GattReadResult() {
        mStatus = GattStatus.ERR_IO.getValue();
        mValue = null;
    }

    public GattReadResult(byte[] value, GattStatus status) {
        mValue = value;
        mStatus = status.getValue();
    }

    // provided for convenience
    public GattReadResult(byte value, GattStatus status) {
        mValue = new byte[] {value};
        mStatus = status.getValue();
    }

    // provided for convenience
    public GattReadResult(GattStatus status) {
        mValue = null;
        mStatus = status.getValue();
    }

    public byte[] getValue() {
        return mValue;
    }

    public int getIntStatus() {
        return mStatus;
    }
}
