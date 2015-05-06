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

package com.android.server;

import android.content.Context;
import android.os.IBinder;
import android.os.INetworkManagementService;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.net.InterfaceConfiguration;
import android.net.INetworkManagementEventObserver;
import android.util.Log;

import java.util.ArrayList;
import java.util.List;


/**
 * This class tracks uEvents associated with Ethernet
 *
 * @author Anatolii.Shuba@ti.com
 * @since 01-09-2012
 */
public class TIEthernetMonitor extends INetworkManagementEventObserver.Stub {
    private static final String TAG = "TIEthernetMonitor";
    private static final boolean DBG = true;

    private TIEthernetService mEthService;
    private INetworkManagementService mNMService;
    private Context context;
    private String ethRegex;

    public TIEthernetMonitor(Context ctx, TIEthernetService service) {
        super();
        mEthService = service;
        context = ctx;
        ethRegex = context.getResources().getString(com.android.internal.R.string.config_ethernet_iface_regex);

        IBinder b = ServiceManager.getService(Context.NETWORKMANAGEMENT_SERVICE);
        mNMService = INetworkManagementService.Stub.asInterface(b);

        startMonitoring();

        Log.d(TAG, "TIEthernetMonitor created");
    }

    /**
     * Get list of ethernet interfaces that already exist on the board
     *
     * @return List of {@link  android.net.InterfaceConfiguration InterfaceConfiguration} objects
     */
    public List<String> getEthIfacesList() {
        // Check for ethernet interfaces that already exist
        String ethRegex = context.getResources().getString(com.android.internal.R.string.config_ethernet_iface_regex);
        ArrayList<String> ifacesList = new ArrayList<String>();
        try {
            final String[] ifaces = mNMService.listInterfaces();
            for (String iface : ifaces) {
                if (iface.matches(ethRegex)) {
                    ifacesList.add(iface);
                }
            }
        } catch (RemoteException e) {
            Log.e(TAG, "Could not get list of interfaces " + e);
        }
        return ifacesList;
    }

    public void interfaceStatusChanged(String iface, boolean up) {
        if (iface.matches(ethRegex)) {
            mEthService.interfaceStatusChanged(iface, up);
        }
    }

    public void interfaceLinkStateChanged(String iface, boolean up) {
        if (iface.matches(ethRegex)) {
            mEthService.interfaceLinkStateChanged(iface, up);
        }
    }

    public void interfaceAdded(String iface) {
        if (iface.matches(ethRegex)) {
            mEthService.interfaceAdded(iface);
        }
    }

    public void interfaceRemoved(String iface) {
        if (iface.matches(ethRegex)) {
            mEthService.interfaceRemoved(iface);
        }
    }

    public void limitReached(String limitName, String iface) {
        if (iface.matches(ethRegex)) {
            mEthService.limitReached(iface, iface);
        }
    }

    public void startMonitoring() {
        // Register for all notifications from NetworkManagement Service
        try {
            mNMService.registerObserver(this);
            Log.e(TAG, "start monitoring for  NetworkManagement Service events");
        } catch (RemoteException e) {
            Log.e(TAG, "Could not register TIEthernetMonitor for connectivity events " + e);
        }
    }

    public void stopMonitoring() {
        // Unegister for any notifications from NetworkManagement Service
        try {
            mNMService.unregisterObserver(this);
            Log.e(TAG, "stop monitoring for  NetworkManagement Service events");
        } catch (RemoteException e) {
            Log.e(TAG, "Could not unregister TIEthernetMonitor from connectivity events " + e);
        }
    }
}
