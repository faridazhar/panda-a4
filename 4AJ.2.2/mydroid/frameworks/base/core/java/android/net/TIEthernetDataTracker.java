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

package android.net;

import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.BroadcastReceiver;
import android.net.NetworkInfo.DetailedState;
import android.net.NetworkInfo.State;
import android.net.eth.TIEthernetManager;
import android.net.eth.EthernetConfiguration;
import android.net.eth.EthernetConfigStore;
import android.net.eth.EthernetConfiguration.IPAssignment;
import android.os.Handler;
import android.os.Message;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.util.Log;
import android.os.SystemProperties;

import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * This class tracks the data connection associated with Ethernet
 * This is a singleton class and an instance will be created by
 * ConnectivityService.
 * @hide
 */
public class TIEthernetDataTracker implements NetworkStateTracker {
    private static final String TAG = "TIEthernetDataTracker";

    private AtomicBoolean mTeardownRequested = new AtomicBoolean(false);
    private AtomicBoolean mPrivateDnsRouteSet = new AtomicBoolean(false);
    private AtomicInteger mDefaultGatewayAddr = new AtomicInteger(0);
    private AtomicBoolean mDefaultRouteSet = new AtomicBoolean(false);

    private LinkProperties mLinkProperties;
    private NetworkInfo mNetworkInfo;
    private BroadcastReceiver mStateChangesReceiver;

    /* For sending events to connectivity service handler */
    private Handler mCsHandler;
    private Context mContext;

    private static TIEthernetDataTracker sInstance;

    public static synchronized TIEthernetDataTracker getInstance() {
        if (sInstance == null) sInstance = new TIEthernetDataTracker();
        return sInstance;
    }

    public Object Clone() throws CloneNotSupportedException {
        throw new CloneNotSupportedException();
    }

    public void setTeardownRequested(boolean isRequested) {
        mTeardownRequested.set(isRequested);
    }

    public boolean isTeardownRequested() {
        return mTeardownRequested.get();
    }

    /**
     * Begin monitoring connectivity
     */
    public void startMonitoring(Context context, Handler target) {

        mContext = context;
        mCsHandler = target;

        IntentFilter filter = new IntentFilter();
        filter.addAction(TIEthernetManager.ETH_LINK_CONFIGURATION_CHANGED_ACTION);
        filter.addAction(TIEthernetManager.ETH_INTERFACE_STATE_CHANGED_ACTION);

        mContext.registerReceiver(mStateChangesReceiver, filter);
    }

    private TIEthernetDataTracker() {
        mNetworkInfo = new NetworkInfo(ConnectivityManager.TYPE_ETHERNET, 0, TIEthernetManager.NETWORKTYPE, "");
        mLinkProperties = new LinkProperties();

        mStateChangesReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                if (intent.getAction().equals(TIEthernetManager.ETH_LINK_CONFIGURATION_CHANGED_ACTION)) {
                    mLinkProperties = intent.getParcelableExtra(TIEthernetManager.EXTRA_LINK_PROPERTIES);
                    if (mLinkProperties == null) {
                        mLinkProperties = new LinkProperties();
                    }

                    mNetworkInfo = intent.getParcelableExtra(TIEthernetManager.EXTRA_NETWORK_INFO);
                    if (mNetworkInfo == null) {
                        mNetworkInfo = new NetworkInfo(ConnectivityManager.TYPE_ETHERNET, 0, TIEthernetManager.NETWORKTYPE, "");
                    }
                    Message msg = mCsHandler.obtainMessage(NetworkStateTracker.EVENT_STATE_CHANGED, new NetworkInfo(mNetworkInfo));
                    msg.sendToTarget();
                } else if (intent.getAction().equals(TIEthernetManager.ETH_INTERFACE_STATE_CHANGED_ACTION)) {
                    mNetworkInfo = intent.getParcelableExtra(TIEthernetManager.EXTRA_NETWORK_INFO);
                    if (mNetworkInfo == null) {
                        mNetworkInfo = new NetworkInfo(ConnectivityManager.TYPE_ETHERNET, 0, TIEthernetManager.NETWORKTYPE, "");
                    }
                    Message msg = mCsHandler.obtainMessage(NetworkStateTracker.EVENT_STATE_CHANGED, new NetworkInfo(mNetworkInfo));
                    msg.sendToTarget();
                }
            }
        };
    }

    /**
     * Disable connectivity to a network
     * TODO: do away with return value after making MobileDataStateTracker async
     */
    public boolean teardown() {
        Log.w(TAG, "Etherenet teardown() requested");
        if (mNetworkInfo.getState() == State.CONNECTED) {
            String iface = mLinkProperties.getInterfaceName();
            EthernetConfiguration ec = EthernetConfigStore.readConfiguration(iface);
            if (null != ec) {
                // TODO turn off and reset interface (?)
            }
            mTeardownRequested.set(true);
        }
        return true;
    }

    /**
     * Re-enable connectivity to a network after a {@link #teardown()}.
     */
    public boolean reconnect() {
        Log.w(TAG, "reconnect() called but not implemented. Because it no need for ethernet.");
        return true;
    }

    /**
     * Turn the wireless radio off for a network.
     * @param turnOn {@code true} to turn the radio on, {@code false}
     */
    public boolean setRadio(boolean turnOn) {
        return true;
    }

    /**
     * @return true - If are we currently tethered with another device.
     */
    public synchronized boolean isAvailable() {
        return mNetworkInfo.isAvailable();
    }

    /**
     * Tells the underlying networking system that the caller wants to
     * begin using the named feature. The interpretation of {@code feature}
     * is completely up to each networking implementation.
     * @param feature the name of the feature to be used
     * @param callingPid the process ID of the process that is issuing this request
     * @param callingUid the user ID of the process that is issuing this request
     * @return an integer value representing the outcome of the request.
     * The interpretation of this value is specific to each networking
     * implementation+feature combination, except that the value {@code -1}
     * always indicates failure.
     * TODO: needs to go away
     */
    public int startUsingNetworkFeature(String feature, int callingPid, int callingUid) {
        return -1;
    }

    /**
     * Tells the underlying networking system that the caller is finished
     * using the named feature. The interpretation of {@code feature}
     * is completely up to each networking implementation.
     * @param feature the name of the feature that is no longer needed.
     * @param callingPid the process ID of the process that is issuing this request
     * @param callingUid the user ID of the process that is issuing this request
     * @return an integer value representing the outcome of the request.
     * The interpretation of this value is specific to each networking
     * implementation+feature combination, except that the value {@code -1}
     * always indicates failure.
     * TODO: needs to go away
     */
    public int stopUsingNetworkFeature(String feature, int callingPid, int callingUid) {
        return -1;
    }

    @Override
    public void setUserDataEnable(boolean enabled) {
        Log.w(TAG, "ignoring setUserDataEnable(" + enabled + ") call");
    }

    @Override
    public void setPolicyDataEnable(boolean enabled) {
        Log.w(TAG, "ignoring setPolicyDataEnable(" + enabled + ") call");
    }

    /**
     * Check if private DNS route is set for the network
     */
    public boolean isPrivateDnsRouteSet() {
        return mPrivateDnsRouteSet.get();
    }

    /**
     * Set a flag indicating private DNS route is set
     */
    public void privateDnsRouteSet(boolean enabled) {
        mPrivateDnsRouteSet.set(enabled);
    }

    /**
     * Fetch NetworkInfo for the network
     */
    public synchronized NetworkInfo getNetworkInfo() {
        return mNetworkInfo;
    }

    /**
     * Fetch LinkProperties for the network
     */
    public synchronized LinkProperties getLinkProperties() {
        return new LinkProperties(mLinkProperties);
    }

   /**
     * A capability is an Integer/String pair, the capabilities
     * are defined in the class LinkSocket#Key.
     *
     * @return a copy of this connections capabilities, may be empty but never null.
     */
    public LinkCapabilities getLinkCapabilities() {
        return new LinkCapabilities();
    }

    /**
     * Fetch default gateway address for the network
     */
    public int getDefaultGatewayAddr() {
        return mDefaultGatewayAddr.get();
    }

    /**
     * Check if default route is set
     */
    public boolean isDefaultRouteSet() {
        return mDefaultRouteSet.get();
    }

    /**
     * Set a flag indicating default route is set for the network
     */
    public void defaultRouteSet(boolean enabled) {
        mDefaultRouteSet.set(enabled);
    }

    /**
     * Return the system properties name associated with the tcp buffer sizes
     * for this network.
     */
    public String getTcpBufferSizesPropName() {
        return "net.tcp.buffersize.eth";
    }

    public void setDependencyMet(boolean met) {
        // not supported on this network
    }
}
