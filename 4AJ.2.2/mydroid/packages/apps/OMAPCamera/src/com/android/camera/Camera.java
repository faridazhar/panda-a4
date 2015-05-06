/*
 * Copyright (C) 2007 The Android Open Source Project
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
import android.content.BroadcastReceiver;
import android.content.ContentProviderClient;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences.Editor;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.SurfaceTexture;
import android.hardware.Camera.CameraInfo;
import android.hardware.Camera.Face;
import android.hardware.Camera.FaceDetectionListener;
import android.hardware.Camera.Parameters;
import android.hardware.Camera.PictureCallback;
import android.hardware.Camera.Size;
import android.location.Location;
import android.media.CameraProfile;
import android.media.MediaActionSound;
import android.net.Uri;
import android.os.Bundle;
import android.os.ConditionVariable;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.MessageQueue;
import android.os.SystemClock;
import android.provider.MediaStore;
import android.util.Log;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.OrientationEventListener;
import android.view.SurfaceView;
import android.view.View;
import android.view.WindowManager;
import android.view.GestureDetector;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

import com.ti.omap.android.camera.ui.CameraPicker;
import com.ti.omap.android.camera.ui.FaceView;
import com.ti.omap.android.camera.ui.IndicatorControlContainer;
import com.ti.omap.android.camera.ui.PopupManager;
import com.ti.omap.android.camera.ui.Rotatable;
import com.ti.omap.android.camera.ui.RotateImageView;
import com.ti.omap.android.camera.ui.RotateLayout;
import com.ti.omap.android.camera.ui.RotateTextToast;
import com.ti.omap.android.camera.ui.TwoStateImageView;
import com.ti.omap.android.camera.ui.ZoomControl;
import com.ti.omap.android.camera.CameraSettings;
import com.ti.omap.android.camera.ui.ManualConvergenceSettings;
import com.ti.omap.android.camera.ui.ManualGainExposureSettings;
import com.ti.omap.android.camera.R;
import com.ti.omap.android.camera.ui.FaceViewData;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Formatter;
import java.util.List;

/** The Camera activity which can preview and take pictures. */
public class Camera extends ActivityBase implements FocusManager.Listener,
        ModePicker.OnModeChangeListener, FaceDetectionListener,
        CameraPreference.OnPreferenceChangedListener, LocationManager.Listener,
        PreviewFrameLayout.OnSizeChangedListener,
        ShutterButton.OnShutterButtonListener,
        TouchManager.Listener,
        ShutterButton.OnShutterButtonLongPressListener {

    private static final String TAG = "camera";

    // We number the request code from 1000 to avoid collision with Gallery.
    private static final int REQUEST_CROP = 1000;

    private static final int FIRST_TIME_INIT = 2;
    private static final int CLEAR_SCREEN_DELAY = 3;
    private static final int SET_CAMERA_PARAMETERS_WHEN_IDLE = 4;
    private static final int CHECK_DISPLAY_ROTATION = 5;
    private static final int SHOW_TAP_TO_FOCUS_TOAST = 6;
    private static final int UPDATE_THUMBNAIL = 7;
    private static final int SWITCH_CAMERA = 8;
    private static final int SWITCH_CAMERA_START_ANIMATION = 9;
    private static final int CAMERA_OPEN_DONE = 10;
    private static final int START_PREVIEW_DONE = 11;
    private static final int OPEN_CAMERA_FAIL = 12;
    private static final int CAMERA_DISABLED = 13;
    private static final int MODE_RESTART = 14;
    private static final int RESTART_PREVIEW = 15;
    public static final int MANUAL_GAIN_EXPOSURE_CHANGED = 16;
    public static final int RELEASE_CAMERA = 17;
    public static final int MANUAL_CONVERGENCE_CHANGED = 18;

    // The subset of parameters we need to update in setCameraParameters().
    private static final int UPDATE_PARAM_INITIALIZE = 1;
    private static final int UPDATE_PARAM_ZOOM = 2;
    private static final int UPDATE_PARAM_PREFERENCE = 4;
    private static final int UPDATE_PARAM_ALL = -1;
    private static final int UPDATE_PARAM_MODE = 8;


    private String mPreviewSize = null;
    private String mAntibanding = null;
    private String mContrast = null;
    private String mBrightness = null;
    private String mSaturation = null;
    private String mSharpness = null;
    private String mISO = null;
    private String mExposureMode = null;
    private String mColorEffect = null;
    private int mJpegQuality = CameraProfile.QUALITY_HIGH;
    private String mLastPreviewFramerate;

    private static final String PARM_CONTRAST = "contrast";
    private static final String PARM_BRIGHTNESS = "brightness";
    private static final String PARM_SHARPNESS = "sharpness";
    private static final String PARM_SATURATION = "saturation";
    private static final String PARM_EXPOSURE_BRACKETING_RANGE = "exp-bracketing-range";
    private static final String EXPOSURE_BRACKETING_RANGE_VALUE = "-30,0,30";
    private static final String PARM_ZOOM_BRACKETING_RANGE = "zoom-bracketing-range";
    private static final String PARM_IPP = "ipp";
    private static final String PARM_BURST = "burst-capture";
    private static final String PARM_ISO = "iso";
    private static final String PARM_EXPOSURE_MODE = "exposure";
    private static final String PARM_IPP_LDCNSF = "ldc-nsf";
    private static final String PARM_IPP_NONE = "off";
    private static final String PARM_GBCE = "gbce";
    private static final String PARM_GLBCE = "glbce";
    private static final String PARM_GBCE_OFF = "disable";
    private static final String PARM_TEMPORAL_BRACKETING_RANGE_POS = "temporal-bracketing-range-positive";
    private static final String PARM_TEMPORAL_BRACKETING_RANGE_NEG = "temporal-bracketing-range-negative";
    private static final int EXPOSURE_BRACKETING_COUNT = 3;
    private static final int ZOOM_BRACKETING_COUNT = 10;
    private static final int CAMERA_RELEASE_DELAY = 3000;
    public static final String PARM_SUPPORTED_ISO_MODES = "iso-mode-values";
    public static final String PARM_SUPPORTED_GBCE = "gbce-supported";
    public static final String PARM_SUPPORTED_GLBCE = "glbce-supported";
    public static final String PARM_SUPPORTED_EXPOSURE_MODES = "exposure-mode-values";

    private boolean mTempBracketingEnabled = false;
    private boolean mTempBracketingStarted = false;
    private boolean mIsBurstStampInitialized = false;
    private boolean mIsBurstTimeStampsSwapNeeded = false;
    private boolean mBurstRunning = false;

    private int mBurstImages = 0;
    private int mCurrBurstImages = 0;

    private double mBurstTimeStamp = 0;
    private double mBurstTmpTimeStamp = 0;
    private double mBurstFPSsum = 0;

    private String mCaptureMode = new String();

    public static final String TRUE = "true";
    public static final String FALSE = "false";

    private String mTouchConvergence;
    private String mManualConvergence;
    private String mTemporalBracketing;
    private String mExposureBracketing;
    private String mHighPerformance;
    private String mHighQuality;
    private String mHighQualityZsl;
    private String mZoomBracketing;
    private String mBracketRange;
    private String mGBCEOff;
    private String mGBCE = "off";
    private String mManualExposure;

    private boolean mPausing;

    // When setCameraParametersWhenIdle() is called, we accumulate the subsets
    // needed to be updated in mUpdateSet.
    private int mUpdateSet;

    private static final int SCREEN_DELAY = 2 * 60 * 1000;

    private int mZoomValue;  // The current zoom value.
    private int mZoomMax;
    private ZoomControl mZoomControl;
    private GestureDetector mGestureDetector;

    private Parameters mParameters;
    private Parameters mInitialParams;
    private boolean mFocusAreaSupported;
    private boolean mMeteringAreaSupported;
    private boolean mAeLockSupported;
    private boolean mAwbLockSupported;
    private boolean mContinousFocusSupported;

    private MyOrientationEventListener mOrientationListener;
    // The degrees of the device rotated clockwise from its natural orientation.
    private int mOrientation = OrientationEventListener.ORIENTATION_UNKNOWN;
    // The orientation compensation for icons and thumbnails. Ex: if the value
    // is 90, the UI components should be rotated 90 degrees counter-clockwise.
    private int mOrientationCompensation = 0;
    private ComboPreferences mPreferences;

    private static final String sTempCropFilename = "crop-temp";

    private ContentProviderClient mMediaProviderClient;
    private ShutterButton mShutterButton;
    private boolean mFaceDetectionStarted = false;

    private PreviewFrameLayout mPreviewFrameLayout;
    private SurfaceTexture mSurfaceTexture;
    private RotateDialogController mRotateDialog;

    private ModePicker mModePicker;
    private FaceView mFaceView;
    private RotateLayout mFocusAreaIndicator;
    private Rotatable mReviewCancelButton;
    private Rotatable mReviewDoneButton;
    private View mReviewRetakeButton;

    // mCropValue and mSaveUri are used only if isImageCaptureIntent() is true.
    private String mCropValue;
    private Uri mSaveUri;

    // Small indicators which show the camera settings in the viewfinder.
    private TextView mExposureIndicator;
    private ImageView mGpsIndicator;
    private ImageView mFlashIndicator;
    private ImageView mSceneIndicator;
    private ImageView mWhiteBalanceIndicator;
    private ImageView mFocusIndicator;
    // A view group that contains all the small indicators.
    private Rotatable mOnScreenIndicators;

    // We use a thread in ImageSaver to do the work of saving images and
    // generating thumbnails. This reduces the shot-to-shot time.
    private ImageSaver mImageSaver;
    // Similarly, we use a thread to generate the name of the picture and insert
    // it into MediaStore while picture taking is still in progress.
    private ImageNamer mImageNamer;

    private MediaActionSound mCameraSound;

    private Runnable mDoSnapRunnable = new Runnable() {
        @Override
        public void run() {
            onShutterButtonClick();
        }
    };

    private final StringBuilder mBuilder = new StringBuilder();
    private final Formatter mFormatter = new Formatter(mBuilder);
    private final Object[] mFormatterArgs = new Object[1];

    /**
     * An unpublished intent flag requesting to return as soon as capturing
     * is completed.
     *
     * TODO: consider publishing by moving into MediaStore.
     */
    private static final String EXTRA_QUICK_CAPTURE =
            "android.intent.extra.quickCapture";

    // The display rotation in degrees. This is only valid when mCameraState is
    // not PREVIEW_STOPPED.
    private int mDisplayRotation;
    // The value for android.hardware.Camera.setDisplayOrientation.
    private int mCameraDisplayOrientation;
    // The value for UI components like indicators.
    private int mDisplayOrientation;
    // The value for android.hardware.Camera.Parameters.setRotation.
    private int mJpegRotation;
    private boolean mFirstTimeInitialized;
    private boolean mIsImageCaptureIntent;

    private static final int PREVIEW_STOPPED = 0;
    private static final int IDLE = 1;  // preview is active
    // Focus is in progress. The exact focus state is in Focus.java.
    private static final int FOCUSING = 2;
    private static final int SNAPSHOT_IN_PROGRESS = 3;
    // Switching between cameras.
    private static final int SWITCHING_CAMERA = 4;
    private int mCameraState = PREVIEW_STOPPED;
    private boolean mSnapshotOnIdle = false;

    private ContentResolver mContentResolver;
    private boolean mDidRegister = false;

    private LocationManager mLocationManager;

    private final ShutterCallback mShutterCallback = new ShutterCallback();
    private final PostViewPictureCallback mPostViewPictureCallback =
            new PostViewPictureCallback();
    private final RawPictureCallback mRawPictureCallback =
            new RawPictureCallback();
    private final AutoFocusCallback mAutoFocusCallback =
            new AutoFocusCallback();
    private final AutoFocusMoveCallback mAutoFocusMoveCallback =
            new AutoFocusMoveCallback();
    private final CameraErrorCallback mErrorCallback = new CameraErrorCallback();

    private long mFocusStartTime;
    private long mShutterCallbackTime;
    private long mPostViewPictureCallbackTime;
    private long mRawPictureCallbackTime;
    private long mJpegPictureCallbackTime;
    private long mOnResumeTime;
    private long mStorageSpace;
    private byte[] mJpegImageData;
    private String mAutoConvergence = null;
    private boolean isExposureInit = false;
    private boolean isManualExposure = false;
    private Integer mManualExposureLeft = new Integer (0);
    private Integer mManualExposureRight = new Integer (0);
    private Integer mManualGainISOLeft = new Integer (0);
    private Integer mManualGainISORight = new Integer (0);
    private Integer mManualExposureControl = new Integer(0);
    private Integer mManualGainISO = new Integer(0);

    private Integer mManualConvergenceValue = new Integer(0);
    private boolean isManualConvergence = false;
    private boolean isConvergenceInit = false;

    // These latency time are for the CameraLatency test.
    public long mAutoFocusTime;
    public long mShutterLag;
    public long mShutterToPictureDisplayedTime;
    public long mPictureDisplayedToJpegCallbackTime;
    public long mJpegCallbackFinishTime;
    public long mCaptureStartTime;

    private TouchManager mTouchManager;
    // This handles everything about focus.
    private FocusManager mFocusManager;
    private String mSceneMode;
    private Toast mNotSelectableToast;

    private final Handler mHandler = new MainHandler();
    private IndicatorControlContainer mIndicatorControlContainer;
    private PreferenceGroup mPreferenceGroup;

    private boolean mQuickCapture;

    CameraStartUpThread mCameraStartUpThread;
    ConditionVariable mStartPreviewPrerequisiteReady = new ConditionVariable();

    // The purpose is not to block the main thread in onCreate and onResume.
    private class CameraStartUpThread extends Thread {
        private volatile boolean mCancelled;

        public void cancel() {
            mCancelled = true;
        }

        @Override
        public void run() {
            try {
                // We need to check whether the activity is paused before long
                // operations to ensure that onPause() can be done ASAP.
                if (mCancelled) return;
                mCameraDevice = Util.openCamera(Camera.this, mCameraId);
                mParameters = mCameraDevice.getParameters();
                // Wait until all the initialization needed by startPreview are
                // done.
                mStartPreviewPrerequisiteReady.block();

                initializeCapabilities();
                if (mFocusManager == null) initializeFocusManager();
                if (mCancelled) return;
                setCameraParameters(UPDATE_PARAM_ALL);
                mHandler.sendEmptyMessage(CAMERA_OPEN_DONE);
                if (mCancelled) return;
                startPreview(true);
                mHandler.sendEmptyMessage(START_PREVIEW_DONE);
                mOnResumeTime = SystemClock.uptimeMillis();
                mHandler.sendEmptyMessage(CHECK_DISPLAY_ROTATION);
            } catch (CameraHardwareException e) {
                mHandler.sendEmptyMessage(OPEN_CAMERA_FAIL);
            } catch (CameraDisabledException e) {
                mHandler.sendEmptyMessage(CAMERA_DISABLED);
            }
        }
    }

    /**
     * This Handler is used to post message back onto the main thread of the
     * application
     */
    private class MainHandler extends Handler {
        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case RESTART_PREVIEW: {
                    boolean updateAllParams = true;
                    if ( msg.arg1 == MODE_RESTART ) {
                        updateAllParams = false;
                    }
                    startPreview(updateAllParams);
                    startFaceDetection();
                    break;
                }
                case CLEAR_SCREEN_DELAY: {
                    getWindow().clearFlags(
                            WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                    break;
                }

                case FIRST_TIME_INIT: {
                    initializeFirstTime();
                    break;
                }

                case SET_CAMERA_PARAMETERS_WHEN_IDLE: {
                    setCameraParametersWhenIdle(0);
                    break;
                }

                case CHECK_DISPLAY_ROTATION: {
                    // Set the display orientation if display rotation has changed.
                    // Sometimes this happens when the device is held upside
                    // down and camera app is opened. Rotation animation will
                    // take some time and the rotation value we have got may be
                    // wrong. Framework does not have a callback for this now.
                    if (Util.getDisplayRotation(Camera.this) != mDisplayRotation) {
                        setDisplayOrientation();
                    }
                    if (SystemClock.uptimeMillis() - mOnResumeTime < 5000) {
                        mHandler.sendEmptyMessageDelayed(CHECK_DISPLAY_ROTATION, 100);
                    }
                    break;
                }

                case SHOW_TAP_TO_FOCUS_TOAST: {
                    showTapToFocusToast();
                    break;
                }

                case UPDATE_THUMBNAIL: {
                    mImageSaver.updateThumbnail();
                    break;
                }

                case MANUAL_CONVERGENCE_CHANGED: {
                    mManualConvergenceValue = (Integer) msg.obj;
                    mParameters.set(CameraSettings.KEY_MANUAL_CONVERGENCE, mManualConvergenceValue.intValue());
                    if (mCameraDevice != null) {
                        mCameraDevice.setParameters(mParameters);
                    }
                    break;
                }

                case MANUAL_GAIN_EXPOSURE_CHANGED: {
                    Bundle data;
                    data = msg.getData();
                    if (mCameraId < 2) {
                        mManualExposureControl = data.getInt("EXPOSURE");
                        if (mManualExposureControl < 1) {
                            mManualExposureControl = 1;
                        }
                        mManualGainISO = data.getInt("ISO");
                        mParameters.set(CameraSettings.KEY_MANUAL_EXPOSURE, mManualExposureControl.intValue());
                        mParameters.set(CameraSettings.KEY_MANUAL_GAIN_ISO, mManualGainISO.intValue());
                    } else {
                        mManualExposureRight = data.getInt("EXPOSURE_RIGHT");
                        if (mManualExposureRight < 1) {
                            mManualExposureRight = 1;
                        }
                        mManualExposureLeft = data.getInt("EXPOSURE_LEFT");
                        if (mManualExposureLeft < 1) {
                            mManualExposureLeft = 1;
                        }
                        mManualGainISORight = data.getInt("ISO_RIGHT");
                        mManualGainISOLeft = data.getInt("ISO_LEFT");

                        mParameters.set(CameraSettings.KEY_MANUAL_EXPOSURE_RIGHT, mManualExposureRight.intValue());
                        mParameters.set(CameraSettings.KEY_MANUAL_EXPOSURE, mManualExposureLeft.intValue());
                        mParameters.set(CameraSettings.KEY_MANUAL_GAIN_ISO, mManualGainISOLeft.intValue());
                        mParameters.set(CameraSettings.KEY_MANUAL_GAIN_ISO_RIGHT, mManualGainISORight.intValue());
                    }
                    if (mCameraDevice != null) {
                        mCameraDevice.setParameters(mParameters);
                    }
                    break;
                }
                case SWITCH_CAMERA: {
                    switchCamera();
                    break;
                }

                case SWITCH_CAMERA_START_ANIMATION: {
                    mCameraScreenNail.animateSwitchCamera();
                    break;
                }

                case CAMERA_OPEN_DONE: {
                    initializeAfterCameraOpen();
                    break;
                }

                case START_PREVIEW_DONE: {
                    mCameraStartUpThread = null;
                    setCameraState(IDLE);
                    startFaceDetection();
                    break;
                }

                case OPEN_CAMERA_FAIL: {
                    mCameraStartUpThread = null;
                    mOpenCameraFail = true;
                    Util.showErrorAndFinish(Camera.this,
                            R.string.cannot_connect_camera);
                    break;
                }

                case CAMERA_DISABLED: {
                    mCameraStartUpThread = null;
                    mCameraDisabled = true;
                    Util.showErrorAndFinish(Camera.this,
                            R.string.camera_disabled);
                    break;
                }
                case RELEASE_CAMERA: {
                    stopPreview();
                    closeCamera();
                    break;
                }
            }
        }
    }

    private void initializeAfterCameraOpen() {
        // These depend on camera parameters.
        setPreviewFrameLayoutAspectRatio();
        mFocusManager.setPreviewSize(mPreviewFrameLayout.getWidth(),
                mPreviewFrameLayout.getHeight());
        mTouchManager.setPreviewSize(mPreviewFrameLayout.getWidth(),
                mPreviewFrameLayout.getHeight());
        if (mIndicatorControlContainer == null) {
            initializeIndicatorControl();
        }
        // This should be enabled after preview is started.
        mIndicatorControlContainer.setEnabled(false);
        initializeZoom();
        updateOnScreenIndicators();
        showTapToFocusToastIfNeeded();
    }

    private void resetExposureCompensation() {
        String value = mPreferences.getString(CameraSettings.KEY_EXPOSURE,
                CameraSettings.EXPOSURE_DEFAULT_VALUE);
        if (!CameraSettings.EXPOSURE_DEFAULT_VALUE.equals(value)) {
            Editor editor = mPreferences.edit();
            editor.putString(CameraSettings.KEY_EXPOSURE, "0");
            editor.apply();
            if (mIndicatorControlContainer != null) {
                mIndicatorControlContainer.reloadPreferences();
            }
        }
    }

    private void keepMediaProviderInstance() {
        // We want to keep a reference to MediaProvider in camera's lifecycle.
        // TODO: Utilize mMediaProviderClient instance to replace
        // ContentResolver calls.
        if (mMediaProviderClient == null) {
            mMediaProviderClient = mContentResolver
                    .acquireContentProviderClient(MediaStore.AUTHORITY);
        }
    }

    // Snapshots can only be taken after this is called. It should be called
    // once only. We could have done these things in onCreate() but we want to
    // make preview screen appear as soon as possible.
    private void initializeFirstTime() {
        if (mFirstTimeInitialized) return;

        // Create orientation listener. This should be done first because it
        // takes some time to get first orientation.
        mOrientationListener = new MyOrientationEventListener(Camera.this);
        mOrientationListener.enable();

        // Initialize location service.
        boolean recordLocation = RecordLocationPreference.get(
                mPreferences, mContentResolver);
        mLocationManager.recordLocation(recordLocation);

        keepMediaProviderInstance();
        checkStorage();

        // Initialize shutter button.
        mShutterButton = (ShutterButton) findViewById(R.id.shutter_button);
        mShutterButton.setOnShutterButtonListener(this);
        mShutterButton.setOnShutterButtonLongPressListener(this);
        mShutterButton.setVisibility(View.VISIBLE);
        CameraInfo info = CameraHolder.instance().getCameraInfo()[mCameraId];
        boolean mirror = (info.facing == CameraInfo.CAMERA_FACING_FRONT);
        int displayRotation = Util.getDisplayRotation(this);
        int displayOrientation = Util.getDisplayOrientation(displayRotation, mCameraId);
        mTouchManager.initialize(this, mirror, displayOrientation);

        mImageSaver = new ImageSaver();
        mImageNamer = new ImageNamer();
        installIntentFilter();

        mFirstTimeInitialized = true;
        addIdleHandler();
    }

    private void showTapToFocusToastIfNeeded() {
        // Show the tap to focus toast if this is the first start.
        if (mFocusAreaSupported &&
                mPreferences.getBoolean(CameraSettings.KEY_CAMERA_FIRST_USE_HINT_SHOWN, true)) {
            // Delay the toast for one second to wait for orientation.
            mHandler.sendEmptyMessageDelayed(SHOW_TAP_TO_FOCUS_TOAST, 1000);
        }
    }

    private void addIdleHandler() {
        MessageQueue queue = Looper.myQueue();
        queue.addIdleHandler(new MessageQueue.IdleHandler() {
            @Override
            public boolean queueIdle() {
                Storage.ensureOSXCompatible();
                return false;
            }
        });
    }

    // If the activity is paused and resumed, this method will be called in
    // onResume.
    private void initializeSecondTime() {
        // Start orientation listener as soon as possible because it takes
        // some time to get first orientation.
        mOrientationListener.enable();

        // Start location update if needed.
        boolean recordLocation = RecordLocationPreference.get(
                mPreferences, mContentResolver);
        mLocationManager.recordLocation(recordLocation);

        installIntentFilter();
        mImageSaver = new ImageSaver();
        mImageNamer = new ImageNamer();
        initializeZoom();
        keepMediaProviderInstance();
        checkStorage();
        hidePostCaptureAlert();

        if (!mIsImageCaptureIntent) {
            mModePicker.setCurrentMode(ModePicker.MODE_CAMERA);
        }
        if (mIndicatorControlContainer != null) {
            mIndicatorControlContainer.reloadPreferences();
        }
    }

    private class ZoomChangeListener implements ZoomControl.OnZoomChangedListener {
        @Override
        public void onZoomValueChanged(int index) {
            // Not useful to change zoom value when the activity is paused.
            if (mPaused) return;
            mZoomValue = index;

            // Set zoom parameters asynchronously
            mParameters.setZoom(mZoomValue);
            mCameraDevice.setParametersAsync(mParameters);
        }
    }

    private void initializeZoom() {
        if (!mParameters.isZoomSupported()) return;
        mZoomMax = mParameters.getMaxZoom();
        // Currently we use immediate zoom for fast zooming to get better UX and
        // there is no plan to take advantage of the smooth zoom.
        mZoomControl.setZoomMax(mZoomMax);
        mZoomControl.setZoomIndex(mParameters.getZoom());
        mZoomControl.setOnZoomChangeListener(new ZoomChangeListener());
        mGestureDetector = new GestureDetector(this, new ZoomGestureListener());
    }

    @Override
    public void startFaceDetection() {
        if (mFaceDetectionStarted) return;
        if (mParameters.getMaxNumDetectedFaces() > 0) {
            mFaceDetectionStarted = true;
            mFaceView.clear();
            mFaceView.setVisibility(View.VISIBLE);
            mFaceView.setDisplayOrientation(mDisplayOrientation);
            CameraInfo info = CameraHolder.instance().getCameraInfo()[mCameraId];
            mFaceView.setMirror(info.facing == CameraInfo.CAMERA_FACING_FRONT);
            mFaceView.resume();
            mFocusManager.setFaceView(mFaceView);
            mCameraDevice.setFaceDetectionListener(this);
            mCameraDevice.startFaceDetection();
        }
    }

    @Override
    public void stopFaceDetection() {
        if (!mFaceDetectionStarted) return;
        if (mParameters.getMaxNumDetectedFaces() > 0) {
            mFaceDetectionStarted = false;
            mCameraDevice.setFaceDetectionListener(null);
            mCameraDevice.stopFaceDetection();
            if (mFaceView != null) mFaceView.clear();
        }
    }

    private class ZoomGestureListener extends
        GestureDetector.SimpleOnGestureListener {

        @Override
        public boolean onDoubleTap(MotionEvent e) {
            // Do not trigger double tap zoom if popup window is opened.
            if (mIndicatorControlContainer.getActiveSettingPopup() != null) return false;

            // Perform zoom only when preview is started and snapshot is not in
            // progress.
            if (mPausing || !isCameraIdle() || mCameraState == PREVIEW_STOPPED) {
                return false;
            }

            float x = e.getX();
            float y = e.getY();
            // Dismiss the event if it is out of preview area.
            if (!Util.pointInView(x, y, mPreviewFrameLayout)) {
                return false;
            }

            if (mZoomValue < mZoomMax) {
                // Zoom in to the maximum.
                mZoomValue = mZoomMax;
            } else {
                mZoomValue = 0;
            }

            setCameraParametersWhenIdle(UPDATE_PARAM_ZOOM);

            mZoomControl.setZoomIndex(mZoomValue);

            return true;
        }

        @Override
        public void onLongPress(MotionEvent e) {
            if (mPaused || mCameraDevice == null || !mFirstTimeInitialized
                    || mCameraState == SNAPSHOT_IN_PROGRESS
                    || mCameraState == SWITCHING_CAMERA
                    || mCameraState == PREVIEW_STOPPED) {
                return;
            }

            float x = e.getX();
            float y = e.getY();
            // Dismiss the event if it is out of preview area.
            if (!Util.pointInView(x, y, mPreviewFrameLayout)) {
                return;
            }

            // Do not trigger touch focus if popup window is opened.
            if (collapseCameraControls()) return;

            if (!isSupported(Parameters.FOCUS_MODE_AUTO,mInitialParams.getSupportedFocusModes()) &&
                    mCaptureMode.equals(mTemporalBracketing)) {
                startTemporalBracketing();
                mFocusManager.setTempBracketingState(FocusManager.TempBracketingStates.RUNNING);
                mFocusManager.drawFocusRectangle();
            }

            final String focusMode = mFocusManager.getFocusMode();
            boolean cafActive = false;
            if (focusMode.equals(Parameters.FOCUS_MODE_CONTINUOUS_PICTURE) ||
                    (focusMode.equals(Parameters.FOCUS_MODE_AUTO) &&
                    mFocusManager.getFocusAreas() != null)) {
                cafActive = true;
            }

            // Check if metering area is supported and touch convergence is selected
            if (mMeteringAreaSupported && !cafActive &&
                    mAutoConvergence.equals(mTouchConvergence)) {
            //    if (mS3dViewEnabled || is2DMode()) {
                    mTouchManager.onTouch(e);
/*
                } else {
                    mTouchManager.onTouch(e, mPreviewLayout);
                }
*/
                return;
            }

            // Check if metering area or focus area is supported.
            if (!mFocusAreaSupported && !mMeteringAreaSupported) return;

            // Do touch AF only in continuous AF mode.
            if (!cafActive) {
                return;
            }

            mFocusManager.onSingleTapUp((int) e.getX(), (int) e.getY());
        }

    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent m) {
        if (mCameraState == SWITCHING_CAMERA) return true;
        if ( mGestureDetector != null && mGestureDetector.onTouchEvent(m) ) {
               return true;
        }
        // Check if the popup window should be dismissed first.
        if (m.getAction() == MotionEvent.ACTION_DOWN) {
            float x = m.getX();
            float y = m.getY();
            // Dismiss the mode selection window if the ACTION_DOWN event is out
            // of its view area.
            if ((mModePicker != null) && !Util.pointInView(x, y, mModePicker)) {
                mModePicker.dismissModeSelection();
            }
            // Check if the popup window is visible. Indicator control can be
            // null if camera is not opened yet.
            if (mIndicatorControlContainer != null) {
                View popup = mIndicatorControlContainer.getActiveSettingPopup();
                if (popup != null) {
                    // Let popup window, indicator control or preview frame
                    // handle the event by themselves. Dismiss the popup window
                    // if users touch on other areas.
                    if (!Util.pointInView(x, y, popup)
                            && !Util.pointInView(x, y, mIndicatorControlContainer)
                            && !Util.pointInView(x, y, mPreviewFrameLayout)) {
                        mIndicatorControlContainer.dismissSettingPopup();
                    }
                }
            }
        }

        return super.dispatchTouchEvent(m);
    }

    private final BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            Log.d(TAG, "Received intent action=" + action);
            if (action.equals(Intent.ACTION_MEDIA_MOUNTED)
                    || action.equals(Intent.ACTION_MEDIA_UNMOUNTED)
                    || action.equals(Intent.ACTION_MEDIA_CHECKING)) {
                checkStorage();
            } else if (action.equals(Intent.ACTION_MEDIA_SCANNER_FINISHED)) {
                checkStorage();
                if (!mIsImageCaptureIntent) {
                    getLastThumbnail();
                }
            }
        }
    };

    private void initOnScreenIndicator() {
        mGpsIndicator = (ImageView) findViewById(R.id.onscreen_gps_indicator);
        mExposureIndicator = (TextView) findViewById(R.id.onscreen_exposure_indicator);
        mFlashIndicator = (ImageView) findViewById(R.id.onscreen_flash_indicator);
        mSceneIndicator = (ImageView) findViewById(R.id.onscreen_scene_indicator);
        mWhiteBalanceIndicator =
                (ImageView) findViewById(R.id.onscreen_white_balance_indicator);
        mFocusIndicator = (ImageView) findViewById(R.id.onscreen_focus_indicator);
    }

    @Override
    public void showGpsOnScreenIndicator(boolean hasSignal) {
        if (mGpsIndicator == null) {
            return;
        }
        if (hasSignal) {
            mGpsIndicator.setImageResource(R.drawable.ic_viewfinder_gps_on);
        } else {
            mGpsIndicator.setImageResource(R.drawable.ic_viewfinder_gps_no_signal);
        }
        mGpsIndicator.setVisibility(View.VISIBLE);
    }

    @Override
    public void hideGpsOnScreenIndicator() {
        if (mGpsIndicator == null) {
            return;
        }
        mGpsIndicator.setVisibility(View.GONE);
    }

    private void updateExposureOnScreenIndicator(int value) {
        if (mExposureIndicator == null) {
            return;
        }
        if (value == 0) {
            mExposureIndicator.setText("");
            mExposureIndicator.setVisibility(View.GONE);
        } else {
            float step = mParameters.getExposureCompensationStep();
            mFormatterArgs[0] = value * step;
            mBuilder.delete(0, mBuilder.length());
            mFormatter.format("%+1.1f", mFormatterArgs);
            String exposure = mFormatter.toString();
            mExposureIndicator.setText(exposure);
            mExposureIndicator.setVisibility(View.VISIBLE);
        }
    }

    private void updateFlashOnScreenIndicator(String value) {
        if (mFlashIndicator == null) {
            return;
        }
        if (value == null || Parameters.FLASH_MODE_OFF.equals(value)) {
            mFlashIndicator.setVisibility(View.GONE);
        } else {
            mFlashIndicator.setVisibility(View.VISIBLE);
            if (Parameters.FLASH_MODE_AUTO.equals(value)) {
                mFlashIndicator.setImageResource(R.drawable.ic_indicators_landscape_flash_auto);
            } else if (Parameters.FLASH_MODE_ON.equals(value)) {
                mFlashIndicator.setImageResource(R.drawable.ic_indicators_landscape_flash_on);
            } else {
                // Should not happen.
                mFlashIndicator.setVisibility(View.GONE);
            }
        }
    }

    private void updateSceneOnScreenIndicator(String value) {
        if (mSceneIndicator == null) {
            return;
        }
        boolean isGone = (value == null) || (Parameters.SCENE_MODE_AUTO.equals(value));
        mSceneIndicator.setVisibility(isGone ? View.GONE : View.VISIBLE);
    }

    private void updateWhiteBalanceOnScreenIndicator(String value) {
        if (mWhiteBalanceIndicator == null) {
            return;
        }
        if (value == null || Parameters.WHITE_BALANCE_AUTO.equals(value)) {
            mWhiteBalanceIndicator.setVisibility(View.GONE);
        } else {
            mWhiteBalanceIndicator.setVisibility(View.VISIBLE);
            if (Parameters.WHITE_BALANCE_FLUORESCENT.equals(value)) {
                mWhiteBalanceIndicator.setImageResource(R.drawable.ic_indicators_fluorescent);
            } else if (Parameters.WHITE_BALANCE_INCANDESCENT.equals(value)) {
                mWhiteBalanceIndicator.setImageResource(R.drawable.ic_indicators_incandescent);
            } else if (Parameters.WHITE_BALANCE_DAYLIGHT.equals(value)) {
                mWhiteBalanceIndicator.setImageResource(R.drawable.ic_indicators_sunlight);
            } else if (Parameters.WHITE_BALANCE_CLOUDY_DAYLIGHT.equals(value)) {
                mWhiteBalanceIndicator.setImageResource(R.drawable.ic_indicators_cloudy);
            } else {
                // Should not happen.
                mWhiteBalanceIndicator.setVisibility(View.GONE);
            }
        }
    }

    private void updateFocusOnScreenIndicator(String value) {
        if (mFocusIndicator == null) {
            return;
        }
        // Do not show the indicator if users cannot choose.
        if (mPreferenceGroup.findPreference(CameraSettings.KEY_FOCUS_MODE) == null) {
            mFocusIndicator.setVisibility(View.GONE);
        } else {
            mFocusIndicator.setVisibility(View.VISIBLE);
            if (Parameters.FOCUS_MODE_INFINITY.equals(value)) {
                mFocusIndicator.setImageResource(R.drawable.ic_indicators_landscape);
            } else if (Parameters.FOCUS_MODE_MACRO.equals(value)) {
                mFocusIndicator.setImageResource(R.drawable.ic_indicators_macro);
            } else {
                // Should not happen.
                mFocusIndicator.setVisibility(View.GONE);
            }
        }
    }

    private void updateOnScreenIndicators() {
        updateSceneOnScreenIndicator(mParameters.getSceneMode());
        updateExposureOnScreenIndicator(CameraSettings.readExposure(mPreferences));
        updateFlashOnScreenIndicator(mParameters.getFlashMode());
        updateWhiteBalanceOnScreenIndicator(mParameters.getWhiteBalance());
        updateFocusOnScreenIndicator(mParameters.getFocusMode());
    }

    private final class ShutterCallback
            implements android.hardware.Camera.ShutterCallback {
        @Override
        public void onShutter() {
            mShutterCallbackTime = System.currentTimeMillis();
            mShutterLag = mShutterCallbackTime - mCaptureStartTime;
            Log.v(TAG, "mShutterLag = " + mShutterLag + "ms");
        }
    }

    private final class PostViewPictureCallback implements PictureCallback {
        @Override
        public void onPictureTaken(
                byte [] data, android.hardware.Camera camera) {
            mPostViewPictureCallbackTime = System.currentTimeMillis();
            Log.v(TAG, "mShutterToPostViewCallbackTime = "
                    + (mPostViewPictureCallbackTime - mShutterCallbackTime)
                    + "ms");
        }
    }

    private final class RawPictureCallback implements PictureCallback {
        @Override
        public void onPictureTaken(
                byte [] rawData, android.hardware.Camera camera) {
            mRawPictureCallbackTime = System.currentTimeMillis();
            Log.v(TAG, "mShutterToRawCallbackTime = "
                    + (mRawPictureCallbackTime - mShutterCallbackTime) + "ms");
        }
    }

    private final class JpegPictureCallback implements PictureCallback {
        Location mLocation;

        public JpegPictureCallback(Location loc) {
            mLocation = loc;
        }

        @Override
        public void onPictureTaken(
                final byte [] jpegData, final android.hardware.Camera camera) {
            if (mPausing) {
                if (mBurstImages > 0) {
                    resetBurst();
                    mBurstImages = 0;
                    mHandler.sendEmptyMessageDelayed(RELEASE_CAMERA,
                                                     CAMERA_RELEASE_DELAY);
                }
                return;
            }

            if ( mBurstRunning ) {
                /* If mBurstRunning */
                /* ----> starts calculating the delay and FPS between two Burst frames */
                if(!mIsBurstStampInitialized) {
                    /* Going here only for first Jpeg Callback:
                     * Save it;s timestamp to mBurstTimeStamp
                     */
                    mBurstTimeStamp = System.currentTimeMillis();
                    mIsBurstStampInitialized = true;

                } else {
                    /* Update mBurstTmpTimeStamp with every new callback arrived */
                    mBurstTmpTimeStamp = System.currentTimeMillis();
                }

                if(mBurstTimeStamp != 0 && mBurstTmpTimeStamp != 0){
                    if(mBurstTimeStamp != mBurstTmpTimeStamp &&
                            (mBurstTmpTimeStamp > mBurstTimeStamp)) {
                        mBurstFPSsum += 1000/(mBurstTmpTimeStamp - mBurstTimeStamp);
                    }
                    /* Copy mBurstTmpTimeStamp to mBurstTimeStamp needed for next measurement */
                    mBurstTimeStamp = mBurstTmpTimeStamp;
                }
                /* ----> End of calculating*/
            }

            FocusManager.TempBracketingStates tempState = mFocusManager.getTempBracketingState();
            mJpegPictureCallbackTime = System.currentTimeMillis();
            // If postview callback has arrived, the captured image is displayed
            // in postview callback. If not, the captured image is displayed in
            // raw picture callback.
            if (mPostViewPictureCallbackTime != 0) {
                mShutterToPictureDisplayedTime =
                        mPostViewPictureCallbackTime - mShutterCallbackTime;
                mPictureDisplayedToJpegCallbackTime =
                        mJpegPictureCallbackTime - mPostViewPictureCallbackTime;
            } else {
                mShutterToPictureDisplayedTime =
                        mRawPictureCallbackTime - mShutterCallbackTime;
                mPictureDisplayedToJpegCallbackTime =
                        mJpegPictureCallbackTime - mRawPictureCallbackTime;
            }
            Log.v(TAG, "mPictureDisplayedToJpegCallbackTime = "
                    + mPictureDisplayedToJpegCallbackTime + "ms");

            mFocusManager.updateFocusUI(); // Ensure focus indicator is hidden.
            if (!mIsImageCaptureIntent) {
                startPreview(true);
                setCameraState(IDLE);
                startFaceDetection();
            }

            if (!mIsImageCaptureIntent) {
                // Calculate the width and the height of the jpeg.
                Size s = mParameters.getPictureSize();
                int orientation = Exif.getOrientation(jpegData);
                int width, height;
                if ((mJpegRotation + orientation) % 180 == 0) {
                    width = s.width;
                    height = s.height;
                } else {
                    width = s.height;
                    height = s.width;
                }
                Uri uri = mImageNamer.getUri();
                String title = mImageNamer.getTitle();
                mImageSaver.addImage(jpegData, uri, title, mLocation,
                        width, height, mThumbnailViewWidth, orientation);
            } else {
                mJpegImageData = jpegData;
                if (!mQuickCapture) {
                    showPostCaptureAlert();
                } else {
                    doAttach();
                }
            }

            if (mCaptureMode.equals(mExposureBracketing) ) {
                mBurstImages --;
                if (mBurstImages == 0 ) {
                    mHandler.sendEmptyMessageDelayed(RESTART_PREVIEW, 0);
                }
            }

            //reset burst in case of exposure bracketing
            if (mCaptureMode.equals(mExposureBracketing) && mBurstImages == 0) {
                mBurstImages = EXPOSURE_BRACKETING_COUNT;
                mParameters.set(PARM_BURST, mBurstImages);
                mCameraDevice.setParameters(mParameters);
            }

            if (mCaptureMode.equals(mZoomBracketing) ) {
                mBurstImages --;
                if (mBurstImages == 0 ) {
                    mHandler.sendEmptyMessageDelayed(RESTART_PREVIEW, 0);
                }
            }

            //reset burst in case of zoom bracketing
            if (mCaptureMode.equals(mZoomBracketing) && mBurstImages == 0) {
                mBurstImages = ZOOM_BRACKETING_COUNT;
                mParameters.set(PARM_BURST, mBurstImages);
                mCameraDevice.setParameters(mParameters);
            }

            if ( tempState == FocusManager.TempBracketingStates.RUNNING ) {
                mBurstImages --;

                if (mBurstImages == 0 ) {
                    mHandler.sendEmptyMessageDelayed(RESTART_PREVIEW, 0);
                    mTempBracketingEnabled = true;
                    stopTemporalBracketing();
                }
            }

            if (mBurstRunning) {
                mBurstImages --;

                if (mBurstImages == 0) {
                    Log.v(TAG,"PPM: Average Jpeg Callbacks FPS in Burst:" + (mBurstFPSsum)/mCurrBurstImages);
                    resetBurst();
                    mBurstRunning = false;
                    mIsBurstStampInitialized = false;
                    /* Reset Burst Timestamp initialization if burst completed */
                    mBurstTimeStamp = 0;
                    mBurstTmpTimeStamp = 0;
                    mBurstFPSsum = 0;
                    checkStorage();
                    mHandler.sendEmptyMessageDelayed(RESTART_PREVIEW, 0);
                }
            } else {
                // Check this in advance of each shot so we don't add to shutter
                // latency. It's true that someone else could write to the SD card in
                // the mean time and fill it, but that could have happened between the
                // shutter press and saving the JPEG too.
                checkStorage();
            }

            long now = System.currentTimeMillis();
            mJpegCallbackFinishTime = now - mJpegPictureCallbackTime;
            Log.v(TAG, "mJpegCallbackFinishTime = "
                    + mJpegCallbackFinishTime + "ms");
            mJpegPictureCallbackTime = 0;
        }
    }

    private final class AutoFocusCallback
            implements android.hardware.Camera.AutoFocusCallback {
        @Override
        public void onAutoFocus(
                boolean focused, android.hardware.Camera camera) {
            if (mPaused) return;

            mAutoFocusTime = System.currentTimeMillis() - mFocusStartTime;
            Log.v(TAG, "mAutoFocusTime = " + mAutoFocusTime + "ms");
            setCameraState(IDLE);

            // If focus completes and the snapshot is not started, enable the
            // controls.
            if (mFocusManager.isFocusCompleted() && (!mTempBracketingEnabled)) {
                enableCameraControls(true);
            } else if ( mTempBracketingEnabled ) {
                startTemporalBracketing();
            }

            mFocusManager.onAutoFocus(focused);
        }
    }

    private final class AutoFocusMoveCallback
            implements android.hardware.Camera.AutoFocusMoveCallback {
        @Override
        public void onAutoFocusMoving(
            boolean moving, android.hardware.Camera camera) {
                mFocusManager.onAutoFocusMoving(moving);
        }
    }

    // Each SaveRequest remembers the data needed to save an image.
    private static class SaveRequest {
        byte[] data;
        Uri uri;
        String title;
        Location loc;
        int width, height;
        int thumbnailWidth;
        int orientation;
    }

    // We use a queue to store the SaveRequests that have not been completed
    // yet. The main thread puts the request into the queue. The saver thread
    // gets it from the queue, does the work, and removes it from the queue.
    //
    // The main thread needs to wait for the saver thread to finish all the work
    // in the queue, when the activity's onPause() is called, we need to finish
    // all the work, so other programs (like Gallery) can see all the images.
    //
    // If the queue becomes too long, adding a new request will block the main
    // thread until the queue length drops below the threshold (QUEUE_LIMIT).
    // If we don't do this, we may face several problems: (1) We may OOM
    // because we are holding all the jpeg data in memory. (2) We may ANR
    // when we need to wait for saver thread finishing all the work (in
    // onPause() or gotoGallery()) because the time to finishing a long queue
    // of work may be too long.
    private class ImageSaver extends Thread {
        private static final int QUEUE_LIMIT = 3;

        private ArrayList<SaveRequest> mQueue;
        private Thumbnail mPendingThumbnail;
        private Object mUpdateThumbnailLock = new Object();
        private boolean mStop;

        // Runs in main thread
        public ImageSaver() {
            mQueue = new ArrayList<SaveRequest>();
            start();
        }

        // Runs in main thread
        public void addImage(final byte[] data, Uri uri, String title,
                Location loc, int width, int height, int thumbnailWidth,
                int orientation) {
            SaveRequest r = new SaveRequest();
            r.data = data;
            r.uri = uri;
            r.title = title;
            r.loc = (loc == null) ? null : new Location(loc);  // make a copy
            r.width = width;
            r.height = height;
            r.thumbnailWidth = thumbnailWidth;
            r.orientation = orientation;
            synchronized (this) {
                while (mQueue.size() >= QUEUE_LIMIT) {
                    try {
                        wait();
                    } catch (InterruptedException ex) {
                        // ignore.
                    }
                }
                mQueue.add(r);
                notifyAll();  // Tell saver thread there is new work to do.
            }
        }

        // Runs in saver thread
        @Override
        public void run() {
            while (true) {
                SaveRequest r;
                synchronized (this) {
                    if (mQueue.isEmpty()) {
                        notifyAll();  // notify main thread in waitDone

                        // Note that we can only stop after we saved all images
                        // in the queue.
                        if (mStop) break;

                        try {
                            wait();
                        } catch (InterruptedException ex) {
                            // ignore.
                        }
                        continue;
                    }
                    r = mQueue.get(0);
                }
                storeImage(r.data, r.uri, r.title, r.loc, r.width, r.height,
                        r.thumbnailWidth, r.orientation);
                synchronized (this) {
                    mQueue.remove(0);
                    notifyAll();  // the main thread may wait in addImage
                }
            }
        }

        // Runs in main thread
        public void waitDone() {
            synchronized (this) {
                while (!mQueue.isEmpty()) {
                    try {
                        wait();
                    } catch (InterruptedException ex) {
                        // ignore.
                    }
                }
            }
            updateThumbnail();
        }

        // Runs in main thread
        public void finish() {
            waitDone();
            synchronized (this) {
                mStop = true;
                notifyAll();
            }
            try {
                join();
            } catch (InterruptedException ex) {
                // ignore.
            }
        }

        // Runs in main thread (because we need to update mThumbnailView in the
        // main thread)
        public void updateThumbnail() {
            Thumbnail t;
            synchronized (mUpdateThumbnailLock) {
                mHandler.removeMessages(UPDATE_THUMBNAIL);
                t = mPendingThumbnail;
                mPendingThumbnail = null;
            }

            if (t != null) {
                mThumbnail = t;
                mThumbnailView.setBitmap(mThumbnail.getBitmap());
            }
        }

        // Runs in saver thread
        private void storeImage(final byte[] data, Uri uri, String title,
                Location loc, int width, int height, int thumbnailWidth,
                int orientation) {
            boolean ok = Storage.updateImage(mContentResolver, uri, title, loc,
                    orientation, data, width, height);
            if (ok) {
                boolean needThumbnail;
                synchronized (this) {
                    // If the number of requests in the queue (include the
                    // current one) is greater than 1, we don't need to generate
                    // thumbnail for this image. Because we'll soon replace it
                    // with the thumbnail for some image later in the queue.
                    needThumbnail = (mQueue.size() <= 1);
                }
                if (needThumbnail) {
                    // Create a thumbnail whose width is equal or bigger than
                    // that of the thumbnail view.
                    int ratio = (int) Math.ceil((double) width / thumbnailWidth);
                    int inSampleSize = Integer.highestOneBit(ratio);
                    Thumbnail t = Thumbnail.createThumbnail(
                                data, orientation, inSampleSize, uri);
                    synchronized (mUpdateThumbnailLock) {
                        // We need to update the thumbnail in the main thread,
                        // so send a message to run updateThumbnail().
                        mPendingThumbnail = t;
                        mHandler.sendEmptyMessage(UPDATE_THUMBNAIL);
                    }
                }
                Util.broadcastNewPicture(Camera.this, uri);
            }
        }
    }

    private static class ImageNamer extends Thread {
        private boolean mRequestPending;
        private ContentResolver mResolver;
        private long mDateTaken;
        private int mWidth, mHeight;
        private boolean mStop;
        private Uri mUri;
        private String mTitle;

        // Runs in main thread
        public ImageNamer() {
            start();
        }

        // Runs in main thread
        public synchronized void prepareUri(ContentResolver resolver,
                long dateTaken, int width, int height, int rotation) {
            if (rotation % 180 != 0) {
                int tmp = width;
                width = height;
                height = tmp;
            }
            mRequestPending = true;
            mResolver = resolver;
            mDateTaken = dateTaken;
            mWidth = width;
            mHeight = height;
            notifyAll();
        }

        // Runs in main thread
        public synchronized Uri getUri() {
            // wait until the request is done.
            while (mRequestPending) {
                try {
                    wait();
                } catch (InterruptedException ex) {
                    // ignore.
                }
            }

            // return the uri generated
            Uri uri = mUri;
            mUri = null;
            return uri;
        }

        // Runs in main thread, should be called after getUri().
        public synchronized String getTitle() {
            return mTitle;
        }

        // Runs in namer thread
        @Override
        public synchronized void run() {
            while (true) {
                if (mStop) break;
                if (!mRequestPending) {
                    try {
                        wait();
                    } catch (InterruptedException ex) {
                        // ignore.
                    }
                    continue;
                }
                cleanOldUri();
                generateUri();
                mRequestPending = false;
                notifyAll();
            }
            cleanOldUri();
        }

        // Runs in main thread
        public synchronized void finish() {
            mStop = true;
            notifyAll();
        }

        // Runs in namer thread
        private void generateUri() {
            mTitle = Util.createJpegName(mDateTaken);
            mUri = Storage.newImage(mResolver, mTitle, mDateTaken, mWidth, mHeight);
        }

        // Runs in namer thread
        private void cleanOldUri() {
            if (mUri == null) return;
            Storage.deleteImage(mResolver, mUri);
            mUri = null;
        }
    }

    private void setCameraState(int state) {
        mCameraState = state;
        switch (state) {
            case SNAPSHOT_IN_PROGRESS:
            case FOCUSING:
            case SWITCHING_CAMERA:
                enableCameraControls(false);
                break;
            case IDLE:
            case PREVIEW_STOPPED:
                enableCameraControls(true);
                break;
        }
    }

    @Override
    public boolean capture() {
        // If we are already in the middle of taking a snapshot then ignore.
        if (mCameraDevice == null || mCameraState == SNAPSHOT_IN_PROGRESS
                || mCameraState == SWITCHING_CAMERA) {
            return false;
        }
        mCaptureStartTime = System.currentTimeMillis();
        mPostViewPictureCallbackTime = 0;
        mJpegImageData = null;

        // Set rotation and gps data.
        mJpegRotation = Util.getJpegRotation(mCameraId, mOrientation);
        mParameters.setRotation(mJpegRotation);
        Location loc = mLocationManager.getCurrentLocation();
        Util.setGpsParameters(mParameters, loc);
        mCameraDevice.setParameters(mParameters);

        mCameraDevice.takePicture(mShutterCallback, mRawPictureCallback,
                mPostViewPictureCallback, new JpegPictureCallback(loc));

        Size size = mParameters.getPictureSize();
        mImageNamer.prepareUri(mContentResolver, mCaptureStartTime,
                size.width, size.height, mJpegRotation);

        if (!mIsImageCaptureIntent) {
            // Start capture animation.
            mCameraScreenNail.animateCapture(getCameraRotation());
        }
        mFaceDetectionStarted = false;
        setCameraState(SNAPSHOT_IN_PROGRESS);
        return true;
    }

    private int getCameraRotation() {
        return (mOrientationCompensation - mDisplayRotation + 360) % 360;
    }

    @Override
    public void setFocusParameters() {
        setCameraParameters(UPDATE_PARAM_PREFERENCE);
    }

    @Override
    public void setTouchParameters() {
        mParameters = mCameraDevice.getParameters();
        mParameters.setMeteringAreas(mTouchManager.getMeteringAreas());
        mCameraDevice.setParameters(mParameters);
    }

    @Override
    public void playSound(int soundId) {
        mCameraSound.play(soundId);
    }

    private int getPreferredCameraId(ComboPreferences preferences) {
        int intentCameraId = Util.getCameraFacingIntentExtras(this);
        if (intentCameraId != -1) {
            // Testing purpose. Launch a specific camera through the intent
            // extras.
            return intentCameraId;
        } else {
            return CameraSettings.readPreferredCameraId(preferences);
        }
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mPreferences = new ComboPreferences(this);
        CameraSettings.upgradeGlobalPreferences(mPreferences.getGlobal());
        mCameraId = getPreferredCameraId(mPreferences);
        PreferenceInflater inflater = new PreferenceInflater(this);
        PreferenceGroup group =
            (PreferenceGroup) inflater.inflate(R.xml.camera_preferences);

        mContentResolver = getContentResolver();

        // To reduce startup time, open the camera and start the preview in
        // another thread.
        mCameraStartUpThread = new CameraStartUpThread();
        mCameraStartUpThread.start();

        setContentView(R.layout.camera);

        // Surface texture is from camera screen nail and startPreview needs it.
        // This must be done before startPreview.
        mIsImageCaptureIntent = isImageCaptureIntent();
        createCameraScreenNail(!mIsImageCaptureIntent);

        ListPreference gbce = group.findPreference(CameraSettings.KEY_GBCE);
        if (gbce != null) {
            mGBCEOff = gbce.findEntryValueByEntry(getString(R.string.pref_camera_gbce_entry_off));
            if (mGBCEOff == null) {
                mGBCEOff = "";
            }
        }

        ListPreference autoConvergencePreference = group.findPreference(CameraSettings.KEY_AUTO_CONVERGENCE);
        if (autoConvergencePreference != null) {
            mTouchConvergence = autoConvergencePreference.findEntryValueByEntry(getString(R.string.pref_camera_autoconvergence_entry_mode_touch));
            if (mTouchConvergence == null) {
                mTouchConvergence = "";
            }
            mManualConvergence = autoConvergencePreference.findEntryValueByEntry(getString(R.string.pref_camera_autoconvergence_entry_mode_manual));
            if (mManualConvergence == null) {
                mManualConvergence = "";
            }
        }

        ListPreference exposure = group.findPreference(CameraSettings.KEY_EXPOSURE_MODE_MENU);
        if (exposure != null) {
            mManualExposure = exposure.findEntryValueByEntry(getString(R.string.pref_camera_exposuremode_entry_manual));
            if (mManualExposure == null) {
                mManualExposure = "";
            }
        }

        ListPreference captureMode = group.findPreference(CameraSettings.KEY_MODE_MENU);
        if (captureMode != null) {
            mTemporalBracketing = captureMode.findEntryValueByEntry(getString(R.string.pref_camera_mode_entry_temporal_bracketing));
            if (mTemporalBracketing == null) {
                mTemporalBracketing = "";
            }

            mExposureBracketing = captureMode.findEntryValueByEntry(getString(R.string.pref_camera_mode_entry_exp_bracketing));
            if (mExposureBracketing == null) {
                mExposureBracketing = "";
            }

            mZoomBracketing = captureMode.findEntryValueByEntry(getString(R.string.pref_camera_mode_entry_zoom_bracketing));
            if (mZoomBracketing == null) {
                mZoomBracketing = "";
            }


            mHighPerformance = captureMode.findEntryValueByEntry(getString(R.string.pref_camera_mode_entry_hs));
            if (mHighPerformance == null) {
                mHighPerformance = "";
            }

            mHighQuality = captureMode.findEntryValueByEntry(getString(R.string.pref_camera_mode_entry_hq));
            if (mHighQuality == null) {
                mHighQuality = "";
            }

            mHighQualityZsl = captureMode.findEntryValueByEntry(getString(R.string.pref_camera_mode_entry_zsl));
            if (mHighQualityZsl == null) {
                mHighQualityZsl = "";
            }
        }
        mTouchManager = new TouchManager();
        mPreferences.setLocalId(this, mCameraId);
        CameraSettings.upgradeLocalPreferences(mPreferences.getLocal());
        mFocusAreaIndicator = (RotateLayout) findViewById(
                R.id.focus_indicator_rotate_layout);
        // we need to reset exposure for the preview
        resetExposureCompensation();
        // Starting the preview needs preferences, camera screen nail, and
        // focus area indicator.
        mStartPreviewPrerequisiteReady.open();

        initializeControlByIntent();
        mRotateDialog = new RotateDialogController(this, R.layout.rotate_dialog);
        mNumberOfCameras = CameraHolder.instance().getNumberOfCameras();
        mQuickCapture = getIntent().getBooleanExtra(EXTRA_QUICK_CAPTURE, false);
        initializeMiscControls();
        mLocationManager = new LocationManager(this, this);
        initOnScreenIndicator();
        // Make sure all views are disabled before camera is open.
        enableCameraControls(false);
    }

    private void overrideCameraSettings(final String flashMode,
            final String whiteBalance, final String focusMode, final String exposure) {
        if (mIndicatorControlContainer != null) {
            mIndicatorControlContainer.enableFilter(true);
            mIndicatorControlContainer.overrideSettings(
                    CameraSettings.KEY_FLASH_MODE, flashMode,
                    CameraSettings.KEY_WHITE_BALANCE, whiteBalance,
                    CameraSettings.KEY_FOCUS_MODE, focusMode,
                    CameraSettings.KEY_EXPOSURE, exposure);
            mIndicatorControlContainer.enableFilter(false);
        }
    }

    private void overrideCameraGBCE(final String gbce) {
        if (gbce != null) {
            Editor editor = mPreferences.edit();
            editor.putString(CameraSettings.KEY_GBCE, gbce);
            editor.apply();
            mParameters.set(PARM_GBCE, gbce);
            mCameraDevice.setParameters(mParameters);
            if (mIndicatorControlContainer != null) {
                mIndicatorControlContainer.reloadPreferences();
            }
        }
    }

    private void overrideCameraBurst(final String burst) {
        if (burst != null) {
            Editor editor = mPreferences.edit();
            editor.putString(CameraSettings.KEY_BURST, burst);
            editor.apply();
            if ( !mCaptureMode.equals(mExposureBracketing) &&
                    !mCaptureMode.equals(mZoomBracketing)) {
                mParameters.set(PARM_BURST, burst);
                mCameraDevice.setParameters(mParameters);
            }
            mBurstRunning = false;
            if (mIndicatorControlContainer != null) {
                mIndicatorControlContainer.reloadPreferences();
            }
        }
    }

    private void updateSceneModeUI() {
        // If scene mode is set, we cannot set flash mode, white balance, and
        // focus mode, instead, we read it from driver
        if (!Parameters.SCENE_MODE_AUTO.equals(mSceneMode)) {
            overrideCameraSettings(mParameters.getFlashMode(),
                    mParameters.getWhiteBalance(), mParameters.getFocusMode(),
                    getString(R.string.pref_exposure_default));
        } else {
            overrideCameraSettings(null, null, null, null);
        }

        // GBCE/GLBCE is only available when in HQ mode
        if ( !mCaptureMode.equals(mHighQuality) ) {
            overrideCameraGBCE(mGBCEOff);
        } else {
            overrideCameraGBCE(null);
        }

        if ( !mCaptureMode.equals(mHighPerformance) ) {
            overrideCameraBurst(getString(R.string.pref_camera_burst_default));
        } else {
            overrideCameraBurst(null);
        }
    }

    private void loadCameraPreferences() {
        CameraSettings settings = new CameraSettings(this, mInitialParams,
                mCameraId, CameraHolder.instance().getCameraInfo());
        mPreferenceGroup = settings.getPreferenceGroup(R.xml.camera_preferences);
    }

    private void initializeIndicatorControl() {
        // setting the indicator buttons.
        mIndicatorControlContainer =
                (IndicatorControlContainer) findViewById(R.id.indicator_control);
        loadCameraPreferences();
        final String[] SETTING_KEYS = {
                CameraSettings.KEY_FLASH_MODE,
                CameraSettings.KEY_WHITE_BALANCE,
                CameraSettings.KEY_EXPOSURE,
                CameraSettings.KEY_SCENE_MODE};
        final String[] OTHER_SETTING_KEYS = {
                CameraSettings.KEY_RECORD_LOCATION,
                CameraSettings.KEY_AUTO_CONVERGENCE,
                CameraSettings.KEY_FOCUS_MODE,
                CameraSettings.KEY_MODE_MENU,
                CameraSettings.KEY_BURST,
                CameraSettings.KEY_GBCE,
                CameraSettings.KEY_ISO,
                CameraSettings.KEY_BRACKET_RANGE,
                CameraSettings.KEY_CONTRAST,
                CameraSettings.KEY_BRIGHTNESS,
                CameraSettings.KEY_SHARPNESS,
                CameraSettings.KEY_SATURATION,
                CameraSettings.KEY_EXPOSURE_MODE_MENU,
                CameraSettings.KEY_PREVIEW_FRAMERATE,
                CameraSettings.KEY_JPEG_QUALITY,
                CameraSettings.KEY_COLOR_EFFECT,
                CameraSettings.KEY_ANTIBANDING,
                CameraSettings.KEY_PREVIEW_SIZE,
                CameraSettings.KEY_PICTURE_SIZE};

        CameraPicker.setImageResourceId(R.drawable.ic_switch_photo_facing_holo_light);
        mIndicatorControlContainer.initialize(this, mPreferenceGroup,
                mParameters.isZoomSupported(),false,
                SETTING_KEYS, OTHER_SETTING_KEYS);
        mCameraPicker = (CameraPicker) mIndicatorControlContainer.findViewById(
                R.id.camera_picker);
        updateSceneModeUI();
        mIndicatorControlContainer.setListener(this);
    }

    private boolean collapseCameraControls() {
        if ((mIndicatorControlContainer != null)
                && mIndicatorControlContainer.dismissSettingPopup()) {
            return true;
        }
        if (mModePicker != null && mModePicker.dismissModeSelection()) return true;
        return false;
    }

    private void enableCameraControls(boolean enable) {
        if (mIndicatorControlContainer != null) {
            mIndicatorControlContainer.setEnabled(enable);
        }
        if (mModePicker != null) mModePicker.setEnabled(enable);
        if (mZoomControl != null) mZoomControl.setEnabled(enable);
        if (mThumbnailView != null) mThumbnailView.setEnabled(enable);
    }

    private class MyOrientationEventListener
            extends OrientationEventListener {
        public MyOrientationEventListener(Context context) {
            super(context);
        }

        @Override
        public void onOrientationChanged(int orientation) {
            // We keep the last known orientation. So if the user first orient
            // the camera then point the camera to floor or sky, we still have
            // the correct orientation.
            if (orientation == ORIENTATION_UNKNOWN) return;
            mOrientation = Util.roundOrientation(orientation, mOrientation);
            // When the screen is unlocked, display rotation may change. Always
            // calculate the up-to-date orientationCompensation.
            int orientationCompensation =
                    (mOrientation + Util.getDisplayRotation(Camera.this)) % 360;
            if (mOrientationCompensation != orientationCompensation) {
                mOrientationCompensation = orientationCompensation;
                setOrientationIndicator(mOrientationCompensation, true);
            }

            // Show the toast after getting the first orientation changed.
            if (mHandler.hasMessages(SHOW_TAP_TO_FOCUS_TOAST)) {
                mHandler.removeMessages(SHOW_TAP_TO_FOCUS_TOAST);
                showTapToFocusToast();
            }
        }
    }

    private void setOrientationIndicator(int orientation, boolean animation) {
        Rotatable[] indicators = {mThumbnailView, mModePicker,
                mIndicatorControlContainer, mZoomControl, mFocusAreaIndicator, mFaceView,
                mReviewDoneButton, mRotateDialog, mOnScreenIndicators};
        for (Rotatable indicator : indicators) {
            if (indicator != null) indicator.setOrientation(orientation, animation);
        }

        // We change the orientation of the review cancel button only for tablet
        // UI because there's a label along with the X icon. For phone UI, we
        // don't change the orientation because there's only a symmetrical X
        // icon.
        if (mReviewCancelButton instanceof RotateLayout) {
            mReviewCancelButton.setOrientation(orientation, animation);
        }
    }

    @Override
    public void onStop() {
        super.onStop();
        if (mMediaProviderClient != null) {
            mMediaProviderClient.release();
            mMediaProviderClient = null;
        }
    }

    private void checkStorage() {
        mStorageSpace = Storage.getAvailableSpace();
        updateStorageHint(mStorageSpace);
    }

    @OnClickAttr
    public void onThumbnailClicked(View v) {
        if (isCameraIdle() && mThumbnail != null) {
            if (mImageSaver != null) mImageSaver.waitDone();
            gotoGallery();
        }
    }

    // onClick handler for R.id.btn_retake
    @OnClickAttr
    public void onReviewRetakeClicked(View v) {
        if (mPaused) return;

        hidePostCaptureAlert();
        startPreview(true);
        setCameraState(IDLE);
        startFaceDetection();
    }

    // onClick handler for R.id.btn_done
    @OnClickAttr
    public void onReviewDoneClicked(View v) {
        doAttach();
    }

    // onClick handler for R.id.btn_cancel
    @OnClickAttr
    public void onReviewCancelClicked(View v) {
        doCancel();
    }

    private void doAttach() {
        if (mPaused) {
            return;
        }

        byte[] data = mJpegImageData;

        if (mCropValue == null) {
            // First handle the no crop case -- just return the value.  If the
            // caller specifies a "save uri" then write the data to its
            // stream. Otherwise, pass back a scaled down version of the bitmap
            // directly in the extras.
            if (mSaveUri != null) {
                OutputStream outputStream = null;
                try {
                    outputStream = mContentResolver.openOutputStream(mSaveUri);
                    outputStream.write(data);
                    outputStream.close();

                    setResultEx(RESULT_OK);
                    finish();
                } catch (IOException ex) {
                    // ignore exception
                } finally {
                    Util.closeSilently(outputStream);
                }
            } else {
                int orientation = Exif.getOrientation(data);
                Bitmap bitmap = Util.makeBitmap(data, 50 * 1024);
                bitmap = Util.rotate(bitmap, orientation);
                setResultEx(RESULT_OK,
                        new Intent("inline-data").putExtra("data", bitmap));
                finish();
            }
        } else {
            // Save the image to a temp file and invoke the cropper
            Uri tempUri = null;
            FileOutputStream tempStream = null;
            try {
                File path = getFileStreamPath(sTempCropFilename);
                path.delete();
                tempStream = openFileOutput(sTempCropFilename, 0);
                tempStream.write(data);
                tempStream.close();
                tempUri = Uri.fromFile(path);
            } catch (FileNotFoundException ex) {
                setResultEx(Activity.RESULT_CANCELED);
                finish();
                return;
            } catch (IOException ex) {
                setResultEx(Activity.RESULT_CANCELED);
                finish();
                return;
            } finally {
                Util.closeSilently(tempStream);
            }

            Bundle newExtras = new Bundle();
            if (mCropValue.equals("circle")) {
                newExtras.putString("circleCrop", "true");
            }
            if (mSaveUri != null) {
                newExtras.putParcelable(MediaStore.EXTRA_OUTPUT, mSaveUri);
            } else {
                newExtras.putBoolean("return-data", true);
            }

            Intent cropIntent = new Intent("com.ti.omap.android.camera.action.CROP");

            cropIntent.setData(tempUri);
            cropIntent.putExtras(newExtras);

            startActivityForResult(cropIntent, REQUEST_CROP);
        }
    }

    private void doCancel() {
        setResultEx(RESULT_CANCELED, new Intent());
        finish();
    }

    @Override
    public void onShutterButtonFocus(boolean pressed) {
        if (mPaused || collapseCameraControls()
                || (mCameraState == SNAPSHOT_IN_PROGRESS)
                || (mCameraState == PREVIEW_STOPPED)) return;

        // Do not do focus if there is not enough storage.
        if (pressed && !canTakePicture()) return;

        if (pressed) {
            mFocusManager.onShutterDown();
        } else {
            mFocusManager.onShutterUp();
        }
    }

    @Override
    public void onShutterButtonClick() {
        if (mPaused || collapseCameraControls()
                || (mCameraState == SWITCHING_CAMERA)
                || (mCameraState == PREVIEW_STOPPED)) return;

        // Do not take the picture if there is not enough storage.
        if (mStorageSpace <= Storage.LOW_STORAGE_THRESHOLD) {
            Log.i(TAG, "Not enough space or storage not ready. remaining=" + mStorageSpace);
            return;
        }
        Log.v(TAG, "onShutterButtonClick: mCameraState=" + mCameraState);

        // If the user wants to do a snapshot while the previous one is still
        // in progress, remember the fact and do it after we finish the previous
        // one and re-start the preview. Snapshot in progress also includes the
        // state that autofocus is focusing and a picture will be taken when
        // focus callback arrives.
        if ((mFocusManager.isFocusingSnapOnFinish() || mCameraState == SNAPSHOT_IN_PROGRESS)
                && !mIsImageCaptureIntent) {
            mSnapshotOnIdle = true;
            return;
        }

        mSnapshotOnIdle = false;
        mFocusManager.doSnap();
    }

    @Override
    public void onShutterButtonLongPressed() {
        if (mPausing || mCameraState == SNAPSHOT_IN_PROGRESS
                || mCameraDevice == null ) return;

        Log.v(TAG, "onShutterButtonLongPressed");
        mFocusManager.shutterLongPressed();

        if (!isSupported(Parameters.FOCUS_MODE_AUTO,mInitialParams.getSupportedFocusModes()) &&
                mCaptureMode.equals(mTemporalBracketing)) {
            startTemporalBracketing();
            mFocusManager.setTempBracketingState(FocusManager.TempBracketingStates.RUNNING);
            mFocusManager.drawFocusRectangle();
        }
    }

    private void installIntentFilter() {
        // install an intent filter to receive SD card related events.
        IntentFilter intentFilter =
                new IntentFilter(Intent.ACTION_MEDIA_MOUNTED);
        intentFilter.addAction(Intent.ACTION_MEDIA_UNMOUNTED);
        intentFilter.addAction(Intent.ACTION_MEDIA_SCANNER_FINISHED);
        intentFilter.addAction(Intent.ACTION_MEDIA_CHECKING);
        intentFilter.addDataScheme("file");
        registerReceiver(mReceiver, intentFilter);
        mDidRegister = true;
    }

    private void initDefaults() {
        if ( null == mAutoConvergence || mPausing ) {
            mAutoConvergence = getString(R.string.pref_camera_autoconvergence_default);
        }

        if ( null == mAntibanding || mPausing ) {
            mAntibanding = getString(R.string.pref_camera_antibanding_default);
        }

        if ( null == mContrast || mPausing ) {
            mContrast = getString(R.string.pref_camera_contrast_default);
        }

        if ( null == mExposureMode || mPausing ) {
            mExposureMode = getString(R.string.pref_camera_exposuremode_default);
        }

        if ( null == mBrightness || mPausing ) {
            mBrightness = getString(R.string.pref_camera_brightness_default);
        }

        if ( null == mSaturation || mPausing ) {
            mSaturation = getString(R.string.pref_camera_saturation_default);
        }

        if ( null == mSharpness || mPausing ) {
            mSharpness = getString(R.string.pref_camera_sharpness_default);
        }

        if ( null == mISO || mPausing ) {
            mISO = getString(R.string.pref_camera_iso_default);
        }

        if ( null == mColorEffect || mPausing ) {
            mColorEffect = getString(R.string.pref_camera_coloreffect_default);
        }

        if (mCaptureMode.equals(mTemporalBracketing) && mPausing) {
            mCaptureMode = getString(R.string.pref_camera_mode_default);
        }
    }

    @Override
    protected void onResume() {
        mPaused = false;
        super.onResume();
        if (mOpenCameraFail || mCameraDisabled) return;

        mHandler.removeMessages(RELEASE_CAMERA);

        initDefaults();

        // Set capture mode to empty value after resuming the app,
        // so updateCameraParametersPreference can set it correcly
        if (mFirstTimeInitialized)
        {
            mCaptureMode = "";
        }

        mPausing = false;

        mJpegPictureCallbackTime = 0;
        mZoomValue = 0;

        if(mFaceView == null) {
            mFaceView = (FaceView) findViewById(R.id.face_view);
        }

        // Start the preview if it is not started.
        if (mCameraState == PREVIEW_STOPPED && mCameraStartUpThread == null) {
            resetExposureCompensation();
            mCameraStartUpThread = new CameraStartUpThread();
            mCameraStartUpThread.start();
        }

        if (!mIsImageCaptureIntent) getLastThumbnail();

        // If first time initialization is not finished, put it in the
        // message queue.
        if (!mFirstTimeInitialized) {
            mHandler.sendEmptyMessage(FIRST_TIME_INIT);
        } else {
            initializeSecondTime();
        }
        keepScreenOnAwhile();

        // Dismiss open menu if exists.
        PopupManager.getInstance(this).notifyShowPopup(null);

        if (mCameraSound == null) {
            mCameraSound = new MediaActionSound();
            // Not required, but reduces latency when playback is requested later.
            mCameraSound.load(MediaActionSound.FOCUS_COMPLETE);
        }
    }

    void waitCameraStartUpThread() {
        try {
            if (mCameraStartUpThread != null) {
                mCameraStartUpThread.cancel();
                mCameraStartUpThread.join();
                mCameraStartUpThread = null;
                setCameraState(IDLE);
            }
        } catch (InterruptedException e) {
            // ignore
        }
    }

    @Override
    protected void onPause() {
        mPaused = true;
        super.onPause();

        // Wait the camera start up thread to finish.
        waitCameraStartUpThread();

        stopPreview();
        // Close the camera now because other activities may need to use it.
        closeCamera();
        if (mSurfaceTexture != null) {
            mCameraScreenNail.releaseSurfaceTexture();
            mSurfaceTexture = null;
        }
        if (mCameraSound != null) {
            mCameraSound.release();
            mCameraSound = null;
        }
        resetScreenOn();

        // Clear UI.
        collapseCameraControls();
        if (mFaceView != null) mFaceView.clear();

        if (mFirstTimeInitialized) {
            mOrientationListener.disable();
            if (mImageSaver != null) {
                mImageSaver.finish();
                mImageSaver = null;
                mImageNamer.finish();
                mImageNamer = null;
            }
        }

        if (mDidRegister) {
            unregisterReceiver(mReceiver);
            mDidRegister = false;
        }
        if (mLocationManager != null) mLocationManager.recordLocation(false);
        updateExposureOnScreenIndicator(0);

        // If we are in an image capture intent and has taken
        // a picture, we just clear it in onPause.
        mJpegImageData = null;

        // Remove the messages in the event queue.
        mHandler.removeMessages(FIRST_TIME_INIT);
        mHandler.removeMessages(CHECK_DISPLAY_ROTATION);
        mHandler.removeMessages(SWITCH_CAMERA);
        mHandler.removeMessages(SWITCH_CAMERA_START_ANIMATION);
        mHandler.removeMessages(CAMERA_OPEN_DONE);
        mHandler.removeMessages(START_PREVIEW_DONE);
        mHandler.removeMessages(OPEN_CAMERA_FAIL);
        mHandler.removeMessages(CAMERA_DISABLED);

        mPendingSwitchCameraId = -1;
        if (mFocusManager != null) mFocusManager.removeMessages();
    }

    private void initializeControlByIntent() {
        if (mIsImageCaptureIntent) {
            // Cannot use RotateImageView for "done" and "cancel" button because
            // the tablet layout uses RotateLayout, which cannot be cast to
            // RotateImageView.
            mReviewDoneButton = (Rotatable) findViewById(R.id.btn_done);
            mReviewCancelButton = (Rotatable) findViewById(R.id.btn_cancel);
            mReviewRetakeButton = findViewById(R.id.btn_retake);
            findViewById(R.id.btn_cancel).setVisibility(View.VISIBLE);

            // Not grayed out upon disabled, to make the follow-up fade-out
            // effect look smooth. Note that the review done button in tablet
            // layout is not a TwoStateImageView.
            if (mReviewDoneButton instanceof TwoStateImageView) {
                ((TwoStateImageView) mReviewDoneButton).enableFilter(false);
            }

            setupCaptureParams();
        } else {
            mThumbnailView = (RotateImageView) findViewById(R.id.thumbnail);
            mThumbnailView.enableFilter(false);
            mThumbnailView.setVisibility(View.VISIBLE);
            mThumbnailViewWidth = mThumbnailView.getLayoutParams().width;

            mModePicker = (ModePicker) findViewById(R.id.mode_picker);
            mModePicker.setVisibility(View.VISIBLE);
            mModePicker.setOnModeChangeListener(this);
            mModePicker.setCurrentMode(ModePicker.MODE_CAMERA);
        }
    }

    private void initializeFocusManager() {
        // Create FocusManager object. startPreview needs it.
        CameraInfo info = CameraHolder.instance().getCameraInfo()[mCameraId];
        boolean mirror = (info.facing == CameraInfo.CAMERA_FACING_FRONT);
        String[] defaultFocusModes = getResources().getStringArray(
                R.array.pref_camera_focusmode_default_array);
        mFocusManager = new FocusManager(mPreferences, defaultFocusModes,
                mFocusAreaIndicator, mInitialParams, this, mirror,
                getMainLooper());
    }

    private void initializeMiscControls() {
        // startPreview needs this.
        mPreviewFrameLayout = (PreviewFrameLayout) findViewById(R.id.frame);
        // Set touch focus listener.
        setSingleTapUpListener(mPreviewFrameLayout);

        mZoomControl = (ZoomControl) findViewById(R.id.zoom_control);
        mOnScreenIndicators = (Rotatable) findViewById(R.id.on_screen_indicators);
        mFaceView = (FaceView) findViewById(R.id.face_view);
        mPreviewFrameLayout.addOnLayoutChangeListener(this);
        mPreviewFrameLayout.setOnSizeChangedListener(this);
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        setDisplayOrientation();

        // Change layout in response to configuration change
        LinearLayout appRoot = (LinearLayout) findViewById(R.id.camera_app_root);
        appRoot.setOrientation(
                newConfig.orientation == Configuration.ORIENTATION_LANDSCAPE
                ? LinearLayout.HORIZONTAL : LinearLayout.VERTICAL);
        appRoot.removeAllViews();
        LayoutInflater inflater = getLayoutInflater();
        inflater.inflate(R.layout.preview_frame, appRoot);
        inflater.inflate(R.layout.camera_control, appRoot);

        // from onCreate()
        initializeControlByIntent();
        initializeFocusManager();
        initializeMiscControls();
        initializeIndicatorControl();
        mFocusAreaIndicator = (RotateLayout) findViewById(
                R.id.focus_indicator_rotate_layout);
        mFocusManager.setFocusAreaIndicator(mFocusAreaIndicator);

        // from onResume()
        if (!mIsImageCaptureIntent) updateThumbnailView();

        // from initializeFirstTime()
        mShutterButton = (ShutterButton) findViewById(R.id.shutter_button);
        mShutterButton.setOnShutterButtonListener(this);
        mShutterButton.setOnShutterButtonLongPressListener(this);
        mShutterButton.setVisibility(View.VISIBLE);

        initializeZoom();
        initOnScreenIndicator();
        updateOnScreenIndicators();
        mFaceView.clear();
        mFaceView.setVisibility(View.VISIBLE);
        mFaceView.setDisplayOrientation(mDisplayOrientation);
        CameraInfo info = CameraHolder.instance().getCameraInfo()[mCameraId];
        mFaceView.setMirror(info.facing == CameraInfo.CAMERA_FACING_FRONT);
        mFaceView.resume();
        mFocusManager.setFaceView(mFaceView);
    }

    @Override
    protected void onActivityResult(
            int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        switch (requestCode) {
            case REQUEST_CROP: {
                Intent intent = new Intent();
                if (data != null) {
                    Bundle extras = data.getExtras();
                    if (extras != null) {
                        intent.putExtras(extras);
                    }
                }
                setResultEx(resultCode, intent);
                finish();

                File path = getFileStreamPath(sTempCropFilename);
                path.delete();

                break;
            }
        }
    }

    private boolean canTakePicture() {
        return isCameraIdle() && (mStorageSpace > Storage.LOW_STORAGE_THRESHOLD);
    }

    @Override
    public void autoFocus() {
        mFocusStartTime = System.currentTimeMillis();
        mCameraDevice.autoFocus(mAutoFocusCallback);
        setCameraState(FOCUSING);
    }

    @Override
    public void cancelAutoFocus() {
        mCameraDevice.cancelAutoFocus();
        setCameraState(IDLE);
        setCameraParameters(UPDATE_PARAM_PREFERENCE);
    }

    // Preview area is touched. Handle touch focus.
    @Override
    protected void onSingleTapUp(View view, int x, int y) {
        if (mPaused || mCameraDevice == null || !mFirstTimeInitialized
                || mCameraState == SNAPSHOT_IN_PROGRESS
                || mCameraState == SWITCHING_CAMERA
                || mCameraState == PREVIEW_STOPPED) {
            return;
        }
    }

    @Override
    public void onBackPressed() {
        if (!isCameraIdle()) {
            // ignore backs while we're taking a picture
            return;
        } else if (!collapseCameraControls()) {
            super.onBackPressed();
        }
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_FOCUS:
                if (mFirstTimeInitialized && event.getRepeatCount() == 0) {
                    onShutterButtonFocus(true);
                }
                return true;
            case KeyEvent.KEYCODE_CAMERA:
                if (mFirstTimeInitialized && event.getRepeatCount() == 0) {
                    onShutterButtonClick();
                }
                return true;
            case KeyEvent.KEYCODE_DPAD_CENTER:
                // If we get a dpad center event without any focused view, move
                // the focus to the shutter button and press it.
                if (mFirstTimeInitialized && event.getRepeatCount() == 0) {
                    // Start auto-focus immediately to reduce shutter lag. After
                    // the shutter button gets the focus, onShutterButtonFocus()
                    // will be called again but it is fine.
                    if (collapseCameraControls()) return true;
                    onShutterButtonFocus(true);
                    if (mShutterButton.isInTouchMode()) {
                        mShutterButton.requestFocusFromTouch();
                    } else {
                        mShutterButton.requestFocus();
                    }
                    mShutterButton.setPressed(true);
                }
                return true;
        }

        return super.onKeyDown(keyCode, event);
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_FOCUS:
                if (mFirstTimeInitialized) {
                    onShutterButtonFocus(false);
                }
                return true;
        }
        return super.onKeyUp(keyCode, event);
    }

    private void closeCamera() {
        if (mCameraDevice != null) {
            mCameraDevice.setZoomChangeListener(null);
            mCameraDevice.setFaceDetectionListener(null);
            mCameraDevice.setErrorCallback(null);
            CameraHolder.instance().release();
            mFaceDetectionStarted = false;
            mCameraDevice = null;
            setCameraState(PREVIEW_STOPPED);
            mFocusManager.onCameraReleased();
        }
    }

    private void setDisplayOrientation() {
        mDisplayRotation = Util.getDisplayRotation(this);
        mDisplayOrientation = Util.getDisplayOrientation(mDisplayRotation, mCameraId);
        mCameraDisplayOrientation = Util.getDisplayOrientation(0, mCameraId);
        if (mFaceView != null) {
            mFaceView.setDisplayOrientation(mDisplayOrientation);
        }
        mFocusManager.setDisplayOrientation(mDisplayOrientation);
    }

    private void startPreview(boolean updateAll) {
        mFocusManager.resetTouchFocus();

        mCameraDevice.setErrorCallback(mErrorCallback);

        // If we're previewing already, stop the preview first (this will blank
        // the screen).
        if (mCameraState != PREVIEW_STOPPED) stopPreview();

        setDisplayOrientation();
        mCameraDevice.setDisplayOrientation(mCameraDisplayOrientation);

        if (!mSnapshotOnIdle) {
            // If the focus mode is continuous autofocus, call cancelAutoFocus to
            // resume it because it may have been paused by autoFocus call.
            if (Parameters.FOCUS_MODE_CONTINUOUS_PICTURE.equals(mFocusManager.getFocusMode())) {
                mCameraDevice.cancelAutoFocus();
            }
            mFocusManager.setAeAwbLock(false); // Unlock AE and AWB.
        }

        if ( updateAll ) {
            Log.v(TAG, "Updating all parameters!");
            setCameraParameters(UPDATE_PARAM_INITIALIZE | UPDATE_PARAM_ZOOM | UPDATE_PARAM_PREFERENCE);
        } else {
            setCameraParameters(UPDATE_PARAM_MODE);
        }

        if (mSurfaceTexture == null) {
            Size size = mParameters.getPreviewSize();
            if (mCameraDisplayOrientation % 180 == 0) {
                mCameraScreenNail.setSize(size.width, size.height);
            } else {
                mCameraScreenNail.setSize(size.height, size.width);
            }
            notifyScreenNailChanged();
            mCameraScreenNail.acquireSurfaceTexture();
            mSurfaceTexture = mCameraScreenNail.getSurfaceTexture();
        }

        mCameraDevice.setPreviewTextureAsync(mSurfaceTexture);
        try {
            Log.v(TAG, "startPreview");
            mCameraDevice.startPreviewAsync();
        } catch (Throwable ex) {
            closeCamera();
            throw new RuntimeException("startPreview failed", ex);
        }

        //Set Camera State to IDLE, but do not enabelCameraControls, because
        //it is in another thread.
        mCameraState = IDLE;

        mFocusManager.onPreviewStarted();
        if ( mTempBracketingEnabled ) {
            mFocusManager.setTempBracketingState(FocusManager.TempBracketingStates.ACTIVE);
        }

        if (mSnapshotOnIdle) {
            mHandler.post(mDoSnapRunnable);
        }
    }

    private void startTemporalBracketing() {
        mParameters.set(CameraSettings.KEY_TEMPORAL_BRACKETING, TRUE);
        mCameraDevice.setParameters(mParameters);
        mTempBracketingStarted = true;
    }

    private void stopTemporalBracketing() {
        mFocusManager.setTempBracketingState(FocusManager.TempBracketingStates.OFF);
        mParameters.set(CameraSettings.KEY_TEMPORAL_BRACKETING, FALSE);
        mCameraDevice.setParameters(mParameters);
        mParameters.remove(CameraSettings.KEY_TEMPORAL_BRACKETING);
        mTempBracketingStarted = false;
    }

    private void stopPreview() {
        if (mCameraDevice != null && mCameraState != PREVIEW_STOPPED) {
            Log.v(TAG, "stopPreview");

            if ( mTempBracketingEnabled ) {
                stopTemporalBracketing();
            }

            mCameraDevice.cancelAutoFocus(); // Reset the focus.
            mCameraDevice.stopPreview();
            mFaceDetectionStarted = false;
        }
        setCameraState(PREVIEW_STOPPED);
        if (mFocusManager != null) mFocusManager.onPreviewStopped();
    }

    private static boolean isSupported(String value, List<String> supported) {
        return supported == null ? false : supported.indexOf(value) >= 0;
    }

    @SuppressWarnings("deprecation")
    private void updateCameraParametersInitialize() {
        // Reset preview frame rate to the maximum because it may be lowered by
        // video camera application.
        List<int[]> frameRates = mParameters.getSupportedPreviewFpsRange();
        if (frameRates != null) {
            int last = frameRates.size() - 1;
            int min = (frameRates.get(last))[mParameters.PREVIEW_FPS_MIN_INDEX];
            int max = (frameRates.get(last))[mParameters.PREVIEW_FPS_MAX_INDEX];
            mParameters.setPreviewFpsRange(min,max);
        }

        mParameters.setRecordingHint(false);

        // Disable video stabilization. Convenience methods not available in API
        // level <= 14
        String vstabSupported = mParameters.get("video-stabilization-supported");
        if ("true".equals(vstabSupported)) {
            mParameters.set("video-stabilization", "false");
        }
    }

    private void updateCameraParametersZoom() {
        // Set zoom.
        if (mParameters.isZoomSupported()) {
            mParameters.setZoom(mZoomValue);
        }
    }

    private String elementExists(String[] firstArr, String[] secondArr){
        for (int i = 0; i < firstArr.length; i++) {
            for (int j = 0; j < secondArr.length; j++) {
                if (firstArr[i].equals(secondArr[j])) {
                    return firstArr[i];
                }
            }
        }
        return null;
    }

    private ListPreference getSupportedListPreference( String supportedKey, String menuKey){
        List<String> supported = new ArrayList<String>();
        ListPreference menu = mPreferenceGroup.findPreference(menuKey);
        if (menu == null) return null;
        if (supportedKey != null) {
            String supp = mParameters.get(supportedKey);
                if (supp !=null && !supp.equals("")) {
                    for (String item : supp.split(",")) {
                        supported.add(item);
                    }
                }
        }
        CameraSettings.filterUnsupportedOptions(mPreferenceGroup, menu, supported);
        return menu;
    }

    private ListPreference getSupportedListPreference(List<Integer> supportedIntegers, String menuKey){
        List<String> supported = new ArrayList<String>();
        ListPreference menu = mPreferenceGroup.findPreference(menuKey);
        if ( ( menu != null ) && !supportedIntegers.isEmpty() ) {
            for (Integer item : supportedIntegers) {
                supported.add(item.toString());
            }
            CameraSettings.filterUnsupportedOptions(mPreferenceGroup, menu, supported);
        }

        return menu;
    }

    private void updateListPreference(ListPreference listPreference, String menuItem) {
        if ((listPreference == null) || (menuItem == null) || (mIndicatorControlContainer == null)) return;
        ArrayList<CharSequence[]> allEntries = new ArrayList<CharSequence[]>();
        ArrayList<CharSequence[]> allEntryValues = new ArrayList<CharSequence[]>();
        allEntries.add(listPreference.getEntries());
        allEntryValues.add(listPreference.getEntryValues());
        mIndicatorControlContainer.replace(menuItem,
                                           listPreference,
                                           allEntries,
                                           allEntryValues);
    }

    private boolean updateCameraParametersPreference() {
        boolean restartNeeded = false;
        boolean captureModeUpdated = false;
        boolean controlUpdateNeeded = false;

        if (mAeLockSupported) {
            mParameters.setAutoExposureLock(mFocusManager.getAeAwbLock());
        }

        if (mAwbLockSupported) {
            mParameters.setAutoWhiteBalanceLock(mFocusManager.getAeAwbLock());
        }

        if (mFocusAreaSupported) {
            mParameters.setFocusAreas(mFocusManager.getFocusAreas());
        }

        if (mMeteringAreaSupported) {
            // Use the same area for focus and metering.
            mParameters.setMeteringAreas(mFocusManager.getMeteringAreas());
        }

        // Auto Convergence
        String autoConvergence = mPreferences.getString(
                CameraSettings.KEY_AUTO_CONVERGENCE,
                getString(R.string.pref_camera_autoconvergence_default));

        if (!autoConvergence.equals(mAutoConvergence)) {
            mParameters.set(CameraSettings.KEY_AUTOCONVERGENCE_MODE, autoConvergence);
            mAutoConvergence = autoConvergence;
            if (!isConvergenceInit) isConvergenceInit = true;
            else if (autoConvergence.equals(mManualConvergence) && isManualConvergence) {
                int convergenceMin = Integer.parseInt(mParameters.get(CameraSettings.KEY_SUPPORTED_MANUAL_CONVERGENCE_MIN));
                int convergenceMax = Integer.parseInt(mParameters.get(CameraSettings.KEY_SUPPORTED_MANUAL_CONVERGENCE_MAX));
                int convergenceStep = Integer.parseInt(mParameters.get(CameraSettings.KEY_SUPPORTED_MANUAL_CONVERGENCE_STEP));
                int convergenceValue = Integer.parseInt(mParameters.get(CameraSettings.KEY_MANUAL_CONVERGENCE));
                ManualConvergenceSettings manualConvergenceDialog = new ManualConvergenceSettings(this, mHandler,
                        convergenceValue, convergenceMin, convergenceMax, convergenceStep);
                Editor edit = mPreferences.edit();
                edit.putString(CameraSettings.KEY_AUTO_CONVERGENCE, autoConvergence);
                edit.commit();
                manualConvergenceDialog.show();
                isManualConvergence = false;
            } else {
                isManualConvergence = true;
            }
        }

        // Capture mode
        String mode = mPreferences.getString(
                CameraSettings.KEY_MODE_MENU, getString(R.string.pref_camera_mode_default));
        if ( !mCaptureMode.equals(mode) ) {
            restartNeeded = true;
            captureModeUpdated = true;
            mCaptureMode = mode;
            setCaptureMode(mode, mParameters);
            // Capture mode can be applied only
            // when preview is stopped.
            mCameraDevice.stopPreview();
            mCameraDevice.setParameters(mParameters);
            mParameters = mCameraDevice.getParameters();
            mInitialParams = mParameters;
            Log.v(TAG,"Capture mode set: " + mParameters.get(CameraSettings.KEY_MODE));

            CameraSettings settings = new CameraSettings(this,
                                                         mParameters,
                                                         mCameraId,
                                                         CameraHolder.instance().getCameraInfo());
            mPreferenceGroup = settings.getPreferenceGroup(R.xml.camera_preferences);

            // Update picture size UI with the new sizes
            List<String> supported = new ArrayList<String>();
            supported = CameraSettings.sizeListToStringList(mParameters.getSupportedPictureSizes());
            ListPreference sizePreference = getSupportedListPreference(supported.toString(),
                                                                       CameraSettings.KEY_PICTURE_SIZE);

            updateListPreference(sizePreference, CameraSettings.KEY_PICTURE_SIZE);

            // Update framerate UI with the new entries
            ListPreference fpsPreference = getSupportedListPreference(mParameters.getSupportedPreviewFrameRates(),
                                                                      CameraSettings.KEY_PREVIEW_FRAMERATE);

            updateListPreference(fpsPreference, CameraSettings.KEY_PREVIEW_FRAMERATE);
        }

        // Set Preview Framerate
        String framerate =  mPreferences.getString(
                CameraSettings.KEY_PREVIEW_FRAMERATE, null);
        if ( ( framerate == null ) || captureModeUpdated ) {
            CameraSettings.initialFrameRate(this, mParameters);
            restartNeeded = true;
        } else if (!framerate.equals(mLastPreviewFramerate) ) {
            mParameters.setPreviewFpsRange(Integer.parseInt(framerate) * 1000,
                    Integer.parseInt(framerate) * 1000);
            mLastPreviewFramerate = framerate;
            restartNeeded = true;
         }

        // Set picture size.
        String pictureSize = mPreferences.getString(
                CameraSettings.KEY_PICTURE_SIZE, null);
        if (pictureSize == null) {
            CameraSettings.initialCameraPictureSize(this, mParameters);
        } else {
            List<String> supported = new ArrayList<String>();
            supported = CameraSettings.sizeListToStringList(mParameters.getSupportedPictureSizes());
            CameraSettings.setCameraPictureSize(pictureSize, supported, mParameters);
        }
        Size size = mParameters.getPictureSize();

        // ISO
        String iso = mPreferences.getString(
                    CameraSettings.KEY_ISO, getString(R.string.pref_camera_iso_default));

        if ( !iso.equals(mISO) ) {
            mParameters.set(PARM_ISO, iso);
            mISO = iso;
        }

        // Set JPEG quality.
        String quality = mPreferences.getString(CameraSettings.KEY_JPEG_QUALITY,
                                                 getString(R.string.pref_camera_jpegquality_default));
        int jpegQuality = CameraProfile.QUALITY_HIGH;
        try {
            jpegQuality = Integer.parseInt(quality);
        } catch (NumberFormatException e) {
            e.printStackTrace();
        }

        if ( mJpegQuality != jpegQuality ) {
            mJpegQuality = jpegQuality;
            jpegQuality = CameraProfile.getJpegEncodingQualityParameter(mCameraId, mJpegQuality);
            mParameters.setJpegQuality(jpegQuality);
        }

        // Set Contrast
        String contrast = mPreferences.getString(
                CameraSettings.KEY_CONTRAST,
                getString(R.string.pref_camera_contrast_default));

        if ( !contrast.equals(mContrast) ) {
            mParameters.set(PARM_CONTRAST, Integer.parseInt(contrast) );
            mContrast = contrast;
        }
        // Set brightness
        String brightness = mPreferences.getString(
                CameraSettings.KEY_BRIGHTNESS,
                getString(R.string.pref_camera_brightness_default));

        if ( !brightness.equals(mBrightness) ) {
            mParameters.set(PARM_BRIGHTNESS, Integer.parseInt(brightness) );
            mBrightness = brightness;
        }
        // Set Saturation
        String saturation =  mPreferences.getString(
                CameraSettings.KEY_SATURATION,
                getString(R.string.pref_camera_saturation_default));

        if ( !saturation.equals(mSaturation) ) {
            mParameters.set(PARM_SATURATION, Integer.parseInt(saturation) );
            mSaturation = saturation;
        }
        // Set Sharpness
        String sharpness = mPreferences.getString(
                CameraSettings.KEY_SHARPNESS,
                getString(R.string.pref_camera_sharpness_default));

        if ( !sharpness.equals(mSharpness) ) {
            mParameters.set(PARM_SHARPNESS, Integer.parseInt(sharpness) );
            mSharpness = sharpness;
        }

        // Color Effects
        String colorEffect = mPreferences.getString(
                CameraSettings.KEY_COLOR_EFFECT,
                getString(R.string.pref_camera_coloreffect_default));

        if (isSupported(colorEffect, mParameters.getSupportedColorEffects()) &&
             !colorEffect.equals(mColorEffect) ) {
            mParameters.setColorEffect(colorEffect);
            mColorEffect = colorEffect;
        }

        //Antibanding
        String antibanding = mPreferences.getString(
                CameraSettings.KEY_ANTIBANDING,
                getString(R.string.pref_camera_antibanding_default));

        if ( !antibanding.equals(mAntibanding) ) {
            mParameters.setAntibanding(antibanding);
            mAntibanding = antibanding;
        }

     // Exposure mode
        String exposureMode = mPreferences.getString(
                    CameraSettings.KEY_EXPOSURE_MODE_MENU, getString(R.string.pref_camera_exposuremode_default));

        if (exposureMode != null && !exposureMode.equals(mExposureMode)) {
            // 3d DualCamera Mode
            mParameters.set(PARM_EXPOSURE_MODE, exposureMode);
            mExposureMode = exposureMode;
            if (!isExposureInit) isExposureInit = true;
            else if (exposureMode.equals(mManualExposure) && isManualExposure) { // ManualExposure Mode
                isManualExposure = false;
                ManualGainExposureSettings manualGainExposureDialog = null;
                if (mCameraId < 2) {
                    int expMin = Integer.parseInt(mParameters.get(CameraSettings.KEY_SUPPORTED_MANUAL_EXPOSURE_MIN));
                    int expMax = Integer.parseInt(mParameters.get(CameraSettings.KEY_SUPPORTED_MANUAL_EXPOSURE_MAX));
                    int expStep = Integer.parseInt(mParameters.get(CameraSettings.KEY_SUPPORTED_MANUAL_EXPOSURE_STEP));
                    int isoMin = Integer.parseInt(mParameters.get(CameraSettings.KEY_SUPPORTED_MANUAL_GAIN_ISO_MIN));
                    int isoMax = Integer.parseInt(mParameters.get(CameraSettings.KEY_SUPPORTED_MANUAL_GAIN_ISO_MAX));
                    int isoStep = Integer.parseInt(mParameters.get(CameraSettings.KEY_SUPPORTED_MANUAL_GAIN_ISO_STEP));
                    int expValue = Integer.parseInt(mParameters.get(CameraSettings.KEY_MANUAL_EXPOSURE));
                    int isoValue = Integer.parseInt(mParameters.get(CameraSettings.KEY_MANUAL_GAIN_ISO));

                    manualGainExposureDialog = new ManualGainExposureSettings(this, mHandler,
                            expValue, isoValue,expMin, expMax,
                            isoMin, isoMax,expStep,isoStep);
                    Editor edit = mPreferences.edit();
                    edit.putString(CameraSettings.KEY_EXPOSURE_MODE_MENU, exposureMode);
                    edit.commit();
                    manualGainExposureDialog.show();
                } else {
                    int expMin = Integer.parseInt(mParameters.get(CameraSettings.KEY_SUPPORTED_MANUAL_EXPOSURE_MIN));
                    int expMax = Integer.parseInt(mParameters.get(CameraSettings.KEY_SUPPORTED_MANUAL_EXPOSURE_MAX));
                    int expStep = Integer.parseInt(mParameters.get(CameraSettings.KEY_SUPPORTED_MANUAL_EXPOSURE_STEP));
                    int isoMin = Integer.parseInt(mParameters.get(CameraSettings.KEY_SUPPORTED_MANUAL_GAIN_ISO_MIN));
                    int isoMax = Integer.parseInt(mParameters.get(CameraSettings.KEY_SUPPORTED_MANUAL_GAIN_ISO_MAX));
                    int isoStep = Integer.parseInt(mParameters.get(CameraSettings.KEY_SUPPORTED_MANUAL_GAIN_ISO_STEP));
                    int expRightValue = Integer.parseInt(mParameters.get(CameraSettings.KEY_MANUAL_EXPOSURE_RIGHT));
                    int expLeftValue = Integer.parseInt(mParameters.get(CameraSettings.KEY_MANUAL_EXPOSURE));
                    int isoRightValue = Integer.parseInt(mParameters.get(CameraSettings.KEY_MANUAL_GAIN_ISO_RIGHT));
                    int isoLeftValue = Integer.parseInt(mParameters.get(CameraSettings.KEY_MANUAL_GAIN_ISO));

                    manualGainExposureDialog = new ManualGainExposureSettings(this, mHandler,
                            expRightValue, expLeftValue, isoRightValue, isoLeftValue,
                            expMin, expMax, isoMin, isoMax,expStep,isoStep);
                    Editor edit = mPreferences.edit();
                    edit.putString(CameraSettings.KEY_EXPOSURE_MODE_MENU, exposureMode);
                    edit.commit();
                }
                manualGainExposureDialog.show();
            } else {
                isManualExposure = true;
                if (mCameraId < 2) {
                    mParameters.set(CameraSettings.KEY_MANUAL_EXPOSURE, 0);
                    mParameters.set(CameraSettings.KEY_MANUAL_GAIN_ISO, 0);
                } else {
                    mParameters.set(CameraSettings.KEY_MANUAL_EXPOSURE, 0);
                    mParameters.set(CameraSettings.KEY_MANUAL_EXPOSURE_RIGHT, 0);
                    mParameters.set(CameraSettings.KEY_MANUAL_GAIN_ISO, 0);
                    mParameters.set(CameraSettings.KEY_MANUAL_GAIN_ISO_RIGHT, 0);
                }
            }
        }

        // Since change scene mode may change supported values,
        // Set scene mode first,
        mSceneMode = mPreferences.getString(
                CameraSettings.KEY_SCENE_MODE,
                getString(R.string.pref_camera_scenemode_default));
        if (isSupported(mSceneMode, mParameters.getSupportedSceneModes())) {
            if (!mParameters.getSceneMode().equals(mSceneMode)) {
                mParameters.setSceneMode(mSceneMode);

                // Setting scene mode will change the settings of flash mode,
                // white balance, and focus mode. Here we read back the
                // parameters, so we can know those settings.
                mCameraDevice.setParameters(mParameters);
                mParameters = mCameraDevice.getParameters();
            }
        } else {
            mSceneMode = mParameters.getSceneMode();
            if (mSceneMode == null) {
                mSceneMode = Parameters.SCENE_MODE_AUTO;
            }
        }

        // For the following settings, we need to check if the settings are
        // still supported by latest driver, if not, ignore the settings.

        String[] previewDefaults = getResources().getStringArray(R.array.pref_camera_previewsize_default_array);
        String defaultPreviewSize = "";
        String[] previewSizes2D = getResources().getStringArray(R.array.pref_camera_previewsize_entryvalues);
        defaultPreviewSize = elementExists(previewDefaults, previewSizes2D);


        String previewSize = mPreferences.getString(CameraSettings.KEY_PREVIEW_SIZE, defaultPreviewSize);
        if (previewSize !=null && !previewSize.equals(mPreviewSize)) {
            List<String> supported = new ArrayList<String>();
            supported = CameraSettings.sizeListToStringList(mParameters.getSupportedPreviewSizes());
            CameraSettings.setCameraPreviewSize(previewSize, supported, mParameters);
            mPreviewSize = previewSize;
        }

        if (setPreviewFrameLayoutAspectRatio()) {
            controlUpdateNeeded = true;
        }

        if (Parameters.SCENE_MODE_AUTO.equals(mSceneMode)) {
            // Set flash mode.
            String flashMode = mPreferences.getString(
                    CameraSettings.KEY_FLASH_MODE,
                    getString(R.string.pref_camera_flashmode_default));
            List<String> supportedFlash = mParameters.getSupportedFlashModes();
            if (isSupported(flashMode, supportedFlash)) {
                mParameters.setFlashMode(flashMode);
            } else {
                flashMode = mParameters.getFlashMode();
                if (flashMode == null) {
                    flashMode = getString(
                            R.string.pref_camera_flashmode_no_flash);
                }
            }

            // Set white balance parameter.
            String whiteBalance = mPreferences.getString(
                    CameraSettings.KEY_WHITE_BALANCE,
                    getString(R.string.pref_camera_whitebalance_default));
            if (isSupported(whiteBalance,
                    mParameters.getSupportedWhiteBalance())) {
                mParameters.setWhiteBalance(whiteBalance);
            } else {
                whiteBalance = mParameters.getWhiteBalance();
                if (whiteBalance == null) {
                    whiteBalance = Parameters.WHITE_BALANCE_AUTO;
                }
            }

            // Set focus mode.
            String focusMode = mFocusManager.getFocusMode();
            mFocusManager.overrideFocusMode(null);
            mParameters.setFocusMode(focusMode);
        } else {
            resetExposureCompensation();

            mFocusManager.overrideFocusMode(mParameters.getFocusMode());
            // Set Color Effects to None.
            Editor editor = mPreferences.edit();
            editor.putString(CameraSettings.KEY_COLOR_EFFECT, Parameters.EFFECT_NONE);
            editor.commit();
            mParameters.setColorEffect(Parameters.EFFECT_NONE);

            // Set ISO to auto
            editor.putString(CameraSettings.KEY_ISO, "auto");
            editor.commit();
            mParameters.set(PARM_ISO, getString(R.string.pref_camera_iso_default));

         // Set Exposure mode to auto
            editor.putString(CameraSettings.KEY_EXPOSURE_MODE_MENU,
                    getString(R.string.pref_camera_exposuremode_default));
            editor.commit();
            mParameters.set(PARM_EXPOSURE_MODE, getString(R.string.pref_camera_exposuremode_default));

            // Set Contrast to Normal
            editor.putString(CameraSettings.KEY_CONTRAST, getString(R.string.pref_camera_contrast_default));
            editor.commit();
            mParameters.set(PARM_CONTRAST, getString(R.string.pref_camera_contrast_default));

            // Set brightness to Normal
            editor.putString(CameraSettings.KEY_BRIGHTNESS, getString(R.string.pref_camera_brightness_default));
            editor.commit();
            mParameters.set(PARM_BRIGHTNESS, getString(R.string.pref_camera_brightness_default));

            // Set Saturation to Normal
            editor.putString(CameraSettings.KEY_SATURATION, getString(R.string.pref_camera_saturation_default));
            editor.commit();
            mParameters.set(PARM_SATURATION, getString(R.string.pref_camera_saturation_default));

            // Set Sharpness to Normal
            editor.putString(CameraSettings.KEY_SHARPNESS, getString(R.string.pref_camera_sharpness_default));
            editor.commit();
            mParameters.set(PARM_SHARPNESS, getString(R.string.pref_camera_sharpness_default));

            // Set antibanding to Normal
            editor.putString(CameraSettings.KEY_ANTIBANDING, getString(R.string.pref_camera_antibanding_default));
            editor.commit();
            mParameters.setAntibanding(getString(R.string.pref_camera_antibanding_default));

            if (mIndicatorControlContainer != null) {
                mIndicatorControlContainer.reloadPreferences();
            }
        }

        // Set exposure compensation
        int value = CameraSettings.readExposure(mPreferences);
        int max = mParameters.getMaxExposureCompensation();
        int min = mParameters.getMinExposureCompensation();
        if (value >= min && value <= max) {
            mParameters.setExposureCompensation(value);
        } else {
            Log.w(TAG, "invalid exposure range: " + value);
        }

        // GBCE/GLBCE
        String gbce = mPreferences.getString(
                CameraSettings.KEY_GBCE, PARM_GBCE_OFF);

        if ( PARM_GLBCE.equals(gbce) ) {
            mParameters.set(PARM_GBCE, FALSE);
            mParameters.set(PARM_GLBCE, TRUE);
        } else if ( PARM_GBCE.equals(gbce) ) {
            mParameters.set(PARM_GBCE, TRUE);
            mParameters.set(PARM_GLBCE, FALSE);
        } else {
            mParameters.set(PARM_GBCE, FALSE);
            mParameters.set(PARM_GLBCE, FALSE);
        }

        if (mContinousFocusSupported) {
            if (mParameters.getFocusMode().equals(Parameters.FOCUS_MODE_CONTINUOUS_PICTURE)) {
                mCameraDevice.setAutoFocusMoveCallback(mAutoFocusMoveCallback);
            } else {
                mCameraDevice.setAutoFocusMoveCallback(null);
            }
        }

        String bracketRange =  mPreferences.getString(
                CameraSettings.KEY_BRACKET_RANGE,
                getString(R.string.pref_camera_bracketrange_default));

        if ( mTempBracketingEnabled ) {
            mParameters.set(PARM_TEMPORAL_BRACKETING_RANGE_POS, Integer.parseInt(bracketRange) );
            mParameters.set(PARM_TEMPORAL_BRACKETING_RANGE_NEG, Integer.parseInt(bracketRange) );
            mBracketRange = bracketRange;
            mBurstImages = Integer.parseInt(mBracketRange) * 2 + 1;
        }

        int burst = Integer.parseInt(mPreferences.getString(
                CameraSettings.KEY_BURST,
                getString(R.string.pref_camera_burst_default)));

        if (( burst != mBurstImages ) &&
                ( mode.equals(mHighPerformance) ) ) {
              mBurstImages = burst;
              mCurrBurstImages = mBurstImages -1;
              mParameters.set(PARM_BURST, mBurstImages);
              if ( 0 < mBurstImages ) {
                  mBurstRunning = true;
              }
        }

        if ((mIndicatorControlContainer != null) && (controlUpdateNeeded)) {
            mIndicatorControlContainer.requestLayout();
        }

        return restartNeeded;
    }

    // We separate the parameters into several subsets, so we can update only
    // the subsets actually need updating. The PREFERENCE set needs extra
    // locking because the preference can be changed from GLThread as well.
    private void setCameraParameters(int updateSet) {
        mParameters = mCameraDevice.getParameters();

        boolean restartPreview = false;
        if ((updateSet & UPDATE_PARAM_INITIALIZE) != 0) {
            updateCameraParametersInitialize();
        }

        if ((updateSet & UPDATE_PARAM_ZOOM) != 0) {
            updateCameraParametersZoom();
        }

        if ((updateSet & UPDATE_PARAM_PREFERENCE) != 0) {
            restartPreview = updateCameraParametersPreference();
        }

        if ((updateSet & UPDATE_PARAM_MODE) != 0 ) {
               updateCameraParametersPreference();
        }

        mCameraDevice.setParameters(mParameters);
        if ( ( restartPreview ) && ( mCameraState != PREVIEW_STOPPED ) ) {
            // This will only restart the preview
            // without trying to apply any new
            // camera parameters.
            mFaceDetectionStarted = false;
            Message msg = new Message();
            msg.what = RESTART_PREVIEW;
            msg.arg1 = MODE_RESTART;
            mHandler.sendMessage(msg);
        }
    }

    // If the Camera is idle, update the parameters immediately, otherwise
    // accumulate them in mUpdateSet and update later.
    private void setCameraParametersWhenIdle(int additionalUpdateSet) {
        mUpdateSet |= additionalUpdateSet;
        if (mCameraDevice == null) {
            // We will update all the parameters when we open the device, so
            // we don't need to do anything now.
            mUpdateSet = 0;
            return;
        } else if (isCameraIdle()) {
            setCameraParameters(mUpdateSet);
            updateSceneModeUI();
            mUpdateSet = 0;
        } else {
            if (!mHandler.hasMessages(SET_CAMERA_PARAMETERS_WHEN_IDLE)) {
                mHandler.sendEmptyMessageDelayed(
                        SET_CAMERA_PARAMETERS_WHEN_IDLE, 1000);
            }
        }
    }

    private void setCaptureMode(String mode, Parameters params) {
        if ( mode.equals(mHighPerformance) ) {
            params.set(CameraSettings.KEY_MODE, mHighPerformance);
            params.set(PARM_IPP, PARM_IPP_NONE);
            mTempBracketingEnabled = false;
            params.remove(PARM_EXPOSURE_BRACKETING_RANGE);
            params.set(CameraSettings.KEY_TEMPORAL_BRACKETING, FALSE);
            params.remove(PARM_ZOOM_BRACKETING_RANGE);
        } else if ( mode.equals(mHighQuality) ) {
            params.set(CameraSettings.KEY_MODE, mHighQuality);
            params.set(PARM_IPP, PARM_IPP_LDCNSF);
            mTempBracketingEnabled = false;
            params.remove(PARM_EXPOSURE_BRACKETING_RANGE);
            params.set(CameraSettings.KEY_TEMPORAL_BRACKETING, FALSE);
            params.remove(PARM_ZOOM_BRACKETING_RANGE);
        } else if ( mode.equals(mHighQualityZsl) ) {
            params.set(CameraSettings.KEY_MODE, mHighQualityZsl);
            params.set(PARM_IPP, PARM_IPP_LDCNSF);
            mTempBracketingEnabled = false;
            params.remove(PARM_EXPOSURE_BRACKETING_RANGE);
            params.set(CameraSettings.KEY_TEMPORAL_BRACKETING, FALSE);
            params.remove(PARM_ZOOM_BRACKETING_RANGE);
        } else if ( mode.equals(mTemporalBracketing) ) {
            params.set(CameraSettings.KEY_MODE, mTemporalBracketing);
            params.set(PARM_IPP, PARM_IPP_NONE);
            params.set(PARM_GBCE, PARM_GBCE_OFF);
            params.remove(PARM_ZOOM_BRACKETING_RANGE);

            //Enable Temporal Bracketing
            mTempBracketingEnabled = true;
            params.remove(PARM_EXPOSURE_BRACKETING_RANGE);
            params.remove(PARM_ZOOM_BRACKETING_RANGE);

        } else if ( mode.equals(mExposureBracketing) ) {

            params.set(CameraSettings.KEY_MODE, mExposureBracketing);
            //Disable GBCE by default
            params.set(PARM_GBCE, PARM_GBCE_OFF);
            params.set(PARM_IPP, PARM_IPP_NONE);
            params.set(PARM_BURST, EXPOSURE_BRACKETING_COUNT);
            params.set(PARM_EXPOSURE_BRACKETING_RANGE, EXPOSURE_BRACKETING_RANGE_VALUE);
            mBurstImages = EXPOSURE_BRACKETING_COUNT;

            //Disable Temporal Brackerting
            mTempBracketingEnabled = false;
            params.set(CameraSettings.KEY_TEMPORAL_BRACKETING, FALSE);
            params.remove(PARM_ZOOM_BRACKETING_RANGE);

        } else if ( mode.equals(mZoomBracketing) ) {

            params.set(CameraSettings.KEY_MODE, mZoomBracketing);
            //Disable GBCE by default
            params.set(PARM_GBCE, PARM_GBCE_OFF);
            params.set(PARM_IPP, PARM_IPP_NONE);
            params.set(PARM_BURST, ZOOM_BRACKETING_COUNT);
            params.set(PARM_ZOOM_BRACKETING_RANGE,
                    getString(R.string.pref_camera_zoom_bracketing_rangevalues));
            mBurstImages = ZOOM_BRACKETING_COUNT;
            params.remove(PARM_EXPOSURE_BRACKETING_RANGE);

            //Disable Temporal Brackerting
            mTempBracketingEnabled = false;
            params.set(CameraSettings.KEY_TEMPORAL_BRACKETING, FALSE);

        } else {
            // Default to HQ with LDC&NSF
            params.set(CameraSettings.KEY_MODE, mHighQuality);
            params.set(PARM_IPP, PARM_IPP_LDCNSF);
            params.remove(PARM_EXPOSURE_BRACKETING_RANGE);
            params.remove(PARM_ZOOM_BRACKETING_RANGE);
        }

        if ( null != mFocusManager ) {
            if( mTempBracketingEnabled ) {
                mFocusManager.setTempBracketingState(FocusManager.TempBracketingStates.ACTIVE);
            }
            else {
                mFocusManager.setTempBracketingState(FocusManager.TempBracketingStates.OFF);
            }
        }
    }

    private boolean isCameraIdle() {
        return (mCameraState == IDLE) ||
                ((mFocusManager != null) && mFocusManager.isFocusCompleted()
                        && (mCameraState != SWITCHING_CAMERA));
    }

    private boolean isImageCaptureIntent() {
        String action = getIntent().getAction();
        return (MediaStore.ACTION_IMAGE_CAPTURE.equals(action));
    }

    private void setupCaptureParams() {
        Bundle myExtras = getIntent().getExtras();
        if (myExtras != null) {
            mSaveUri = (Uri) myExtras.getParcelable(MediaStore.EXTRA_OUTPUT);
            mCropValue = myExtras.getString("crop");
        }
    }

    private void showPostCaptureAlert() {
        if (mIsImageCaptureIntent) {
            Util.fadeOut(mIndicatorControlContainer);
            Util.fadeOut(mShutterButton);

            Util.fadeIn(mReviewRetakeButton);
            Util.fadeIn((View) mReviewDoneButton);
        }
    }

    private void hidePostCaptureAlert() {
        if (mIsImageCaptureIntent) {
            Util.fadeOut(mReviewRetakeButton);
            Util.fadeOut((View) mReviewDoneButton);

            Util.fadeIn(mShutterButton);
            if (mIndicatorControlContainer != null) {
                Util.fadeIn(mIndicatorControlContainer);
            }
        }
    }

    private void switchToOtherMode(int mode) {
        if (isFinishing()) return;
        if (mImageSaver != null) mImageSaver.waitDone();
        if (mThumbnail != null) ThumbnailHolder.keep(mThumbnail);
        MenuHelper.gotoMode(mode, Camera.this);
        mHandler.removeMessages(FIRST_TIME_INIT);
        finish();
    }

    @Override
    public void onModeChanged(int mode) {
        if (mode != ModePicker.MODE_CAMERA) switchToOtherMode(mode);
    }

    @Override
    public void onSharedPreferenceChanged() {
        // ignore the events after "onPause()"
        if (mPaused) return;

        boolean recordLocation = RecordLocationPreference.get(
                mPreferences, mContentResolver);
        mLocationManager.recordLocation(recordLocation);

        setCameraParametersWhenIdle(UPDATE_PARAM_PREFERENCE);
        setPreviewFrameLayoutAspectRatio();
        updateOnScreenIndicators();
    }

    @Override
    public void onCameraPickerClicked(int cameraId) {
        if (mPaused || mPendingSwitchCameraId != -1) return;

        Log.v(TAG, "Start to copy texture. cameraId=" + cameraId);
        // We need to keep a preview frame for the animation before
        // releasing the camera. This will trigger onPreviewTextureCopied.
        mCameraScreenNail.copyTexture();
        mPendingSwitchCameraId = cameraId;
        // Disable all camera controls.
        setCameraState(SWITCHING_CAMERA);
    }

    private void switchCamera() {
        if (mPaused) return;

        Log.v(TAG, "Start to switch camera. id=" + mPendingSwitchCameraId);
        mCameraId = mPendingSwitchCameraId;
        mPendingSwitchCameraId = -1;
        mCameraPicker.setCameraId(mCameraId);

        // from onPause
        closeCamera();
        collapseCameraControls();
        if (mFaceView != null) mFaceView.clear();
        if (mFocusManager != null) mFocusManager.removeMessages();

        // Restart the camera and initialize the UI. From onCreate.
        mPreferences.setLocalId(Camera.this, mCameraId);
        CameraSettings.upgradeLocalPreferences(mPreferences.getLocal());
        CameraOpenThread cameraOpenThread = new CameraOpenThread();
        cameraOpenThread.start();
        try {
            cameraOpenThread.join();
        } catch (InterruptedException ex) {
            // ignore
        }
        initializeCapabilities();
        CameraInfo info = CameraHolder.instance().getCameraInfo()[mCameraId];
        boolean mirror = (info.facing == CameraInfo.CAMERA_FACING_FRONT);
        mFocusManager.setMirror(mirror);
        mFocusManager.setParameters(mInitialParams);
        startPreview(false);
        setCameraState(IDLE);
        startFaceDetection();
        initializeIndicatorControl();

        // from onResume
        setOrientationIndicator(mOrientationCompensation, false);
        // from initializeFirstTime
        initializeZoom();
        updateOnScreenIndicators();
        showTapToFocusToastIfNeeded();

        // Start switch camera animation. Post a message because
        // onFrameAvailable from the old camera may already exist.
        mHandler.sendEmptyMessage(SWITCH_CAMERA_START_ANIMATION);
    }

    // Preview texture has been copied. Now camera can be released and the
    // animation can be started.
    @Override
    protected void onPreviewTextureCopied() {
        mHandler.sendEmptyMessage(SWITCH_CAMERA);
    }

    @Override
    public void onUserInteraction() {
        super.onUserInteraction();
        keepScreenOnAwhile();
    }

    private void resetBurst() {
        mParameters.set(PARM_BURST, 0);
        mCameraDevice.setParameters(mParameters);
        Editor editor = mPreferences.edit();
        editor.putString(CameraSettings.KEY_BURST, "0");
        editor.apply();
        mIndicatorControlContainer.reloadPreferences();
    }

    private void resetScreenOn() {
        mHandler.removeMessages(CLEAR_SCREEN_DELAY);
        getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    }

    private void keepScreenOnAwhile() {
        mHandler.removeMessages(CLEAR_SCREEN_DELAY);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        mHandler.sendEmptyMessageDelayed(CLEAR_SCREEN_DELAY, SCREEN_DELAY);
    }

    @Override
    public void onRestorePreferencesClicked() {
        if (mPaused) return;
        Runnable runnable = new Runnable() {
            @Override
            public void run() {
                restorePreferences();
            }
        };
        mRotateDialog.showAlertDialog(
                null,
                getString(R.string.confirm_restore_message),
                getString(android.R.string.ok), runnable,
                getString(android.R.string.cancel), null);
    }

    private void restorePreferences() {
        // Reset the zoom. Zoom value is not stored in preference.
        if (mParameters.isZoomSupported()) {
            mZoomValue = 0;
            setCameraParametersWhenIdle(UPDATE_PARAM_ZOOM);
            mZoomControl.setZoomIndex(0);
        }
        if (mIndicatorControlContainer != null) {
            mIndicatorControlContainer.dismissSettingPopup();
            CameraSettings.restorePreferences(Camera.this, mPreferences,
                    mParameters);
            mIndicatorControlContainer.reloadPreferences();
            onSharedPreferenceChanged();
        }
    }

    @Override
    public void onOverriddenPreferencesClicked() {
        if (mPaused) return;
        if (mNotSelectableToast == null) {
            String str = getResources().getString(R.string.not_selectable_in_scene_mode);
            mNotSelectableToast = Toast.makeText(Camera.this, str, Toast.LENGTH_SHORT);
        }
        mNotSelectableToast.show();
    }

    @Override
    public void onFaceDetection(Face[] faces, android.hardware.Camera camera) {
        // Sometimes onFaceDetection is called after onPause.
        // Ex: onPause during burst capture. Ignore it.
        if (mPausing || isFinishing()) return;

        FaceViewData faceData[] = new FaceViewData[faces.length];

        int i = 0;
        for ( Face face : faces ) {

            faceData[i] = new FaceViewData();
            if ( null == faceData[i] ) {
                break;
            }

            faceData[i].id = face.id;
            faceData[i].leftEye = face.leftEye;
            faceData[i].mouth = face.mouth;
            faceData[i].rect = face.rect;
            faceData[i].rightEye = face.rightEye;
            faceData[i].score = face.score;
            i++;
        }

        mFaceView.setFaces(faceData);
    }

    private void showTapToFocusToast() {
        new RotateTextToast(this, R.string.tap_to_focus, mOrientationCompensation).show();
        // Clear the preference.
        Editor editor = mPreferences.edit();
        editor.putBoolean(CameraSettings.KEY_CAMERA_FIRST_USE_HINT_SHOWN, false);
        editor.apply();
    }

    private void initializeCapabilities() {
        mInitialParams = mCameraDevice.getParameters();
        mFocusAreaSupported = (mInitialParams.getMaxNumFocusAreas() > 0
                && isSupported(Parameters.FOCUS_MODE_AUTO,
                        mInitialParams.getSupportedFocusModes()));
        mMeteringAreaSupported = (mInitialParams.getMaxNumMeteringAreas() > 0);
        mAeLockSupported = mInitialParams.isAutoExposureLockSupported();
        mAwbLockSupported = mInitialParams.isAutoWhiteBalanceLockSupported();
        mContinousFocusSupported = mInitialParams.getSupportedFocusModes().contains(
                Parameters.FOCUS_MODE_CONTINUOUS_PICTURE);
    }

    // PreviewFrameLayout size has changed.
    @Override
    public void onSizeChanged(int width, int height) {
        if (mFocusManager != null) mFocusManager.setPreviewSize(width, height);
        if (mTouchManager != null) mTouchManager.setPreviewSize(width, height);
    }

    boolean setPreviewFrameLayoutAspectRatio() {
        // Set the preview frame aspect ratio according to the picture size.
        Size size = mParameters.getPictureSize();
        return mPreviewFrameLayout.setAspectRatio((double) size.width / size.height);
    }
}
