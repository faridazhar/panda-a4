/*
 * Copyright (C) 2012 Texas Instruments Corporation
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

package android.server;

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map.Entry;
import java.util.Set;
import java.util.SortedSet;
import java.util.TreeSet;
import java.util.concurrent.ConcurrentHashMap;

import android.app.Service;
import android.content.ComponentName;
import android.content.Intent;
import android.content.Context;
import android.content.ServiceConnection;
import android.os.AsyncTask;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;
import android.os.Looper;
import android.os.Message;
import android.os.PowerManager;
import android.os.RemoteCallbackList;
import android.content.BroadcastReceiver;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.pm.ServiceInfo;
import android.os.RemoteException;
import android.provider.Settings;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;

import com.google.android.collect.Sets;
import com.ti.bluetooth.gatt.IBluetoothGattServer;
import com.ti.bluetooth.gatt.IBluetoothGattServerCallback;
import com.ti.bluetooth.gatt.GattReadResult;
import com.ti.bluetooth.gatt.GattStatus;

import android.util.Log;
import android.widget.Toast;

public class BluetoothGattServerService extends IBluetoothGattServer.Stub {
    static final String TAG = "BluetoothGattServerService";
    static final boolean DBG = true;
    static final boolean VERBOSE = true;

    // service name
    public static final String BLUETOOTH_GATTSERVER_SERVICE = "bluetooth_gattserver";

    private static final String BLUETOOTH_ADMIN_PERM =
                android.Manifest.permission.BLUETOOTH_ADMIN;
    private static final String BLUETOOTH_PERM =
                android.Manifest.permission.BLUETOOTH;

    private static final String ACTION_ATT_PROFILE_SERVICE_START =
                "com.ti.bluetooth.gatt.action.ATT_PROFILE_SERVICE_START";

    private static final String TX_POWER_READ_ACTION =
                            "com.ti.bluetooth.gatt.txpower.action.TX_POWER_READ";
    private static final String TX_POWER_EXTRA_DEVICE =
                            "com.ti.bluetooth.gatt.txpower.extra.DEVICE";
    private static final String TX_POWER_EXTRA_LEVEL =
                            "com.ti.bluetooth.gatt.txpower.extra.LEVEL";

    // these may contain the package name in the "obj" parameter
    private static final int REFRESH_PROFILE_MSG = 1;
    private static final int FORCE_REFRESH_PROFILE_MSG = 2;

    private static final int BT_ON_MSG = 3;
    private static final int BT_OFF_MSG = 4;

    // these will contain the dyn profile unique name in the "obj" parameter
    private static final int ADD_DYN_PROFILE_MSG = 5;
    private static final int REMOVE_DYN_PROFILE_MSG = 6;

    // give dynamic BT profiles some time to register before starting the profiles
    private static final int BT_ON_DELAY_MS = 5000;

    private static final int BT_TOGGLE_TIMEOUT_MS = 30 * 1000;

    private static final int PROFILE_INIT_TIMEOUT_MS = 30 * 1000;

    // prefix for all settings put in Settings.Secure for this service
    static final String BLUETOOTH_GATT_SERVER_SETTINGS = "bluetooth_gatt_server_";

    // for JNI context
    private int mNativeData;

    // mapping between profiles Ids and profile data structures
    private ConcurrentHashMap<Integer, ProfileData> mProfileMap =
                new ConcurrentHashMap<Integer, ProfileData>();

    // mapping between handles and profile Ids. needed for read, write and
    // indication callbacks
    private ConcurrentHashMap<Integer, Integer> mHandleMap =
                            new ConcurrentHashMap<Integer, Integer>();

    private int mBluetoothState;
    private BluetoothService mBluetoothService;
    private boolean mProfilesRunning = false;
    private int mDisableDepth = 0;

    private PackageManager mPackageManager;

    // only used during profile registration (on BT on)
    private int mCurrentProfileId = 1;

    // event handling - for profiles
    private HandlerThread mHandlerThread = new HandlerThread("ProfileHandler");
    private ProfileHandler mHandler;

    // used for synchronization when changing BT state
    private Object mBtStateLock = new Object();
    private boolean mInternalBtStateChange = false;

    // used for indications
    private int mCookieCounter = 1;
    // map between cookies and profile-ids of the owning profiles
    private HashMap<Integer, Integer> mCookieMap =
                            new HashMap<Integer, Integer>();

    // wakelock for profiles init/deinit
    private PowerManager.WakeLock mWakeLock;

    private Context mContext;
    private BluetoothAdapter mAdapter;

    // dynamic GATT profiles
    private ConcurrentHashMap<String, IBluetoothGattServerCallback> mDynamicProfileMap =
            new ConcurrentHashMap<String, IBluetoothGattServerCallback>();

    static { classInitNative(); }

    public BluetoothGattServerService(Context context, BluetoothService bluetoothService) {
        if (DBG) Log.d(TAG, "Creating");

        initializeNativeDataNative();

        mContext = context;
        mBluetoothService = bluetoothService;

        IntentFilter btFilter = new IntentFilter();
        btFilter.addAction(BluetoothAdapter.ACTION_STATE_CHANGED);
        mContext.registerReceiver(mBtStateReceiver, btFilter);

        IntentFilter packageFilter = new IntentFilter();

        // watch for ATT profile packages
        packageFilter.addAction(Intent.ACTION_PACKAGE_ADDED);
        packageFilter.addAction(Intent.ACTION_PACKAGE_REMOVED);
        packageFilter.addCategory(Intent.CATEGORY_DEFAULT);
        packageFilter.addDataScheme("package");

        mAdapter = BluetoothAdapter.getDefaultAdapter();
        mBluetoothState = mAdapter.getState();

        mPackageManager = mContext.getPackageManager();

        mHandlerThread.start();
        mHandler = new ProfileHandler(mHandlerThread.getLooper());

        PowerManager pm = (PowerManager) mContext.getSystemService(Context.POWER_SERVICE);
        mWakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "GattServerWakeLock");

        // start profiles if bluetooth is on (we might have crashed before).
        // this will guarantee a serviceChanged if any dynamic profile exists
        if (mBluetoothState == BluetoothAdapter.STATE_ON) {
            mWakeLock.acquire();
            mHandler.sendEmptyMessage(BT_ON_MSG);
        }

        // start getting state changes
        mContext.registerReceiver(mPackageReceiver, packageFilter);
    }

    @Override
    protected void finalize() throws Throwable {
        if (DBG) Log.d(TAG, "finalize");
        mContext.unregisterReceiver(mBtStateReceiver);
        mContext.unregisterReceiver(mPackageReceiver);
        try {
            cleanupNativeDataNative();
        } finally {
            super.finalize();
        }
    }

    private BroadcastReceiver mBtStateReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            synchronized (mBtStateLock) {
                int curBluetoothState = intent.getIntExtra(
                        BluetoothAdapter.EXTRA_STATE,
                        BluetoothAdapter.STATE_OFF);
                if (mBluetoothState == curBluetoothState) {
                    if (DBG) Log.d(TAG, "bogus state change to: " + mBluetoothState);
                    return;
                }

                mBluetoothState = curBluetoothState;

                boolean stateChangeComplete = false;
                int what = (mBluetoothState == BluetoothAdapter.STATE_ON) ?
                        BT_ON_MSG : BT_OFF_MSG;
                // delay the BT_ON message to let profiles initialize
                int delay = (mBluetoothState == BluetoothAdapter.STATE_ON) ?
                        BT_ON_DELAY_MS : 0;
                if (mBluetoothState == BluetoothAdapter.STATE_ON ||
                    mBluetoothState == BluetoothAdapter.STATE_OFF) {
                    // we want to get notified about external state changes
                    if (!mInternalBtStateChange) {
                        mWakeLock.acquire();
                        mHandler.sendEmptyMessageDelayed(what, delay);
                    }
                    stateChangeComplete = true;
                }

                // notify the new state if installing/uninstalling profiles.
                // note the BT_ON_MSG/BT_OFF_MSG will only be received by the
                // handler after the install/uninstall is processed, but that's
                // ok.
                if (stateChangeComplete && mInternalBtStateChange)
                    mBtStateLock.notify();
            }
        }
    };

    private BroadcastReceiver mPackageReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();

            int prefixLen = "package:".length();
            String packageName = intent.getDataString().substring(prefixLen);
            if (action.equals(Intent.ACTION_PACKAGE_ADDED)) {
                if (DBG) Log.d(TAG, "package added: " + packageName);
                mWakeLock.acquire();
                Message msg = mHandler.obtainMessage(REFRESH_PROFILE_MSG,
                        packageName);
                mHandler.sendMessage(msg);
            } else if (action.equals(Intent.ACTION_PACKAGE_REMOVED)) {
                boolean replace = intent.getBooleanExtra(Intent.EXTRA_REPLACING,
                        false);
                // if this is a replacement - we will get a ACTION_PACKAGE_ADDED
                // soon. no need for this.
                if (replace)
                    return;

                if (DBG) Log.d(TAG, "package removed: " + packageName);
                mWakeLock.acquire();
                Message msg = mHandler.obtainMessage(REFRESH_PROFILE_MSG,
                        packageName);
                mHandler.sendMessage(msg);
            }
        }
    };

    class ProfileData implements IBinder.DeathRecipient {
        String mServiceName;
        String mPackageName;
        int mProfileId;
        HashMap<Integer,Integer> mStartHandles;
        boolean mDynamic;
        ArrayList<Integer> mSdpHandles;

        private ServiceConnection mConn;
        private IBluetoothGattServerCallback mService;

        // this means the profile has called endProfileInit()
        boolean mInitialized;

        ProfileData() { mInitialized = false; }

        // conn can be null for dynamic profiles
        synchronized boolean initConnection(ServiceConnection conn,
                    IBluetoothGattServerCallback srv) {
            mConn = conn;
            mService = srv;

            try {
                mService.asBinder().linkToDeath(this, 0);
            } catch (RemoteException e) {
                e.printStackTrace();
                return false;
            }

            return true;
        }

        @Override
        synchronized public void binderDied() {
            mService = null;
            mConn = null;
            profileDied(this);
        }
    }

    private class ProfileHandler extends Handler {
        public ProfileHandler(Looper looper) {
            super(looper);
        }

        @Override
        public void handleMessage(Message msg) {
            if (DBG) Log.d(TAG, "ProfileHandler got msg " + msg.what);
            boolean forceRefresh = false;

            switch (msg.what) {
            case ADD_DYN_PROFILE_MSG:
            case REMOVE_DYN_PROFILE_MSG:
                // if profiles are not running, no real change should be made,
                // only signal serviceChanged
                // TODO: we won't always need serviceChanged here when we have
                // persistency
                if (!mProfilesRunning) {
                    setServiceChanged();
                    break;
                }

                String name = (String)msg.obj;
                if (msg.what == ADD_DYN_PROFILE_MSG)
                    handleDynamicProfileStart(name);
                else
                    handleDynamicProfileStop(name);

                break;
            case FORCE_REFRESH_PROFILE_MSG:
                forceRefresh = true;
                // fall-through intentional
            case REFRESH_PROFILE_MSG:
                // only operate if one of our packages changed. note msg.obj
                // may be null here.
                if (!refreshProfiles((String)msg.obj) && !forceRefresh)
                    break; // make sure we release the wakelock

                // always turn off BT on profile change. note we don't
                // get the BT_ON_MSG/BT_OFF_MSG for these state changes, so
                // we call the appropriate actions manually
                if (mProfilesRunning && setBtState(false)) {
                    onBluetoothOff();
                    setBtState(true);

                    // we might have fail re-enabling BT. don't start the
                    // profiles in that case.
                    if (mBluetoothState == BluetoothAdapter.STATE_ON)
                        onBluetoothOn((String)msg.obj);
                }
                break;
            case BT_ON_MSG:
                mBluetoothState = mAdapter.getState();
                if(mBluetoothState != BluetoothAdapter.STATE_ON) {
                    if (DBG) Log.d(TAG, "BT_ON_MSG, while BT state=" + mBluetoothState);
                    onBluetoothOff();
                    break;
                }
                onBluetoothOn(null);
                break;
            case BT_OFF_MSG:
                onBluetoothOff();
                break;
            }

            // always release the wakelock here - it is always acquired before
            // sending a message to the handler
            mWakeLock.release();
        }
    }

    void handleDynamicProfileStart(String name) {
        if (DBG) Log.d(TAG, "starting late coming dynamic profile " + name);
        // TODO: if the profile is one of the existing dynamic profiles, and we are
        // during BT init, just start it. no serviceChanged needed.

        // TODO: if the profile is a new profile, start it, add it to the snapshot
        // (saving handles as well) and send serviceChanged

        startDynamicProfile(name);
        setServiceChanged();
    }

    void handleDynamicProfileStop(String name) {
        if (DBG) Log.d(TAG, "stopping early going dynamic profile " + name);
        ProfileData prof = getProfileForName(name);
        if (prof == null)
            return;

        stopProfile(prof, true);
        mProfileMap.remove(prof.mProfileId);

        setServiceChanged();

        // TODO
        // remove the profile from the persistent map
    }

    ProfileData getProfileForName(String name) {
        for (ProfileData p : mProfileMap.values()) {
            if (p.mServiceName.equals(name))
                return p;
        }

        return null;
    }

    // sets the BT state to the desired state and returns the previous state
    boolean setBtState(boolean state) {
        synchronized (mBtStateLock) {
            boolean prevState = (mAdapter.getState() == BluetoothAdapter.STATE_ON);
            boolean stateChangeStarted = false;

            // this variable signals the BroadcastReceiver that this is our
            // change. no need to send BT_ON_MSG/BT_OFF_MSG to the handler
            mInternalBtStateChange = true;

            if (DBG)
                Log.d(TAG, "current BT state " + prevState + " setting to " + state);
            if (!state && prevState)
                stateChangeStarted = mAdapter.disable();
            else if (state && !prevState)
                stateChangeStarted = mAdapter.enable();

            // wait for the actual disable/enable
            if (stateChangeStarted) {
                try {
                    mBtStateLock.wait(BT_TOGGLE_TIMEOUT_MS);
                } catch (InterruptedException e) { e.printStackTrace(); }
                if (DBG) Log.d(TAG, "BT state change completed");
            }

            mInternalBtStateChange = false;
            return prevState;
        }
    }

    /////////////////////// Storage related functions //////////////////////

    SortedSet<String> getSortedSet(String key) {
        SortedSet<String> resSet = new TreeSet<String>();
        int size = getInt(key + "size", 0);
        if (size == 0)
            return resSet;

        for (int i = 0; i < size; i++) {
            String value = getString(key + Integer.toString(i), null);
            if (value == null)
                continue;

            if (VERBOSE) Log.d(TAG, "get " + key + " idx " + i + " val " + value);
            resSet.add(value);
        }

        return resSet;
    }

    void putSortedSet(String key, SortedSet<String> origSet) {
        if (origSet == null || origSet.size() == 0) {
            // set 0 as the size if the collection
            putInt(key + "size", 0);
            return;
        }

        // put the numbers in order
        Iterator<String> it = origSet.iterator();
        int size = 0;
        for (int i = 0; it.hasNext(); i++) {
            String val = it.next();
            if (VERBOSE) Log.d(TAG, "set " + key + " idx " + i + " val " + val);
            putString(key + Integer.toString(i), val);
            size++;
        }

        putInt(key + "size", size);
    }

    void putString(String key, String value) {
        Settings.Secure.putString(mContext.getContentResolver(),
                BLUETOOTH_GATT_SERVER_SETTINGS + key, value);
    }

    String getString(String key, String defValue) {
        String value = Settings.Secure.getString(mContext.getContentResolver(),
                BLUETOOTH_GATT_SERVER_SETTINGS + key);
        if (value == null)
            return defValue;

        return value;
    }

    void putInt(String key, int value) {
        Settings.Secure.putInt(mContext.getContentResolver(),
                BLUETOOTH_GATT_SERVER_SETTINGS + key, value);
    }

    int getInt(String key, int defValue) {
        return Settings.Secure.getInt(mContext.getContentResolver(),
                BLUETOOTH_GATT_SERVER_SETTINGS + key, defValue);
    }

    void putBoolean(String key, boolean status) {
        putInt(key, status ? 1 : 0);
    }

    boolean getBoolean(String key, boolean defValue) {
        int result = getInt(key, defValue ? 1 : 0);
        return result != 0;
    }

    SortedSet<String> getInstalledAttServices() {
        return getSortedSet("profiles");
    }

    void putInstalledAttServices(SortedSet<String> profiles) {
        putSortedSet("profiles", profiles);
    }

    boolean getServiceChanged() {
        return getBoolean("serviceChanged", false);
    }

    void putServiceChanged(boolean value) {
        putBoolean("serviceChanged", value);
    }

    void putProfileBlackList(String serviceName, boolean status) {
        if (DBG)
            Log.d(TAG, "blacklist status change to " + status + " for service " +
                serviceName);
        putBoolean(serviceName + "blackList", status);
    }

    boolean getProfileBlacklist(String serviceName) {
        return getBoolean(serviceName + "blackList", false);
    }

    void clearProfileData(String serviceName) {
        putBoolean(serviceName + "blackList", false);
    }

    List<ResolveInfo> getPackageAttServices(String packageName) {
        // see if this package is an ATT profile package
        Intent intent = new Intent(ACTION_ATT_PROFILE_SERVICE_START);
        intent.setPackage(packageName);
        return mPackageManager.queryIntentServices(intent,
                            PackageManager.GET_RESOLVED_FILTER);
    }

    List<ResolveInfo> getExistingAttResolves() {
        Intent intent = new Intent(ACTION_ATT_PROFILE_SERVICE_START);
        return mPackageManager.queryIntentServices(intent,
                PackageManager.GET_RESOLVED_FILTER);
    }

    String getPackageForService(List<ResolveInfo> resolves, String svc) {
        for (ResolveInfo r : resolves) {
            if (r.serviceInfo.name.equals(svc))
                return r.serviceInfo.packageName;
        }

        return null;
    }

    // this includes services from resolves as well as dynamic profiles
    SortedSet<String> getExistingLegacyAttServices() {
        List<ResolveInfo> resolves = getExistingAttResolves();
        TreeSet<String> existingServices = new TreeSet<String>();
        for (ResolveInfo r : resolves)
            existingServices.add(r.serviceInfo.name);

        return existingServices;
    }

    SortedSet<String> getExistingDynamicAttServices() {
        TreeSet<String> existingServices = new TreeSet<String>();
        for (String s : mDynamicProfileMap.keySet())
            existingServices.add(s);

        return existingServices;
    }

    // changedPackage can be null.
    boolean refreshProfiles(String changedPackage) {
        if (DBG)
            Log.d(TAG, "refresh profiles start changed pacakge " + changedPackage);
            SortedSet<String> installedProfiles = getInstalledAttServices();
            SortedSet<String> existingProfiles = getExistingLegacyAttServices();
            if (existingProfiles.isEmpty()) {
            // nothing changed if we have no installed profiles
            if (installedProfiles.isEmpty())
                return false;

            // we have a change - last profile uninstalled
            if (DBG)
                Log.d(TAG, "no packages installed (probably some removed). setting" +
                    " serviceChanged");
            putInstalledAttServices(null);
            return true;
        }

        // remove stale packages
        List<ResolveInfo> resolves = getExistingAttResolves();
        boolean profilesChanged = false;
        Iterator<String> it = installedProfiles.iterator();
        while (it.hasNext()) {
            String prof = it.next();
            if (!existingProfiles.contains(prof)) {
                it.remove();
                clearProfileData(prof);
                profilesChanged = true;
                if (DBG) Log.d(TAG, "removed profile " + prof + " - serviceChanged");
                continue;
            }

            // this package has been changed - clear all profiles in it
            if (changedPackage != null) {
                String profPkg = getPackageForService(resolves, prof);
                if (profPkg != null && profPkg.equals(changedPackage)) {
                    clearProfileData(prof);
                    profilesChanged = true;
                    if (DBG) Log.d(TAG, "clear modified prof data " + prof + " - serviceChanged");
                }
            }
        }

        // add new packages
        it = existingProfiles.iterator();
        while (it.hasNext()) {
            String prof = it.next();
            if (!installedProfiles.contains(prof)) {
                installedProfiles.add(prof);
                profilesChanged = true;
                if (DBG) Log.d(TAG, "added profile " + prof + " - serviceChanged");
            }
        }

        // record new profiles if needed
    if (profilesChanged)
            putInstalledAttServices(installedProfiles);

        if (DBG)
            Log.d(TAG, "refresh profiles end changed pacakge " +
                         changedPackage + " serviceChanged " + profilesChanged);
        return profilesChanged;
    }

    void startLegacyProfiles(SortedSet<String> profiles) {
        List<ResolveInfo> resolves = getExistingAttResolves();
        // start profiles in order. blacklist a profile if start fails for any
        // reason.
        for (String prof : profiles) {
            if (getProfileBlacklist(prof)) {
                Log.e(TAG, "profile " + prof + " blacklisted - skip");
                continue;
            }

            boolean success = startLegacyProfile(getPackageForService(resolves, prof), prof);
            if (!success) {
                putProfileBlackList(prof, true);
                // this will toggle BT off/on to clear the blacklisted profile
                mWakeLock.acquire();
                mHandler.sendEmptyMessage(FORCE_REFRESH_PROFILE_MSG);
                break;
            }
        }
    }

    void startDyanmicProfiles(SortedSet<String> profiles) {
        boolean started = false;

        for (String p : profiles) {
            if (startDynamicProfile(p))
                started = true;
        }
    }

    private class GattServiceConnection implements ServiceConnection {
        ProfileData mProf;

        GattServiceConnection(ProfileData prof) {
            mProf = prof;
        }

        public void onServiceConnected(ComponentName className, IBinder service) {
            IBluetoothGattServerCallback srv = IBluetoothGattServerCallback.Stub.asInterface(service);

            if (DBG) Log.d(TAG, "Attached to component: " + className.getClassName());

            synchronized (mProf) {
                if (!mProf.initConnection(this, srv)) {
                    Log.e(TAG, "new ServiceConnection - failed initConnection");
                    return;
                }

                try {
                    srv.start(mProf.mProfileId, BluetoothGattServerService.this.asBinder());
                } catch (RemoteException e) {
                    e.printStackTrace();
                    // if this failed, we will never get endProfileInit()
                }
            }
        }

        public void onServiceDisconnected(ComponentName className) {
            if (DBG) Log.d(TAG, "Detached from component " + className);
            // the DeathRecipient will automatically clean everything
        }
    };

    boolean startLegacyProfileInternal(ProfileData prof) {
        synchronized(prof) {
            if (DBG) Log.d(TAG, "starting profile " + prof.mServiceName);

            // the onServiceConnected will come on the UI thread
            Intent intent = new Intent(ACTION_ATT_PROFILE_SERVICE_START);
            intent.setClassName(prof.mPackageName, prof.mServiceName);
            if (!mContext.bindService(intent, new GattServiceConnection(prof),
                                        Context.BIND_AUTO_CREATE)) {
                Log.e(TAG, "cannot bind to service " + prof.mServiceName);
                return false;
            }

            // wait for initialization to complete - we will be notified
            // when the profile calls endProfileInit()
            try {
                prof.wait(PROFILE_INIT_TIMEOUT_MS);
            } catch (InterruptedException e) { e.printStackTrace(); }

            if (!prof.mInitialized) {
                Log.e(TAG, "Timeout initializing profile " + prof.mServiceName);
                return false;
            }

            if (DBG) Log.d(TAG, "Initialization complete for profile " + prof.mServiceName);
            return true;
        }
    }

    boolean startDynamicProfile(String name) {
        // sanity check
        if (getProfileForName(name) != null) {
            Log.e(TAG, "cannot start already running (stale?) profile " + name);
            return false;
        }

        IBluetoothGattServerCallback srv = mDynamicProfileMap.get(name);
        if (srv == null)
            return false;

        final ProfileData p = new ProfileData();
        p.mPackageName = null;
        p.mServiceName = name;
        p.mProfileId = mCurrentProfileId;
        p.mStartHandles = new HashMap<Integer, Integer>();
        p.mSdpHandles = new ArrayList<Integer>();
        p.mDynamic = true;
        mProfileMap.put(mCurrentProfileId, p);

        synchronized (p) {
            if (!p.initConnection(null, srv)) {
                Log.e(TAG, "cannot init dynamic profile connection");
                mProfileMap.remove(p.mProfileId);
                return false;
            }

            // execute the start() in a new thread, so we can block for the finish
            new Thread(new Runnable() {
                public void run() {
                    try {
                        p.mService.start(p.mProfileId, BluetoothGattServerService.this.asBinder());
                    } catch (RemoteException e) {
                        e.printStackTrace();
                        // if this failed, we will never get endProfileInit()
                    }
                }
            }).start();

            // wait for initialization to complete - we will be notified
            // when the profile calls endProfileInit()
            try {
                p.wait(PROFILE_INIT_TIMEOUT_MS);
            } catch (InterruptedException e) { e.printStackTrace(); }

            if (!p.mInitialized) {
                Log.e(TAG, "Timeout initializing dynamic profile " + p.mServiceName);
                mProfileMap.remove(p.mProfileId);
                return false;
            }

            if (DBG) Log.d(TAG, "Initialization complete for dynamic profile " + p.mServiceName);
            mCurrentProfileId++;
            return true;
        }
    }

    boolean startLegacyProfile(String packageName, String serviceName) {
        ProfileData profileData = new ProfileData();

        profileData.mPackageName = packageName;
        profileData.mServiceName = serviceName;
        profileData.mProfileId = mCurrentProfileId;
        profileData.mStartHandles = new HashMap<Integer, Integer>();
        profileData.mSdpHandles = new ArrayList<Integer>();
        profileData.mDynamic = false;
        mProfileMap.put(mCurrentProfileId, profileData);

        if (!startLegacyProfileInternal(profileData)) {
            mProfileMap.remove(profileData.mProfileId);
            return false;
        }

        mCurrentProfileId++;
        return true;
    }

    void stopProfile(ProfileData prof, boolean btOperational) {
        if (DBG) Log.d(TAG, "Stopping profile " + prof.mServiceName);

        synchronized (prof) {
            try {
                if (prof.mService != null)
                    prof.mService.stop();
            } catch (RemoteException e) { e.printStackTrace(); }

            try {
                if (prof.mConn != null)
                    mContext.unbindService(prof.mConn);
            } catch (IllegalArgumentException e) { e.printStackTrace(); }
        }

        Iterator<Entry<Integer, Integer>> it = mHandleMap.entrySet().iterator();
        while (it.hasNext()) {
            Entry<Integer, Integer> handleEntry = it.next();
            if (handleEntry.getValue() == prof.mProfileId) {
                if (btOperational)
                    removeAttributeNative(handleEntry.getKey());

                it.remove();
        }
    }

        // remove SDP records (if there were some)
        if (btOperational) {
            for (int h : prof.mSdpHandles)
                unregisterSdpRecordNative(h);
        }
    }

    void profileDied(ProfileData prof) {
        // we won't accept crashes while initializing
        if (!prof.mInitialized && !prof.mDynamic)
            putProfileBlackList(prof.mServiceName, true);
    }

    void sendServiceChanged() {
        if (getServiceChanged() == false)
            return;

        if (DBG) Log.d(TAG, "Sending invalidate ATT cache");
        invalidateAttCacheNative();
        putServiceChanged(false);
    }

    void setServiceChanged() {
        if (DBG) Log.d(TAG, "Profiles changed - setting ServiceChanged");
        putServiceChanged(true);

        if (mProfilesRunning)
            sendServiceChanged();
        }

    void onBluetoothOn(String changedPackage) {
        if (mDisableDepth++ != 0) {
            if (DBG) Log.d(TAG, "out of order onBluetoothOn");
            return;
        }

        if (DBG) Log.d(TAG, "starting all profiles...");

        mBluetoothState = mAdapter.getState();
        if(mBluetoothState != BluetoothAdapter.STATE_ON) {
            if (DBG) Log.d(TAG , "Bluetooth State Changed from On before init ");
            onBluetoothOff();
            return;
        }
        if (!setupNativeDataNative()) {
            onBluetoothOff();
            return;
        }

        boolean serviceChanged = refreshProfiles(changedPackage);
        if (serviceChanged)
            setServiceChanged();

        startLegacyProfiles(getInstalledAttServices());
        startDyanmicProfiles(getExistingDynamicAttServices());

        if (DBG) Log.d(TAG, "all profiles started.");
        mProfilesRunning = true;
        sendServiceChanged();
    }

    void onBluetoothOff() {
        if (mDisableDepth-- != 1) {
            if (DBG) Log.d(TAG, "out of order onBluetoothOff");
            return;
        }

        Iterator<Entry<Integer, ProfileData>> it = mProfileMap.entrySet().iterator();
        while (it.hasNext()) {
            stopProfile(it.next().getValue(), false);
            it.remove();
        }

        mCurrentProfileId = 1;
        if (DBG) Log.d(TAG, "all profiles stopped.");
        // Check if the DBus allready registered in setupNativeDataNative() function.
        if(!mProfilesRunning)
            return;

        tearDownNativeDataNative();
        mProfilesRunning = false;
    }

    //////////////////// IBluetoothGattServer methods /////////////////

    synchronized public int addService(int profileId, int svcIdx, byte[] svcUuid,
                    int attributeNum) {
        mContext.enforceCallingOrSelfPermission(BLUETOOTH_ADMIN_PERM,
                "Need BLUETOOTH_ADMIN permission");
        clearCallingIdentity();
        if (DBG)
            Log.d(TAG, "addService start profile " + profileId + " service " +
                svcIdx + " attribute num " + attributeNum);

        ProfileData prof = mProfileMap.get(profileId);
        if (prof == null) {
            Log.e(TAG, "addService bad profileId " + profileId);
            return 0;
        }

        // if the profile is started - give it the recorded start handle
        // this is mostly for the OOM case.
        if (prof.mInitialized) {
            Integer existingHandle = prof.mStartHandles.get(svcIdx);
            if (existingHandle != null) {
                if (DBG)
                    Log.d(TAG, "re-init crash service " + prof.mServiceName +
                        " with handle " + existingHandle);
                return existingHandle;
            }

            // could not re-init. nothing much to do here.
            Log.e(TAG, "could not re-init crashed service " + prof.mServiceName);
            return 0;
        }

        int startHandle = findAvailableNative(svcUuid, attributeNum);

        // zero means we didn't find a suitable start - bail out and
        // put in blacklist, as this is deterministic
        if (startHandle == 0) {
            Log.e(TAG, "no free handles found for svc " + svcIdx +
                    " handle count " + attributeNum);
            putProfileBlackList(prof.mServiceName, true);
            return 0;
        }

        if (DBG)
            Log.d(TAG, "addService end " + prof.mServiceName + " svc " + svcIdx +
                " start handle " + startHandle + " handle count " +
                attributeNum);

        // record start handle for profile OOM or crash on bug (binder death)
        prof.mStartHandles.put(svcIdx, startHandle);
        return startHandle;
    }

    synchronized public void addAttribute(int profileId, int serviceIdx, int handle,
            byte[] uuid, int read_reqs, int write_reqs, byte[] value,
            boolean registerCb) {
        mContext.enforceCallingOrSelfPermission(BLUETOOTH_ADMIN_PERM,
                "Need BLUETOOTH_ADMIN permission");
        clearCallingIdentity();
        if (DBG)
            Log.d(TAG, "addAttribute profile " + profileId + " service " +
                serviceIdx + " handle " + handle + " read_reqs " +
                read_reqs + " write_reqs " + write_reqs + " regCb " +
                registerCb);

        ProfileData prof = mProfileMap.get(profileId);
        if (prof == null) {
            Log.e(TAG, "addAttribute bad profileId " + profileId);
            return;
        }

        if (prof.mInitialized) {
            Log.e(TAG, "cannot add attributes to initialized profile " +
                    profileId);
            return;
        }

        addAttributeNative(handle, uuid, read_reqs, write_reqs, value, registerCb);

        // added successfully
        mHandleMap.put(handle, profileId);
    }

    synchronized public void registerSdpRecord(int profileId, int serviceIdx,
            int handle, String name) {
        mContext.enforceCallingOrSelfPermission(BLUETOOTH_ADMIN_PERM,
                "Need BLUETOOTH_ADMIN permission");
        clearCallingIdentity();
        if (DBG)
            Log.d(TAG, "registerSdpRecord profile " + profileId + " service " +
                serviceIdx + " handle " + handle + " name " + name);

        ProfileData prof = mProfileMap.get(profileId);
        if (prof == null) {
            Log.e(TAG, "registerSdpRecord bad profileId " + profileId);
            return;
        }

        if (prof.mInitialized) {
            Log.e(TAG, "cannot register Sdp Record in initialized profile " +
                    profileId);
            return;
        }

        int sdp_handle = registerSdpRecordNative(handle, name);
        if (sdp_handle != 0) {
            if (DBG) Log.d(TAG, "added SDP handle " + sdp_handle);
            prof.mSdpHandles.add(sdp_handle);
        }
    }

    synchronized public void endProfileInit(int profileId) {
        mContext.enforceCallingOrSelfPermission(BLUETOOTH_ADMIN_PERM,
                "Need BLUETOOTH_ADMIN permission");
        clearCallingIdentity();
        ProfileData p = mProfileMap.get(profileId);
        if (p == null)
            return;

        synchronized(p) {
            p.mInitialized = true;
            p.notify();
        }
    }

    public void sendNotification(int profileId, String address, int handle,
            int ccc, byte[] value) {
        mContext.enforceCallingOrSelfPermission(BLUETOOTH_ADMIN_PERM,
                "Need BLUETOOTH_ADMIN permission");
        clearCallingIdentity();
        ProfileData prof = mProfileMap.get(profileId);
        if (prof == null) {
            Log.e(TAG, "sendNotification - invalid profile id " + profileId);
            return;
        }

        if (!prof.mInitialized) {
            Log.e(TAG, "sendNotification - profile " + profileId +
                    "not initialized");
            return;
        }

        // print errors as debug logs - it might be ok not to notify
        // if the device disconnected
        if (!sendNotificationNative(address, handle, ccc, value)) {
            if (DBG) Log.d(TAG, "failed sending notification");
        }
    }

    // a return of 0 means failure. otherwise a unique cookie is returned
    public int sendIndication(int profileId, String address,
            int handle, int ccc, byte[] value) {
        mContext.enforceCallingOrSelfPermission(BLUETOOTH_ADMIN_PERM,
                "Need BLUETOOTH_ADMIN permission");
        clearCallingIdentity();
        ProfileData prof = mProfileMap.get(profileId);
        if (prof == null) {
            Log.e(TAG, "sendIndication - invalid profile id " + profileId);
            return 0;
        }

        if (!prof.mInitialized) {
            Log.e(TAG, "sendIndication - profile " + profileId +
                    "not initialized");
            return 0;
        }

        if (!sendIndicationNative(address, handle, ccc, value, mCookieCounter))
            return 0;

        // the sync is for updating the counter
        synchronized(mCookieMap) {
            int currentCookie = mCookieCounter;

            // increment the cookie for next time and take care of overflows
            mCookieCounter++;
            if (mCookieCounter == 0)
                mCookieCounter = 1;

            if (DBG)
                Log.d(TAG, "sendIndication profile " + profileId + " address " +
                    address + " handle " + handle + " cookie " + currentCookie);
            mCookieMap.put(currentCookie, profileId);
            return currentCookie;
        }
    }

    public void addDynamicProfile(String profileUniqueName, IBluetoothGattServerCallback cb) {
        mContext.enforceCallingOrSelfPermission(BLUETOOTH_ADMIN_PERM,
                "Need BLUETOOTH_ADMIN permission");
        clearCallingIdentity();

        if (mDynamicProfileMap.get(profileUniqueName) != null) {
            Log.e(TAG, "non unique profile name registered: " + profileUniqueName);
            return;
        }

        Log.d(TAG, "Add dynamic profile " + profileUniqueName);
        mDynamicProfileMap.put(profileUniqueName, cb);

        // remove the profile in case of crash
        try {
            cb.asBinder().linkToDeath(new DynProfileRecipient(profileUniqueName), 0);
        } catch (RemoteException e) { e.printStackTrace(); }

        // this profile was started during BT-on. save it as a persistent profile.
        if (mBluetoothState != BluetoothAdapter.STATE_ON)
            return;

        mWakeLock.acquire();
        Message msg = mHandler.obtainMessage(ADD_DYN_PROFILE_MSG, profileUniqueName);
        mHandler.sendMessage(msg);
    }

    public void removeDynamicProfile(String name) {
        mContext.enforceCallingOrSelfPermission(BLUETOOTH_ADMIN_PERM,
                "Need BLUETOOTH_ADMIN permission");
        clearCallingIdentity();
        if (mDynamicProfileMap.remove(name) == null)
            return;

        Log.d(TAG, "Remove dynamic profile " + name);
        mWakeLock.acquire();
        Message msg = mHandler.obtainMessage(REMOVE_DYN_PROFILE_MSG, name);
        mHandler.sendMessage(msg);
    }

    // death recipient for removed profiles
    class DynProfileRecipient implements IBinder.DeathRecipient {
        private String mName;

        DynProfileRecipient(String name) { mName = name; }

        @Override
        public void binderDied() {
            Log.d(TAG, "remote dynamic profile " + mName + " died");
            removeDynamicProfile(mName);
        }
    };

    // returns null on failure
    private ProfileData getProfileForHandle(int handle) {
        Integer profId = mHandleMap.get(handle);
        if (profId == null) {
            Log.e(TAG, "no profile mapping for handle " + handle);
            return null;
        }

        ProfileData prof = mProfileMap.get(profId);
        if (prof == null) {
            Log.e(TAG, "profile not found " + profId);
            return null;
        }

        return prof;
    }

    ///////////////////// Callbacks from D-Bus/Bluez ///////////////////////

    // wrap everything in an exception handler - we shouldn't throw
    // exceptions into JNI
    private byte[] onRead(String objectPath, int handle) {
        try {
            String address = mBluetoothService.getAddressFromObjectPath(objectPath);
            if (address == null) {
                Log.e(TAG, "onRead: no device for object path " + objectPath);
                return new byte[] {(byte)GattStatus.ERR_IO.getValue()};
            }

            if (DBG)
                Log.d(TAG, "onRead from device " + address + " handle " + handle);
            ProfileData prof = getProfileForHandle(handle);
            if (prof == null)
                return new byte[] {(byte)GattStatus.ERR_IO.getValue()};

            if (!prof.mInitialized || prof.mService == null) {
                Log.e(TAG, "onRead - profile not initialized or disconnected");
                return new byte[] {(byte)GattStatus.ERR_IO.getValue()};
            }

            BluetoothDevice device = mAdapter.getRemoteDevice(address);
            try {
                GattReadResult result = prof.mService.readCallback(handle, device);
                byte[] val = result.getValue();
                int len = (val != null) ? val.length : 0;
                byte[] arr = new byte[len + 1];
                arr[0] = (byte)result.getIntStatus();
                // deal with 0 length arrays
                if (len != 0)
                    System.arraycopy(val, 0, arr, 1, len);

                return arr;
            } catch (RemoteException e) {
                e.printStackTrace();
                return new byte[] {(byte)GattStatus.ERR_IO.getValue()};
            }
        } catch (Exception e) {
            e.printStackTrace();
            return new byte[] {(byte)GattStatus.ERR_IO.getValue()};
        } catch (Throwable th) {
            th.printStackTrace();
            return new byte[] {(byte)GattStatus.ERR_IO.getValue()};
        }
    }

    private byte onWrite(String objectPath, int handle, byte[] value) {
        try {
            String address = mBluetoothService.getAddressFromObjectPath(objectPath);
            if (address == null) {
                Log.e(TAG, "onWrite: no device for object path " + objectPath);
                return (byte)GattStatus.ERR_IO.getValue();
            }

            if (DBG)
                Log.d(TAG, "onWrite from device " + address + " handle " + handle +
                    " size " + value.length);

            ProfileData prof = getProfileForHandle(handle);
            if (prof == null) {
                Log.e(TAG, "onWrite - profile not found");
                return (byte)GattStatus.ERR_IO.getValue();
            }

            if (!prof.mInitialized || prof.mService == null) {
                Log.e(TAG, "onWrite - profile not initialized or disconnected");
                return (byte)GattStatus.ERR_IO.getValue();
            }

            BluetoothDevice device = mAdapter.getRemoteDevice(address);
            try {
                return (byte)prof.mService.writeCallback(handle, device, value);
            } catch (RemoteException e) {
                e.printStackTrace();
                return (byte)GattStatus.ERR_IO.getValue();
            }
        } catch (Exception e) {
            e.printStackTrace();
            return (byte)GattStatus.ERR_IO.getValue();
        } catch (Throwable th) {
            th.printStackTrace();
            return (byte)GattStatus.ERR_IO.getValue();
        }
    }

    private void onIndicateResult(int cookie, int status) {
        try {
            if (DBG)
                Log.d(TAG, "onIndicateResult status " + status + " cookie " +
                        cookie);
            Integer profId;

            synchronized (mCookieMap) {
                // we only get the callback once
                profId = mCookieMap.remove(cookie);
                if (profId == null) {
                    Log.e(TAG, "no profile mapping for cookie " + cookie);
                    return;
                }
            }

            ProfileData prof = mProfileMap.get(profId);
            if (prof == null) {
                Log.e(TAG, "indicateResult profile not found " + profId);
                return;
            }

            if (!prof.mInitialized || prof.mService == null) {
                Log.e(TAG, "indicateResult - profile not initialized or disconnected");
                return;
            }

            try {
                prof.mService.indicateResult(cookie, status);
            } catch (RemoteException e) { e.printStackTrace(); }
        } catch (Exception e) {
            e.printStackTrace();
        } catch (Throwable th) {
            th.printStackTrace();
        }
    }

    private void onTxPowerLevelRead(String objectPath, byte level) {
        try {
            String address = mBluetoothService.getAddressFromObjectPath(objectPath);
            if (address == null) {
                Log.e(TAG, "onTxPowerLevelRead: no device for object path " + objectPath);
                return;
            }

            if (DBG)
                Log.d(TAG, "onTxPowerLevelRead from device " + address + " level " +
                    level);

            BluetoothDevice device = mAdapter.getRemoteDevice(address);
            Intent intent = new Intent(TX_POWER_READ_ACTION);
            intent.putExtra(TX_POWER_EXTRA_DEVICE, device);
            intent.putExtra(TX_POWER_EXTRA_LEVEL, (int)level);
            mContext.sendBroadcast(intent, BLUETOOTH_PERM);
        } catch (Exception e) {
            e.printStackTrace();
        } catch (Throwable th) {
            th.printStackTrace();
        }
    }

    private native static void classInitNative();
    private native void initializeNativeDataNative();
    private native boolean setupNativeDataNative();
    private native boolean tearDownNativeDataNative();
    private native void cleanupNativeDataNative();

    private native int findAvailableNative(byte[] uuid, int numItems);
    private native boolean addAttributeNative(int handle, byte[] uuid,
                            int readReqs, int writeReqs, byte[] value,
                            boolean registerCb);
    private native boolean removeAttributeNative(int handle);
    private native boolean sendNotificationNative(String address, int handle,
                                int handle_ccc, byte[] value);
    private native boolean sendIndicationNative(String address, int handle,
                                int handle_ccc, byte[] value, int cookie);
    private native boolean invalidateAttCacheNative();
    private native int registerSdpRecordNative(int handle, String name);
    private native boolean unregisterSdpRecordNative(int sdp_handle);
}
