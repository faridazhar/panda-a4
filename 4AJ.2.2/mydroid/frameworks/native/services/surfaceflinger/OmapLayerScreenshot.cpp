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


#include "OmapLayerScreenshot.h"
#include "S3DSurfaceFlinger.h"

namespace android {

OmapLayerScreenshot::OmapLayerScreenshot(S3DSurfaceFlinger* flinger,
            DisplayID display,const sp<Client>& client)
         :  LayerScreenshot(flinger, display, client),
            mFlingerS3D(flinger)
{
    mType = eMono;
    mViewOrder = eLeftViewFirst;
}

uint32_t OmapLayerScreenshot::doTransaction(uint32_t flags)
{
    const Layer::State& draw(drawingState());
    const Layer::State& curr(currentState());

    if (draw.flags & ISurfaceComposer::eLayerHidden) {
        if (!(curr.flags & ISurfaceComposer::eLayerHidden)) {
            // we're going from hidden to visible
            if (mFlingerS3D->isS3DLayerVisible_l()) {
                mType = eSideBySide;
                flags = LayerScreenshot::doTransaction(flags);
                mFlingerS3D->addS3DLayer_l(this);
                return flags;
            } else {
                mType = eMono;
            }
        }
    }
    return LayerScreenshot::doTransaction(flags);
}

void OmapLayerScreenshot::onRemoved()
{
    mFlingerS3D->removeS3DLayer_l(this);
    LayerScreenshot::onRemoved();
}

void OmapLayerScreenshot::drawS3DRegion(const Region& clip, int hw_w, int hw_h) const
{
    GLfloat u = mTexCoords[4];
    GLfloat v = mTexCoords[1];

    struct TexCoords {
        GLfloat u;
        GLfloat v;
    };

    TexCoords texCoords[4];
    texCoords[0].u = 0;
    texCoords[0].v = v;
    texCoords[1].u = 0;
    texCoords[1].v = 0;
    texCoords[2].u = u;
    texCoords[2].v = 0;
    texCoords[3].u = u;
    texCoords[3].v = v;

    if(mFlingerS3D->isDrawingLeft()) {
        texCoords[2].u = 0.5f*u;
        texCoords[3].u = 0.5f*u;
    }
    else {
        texCoords[0].u = 0.5f*u;
        texCoords[1].u = 0.5f*u;
    }

    glTexCoordPointer(2, GL_FLOAT, 0, texCoords);
    glDrawArrays(GL_TRIANGLE_FAN, 0, mNumVertices);
}

void OmapLayerScreenshot::drawRegion(const Region& clip, int hw_w, int hw_h) const
{
    if (mFlingerS3D->isDefaultRender() || (!isS3D() && !mFlingerS3D->isFramePackingRender())) {
        LayerScreenshot::drawRegion(clip, hw_w, hw_h);
        return;
    }

    if (isS3D() && mFlingerS3D->isInterleaveRender()) {
        glEnable(GL_STENCIL_TEST);
    } else if (isS3D() && mFlingerS3D->isAnaglyphRender()) {
        //Left view = RED
        glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_TRUE);
    }

    drawS3DRegion(clip, hw_w, hw_h);

    //This layer draws its right view here as the viewport is not changed.
    //This is done so that blending of any higher z layers with this one is correct.
    if(isS3D() && !mFlingerS3D->isFramePackingRender()) {
        mFlingerS3D->setDrawState(S3DSurfaceFlinger::eDrawingS3DRight);
        if (mFlingerS3D->isAnaglyphRender()) {
            //right view = cyan
            glColorMask(GL_FALSE, GL_TRUE, GL_TRUE, GL_TRUE);
        }
        drawS3DRegion(clip, hw_w, hw_h);
        mFlingerS3D->setDrawState(S3DSurfaceFlinger::eDrawingS3DLeft);
    }

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDisable(GL_STENCIL_TEST);
}

};
