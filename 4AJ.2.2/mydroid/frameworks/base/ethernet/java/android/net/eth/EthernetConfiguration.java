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

import android.util.Log;
import android.os.Parcelable;
import android.os.Parcel;
import android.text.TextUtils;

import java.net.InetAddress;
import java.net.UnknownHostException;
import java.util.ArrayList;
import java.util.List;

/**
 * A class which represents persistent configuration of the ethernet interface
 * @hide
 */
public class EthernetConfiguration implements Parcelable {

    private static final String TAG = "EthernetConfiguration";

    private InetAddress ifaceAddress;

    private InetAddress gateway;

    private List<InetAddress> DNSaddresses;

    private IPAssignment ipAssignment;

    private String ifaceName;

    private int prefixLength;

    public enum IPAssignment {
        /* Use statically configured IP settings. Configuration can be accessed
         * with linkProperties */
        STATIC,
        /* Use dynamically configured IP settigns */
        DHCP,
        /* Initial value*/
        UNASSIGNED
    };


    public EthernetConfiguration() {
        ifaceAddress = null;
        gateway = null;
        DNSaddresses = new ArrayList<InetAddress>();
        ipAssignment = IPAssignment.UNASSIGNED;
        ifaceName = null;
        prefixLength = 0;
    }

    @Override
    public String toString() {
        StringBuilder sbuilder = new StringBuilder();

        sbuilder.append("Interface name: ").append(this.ifaceName).append('\n');
        sbuilder.append("Interface address : ").append(null == this.ifaceAddress ? "NONE" : this.ifaceAddress.toString()).append('\n');
        sbuilder.append("Prefix length: ").append(this.prefixLength).append("\n");
        sbuilder.append("IP assignment: ").append(ipAssignment.toString()).append("\n");
        sbuilder.append("Gateway: ").append(null == this.gateway ? "NONE" : gateway.toString()).append("\n");
        sbuilder.append("DNS addresses: ").append(DNSaddresses.size() == 0 ? "NONE" : "").append("\n");
        for (InetAddress addr : DNSaddresses) {
            sbuilder.append("   ").append(addr).append("\n");
        }

        return sbuilder.toString();
    }

    /** Implement the Parcelable interface */
    public int describeContents() {
        return 0;
    }

    /** copy constructor */
    public EthernetConfiguration(EthernetConfiguration source) {
        if (source != null) {
            this.ifaceAddress = source.ifaceAddress;
            this.prefixLength = source.prefixLength;
            this.ifaceName    = source.ifaceName;
            this.gateway      = source.gateway;
            this.ipAssignment = source.ipAssignment;
            this.DNSaddresses = new ArrayList<InetAddress>();
            for (InetAddress addr : source.getDNSaddresses())
                this.DNSaddresses.add(addr);
        }
    }

    /** Implement the Parcelable interface */
    public void writeToParcel(Parcel dest, int flags) {

        dest.writeString(ifaceName);
        dest.writeString(ipAssignment.name());
        if (IPAssignment.STATIC == ipAssignment) {
            dest.writeByteArray(ifaceAddress.getAddress());
            dest.writeInt(prefixLength);
            dest.writeByteArray(gateway.getAddress());

            dest.writeInt(DNSaddresses.size());
            for (InetAddress addr : DNSaddresses)
                dest.writeByteArray(addr.getAddress());
        }
    }

    /** Implement the Parcelable interface */
    public static final Creator<EthernetConfiguration> CREATOR =
        new Creator<EthernetConfiguration>() {
            public EthernetConfiguration createFromParcel(Parcel in) {
                EthernetConfiguration config = new EthernetConfiguration();

                try {
                    config.setIfaceName(in.readString());
                    config.setIpAssignment(IPAssignment.valueOf(in.readString()));
                    if (IPAssignment.STATIC == config.getIpAssignment()) {
                        config.setIfaceAddress(InetAddress.getByAddress(in.createByteArray()));
                        config.setPrefixLength(in.readInt());
                        config.setGateway(InetAddress.getByAddress(in.createByteArray()));

                        int addressCount = in.readInt();
                        for (int i = 0; i < addressCount; i++) {
                            config.setDNSaddress(InetAddress.getByAddress(in.createByteArray()));
                        }
                    }
                } catch (UnknownHostException e) {
                    Log.e(TAG, "Exception during reading ethernet DNS records", e);
                }

                return config;
            }

            public EthernetConfiguration[] newArray(int size) {
                return new EthernetConfiguration[size];
            }
        };

    /**
     * Overriding Object.hashCode() method
     * @see java.lang.Object#hashCode()
     */
    @Override
    public int hashCode() {
        return this.ifaceName.hashCode();
    }

    /**
     * Overriding Object.equals() method
     * @see java.lang.Object#equals(java.lang.Object)
     */
    @Override
    public boolean equals(Object obj) {
        if (this == obj)
            return true;
        if (obj == null)
            return false;
        if (getClass() != obj.getClass())
            return false;
        final EthernetConfiguration other = (EthernetConfiguration) obj;

        if (!TextUtils.isEmpty(this.ifaceName) && !TextUtils.isEmpty(other.getIfaceName())) {
            // Configurations with same iface name are same
            return this.ifaceName.equals(other.getIfaceName());
        }

        return false;
    }

    public List<InetAddress> getDNSaddresses() {
        return this.DNSaddresses;
    }

    public void setDNSaddresses(ArrayList<InetAddress> DNSaddresses) {
        this.DNSaddresses = DNSaddresses;
    }

    public void setDNSaddress(InetAddress addr) {
        if (null != addr)
            this.DNSaddresses.add(addr);
    }

    public void setDNSaddresses(InetAddress dnsAddr) {
        this.DNSaddresses = DNSaddresses;
    }

    public InetAddress getGateway() {
        return this.gateway;
    }

    public void setGateway(InetAddress gateway) {
        this.gateway = gateway;
    }

    public InetAddress getIfaceAddress() {
        return this.ifaceAddress;
    }

    public void setIfaceAddress(InetAddress ifaceAddress) {
        this.ifaceAddress = ifaceAddress;
    }

    public String getIfaceName() {
        return ifaceName;
    }

    public void setIfaceName(String ifaceName) {
        this.ifaceName = ifaceName;
    }

    public IPAssignment getIpAssignment() {
        return this.ipAssignment;
    }

    public void setIpAssignment(IPAssignment ipAssignment) {
        this.ipAssignment = ipAssignment;
    }

    public int getPrefixLength() {
        return this.prefixLength;
    }

    public void setPrefixLength(int prefixLength) {
        this.prefixLength = prefixLength;
    }
}
