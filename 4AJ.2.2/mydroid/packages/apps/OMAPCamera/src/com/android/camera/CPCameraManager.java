/*
 * Copyright (C) 2012 The Android Open Source Project
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

import static com.ti.omap.android.camera.Util.Assert;

import android.graphics.SurfaceTexture;
import com.ti.omap.android.cpcam.CPCam.AutoFocusCallback;
import com.ti.omap.android.cpcam.CPCam.AutoFocusMoveCallback;
import com.ti.omap.android.cpcam.CPCam.ErrorCallback;
import com.ti.omap.android.cpcam.CPCam.FaceDetectionListener;
import com.ti.omap.android.cpcam.CPCam.OnZoomChangeListener;
import com.ti.omap.android.cpcam.CPCam.Parameters;
import com.ti.omap.android.cpcam.CPCam.PictureCallback;
import com.ti.omap.android.cpcam.CPCam.PreviewCallback;
import com.ti.omap.android.cpcam.CPCam.ShutterCallback;
import com.ti.omap.android.cpcam.CPCamBufferQueue;
import android.os.ConditionVariable;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;
import android.util.Log;

import java.io.IOException;

public class CPCameraManager {
    private static final String TAG = "CameraManager";
    private static CPCameraManager sCameraManager = new CPCameraManager();

    // Thread progress signals
    private ConditionVariable mSig = new ConditionVariable();

    private com.ti.omap.android.cpcam.CPCam.Parameters mParameters;
    private IOException mReconnectException;

    private static final int RELEASE = 1;
    private static final int RECONNECT = 2;
    private static final int UNLOCK = 3;
    private static final int LOCK = 4;
    private static final int SET_PREVIEW_TEXTURE_ASYNC = 5;
    private static final int START_PREVIEW_ASYNC = 6;
    private static final int STOP_PREVIEW = 7;
    private static final int SET_PREVIEW_CALLBACK_WITH_BUFFER = 8;
    private static final int ADD_CALLBACK_BUFFER = 9;
    private static final int AUTO_FOCUS = 10;
    private static final int CANCEL_AUTO_FOCUS = 11;
    private static final int SET_AUTO_FOCUS_MOVE_CALLBACK = 12;
    private static final int SET_DISPLAY_ORIENTATION = 13;
    private static final int SET_ZOOM_CHANGE_LISTENER = 14;
    private static final int SET_FACE_DETECTION_LISTENER = 15;
    private static final int START_FACE_DETECTION = 16;
    private static final int STOP_FACE_DETECTION = 17;
    private static final int SET_ERROR_CALLBACK = 18;
    private static final int SET_PARAMETERS = 19;
    private static final int GET_PARAMETERS = 20;
    private static final int SET_PARAMETERS_ASYNC = 21;
    private static final int WAIT_FOR_IDLE = 22;
    private static final int REPROCESS = 23;

    private Handler mCameraHandler;
    private CPCameraProxy mCameraProxy;
    private com.ti.omap.android.cpcam.CPCam mCPCamera;

    public static CPCameraManager instance() {
        return sCameraManager;
    }

    private CPCameraManager() {
        HandlerThread ht = new HandlerThread("Camera Handler Thread");
        ht.start();
        mCameraHandler = new CameraHandler(ht.getLooper());
    }

    private class CameraHandler extends Handler {
        CameraHandler(Looper looper) {
            super(looper);
        }

        @Override
        public void handleMessage(final Message msg) {
            try {
                switch (msg.what) {
                    case RELEASE:
                        mCPCamera.release();
                        mCPCamera = null;
                        mCameraProxy = null;
                        break;

                    case RECONNECT:
                        mReconnectException = null;
                        try {
                            mCPCamera.reconnect();
                        } catch (IOException ex) {
                            mReconnectException = ex;
                        }
                        break;

                    case UNLOCK:
                        mCPCamera.unlock();
                        break;

                    case LOCK:
                        mCPCamera.lock();
                        break;

                    case SET_PREVIEW_TEXTURE_ASYNC:
                        try {
                            mCPCamera.setPreviewTexture((SurfaceTexture) msg.obj);
                        } catch(IOException e) {
                            throw new RuntimeException(e);
                        }
                        return;  // no need to call mSig.open()

                    case START_PREVIEW_ASYNC:
                        mCPCamera.startPreview();
                        return;  // no need to call mSig.open()

                    case STOP_PREVIEW:
                        mCPCamera.stopPreview();
                        break;

                    case SET_PREVIEW_CALLBACK_WITH_BUFFER:
                        mCPCamera.setPreviewCallbackWithBuffer(
                            (PreviewCallback) msg.obj);
                        break;

                    case ADD_CALLBACK_BUFFER:
                        mCPCamera.addCallbackBuffer((byte[]) msg.obj);
                        break;

                    case AUTO_FOCUS:
                        mCPCamera.autoFocus((AutoFocusCallback) msg.obj);
                        break;

                    case CANCEL_AUTO_FOCUS:
                        mCPCamera.cancelAutoFocus();
                        break;

                    case SET_AUTO_FOCUS_MOVE_CALLBACK:
                        mCPCamera.setAutoFocusMoveCallback(
                            (AutoFocusMoveCallback) msg.obj);
                        break;

                    case SET_DISPLAY_ORIENTATION:
                        mCPCamera.setDisplayOrientation(msg.arg1);
                        break;

                    case SET_ZOOM_CHANGE_LISTENER:
                        mCPCamera.setZoomChangeListener(
                            (OnZoomChangeListener) msg.obj);
                        break;

                    case SET_FACE_DETECTION_LISTENER:
                        mCPCamera.setFaceDetectionListener(
                            (FaceDetectionListener) msg.obj);
                        break;

                    case START_FACE_DETECTION:
                        mCPCamera.startFaceDetection();
                        break;

                    case STOP_FACE_DETECTION:
                        mCPCamera.stopFaceDetection();
                        break;

                    case SET_ERROR_CALLBACK:
                        mCPCamera.setErrorCallback((ErrorCallback) msg.obj);
                        break;

                    case SET_PARAMETERS:
                        mCPCamera.setParameters((Parameters) msg.obj);
                        break;

                    case GET_PARAMETERS:
                        mParameters = mCPCamera.getParameters();
                        break;

                    case REPROCESS:
                        try {
                            mCPCamera.reprocess((Parameters) msg.obj);
                        } catch (IOException ioe) {
                            ioe.printStackTrace();
                        }
                        break;

                    case SET_PARAMETERS_ASYNC:
                        mCPCamera.setParameters((Parameters) msg.obj);
                        return;  // no need to call mSig.open()

                    case WAIT_FOR_IDLE:
                        // do nothing
                        break;
                }
            } catch (RuntimeException e) {
                if (msg.what != RELEASE && mCPCamera != null) {
                    try {
                        mCPCamera.release();
                    } catch (Exception ex) {
                        Log.e(TAG, "Fail to release the camera.");
                    }
                    mCPCamera = null;
                    mCameraProxy = null;
                }
                throw e;
            }
            mSig.open();
        }
    }

    // Open camera synchronously. This method is invoked in the context of a
    // background thread.
    CPCameraProxy cameraOpen(int cameraId) {
        // Cannot open camera in mCameraHandler, otherwise all camera events
        // will be routed to mCameraHandler looper, which in turn will call
        // event handler like Camera.onFaceDetection, which in turn will modify
        // UI and cause exception like this:
        // CalledFromWrongThreadException: Only the original thread that created
        // a view hierarchy can touch its views.
        mCPCamera = com.ti.omap.android.cpcam.CPCam.open(cameraId);
        if (mCPCamera != null) {
            mCameraProxy = new CPCameraProxy();
            return mCameraProxy;
        } else {
            return null;
        }
    }

    public class CPCameraProxy {
        private CPCameraProxy() {
            Assert(mCPCamera != null);
        }

        public com.ti.omap.android.cpcam.CPCam getCamera() {
            return mCPCamera;
        }

        public void release() {
            mSig.close();
            mCameraHandler.sendEmptyMessage(RELEASE);
            mSig.block();
        }

        public void reconnect() throws IOException {
            mSig.close();
            mCameraHandler.sendEmptyMessage(RECONNECT);
            mSig.block();
            if (mReconnectException != null) {
                throw mReconnectException;
            }
        }

        public void unlock() {
            mSig.close();
            mCameraHandler.sendEmptyMessage(UNLOCK);
            mSig.block();
        }

        public void lock() {
            mSig.close();
            mCameraHandler.sendEmptyMessage(LOCK);
            mSig.block();
        }

        public void setPreviewTextureAsync(final SurfaceTexture surfaceTexture) {
            mCameraHandler.obtainMessage(SET_PREVIEW_TEXTURE_ASYNC, surfaceTexture).sendToTarget();
        }

        public void startPreviewAsync() {
            mCameraHandler.sendEmptyMessage(START_PREVIEW_ASYNC);
        }

        public void stopPreview() {
            mSig.close();
            mCameraHandler.sendEmptyMessage(STOP_PREVIEW);
            mSig.block();
        }

        public void setPreviewCallbackWithBuffer(final PreviewCallback cb) {
            mSig.close();
            mCameraHandler.obtainMessage(SET_PREVIEW_CALLBACK_WITH_BUFFER, cb).sendToTarget();
            mSig.block();
        }

        public void addCallbackBuffer(byte[] callbackBuffer) {
            mSig.close();
            mCameraHandler.obtainMessage(ADD_CALLBACK_BUFFER, callbackBuffer).sendToTarget();
            mSig.block();
        }

        public void autoFocus(AutoFocusCallback cb) {
            mSig.close();
            mCameraHandler.obtainMessage(AUTO_FOCUS, cb).sendToTarget();
            mSig.block();
        }

        public void cancelAutoFocus() {
            mSig.close();
            mCameraHandler.sendEmptyMessage(CANCEL_AUTO_FOCUS);
            mSig.block();
        }

        public void setAutoFocusMoveCallback(AutoFocusMoveCallback cb) {
            mSig.close();
            mCameraHandler.obtainMessage(SET_AUTO_FOCUS_MOVE_CALLBACK, cb).sendToTarget();
            mSig.block();
        }

        public void takePicture(final ShutterCallback shutter, final PictureCallback raw,
                final PictureCallback postview, final PictureCallback jpeg, final Parameters param) {
            mSig.close();
            // Too many parameters, so use post for simplicity
            mCameraHandler.post(new Runnable() {
                @Override
                public void run() {
                    mCPCamera.takePicture(shutter, raw, postview, jpeg, param);
                    mSig.open();
                }
            });
            mSig.block();
        }

        public void setDisplayOrientation(int degrees) {
            mSig.close();
            mCameraHandler.obtainMessage(SET_DISPLAY_ORIENTATION, degrees, 0)
                    .sendToTarget();
            mSig.block();
        }

        public void setZoomChangeListener(OnZoomChangeListener listener) {
            mSig.close();
            mCameraHandler.obtainMessage(SET_ZOOM_CHANGE_LISTENER, listener).sendToTarget();
            mSig.block();
        }

        public void setFaceDetectionListener(FaceDetectionListener listener) {
            mSig.close();
            mCameraHandler.obtainMessage(SET_FACE_DETECTION_LISTENER, listener).sendToTarget();
            mSig.block();
        }

        public void setBufferSource(final CPCamBufferQueue tapIn, final CPCamBufferQueue tapOut) {
            mSig.close();
            // Too many parameters, so use post for simplicity
            mCameraHandler.post(new Runnable() {
                @Override
                public void run() {
                    try {
                        mCPCamera.setBufferSource(tapIn, tapOut);
                    } catch (IOException ioe) {
                        Log.e(TAG, "Error trying to setBufferSource!");
                    }
                    mSig.open();
                }
            });
            mSig.block();
        }

        public void startFaceDetection() {
            mSig.close();
            mCameraHandler.sendEmptyMessage(START_FACE_DETECTION);
            mSig.block();
        }

        public void stopFaceDetection() {
            mSig.close();
            mCameraHandler.sendEmptyMessage(STOP_FACE_DETECTION);
            mSig.block();
        }

        public void setErrorCallback(ErrorCallback cb) {
            mSig.close();
            mCameraHandler.obtainMessage(SET_ERROR_CALLBACK, cb).sendToTarget();
            mSig.block();
        }

        public void setParameters(Parameters params) {
            mSig.close();
            mCameraHandler.obtainMessage(SET_PARAMETERS, params).sendToTarget();
            mSig.block();
        }

        public void setParametersAsync(Parameters params) {
            mCameraHandler.removeMessages(SET_PARAMETERS_ASYNC);
            mCameraHandler.obtainMessage(SET_PARAMETERS_ASYNC, params).sendToTarget();
        }

        public Parameters getParameters() {
            mSig.close();
            mCameraHandler.sendEmptyMessage(GET_PARAMETERS);
            mSig.block();
            return mParameters;
        }

        public void waitForIdle() {
            mSig.close();
            mCameraHandler.sendEmptyMessage(WAIT_FOR_IDLE);
            mSig.block();
        }

        public void reprocess(Parameters shotParams) {
            mSig.close();
            mCameraHandler.obtainMessage(REPROCESS, shotParams).sendToTarget();
            mSig.block();
        }

    }
}
