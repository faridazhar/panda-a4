/*
 * Copyright (C) 2011 The Android Open Source Project
 * Copyright (C) 2010-2012 Texas Instruments, Inc. All rights reserved.
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

package board_id.com.ti;

import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.PowerManager;
import android.os.RemoteException;
import android.util.Log;

public class BoardIDService extends Service {

    private static final String TAG = "BoardIDService";
    public static final int SOC_FAMILY = 0;
    public static final int SOC_REV = 1;
    public static final int SOC_TYPE = 2;
    public static final int SOC_MAX_FREQ = 3;
    public static final int APPS_BOARD_REV = 4;
    public static final int CPU_MAX_FREQ = 5;
    public static final int CPU_GOV = 6;
    public static final int LINUX_VERSION = 7;
    public static final int LINUX_PVR_VER = 8;
    public static final int LINUX_CMDLINE = 9;
    public static final int LINUX_CPU1_STAT = 10;
    public static final int LINUX_OFF_MODE = 11;
    public static final int WILINK_VERSION = 12;
    public static final int DPLL_TRIM = 13;
    public static final int RBB_TRIM = 14;
    public static final int PRODUCTION_ID = 15;
    public static final int DIE_ID = 16;

    public static final int PROP_DISPLAY_ID = 0;
    public static final int PROP_BUILD_TYPE = 1;
    public static final int PROP_SER_NO	= 2;
    public static final int PROP_BOOTLOADER = 3;
    public static final int PROP_DEBUGGABLE = 4;
    public static final int PROP_CRYPTO_STATE =	5;

    // Reference to the system power manager
    protected PowerManager pm;

    // Used to control the service's wakelock
    protected PowerManager.WakeLock serviceWakeLock;

     // Constructor method which sets up any needed initializations.
    public BoardIDService() {
        super();
    }

    // Overrides the Service class's standard method. It loads the ThermalObserver
    @Override
    public void onCreate() {

        //Log.d(TAG,"+BoardIDService.onCreate loading JNI");
	System.load("/system/lib/libboard_idJNI.so");
        // Let's {responsibly} control the power state
        pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
        // get a wake lock when we start
        serviceWakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, TAG);
    }

    @Override
    public IBinder onBind(Intent intent) {
        Log.d(TAG,"+BoardIDService.onBind()");
        if (IBoardIDService.class.getName().equals(intent.getAction())) {
            return mBinder;
        } else {
            return null;
        }
    }

    @Override
    public boolean onUnbind(Intent intent) {
        //Log.d(TAG,"+BoardIDService.onUnbind()");
        if (IBoardIDService.class.getName().equals(intent.getAction())) {
            return super.onUnbind(intent);
        } else {
            return false;
        }
    }

    @Override
    public void onRebind(Intent intent) {
        //Log.d(TAG,"+BoardIDService.onRebind()");
        if (IBoardIDService.class.getName().equals(intent.getAction())) {
            super.onRebind(intent);
        }
    }

    @Override
    public void onDestroy() {
	return;
    }

    public String GetBoardProp(int value) {
        //Log.d(TAG,"+BoardIDService.onCreate loading JNI");
	System.load("/system/lib/libboard_idJNI.so");
        return getBoardIDsysfsNative(value);
    }

    public String GetProperty(int value) {
        //Log.d(TAG,"+BoardIDService.onCreate loading JNI");
	System.load("/system/lib/libboard_idJNI.so");
        return getBoardIDpropNative(value);
    }

    public int SetGovernor(int value) {
        //Log.d(TAG,"+BoardIDService.onCreate loading JNI");
	System.load("/system/lib/libboard_idJNI.so");
        return setGovernorNative(value);
    }

    private native String getBoardIDsysfsNative(int value);
    private native String getBoardIDpropNative(int value);
    private native int setGovernorNative(int value);

    //**************************************************************************
    // PRIVATE
    //**************************************************************************

    /** Defines our local RPC instance object which clients use to communicate with us. */
    private final IBoardIDService.Stub mBinder = new IBoardIDService.Stub() {
        public String GetBoardProp(int value) {
		return "Not Avaialble";
        }
        public String GetProperty(int value) {
		return "Not Avaialble";
        }
        public int SetGovernor(int value) {
		return -1;
        }
    };

}

