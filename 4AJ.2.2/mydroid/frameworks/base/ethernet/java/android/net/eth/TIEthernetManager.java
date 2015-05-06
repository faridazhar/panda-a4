/*
 * Copyright (C) Texas Instruments - http://www.ti.com/
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

package android.net.eth;

import android.os.Handler;
import android.os.RemoteException;

import android.util.Log;

import java.util.ArrayList;
import java.util.List;

/**
 * This class provides the primary API for managing ethernet
 * connectivity. Get an instance of this class by calling
 * {@link android.content.Context#getSystemService(String) Context.getSystemService(Context.ETHERNET_SERVICE)}.

 * It deals with several categories of items:
 * <ul>
 * <li>The list of configured interfaces. The list can be viewed and updated,
 * and attributes of individual ethernet interface can be modified.</li>
 * <li>List of ethernet interfaces presented on board</li>
 * </ul>
 * @hide
 */

public class TIEthernetManager {
    public static final String TAG = "TIEthernetManager";
    public static final String NETWORKTYPE = "ETHERNET";

    private ITIEthernetManager mService;
    private Handler mHandler;

    /**
     * Intent will be send in case of enabling/disabling Ethernet on the board
     */
    public static final String ETH_GLOBAL_STATE_CHANGED_ACTION = "android.net.eth.GLOBAL_STATE_CHANGE";

    public static final String ETH_INTERFACE_STATE_CHANGED_ACTION = "android.net.eth.INTERFACE_STATE_CHANGE";
    /**
     * Broadcast intent action indicating that the link configuration
     * was changed on ethernet interface.
     */
    public static final String ETH_LINK_CONFIGURATION_CHANGED_ACTION = "android.net.eth.LINK_CHANGE";
    /**
     * The lookup key for a LinkProperties object associated with the
     * ethernet interface. Retrieve it with {@link android.content.Intent#getParcelableExtra(String)}.
     */
    public static final String EXTRA_LINK_PROPERTIES = "linkProperties";
    /**
     * The lookup key for a {@link android.net.NetworkInfo} object associated with the
     * ethernet interface. Retrieve with
     * {@link android.content.Intent#getParcelableExtra(String)}.
     */
    public static final String EXTRA_NETWORK_INFO = "networkInfo";

    /**
     * Create a new TI EthernetManager instance.
     * Applications will almost always want to use
     * {@link android.content.Context#getSystemService Context.getSystemService()} to retrieve
     * the standard {@link android.content.Context#ETHERNET_SERVICE Context.ETHERNET_SERVICE}.
     * @param service the Binder interface
     * @param handler target for messages
     */
    public TIEthernetManager(ITIEthernetManager service, Handler handler) {
        mService = service;
        mHandler = handler;
    }

    /**
     * Update configuration of the corresponding ethernet interface
     * @param ec {@link android.net.eth.EthernetConfiguration}
     */
    public void updateIfaceConfiguration(EthernetConfiguration ec) {
        try {
            mService.updateIfaceConfiguration(ec);
        } catch (RemoteException e) {
            Log.e(TAG, "Remote exception during updateIfaceConfiguration method call. Ethernet service died?", e);
        }
    }

    /**
     * Fetch ethernet interface configuration from the store
     * @param iface interface name to fetch
     * @return {@code null} or persistent configuration previously saved
     */
    public EthernetConfiguration getConfiguredIface(String iface) {
        try {
            return mService.getConfiguredIface(iface);
        } catch (RemoteException e) {
            Log.e(TAG, "Remote exception during getConfiguredIface method call. Ethernet service died?", e);
            return null;
        }
    }

    /**
     * Fetch all ethernet interfaces configuration saved in the store
     * @return {@link java.util.List}  of interface configurations previosly saved.
     * May be empty but not null
     */
    public List<EthernetConfiguration> getConfiguredIfaces() {
        try {
            return mService.getConfiguredIfaces();
        } catch (RemoteException e) {
            Log.e(TAG, "Remote exception during getConfiguredIfaces method call. Ethernet service died?", e);
            return new ArrayList<EthernetConfiguration>();
        }
    }

    /**
     * Get {@link java.util.List List} of all ethernet interfaces presented on board
     * @return {@link java.util.List} of ethernet interfaces available on board.
     * May be empty
     */
    public List<String> getIfaces() {
        try {
            return mService.getIfaces();
        } catch (RemoteException e) {
            Log.e(TAG, "Remote exception during getIfaces method call. Ethernet service died?", e);
            return new ArrayList<String>();
        }
    }

    /**
     * Change Ethernet global state
     * @param state to change
     */
    public void setEthernetState(boolean state) {
        try {
            mService.setEthernetState(state);
        } catch (RemoteException e) {
            Log.e(TAG, "Remote exception during changeEthernetState method call. Ethernet service died?", e);
        }
    }

    /**
     * Get Ethernet global state
     */
    public boolean getEthernetState() {
        try {
            return mService.getEthernetState();
        } catch (RemoteException e) {
            Log.e(TAG, "Remote exception during getEthernetState method call. Ethernet service died?", e);
            return false;
        }
    }
}
