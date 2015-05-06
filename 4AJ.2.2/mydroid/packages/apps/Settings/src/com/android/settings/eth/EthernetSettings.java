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

package com.android.settings.eth;

import com.android.settings.R;
import com.android.settings.SettingsPreferenceFragment;

import java.util.List;
import android.app.Activity;
import android.app.Dialog;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Bundle;
import android.os.Handler;
import android.preference.Preference;
import android.preference.PreferenceScreen;
import android.util.Log;
import android.net.eth.EthernetConfiguration;
import android.net.eth.TIEthernetManager;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.net.NetworkInfo.DetailedState;
import android.net.LinkProperties;
import android.widget.TextView;

/**
 * Ethernet interfaces configuration GUI
 *
 */
public class EthernetSettings extends SettingsPreferenceFragment {

    private static final String TAG = "EthernetSettings";

    private TIEthernetManager mEthManager;

    private String mIfaceToConfigure = null;

    private final IntentFilter mFilter;
    private final BroadcastReceiver mReceiver;

    private TextView mEmptyView;

    private int messageId;

    public EthernetSettings() {

        messageId = 0;

        mFilter = new IntentFilter();
        mFilter.addAction(TIEthernetManager.ETH_INTERFACE_STATE_CHANGED_ACTION);
        mFilter.addAction(TIEthernetManager.ETH_GLOBAL_STATE_CHANGED_ACTION);
        mFilter.addAction(TIEthernetManager.ETH_LINK_CONFIGURATION_CHANGED_ACTION);

        mReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                Log.e(TAG, "Intent received " + intent.getAction());
                handleEthernetEvent(context, intent);
            }
        };
    }

    @Override
    public void onCreate(Bundle bundle) {
        super.onCreate(bundle);
        mIfaceToConfigure = null;
        mEthManager = (TIEthernetManager) getSystemService(Context.ETHERNET_SERVICE);
        // Load ethernet preference from an XML resource
        addPreferencesFromResource(R.xml.ethernet_settings);
        getActivity().registerReceiver(mReceiver, mFilter);
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        mEmptyView = (TextView) getView().findViewById(android.R.id.empty);
        mEmptyView.setText(R.string.ethernet_empty_list_ethernet_off);
        getListView().setEmptyView(mEmptyView);
    }

    @Override
    public void onResume() {
        super.onResume();
        getActivity().registerReceiver(mReceiver, mFilter);
        refreshView();
    }

    @Override
    public void onPause() {
        super.onPause();
        getActivity().unregisterReceiver(mReceiver);
    }

    @Override
    public boolean onPreferenceTreeClick(PreferenceScreen preferenceScreen, Preference preference) {
        if (preference instanceof EthernetInterfacePref) {
            EthernetInterfacePref p = (EthernetInterfacePref) preference;
        } else {
            return super.onPreferenceTreeClick(preferenceScreen, preference);
        }

        return true;
    }

    private void updateIfacesList() {
        EthernetInterfacePref pref = new EthernetInterfacePref(getActivity());
        boolean foundSmth = false;
        getPreferenceScreen().removeAll();
        for (String iface : mEthManager.getIfaces()) {
            pref.SetIfaceName(iface);
            mIfaceToConfigure = iface;
            foundSmth = true;
        }
        // Show last found interface only
        if (foundSmth) {
            // Get current connection status
            ConnectivityManager connMgr = (ConnectivityManager) this.getSystemService(Context.CONNECTIVITY_SERVICE);
            NetworkInfo networkInfo = connMgr.getNetworkInfo(ConnectivityManager.TYPE_ETHERNET);
            LinkProperties link = connMgr.getLinkProperties(ConnectivityManager.TYPE_ETHERNET);
            if (link.getInterfaceName().equals(mIfaceToConfigure)) {
                // Set current status
                pref.SetIfaceStatus(networkInfo.getDetailedState());
            }
            // Add pereference
            getPreferenceScreen().addPreference(pref);
        } else {
            mEmptyView.setText(R.string.ethernet_interface_not_found);
        }
    }

    private void updateIfaceProperties(LinkProperties prop) {

        String iface = prop.getInterfaceName();
        if (iface.equals(mIfaceToConfigure)) {
            EthernetInterfacePref pref = (EthernetInterfacePref) getPreferenceScreen().findPreference(iface);
            pref.SetLinkProperties(prop);
        }
    }

    private void handleEthernetEvent(Context context, Intent intent) {
        String action = intent.getAction();

        if (TIEthernetManager.ETH_INTERFACE_STATE_CHANGED_ACTION.equals(action)) {
            if (null != mEthManager) {
                if (mEthManager.getEthernetState()) {
                    NetworkInfo    info = (NetworkInfo) intent.getExtra(TIEthernetManager.EXTRA_NETWORK_INFO);
                    LinkProperties prop = (LinkProperties) intent.getExtra(TIEthernetManager.EXTRA_LINK_PROPERTIES);
                    refreshView();
                    updateIfaceProperties(prop);
                }
            }
        } else if (TIEthernetManager.ETH_LINK_CONFIGURATION_CHANGED_ACTION.equals(action)) {
            if (null != mEthManager) {
                if (mEthManager.getEthernetState()) {
                    LinkProperties prop = (LinkProperties) intent.getExtra(TIEthernetManager.EXTRA_LINK_PROPERTIES);
                    refreshView();
                    updateIfaceProperties(prop);
                }
            }
        } else if (TIEthernetManager.ETH_GLOBAL_STATE_CHANGED_ACTION.equals(action)) {
            refreshView();
        }
    }

    private void refreshView() {
        if (null != mEthManager) {
            if (!mEthManager.getEthernetState()) {
                getPreferenceScreen().removeAll();
                mEmptyView.setText(R.string.ethernet_empty_list_ethernet_off);
            } else {
                updateIfacesList();
            }
        }
    }
}
