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

package com.ti.wilink;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStreamReader;

import android.util.Log;

public class WiLinkInfo {
    private static final String TAG = "WiLinkInfo";

    private static String mChipVersion = null;
    private static int mChipProject  = -1;

    public static final int WILINK_TRIO = 7;
    public static final int WILINK_12XX = 10;
    public static final int WILINK_185X = 11;
    public static final int WILINK_189X = 12;

    public static final String ANT_BLE_WBS_SWITCH = "com.ti.ant_ble_wbs_switch";
    public static final String WBS_ENABLED = "com.ti.wbs_enabled";

    /** @hide */
    public static String getChipVersion() {
    if (mChipVersion == null) {
        try {
        File file = new File("/sys/kernel/debug/ti-st/version");
        FileInputStream is  = new FileInputStream(file);
        BufferedReader reader = new BufferedReader(new InputStreamReader(is));
        String verLine;

        verLine = reader.readLine();
        reader.close();
        is.close();
        Log.i(TAG, "WiLink Chip Version = "+ verLine);

        String delims = "[ ]";
        String vers[] = verLine.split(delims);
        if (vers.length < 2){
            Log.e(TAG, "Error getting WiLink chip version");
            return null;
        }
        mChipVersion = vers[1];

        } catch (Exception e) {
        e.printStackTrace();
        }

    }
    return mChipVersion;
    }

    /** @hide */
    public static int getChipProject() {
    if (mChipProject == -1) {
        getChipVersion();

        String delims = "[.]";
        String vers[] = mChipVersion.split(delims);
        if (vers.length < 3) {
        Log.e(TAG, "Error getting WiLink chip project");
        return -1;
        }

        mChipProject = Integer.parseInt(vers[0]);
    }
    return mChipProject;
    }

    /** @hide */
    public static int getDefaultValueForKey(String key) {
    getChipProject();

    switch (mChipProject) {
        case WILINK_TRIO:
        case WILINK_12XX:
        if (key.equals(ANT_BLE_WBS_SWITCH))
            return 1;
        if (key.equals(WBS_ENABLED))
            return 0;
        break;
        case WILINK_185X:
        case WILINK_189X:
        if (key.equals(ANT_BLE_WBS_SWITCH))
            return 0;
        if (key.equals(WBS_ENABLED))
            return 1;
        break;
        default:
        Log.e(TAG, "Unknown chip project " + mChipProject);
        if (key.equals(ANT_BLE_WBS_SWITCH))
            return 1;
        if (key.equals(WBS_ENABLED))
            return 0;
    }
    return 0;
    }
}
