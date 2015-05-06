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

import android.app.Activity;
import android.util.Log;

import android.content.Context;
import android.os.IBinder;
import android.os.Message;
import android.os.RemoteException;
import android.content.ComponentName;
import android.content.Intent;
import android.content.ServiceConnection;

/**
* @brief
* BoardIDAgent is the main entry point to the BoardIDDaemon
*
*/
public class BoardIDAgent implements ServiceConnection {

    private static final String TAG = "BoardIDAgent";
    private IBinder mServiceToken;
    private IBoardIDService mService;
    private static Context mContext;
    private boolean mConnected;
    private static BoardIDAgentConnection mClient;

    /**
    * Constructor[s]
    */
    public BoardIDAgent(Context context, BoardIDAgentConnection client) {
        mContext = context;
        mClient = client;
        mService = null;
        mServiceToken = null;
        mConnected = false;
    }

    /**
    *
    * Creates a connection to the BoardIDService.
    * (System call by Android! no direct invocation)
    *
    */
    public void onServiceConnected(ComponentName className, IBinder service) {
        synchronized( this ) {
            mService = IBoardIDService.Stub.asInterface((IBinder)service);
            mServiceToken = (IBinder)service;
            mConnected = true;
            if (mClient != null)
                mClient.onAgentConnected();
        }
    }

    /**
    *
    * Releases the connection to the BoardIDDaemon.
    * (System call by Android! no direct invocation)
    *
    */
    public void onServiceDisconnected(ComponentName className) {
        synchronized( this ) {
            mClient.onAgentDisconnected();
            mService = null;
            mServiceToken = null;
            mConnected = false;
        }
    }

    /** Creates a connection to the BoardIDDaemon. */
    public void Connect() {
        synchronized( this ) {
            if (!mConnected) {
                Intent intent = new Intent("board_id.com.ti.IBoardIDService");
                if (!mContext.bindService(intent, this, Context.BIND_AUTO_CREATE)) {
                    Log.e(TAG, " Failed to launch intent connecting to IBoardIDService");
                } else {
                    mConnected = true;
                    Log.e(TAG, " Connect():: TRUELY connected!");
                }
            }
            else
            {
                Log.d(TAG, "Already connected!");
            }
        }
    }

    /**
    *
    * Releases the connection to the BoardIDDaemon.
    *
    */
    public void Disconnect() {
        synchronized (this) {
            if (mConnected) {
                try {
                    mContext.unbindService(this);
                } catch (IllegalArgumentException ex) {
                    Log.v(TAG, " Could not disconnect from service: "+ex.toString());
                }
                mConnected = false;
            } else {
                Log.d(TAG, "Already disconnected!");
            }
        }
    }

    /**
    *
    * Returns whether we are connected to the BoardIDDaemon service
    * @return true if we are connected, false otherwise
    */
    public synchronized boolean isConnected() {
        return (mConnected);
    }

    /**
    * Defines a power state for the USP's current profile.
    *
    * @param state  - power state to configure USP with
    */
    public int GetBoardProp(int value)
    {
	int ret = 0;

        if (mService != null) {
            try {
                mService.GetBoardProp(value);
            } catch (RemoteException e) {
                Log.e(TAG, " Remote call exception: " + e.toString());
            } catch (NullPointerException e2) {
                Log.e(TAG, " Null pointer exception: "+ e2.toString());
            }
        }
	return ret;
    }

    public int GetProperty(int value)
    {
	int ret = 0;

        if (mService != null) {
            try {
                mService.GetProperty(value);
            } catch (RemoteException e) {
                Log.e(TAG, " Remote call exception: " + e.toString());
            } catch (NullPointerException e2) {
                Log.e(TAG, " Null pointer exception: "+ e2.toString());
            }
        }
	return ret;
    }

    public int SetGovernor(int value)
    {
	int ret = 0;

        if (mService != null) {
            try {
                ret = mService.SetGovernor(value);
            } catch (RemoteException e) {
                Log.e(TAG, " Remote call exception: " + e.toString());
            } catch (NullPointerException e2) {
                Log.e(TAG, " Null pointer exception: "+ e2.toString());
            }
        }
	return ret;
    }

    //**************************************************************************
    // INTERFACES
    //**************************************************************************

    /**
    *
    *  A callback interface that allows the applications (clients
    *  of the BoardIDAgent) to operate on the BoardID Service, i.e.
    *  connect to / disconnect from it.
    *
    */
    public interface BoardIDAgentConnection {

        /** Called when the BoardIDAgent connects to the BoardIDService */
        public void onAgentConnected();

        /** Called when the BoardIDDaemon changes the Power State */
        public void onPowerStateChange(int newState);

        /** Called when the BoardIDAgent disconnects from the BoardIDService */
        public void onAgentDisconnected();
    }
}

