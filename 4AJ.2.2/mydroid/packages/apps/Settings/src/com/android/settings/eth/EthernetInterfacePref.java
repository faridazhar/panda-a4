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

import android.app.AlertDialog;
import android.preference.DialogPreference;
import android.content.Context;
import android.content.DialogInterface;
import android.net.eth.EthernetConfiguration;
import android.net.eth.EthernetConfiguration.IPAssignment;
import android.net.eth.TIEthernetManager;
import android.net.LinkProperties;
import android.net.NetworkInfo;
import android.net.NetworkInfo.DetailedState;
import android.net.NetworkUtils;
import android.os.Bundle;
import android.os.Handler;
import android.text.Editable;
import android.text.TextWatcher;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.Log;
import android.view.View;
import android.widget.AdapterView;
import android.widget.Button;
import android.widget.Spinner;
import android.widget.TextView;

import java.net.InetAddress;
import java.util.List;

public class EthernetInterfacePref extends DialogPreference
                                   implements AdapterView.OnItemSelectedListener, TextWatcher {

    private static final String TAG = "EthernetInterfacePref";

    /* This value comes from "ethernet_ip_settings" resource array */
    private static final int DHCP      = 0;
    private static final int STATIC_IP = 1;

    private String mIfaceName = null;
    private Context mContext;
    private TIEthernetManager mEthManager;
    private EthernetConfiguration mEthConf;

    private LinkProperties mLinkProperties;
    // UI controls
    private Spinner mIpSettingsSpinner;
    private View mView;
    private TextView mIpAddressView;
    private TextView mGatewayView;
    private TextView mNetworkPrefixLengthView;
    private TextView mDns1View;
    private TextView mDns2View;

    private final Handler mTextViewChangedHandler;

    public EthernetInterfacePref(Context context) {
        super(context, null);
        super.setDialogLayoutResource(R.layout.ethernet_dialog);
        super.setDialogIcon(R.drawable.ic_settings_ethernet);
        this.mTextViewChangedHandler = new Handler();
        this.mContext = context;
        init();
    }

    public EthernetInterfacePref(Context context, AttributeSet attrs) {
        super(context, attrs);
        super.setDialogLayoutResource(R.layout.ethernet_dialog);
        super.setDialogIcon(R.drawable.ic_settings_ethernet);
        this.mTextViewChangedHandler = new Handler();
        this.mContext = context;
        init();
    }

    public EthernetInterfacePref(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        super.setDialogLayoutResource(R.layout.ethernet_dialog);
        super.setDialogIcon(R.drawable.ic_settings_ethernet);
        this.mTextViewChangedHandler = new Handler();
        this.mContext = context;
        init();
    }

    private void init() {
        setPositiveButtonText(mContext.getString(R.string.ethernet_apply_button));
        mEthManager = (TIEthernetManager) mContext.getSystemService(Context.ETHERNET_SERVICE);
        mLinkProperties = new LinkProperties();
    }

    public void SetIfaceName(String name) {
        super.setDialogTitle(name);
        super.setTitle(name);
        // Set pref key based on interface name
        // Will be used to quick access to corresponding preference
        super.setKey(name);
        this.mIfaceName = name;
        this.mLinkProperties.setInterfaceName(name);
    }

    public String GetIfaceName() {
        return mIfaceName;
    }

    public void SetIfaceStatus(DetailedState status) {
        if (DetailedState.CONNECTING == status) {
            super.setSummary(mContext.getString(R.string.ethernet_connecting_message));
        } else if (DetailedState.DISCONNECTING == status) {
            super.setSummary(mContext.getString(R.string.ethernet_disconnecting_message));
        } else if (DetailedState.CONNECTED == status) {
            super.setSummary(mContext.getString(R.string.ethernet_connected_message));
        } else if (DetailedState.DISCONNECTED == status) {
            super.setSummary(mContext.getString(R.string.ethernet_disconnected_message));
        } else if (DetailedState.OBTAINING_IPADDR == status) {
            super.setSummary(mContext.getString(R.string.ethernet_obtaining_ip_addr_message));
        } else if (DetailedState.FAILED == status) {
            super.setSummary(mContext.getString(R.string.ethernet_fail_message));
        } else {
           super.setSummary(status.toString());
       }
    }

    public void SetLinkProperties(LinkProperties prop) {
        mLinkProperties = prop;
    }

    @Override
    protected View onCreateDialogView () {
        mView = super.onCreateDialogView();

        // Read corresponding persistent configuration
        mEthConf = mEthManager.getConfiguredIface(mIfaceName);

        mIpSettingsSpinner = (Spinner) mView.findViewById(R.id.ip_settings);
        mIpAddressView = (TextView) mView.findViewById(R.id.ipaddress);
        mGatewayView = (TextView) mView.findViewById(R.id.gateway);
        mNetworkPrefixLengthView = (TextView) mView.findViewById(R.id.network_prefix_length);
        mDns1View = (TextView) mView.findViewById(R.id.dns1);
        mDns2View = (TextView) mView.findViewById(R.id.dns2);

        // If interface has been already initialized show saved preferences
        if (null != mEthConf) {
            IPAssignment assign = mEthConf.getIpAssignment();
            if (IPAssignment.DHCP == assign) {
                mIpSettingsSpinner.setSelection(DHCP);
            } else if (IPAssignment.STATIC == assign) {
                mIpSettingsSpinner.setSelection(STATIC_IP);
                mIpAddressView.setText(mEthConf.getIfaceAddress().getHostAddress());

                mGatewayView.setText(mEthConf.getGateway().getHostAddress());

                mNetworkPrefixLengthView.setText(Integer.toString(mEthConf.getPrefixLength()));

                List<InetAddress> dnses = mEthConf.getDNSaddresses();

                if (dnses.size() > 0)
                    mDns1View.setText(dnses.get(0).getHostAddress());
                if (dnses.size() > 1)
                    mDns2View.setText(dnses.get(1).getHostAddress());
            }
        }

        // Initialize listeners
        mIpSettingsSpinner.setOnItemSelectedListener(this);
        mIpAddressView.addTextChangedListener(this);
        mGatewayView.addTextChangedListener(this);
        mNetworkPrefixLengthView.addTextChangedListener(this);
        mDns1View.addTextChangedListener(this);
        mDns2View.addTextChangedListener(this);

        return mView;
    }

    /**
     * Spinner selection listener
     */
    @Override
    public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
        if (parent == mIpSettingsSpinner) {
            showIpConfigFields();
            enableApplyButton();
        }
    }

    /**
     * Spinner selection listener
     */
    @Override
    public void onNothingSelected(AdapterView<?> parent) {
        enableApplyButton();
    }

    /**
     * Dialog buttons clicks listener
     */
    @Override
    public void onClick(DialogInterface dialog, int which) {
        if (DialogInterface.BUTTON_POSITIVE == which) {
            // If Apply button enabled then configuration has already validated
            EthernetConfiguration ec = new EthernetConfiguration();
            Log.e(TAG, "Set Conf iface name " + mIfaceName);
            ec.setIfaceName(mIfaceName);
            if (STATIC_IP == mIpSettingsSpinner.getSelectedItemPosition()) {
                ec.setIpAssignment(IPAssignment.STATIC);
                ec.setIfaceAddress(NetworkUtils.numericToInetAddress(mIpAddressView.getText().toString()));
                ec.setGateway(NetworkUtils.numericToInetAddress(mGatewayView.getText().toString()));
                ec.setPrefixLength(Integer.parseInt(mNetworkPrefixLengthView.getText().toString()));
                String dns = mDns1View.getText().toString();
                if (!TextUtils.isEmpty(dns)) {
                    ec.setDNSaddress(NetworkUtils.numericToInetAddress(dns));
                }
                dns = mDns2View.getText().toString();
                if (!TextUtils.isEmpty(dns)) {
                    ec.setDNSaddress(NetworkUtils.numericToInetAddress(dns));
                }
            } else if (DHCP == mIpSettingsSpinner.getSelectedItemPosition()) {
                ec.setIpAssignment(IPAssignment.DHCP);
            }
            mEthConf = ec;
            mEthManager.updateIfaceConfiguration(ec);
        } else if (which == DialogInterface.BUTTON_NEGATIVE){
            // Do nothing
        }
    }

    private void showIpConfigFields() {
        mView.findViewById(R.id.ip_fields).setVisibility(View.VISIBLE);
        if (mIpSettingsSpinner.getSelectedItemPosition() == STATIC_IP) {
            mView.findViewById(R.id.staticip).setVisibility(View.VISIBLE);
        } else {
            mView.findViewById(R.id.staticip).setVisibility(View.GONE);
        }
    }

    /*
     * Check if all IP fields are valid
     */
    private boolean ipFieldsAreValid() {
        boolean bResult = false;
        if (mIpSettingsSpinner.getSelectedItemPosition() == STATIC_IP) {
            int i = validateIpConfigFields();
            if (i == 0) {
                bResult = true;
            }
        } else if (mIpSettingsSpinner.getSelectedItemPosition() == DHCP) {
            bResult = true;
        }

        return bResult;
    }

    /**
     * IP config fields validation
     * @return zero in case of success and resource ID in case of some invalid fields
     */
    private int validateIpConfigFields() {
        if (mIpAddressView == null)
            return R.string.ethernet_ip_settings_invalid_ip_address;

        String ipAddr = mIpAddressView.getText().toString();
        if (TextUtils.isEmpty(ipAddr))
            return R.string.ethernet_ip_settings_invalid_ip_address;

        InetAddress inetAddr = null;
        try {
            inetAddr = NetworkUtils.numericToInetAddress(ipAddr);
        } catch (IllegalArgumentException e) {
            return R.string.ethernet_ip_settings_invalid_ip_address;
        }

        int networkPrefixLength = -1;
        try {
            networkPrefixLength = Integer.parseInt(mNetworkPrefixLengthView.getText().toString());
            if (networkPrefixLength < 0 || networkPrefixLength > 32) {
                return R.string.ethernet_ip_settings_invalid_network_prefix_length;
            }
        } catch (NumberFormatException e) {
            return R.string.ethernet_ip_settings_invalid_network_prefix_length;
        }

        String gateway = mGatewayView.getText().toString();
        if (TextUtils.isEmpty(gateway)) {
            // If gateway field still empty
            // set gateway hint based on net mask and static IP already filled
            try {
                //Extract a default gateway from IP address
                InetAddress netPart = NetworkUtils.getNetworkPart(inetAddr, networkPrefixLength);
                byte[] addr = netPart.getAddress();
                addr[addr.length-1] = 1;
                mGatewayView.setHint(InetAddress.getByAddress(addr).getHostAddress());
            } catch (RuntimeException ee) {
                return R.string.ethernet_ip_settings_invalid_ip_address;
            } catch (java.net.UnknownHostException u) {
                return R.string.ethernet_ip_settings_invalid_ip_address;
            }
        } else {
            InetAddress gatewayAddr = null;
            try {
                gatewayAddr = NetworkUtils.numericToInetAddress(gateway);
            } catch (IllegalArgumentException e) {
                return R.string.ethernet_ip_settings_invalid_gateway;
            }
        }

        String dns1 = mDns1View.getText().toString();
        InetAddress dnsAddr = null;

        if (TextUtils.isEmpty(dns1)) {
            //If everything else is valid, provide hint as a default option
            mDns1View.setHint(mContext.getString(R.string.ethernet_dns1_hint));
        } else {
            try {
                dnsAddr = NetworkUtils.numericToInetAddress(dns1);
            } catch (IllegalArgumentException e) {
                return R.string.ethernet_ip_settings_invalid_dns;
            }
        }

        String dns2 = mDns2View.getText().toString();
        if (TextUtils.isEmpty(dns2)) {
            //If everything else is valid, provide hint as a default option
            mDns2View.setHint(mContext.getString(R.string.ethernet_dns2_hint));
        } else {
            try {
                dnsAddr = NetworkUtils.numericToInetAddress(dns2);
            } catch (IllegalArgumentException e) {
                return R.string.ethernet_ip_settings_invalid_dns;
            }
        }

        // Check whether both dns fields are empty
        if (TextUtils.isEmpty(dns1) && TextUtils.isEmpty(dns2)) {
            return R.string.ethernet_ip_settings_invalid_dns;
        }

        return 0;
    }

    /**
     * Enable Apply button if ip settings are valid
     **/
    void enableApplyButton() {
        AlertDialog dialog = (AlertDialog) getDialog();
        Button button = dialog.getButton(AlertDialog.BUTTON_POSITIVE);
        button.setEnabled(ipFieldsAreValid());
    }

    /**
     * Implementing TextWatcher interface
     */
    @Override
    public void afterTextChanged(Editable s) {
        enableApplyButton();
    }

    /**
     * Implementing TextWatcher interface
     */
    @Override
    public void beforeTextChanged(CharSequence s, int start, int count, int after) {
    }

    /**
     * Implementing TextWatcher interface
     */
    @Override
    public void onTextChanged(CharSequence s, int start, int before, int count) {
    }
}
