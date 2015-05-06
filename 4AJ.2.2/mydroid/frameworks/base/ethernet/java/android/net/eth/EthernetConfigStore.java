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

import android.net.NetworkUtils;
import android.os.Environment;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.Log;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.EOFException;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.FileNotFoundException;
import java.net.InetAddress;
import java.util.ArrayList;

import android.net.eth.EthernetConfiguration.IPAssignment;

/**
 * This class provides the API to manage configured
 * ethernet interfaces. The API is not thread safe.
 *
 * It deals with the following
 * - Add/update/remove a EthernetConfiguration
 *   The configuration contains two types of information.
 *     = IP configuration that is handled by EthernetConfigStore and
 *       saved to disk on change.
 *
 *       The format of configuration file is as follows:
 *       <version>
 *       <netA_key1><netA_value1><netA_key2><netA_value2>...<EOS>
 *       <netB_key1><netB_value1><netB_key2><netB_value2>...<EOS>
 *       ..
 *
 *       (key, value) pairs for a given interface are grouped together and can
 *       be in any order. A EOS at the end of a set of (key, value) pairs
 *       indicates that the next set of (key, value) pairs are for a new
 *       interface. A interface is identified by a unique interface name. If there is no
 *       interface name in the (key, value) pairs, the data is discarded
 *       and null returned on config request
 *
 *       Any failures during read or write to the configuration file are ignored
 *       without reporting to the user since the likelihood of these errors are
 *       low and the impact on connectivity is low.
 * @hide
 */

public class EthernetConfigStore {

    private static final String TAG = "EthernetConfigStore";
    private static final boolean DBG = true;

    private static final String ethConfigFile = Environment.getDataDirectory() + "/misc/eth/ethconfig.cfg";

    /* IP and proxy configuration keys */
    private static final String IFACE_ADDR_KEY    = "ifaceAddr";
    private static final String IFACE_PREFIX_KEY  = "prefix";
    private static final String IFACE_NAME_KEY    = "ifaceName";
    private static final String IP_ASSIGNMENT_KEY = "ipAssignment";
    private static final String GATEWAY_KEY       = "gateway";
    private static final String DNS_KEY           = "dns";
    private static final String EOS               = "eos";

    /**
     * Get interface persistent configuration
     *
     * @param ifaceName interface name to retrieve
     *
     * @return {@link android.net.eth.EthernetConfiguration EthernetConfiguration} object
     * or null if configuration doesnt found
     */
    public static EthernetConfiguration readConfiguration(String ifaceName) {
        ArrayList<EthernetConfiguration> confs = readConfigurations(ifaceName);
        if (confs.size() > 0) {
            return confs.get(0);
        } else {
            return null;
        }
    }

    /**
     * Get stored interfaces configurations
     * @param  ifaceName interface name to retrieve. Null to retrive all
     * available interfaces configurations
     * @return List of {@link  android.net.eth.EthernetConfiguration EthernetConfiguration}
     * objects (May be empty)
     */
    public static ArrayList<EthernetConfiguration> readConfigurations(String ifaceName) {

        ArrayList<EthernetConfiguration> configs = new ArrayList<EthernetConfiguration>();

        DataInputStream in = null;
        try {
            in = new DataInputStream(new BufferedInputStream(new FileInputStream(ethConfigFile)));

            while (true) {
                EthernetConfiguration ethConf = new EthernetConfiguration();
                String key;

                do {
                    key = in.readUTF();
                    try {
                        if (key.equals(IFACE_NAME_KEY)) {
                            ethConf.setIfaceName(in.readUTF());
                        } else if (key.equals(IP_ASSIGNMENT_KEY)) {
                            ethConf.setIpAssignment(IPAssignment.valueOf(in.readUTF()));
                        } else if (key.equals(IFACE_ADDR_KEY)) {
                            ethConf.setIfaceAddress(NetworkUtils.numericToInetAddress(in.readUTF()));
                        } else if (key.equals(IFACE_PREFIX_KEY)) {
                            ethConf.setPrefixLength(in.readInt());
                        } else if (key.equals(GATEWAY_KEY)) {
                            ethConf.setGateway(NetworkUtils.numericToInetAddress(in.readUTF()));
                        } else if (key.equals(DNS_KEY)) {
                            ethConf.setDNSaddress(NetworkUtils.numericToInetAddress(in.readUTF()));
                        } else if (key.equals(EOS)) {
                            break;
                        } else {
                            Log.d(TAG, "Ignore unknown key " + key + "while reading ethernet config");
                        }
                    } catch (IllegalArgumentException e) {
                        Log.d(TAG, "Ignore invalid value while reading ethernet config", e);
                    }
                } while (true);

                if (null == ifaceName) {
                    // Nothing to search. Gather all available interfaces
                    configs.add(ethConf);
                } else if (ethConf.getIfaceName().equals(ifaceName)) {
                    // Correponding interface configuration has been found.
                    // Stop searching.
                    configs.add(ethConf);
                    break;
                }
            }
        } catch (EOFException ignore) {
        } catch (FileNotFoundException ignore) {
            // Config file is absent yet. Not an error. Ignoring and returning empty list.
        } catch (IOException e) {
            Log.d(TAG, "Error parsing ethernet configuration", e);
        } finally {
            if (in != null) {
                try {
                    in.close();
                } catch (Exception e) {}
            }
        }

        return configs;
    }

    /**
     * Is interface configuration exists in the store
     * @return true if interface configuration exists in the store
     */
    public static boolean isInterfaceConfigured(String ifname) {
        return null != readConfiguration(ifname);
    }

    public static void addOrUpdateConfiguration(EthernetConfiguration ec) {

        if (null == ec)
            return;

        // Read all previous stored configs
        ArrayList<EthernetConfiguration> configs = readConfigurations(null);
        if (configs.size() == 0) {
            // No configs saved yet just add first one
            Log.d(TAG, "Adding config of " + ec.getIfaceName());
                Log.d(TAG, ec.toString());
            configs.add(ec);
        } else {
            // Update corresponding config and write all configs back to store
            int idx = configs.indexOf(ec);
            if (idx == -1) {
                Log.d(TAG, "Adding configuration of " + ec.getIfaceName());
                Log.d(TAG, ec.toString());
                configs.add(ec);
            } else {
                Log.d(TAG, "Updating configuration of " + ec.getIfaceName());
                Log.d(TAG, ec.toString());
                configs.set(idx, ec);
            }
        }
        writeConfig(configs);
    }

    private static void writeConfig(ArrayList<EthernetConfiguration> configsList) {

        DataOutputStream out = null;
        try {
            out = new DataOutputStream(new BufferedOutputStream(new FileOutputStream(ethConfigFile, false)));
            for (EthernetConfiguration config : configsList) {
                try {
                    IPAssignment ipAssignment = config.getIpAssignment();
                    switch (ipAssignment) {
                        case STATIC:
                            out.writeUTF(IFACE_NAME_KEY);
                            out.writeUTF(config.getIfaceName());

                            out.writeUTF(IP_ASSIGNMENT_KEY);
                            out.writeUTF(config.getIpAssignment().toString());

                            out.writeUTF(IFACE_ADDR_KEY);
                            out.writeUTF(config.getIfaceAddress().getHostAddress());

                            out.writeUTF(IFACE_PREFIX_KEY);
                            out.writeInt(config.getPrefixLength());

                            out.writeUTF(GATEWAY_KEY);
                            out.writeUTF(config.getGateway().getHostAddress());

                            for (InetAddress dnsAddr : config.getDNSaddresses()) {
                                out.writeUTF(DNS_KEY);
                                out.writeUTF(dnsAddr.getHostAddress());
                            }
                            break;
                        case DHCP:
                        case UNASSIGNED:
                            out.writeUTF(IFACE_NAME_KEY);
                            out.writeUTF(config.getIfaceName());

                            out.writeUTF(IP_ASSIGNMENT_KEY);
                            out.writeUTF(config.getIpAssignment().toString());
                            break;
                        default:
                            Log.d(TAG, "Ignore invalid ip assignment while writing ethernet configuration");
                            break;
                    }
                } catch (NullPointerException e) {
                    Log.d(TAG, "Failure in ethernet configuration writing - " + config.getIfaceName(), e);
                }
                out.writeUTF(EOS);
            }

        } catch (IOException e) {
            Log.d(TAG, "Error writing ethernet configuration data file", e);
        } finally {
            if (out != null) {
                try {
                    out.close();
                } catch (Exception e) {}
            }
        }
    }
}
