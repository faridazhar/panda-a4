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

package com.ti.s3d.cube;

import java.util.ArrayList;
import javax.microedition.khronos.opengles.GL10;

import android.content.Context;

import com.ti.s3d.render.Scene;

public class CubeScene implements Scene {

    private TexturedCube mCube;
    private ArrayList<Transform> transforms;
    private float mAngle;
    private float mAngleDelta;
    private int h;

    //Draws a cube formation of textured cubes
    public CubeScene(Context context, int resourceId) {
        mCube = new TexturedCube(context, resourceId);
        transforms = new ArrayList<Transform>(9);
        float location = 2.0f;
        transforms.add(new Transform(0.0f, 0.0f, 0.0f));
        for (int i=0;i<8;i++) {
            float x = ( (i & 0x4) == 0) ? -location : location;
            float y = ( (i & 0x2) == 0) ? -location : location;
            float z = ( (i & 0x1) == 0) ? -location : location;
            transforms.add(new Transform(x, y, z));
        }
        mAngleDelta = 1.5f;
    }

    public void draw(GL10 gl) {
        mCube.drawBatchStart(gl);
        for(Transform t : transforms) {
            gl.glPushMatrix();
            t.apply(gl,mAngle);
            mCube.draw(gl);
            gl.glPopMatrix();
        }
        mCube.drawBatchStop(gl);
    }

    public void drawEnd(GL10 gl) {
        mAngle += mAngleDelta;
    }

    public void setup(GL10 gl, int w, int h) {
        mCube.setup(gl);
        this.h = h;
    }

    public void onTouch(float deltaX, float deltaY) {
        deltaY /= -h;
        deltaY *= 20.0f;
        mAngleDelta += deltaY;
        if(mAngleDelta < 0.0f)
            mAngleDelta = 0.0f;
        else if(mAngleDelta > 20.0f)
            mAngleDelta = 20.0f;
    }

    private static class Transform {
        //Translation
        private float x, y, z;
        //Angle ratios
        private float ax, ay, az;

        private Transform(float x, float y, float z) {
            this.x = x;
            this.y = y;
            this.z = z;
            ax = (float)Math.random();
            ay = (float)Math.random();
            az = (float)Math.random();
        }

        private void apply(GL10 gl, float angle) {
            gl.glTranslatef(x, y, z);
            gl.glRotatef(ax*angle, 1, 0, 0);
            gl.glRotatef(ay*angle, 0, 1, 0);
            gl.glRotatef(az*angle, 0, 0, 1);
        }
    }
}
