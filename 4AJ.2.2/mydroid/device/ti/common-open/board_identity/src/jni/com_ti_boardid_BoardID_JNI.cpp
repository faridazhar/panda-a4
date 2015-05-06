/*
 * Copyright (C) 2011 The Android Open Source Project
 * Copyright (C) 2010-2012 Texas Instruments, Inc. All rights reserved.
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

#define LOG_TAG "Board_ID"

#include "jni.h"
#include "JNIHelp.h"
#include "android_runtime/AndroidRuntime.h"

#include <utils/misc.h>
#include <utils/Log.h>
#include "../../include/omap4_board_identity.h"

#include <stdio.h>

namespace android
{
    static jstring getBoardID_sysfs(JNIEnv *env __attribute__ ((unused)),
                              jobject clazz __attribute__ ((unused)),
                              jint id_to_get)
    {
        int data_size = 0;
        char* data_ret;

        ALOGI("Getting the ID %i\n", id_to_get);
	data_ret = get_device_identity(id_to_get);
        jstring jdata_ret = env->NewStringUTF(data_ret);
	return jdata_ret;
    }
    static jstring getBoardID_prop(JNIEnv *env __attribute__ ((unused)),
                              jobject clazz __attribute__ ((unused)),
                              jint id_to_get)
    {
        int data_size = 0;
        char* data_ret;

        ALOGI("Getting the ID %i\n", id_to_get);
	data_ret = get_device_property(id_to_get);
        jstring jdata_ret = env->NewStringUTF(data_ret);
	return jdata_ret;
    }

    static jint setGov(JNIEnv *env __attribute__ ((unused)),
                              jobject clazz __attribute__ ((unused)),
                              jint gov_to_set)
    {
        int data_ret;

        ALOGI("Getting the ID %i\n", gov_to_set);
	data_ret = set_governor(gov_to_set);
	return data_ret;
    }

    static JNINativeMethod method_table[] = {
        { "getBoardIDsysfsNative", "(I)Ljava/lang/String;", (void*)getBoardID_sysfs },
        { "getBoardIDpropNative", "(I)Ljava/lang/String;", (void*)getBoardID_prop },
        { "setGovernorNative", "(I)I", (void*)setGov },
    };

    int register_Board_ID_Service(JNIEnv *env)
    {
         return jniRegisterNativeMethods(env, "board_id/com/ti/BoardIDService",
            method_table, NELEM(method_table));
    }

};

using namespace android;
extern "C" jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    JNIEnv* env = NULL;
    jint result = -1;

    if (vm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {
        ALOGE("GetEnv failed!");
        return result;
    }
    ALOG_ASSERT(env, "Could not retrieve the env!");

    register_Board_ID_Service(env);

    return JNI_VERSION_1_4;
}
