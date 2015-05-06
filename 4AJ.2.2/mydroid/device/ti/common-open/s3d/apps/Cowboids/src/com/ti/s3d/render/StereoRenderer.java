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

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;
import android.opengl.GLSurfaceView;

import java.lang.Math;

/**
 * Render a stereo scene.
 */

public class StereoRenderer implements GLSurfaceView.Renderer {

    private static final float DEGREES_TO_RADIANS = 0.0174532925f;

    private float mDepthZ = -10.0f;
    private static final float nearZ = 3.0f;
    private static final float farZ = 30.0f;
    private static final float maxDepthZ = -(nearZ+2.0f);
    private static final float minDepthZ = -(farZ-7.0f);
    private static final float screenPlaneZ = 10.0f;
    private static final float fovy = 45.0f;
    //private static final float IOD = 0.2f;
    private float IOD = 0.2f;

    private int width;
    private int height;
    private S3DRenderMode renderMode;
    private Camera leftCam = new Camera(S3DView.LEFT);
    private Camera rightCam = new Camera(S3DView.RIGHT);

    private Scene scene;

    public StereoRenderer(Scene scene) {
        renderMode = S3DRenderMode.SIDE_BY_SIDE;
        this.scene = scene;
    }

    private void setViewPort(GL10 gl, S3DView view) {
        gl.glViewport(renderMode.viewportX(view,width),
        renderMode.viewportY(view,height),
        renderMode.viewportWidth(width),
        renderMode.viewportHeight(height));
    }

    private void drawScene(GL10 gl, S3DView view) {
        setViewPort(gl, view);

        gl.glMatrixMode(GL10.GL_MODELVIEW);
        gl.glLoadIdentity();
        gl.glPushMatrix();
        gl.glTranslatef(0.0f, 0.0f, mDepthZ);
        scene.draw(gl);
        gl.glPopMatrix();
    }

    public void onDrawFrame(GL10 gl) {
        gl.glClear(GL10.GL_COLOR_BUFFER_BIT | GL10.GL_DEPTH_BUFFER_BIT);

        leftCam.apply(gl);
        drawScene(gl, S3DView.LEFT);

        rightCam.apply(gl);
        drawScene(gl, S3DView.RIGHT);

        scene.drawEnd(gl);
    }

    private void setupGL(GL10 gl) {
        gl.glClearColor(0,0,0,0);
        gl.glEnable(GL10.GL_CULL_FACE);
        gl.glEnable(GL10.GL_DEPTH_TEST);
    }

    public void onSurfaceChanged(GL10 gl, int w, int h) {
        width = w;
        height = h;
        setupGL(gl);
        scene.setup(gl, w, h);

        float aspect = (float)w/(float)h;
        leftCam.setup(IOD, aspect);
        rightCam.setup(IOD, aspect);
    }

    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
        gl.glDisable(GL10.GL_DITHER);
        gl.glEnable(GL10.GL_CULL_FACE);
        gl.glEnable(GL10.GL_DEPTH_TEST);
        gl.glEnable(GL10.GL_TEXTURE_2D);
        gl.glShadeModel(GL10.GL_SMOOTH);
        gl.glClearColor(0,0,0,0);
        gl.glHint(GL10.GL_PERSPECTIVE_CORRECTION_HINT, GL10.GL_NICEST);
    }

    public void moveCam(float deltaX) {
        deltaX /= (float)width;
        deltaX *= 20.0f;
        mDepthZ += deltaX;
        if(mDepthZ >= maxDepthZ)
            mDepthZ = maxDepthZ;
        else if(mDepthZ <= minDepthZ)
            mDepthZ = minDepthZ;
    }

    public void changeIOD(float deltaY) {
        deltaY /= (float)height;
        IOD += deltaY;
        if(IOD > 0.75f)
            IOD=0.75f;
        else if(IOD < 0.0f)
            IOD=0.0f;
        float aspect = (float)width/(float)height;
        leftCam.setup(IOD, aspect);
        rightCam.setup(IOD, aspect);
    }

    private static enum S3DView { LEFT, RIGHT };
    protected static enum S3DRenderMode {
        SIDE_BY_SIDE {
            S3DRenderMode next() { return OVER_UNDER; }
            int viewportWidth(int width) { return width/2; }
            int viewportHeight(int height) { return height; }
            int viewportX(S3DView view, int w) { return view == S3DView.LEFT ? 0 : w/2; }
            int viewportY(S3DView view, int h) { return 0; }
        },
        OVER_UNDER {
            S3DRenderMode next() { return SIDE_BY_SIDE; }
            int viewportWidth(int width) { return width; }
            int viewportHeight(int height) { return height/2; }
            int viewportX(S3DView view, int w) { return 0; }
            int viewportY(S3DView view, int h) { return (view == S3DView.RIGHT) ? 0 : h/2; }
        };

        abstract S3DRenderMode next();
        abstract int viewportWidth(int width);
        abstract int viewportHeight(int height);
        abstract int viewportX(S3DView view, int w);
        abstract int viewportY(S3DView view, int h);
    }

    private static class Camera {

        private Camera(S3DView view) {
            this.view = view;
        };

        private void setup(float IOD, float aspect) {
            float top = nearZ*(float)Math.tan(DEGREES_TO_RADIANS*fovy/2.0f);
            float r = aspect*top;
            float frustumshift = (IOD/2.0f)*nearZ/screenPlaneZ;
            this.top = top;
            this.bottom = -top;
            this.left = view == S3DView.LEFT ? -r + frustumshift : -r-frustumshift;
            this.right = view == S3DView.LEFT ? r + frustumshift : r-frustumshift;
            this.x = view == S3DView.LEFT ? IOD/2.0f : -IOD/2.0f;
        }

        private void apply(GL10 gl) {
            gl.glMatrixMode(GL10.GL_PROJECTION);
            gl.glLoadIdentity();
            gl.glFrustumf(left, right, bottom, top,nearZ, farZ);
            gl.glTranslatef(x, 0.0f, 0.0f);
        }

        private float left;
        private float right;
        private float bottom;
        private float top;
        private float x;
        private S3DView view;
    }
}
