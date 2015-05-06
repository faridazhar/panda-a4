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
import android.graphics.ImageFormat;
import android.graphics.SurfaceTexture;
import android.hardware.Camera.CameraInfo;
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
import android.os.Process;
import android.provider.MediaStore;
import android.util.Log;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.OrientationEventListener;
import android.view.View;
import android.view.WindowManager;
import android.view.GestureDetector;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;
import android.widget.Button;


import com.ti.omap.android.camera.CPCamFocusManager.QueuedShotStates;
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
import com.ti.omap.android.camera.ui.ManualGainExposureSettings;
import com.ti.omap.android.camera.R;
import com.ti.omap.android.camera.ui.FaceViewData;
import com.ti.omap.android.camera.ui.CPcamExposureControl;
import com.ti.omap.android.camera.ui.CPcamGainControl;
import com.ti.omap.android.cpcam.CPCam.Face;
import com.ti.omap.android.cpcam.CPCam.FaceDetectionListener;
import com.ti.omap.android.cpcam.CPCam.Parameters;
import com.ti.omap.android.cpcam.CPCam.PictureCallback;
import com.ti.omap.android.cpcam.CPCam.PreviewCallback;
import com.ti.omap.android.cpcam.CPCam.Size;
import com.ti.omap.android.cpcam.CPCamBufferQueue;
import com.ti.omap.android.cpcam.CPCamMetadata;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Formatter;
import java.util.List;
import java.util.StringTokenizer;
import java.text.DecimalFormat;

/** The Camera activity which can preview and take pictures. */
public class CPCam extends ActivityBase implements CPCamFocusManager.Listener,
        ModePicker.OnModeChangeListener, FaceDetectionListener,
        CameraPreference.OnPreferenceChangedListener, LocationManager.Listener,
        PreviewFrameLayout.OnSizeChangedListener,
        ShutterButton.OnShutterButtonListener,
        com.ti.omap.android.cpcam.CPCamBufferQueue.OnFrameAvailableListener,
        ShutterButton.OnShutterButtonLongPressListener {

    private static final String TAG = "CPCam";

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
    private static final int QUEUE_NEXT_SHOT = 18;

    // The subset of parameters we need to update in setCameraParameters().
    private static final int UPDATE_PARAM_INITIALIZE = 1;
    private static final int UPDATE_PARAM_ZOOM = 2;
    private static final int UPDATE_PARAM_PREFERENCE = 4;
    private static final int UPDATE_PARAM_ALL = -1;
    private static final int UPDATE_PARAM_MODE = 8;

    //CPCam
    protected CPCameraManager.CPCameraProxy mCPCamDevice;
    private String mShotParamsGain = "400"; //Default values
    private String mShotParamsExposure = "40000";
    private static final String DEFAULT_EXPOSURE_GAIN = "(40000,400)";
    private static final String ABSOLUTE_EXP_GAIN_TEXT = "Absolute";
    private static final String RELATIVE_EXP_GAIN_TEXT = "Relative";
    private com.ti.omap.android.cpcam.CPCamBufferQueue mTapOut;
    private int mFrameWidth,mFrameHeight;

    private static final String ANALOG_GAIN_METADATA = "analog-gain";
    private static final String ANALOG_GAIN_REQUESTED_METADATA = "analog-gain-req";
    private static final String EXPOSURE_TIME_METADATA = "exposure-time";
    private static final String EXPOSURE_TIME_REQUESTED_METADATA = "exposure-time-req";

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

    private String mCaptureMode = "cp-cam";

    public static final String TRUE = "true";
    public static final String FALSE = "false";

    private boolean mReprocessNextFrame = false;
    private boolean mRestartQueueShot = false;

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

    private boolean mIsRelativeExposureGainPair = false;

    private boolean mPausing;

    // When setCameraParametersWhenIdle() is called, we accumulate the subsets
    // needed to be updated in mUpdateSet.
    private int mUpdateSet;

    private static final int SCREEN_DELAY = 2 * 60 * 1000;

    private int mZoomValue;  // The current zoom value.
    private int mZoomMax;
    private ZoomControl mZoomControl;
    private GestureDetector mGestureDetector;

    private CPcamGainControl mCpcamGainControl;
    private CPcamExposureControl mCpcamExposureControl;

    private com.ti.omap.android.cpcam.CPCam.Parameters mParameters;
    private com.ti.omap.android.cpcam.CPCam.Parameters mInitialParams;
    private com.ti.omap.android.cpcam.CPCam.Parameters mShotParams;

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

    private Button mReprocessButton;
    private Button mExpGainButton;

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
    private TextView mMetaDataIndicator;
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
    private static final int QUEUED_SHOT_IN_PROGRESS = 5;
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
    private final JpegPictureCallback mJpegCallback =
        new JpegPictureCallback(null);
    private final AutoFocusMoveCallback mAutoFocusMoveCallback =
            new AutoFocusMoveCallback();
    private final CPCameraErrorCallback mErrorCallback = new CPCameraErrorCallback();

    private long mFocusStartTime;
    private long mShutterCallbackTime;
    private long mPostViewPictureCallbackTime;
    private long mRawPictureCallbackTime;
    private long mJpegPictureCallbackTime;
    private long mOnResumeTime;
    private long mStorageSpace;
    private byte[] mJpegImageData;
    private int mManualExposureControl;
    private int mManualGainISO;
    private int mManualExposureControlValue = 40; //Default values
    private int mManualGainControlValue = 400;

    // These latency time are for the CameraLatency test.
    public long mAutoFocusTime;
    public long mShutterLag;
    public long mShutterToPictureDisplayedTime;
    public long mPictureDisplayedToJpegCallbackTime;
    public long mJpegCallbackFinishTime;
    public long mCaptureStartTime;

    // This handles everything about focus.
    private CPCamFocusManager mFocusManager;
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
                mCPCamDevice = Util.openCPCamera(CPCam.this, mCameraId);
                mParameters = mCPCamDevice.getParameters();
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
                    if (mJpegPictureCallbackTime != 0) {
                        long now = System.currentTimeMillis();
                        mJpegCallbackFinishTime = now - mJpegPictureCallbackTime;
                        Log.v(TAG, "mJpegCallbackFinishTime = "
                                + mJpegCallbackFinishTime + "ms");
                        mJpegPictureCallbackTime = 0;
                    }
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
                    if (Util.getDisplayRotation(CPCam.this) != mDisplayRotation) {
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

                case MANUAL_GAIN_EXPOSURE_CHANGED: {
                    Bundle data;
                    data = msg.getData();
                    mManualExposureControl = data.getInt("EXPOSURE");
                    mManualGainISO = data.getInt("ISO");

                    if(mIsRelativeExposureGainPair) {
                        //set Relative exposure and gain values
                        mManualExposureControl = mManualExposureControl
                                -10*mParameters.getMaxExposureCompensation();
                        mManualGainISO = mManualGainISO
                                -10*mParameters.getMaxExposureCompensation();
                        // decrement with 300 to get the range (-300;+300 )
                        if (mManualExposureControl < 10*mParameters.getMinExposureCompensation())
                            mManualExposureControl = 10*mParameters.getMinExposureCompensation();

                        if(mManualExposureControl > 0) {
                            mShotParamsExposure = "+" + Integer.toString(mManualExposureControl);
                        } else {
                            mShotParamsExposure = Integer.toString(mManualExposureControl);
                        }

                        if ( mManualGainISO <= 10*mParameters.getMinExposureCompensation())
                            mManualGainISO = 10*mParameters.getMinExposureCompensation();

                        if(mManualGainISO > 0) {
                            mShotParamsGain = "+" + Integer.toString(mManualGainISO);
                        } else {
                            mShotParamsGain = Integer.toString(mManualGainISO);
                        }

                    } else {
                        //Set Absolute exposure and gain values
                        if (mManualExposureControl <= 0) {
                            mManualExposureControl = 1;
                        }
                        mShotParamsExposure = Integer.toString(1000*mManualExposureControl);

                        if ( mManualGainISO <= 0) {
                            mManualGainISO = 0;
                        }
                        mShotParamsGain = Integer.toString(mManualGainISO);
                    }
                    Log.e(TAG,mIsRelativeExposureGainPair
                    + mShotParamsExposure + " , " + mShotParamsGain);
                    String expGainPair = new String( "(" + mShotParamsExposure + "," + mShotParamsGain + ")" );

                    if ( null == mShotParams && null != mCPCamDevice ) {
                        mShotParams = mCPCamDevice.getParameters();
                    }

                    mShotParams.setPictureFormat(ImageFormat.NV21);
                    mShotParams.set(CPCameraSettings.KEY_SHOTPARAMS_EXP_GAIN_PAIRS, expGainPair);
                    mShotParams.set(CPCameraSettings.KEY_SHOTPARAMS_BURST, 1);

                    break;
                }

                case QUEUE_NEXT_SHOT: {
                    if ( null != mCPCamDevice ) {
                        mCPCamDevice.takePicture(null, null, null, mJpegCallback, mShotParams);
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
                    Util.showErrorAndFinish(CPCam.this,
                            R.string.cannot_connect_camera);
                    break;
                }

                case CAMERA_DISABLED: {
                    mCameraStartUpThread = null;
                    mCameraDisabled = true;
                    Util.showErrorAndFinish(CPCam.this,
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
        if (mIndicatorControlContainer == null) {
            initializeIndicatorControl();
        }
        initializeCPcamSliders(mIsRelativeExposureGainPair);

        // This should be enabled after preview is started.
        mIndicatorControlContainer.setEnabled(false);
        showTapToFocusToastIfNeeded();
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
        mOrientationListener = new MyOrientationEventListener(CPCam.this);
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

        mImageSaver = new ImageSaver();
        mImageNamer = new ImageNamer();
        installIntentFilter();
        initializeCPcamSliders(mIsRelativeExposureGainPair);
        mFirstTimeInitialized = true;
        addIdleHandler();

        mTapOut = new CPCamBufferQueue(true);
        mTapOut.setOnFrameAvailableListener(this);

        mCPCamDevice.setBufferSource(null, mTapOut);
    }

    private void showTapToFocusToastIfNeeded() {
        // Show the tap to focus toast if this is the first start.
        if (mFocusAreaSupported &&
                mPreferences.getBoolean(CPCameraSettings.KEY_CAMERA_FIRST_USE_HINT_SHOWN, true)) {
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
        keepMediaProviderInstance();
        checkStorage();
        hidePostCaptureAlert();

        initializeCPcamSliders(mIsRelativeExposureGainPair);

        if (!mIsImageCaptureIntent) {
            mModePicker.setCurrentMode(ModePicker.MODE_CPCAM);
        }
        if (mIndicatorControlContainer != null) {
            mIndicatorControlContainer.reloadPreferences();
        }
    }

    private class CpcamGainChangeListener implements CPcamGainControl.OnGainChangedListener {
        public void onGainValueChanged(int index) {
            CPCam.this.onCpcamGainValueChanged(index);
        }
    }

    private class CpcamExposureChangeListener implements CPcamExposureControl.OnExposureChangedListener {
        public void onExposureValueChanged(int index) {
            CPCam.this.onCpcamExposureValueChanged(index);
        }
    }

    private void initializeCPcamSliders(boolean mIsRelativeExposureGainPair) {
        int expMin,expMax,isoMax,isoStep,isoMin;
        if(mIsRelativeExposureGainPair){
            //Relative exposure and gain values
            //Set the range [0:600]
            expMin = 0;
            expMax = 20*mParameters.getMaxExposureCompensation();
            isoMin = 0;
            isoMax = 20*mParameters.getMaxExposureCompensation();
        } else {
            //Absolute exposure and gain values
            expMin = Integer.parseInt(mParameters.get(CPCameraSettings.KEY_SUPPORTED_MANUAL_EXPOSURE_MIN));
            expMax = Integer.parseInt(mParameters.get(CPCameraSettings.KEY_SUPPORTED_MANUAL_EXPOSURE_MAX));
            isoMin = Integer.parseInt(mParameters.get(CPCameraSettings.KEY_SUPPORTED_MANUAL_GAIN_ISO_MIN));
            isoMax = Integer.parseInt(mParameters.get(CPCameraSettings.KEY_SUPPORTED_MANUAL_GAIN_ISO_MAX));
        }

        mCpcamGainControl.setGainMinMax(isoMin,isoMax);
        mCpcamExposureControl.setExposureMinMax(expMin,expMax);

        mCpcamGainControl.setOnGainChangeListener(new CpcamGainChangeListener());
        mCpcamExposureControl.setOnExposureChangeListener(new CpcamExposureChangeListener());

        if ( !mIsRelativeExposureGainPair ) {
            int resetISO = (isoMax - isoMin ) / 2;
            mCpcamGainControl.setGainIndex(resetISO);
            onCpcamGainValueChanged(resetISO);
        }
        int resetExp = ( expMax - expMin ) / 2;
        mCpcamExposureControl.setExposureIndex(resetExp);
        onCpcamExposureValueChanged(resetExp);
    }

    private void onCpcamGainValueChanged(int index) {
        // Not useful to change zoom value when the activity is paused.
        if (mPausing) return;
        Message msg = new Message();
        Bundle data;
        data = new Bundle ();
        mManualGainControlValue = index;
        data.putInt("ISO", index);
        data.putInt("EXPOSURE", mManualExposureControlValue);
        msg.what = Camera.MANUAL_GAIN_EXPOSURE_CHANGED;
        msg.setData(data);
        mHandler.sendMessage(msg);
    }

    private void onCpcamExposureValueChanged(int index) {
        // Not useful to change zoom value when the activity is paused.
        if (mPausing) return;
        Message msg = new Message();
        Bundle data;
        data = new Bundle ();
        mManualExposureControlValue = index;
        data.putInt("EXPOSURE", index);
        data.putInt("ISO", mManualGainControlValue);
        msg.what = Camera.MANUAL_GAIN_EXPOSURE_CHANGED;
        msg.setData(data);
        mHandler.sendMessage(msg);
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
            mCPCamDevice.setFaceDetectionListener(this);
            mCPCamDevice.startFaceDetection();
        }
    }

    @Override
    public void stopFaceDetection() {
        if (!mFaceDetectionStarted) return;
        if (mParameters.getMaxNumDetectedFaces() > 0) {
            mFaceDetectionStarted = false;
            mCPCamDevice.setFaceDetectionListener(null);
            mCPCamDevice.stopFaceDetection();
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
            if (mPaused || mCPCamDevice == null || !mFirstTimeInitialized
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

            // Check if metering area or focus area is supported.
            if (!mFocusAreaSupported && !mMeteringAreaSupported) return;

            mFocusManager.onSingleTapUp((int) e.getX(), (int) e.getY());
        }

    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent m) {
        if (mCameraState == SWITCHING_CAMERA) return true;
        if ( mGestureDetector != null && mGestureDetector.onTouchEvent(m) ) {
               return true;
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
        mMetaDataIndicator = (TextView) findViewById(R.id.onscreen_metadata_indicator);
        mFocusIndicator = (ImageView) findViewById(R.id.onscreen_focus_indicator);
    }

    private void updateMetadataIndicator(String exposure,
                                         String exposureReq,
                                         String gain,
                                         String gainReq,
                                         String evCompSet) {
        String metadata = new String();
        try {
            if ( null !=  exposure ) {
                int expTime = Integer.parseInt(exposure) / 1000;
                metadata =  "Exposure[ms]: " + expTime + "\n" ;
            }

            if ( null != exposureReq ) {
                int expTimeReq = Integer.parseInt(exposureReq) / 1000;
                metadata += "Exposure Requested[ms]: " + expTimeReq + "\n";
            }

        } catch ( NumberFormatException e ) { e.printStackTrace(); }

        if ( null != gain ) {
            metadata += "Gain: " + gain + "\n";
        }

        if ( null != gainReq ) {
            metadata += "Gain Requested: " + gainReq + "\n";
        }

        if ( null != evCompSet ) {
            metadata += "EV compensation set[EV]: " + evCompSet + "\n";
        }

        mMetaDataIndicator.setText(metadata);
        mMetaDataIndicator.setVisibility(View.VISIBLE);
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

    private void updateOnScreenMetadataIndicators(CPCamMetadata metaData) {
        if (metaData == null) return;
        String exposure = Integer.toString(metaData.exposureTime);
        String gain = Integer.toString(metaData.analogGain);
        String gainRequested = null;
        String exposureRequested = null;
        String evCompSet = null;
        if(!mIsRelativeExposureGainPair) {
            gainRequested = Integer.toString(metaData.analogGainReq);
            exposureRequested = Integer.toString(metaData.exposureTimeReq);
        } else {
            DecimalFormat evFormat = new DecimalFormat("#.##");
            float expStepScale = mParameters.getExposureCompensationStep();
            expStepScale *= expStepScale;
            float evCompCurrent = mManualExposureControl * expStepScale;
            evCompSet = evFormat.format(evCompCurrent);
        }

        updateMetadataIndicator(exposure,
                                exposureRequested,
                                gain,
                                gainRequested,
                                evCompSet);
    }

    private final class ShutterCallback
            implements com.ti.omap.android.cpcam.CPCam.ShutterCallback {
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
                byte [] data, com.ti.omap.android.cpcam.CPCam camera) {
            mPostViewPictureCallbackTime = System.currentTimeMillis();
            Log.v(TAG, "mShutterToPostViewCallbackTime = "
                    + (mPostViewPictureCallbackTime - mShutterCallbackTime)
                    + "ms");
        }
    }

    private final class RawPictureCallback implements PictureCallback {
        @Override
        public void onPictureTaken(
                byte [] rawData, com.ti.omap.android.cpcam.CPCam camera) {
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
                final byte [] jpegData, final com.ti.omap.android.cpcam.CPCam camera) {
            if (mPausing) {
                return;
            }

            // WA: Re-create CPCamBufferQueue before next shot
            try {
                camera.setBufferSource(null, null);
                mTapOut.release();
                mTapOut = new CPCamBufferQueue(true);
                mTapOut.setOnFrameAvailableListener(CPCam.this);
                camera.setBufferSource(null, mTapOut);
            } catch(IOException e) { e.printStackTrace(); }

            if ( mReprocessNextFrame ) {
                mReprocessNextFrame = false;
            }

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

                Size size = mParameters.getPictureSize();
                mImageNamer.prepareUri(mContentResolver, mCaptureStartTime,
                        size.width, size.height, mJpegRotation);

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

            checkStorage();

            long now = System.currentTimeMillis();
            mJpegCallbackFinishTime = now - mJpegPictureCallbackTime;
            Log.v(TAG, "mJpegCallbackFinishTime = "
                    + mJpegCallbackFinishTime + "ms");
            mJpegPictureCallbackTime = 0;
        }
    }

    private final class AutoFocusCallback
            implements com.ti.omap.android.cpcam.CPCam.AutoFocusCallback {
        @Override
        public void onAutoFocus(
                boolean focused, com.ti.omap.android.cpcam.CPCam camera) {
            if (mPaused) return;

            mAutoFocusTime = System.currentTimeMillis() - mFocusStartTime;
            Log.v(TAG, "mAutoFocusTime = " + mAutoFocusTime + "ms");
            setCameraState(IDLE);

            // If focus completes and the snapshot is not started, enable the
            // controls.
            if (mFocusManager.isFocusCompleted()) {
                enableCameraControls(true);
            }

            mFocusManager.onAutoFocus(focused);
        }
    }

    private final class AutoFocusMoveCallback
            implements com.ti.omap.android.cpcam.CPCam.AutoFocusMoveCallback {
        @Override
        public void onAutoFocusMoving(
            boolean moving, com.ti.omap.android.cpcam.CPCam camera) {
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
                Util.broadcastNewPicture(CPCam.this, uri);
            }
        }
    }

    private boolean setUpQueuedShot() {
        mCaptureStartTime = System.currentTimeMillis();
        mPostViewPictureCallbackTime = 0;
        mJpegImageData = null;

        // Set rotation and gps data.
        mJpegRotation = Util.getJpegRotation(mCameraId, mOrientation);
        mParameters.setRotation(mJpegRotation);
        Location loc = mLocationManager.getCurrentLocation();
        Util.setGpsParametersCPCam(mParameters, loc);
        mParameters.setPictureFormat(ImageFormat.NV21);
        mParameters.setPictureSize(mFrameWidth, mFrameHeight);
        mCPCamDevice.setParameters(mParameters);

        if(mShotParams == null) {
            //If shot params are not set, put default values
            mShotParams = mCPCamDevice.getParameters();
            mShotParams.set(CPCameraSettings.KEY_SHOTPARAMS_EXP_GAIN_PAIRS,
                            DEFAULT_EXPOSURE_GAIN);
               mShotParams.set(CPCameraSettings.KEY_SHOTPARAMS_BURST, 1);
        } else {
            mCPCamDevice.setParameters(mShotParams);
        }

        try {
            mCPCamDevice.takePicture(null, null, null, mJpegCallback, mShotParams);
        } catch (RuntimeException e ) {
            e.printStackTrace();
            return false;
        }

        mFaceDetectionStarted = false;
        setCameraState(QUEUED_SHOT_IN_PROGRESS);
        mReprocessButton.setVisibility(View.VISIBLE);
        mExpGainButton.setVisibility(View.VISIBLE);
        mIndicatorControlContainer.showCPCamSliders(true);
        return true;
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
        Log.d(TAG,"Capture()");
        // If we are already in the middle of taking a snapshot then ignore.
        if (mCPCamDevice == null) {
            return false;
        }

        if ( mCameraState == QUEUED_SHOT_IN_PROGRESS ) {
            setCameraState(IDLE);
            mFocusManager.setQueuedShotState(QueuedShotStates.OFF);
            mReprocessButton.setVisibility(View.INVISIBLE);
            mMetaDataIndicator.setVisibility(View.INVISIBLE);
            mExpGainButton.setVisibility(View.INVISIBLE);
            mIndicatorControlContainer.showCPCamSliders(false);
            Message msg = new Message();
            msg.what = RESTART_PREVIEW;
            msg.arg1 = MODE_RESTART;
            mHandler.sendMessage(msg);
            return true;
        }
        return setUpQueuedShot();
    }

    private int getCameraRotation() {
        return (mOrientationCompensation - mDisplayRotation + 360) % 360;
    }

    @Override
    public void setFocusParameters() {
        setCameraParameters(UPDATE_PARAM_PREFERENCE);
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
            return CPCameraSettings.readPreferredCameraId(preferences);
        }
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mPreferences = new ComboPreferences(this);
        CPCameraSettings.upgradeGlobalPreferences(mPreferences.getGlobal());
        mCameraId = getPreferredCameraId(mPreferences);
        PreferenceInflater inflater = new PreferenceInflater(this);
        PreferenceGroup group =
            (PreferenceGroup) inflater.inflate(R.xml.camera_preferences);

        mContentResolver = getContentResolver();

        // To reduce startup time, open the camera and start the preview in
        // another thread.
        mCameraStartUpThread = new CameraStartUpThread();
        mCameraStartUpThread.start();

        setContentView(R.layout.cpcamcamera);

        // Surface texture is from camera screen nail and startPreview needs it.
        // This must be done before startPreview.
        mIsImageCaptureIntent = isImageCaptureIntent();
        createCameraScreenNail(!mIsImageCaptureIntent);

        mPreferences.setLocalId(this, mCameraId);
        CPCameraSettings.upgradeLocalPreferences(mPreferences.getLocal());
        mFocusAreaIndicator = (RotateLayout) findViewById(
                R.id.focus_indicator_rotate_layout);
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

    private void loadCameraPreferences() {
        CPCameraSettings settings = new CPCameraSettings((Activity)this, mInitialParams,
                mCameraId, CameraHolder.instance().getCameraInfo());
        mPreferenceGroup = settings.getPreferenceGroup(R.xml.camera_preferences);
    }

    private void initializeIndicatorControl() {
        // setting the indicator buttons.
        mIndicatorControlContainer =
                (IndicatorControlContainer) findViewById(R.id.cpcam_indicator_control);
        loadCameraPreferences();

        mIndicatorControlContainer.initialize(this, mPreferenceGroup,
                false, true, null, null);
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
        if (mCpcamGainControl != null) mCpcamGainControl.setEnabled(enable);
        if (mCpcamExposureControl != null) mCpcamExposureControl.setEnabled(enable);
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
                    (mOrientation + Util.getDisplayRotation(CPCam.this)) % 360;
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
                mIndicatorControlContainer, null,  null, mCpcamGainControl, mCpcamExposureControl,
                mFocusAreaIndicator, null, mReviewDoneButton, mRotateDialog, mOnScreenIndicators};
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
        setResultEx(RESULT_OK);
        finish();
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
        if (mPaused || collapseCameraControls()) return;

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
                || mCPCamDevice == null ) return;

        Log.v(TAG, "onShutterButtonLongPressed");
        mFocusManager.shutterLongPressed();
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

    @Override
    protected void onResume() {
        mPaused = false;
        super.onResume();
        if (mOpenCameraFail || mCameraDisabled) return;

        mHandler.removeMessages(RELEASE_CAMERA);

        mPausing = false;

        mJpegPictureCallbackTime = 0;
        mZoomValue = 0;

        // Start the preview if it is not started.
        if (mCameraState == PREVIEW_STOPPED && mCameraStartUpThread == null) {
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
        mCPCamDevice.setBufferSource(null, mTapOut);
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
            mModePicker.setCurrentMode(ModePicker.MODE_CPCAM);
        }
    }

    private void initializeFocusManager() {
        // Create FocusManager object. startPreview needs it.
        CameraInfo info = CameraHolder.instance().getCameraInfo()[mCameraId];
        boolean mirror = (info.facing == CameraInfo.CAMERA_FACING_FRONT);
        String[] defaultFocusModes = getResources().getStringArray(
                R.array.pref_camera_focusmode_default_array);
        mFocusManager = new CPCamFocusManager(mPreferences, defaultFocusModes,
                mFocusAreaIndicator, mInitialParams, this, mirror,
                getMainLooper());
    }

    private void initializeMiscControls() {
        // startPreview needs this.
        mPreviewFrameLayout = (PreviewFrameLayout) findViewById(R.id.frame);
        // Set touch focus listener.
        setSingleTapUpListener(mPreviewFrameLayout);


        mCpcamGainControl = (CPcamGainControl) findViewById(R.id.gain_control);
        mCpcamExposureControl = (CPcamExposureControl) findViewById(R.id.exposure_control);

        mOnScreenIndicators = (Rotatable) findViewById(R.id.on_screen_indicators);
        mFaceView = (FaceView) findViewById(R.id.face_view);
        mPreviewFrameLayout.addOnLayoutChangeListener(this);
        mPreviewFrameLayout.setOnSizeChangedListener(this);

        mExpGainButton = (Button) findViewById(R.id.manual_gain_exposure_button);
        mExpGainButton.setOnClickListener(new View.OnClickListener() {
                public void onClick(View v) {
                    if(mIsRelativeExposureGainPair ){
                        mExpGainButton.setText(ABSOLUTE_EXP_GAIN_TEXT);
                        mIsRelativeExposureGainPair = false;
                    } else {
                        mExpGainButton.setText(RELATIVE_EXP_GAIN_TEXT);
                        mIsRelativeExposureGainPair = true;
                    }
                    initializeCPcamSliders(mIsRelativeExposureGainPair);
                }
        });

        mReprocessButton = (Button) findViewById(R.id.reprocess_button);
        mReprocessButton.setOnClickListener(new View.OnClickListener() {
                public void onClick(View v) {
                    mReprocessNextFrame = true;
                    mRestartQueueShot = true;
                }
        });
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
        inflater.inflate(R.layout.cppreview_frame, appRoot);
        inflater.inflate(R.layout.camera_control, appRoot);

        // from onCreate()
        initializeControlByIntent();
        initializeFocusManager();
        initializeMiscControls();
        initializeIndicatorControl();
        initializeCPcamSliders(mIsRelativeExposureGainPair);
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

        initOnScreenIndicator();
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
        mCPCamDevice.autoFocus(mAutoFocusCallback);
        setCameraState(FOCUSING);
    }

    @Override
    public void cancelAutoFocus() {
        mCPCamDevice.cancelAutoFocus();
        setCameraState(IDLE);
        setCameraParameters(UPDATE_PARAM_PREFERENCE);
    }

    // Preview area is touched. Handle touch focus.
    @Override
    protected void onSingleTapUp(View view, int x, int y) {
        if (mPaused || mCPCamDevice == null || !mFirstTimeInitialized
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
        if (mCPCamDevice != null) {
            mCPCamDevice.setZoomChangeListener(null);
            mCPCamDevice.setFaceDetectionListener(null);
            mCPCamDevice.setErrorCallback(null);
            CameraHolder.instance().CPCamInstanceRelease();
            mFaceDetectionStarted = false;
            mCPCamDevice = null;
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
        mCPCamDevice.setErrorCallback(mErrorCallback);

        // If we're previewing already, stop the preview first (this will blank
        // the screen).
        if (mCameraState != PREVIEW_STOPPED) stopPreview();

        setDisplayOrientation();
        mCPCamDevice.setDisplayOrientation(mCameraDisplayOrientation);

        if (!mSnapshotOnIdle) {
            // If the focus mode is continuous autofocus, call cancelAutoFocus to
            // resume it because it may have been paused by autoFocus call.
            if (Parameters.FOCUS_MODE_CONTINUOUS_PICTURE.equals(mFocusManager.getFocusMode())) {
                mCPCamDevice.cancelAutoFocus();
            }
            mFocusManager.setAeAwbLock(false); // Unlock AE and AWB.
        }

        if ( updateAll ) {
            Log.v(TAG, "Updating all parameters!");
            setCameraParameters(UPDATE_PARAM_INITIALIZE | UPDATE_PARAM_PREFERENCE);
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

        mCPCamDevice.setPreviewTextureAsync(mSurfaceTexture);
        try {
            Log.v(TAG, "startPreview");
            mCPCamDevice.startPreviewAsync();
        } catch (Throwable ex) {
            closeCamera();
            throw new RuntimeException("startPreview failed", ex);
        }

        mFocusManager.onPreviewStarted();

        if ( mRestartQueueShot ) {
            mParameters.setPictureFormat(ImageFormat.NV21);
            mParameters.setPictureSize(mFrameWidth, mFrameHeight);
            mCPCamDevice.setParameters(mParameters);
            if(mShotParams == null) {
                //If shot params are not set, put default values
                mShotParams = mCPCamDevice.getParameters();
                mShotParams.set(CPCameraSettings.KEY_SHOTPARAMS_EXP_GAIN_PAIRS,
                                DEFAULT_EXPOSURE_GAIN);
                   mShotParams.set(CPCameraSettings.KEY_SHOTPARAMS_BURST, 1);
            } else {
                mCPCamDevice.setParameters(mShotParams);
            }

            mRestartQueueShot = false;
            setCameraState(QUEUED_SHOT_IN_PROGRESS);
            // WA: This should be done on first preview callback.
            //     For some reason callbacks are not called after
            //     the second iteration.
            mHandler.sendEmptyMessageDelayed(QUEUE_NEXT_SHOT, CAMERA_RELEASE_DELAY);

        } else {
            setCameraState(IDLE);
        }

        if (mSnapshotOnIdle) {
            mHandler.post(mDoSnapRunnable);
        }
    }

    private void stopPreview() {
        if (mCPCamDevice != null && mCameraState != PREVIEW_STOPPED) {
            Log.v(TAG, "stopPreview");

            mCPCamDevice.cancelAutoFocus(); // Reset the focus.
            mCPCamDevice.stopPreview();
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

        //Disable IPP (LDCNSF) for reprocess - will be enabled later when Ducati support is added
        mParameters.set(PARM_IPP, PARM_IPP_NONE);
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
        CPCameraSettings.filterUnsupportedOptions(mPreferenceGroup, menu, supported);
        return menu;
    }

    private ListPreference getSupportedListPreference(List<Integer> supportedIntegers, String menuKey){
        List<String> supported = new ArrayList<String>();
        ListPreference menu = mPreferenceGroup.findPreference(menuKey);
        if ( ( menu != null ) && !supportedIntegers.isEmpty() ) {
            for (Integer item : supportedIntegers) {
                supported.add(item.toString());
            }
            CPCameraSettings.filterUnsupportedOptions(mPreferenceGroup, menu, supported);
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

        if (mFocusAreaSupported) {
            mParameters.setFocusAreas(mFocusManager.getFocusAreas());
        }

        // Set picture size.
        String pictureSize = mPreferences.getString(
                CPCameraSettings.KEY_PICTURE_SIZE, null);
        if (pictureSize == null) {
            CPCameraSettings.initialCameraPictureSize(this, mParameters);
        } else {
            List<String> supported = new ArrayList<String>();
            supported = CPCameraSettings.sizeListToStringList(mParameters.getSupportedPictureSizes());

            CPCameraSettings.setCameraPictureSize(
                    pictureSize, supported, mParameters);
            mFrameWidth = CPCameraSettings.getCameraPictureSizeWidth(pictureSize);
            mFrameHeight = CPCameraSettings.getCameraPictureSizeHeight(pictureSize);
        }

        // For the following settings, we need to check if the settings are
        // still supported by latest driver, if not, ignore the settings.

        String[] previewDefaults = getResources().getStringArray(R.array.pref_camera_previewsize_default_array);
        String defaultPreviewSize = "";
        String[] previewSizes2D = getResources().getStringArray(R.array.pref_camera_previewsize_entryvalues);
        defaultPreviewSize = elementExists(previewDefaults, previewSizes2D);


        String previewSize = mPreferences.getString(CPCameraSettings.KEY_PREVIEW_SIZE, defaultPreviewSize);
        if (previewSize !=null && !previewSize.equals(mPreviewSize)) {
            List<String> supported = new ArrayList<String>();
            supported = CPCameraSettings.sizeListToStringList(mParameters.getSupportedPreviewSizes());
            CPCameraSettings.setCameraPreviewSize(previewSize, supported, mParameters);
            mPreviewSize = previewSize;
            enableCameraControls(true);
            restartNeeded = true;
        }

        if (mContinousFocusSupported) {
            if (mParameters.getFocusMode().equals(Parameters.FOCUS_MODE_CONTINUOUS_PICTURE)) {
                mCPCamDevice.setAutoFocusMoveCallback(mAutoFocusMoveCallback);
            } else {
                mCPCamDevice.setAutoFocusMoveCallback(null);
            }
        }

        mParameters.set(CameraSettings.KEY_MODE, mCaptureMode);

        if (mIndicatorControlContainer != null) {
            mIndicatorControlContainer.requestLayout();
        }

        return restartNeeded;
    }

    // We separate the parameters into several subsets, so we can update only
    // the subsets actually need updating. The PREFERENCE set needs extra
    // locking because the preference can be changed from GLThread as well.
    private void setCameraParameters(int updateSet) {
        boolean restartPreview = false;

        mParameters = mCPCamDevice.getParameters();

        if ((updateSet & UPDATE_PARAM_INITIALIZE) != 0) {
            updateCameraParametersInitialize();
        }

        if ((updateSet & UPDATE_PARAM_PREFERENCE) != 0) {
            restartPreview = updateCameraParametersPreference();
        }

        if ((updateSet & UPDATE_PARAM_MODE) != 0 ) {
               updateCameraParametersPreference();
        }

        mCPCamDevice.setParameters(mParameters);
        if ( ( restartPreview ) && ( mCameraState != PREVIEW_STOPPED ) ) {
            // This will only restart the preview
            // without trying to apply any new
            // camera parameters.
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
        if (mCPCamDevice == null) {
            // We will update all the parameters when we open the device, so
            // we don't need to do anything now.
            mUpdateSet = 0;
            return;
        } else if (isCameraIdle()) {
            setCameraParameters(mUpdateSet);
            mUpdateSet = 0;
        } else {
            if (!mHandler.hasMessages(SET_CAMERA_PARAMETERS_WHEN_IDLE)) {
                mHandler.sendEmptyMessageDelayed(
                        SET_CAMERA_PARAMETERS_WHEN_IDLE, 1000);
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
        MenuHelper.gotoMode(mode, CPCam.this);
        mHandler.removeMessages(FIRST_TIME_INIT);
        finish();
    }

    @Override
    public void onModeChanged(int mode) {
        if (mode != ModePicker.MODE_CPCAM) switchToOtherMode(mode);
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
        mPreferences.setLocalId(CPCam.this, mCameraId);
        CPCameraSettings.upgradeLocalPreferences(mPreferences.getLocal());
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
        initializeCPcamSliders(mIsRelativeExposureGainPair);

        if (mIndicatorControlContainer != null) {
            mIndicatorControlContainer.dismissSettingPopup();
            CPCameraSettings.restorePreferences(CPCam.this, mPreferences,
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
            mNotSelectableToast = Toast.makeText(CPCam.this, str, Toast.LENGTH_SHORT);
        }
        mNotSelectableToast.show();
    }

    @Override
    public void onFaceDetection(Face[] faces, com.ti.omap.android.cpcam.CPCam camera) {
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
        editor.putBoolean(CPCameraSettings.KEY_CAMERA_FIRST_USE_HINT_SHOWN, false);
        editor.apply();
    }

    private void initializeCapabilities() {
        mInitialParams = mCPCamDevice.getParameters();
        mFocusAreaSupported = (mInitialParams.getMaxNumFocusAreas() > 0
                && isSupported(Parameters.FOCUS_MODE_AUTO,
                        mInitialParams.getSupportedFocusModes()));
        mContinousFocusSupported = mInitialParams.getSupportedFocusModes().contains(
                Parameters.FOCUS_MODE_CONTINUOUS_PICTURE);
    }

    // PreviewFrameLayout size has changed.
    @Override
    public void onSizeChanged(int width, int height) {
        if (mFocusManager != null) mFocusManager.setPreviewSize(width, height);
    }

    void setPreviewFrameLayoutAspectRatio() {
        // Set the preview frame aspect ratio according to the picture size.
        Size size = mParameters.getPictureSize();
        mPreviewFrameLayout.setAspectRatio((double) size.width / size.height);
    }

    public void onFrameAvailable(final CPCamBufferQueue bq) {
        // Invoked every time there's a new frame available in SurfaceTexture
        new Thread(new Runnable() {
            public void run() {
                Log.v(TAG, "onFrameAvailable: SurfaceTexture got updated, Tid: " + Process.myTid());
                if (!mReprocessNextFrame &&
                    ( mCameraState == QUEUED_SHOT_IN_PROGRESS ) ) {
                    // Queue next shot
                    final int slot = bq.acquireBuffer();
                    mHandler.post( new Runnable()  {
                        @Override
                        public void run() {
                            updateOnScreenMetadataIndicators(CPCamMetadata.getMetadata(bq, slot));
                        }
                    });
                    Message msg = new Message();
                    msg.what = QUEUE_NEXT_SHOT;
                    mHandler.sendMessage(msg);
                    bq.releaseBuffer(slot);
                } else if ( mReprocessNextFrame &&
                           ( mCameraState == QUEUED_SHOT_IN_PROGRESS ) ) {
                    // Reprocess
                    if (!mIsImageCaptureIntent) {
                        // Start capture animation.
                        mCameraScreenNail.animateCapture(getCameraRotation());
                    }

                    mParameters.setPictureFormat(ImageFormat.JPEG);
                    mParameters.setPictureSize(mFrameWidth, mFrameHeight);
                    mCPCamDevice.setParameters(mParameters);
                    mTapOut.setDefaultBufferSize(mFrameWidth, mFrameHeight);
                    mCPCamDevice.setBufferSource(mTapOut,null);
                    mCPCamDevice.reprocess(mShotParams);
                }
            }
        }).start();
    }
}

