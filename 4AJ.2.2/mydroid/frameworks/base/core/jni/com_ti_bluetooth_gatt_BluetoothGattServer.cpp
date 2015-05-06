/*
** Copyright 2012 Texas Instruments Israel Ltd.
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#define DBUS_ADAPTER_IFACE BLUEZ_DBUS_BASE_IFC ".Adapter"
#define DBUS_DEVICE_IFACE BLUEZ_DBUS_BASE_IFC ".Device"
#define DBUS_INPUT_IFACE BLUEZ_DBUS_BASE_IFC ".Input"
#define DBUS_NETWORK_IFACE BLUEZ_DBUS_BASE_IFC ".Network"
#define DBUS_NETWORKSERVER_IFACE BLUEZ_DBUS_BASE_IFC ".NetworkServer"
#define DBUS_HEALTH_MANAGER_PATH "/org/bluez"
#define DBUS_HEALTH_MANAGER_IFACE BLUEZ_DBUS_BASE_IFC ".HealthManager"
#define DBUS_HEALTH_DEVICE_IFACE BLUEZ_DBUS_BASE_IFC ".HealthDevice"
#define DBUS_HEALTH_CHANNEL_IFACE BLUEZ_DBUS_BASE_IFC ".HealthChannel"
#define DBUS_GGSP_IFACE BLUEZ_DBUS_BASE_IFC ".ggsp"
#define DBUS_GGSP_AGENT_IFACE BLUEZ_DBUS_BASE_IFC ".ggsp_agent"

#define LOG_TAG "BluetoothGattServer.cpp"

#include "android_bluetooth_common.h"
#include "android_runtime/AndroidRuntime.h"
#include "android_util_Binder.h"
#include "JNIHelp.h"
#include "jni.h"
#include "utils/Log.h"
#include "utils/misc.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#ifdef HAVE_BLUETOOTH
#include <dbus/dbus.h>
#include <bluedroid/bluetooth.h>
#endif

#include <cutils/properties.h>
#ifndef LOGE
#define LOGV ALOGV
#define LOGD ALOGD
#define LOGE ALOGE
#define LOGW ALOGW
#define LOGI ALOGI
#endif


namespace android {

#define BLUETOOTH_CLASS_ERROR 0xFF000000
#define PROPERTIES_NREFS 10

#ifdef HAVE_BLUETOOTH
// We initialize these variables when we load class
// android.server.BluetoothService
static jfieldID field_mNativeData;
static jmethodID method_onAttRead;
static jmethodID method_onAttWrite;
static jmethodID method_onIndicateResult;
static jmethodID method_onTxPowerLevelRead;

typedef event_loop_native_data_t native_data_t;

static jboolean startEventLoopNative(JNIEnv *env, jobject object);
static void stopEventLoopNative(JNIEnv *env, jobject object);

/** Get native data stored in the opaque (Java code maintained) pointer mNativeData
 *  Perform quick sanity check, if there are any problems return NULL
 */
static inline native_data_t * get_native_data(JNIEnv *env, jobject object) {
    native_data_t *nat =
            (native_data_t *)(env->GetIntField(object, field_mNativeData));
    if (nat == NULL || nat->conn == NULL) {
        LOGE("Uninitialized native data\n");
        return NULL;
    }
    return nat;
}
#endif

static void classInitNative(JNIEnv* env, jclass clazz) {
    LOGV("%s", __FUNCTION__);
#ifdef BLUETI_ENHANCEMENT
#ifdef HAVE_BLUETOOTH
    field_mNativeData = get_field(env, clazz, "mNativeData", "I");
    method_onAttRead = env->GetMethodID(clazz, "onRead",
                               "(Ljava/lang/String;I)[B");
    method_onAttWrite = env->GetMethodID(clazz, "onWrite",
                               "(Ljava/lang/String;I[B)B");
    method_onIndicateResult = env->GetMethodID(clazz, "onIndicateResult",
                               "(II)V");
    method_onTxPowerLevelRead = env->GetMethodID(clazz, "onTxPowerLevelRead",
                               "(Ljava/lang/String;B)V");
#endif
#endif
}

/* Returns true on success (even if adapter is present but disabled).
 * Return false if dbus is down, or another serious error (out of memory)
*/
static bool initializeNativeDataNative(JNIEnv* env, jobject object) {
    LOGV("%s", __FUNCTION__);
#ifdef HAVE_BLUETOOTH
    native_data_t *nat = (native_data_t *)calloc(1, sizeof(native_data_t));
    if (NULL == nat) {
        LOGE("%s: out of memory!", __FUNCTION__);
        return false;
    }
    env->GetJavaVM( &(nat->vm) );
    nat->envVer = env->GetVersion();
    nat->me = env->NewGlobalRef(object);

    env->SetIntField(object, field_mNativeData, (jint)nat);

    pthread_mutex_init(&(nat->thread_mutex), NULL);

    DBusError err;
    dbus_error_init(&err);
    dbus_threads_init_default();
    /* get our own connection - for our mainloop */
    nat->conn = dbus_bus_get_private(DBUS_BUS_SYSTEM, &err);
    if (dbus_error_is_set(&err)) {
        LOGE("Could not get onto the system bus: %s", err.message);
        dbus_error_free(&err);
        return false;
    }
    dbus_connection_set_exit_on_disconnect(nat->conn, FALSE);
#endif  /*HAVE_BLUETOOTH*/
    return true;
}

// we leak the message here to get the reply
static const char *get_adapter_path(JNIEnv* env, jobject object) {
#ifdef HAVE_BLUETOOTH
    native_data_t *nat =
               (native_data_t *)env->GetIntField(object, field_mNativeData);
    if (nat == NULL)
        return NULL;

    DBusMessage *msg = NULL, *reply = NULL;
    DBusError err;
    const char *device_path = NULL;
    int attempt = 0;

    for (attempt = 0; attempt < 1000 && reply == NULL; attempt ++) {
        msg = dbus_message_new_method_call("org.bluez", "/",
              "org.bluez.Manager", "DefaultAdapter");
        if (!msg) {
            LOGE("%s: Can't allocate new method call for get_adapter_path!",
                  __FUNCTION__);
            return NULL;
        }
        dbus_message_append_args(msg, DBUS_TYPE_INVALID);
        dbus_error_init(&err);
        reply = dbus_connection_send_with_reply_and_block(nat->conn, msg, -1, &err);

        if (!reply) {
            if (dbus_error_is_set(&err)) {
                if (dbus_error_has_name(&err,
                    "org.freedesktop.DBus.Error.ServiceUnknown")) {
                    // bluetoothd is still down, retry
                    LOG_AND_FREE_DBUS_ERROR(&err);
                    usleep(10000);  // 10 ms
                    continue;
                } else {
                    // Some other error we weren't expecting
                    LOG_AND_FREE_DBUS_ERROR(&err);
                }
            }
            goto failed;
        }
    }
    if (attempt == 1000) {
        LOGE("Time out while trying to get Adapter path, is bluetoothd up ?");
        goto failed;
    }

    if (!dbus_message_get_args(reply, &err, DBUS_TYPE_OBJECT_PATH,
                               &device_path, DBUS_TYPE_INVALID)
                               || !device_path){
        if (dbus_error_is_set(&err)) {
            LOG_AND_FREE_DBUS_ERROR(&err);
        }
        goto failed;
    }
    dbus_message_unref(msg);
    return device_path;

failed:
    dbus_message_unref(msg);
    return NULL;
#else
    return NULL;
#endif
}

#ifdef HAVE_BLUETOOTH

#define EVENT_LOOP_REFS 10

DBusHandlerResult ggsp_event_filter(DBusConnection *conn,
                                     DBusMessage *msg, void *data) {
    native_data_t *nat = (native_data_t *)data;
    JNIEnv *env;

    if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_METHOD_CALL) {
        LOGV("%s: not interested (not a method call).", __FUNCTION__);
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    LOGI("%s: Received method %s:%s", __FUNCTION__,
         dbus_message_get_interface(msg), dbus_message_get_member(msg));

    if (nat == NULL) return DBUS_HANDLER_RESULT_HANDLED;

    nat->vm->GetEnv((void**)&env, nat->envVer);
    env->PushLocalFrame(EVENT_LOOP_REFS);

    if (dbus_message_is_method_call(msg,
            DBUS_GGSP_AGENT_IFACE, "ReadCB")) {
        char *dev_path;
        unsigned short handle;
        if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_OBJECT_PATH, &dev_path,
                                   DBUS_TYPE_UINT16, &handle,
                                   DBUS_TYPE_INVALID)) {
            LOGE("%s: invalid args for ReadCb message", __FUNCTION__);
            goto failure;
        }

        // reply
        DBusMessage *reply = dbus_message_new_method_return(msg);
        if (!reply) {
            LOGE("%s: Cannot create message reply\n", __FUNCTION__);
            goto failure;
        }

        jbyteArray read_res = (jbyteArray)env->CallObjectMethod(nat->me,
                   method_onAttRead, env->NewStringUTF(dev_path), (int)handle);

        unsigned char byte_status = 0x80;
        unsigned char *read_value = NULL;
        int read_len = 0, val_len = 0;
        jbyte *value = NULL;

        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
            goto send_read;
        }

        if (read_res == NULL)
            goto send_read;

        value = env->GetByteArrayElements(read_res, NULL);
        if (value == NULL)
            goto send_read;

        val_len = env->GetArrayLength(read_res);
        if (val_len < 1)
            goto send_read;

        byte_status = (unsigned char)value[0];
        read_value = (unsigned char *)&value[1];
        read_len = val_len - 1;

        if (read_len == 0)
            read_value = NULL;

send_read:
        LOGD("ReadCb: returning %d byte array with status %d",
             read_len, byte_status);

        dbus_message_append_args(reply, DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
                                 &read_value, read_len,
                                 DBUS_TYPE_BYTE, &byte_status,
                                 DBUS_TYPE_INVALID);
        dbus_connection_send(nat->conn, reply, NULL);
        dbus_message_unref(reply);
        if (value != NULL)
            env->ReleaseByteArrayElements(read_res, value, 0);
        goto success;

    } else if (dbus_message_is_method_call(msg,
            DBUS_GGSP_AGENT_IFACE, "WriteCB")) {
        char *dev_path;
        unsigned short handle;
        unsigned char *value;
        int val_len;
        if (!dbus_message_get_args(msg, NULL,
                                   DBUS_TYPE_OBJECT_PATH, &dev_path,
                                   DBUS_TYPE_UINT16, &handle,
                                   DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
                                   &value, &val_len,
                                   DBUS_TYPE_INVALID)) {
            LOGE("%s: Invalid arguments for WriteCb", __FUNCTION__);
            goto failure;
        }

        // reply
        DBusMessage *reply = dbus_message_new_method_return(msg);
        if (!reply) {
            LOGE("%s: Cannot create message reply\n", __FUNCTION__);
            goto failure;
        }

        LOGD("WriteCb: passing array of size %d", val_len);

        // this reference will be garbage collected by java
        jbyteArray valueArr = env->NewByteArray(val_len);
        if (val_len > 0)
            env->SetByteArrayRegion(valueArr, 0, val_len, (jbyte *)value);

        jbyte status = env->CallByteMethod(nat->me, method_onAttWrite,
                env->NewStringUTF(dev_path), (int)handle, valueArr);
        unsigned char byte_status = 0x80;

        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
            goto send_write;
        }

        byte_status = (unsigned char)status;

send_write:
        dbus_message_append_args(reply, DBUS_TYPE_BYTE, &byte_status,
                                 DBUS_TYPE_INVALID);
        dbus_connection_send(nat->conn, reply, NULL);
        dbus_message_unref(reply);
        goto success;
    } else if (dbus_message_is_method_call(msg,
            DBUS_GGSP_AGENT_IFACE, "IndicationResult")) {
        char *dev_path;
        int cookie;
        unsigned char byte_status;
        if (!dbus_message_get_args(msg, NULL,
                                   DBUS_TYPE_OBJECT_PATH, &dev_path,
                                   DBUS_TYPE_UINT32, &cookie,
                                   DBUS_TYPE_BYTE, &byte_status,
                                   DBUS_TYPE_INVALID)) {
            LOGE("%s: Invalid arguments", __FUNCTION__);
            goto failure;
        }

        int status = (int)byte_status;
        LOGD("%s: cookie %d status %d", __FUNCTION__, cookie, status);

        env->CallVoidMethod(nat->me, method_onIndicateResult,
                            (jint)cookie, (jint)status);
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }

        goto success;
    } else if (dbus_message_is_method_call(msg,
            DBUS_GGSP_AGENT_IFACE, "TxPower")) {
        char *dev_path;
        int cookie;
        unsigned char byte_level;
        if (!dbus_message_get_args(msg, NULL,
                                   DBUS_TYPE_OBJECT_PATH, &dev_path,
                                   DBUS_TYPE_BYTE, &byte_level,
                                   DBUS_TYPE_INVALID)) {
            LOGE("%s: Invalid arguments", __FUNCTION__);
            goto failure;
        }

        LOGD("%s: level %d", __FUNCTION__, (int)byte_level);

        env->CallVoidMethod(nat->me, method_onTxPowerLevelRead,
                            env->NewStringUTF(dev_path), (jbyte)byte_level);
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        goto success;
    }

    goto success;

failure:
    env->PopLocalFrame(NULL);
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

success:
    env->PopLocalFrame(NULL);
    return DBUS_HANDLER_RESULT_HANDLED;
}
#endif

#define GGSP_AGENT_PATH "/android/bluetooth/ggsp_agent"

static int register_ggsp_agent(native_data_t *nat, const char *adapter)
{
    DBusMessage *msg, *reply;
    DBusError err;
    const char *agent_path = GGSP_AGENT_PATH;

    static const DBusObjectPathVTable ggsp_agent_vtable = {
                 NULL, ggsp_event_filter, NULL, NULL, NULL, NULL };
    if (!dbus_connection_register_object_path(nat->conn, agent_path,
            &ggsp_agent_vtable, nat)) {
        LOGE("%s: Can't register object path %s for agent!",
              __FUNCTION__, agent_path);
        return -1;
    }

    msg = dbus_message_new_method_call(BLUEZ_DBUS_BASE_IFC, adapter,
          DBUS_GGSP_IFACE, "Register");
    if (!msg) {
        LOGE("%s: Can't allocate new method call for ggsp agent!",
              __FUNCTION__);
        return -1;
    }
    dbus_message_append_args(msg,
                             DBUS_TYPE_OBJECT_PATH, &agent_path,
                             DBUS_TYPE_INVALID);

    dbus_error_init(&err);
    reply = dbus_connection_send_with_reply_and_block(nat->conn, msg, -1, &err);
    dbus_message_unref(msg);

    if (!reply) {
        LOGE("%s: Can't register agent!", __FUNCTION__);
        if (dbus_error_is_set(&err)) {
            LOG_AND_FREE_DBUS_ERROR(&err);
        }
        return -1;
    }

    dbus_message_unref(reply);
    dbus_connection_flush(nat->conn);

    return 0;
}


// This function is called when the adapter is enabled.
static jboolean setupNativeDataNative(JNIEnv* env, jobject object) {
    LOGV("%s", __FUNCTION__);
#ifdef HAVE_BLUETOOTH

    native_data_t *nat =
               (native_data_t *)env->GetIntField(object, field_mNativeData);
    if (nat == NULL)
    return JNI_FALSE;

    startEventLoopNative(env, object);
    // Register agent for GGSP callbacks
    nat->adapter = get_adapter_path(env, object);
    if (nat->adapter == NULL)
        return JNI_FALSE;
    if (register_ggsp_agent(nat, nat->adapter) != 0)
        return JNI_FALSE;

    LOGD("%s: adapter path %s", __FUNCTION__, nat->adapter);
    return JNI_TRUE;
#endif /*HAVE_BLUETOOTH*/
    return JNI_FALSE;
}

static jboolean tearDownNativeDataNative(JNIEnv *env, jobject object) {
    LOGV("%s", __FUNCTION__);
#ifdef HAVE_BLUETOOTH
    native_data_t *nat =
               (native_data_t *)env->GetIntField(object, field_mNativeData);
    if (nat != NULL) {
        dbus_connection_unregister_object_path (nat->conn, GGSP_AGENT_PATH);
    stopEventLoopNative(env, object);
    }

#endif /*HAVE_BLUETOOTH*/
    return JNI_TRUE;
}

static void cleanupNativeDataNative(JNIEnv* env, jobject object) {
    LOGV("%s", __FUNCTION__);
#ifdef HAVE_BLUETOOTH
    native_data_t *nat =
        (native_data_t *)env->GetIntField(object, field_mNativeData);
    env->DeleteGlobalRef(nat->me);
    pthread_mutex_destroy(&(nat->thread_mutex));
    dbus_connection_unref(nat->conn);
    free(nat);
    nat = NULL;
#endif
}

static jint findAvailableNative(JNIEnv *env, jobject object,
                                jbyteArray uuid, jint nitems) {
#ifdef HAVE_BLUETOOTH
    LOGV("%s", __FUNCTION__);
    native_data_t *nat = get_native_data(env, object);
    if (!nat)
        return 0;

    DBusMessage *msg = NULL, *reply = NULL;
    DBusError err;
    unsigned short start_handle = 0;
    dbus_error_init(&err);

    msg = dbus_message_new_method_call(BLUEZ_DBUS_BASE_IFC,
                                       nat->adapter,
                                       DBUS_GGSP_IFACE,
                                       "FindAvailable");
    if (!msg) {
        LOGE("%s: Can't allocate new method call!", __FUNCTION__);
        return 0;
    }

    jbyte *uuid_ptr = env->GetByteArrayElements(uuid, NULL);
    int uuid_len = env->GetArrayLength(uuid);

    short short_items = (short)nitems;

    dbus_message_append_args(msg,
                             DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
                             &uuid_ptr, uuid_len,
                             DBUS_TYPE_UINT16, &short_items,
                             DBUS_TYPE_INVALID);

    reply = dbus_connection_send_with_reply_and_block(nat->conn, msg, -1, &err);
    if (!reply) {
        if (dbus_error_is_set(&err)) {
            LOG_AND_FREE_DBUS_ERROR(&err);
        }
    } else {
        if (!dbus_message_get_args(reply, &err,
                              DBUS_TYPE_UINT16, &start_handle,
                              DBUS_TYPE_INVALID)) {
            if (dbus_error_is_set(&err)) {
                LOG_AND_FREE_DBUS_ERROR(&err);
            }
            start_handle = 0;
        }
    }

done:
    env->ReleaseByteArrayElements(uuid, uuid_ptr, 0);

    if (msg) dbus_message_unref(msg);
    if (reply) dbus_message_unref(reply);
#endif
    return (int)start_handle;
}

static jboolean addAttributeNative(JNIEnv *env, jobject object,
                                   jint handle, jbyteArray uuid,
                                   jint read_reqs, jint write_reqs,
                                   jbyteArray value, jboolean registerCb) {
#ifdef HAVE_BLUETOOTH
    LOGV("%s", __FUNCTION__);
    jboolean ret = JNI_FALSE;
    native_data_t *nat = get_native_data(env, object);
    if (!nat)
        return JNI_FALSE;

    DBusMessage *msg = NULL, *reply = NULL;
    DBusError err;
    dbus_error_init(&err);

    msg = dbus_message_new_method_call(BLUEZ_DBUS_BASE_IFC,
                                       nat->adapter,
                                       DBUS_GGSP_IFACE,
                                       "AddAttribute");
    if (!msg) {
        LOGE("%s: Can't allocate new method call!", __FUNCTION__);
        return JNI_FALSE;
    }

    jbyte *uuid_ptr = env->GetByteArrayElements(uuid, NULL);
    int uuid_len = env->GetArrayLength(uuid);
    jbyte *val_ptr = env->GetByteArrayElements(value, NULL);
    int val_len = env->GetArrayLength(value);
    short short_handle = (short)handle; // for 0xffff
    dbus_bool_t dbus_registerCb = (dbus_bool_t)registerCb;

    dbus_message_append_args(msg,
                             DBUS_TYPE_UINT16, &short_handle,
                             DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
                             &uuid_ptr, uuid_len,
                             DBUS_TYPE_UINT32, &read_reqs,
                             DBUS_TYPE_UINT32, &write_reqs,
                             DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
                             &val_ptr, val_len,
                             DBUS_TYPE_BOOLEAN, &dbus_registerCb,
                             DBUS_TYPE_INVALID);

    reply = dbus_connection_send_with_reply_and_block(nat->conn, msg, -1, &err);
    if (!reply) {
        if (dbus_error_is_set(&err)) {
            LOG_AND_FREE_DBUS_ERROR(&err);
        }
        ret = JNI_FALSE;
    }

    ret = JNI_TRUE;
done:
    env->ReleaseByteArrayElements(uuid, uuid_ptr, 0);
    env->ReleaseByteArrayElements(value, val_ptr, 0);

    if (msg) dbus_message_unref(msg);
    if (reply) dbus_message_unref(reply);
    return ret;
#else
    return JNI_FALSE;
#endif
}

static jboolean removeAttributeNative(JNIEnv *env, jobject object, jint handle) {
#ifdef HAVE_BLUETOOTH
    LOGV("%s", __FUNCTION__);
    jboolean ret = JNI_FALSE;
    native_data_t *nat = get_native_data(env, object);
    if (!nat)
        return JNI_FALSE;

    DBusMessage *msg = NULL, *reply = NULL;
    DBusError err;
    dbus_error_init(&err);

    msg = dbus_message_new_method_call(BLUEZ_DBUS_BASE_IFC,
                                       nat->adapter,
                                       DBUS_GGSP_IFACE,
                                       "RemoveAttribute");
    if (!msg) {
        LOGE("%s: Can't allocate new method call!", __FUNCTION__);
        return JNI_FALSE;
    }

    unsigned short short_handle = (unsigned short)handle;
    dbus_message_append_args(msg,
                             DBUS_TYPE_UINT16, &short_handle,
                             DBUS_TYPE_INVALID);

    reply = dbus_connection_send_with_reply_and_block(nat->conn, msg, -1, &err);
    if (!reply) {
        if (dbus_error_is_set(&err)) {
            LOG_AND_FREE_DBUS_ERROR(&err);
        }
        ret = JNI_FALSE;
    }

    ret = JNI_TRUE;
done:
    if (msg) dbus_message_unref(msg);
    if (reply) dbus_message_unref(reply);
    return ret;
#else
    return JNI_FALSE;
#endif
}

static jboolean sendNotificationNative(JNIEnv *env, jobject object,
                                   jstring address, jint handle,
                                   jint handle_ccc, jbyteArray value) {
#ifdef HAVE_BLUETOOTH
    LOGV("%s", __FUNCTION__);
    jboolean ret = JNI_FALSE;
    native_data_t *nat = get_native_data(env, object);
    if (!nat)
        return JNI_FALSE;

    DBusMessage *msg = NULL;

    msg = dbus_message_new_method_call(BLUEZ_DBUS_BASE_IFC,
                                       nat->adapter,
                                       DBUS_GGSP_IFACE,
                                       "SendNotification");
    if (!msg) {
        LOGE("%s: Can't allocate new method call!", __FUNCTION__);
        return JNI_FALSE;
    }

    const char *c_address = env->GetStringUTFChars(address, NULL);
    jbyte *val_ptr = env->GetByteArrayElements(value, NULL);
    int val_len = env->GetArrayLength(value);
    short short_handle = (short)handle; // cast from java int for unsigned
    short short_handle_ccc = (short)handle_ccc; // same here

    dbus_message_append_args(msg,
                             DBUS_TYPE_STRING, &c_address,
                             DBUS_TYPE_UINT16, &short_handle,
                             DBUS_TYPE_UINT16, &short_handle_ccc,
                             DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
                             &val_ptr, val_len,
                             DBUS_TYPE_INVALID);

    // send the message asynchronously - we don't care about the reply
    if (!dbus_connection_send(nat->conn, msg, NULL))
        ret = JNI_FALSE;

    ret = JNI_TRUE;
done:
    env->ReleaseStringUTFChars(address, c_address);
    env->ReleaseByteArrayElements(value, val_ptr, 0);

    if (msg) dbus_message_unref(msg);
    return ret;
#else
    return JNI_FALSE;
#endif
}

// note - this is not the indication status. This is just reply from the
// sendIndicationNative, which we got asynchronously.
static void onSendIndicationReply(DBusMessage *msg, void *user, void *n) {
#ifdef HAVE_BLUETOOTH
    LOGV("%s", __FUNCTION__);
    native_data_t *nat = (native_data_t *)n;
    DBusError err;
    dbus_error_init(&err);
    JNIEnv *env;
    jstring addr;
    int cookie = *(int *)user;

    nat->vm->GetEnv((void**)&env, nat->envVer);

    LOGV("... cookie = %d", cookie);

    jint status = 0; // ATT success
    int num_sent = 0;
    if (dbus_set_error_from_message(&err, msg)) {
        LOGD("%s: error ret from dbus", __FUNCTION__);
        status = 0x80; // ATT_IO error
    } else if (!dbus_message_get_args(msg, &err,
                              DBUS_TYPE_UINT32, &num_sent,
                              DBUS_TYPE_INVALID)) {
        if (dbus_error_is_set(&err)) {
                LOG_AND_FREE_DBUS_ERROR(&err);
        }
        LOGD("%s: could not get args", __FUNCTION__);
        status = 0x80;
    }

    if (num_sent == 0) {
        LOGD("%s: nothing sent (probably CCC not configured)", __FUNCTION__);
        status = 0x80;
    }

    // call the indication result on failure
    if (status != 0) {
        env->CallVoidMethod(nat->me,
                            method_onIndicateResult,
                            (jint)cookie,
                            status);
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
    }

    if (dbus_error_is_set(&err)) {
        LOG_AND_FREE_DBUS_ERROR(&err);
    }
    free(user);
#endif
}

static jboolean sendIndicationNative(JNIEnv *env, jobject object,
                                   jstring address, jint handle,
                                   jint handle_ccc, jbyteArray value,
                                   jint cookie) {
#ifdef HAVE_BLUETOOTH
    LOGV("%s", __FUNCTION__);
    native_data_t *nat = get_native_data(env, object);
    if (!nat)
        return JNI_FALSE;

    int *data = (int *) malloc(sizeof(int));
    if (data == NULL)
        return JNI_FALSE;

    const char *c_address = env->GetStringUTFChars(address, NULL);
    jbyte *val_ptr = env->GetByteArrayElements(value, NULL);
    int val_len = env->GetArrayLength(value);
    short short_handle = (short)handle; // cast from java int for unsigned
    short short_handle_ccc = (short)handle_ccc; // same here

    *data = (int)cookie;
    bool ret = dbus_func_args_async(env, nat->conn, -1, onSendIndicationReply,
                                    data, nat, nat->adapter,
                                    DBUS_GGSP_IFACE, "SendIndication",
                                    DBUS_TYPE_STRING, &c_address,
                                    DBUS_TYPE_UINT16, &short_handle,
                                    DBUS_TYPE_UINT16, &short_handle_ccc,
                                    DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
                                    &val_ptr, val_len,
                                    DBUS_TYPE_UINT32, &cookie,
                                    DBUS_TYPE_INVALID);

    env->ReleaseStringUTFChars(address, c_address);
    env->ReleaseByteArrayElements(value, val_ptr, 0);

    return ret;
#else
    return JNI_FALSE;
#endif
}

static jint registerSdpRecordNative(JNIEnv *env, jobject object,
                                   jint handle, jstring name) {
#ifdef HAVE_BLUETOOTH
    LOGV("%s", __FUNCTION__);
    native_data_t *nat = get_native_data(env, object);
    if (!nat)
        return 0;

    DBusMessage *msg = NULL, *reply = NULL;
    int sdp_handle = 0;
    DBusError err;
    dbus_error_init(&err);

    msg = dbus_message_new_method_call(BLUEZ_DBUS_BASE_IFC,
                                       nat->adapter,
                                       DBUS_GGSP_IFACE,
                                       "RegisterSDPRecord");
    if (!msg) {
        LOGE("%s: Can't allocate new method call!", __FUNCTION__);
        return 0;
    }

    const char *c_name = env->GetStringUTFChars(name, NULL);

    unsigned short short_handle = (unsigned short)handle;
    dbus_message_append_args(msg,
                             DBUS_TYPE_UINT16, &short_handle,
                             DBUS_TYPE_STRING, &c_name,
                             DBUS_TYPE_INVALID);

    reply = dbus_connection_send_with_reply_and_block(nat->conn, msg, -1, &err);
    if (!reply) {
        if (dbus_error_is_set(&err)) {
            LOG_AND_FREE_DBUS_ERROR(&err);
        }
    } else {
        if (!dbus_message_get_args(reply, &err,
                              DBUS_TYPE_UINT32, &sdp_handle,
                              DBUS_TYPE_INVALID)) {
            if (dbus_error_is_set(&err)) {
                LOG_AND_FREE_DBUS_ERROR(&err);
            }
            sdp_handle = 0;
        }
    }

done:
    env->ReleaseStringUTFChars(name, c_name);

    if (msg) dbus_message_unref(msg);
    if (reply) dbus_message_unref(reply);
    return sdp_handle;
#else
    return 0;
#endif
}

static jboolean unregisterSdpRecordNative(JNIEnv *env, jobject object, jint sdp_handle) {
#ifdef HAVE_BLUETOOTH
    LOGV("%s", __FUNCTION__);
    jboolean ret = JNI_FALSE;
    native_data_t *nat = get_native_data(env, object);
    if (!nat)
        return JNI_FALSE;

    DBusMessage *msg = NULL, *reply = NULL;
    DBusError err;
    dbus_error_init(&err);

    msg = dbus_message_new_method_call(BLUEZ_DBUS_BASE_IFC,
                                       nat->adapter,
                                       DBUS_GGSP_IFACE,
                                       "UnregisterSDPRecord");
    if (!msg) {
        LOGE("%s: Can't allocate new method call!", __FUNCTION__);
        return JNI_FALSE;
    }

    dbus_message_append_args(msg,
                             DBUS_TYPE_UINT32, &sdp_handle,
                             DBUS_TYPE_INVALID);

    reply = dbus_connection_send_with_reply_and_block(nat->conn, msg, -1, &err);
    if (!reply) {
        if (dbus_error_is_set(&err)) {
            LOG_AND_FREE_DBUS_ERROR(&err);
        }
        ret = JNI_FALSE;
    }

    ret = JNI_TRUE;
done:
    if (msg) dbus_message_unref(msg);
    if (reply) dbus_message_unref(reply);
    return ret;
#else
    return JNI_FALSE;
#endif
}

static jboolean invalidateAttCacheNative(JNIEnv *env, jobject object) {
#ifdef HAVE_BLUETOOTH
    LOGV("%s", __FUNCTION__);
    jboolean ret = JNI_FALSE;
    native_data_t *nat = get_native_data(env, object);
    if (!nat)
        return JNI_FALSE;

    DBusMessage *msg = NULL, *reply = NULL;
    DBusError err;
    dbus_error_init(&err);

    msg = dbus_message_new_method_call(BLUEZ_DBUS_BASE_IFC,
                                       nat->adapter,
                                       DBUS_GGSP_IFACE,
                                       "InvalidateCache");
    if (!msg) {
        LOGE("%s: Can't allocate new method call!", __FUNCTION__);
        return JNI_FALSE;
    }

    reply = dbus_connection_send_with_reply_and_block(nat->conn, msg, -1, &err);
    if (!reply) {
        if (dbus_error_is_set(&err)) {
            LOG_AND_FREE_DBUS_ERROR(&err);
        }
        ret = JNI_FALSE;
    }

    ret = JNI_TRUE;
done:
    if (msg) dbus_message_unref(msg);
    if (reply) dbus_message_unref(reply);
    return ret;
#else
    return JNI_FALSE;
#endif
}

#ifdef HAVE_BLUETOOTH
static unsigned int unix_events_to_dbus_flags(short events) {
    return (events & DBUS_WATCH_READABLE ? POLLIN : 0) |
           (events & DBUS_WATCH_WRITABLE ? POLLOUT : 0) |
           (events & DBUS_WATCH_ERROR ? POLLERR : 0) |
           (events & DBUS_WATCH_HANGUP ? POLLHUP : 0);
}

static short dbus_flags_to_unix_events(unsigned int flags) {
    return (flags & POLLIN ? DBUS_WATCH_READABLE : 0) |
           (flags & POLLOUT ? DBUS_WATCH_WRITABLE : 0) |
           (flags & POLLERR ? DBUS_WATCH_ERROR : 0) |
           (flags & POLLHUP ? DBUS_WATCH_HANGUP : 0);
}

static jboolean setUpEventLoop(native_data_t *nat) {
    LOGV("%s", __FUNCTION__);

    if (nat != NULL && nat->conn != NULL) {
        dbus_threads_init_default();
        DBusError err;
        dbus_error_init(&err);


        // Set which messages will be processed by this dbus connection
        dbus_bus_add_match(nat->conn,
                "type='signal',interface='org.freedesktop.DBus'",
                &err);
        if (dbus_error_is_set(&err)) {
            LOG_AND_FREE_DBUS_ERROR(&err);
            return JNI_FALSE;
        }

        return JNI_TRUE;
    }
    return JNI_FALSE;
}

static void tearDownEventLoop(native_data_t *nat) {
    LOGV("%s", __FUNCTION__);
    if (nat != NULL && nat->conn != NULL) {
    dbus_connection_flush(nat->conn);
    }
}

#define EVENT_LOOP_EXIT 1
#define EVENT_LOOP_ADD  2
#define EVENT_LOOP_REMOVE 3
#define EVENT_LOOP_WAKEUP 4

static dbus_bool_t dbusAddWatch(DBusWatch *watch, void *data) {
    native_data_t *nat = (native_data_t *)data;

    if (dbus_watch_get_enabled(watch)) {
        // note that we can't just send the watch and inspect it later
        // because we may get a removeWatch call before this data is reacted
        // to by our eventloop and remove this watch..  reading the add first
        // and then inspecting the recently deceased watch would be bad.
        char control = EVENT_LOOP_ADD;
        write(nat->controlFdW, &control, sizeof(char));

        int fd = dbus_watch_get_fd(watch);
        write(nat->controlFdW, &fd, sizeof(int));

        unsigned int flags = dbus_watch_get_flags(watch);
        write(nat->controlFdW, &flags, sizeof(unsigned int));

        write(nat->controlFdW, &watch, sizeof(DBusWatch*));
    }
    return true;
}

static void dbusRemoveWatch(DBusWatch *watch, void *data) {
    native_data_t *nat = (native_data_t *)data;

    char control = EVENT_LOOP_REMOVE;
    write(nat->controlFdW, &control, sizeof(char));

    int fd = dbus_watch_get_fd(watch);
    write(nat->controlFdW, &fd, sizeof(int));

    unsigned int flags = dbus_watch_get_flags(watch);
    write(nat->controlFdW, &flags, sizeof(unsigned int));
}

static void dbusToggleWatch(DBusWatch *watch, void *data) {
    if (dbus_watch_get_enabled(watch)) {
        dbusAddWatch(watch, data);
    } else {
        dbusRemoveWatch(watch, data);
    }
}

static void dbusWakeup(void *data) {
    native_data_t *nat = (native_data_t *)data;

    char control = EVENT_LOOP_WAKEUP;
    write(nat->controlFdW, &control, sizeof(char));
}

static void handleWatchAdd(native_data_t *nat) {
    DBusWatch *watch;
    int newFD;
    unsigned int flags;

    read(nat->controlFdR, &newFD, sizeof(int));
    read(nat->controlFdR, &flags, sizeof(unsigned int));
    read(nat->controlFdR, &watch, sizeof(DBusWatch *));
    short events = dbus_flags_to_unix_events(flags);

    for (int y = 0; y<nat->pollMemberCount; y++) {
        if ((nat->pollData[y].fd == newFD) &&
                (nat->pollData[y].events == events)) {
            LOGV("DBusWatch duplicate add");
            return;
        }
    }
    if (nat->pollMemberCount == nat->pollDataSize) {
        LOGV("Bluetooth EventLoop poll struct growing");
        struct pollfd *temp = (struct pollfd *)malloc(
                sizeof(struct pollfd) * (nat->pollMemberCount+1));
        if (!temp) {
            return;
        }
        memcpy(temp, nat->pollData, sizeof(struct pollfd) *
                nat->pollMemberCount);
        free(nat->pollData);
        nat->pollData = temp;
        DBusWatch **temp2 = (DBusWatch **)malloc(sizeof(DBusWatch *) *
                (nat->pollMemberCount+1));
        if (!temp2) {
            return;
        }
        memcpy(temp2, nat->watchData, sizeof(DBusWatch *) *
                nat->pollMemberCount);
        free(nat->watchData);
        nat->watchData = temp2;
        nat->pollDataSize++;
    }
    nat->pollData[nat->pollMemberCount].fd = newFD;
    nat->pollData[nat->pollMemberCount].revents = 0;
    nat->pollData[nat->pollMemberCount].events = events;
    nat->watchData[nat->pollMemberCount] = watch;
    nat->pollMemberCount++;
}

static void handleWatchRemove(native_data_t *nat) {
    int removeFD;
    unsigned int flags;

    read(nat->controlFdR, &removeFD, sizeof(int));
    read(nat->controlFdR, &flags, sizeof(unsigned int));
    short events = dbus_flags_to_unix_events(flags);

    for (int y = 0; y < nat->pollMemberCount; y++) {
        if ((nat->pollData[y].fd == removeFD) &&
                (nat->pollData[y].events == events)) {
            int newCount = --nat->pollMemberCount;
            // copy the last live member over this one
            nat->pollData[y].fd = nat->pollData[newCount].fd;
            nat->pollData[y].events = nat->pollData[newCount].events;
            nat->pollData[y].revents = nat->pollData[newCount].revents;
            nat->watchData[y] = nat->watchData[newCount];
            return;
        }
    }
    LOGW("WatchRemove given with unknown watch");
}

static void *eventLoopMain(void *ptr) {
    native_data_t *nat = (native_data_t *)ptr;
    JNIEnv *env;

    JavaVMAttachArgs args;
    char name[] = "BT EventLoop";
    args.version = nat->envVer;
    args.name = name;
    args.group = NULL;

    nat->vm->AttachCurrentThread(&env, &args);

    dbus_connection_set_watch_functions(nat->conn, dbusAddWatch,
            dbusRemoveWatch, dbusToggleWatch, ptr, NULL);
    dbus_connection_set_wakeup_main_function(nat->conn, dbusWakeup, ptr, NULL);

    nat->running = true;

    while (1) {
        for (int i = 0; i < nat->pollMemberCount; i++) {
            if (!nat->pollData[i].revents) {
                continue;
            }
            if (nat->pollData[i].fd == nat->controlFdR) {
                char data;
                while (recv(nat->controlFdR, &data, sizeof(char), MSG_DONTWAIT)
                        != -1) {
                    switch (data) {
                    case EVENT_LOOP_EXIT:
                    {
                        dbus_connection_set_watch_functions(nat->conn,
                                NULL, NULL, NULL, NULL, NULL);
                        tearDownEventLoop(nat);
                        nat->vm->DetachCurrentThread();

                        int fd = nat->controlFdR;
                        nat->controlFdR = 0;
                        close(fd);
                        return NULL;
                    }
                    case EVENT_LOOP_ADD:
                    {
                        handleWatchAdd(nat);
                        break;
                    }
                    case EVENT_LOOP_REMOVE:
                    {
                        handleWatchRemove(nat);
                        break;
                    }
                    case EVENT_LOOP_WAKEUP:
                    {
                        // noop
                        break;
                    }
                    }
                }
            } else {
                short events = nat->pollData[i].revents;
                unsigned int flags = unix_events_to_dbus_flags(events);
                dbus_watch_handle(nat->watchData[i], flags);
                nat->pollData[i].revents = 0;
                // can only do one - it may have caused a 'remove'
                break;
            }
        }
        while (dbus_connection_dispatch(nat->conn) ==
                DBUS_DISPATCH_DATA_REMAINS) {
        }

        poll(nat->pollData, nat->pollMemberCount, -1);
    }
}
#endif // HAVE_BLUETOOTH

static jboolean startEventLoopNative(JNIEnv *env, jobject object) {
    jboolean result = JNI_FALSE;
#ifdef HAVE_BLUETOOTH
    event_loop_native_data_t *nat = get_native_data(env, object);

    pthread_mutex_lock(&(nat->thread_mutex));

    nat->running = false;

    if (nat->pollData) {
        LOGW("trying to start EventLoop a second time!");
        pthread_mutex_unlock( &(nat->thread_mutex) );
        return JNI_FALSE;
    }

    nat->pollData = (struct pollfd *)malloc(sizeof(struct pollfd) *
            DEFAULT_INITIAL_POLLFD_COUNT);
    if (!nat->pollData) {
        LOGE("out of memory error starting EventLoop!");
        goto done;
    }

    nat->watchData = (DBusWatch **)malloc(sizeof(DBusWatch *) *
            DEFAULT_INITIAL_POLLFD_COUNT);
    if (!nat->watchData) {
        LOGE("out of memory error starting EventLoop!");
        goto done;
    }

    memset(nat->pollData, 0, sizeof(struct pollfd) *
            DEFAULT_INITIAL_POLLFD_COUNT);
    memset(nat->watchData, 0, sizeof(DBusWatch *) *
            DEFAULT_INITIAL_POLLFD_COUNT);
    nat->pollDataSize = DEFAULT_INITIAL_POLLFD_COUNT;
    nat->pollMemberCount = 1;

    if (socketpair(AF_LOCAL, SOCK_STREAM, 0, &(nat->controlFdR))) {
        LOGE("Error getting BT control socket");
        goto done;
    }
    nat->pollData[0].fd = nat->controlFdR;
    nat->pollData[0].events = POLLIN;

    if (setUpEventLoop(nat) != JNI_TRUE) {
        LOGE("failure setting up Event Loop!");
        goto done;
    }

    pthread_create(&(nat->thread), NULL, eventLoopMain, nat);
    result = JNI_TRUE;

done:
    if (JNI_FALSE == result) {
        if (nat->controlFdW) {
            close(nat->controlFdW);
            nat->controlFdW = 0;
        }
        if (nat->controlFdR) {
            close(nat->controlFdR);
            nat->controlFdR = 0;
        }
        if (nat->pollData) free(nat->pollData);
        nat->pollData = NULL;
        if (nat->watchData) free(nat->watchData);
        nat->watchData = NULL;
        nat->pollDataSize = 0;
        nat->pollMemberCount = 0;
    }

    pthread_mutex_unlock(&(nat->thread_mutex));
#endif // HAVE_BLUETOOTH
    return result;
}

static void stopEventLoopNative(JNIEnv *env, jobject object) {
#ifdef HAVE_BLUETOOTH
    native_data_t *nat = get_native_data(env, object);

    pthread_mutex_lock(&(nat->thread_mutex));
    if (nat->pollData) {
        char data = EVENT_LOOP_EXIT;
        ssize_t t = write(nat->controlFdW, &data, sizeof(char));
        void *ret;
        pthread_join(nat->thread, &ret);

        free(nat->pollData);
        nat->pollData = NULL;
        free(nat->watchData);
        nat->watchData = NULL;
        nat->pollDataSize = 0;
        nat->pollMemberCount = 0;

        int fd = nat->controlFdW;
        nat->controlFdW = 0;
        close(fd);
    }
    nat->running = false;
    pthread_mutex_unlock(&(nat->thread_mutex));
#endif // HAVE_BLUETOOTH
}

static jboolean isEventLoopRunningNative(JNIEnv *env, jobject object) {
    jboolean result = JNI_FALSE;
#ifdef HAVE_BLUETOOTH
    native_data_t *nat = get_native_data(env, object);

    pthread_mutex_lock(&(nat->thread_mutex));
    if (nat->running) {
        result = JNI_TRUE;
    }
    pthread_mutex_unlock(&(nat->thread_mutex));

#endif // HAVE_BLUETOOTH
    return result;
}

static JNINativeMethod sMethods[] = {
     /* name, signature, funcPtr */
    {"classInitNative", "()V", (void*)classInitNative},
    {"initializeNativeDataNative", "()V", (void *)initializeNativeDataNative},
    {"setupNativeDataNative", "()Z", (void *)setupNativeDataNative},
    {"tearDownNativeDataNative", "()Z", (void *)tearDownNativeDataNative},
    {"cleanupNativeDataNative", "()V", (void *)cleanupNativeDataNative},

    {"findAvailableNative", "([BI)I", (void *)findAvailableNative},
    {"addAttributeNative", "(I[BII[BZ)Z", (void *)addAttributeNative},
    {"removeAttributeNative", "(I)Z", (void *)removeAttributeNative},
    {"sendNotificationNative", "(Ljava/lang/String;II[B)Z", (void *)sendNotificationNative},
    {"sendIndicationNative", "(Ljava/lang/String;II[BI)Z", (void *)sendIndicationNative},
    {"invalidateAttCacheNative", "()Z", (void *)invalidateAttCacheNative},
    {"registerSdpRecordNative", "(ILjava/lang/String;)I", (void *)registerSdpRecordNative},
    {"unregisterSdpRecordNative", "(I)Z", (void *)unregisterSdpRecordNative},
};


int register_com_ti_bluetooth_BluetoothGattServer(JNIEnv *env) {
    return AndroidRuntime::registerNativeMethods(env,
                "android/server/BluetoothGattServerService", sMethods, NELEM(sMethods));
}

} /* namespace android */
