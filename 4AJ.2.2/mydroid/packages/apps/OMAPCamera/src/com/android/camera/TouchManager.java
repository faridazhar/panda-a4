package com.ti.omap.android.camera;

import android.graphics.Matrix;
import android.graphics.Rect;
import android.graphics.RectF;
import android.hardware.Camera.Area;
import android.util.Log;
import android.view.View;
import android.view.MotionEvent;
import android.view.ViewGroup;
import android.widget.RelativeLayout;

import java.util.ArrayList;
import java.util.List;

// A class that handles the metering area
public class TouchManager {
    private static final String TAG = "TouchManager";

    private boolean mInitialized;
    private List<Area> mMeteringArea; // metering area in driver format
    private int mTouchWidth;
    private int mTouchHeight;
    private int mDisplayOrientation;
    private boolean mMirror;
    private int mPreviewWidth;
    private int mPreviewHeight;
    private Matrix mMatrix;
    Listener mListener;
    private String mPreviewLayout;

    public interface Listener {
        public void setTouchParameters();
    }

    public TouchManager() {
        mMatrix = new Matrix();
    }

    public void initialize(Listener listener, boolean mirror, int displayOrientation) {
        mListener       = listener;
        mDisplayOrientation = displayOrientation;
        mMirror = mirror;
    }

    public void setPreviewSize(int previewWidth, int previewHeight) {
        if (mPreviewWidth != previewWidth || mPreviewHeight != previewHeight) {
            mTouchWidth = previewHeight / 3;
            mTouchHeight = previewHeight / 3;
            mPreviewWidth = previewWidth;
            mPreviewHeight = previewHeight;
            Matrix matrix = new Matrix();
            Util.prepareMatrix(matrix, mMirror, mDisplayOrientation,
                    mPreviewWidth, mPreviewHeight);
            // In face detection, the matrix converts the driver coordinates to UI
            // coordinates. When touching, the inverted matrix converts the UI
            // coordinates to driver coordinates.
            matrix.invert(mMatrix);

            mInitialized = true;
        }
    }

    public void calculateTapArea(int touchWidth, int touchHeight, float areaMultiple,
            int x, int y, int previewWidth, int previewHeight, Rect rect) {

        int areaWidth = (int)(touchWidth * areaMultiple);
        int areaHeight = (int)(touchHeight * areaMultiple);
        int left = Util.clamp(x - areaWidth / 2, 0, previewWidth - areaWidth);
        int top = Util.clamp(y - areaHeight / 2, 0, previewHeight - areaHeight);

        RectF rectF = new RectF(left, top, left + areaWidth, top + areaHeight);
        mMatrix.mapRect(rectF);
        Util.rectFToRect(rectF, rect);
    }

    public boolean onTouch(MotionEvent e) {
        if(!mInitialized)
            return false;
        // Initialize variables.
        int x = Math.round(e.getX());
        int y = Math.round(e.getY());
        if (mMeteringArea == null) {
            mMeteringArea = new ArrayList<Area>();
            mMeteringArea.add(new Area(new Rect(), 1));
        }

        // Convert the coordinates to driver format.

/*
        if (CameraSettings.SS_FULL_S3D_LAYOUT.equals(mPreviewLayout)) {
            calculateTapArea(mTouchWidth, mTouchHeight * 2, 1, x, y, mPreviewWidth, mPreviewHeight,
                    mMeteringArea.get(0).rect);
        } else if (CameraSettings.TB_FULL_S3D_LAYOUT.equals(mPreviewLayout)) {
            calculateTapArea(mTouchWidth * 2, mTouchHeight, 1, x, y, mPreviewWidth, mPreviewHeight,
                    mMeteringArea.get(0).rect);
        } else {
*/
            calculateTapArea(mTouchWidth, mTouchHeight, 1, x, y, mPreviewWidth, mPreviewHeight,
                    mMeteringArea.get(0).rect);
//        }

        // Set the metering area.
        mListener.setTouchParameters();
        return true;
    }

/*
    public boolean onTouch(MotionEvent e, String previewLayout) {
        if(!mInitialized)
            return false;
        float x = e.getX();
        float y = e.getY();
        mPreviewLayout = previewLayout;
        if (CameraSettings.SS_FULL_S3D_LAYOUT.equals(previewLayout)) {
            if (y < mPreviewHeight / 2) {
                return onTouch(e);
            } else {
                e.setLocation(x, y - mPreviewHeight / 2);
                return onTouch(e);
            }
        } else if (CameraSettings.TB_FULL_S3D_LAYOUT.equals(previewLayout)) {
            if (x < mPreviewWidth / 2) {
                return onTouch(e);
            } else {
                e.setLocation(x - mPreviewWidth / 2, y);
                return onTouch(e);
            }
        } else if (CameraSettings.TB_SUB_S3D_LAYOUT.equals(previewLayout)) {
            if (x < mPreviewWidth / 2) {
                e.setLocation(x * 2, y);
                return onTouch(e);
            } else {
                e.setLocation((x - mPreviewWidth / 2) * 2, y);
                return onTouch(e);
            }
        } else if (CameraSettings.SS_SUB_S3D_LAYOUT.equals(previewLayout)) {
            if (y < mPreviewHeight / 2) {
                e.setLocation(x, y * 2);
                return onTouch(e);
            } else {
                e.setLocation(x, (y - mPreviewHeight / 2) * 2);
                return onTouch(e);
            }
        } else {
            return onTouch(e);
        }
    }
*/

    public List<Area> getMeteringAreas() {
        return mMeteringArea;
    }

}
