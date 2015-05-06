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

#define LOG_TAG "S3DView"
#include "utils/Log.h"

#include "jni.h"
#include "JNIHelp.h"

#include <binder/Parcel.h>
#include <binder/IServiceManager.h>
#include <gui/Surface.h>
#include <ui/S3DFormat.h>

using namespace android;

struct so_t {
    jfieldID surface;
};
static so_t so;

struct lo_t {
    jfieldID mono;
    jfieldID sbs_LR;
    jfieldID sbs_RL;
    jfieldID tb_L;
    jfieldID tb_R;
};
static lo_t lo;

enum { //keep in sync with S3DSurfaceFlinger.h
    SET_SURF_CONFIG = 4000,
    GET_PREF_LAYOUT = 4001,
    SET_WINDOW_CONFIG = 4003,
};

// ---------------------------------------------------------------------------
static sp<Surface> getSurface(JNIEnv* env, jobject surfobj) {
    sp<Surface> surf(reinterpret_cast<Surface*>(env->GetIntField(surfobj, so.surface)));
    if (surf == 0) {
        jniThrowException(env, "java/lang/NullPointerException", "Failed obtaining native surface");
    }
    if (!Surface::isValid(surf)) {
        jniThrowException(env, "java/lang/IllegalArgumentException", "Invalid surface");
    }
    return surf;
}

static status_t sendCommand(uint32_t cmd,
                            const sp<Surface> surface,
                            const String8& name,
                            Parcel *reply,
                            const int32_t *data,
                            const int32_t nElements) {
    sp<IServiceManager> sm = defaultServiceManager();
    sp<IBinder> service = sm->checkService(String16("SurfaceFlinger"));
    if (service == NULL) {
        ALOGE("failed to find SurfaceFlinger service");
        return BAD_VALUE;
    }

    Parcel parcel, dummy, rep;
    status_t status;
    //Could not obtain the interface name
    status = service->transact(IBinder::INTERFACE_TRANSACTION, dummy, &rep);
    if (status != NO_ERROR) {
        ALOGE("failed to get SurfaceFlinger service interface name");
        return status;
    }

    String16 ifName = rep.readString16();
    if (ifName.size() <= 0) {
        ALOGE("interface name is empty");
        return BAD_VALUE;
    }

    parcel.writeInterfaceToken(ifName);

    if (surface != NULL) {
        parcel.writeStrongBinder(surface->asBinder());
    }

    if (!name.isEmpty()) {
        parcel.writeString8(name);
    }

    for (int32_t i = 0; i < nElements; i++)
        parcel.writeInt32(data[i]);

    status = service->transact(cmd, parcel, reply);
    if (status)
        ALOGE("failed transacting with surfaceflinger (%x)", status);

    return status;
}

static void S3DView_setConfig(JNIEnv* env, jclass clazz, jobject surf,
                                jint type, jint order, jint mode) {
    const sp<Surface>& sur(getSurface(env, surf));
    Parcel reply;
    int32_t data[]={type, order, mode};
    sendCommand(SET_SURF_CONFIG, sur, String8(), &reply, data, 3);
}

static void S3DView_setWindowConfig(JNIEnv* env, jclass clazz,
                                    jstring jname,
                                    jint type,
                                    jint order,
                                    jint mode) {
    Parcel reply;
    int32_t data[]={type, order, mode};

    const jchar* str = env->GetStringCritical(jname, 0);
    const String8 name(str, env->GetStringLength(jname));
    env->ReleaseStringCritical(jname, str);

    sendCommand(SET_WINDOW_CONFIG, NULL, name, &reply, data, 3);
}


static jobject S3DView_getPrefLayout(JNIEnv* env, jclass clazz) {
    jfieldID layoutID = lo.sbs_LR;
    Parcel reply;
    if(sendCommand(GET_PREF_LAYOUT, NULL, String8(), &reply, NULL, 0) == NO_ERROR) {
        int32_t layoutType = reply.readInt32();
        int32_t layoutOrder = reply.readInt32();
        if(layoutType == eTopBottom && layoutOrder == eLeftViewFirst)
            layoutID = lo.tb_L;
        else if(layoutType == eTopBottom && layoutOrder == eRightViewFirst)
            layoutID = lo.tb_R;
        else if(layoutType == eSideBySide && layoutOrder == eLeftViewFirst)
            layoutID = lo.sbs_LR;
        else if(layoutType == eSideBySide && layoutOrder == eRightViewFirst)
            layoutID = lo.sbs_RL;
    }
    jclass layout = env->FindClass("com/ti/s3d/S3DView$Layout");
    return env->GetStaticObjectField(layout, layoutID);
}

// ----------------------------------------------------------------------------
static void nativeClassInit(JNIEnv* env, jclass clazz);

static const JNINativeMethod gMethods[] = {
    { "nativeClassInit",     "()V", (void*)nativeClassInit },
    { "native_setConfig",   "(Landroid/view/Surface;III)V", (void*)S3DView_setConfig},
    { "native_getPrefLayout",   "()Lcom/ti/s3d/S3DView$Layout;", (void*)S3DView_getPrefLayout},
    { "native_setWindowConfig",   "(Ljava/lang/String;III)V", (void*)S3DView_setWindowConfig}
};

static void nativeClassInit(JNIEnv* env, jclass clazz) {
    const char* const kLayoutClassSignature = "Lcom/ti/s3d/S3DView$Layout;";

    //Obtain native surface offset field
    jclass surface = env->FindClass("android/view/Surface");
    so.surface = env->GetFieldID(surface, ANDROID_VIEW_SURFACE_JNI_ID, "I");

    jclass layout = env->FindClass("com/ti/s3d/S3DView$Layout");
    lo.mono = env->GetStaticFieldID(layout, "MONO", kLayoutClassSignature);
    lo.sbs_LR = env->GetStaticFieldID(layout, "SIDE_BY_SIDE_LR", kLayoutClassSignature);
    lo.sbs_RL = env->GetStaticFieldID(layout, "SIDE_BY_SIDE_RL", kLayoutClassSignature);
    lo.tb_L = env->GetStaticFieldID(layout, "TOPBOTTOM_L", kLayoutClassSignature);
    lo.tb_R = env->GetStaticFieldID(layout, "TOPBOTTOM_R", kLayoutClassSignature);
}

static int registerMethods(JNIEnv* env) {
    static const char* const kClassName = "com/ti/s3d/S3DView";

    jclass clazz;

    /* look up the class */
    clazz = env->FindClass(kClassName);
    if (clazz == NULL) {
        ALOGE("Can't find class %s\n", kClassName);
        return -1;
    }

    /* register all the methods */
    if (env->RegisterNatives(clazz, gMethods,
            sizeof(gMethods) / sizeof(gMethods[0])) != JNI_OK)
    {
        ALOGE("Failed registering methods for %s\n", kClassName);
        return -1;
    }

    return 0;
}

jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    JNIEnv* env = NULL;
    jint result = -1;

    if (vm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {
        ALOGE("ERROR: GetEnv failed\n");
        goto bail;
    }
    assert(env != NULL);

    if (registerMethods(env) != 0) {
        ALOGE("ERROR: S3DView native registration failed\n");
        goto bail;
    }

    /* success -- return valid version number */
    result = JNI_VERSION_1_4;

bail:
    return result;
}
