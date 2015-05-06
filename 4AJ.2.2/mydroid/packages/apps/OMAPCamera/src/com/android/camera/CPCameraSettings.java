/*
 * Copyright (C) 2009 The Android Open Source Project
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

package com.ti.omap.android.camera;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.hardware.Camera.CameraInfo;
import com.ti.omap.android.cpcam.CPCam.Parameters;
import com.ti.omap.android.cpcam.CPCam.Size;
import android.media.CamcorderProfile;
import android.util.Log;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.StringTokenizer;

/**
 *  Provides utilities and keys for Camera settings.
 */
public class CPCameraSettings {
    private static final int NOT_FOUND = -1;

    public static final String KEY_VERSION = "pref_version_key";
    public static final String KEY_LOCAL_VERSION = "pref_local_version_key";
    public static final String KEY_RECORD_LOCATION = RecordLocationPreference.KEY;
    public static final String KEY_PICTURE_SIZE = "pref_camera_picturesize_key";
    public static final String KEY_JPEG_QUALITY = "pref_camera_jpegquality_key";
    public static final String KEY_FOCUS_MODE = "pref_camera_focusmode_key";
    public static final String KEY_FLASH_MODE = "pref_camera_flashmode_key";
    public static final String KEY_CAMERA_ID = "pref_camera_id_key";
    public static final String KEY_TAP_TO_FOCUS_PROMPT_SHOWN = "pref_tap_to_focus_prompt_shown_key";
    public static final String KEY_MODE = "mode";
    public static final String KEY_MODE_MENU = "pref_camera_mode_key";
    public static final String KEY_CAP_MODE_VALUES = "mode-values";
    public static final String KEY_PREVIEW_SIZE = "pref_camera_previewsize_key";

    // 3D exposure keys
    public static final String KEY_SUPPORTED_MANUAL_EXPOSURE_MIN = "supported-manual-exposure-min";
    public static final String KEY_SUPPORTED_MANUAL_EXPOSURE_MAX = "supported-manual-exposure-max";
    public static final String KEY_SUPPORTED_MANUAL_GAIN_ISO_MIN = "supported-manual-gain-iso-min";
    public static final String KEY_SUPPORTED_MANUAL_GAIN_ISO_MAX = "supported-manual-gain-iso-max";

    public static final String KEY_CAMERA_FIRST_USE_HINT_SHOWN = "pref_camera_first_use_hint_shown_key";

    public static final String KEY_PICTURE_FORMAT_MENU = "pref_camera_picture_format_key";
    public static final String KEY_PICTURE_FORMAT = "picture-format";
    public static final String KEY_SUPPORTED_PICTURE_FORMATS = "picture-format-values";

    //CPCAM shot parameres
    public static final String KEY_SHOTPARAMS_BURST = "burst-capture";
    public static final String KEY_SHOTPARAMS_EXP_GAIN_PAIRS = "exp-gain-pairs";
    public static final String KEY_EXP_COMPENSATION = "exp-compensation";
    public static final String KEY_FLUSH_CONFIG = "flush";

    //CPCam metadata keys
    public static final String KEY_ENABLE_LSC_METADATA = "enable-lsc-metadata";
    public static final String KEY_ENABLE_BSC_METADATA = "enable-bsc-metadata";
    public static final String KEY_ENABLE_AUX_IMAGE_METADATA = "enable-aux-metadata";
    public static final String KEY_ENABLE_AF_IMAGE_METADATA = "enable-af-metadata";
    public static final String KEY_ENABLE_AEWB_IMAGE_METADATA = "enable-aewb-metadata";

    public static final int CURRENT_VERSION = 5;
    public static final int CURRENT_LOCAL_VERSION = 2;

    private static final String TAG = "CPCameraSettings";

    private final Context mContext;
    private final Parameters mParameters;
    private final CameraInfo[] mCameraInfo;
    private final int mCameraId;

    public CPCameraSettings(Activity activity, Parameters parameters,
                          int cameraId, CameraInfo[] cameraInfo) {
        mContext = activity;
        mParameters = parameters;
        mCameraId = cameraId;
        mCameraInfo = cameraInfo;
    }

    public PreferenceGroup getPreferenceGroup(int preferenceRes) {
        PreferenceInflater inflater = new PreferenceInflater(mContext);
        PreferenceGroup group =
                (PreferenceGroup) inflater.inflate(preferenceRes);
        initPreference(group);
        return group;
    }


    public static void initialCameraPictureSize(
            Context context, Parameters parameters) {
        // When launching the camera app first time, we will set the picture
        // size to the first one in the list defined in "arrays.xml" and is also
        // supported by the driver.
        List<String> supported = new ArrayList<String>();
        List<Size> suppSizes = parameters.getSupportedPictureSizes();
        if (suppSizes != null) {
            supported = sizeListToStringList(suppSizes);
        }

        if (supported == null) return;
        for (String candidate : context.getResources().getStringArray(
                R.array.pref_camera_picturesize_entryvalues)) {
            if (setCameraPictureSize(candidate, supported, parameters)) {
                SharedPreferences.Editor editor = ComboPreferences
                        .get(context).edit();
                editor.putString(KEY_PICTURE_SIZE, candidate);
                editor.apply();
                return;
            }
        }
        Log.e(TAG, "No supported picture size found");
    }

    public static void removePreferenceFromScreen(
            PreferenceGroup group, String key) {
        removePreference(group, key);
    }

    public static boolean setCameraPictureSize(
            String candidate, List<String> supported, Parameters parameters) {
        int index = candidate.indexOf('x');
        if (index == NOT_FOUND) return false;
        int width = Integer.parseInt(candidate.substring(0, index));
        int height = Integer.parseInt(candidate.substring(index + 1));
        for (String item : supported) {
            int w = Integer.parseInt(item.substring(0, item.indexOf("x")));
            int h = Integer.parseInt(item.substring(item.indexOf("x") + 1));
            if (w == width && h == height) {
                parameters.setPictureSize(width, height);
                return true;
            }
        }
        return false;
    }
    public static int getCameraPictureSizeWidth(String candidate){
        int index = candidate.indexOf('x');
        if (index == NOT_FOUND) return 0;
        int width = Integer.parseInt(candidate.substring(0, index));
        return width;
    }

    public static int getCameraPictureSizeHeight(String candidate){
        int index = candidate.indexOf('x');
        if (index == NOT_FOUND) return 0;
        int height = Integer.parseInt(candidate.substring(index + 1));
        return height;
    }

    public static boolean setCameraPreviewSize(
          String candidate, List<String> supported, Parameters parameters) {
       int index = candidate.indexOf('x');
       if (index == NOT_FOUND) return false;
       int width = Integer.parseInt(candidate.substring(0, index));
       int height = Integer.parseInt(candidate.substring(index + 1));
       for (String item: supported) {
           int w = Integer.parseInt(item.substring(0, item.indexOf("x")));
           int h = Integer.parseInt(item.substring(item.indexOf("x") + 1));
           if (w == width && h == height) {
               parameters.setPreviewSize(w, h);
               return true;
           }
       }
       return false;
    }

    private void initPreference(PreferenceGroup group) {
        IconListPreference cameraIdPref =
                (IconListPreference) group.findPreference(KEY_CAMERA_ID);
        ListPreference mode = group.findPreference(KEY_MODE_MENU);
        ListPreference pictureFormat = group.findPreference(KEY_PICTURE_FORMAT_MENU);
        ListPreference pictureSize = group.findPreference(KEY_PICTURE_SIZE);
        ListPreference previewSize = group.findPreference(KEY_PREVIEW_SIZE);

        ArrayList<CharSequence[]> allPictureEntries = new ArrayList<CharSequence[]>();
        ArrayList<CharSequence[]> allPictureEntryValues = new ArrayList<CharSequence[]>();

        ArrayList<CharSequence[]> allPreviewEntries = new ArrayList<CharSequence[]>();
        ArrayList<CharSequence[]> allPreviewEntryValues = new ArrayList<CharSequence[]>();

        if (pictureSize != null) {
            ListPreference pictureSizes = group.findPreference(KEY_PICTURE_SIZE);
            if (pictureSizes !=null) {
                pictureSizes.clearAndSetEntries(allPictureEntries, allPictureEntryValues,
                        pictureSize.getEntries(), pictureSize.getEntryValues());
            }
        }

        if (previewSize != null) {
            ListPreference previewSizes = group.findPreference(KEY_PREVIEW_SIZE);
            if (previewSizes !=null) {
                previewSizes.clearAndSetEntries(allPreviewEntries, allPreviewEntryValues,
                        previewSize.getEntries(), previewSize.getEntryValues());
            }
        }

        if (cameraIdPref != null) buildCameraId(group, cameraIdPref);
        if(mode !=null){
            ArrayList<String> suppMode = new ArrayList<String>();
            String modeValues = mParameters.get(KEY_CAP_MODE_VALUES);
            if(modeValues != null && !modeValues.equals("")){
                for(String item : modeValues.split(",")){
                    suppMode.add(item);
                }
            }
            filterUnsupportedOptions(group, mode, suppMode);
        }

        if (pictureFormat != null) {
            ArrayList<String> suppPictureFormat = new ArrayList<String>();
            String pictureFormatValues = mParameters.get(KEY_SUPPORTED_PICTURE_FORMATS);
            if (pictureFormatValues != null && !pictureFormatValues.equals("")) {
                for (String item : pictureFormatValues.split(",")) {
                    suppPictureFormat.add(item);
                }
            }
            filterUnsupportedOptions(group, pictureFormat, suppPictureFormat);
        }
    }

    private void buildCameraId(
            PreferenceGroup group, IconListPreference preference) {
        int numOfCameras = mCameraInfo.length;
        if (numOfCameras < 2) {
            removePreference(group, preference.getKey());
            return;
        }

        CharSequence[] entryValues = new CharSequence[numOfCameras];
        for (int i = 0; i < mCameraInfo.length; ++i) {
            entryValues[i] = "" + i;
        }
        preference.setEntryValues(entryValues);
    }

    private static boolean removePreference(PreferenceGroup group, String key) {
        for (int i = 0, n = group.size(); i < n; i++) {
            CameraPreference child = group.get(i);
            if (child instanceof PreferenceGroup) {
                if (removePreference((PreferenceGroup) child, key)) {
                    return true;
                }
            }
            if (child instanceof ListPreference &&
                    ((ListPreference) child).getKey().equals(key)) {
                group.removePreference(i);
                return true;
            }
        }
        return false;
    }

    public static void filterUnsupportedOptions(PreferenceGroup group,
            ListPreference pref, List<String> supported) {

        // Remove the preference if the parameter is not supported or there is
        // only one options for the settings.
        if (supported == null || supported.size() <= 1) {
            removePreference(group, pref.getKey());
            return;
        }

        pref.filterUnsupported(supported);
        if (pref.getEntries().length <= 1) {
            removePreference(group, pref.getKey());
            return;
        }

        resetIfInvalid(pref);
    }

    private void filterUnsupportedOptionsInt(PreferenceGroup group,
            ListPreference pref, List<Integer> supported) {
        // Remove the preference if the parameter is not supported or there is
        // only one options for the settings.
        if (supported == null || supported.size() <= 1) {
            removePreference(group, pref.getKey());
            return;
        }

        pref.filterUnsupportedInt(supported);

        // Set the value to the first entry if it is invalid.
        String value = pref.getValue();
        if (pref.findIndexOfValue(value) == NOT_FOUND) {
            pref.setValueIndex(0);
        }
    }

    private static void resetIfInvalid(ListPreference pref) {
        // Set the value to the first entry if it is invalid.
        String value = pref.getValue();
        if (pref.findIndexOfValue(value) == NOT_FOUND) {
            pref.setValueIndex(0);
        }
    }

    public static List<String> sizeListToStringList(List<Size> sizes) {
        ArrayList<String> list = new ArrayList<String>();

        if (sizes != null) {
            for (Size size : sizes) {
                list.add(String.format("%dx%d", size.width, size.height));
            }
        }

        return list;
    }

    public static void upgradeLocalPreferences(SharedPreferences pref) {
        int version;
        try {
            version = pref.getInt(KEY_LOCAL_VERSION, 0);
        } catch (Exception ex) {
            version = 0;
        }
        if (version == CURRENT_LOCAL_VERSION) return;

        SharedPreferences.Editor editor = pref.edit();
        if (version == 1) {
            // We use numbers to represent the quality now. The quality definition is identical to
            // that of CamcorderProfile.java.
            editor.remove("pref_video_quality_key");
        }
        editor.putInt(KEY_LOCAL_VERSION, CURRENT_LOCAL_VERSION);
        editor.apply();
    }

    public static void upgradeGlobalPreferences(SharedPreferences pref) {
        int version;
        try {
            version = pref.getInt(KEY_VERSION, 0);
        } catch (Exception ex) {
            version = 0;
        }
        if (version == CURRENT_VERSION) return;

        SharedPreferences.Editor editor = pref.edit();
        if (version == 0) {
            // We won't use the preference which change in version 1.
            // So, just upgrade to version 1 directly
            version = 1;
        }
        if (version == 2) {
            editor.putString(KEY_RECORD_LOCATION,
                    pref.getBoolean(KEY_RECORD_LOCATION, false)
                    ? RecordLocationPreference.VALUE_ON
                    : RecordLocationPreference.VALUE_NONE);
            version = 3;
        }
        if (version == 3) {
            // Just use video quality to replace it and
            // ignore the current settings.
            editor.remove("pref_camera_videoquality_key");
            editor.remove("pref_camera_video_duration_key");
        }

        editor.putInt(KEY_VERSION, CURRENT_VERSION);
        editor.apply();
    }

    public static int readPreferredCameraId(SharedPreferences pref) {
        return Integer.parseInt(pref.getString(KEY_CAMERA_ID, "0"));
    }

    public static void writePreferredCameraId(SharedPreferences pref,
            int cameraId) {
        Editor editor = pref.edit();
        editor.putString(KEY_CAMERA_ID, Integer.toString(cameraId));
        editor.apply();
    }

    public static void restorePreferences(Context context,
            ComboPreferences preferences, Parameters parameters) {
        int currentCameraId = readPreferredCameraId(preferences);

        for (int i=0; i<CameraHolder.instance().getNumberOfCameras(); i++)
        {
            preferences.setLocalId(context, i);
            Editor editor = preferences.edit();
            editor.clear();
            editor.apply();
        }

        // Switch back to the preferences of the current camera. Otherwise,
        // we may write the preference to wrong camera later.
        preferences.setLocalId(context, currentCameraId);

        upgradeGlobalPreferences(preferences.getGlobal());
        upgradeLocalPreferences(preferences.getLocal());

        // Write back the current camera id because parameters are related to
        // the camera. Otherwise, we may switch to the front camera but the
        // initial picture size is that of the back camera.
        initialCameraPictureSize(context, parameters);
        writePreferredCameraId(preferences, currentCameraId);
    }

    public List<String> parseToList(String str) {
        if (null == str)
            return null;

        StringTokenizer tokenizer = new StringTokenizer(str, ",");
        ArrayList<String> substrings = new ArrayList<String>();

        while (tokenizer.hasMoreElements())
            substrings.add(tokenizer.nextToken());

        return substrings;
    }

}
