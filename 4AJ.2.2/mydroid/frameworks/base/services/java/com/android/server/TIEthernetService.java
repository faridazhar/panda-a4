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

import android.content.BroadcastReceiver;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.IBinder;
import android.os.INetworkManagementService;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.net.ConnectivityManager;
import android.net.DhcpInfoInternal;
import android.net.InterfaceConfiguration;
import android.net.NetworkInfo;
import android.net.NetworkInfo.DetailedState;
import android.net.LinkProperties;
import android.net.LinkAddress;
import android.net.RouteInfo;
import android.net.NetworkUtils;
import android.net.eth.EthernetConfiguration;
import android.net.eth.EthernetConfigStore;
import android.net.eth.EthernetConfiguration.IPAssignment;
import android.net.eth.ITIEthernetManager;
import android.net.eth.TIEthernetManager;
import android.provider.Settings;
import android.util.Log;

import java.net.InetAddress;
import java.net.UnknownHostException;
import java.util.ArrayList;
import java.util.List;


/**
 * TI ethernet Service handles ethernet operation requests by implementing
 * the ITIEthernetManager interface.
 *
 * @author Anatolii.Shuba@ti.com
 * @since 01-09-2012
 */

public class TIEthernetService extends ITIEthernetManager.Stub {
    private static final String TAG = "TIEthernetService";
    private static final boolean DBG = true;

    private Context mContext;
    private TIEthernetMonitor mEthMonitor;
    private INetworkManagementService mNMService;
    private String mHwAddr;
    private BroadcastReceiver bootCompleteReceiver;
    private boolean mIsEthernetEnabled;

    public TIEthernetService(Context context) {
        Log.d(TAG, "TIEthernetService created");
        mContext = context;

        mIsEthernetEnabled = false;

        IBinder binder = ServiceManager.getService(Context.NETWORKMANAGEMENT_SERVICE);
        mNMService = INetworkManagementService.Stub.asInterface(binder);
        mEthMonitor = new TIEthernetMonitor(mContext, TIEthernetService.this);

        bootCompleteReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                // Boot completed. Check persistent state and enable ethernet if need
                if (getEthernetPersistedState()) {
                    setEthernetState(true);
                }
            }
        };

        // Registering receiver for BOOT_COMPLETED intent;
        // It is prohibited to send broadcast intents (ETH_INTERFACE_STATE_CHANGED_ACTION) before booting ends
        mContext.registerReceiver(bootCompleteReceiver, new IntentFilter(Intent.ACTION_BOOT_COMPLETED));
    }

    /**
     * Configure interface using static IP address info
     */
    private void configureInterface(EthernetConfiguration ec) {
        String iface = ec.getIfaceName();
        List<InetAddress> dnses = ec.getDNSaddresses();
        int dns1 = dnses.size() > 0 ? NetworkUtils.inetAddressToInt(dnses.get(0)) : 0;
        int dns2 = dnses.size() > 1 ? NetworkUtils.inetAddressToInt(dnses.get(1)) : 0;

        Log.e(TAG, "Start static configuration for " + iface);

        LinkProperties linkProp = new LinkProperties();
        linkProp.setInterfaceName(iface);

        NetworkInfo netInfo = new NetworkInfo(ConnectivityManager.TYPE_ETHERNET, 0, TIEthernetManager.NETWORKTYPE, "");
        netInfo.setIsAvailable(false);
        netInfo.setDetailedState(DetailedState.OBTAINING_IPADDR, null, mHwAddr);

        sendInterfaceStateChangeBroadcast(linkProp, netInfo);

        if (!NetworkUtils.configureIface(iface,
                                         NetworkUtils.inetAddressToInt(ec.getIfaceAddress()),
                                         ec.getPrefixLength(),
                                         NetworkUtils.inetAddressToInt(ec.getGateway()),
                                         dns1,
                                         dns2)) {
            Log.e(TAG, "Static IP configuration error for " + ec.getIfaceName());
            linkProp = new LinkProperties();
            linkProp.setInterfaceName(iface);

            netInfo = new NetworkInfo(ConnectivityManager.TYPE_ETHERNET, 0, TIEthernetManager.NETWORKTYPE, "");
            netInfo.setDetailedState(DetailedState.FAILED, null, mHwAddr);
            netInfo.setIsAvailable(false);

            sendInterfaceLinkConfigurationChangedBroadcast(linkProp);
            sendInterfaceStateChangeBroadcast(linkProp, netInfo);
        } else {

            linkProp = new LinkProperties();
            linkProp.addLinkAddress(new LinkAddress(ec.getIfaceAddress(), ec.getPrefixLength()));
            linkProp.addRoute(new RouteInfo(ec.getGateway()));
            linkProp.setInterfaceName(iface);
            if (dnses.size() > 0)
                linkProp.addDns(dnses.get(0));
            if (dnses.size() > 1)
                linkProp.addDns(dnses.get(1));

            netInfo = new NetworkInfo(ConnectivityManager.TYPE_ETHERNET, 0, TIEthernetManager.NETWORKTYPE, "");
            netInfo.setDetailedState(DetailedState.CONNECTED, null, mHwAddr);
            netInfo.setIsAvailable(true);

            try {
                Log.e(TAG, "Configurating of static IP finished for " + ec.getIfaceName() + ". " + mNMService.getInterfaceConfig(ec.getIfaceName()));
            } catch (RemoteException ex) {
                Log.e(TAG, "Unexpected exception. Network manager service died?", ex);
            }

            sendInterfaceLinkConfigurationChangedBroadcast(linkProp);
            sendInterfaceStateChangeBroadcast(linkProp, netInfo);
        }
    }

    /**
     * Stop using static IP address for corresponding interface
     */
    private void disconfigureInterface(EthernetConfiguration ec) {
        String ifaceName = ec.getIfaceName();

        LinkProperties linkProp = new LinkProperties();
        linkProp.setInterfaceName(ifaceName);

        NetworkInfo netInfo = new NetworkInfo(ConnectivityManager.TYPE_ETHERNET, 0, TIEthernetManager.NETWORKTYPE, "");
        netInfo.setIsAvailable(false);
        netInfo.setDetailedState(DetailedState.DISCONNECTING, null, mHwAddr);

        sendInterfaceStateChangeBroadcast(linkProp, netInfo);

        // Clear assigned static IP addresses
        if (!NetworkUtils.clearIfaceAddresses(ifaceName)) {
            Log.e(TAG, "Error clearing " + ifaceName + " addresses. Network unavailable");
            linkProp = new LinkProperties();
            linkProp.setInterfaceName(ifaceName);
            netInfo = new NetworkInfo(ConnectivityManager.TYPE_ETHERNET, 0, TIEthernetManager.NETWORKTYPE, "");
            netInfo.setDetailedState(DetailedState.FAILED, null, mHwAddr);
            netInfo.setIsAvailable(false);
            sendInterfaceStateChangeBroadcast(linkProp, netInfo);
        } else {
            linkProp = new LinkProperties();
            linkProp.setInterfaceName(ifaceName);
            netInfo = new NetworkInfo(ConnectivityManager.TYPE_ETHERNET, 0, TIEthernetManager.NETWORKTYPE, "");
            netInfo.setIsAvailable(false);
            netInfo.setDetailedState(DetailedState.DISCONNECTED, null, mHwAddr);
            sendInterfaceLinkConfigurationChangedBroadcast(linkProp);
            sendInterfaceStateChangeBroadcast(linkProp, netInfo);
        }
    }

    /**
     * Hot configuration update of the corresponding ethernet interface.
     * If interface currently in {@link android.net.NetworkInfo.DetailedState.CONNECTED}
     * or {@link android.net.NetworkInfo.DetailedState.FAILED}
     * state it will be disconnected first and then connected with new configuration
     *
     * @param ec {@link android.net.eth.EthernetConfiguration}
     */
    public void updateIfaceConfiguration(EthernetConfiguration ec) {
        ConnectivityManager connMgr = (ConnectivityManager) mContext.getSystemService(Context.CONNECTIVITY_SERVICE);

        // Stop using previous configuration
        if (getEthernetState()) {
            NetworkInfo networkInfo = connMgr.getNetworkInfo(ConnectivityManager.TYPE_ETHERNET);
            DetailedState detailedState = networkInfo.getDetailedState();
            // Properly stop currently connected or failed interface
            // before applying new configurattion
            if (detailedState == DetailedState.CONNECTED
                || detailedState == DetailedState.FAILED) {
                stopInterface(ec.getIfaceName());
            }
        }

        // Store new configuration
        EthernetConfigStore.addOrUpdateConfiguration(ec);

        // Tring to use new interface configuration
        if (getEthernetState()) {
            // Start using new configuration
            startInterface(ec.getIfaceName());
        }
    }

    /**
     * Fetch ethernet interface configuration from the store
     * @param iface interface name to fetch
     */
    public EthernetConfiguration getConfiguredIface(String iface) {
        return EthernetConfigStore.readConfiguration(iface);
    }

    /**
     * Fetch all ethernet interfaces configuration saved in the store
     * @return {@link java.util.List}  of interfaces. May be empty
     */
    public List<EthernetConfiguration> getConfiguredIfaces() {
        return EthernetConfigStore.readConfigurations(null);
    }

    /**
     * Get {@link java.util.List List} of all ethernet interfaces presented on board
     * @return {@link java.util.List} of ethernet interfaces available on board.
     * May be empty
     */
    public List<String> getIfaces() {
        return mEthMonitor.getEthIfacesList();
    }
    /**
     * Start DHCP negotiation process
     */
    private void runDhcp(EthernetConfiguration ec) {
        final String iface = ec.getIfaceName();

        LinkProperties linkProp = new LinkProperties();
        linkProp.setInterfaceName(iface);

        NetworkInfo netInfo = new NetworkInfo(ConnectivityManager.TYPE_ETHERNET, 0, TIEthernetManager.NETWORKTYPE, "");
        netInfo.setIsAvailable(false);
        netInfo.setDetailedState(DetailedState.OBTAINING_IPADDR, null, mHwAddr);

        sendInterfaceStateChangeBroadcast(linkProp, netInfo);

        Thread dhcpThread = new Thread(new Runnable() {
            public void run() {
                Log.e(TAG, "DHCP negotiation started for " + iface);
                DhcpInfoInternal dhcpInfoInternal = new DhcpInfoInternal();
                if (!NetworkUtils.runDhcp(iface, dhcpInfoInternal)) {
                    Log.e(TAG, "DHCP negotiation error:" + NetworkUtils.getDhcpError());
                    LinkProperties linkProp = new LinkProperties();
                    linkProp.setInterfaceName(iface);

                    NetworkInfo netInfo = new NetworkInfo(ConnectivityManager.TYPE_ETHERNET, 0, TIEthernetManager.NETWORKTYPE, "");
                    netInfo.setDetailedState(DetailedState.FAILED, null, mHwAddr);
                    netInfo.setIsAvailable(true);

                    sendInterfaceLinkConfigurationChangedBroadcast(linkProp);
                    sendInterfaceStateChangeBroadcast(linkProp, netInfo);
                } else {
                    Log.e(TAG, "DHCP negotiation finished for " + iface + ": " + dhcpInfoInternal.toString());
                    LinkProperties linkProp = dhcpInfoInternal.makeLinkProperties();
                    linkProp.setInterfaceName(iface);

                    NetworkInfo netInfo = new NetworkInfo(ConnectivityManager.TYPE_ETHERNET, 0, TIEthernetManager.NETWORKTYPE, "");
                    netInfo.setDetailedState(DetailedState.CONNECTED, null, mHwAddr);
                    netInfo.setIsAvailable(true);

                    sendInterfaceLinkConfigurationChangedBroadcast(linkProp);
                    sendInterfaceStateChangeBroadcast(linkProp, netInfo);
                }
            }
        });
        dhcpThread.start();
    }

    /**
     * Trying to free DHCP IP address
     */
    private void stopDhcp(EthernetConfiguration ec) {

        String iface = ec.getIfaceName();

        LinkProperties linkProp = new LinkProperties();
        linkProp.setInterfaceName(iface);

        NetworkInfo netInfo = new NetworkInfo(ConnectivityManager.TYPE_ETHERNET, 0, TIEthernetManager.NETWORKTYPE, "");
        netInfo.setIsAvailable(false);
        netInfo.setDetailedState(DetailedState.DISCONNECTING, null, mHwAddr);

        sendInterfaceStateChangeBroadcast(linkProp, netInfo);

        if (NetworkUtils.stopDhcp(iface)) {
            Log.e(TAG, "DHCP successfuly stopped for " + iface);

            linkProp = new LinkProperties();
            linkProp.setInterfaceName(iface);

            netInfo = new NetworkInfo(ConnectivityManager.TYPE_ETHERNET, 0, TIEthernetManager.NETWORKTYPE, "");
            netInfo.setIsAvailable(false);
            netInfo.setDetailedState(DetailedState.DISCONNECTED, null, mHwAddr);

            sendInterfaceLinkConfigurationChangedBroadcast(linkProp);
            sendInterfaceStateChangeBroadcast(linkProp, netInfo);
        } else {
            Log.e(TAG, "DHCP stop failed for " + iface);
        }
    }

    private void sendInterfaceLinkConfigurationChangedBroadcast(LinkProperties prop) {
        Intent intent = new Intent(TIEthernetManager.ETH_LINK_CONFIGURATION_CHANGED_ACTION);
        intent.putExtra(TIEthernetManager.EXTRA_LINK_PROPERTIES, new LinkProperties(prop));
        mContext.sendBroadcast(intent);
    }

    private void sendInterfaceStateChangeBroadcast(LinkProperties prop, NetworkInfo info) {
        Intent intent = new Intent(TIEthernetManager.ETH_INTERFACE_STATE_CHANGED_ACTION);
        intent.putExtra(TIEthernetManager.EXTRA_NETWORK_INFO, new NetworkInfo(info));
        intent.putExtra(TIEthernetManager.EXTRA_LINK_PROPERTIES, new LinkProperties (prop));
        mContext.sendBroadcast(intent);
    }

    private void sendGlobalStateChangeBroadcast() {
        Intent intent = new Intent(TIEthernetManager.ETH_GLOBAL_STATE_CHANGED_ACTION);
        mContext.sendBroadcast(intent);
    }

    /**
     * This method usually called by etherned monitor
     *
     * @param iface interface name which was removed
     */
    public void interfaceRemoved(String iface) {
        Log.d(TAG, "interfaceRemoved " + iface + " uevent received");
    }

    /**
     * This method usually called by etherned monitor
     *
     * @param iface interface name which status has been changed
     * @param up new state of interface (true - up, false - down)
     */
    public void interfaceStatusChanged(String iface, boolean up) {
        Log.d(TAG, "Interface status changed: " + iface + (up ? "up" : "down") + " uevent received");
    }

    /**
     * This method usually called by etherned monitor
     *
     * @param limitName limit name which limit has been reached
     * @param iface interface name where limit has been reached
     */
    public void limitReached(String limitName, String iface) {
        Log.d(TAG, "limitReached: " + iface + ", limitName=" + iface + " uevent received");
        LinkProperties linkProp = new LinkProperties();
        linkProp.setInterfaceName(iface);
        sendInterfaceLinkConfigurationChangedBroadcast(linkProp);
    }

    /**
     * This method usually called by etherned monitor
     *
     * @param iface interface name which has been added
     */
    public void interfaceAdded(String iface) {
        Log.d(TAG, "interfaceAdded " + iface + " uevent received");
        try {
            // We don't get link status indications unless the iface is up.
            // Bring it up!
            Log.d(TAG, "Setting up " + iface + " ethernet interface");
            mNMService.setInterfaceUp(iface);

            LinkProperties linkProp = new LinkProperties();
            linkProp.setInterfaceName(iface);
            sendInterfaceLinkConfigurationChangedBroadcast(linkProp);
        } catch (RemoteException e) {
            Log.e(TAG, "Could not set interface UP - " + iface, e);
        }
    }

    /**
     * This method usually called by etherned monitor
     *
     * @param iface interface name which link has been changed
     * @param up new state of link state (true - up, false - down)
     */
    public void interfaceLinkStateChanged(String iface, boolean up) {
        Log.d(TAG, "Interface " + iface + " link " + (up ? "up" : "down") + " uevent received");

        if (!EthernetConfigStore.isInterfaceConfigured(iface)) {
            Log.d(TAG, "No persistent configuration found for the " + iface);
            return;
        }
        // Checking for interface configuration inside config store
        EthernetConfiguration ec = EthernetConfigStore.readConfiguration(iface);
        if (null == ec)
            return;
        if (up) {
            // If link is up apply stored configuration to interface
            startInterface(iface);
        } else {
            stopInterface(iface);
        }
    }

    private void startInterface(String iface) {
        // Checking for interface configuration inside config store
        EthernetConfiguration ec = EthernetConfigStore.readConfiguration(iface);
        if (null == ec)
            // No interface configuration found
            return;

        // If link is up apply stored configuration to interface
        if (IPAssignment.DHCP == ec.getIpAssignment()) {
            // Start DHCP
            runDhcp(ec);
        } else {
            // Start static IP configuration
            configureInterface(ec);
        }
    }

    private void stopInterface(String iface) {
        // Checking for interface configuration inside config store
        EthernetConfiguration ec = EthernetConfigStore.readConfiguration(iface);
        if (null == ec)
            return;
        // Release interface resources
        if (IPAssignment.DHCP == ec.getIpAssignment()) {
            stopDhcp(ec);
        } else if (IPAssignment.STATIC == ec.getIpAssignment()) {
            disconfigureInterface(ec);
        }
    }

    /**
     * Change Ethernet global state
     * @param state to change
     */
    public void setEthernetState(boolean state) {
        if (mIsEthernetEnabled == state) {
            return;
        }
        mIsEthernetEnabled = state;
        List<String> ifaces = mEthMonitor.getEthIfacesList();

        String lastPluggedIface = null;

        for (String ifacename : ifaces) {
            lastPluggedIface = ifacename;
        }

        if (state) {
            mEthMonitor.startMonitoring();
        } else {
            mEthMonitor.stopMonitoring();
        }

        sendGlobalStateChangeBroadcast();

        if (state && null != lastPluggedIface) {
            // Setting UP last plugged ethernet interface
            if (EthernetConfigStore.isInterfaceConfigured(lastPluggedIface)) {
                try {
                    // we don't get link status indications unless the iface is up - bring it up
                    Log.d(TAG, "Setting up " + lastPluggedIface + " ethernet interface");
                    mNMService.setInterfaceUp(lastPluggedIface);
                    InterfaceConfiguration config = mNMService.getInterfaceConfig(lastPluggedIface);
                    if (config != null && mHwAddr == null) {
                        mHwAddr = config.getHardwareAddress();
                    }
                } catch (RemoteException e) {
                    Log.e(TAG, "Cant set interface UP - " + lastPluggedIface, e);
                }
            } else {
                Log.d(TAG, "Interface " + lastPluggedIface + " did not configured");
            }
        } else if (!state && null != lastPluggedIface){
            // Setting DOWN last plugged ethernet interface
            if (EthernetConfigStore.isInterfaceConfigured(lastPluggedIface)) {
                try {
                    Log.d(TAG, "Setting down " + lastPluggedIface + " ethernet interface");
                    stopInterface(lastPluggedIface);
                    mNMService.setInterfaceDown(lastPluggedIface);
                    InterfaceConfiguration config = mNMService.getInterfaceConfig(lastPluggedIface);
                    if (config != null && config.isActive()) {
                        Log.e(TAG, "Cant set interface to DOWN - " + lastPluggedIface);
                    }
                } catch (RemoteException e) {
                    Log.e(TAG, "Cant set interface to DOWN - " + lastPluggedIface, e);
                }
            } else {
                Log.d(TAG, "Interface " + lastPluggedIface + " did not configured");
            }
        }
        // Save ethernet persistent state
        setEthernetpersistedState(state);
    }

    /**
     * Get Ethernet global state
     */
    public boolean getEthernetState() {
        return mIsEthernetEnabled;
    }

    private void setEthernetpersistedState(boolean setOn) {
        ContentResolver contentResolver = mContext.getContentResolver();
        Settings.Secure.putInt(contentResolver, Settings.Secure.ETHERNET_ON, setOn ? 1 : 0);
    }

    private boolean getEthernetPersistedState() {
        ContentResolver contentResolver = mContext.getContentResolver();
        return (Settings.Secure.getInt(contentResolver, Settings.Secure.ETHERNET_ON, 0) > 0);
    }
}
