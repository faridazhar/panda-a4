/*
 * Copyright (C) 2011 Texas Instruments Inc.
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

package com.ti.s3d.render;

import android.view.MotionEvent;
import android.opengl.GLSurfaceView;
import android.content.Context;

import com.ti.s3d.S3DView;


public class StereoGLView extends GLSurfaceView {

    public StereoGLView(Context context, Scene scene) {
        super(context);
        this.mScene = scene;
        //Create an S3D view to signal we are rendering stereo content
        s3dView = new S3DView(getHolder());
        mRenderer = new StereoRenderer(scene);
        setRenderer(mRenderer);
        mPrevX = 0;
        mPrevY = 0;
    }

    @Override
    public boolean onTouchEvent(MotionEvent e) {
        if (e.getActionMasked() == MotionEvent.ACTION_DOWN) {
            mPrevX = e.getX();
            mPrevY = e.getY();
        }
        if (e.getActionMasked() == MotionEvent.ACTION_MOVE) {
            float deltaX = e.getX() - mPrevX;
            float deltaY = e.getY() - mPrevY;
            mRenderer.moveCam(deltaX);
            mScene.onTouch(deltaX, deltaY);
            mPrevX = e.getX();
            mPrevY = e.getY();
        }
        return true;
    }

    private float mPrevX;
    private float mPrevY;
    private S3DView s3dView;
    private StereoRenderer mRenderer;
    private Scene mScene;
}
