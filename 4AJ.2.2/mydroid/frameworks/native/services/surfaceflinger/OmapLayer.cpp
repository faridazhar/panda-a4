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


#include "OmapLayer.h"
#include "S3DSurfaceFlinger.h"

namespace android {

OmapLayer::OmapLayer(S3DSurfaceFlinger* flinger, DisplayID display, const sp<Client>& client)
         :  Layer(flinger, display, client),
            mFlingerS3D(flinger),
            mType(eMono),
            mViewOrder(eLeftViewFirst),
            mRenderMode(eRenderStereo),
            mLockConfig(false)
{
}

void OmapLayer::setConfig(S3DLayoutType type, S3DLayoutOrder order, S3DRenderMode mode)
{
    mType = type;
    mViewOrder = order;
    mRenderMode = mode;
    //Mono layers are always allowed to change into stereoscopic
    //through set_buffers_layout api
    mLockConfig = (mType == eMono) ? false : true;
    //Cause redraw of visible region
    mCurrentState.sequence++;
    requestTransaction();
}

void OmapLayer::onRemoved()
{
    mFlingerS3D->removeS3DLayer_l(this);
    Layer::onRemoved();
}

void OmapLayer::setGeometry(hwc_layer_t* hwcl)
{
    Layer::setGeometry(hwcl);
    hwcl->flags &= ~S3DLayoutTypeMask;
    hwcl->flags |= mType << S3DLayoutTypeShift;
    hwcl->flags &= ~S3DLayoutOrderMask;
    hwcl->flags |= mViewOrder << S3DLayoutOrderShift;
    hwcl->flags &= ~S3DRenderModeMask;
    hwcl->flags |= mRenderMode << S3DRenderModeShift;
}

void OmapLayer::lockPageFlip(bool& recomputeVisibleRegions)
{
    Layer::lockPageFlip(recomputeVisibleRegions);
#ifdef OMAP_ENHANCEMENT
    uint8_t s3d_type = (mCurrentLayout >> 16 ) & 0xFF;
    uint8_t s3d_order = (mCurrentLayout >> 24 ) & 0xFF;
    S3DLayoutType prevType = mType;
    switch (s3d_type) {
        case eSideBySide:
        case eTopBottom:
        case eRowInterleaved:
        case eColInterleaved:
            mType = static_cast<S3DLayoutType>(s3d_type);
            break;
        default:
            //If the layer has been explicitly configured through
            //setConfig we don't want to reset the layer type back to
            //mono
            if (!mLockConfig) mType = eMono;
            return;
    }
    switch (s3d_order) {
        case eLeftViewFirst:
        case eRightViewFirst:
            mViewOrder = static_cast<S3DLayoutOrder>(s3d_order);
        default:
            //Invalid order
            mViewOrder = eLeftViewFirst;
            break;
    }
    if (mType != prevType && mType != eMono) {
        mFlingerS3D->addS3DLayer_l(this);
    }
#endif
}

void OmapLayer::drawWithOpenGL(const Region& clip) const
{
    if (mFlingerS3D->isDefaultRender() ||
        (!isS3D() && !mFlingerS3D->isFramePackingRender())) {
        //No custom drawing needed.
        //TODO: filter Monoscopic layers when doing interleaved rendering
        //if high-quality setting is selected
        LayerBase::drawWithOpenGL(clip);
        return;
    }

    //Enable filtering for custom drawing as there scaling will occur
    glTexParameterx(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterx(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    S3DRenderMode viewToRender = mFlingerS3D->isDrawingLeft() ? eRenderLeft : eRenderRight;
    if (isS3D()&& mRenderMode != eRenderStereo && mRenderMode != viewToRender) {
        //If the layer has been configured to render a specific view (left or right)
        //then draw that view, not the one being asked for by surfaceflinger
        if (mFlingerS3D->isFramePackingRender() || mFlingerS3D->isMonoRender()) {
            viewToRender = mRenderMode;
        }
    }

    if (isS3D() && mFlingerS3D->isInterleaveRender() && mRenderMode == eRenderStereo) {
        glEnable(GL_STENCIL_TEST);
    } else if (isS3D() && mFlingerS3D->isAnaglyphRender() && mRenderMode == eRenderStereo) {
        //Left view = RED
        glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_TRUE);
    }

    drawWithOpenGL(clip, isDrawingFirstHalf(viewToRender));

    //This layer draws its right view here as the viewport is not changed.
    //This is done so that blending of any higher z layers with this one is correct.
    if (isS3D() && mRenderMode == eRenderStereo &&
        !mFlingerS3D->isFramePackingRender() && !mFlingerS3D->isMonoRender()) {
        mFlingerS3D->setDrawState(S3DSurfaceFlinger::eDrawingS3DRight);
        if (mFlingerS3D->isAnaglyphRender()) {
            //right view = cyan
            glColorMask(GL_FALSE, GL_TRUE, GL_TRUE, GL_TRUE);
        }
        drawWithOpenGL(clip, isDrawingFirstHalf(eRenderRight));
        mFlingerS3D->setDrawState(S3DSurfaceFlinger::eDrawingS3DLeft);
    }

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDisable(GL_STENCIL_TEST);
}

void OmapLayer::drawWithOpenGL(const Region& clip, bool drawFirstHalf) const
{
    const DisplayHardware& hw(graphicPlane(0).displayHardware());
    const uint32_t fbHeight = hw.getHeight();
    const State& s(drawingState());

    GLenum src = mPremultipliedAlpha ? GL_ONE : GL_SRC_ALPHA;
    if (CC_UNLIKELY(s.alpha < 0xFF)) {
        const GLfloat alpha = s.alpha * (1.0f/255.0f);
        if (mPremultipliedAlpha) {
            glColor4f(alpha, alpha, alpha, alpha);
        } else {
            glColor4f(1, 1, 1, alpha);
        }
        glEnable(GL_BLEND);
        glBlendFunc(src, GL_ONE_MINUS_SRC_ALPHA);
        glTexEnvx(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    } else {
        glColor4f(1, 1, 1, 1);
        glTexEnvx(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
        if (!isOpaque()) {
            glEnable(GL_BLEND);
            glBlendFunc(src, GL_ONE_MINUS_SRC_ALPHA);
        } else {
            glDisable(GL_BLEND);
        }
    }

    struct TexCoords {
        GLfloat u;
        GLfloat v;
    };

    Rect crop(s.active.w, s.active.h);
    if (!s.active.crop.isEmpty()) {
        crop = s.active.crop;
    }

    GLfloat left = GLfloat(crop.left) / GLfloat(s.active.w);
    GLfloat top = GLfloat(crop.top) / GLfloat(s.active.h);
    GLfloat right = GLfloat(crop.right) / GLfloat(s.active.w);
    GLfloat bottom = GLfloat(crop.bottom) / GLfloat(s.active.h);

    if (isS3D()) {
        if (drawFirstHalf) {
            switch(mType) {
                //top half
                case eTopBottom:
                    top *= 0.5f;
                    bottom *= 0.5f;
                    break;
                //left half
                case eSideBySide:
                    left *= 0.5f;
                    right *= 0.5f;
                    break;
                default:
                    //Should never happen!
                    break;
            }
        } else {
            switch(mType) {
                //bottom half
                case eTopBottom:
                    top = top*0.5f + 0.5f;
                    bottom = bottom*0.5f + 0.5f;
                    break;
                //right half
                case eSideBySide:
                    left = left*0.5f + 0.5f;
                    right = right*0.5f + 0.5f;
                    break;
                default:
                    //Should never happen!
                    break;
            }
        }
    }

    TexCoords texCoords[4];
    texCoords[0].u = left;
    texCoords[0].v = top;
    texCoords[1].u = left;
    texCoords[1].v = bottom;
    texCoords[2].u = right;
    texCoords[2].v = bottom;
    texCoords[3].u = right;
    texCoords[3].v = top;
    for (int i = 0; i < 4; i++) {
        texCoords[i].v = 1.0f - texCoords[i].v;
    }

    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, mVertices);
    glTexCoordPointer(2, GL_FLOAT, 0, texCoords);
    glDrawArrays(GL_TRIANGLE_FAN, 0, mNumVertices);

    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisable(GL_BLEND);
}

};
