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
#define DBUS_CHAR_IFACE BLUEZ_DBUS_BASE_IFC ".Characteristic"

#define LOG_TAG "BluetoothGattClient.cpp"

#include "android_bluetooth_common.h"
#include "android_runtime/AndroidRuntime.h"
#include "JNIHelp.h"
#include "utils/Log.h"

#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#ifdef HAVE_BLUETOOTH
#include <bluetooth/bluetooth.h>
#endif

#ifndef LOGE
#define LOGV ALOGV
#define LOGD ALOGD
#define LOGE ALOGE
#define LOGW ALOGW
#define LOGI ALOGI
#endif

namespace android {
#ifdef HAVE_BLUETOOTH

static jmethodID method_onValueChanged;
static jmethodID method_WriteRequestComplete;
static jmethodID method_ReadComplete;
static jmethodID method_onCharacteristicPropertyChanged;

static jfieldID field_mNativeData;

DBusHandlerResult watcher_event_filter(DBusConnection *conn,DBusMessage *msg, void *data);

typedef struct {
    JavaVM *vm;
    int envVer;
    DBusConnection *conn;
    jobject me;  // for callbacks to java
    char adapter_path[256];
} native_data_t;

native_data_t *gnat = 0;

static inline native_data_t * get_native_data(JNIEnv *env, jobject object) {
    return (native_data_t *)(env->GetIntField(object,
                                                 field_mNativeData));
}

native_data_t *get_GattClientService_native_data(JNIEnv *env, jobject object) {
    return get_native_data(env, object);
}

static const DBusObjectPathVTable gattclient_vtable = {
    NULL, watcher_event_filter, NULL, NULL, NULL, NULL
};

#define GATT_CLIENT_WATCHER_PATH "/com/ti/bluetooth/gatt/client"
#define BLUEZ_WATCHER_IFACE "org.bluez.Watcher"
#define GATT_CHAR_VALUE_CHANGED_METHOD "ValueChanged"
#define GATT_CHAR_WRITE_REQ_RESPONSE_METHOD "WriteResponse"
#define GATT_CHAR_READ_REQ_RESPONSE_METHOD "ReadResponse"

#define EVENT_LOOP_REFS 10
#define PROPERTIES_NREFS 18

static Properties service_properties[] = {
        {"Characteristics", DBUS_TYPE_ARRAY},
        {"UUID", DBUS_TYPE_STRING},
      };

static Properties characteristic_properties[] = {
        {"UUID", DBUS_TYPE_STRING},
        {"Name", DBUS_TYPE_STRING},
        {"Description", DBUS_TYPE_STRING},
        {"Broadcast", DBUS_TYPE_BOOLEAN},
        {"Indicate", DBUS_TYPE_BOOLEAN},
        {"Notify", DBUS_TYPE_BOOLEAN},
        {"Readable", DBUS_TYPE_BOOLEAN},
        {"WriteMethods",DBUS_TYPE_ARRAY},
        {"Value",DBUS_TYPE_ARRAY},
      };

jobjectArray parse_remote_service_properties(JNIEnv *env, DBusMessageIter *iter) {
    return parse_properties(env, iter, (Properties *) &service_properties,
                          sizeof(service_properties) / sizeof(Properties));
}

jobjectArray parse_remote_characteristic_properties(JNIEnv *env, DBusMessageIter *iter) {
    return parse_properties(env, iter, (Properties *) &characteristic_properties,
                          sizeof(characteristic_properties) / sizeof(Properties));
}

jobjectArray parse_remote_characteristic_property_change(JNIEnv *env, DBusMessage *msg) {
    return parse_property_change(env, msg, (Properties *) &characteristic_properties,
                    sizeof(characteristic_properties) / sizeof(Properties));
}

#endif


static void classInitNative(JNIEnv* env, jclass clazz) {
    LOGV(__FUNCTION__);
#ifdef BLUETI_ENHANCEMENT
#ifdef HAVE_BLUETOOTH
    method_onValueChanged = env->GetMethodID(clazz, "onCharValueChanged",
                                               "(Ljava/lang/String;[B)V");
    method_WriteRequestComplete = env->GetMethodID(clazz, "onCharValueWriteReqComplete",
            "(Ljava/lang/String;B)V");
    method_ReadComplete = env->GetMethodID(clazz, "onCharValueReadComplete",
            "(Ljava/lang/String;B[B)V");
    method_onCharacteristicPropertyChanged = env->GetMethodID(clazz,
            "onCharacteristicPropertyChanged",
            "(Ljava/lang/String;[Ljava/lang/String;)V");
    field_mNativeData = env->GetFieldID(clazz, "mNativeData", "I");
#endif
#endif
}

/* Returns true on success (even if adapter is present but disabled).
 * Return false if dbus is down, or another serious error (out of memory)
*/
static bool initNative(JNIEnv* env, jobject object) {
    DBusMessage *msg = NULL, *reply = NULL;
    DBusError err;
    const char *device_path = NULL;
    int attempt = 0;

    LOGV("%s", __FUNCTION__);
#ifdef HAVE_BLUETOOTH
    native_data_t *nat = (native_data_t *)calloc(1, sizeof(native_data_t));
    if (NULL == nat) {
        LOGE("%s: out of memory!", __FUNCTION__);
        return false;
    }

    env->SetIntField(object, field_mNativeData, (jint)nat);
    gnat = nat;

    env->GetJavaVM( &(nat->vm) );
    nat->envVer = env->GetVersion();
    nat->me = env->NewGlobalRef(object);

    dbus_error_init(&err);
    dbus_threads_init_default();
    nat->conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
    if (dbus_error_is_set(&err)) {
        LOGE("Could not get onto the system bus: %s", err.message);
        dbus_error_free(&err);
        return false;
    }
    dbus_connection_set_exit_on_disconnect(nat->conn, FALSE);

    LOGE("%s Registering obecjt path at %s",__FUNCTION__, GATT_CLIENT_WATCHER_PATH);
    if (!dbus_connection_register_object_path(nat->conn, GATT_CLIENT_WATCHER_PATH,
            &gattclient_vtable, nat)) {
        LOGE("%s: Can't register object path %s for GATT Client watcher!",
              __FUNCTION__, GATT_CLIENT_WATCHER_PATH);
        return -1;
    }

#endif
    return true;
}

static void cleanupNative(JNIEnv* env, jobject object) {
#ifdef HAVE_BLUETOOTH
    LOGV("%s", __FUNCTION__);
    native_data_t *nat =
            (native_data_t *)env->GetIntField(object, field_mNativeData);
    if (nat) {
        if (!dbus_connection_unregister_object_path (nat->conn, GATT_CLIENT_WATCHER_PATH)) {
            LOGE("%s: Can't unregister object path %s for GATT Client watcher!",
                  __FUNCTION__, GATT_CLIENT_WATCHER_PATH);
        }

        dbus_connection_close(nat->conn);
        env->DeleteGlobalRef(nat->me);
        free(nat);
        nat = NULL;
    }
#endif
}

static int register_watcher(native_data_t *nat,
                          const char * watcher_path, const char *device_path)
{
#ifdef HAVE_BLUETOOTH
    DBusMessage *msg, *reply;
    DBusError err;

    msg = dbus_message_new_method_call("org.bluez", device_path,
          "org.bluez.Characteristic", "RegisterCharacteristicsWatcher");
    if (!msg) {
        LOGE("%s: Can't allocate new method call for watcher!",
              __FUNCTION__);
        return -1;
    }
    dbus_message_append_args(msg, DBUS_TYPE_OBJECT_PATH, &watcher_path,
                             DBUS_TYPE_INVALID);

    dbus_error_init(&err);
    reply = dbus_connection_send_with_reply_and_block(nat->conn, msg, -1, &err);
    dbus_message_unref(msg);

    if (!reply) {
        LOGE("%s: Can't register watcher!", __FUNCTION__);
        if (dbus_error_is_set(&err)) {
            LOG_AND_FREE_DBUS_ERROR(&err);
        }
        return -1;
    }

    dbus_message_unref(reply);
    dbus_connection_flush(nat->conn);
#endif
    return 0;
}

static int unregister_watcher(native_data_t *nat,
                          const char * watcher_path, const char *device_path)
{
#ifdef HAVE_BLUETOOTH
    DBusMessage *msg, *reply;
    DBusError err;

    msg = dbus_message_new_method_call("org.bluez", device_path,
          "org.bluez.Characteristic", "UnregisterCharacteristicsWatcher");
    if (!msg) {
        LOGE("%s: Can't allocate new method call for watcher!",
              __FUNCTION__);
        return -1;
    }
    dbus_message_append_args(msg, DBUS_TYPE_OBJECT_PATH, &watcher_path,
                             DBUS_TYPE_INVALID);

    dbus_error_init(&err);
    reply = dbus_connection_send_with_reply_and_block(nat->conn, msg, -1, &err);
    dbus_message_unref(msg);

    if (!reply) {
        LOGE("%s: Can't register watcher!", __FUNCTION__);
        if (dbus_error_is_set(&err)) {
            LOG_AND_FREE_DBUS_ERROR(&err);
        }
        return -1;
    }

    dbus_message_unref(reply);
    dbus_connection_flush(nat->conn);
#endif
    return 0;
}

static jboolean registerWatcherNative(JNIEnv *env, jobject obj, jstring svc) {
#ifdef HAVE_BLUETOOTH
    const char *c_svc = env->GetStringUTFChars(svc, NULL);
    LOGE("%s registering watcher on %s",__FUNCTION__, c_svc);

    native_data_t *nat = get_native_data(env, obj);

    if (nat != NULL && nat->conn != NULL) {
        return (register_watcher(nat, GATT_CLIENT_WATCHER_PATH, c_svc) == 0);
    }

#endif
    return 0;
}

static jboolean unregisterWatcherNative(JNIEnv *env, jobject obj, jstring svc) {
#ifdef HAVE_BLUETOOTH
    const char *c_svc = env->GetStringUTFChars(svc, NULL);
    LOGE("%s unregistering watcher on %s",__FUNCTION__, c_svc);

    native_data_t *nat = get_native_data(env, obj);

    if (nat != NULL && nat->conn != NULL) {
        return (unregister_watcher(nat, GATT_CLIENT_WATCHER_PATH, c_svc) == 0);
    }
#endif
    return 0;
}

static jobjectArray getServicePropertiesNative(JNIEnv *env, jobject object,
                                                    jstring path)
{
#ifdef HAVE_BLUETOOTH
    LOGV("%s", __FUNCTION__);

    native_data_t *nat = get_native_data(env, object);

    if (nat) {
        DBusMessage *msg, *reply;
        DBusError err;
        dbus_error_init(&err);

        const char *c_path = env->GetStringUTFChars(path, NULL);
        LOGE("%s c_path=%s",__FUNCTION__, c_path);
        reply = dbus_func_args_timeout(env,
                                   nat->conn, -1, c_path,
                                   DBUS_CHAR_IFACE, "GetProperties",
                                   DBUS_TYPE_INVALID);
        env->ReleaseStringUTFChars(path, c_path);

        if (!reply) {
            if (dbus_error_is_set(&err)) {
                LOG_AND_FREE_DBUS_ERROR(&err);
            } else
                LOGE("DBus reply is NULL in function %s", __FUNCTION__);

            LOGE("%s returning null for some reason", __FUNCTION__);
            return NULL;
        }
        env->PushLocalFrame(30);

        DBusMessageIter iter;
        jobjectArray str_array = NULL;
        if (dbus_message_iter_init(reply, &iter))
           str_array =  parse_remote_service_properties(env, &iter);
        dbus_message_unref(reply);

        return (jobjectArray) env->PopLocalFrame(str_array);
    }
#endif
    return NULL;
}

static jobjectArray getCharacteristicPropertiesNative(JNIEnv *env,
        jobject object, jstring path)
{
#ifdef HAVE_BLUETOOTH
    LOGV("%s", __FUNCTION__);

    native_data_t *nat = get_native_data(env, object);

    if (nat) {
        DBusMessage *msg, *reply;
        DBusError err;
        dbus_error_init(&err);

        const char *c_path = env->GetStringUTFChars(path, NULL);
        LOGE("%s c_path=%s",__FUNCTION__, c_path);
        reply = dbus_func_args_timeout(env,
                                   nat->conn, -1, c_path,
                                   DBUS_CHAR_IFACE, "GetProperties",
                                   DBUS_TYPE_INVALID);
        env->ReleaseStringUTFChars(path, c_path);

        if (!reply) {
            if (dbus_error_is_set(&err)) {
                LOG_AND_FREE_DBUS_ERROR(&err);
            } else
                LOGE("DBus reply is NULL in function %s", __FUNCTION__);

            LOGE("%s returning null for some reason", __FUNCTION__);
            return NULL;
        }
        env->PushLocalFrame(30);

        DBusMessageIter iter;
        jobjectArray str_array = NULL;
        if (dbus_message_iter_init(reply, &iter))
           str_array =  parse_remote_characteristic_properties(env, &iter);
        dbus_message_unref(reply);

        return (jobjectArray) env->PopLocalFrame(str_array);
    }
#endif
    return NULL;
}

static jobjectArray discoverServiceCharacteristicsNative(JNIEnv *env,
                                                            jobject object, jstring path)
{
#ifdef HAVE_BLUETOOTH
    LOGV("%s", __FUNCTION__);

    native_data_t *nat = get_native_data(env, object);

    if (nat) {
        DBusMessage *msg, *reply;
        DBusError err;
        dbus_error_init(&err);

        const char *c_path = env->GetStringUTFChars(path, NULL);
        LOGE("%s c_path=%s",__FUNCTION__, c_path);
        reply = dbus_func_args_timeout(env,
                                   nat->conn, -1, c_path,
                                   DBUS_CHAR_IFACE, "DiscoverCharacteristics",
                                   DBUS_TYPE_INVALID);
        env->ReleaseStringUTFChars(path, c_path);

        if (!reply) {
            if (dbus_error_is_set(&err)) {
                LOG_AND_FREE_DBUS_ERROR(&err);
            } else
                LOGE("DBus reply is NULL in function %s", __FUNCTION__);

            LOGE("%s returning null for some reason", __FUNCTION__);
            return NULL;
        }
        env->PushLocalFrame(30);

        DBusMessageIter iter;
        jobjectArray str_array = NULL;

        if (dbus_message_iter_init(reply, &iter)) {

            str_array = dbus_returns_array_of_object_path(env,reply);

        }
        return (jobjectArray) env->PopLocalFrame(str_array);
    }
#endif
    return NULL;
}

static jboolean setCharacteristicPropertyNative(JNIEnv *env, jobject object, jstring path, jstring property, void *value, jint type)
{
#ifdef HAVE_BLUETOOTH
    LOGV("%s", __FUNCTION__);

    native_data_t *nat = get_native_data(env, object);

    if (nat) {
        DBusMessage *msg;
        DBusMessageIter iter;
        DBusError err;
        dbus_error_init(&err);
        const char *c_path = env->GetStringUTFChars(path, NULL);
        const char *c_key = env->GetStringUTFChars(property, NULL);
        dbus_bool_t reply = JNI_FALSE;

        LOGE("%s c_path=%s",__FUNCTION__, c_path);

        msg = dbus_message_new_method_call(BLUEZ_DBUS_BASE_IFC,
                                          c_path, DBUS_CHAR_IFACE, "SetProperty");
        if (!msg) {
            LOGE("%s: Can't allocate new method call for device SetProperty!", __FUNCTION__);
            env->ReleaseStringUTFChars(path, c_path);
            env->ReleaseStringUTFChars(property, c_key);
            return JNI_FALSE;
        }

        dbus_message_append_args(msg, DBUS_TYPE_STRING, &c_key, DBUS_TYPE_INVALID);
        dbus_message_iter_init_append(msg, &iter);
        append_variant(&iter, type, value);

        reply = dbus_connection_send_with_reply(nat->conn, msg, NULL, -1);
        if (!reply) {
            if (dbus_error_is_set(&err)) {
                LOG_AND_FREE_DBUS_ERROR(&err);
            } else {
                LOGE("DBus reply is NULL in function %s", __FUNCTION__);
            }
        }
        dbus_message_unref(msg);
        env->ReleaseStringUTFChars(path, c_path);
        env->ReleaseStringUTFChars(property, c_key);
        return reply ? JNI_TRUE : JNI_FALSE;
    }
#endif
    return JNI_FALSE;
}

static jboolean setCharacteristicPropertyStringNative(JNIEnv *env, jobject object, jstring path, jstring key,
                                               jstring value) {
#ifdef HAVE_BLUETOOTH
    const char *c_value = env->GetStringUTFChars(value, NULL);
    jboolean ret =  setCharacteristicPropertyNative(env, object, path, key, (void *)&c_value, DBUS_TYPE_STRING);
    env->ReleaseStringUTFChars(value, (char *)c_value);
    return ret;
#else
    return JNI_FALSE;
#endif
}

static jboolean setCharacteristicPropertyIntegerNative(JNIEnv *env, jobject object, jstring path, jstring key,
                                               jint value) {
#ifdef HAVE_BLUETOOTH
    return setCharacteristicPropertyNative(env, object, path, key, (void *)&value, DBUS_TYPE_UINT32);
#else
    return JNI_FALSE;
#endif
}

static jboolean setCharacteristicPropertyBooleanNative(JNIEnv *env, jobject object, jstring path, jstring key,
                                               jint value) {
#ifdef HAVE_BLUETOOTH
    return setCharacteristicPropertyNative(env, object, path, key, (void *)&value, DBUS_TYPE_BOOLEAN);
#else
    return JNI_FALSE;
#endif
}

static jboolean writeValueRequestNative(JNIEnv *env, jobject object, jstring path, jbyteArray buff)
{
#ifdef HAVE_BLUETOOTH
    LOGV("%s", __FUNCTION__);

    native_data_t *nat = get_native_data(env, object);

    if (nat) {
        DBusMessage *msg, *reply;
        DBusError err;
        dbus_error_init(&err);
        jsize len = env->GetArrayLength(buff);
        uint16_t offset = 0;
        uint16_t timeout = 0;
        jbyte *value = env->GetByteArrayElements(buff, NULL);
        const char *c_op = "WriteWithResponse";

        if (value == NULL)
            return false;


        const char *c_path = env->GetStringUTFChars(path, NULL);
        LOGE("%s c_path=%s",__FUNCTION__, c_path);

        reply = dbus_func_args_timeout(env,
                                   nat->conn, -1, c_path,
                                   DBUS_CHAR_IFACE, "WriteValue",
                                   DBUS_TYPE_ARRAY,DBUS_TYPE_BYTE, &value, len,
                                   DBUS_TYPE_STRING, &c_op,
                                   DBUS_TYPE_UINT16, &offset,
                                   DBUS_TYPE_UINT16, &timeout,
                                   DBUS_TYPE_INVALID);
        env->ReleaseStringUTFChars(path, c_path);
        env->ReleaseByteArrayElements(buff, value, 0);

        if (!reply) {
            if (dbus_error_is_set(&err)) {
                LOG_AND_FREE_DBUS_ERROR(&err);
            } else
                LOGE("DBus reply is NULL in function %s", __FUNCTION__);

            LOGE("%s returning null for some reason", __FUNCTION__);
            return false;
        }
        dbus_message_unref(reply);
        return true;
    }
#endif
    return false;
}

static jboolean writeValueCommandNative(JNIEnv *env, jobject object, jstring path, jbyteArray buff)
{
#ifdef HAVE_BLUETOOTH
    LOGV("%s", __FUNCTION__);

    native_data_t *nat = get_native_data(env, object);

    if (nat) {
        DBusMessage *msg, *reply;
        DBusError err;
        dbus_error_init(&err);
        jsize len = env->GetArrayLength(buff);
        uint16_t offset = 0;
        uint16_t timeout = 0;
        const char *c_op = "WriteWithoutResponse";

        jbyte *value = env->GetByteArrayElements(buff, NULL);

        if (value == NULL)
            return false;

        const char *c_path = env->GetStringUTFChars(path, NULL);
        LOGE("%s c_path=%s buff_len=%d",__FUNCTION__, c_path,len);

        reply = dbus_func_args_timeout(env,
                                   nat->conn, -1, c_path,
                                   DBUS_CHAR_IFACE, "WriteValue",
                                   DBUS_TYPE_ARRAY,DBUS_TYPE_BYTE, &value, len,
                                   DBUS_TYPE_STRING, &c_op,
                                   DBUS_TYPE_UINT16, &offset,
                                   DBUS_TYPE_UINT16, &timeout,
                                   DBUS_TYPE_INVALID);

        env->ReleaseStringUTFChars(path, c_path);
        env->ReleaseByteArrayElements(buff, value, 0);

        if (!reply) {
            if (dbus_error_is_set(&err)) {
                LOG_AND_FREE_DBUS_ERROR(&err);
            } else
                LOGE("DBus reply is NULL in function %s", __FUNCTION__);

            LOGE("%s returning null for some reason", __FUNCTION__);
            return false;
        }
        dbus_message_unref(reply);
        return true;
    }
#endif
    return false;
}

static jboolean readValueCommandNative(JNIEnv *env, jobject object, jstring path, jint offst)
{
#ifdef HAVE_BLUETOOTH
    LOGV("%s", __FUNCTION__);

    native_data_t *nat = get_native_data(env, object);

    if (nat) {
        DBusMessage *msg, *reply;
        DBusError err;
        dbus_error_init(&err);
        uint16_t offset = 0; /* TODO : Do not ignore the offst function argument */
        uint16_t timeout = 0;

        const char *c_path = env->GetStringUTFChars(path, NULL);
        LOGE("%s c_path=%s offset=%d",__FUNCTION__, c_path,offst);

        reply = dbus_func_args_timeout(env,
                                   nat->conn, -1, c_path,
                                   DBUS_CHAR_IFACE, "ReadValue",
                                   DBUS_TYPE_UINT16, &offset,
                                   DBUS_TYPE_UINT16, &timeout,
                                   DBUS_TYPE_INVALID);

        env->ReleaseStringUTFChars(path, c_path);

        if (!reply) {
            if (dbus_error_is_set(&err)) {
                LOG_AND_FREE_DBUS_ERROR(&err);
            } else
                LOGE("DBus reply is NULL in function %s", __FUNCTION__);

            LOGE("%s returning null for some reason", __FUNCTION__);
            return false;
        }
        dbus_message_unref(reply);
        return true;
    }
#endif
    return false;
}

static void connectNative(JNIEnv *env, jobject object, jstring path)
{
#ifdef HAVE_BLUETOOTH
    LOGV("%s", __FUNCTION__);

    native_data_t *nat = get_native_data(env, object);

    if (nat) {
        DBusMessage *msg, *reply;
        DBusError err;
        dbus_error_init(&err);

        const char *c_path = env->GetStringUTFChars(path, NULL);
        LOGE("%s c_path=%s",__FUNCTION__, c_path);
        reply = dbus_func_args_timeout(env,
                                   nat->conn, -1, c_path,
                                   DBUS_DEVICE_IFACE, "GattClientConnect",
                                   DBUS_TYPE_INVALID);
        env->ReleaseStringUTFChars(path, c_path);

        if (!reply) {
            if (dbus_error_is_set(&err)) {
                LOG_AND_FREE_DBUS_ERROR(&err);
            } else
                LOGE("DBus reply is NULL in function %s", __FUNCTION__);

            LOGE("%s returning null for some reason", __FUNCTION__);
        } else
            dbus_message_unref(reply);

    }
#endif
}

static void disconnectNative(JNIEnv *env, jobject object, jstring path)
{
#ifdef HAVE_BLUETOOTH
    LOGV("%s", __FUNCTION__);

    native_data_t *nat = get_native_data(env, object);

    if (nat) {
        DBusMessage *msg, *reply;
        DBusError err;
        dbus_error_init(&err);

        const char *c_path = env->GetStringUTFChars(path, NULL);
        LOGE("%s c_path=%s",__FUNCTION__, c_path);
        reply = dbus_func_args_timeout(env,
                                   nat->conn, -1, c_path,
                                   DBUS_DEVICE_IFACE, "GattClientDisconnect",
                                   DBUS_TYPE_INVALID);
        env->ReleaseStringUTFChars(path, c_path);

        if (!reply) {
            if (dbus_error_is_set(&err)) {
                LOG_AND_FREE_DBUS_ERROR(&err);
            } else
                LOGE("DBus reply is NULL in function %s", __FUNCTION__);

            LOGE("%s returning null for some reason", __FUNCTION__);
        } else
            dbus_message_unref(reply);
    }
#endif
}

static void throwErrnoNative(JNIEnv *env, jobject obj, jint err) {
    jniThrowIOException(env, err);
}

#ifdef HAVE_BLUETOOTH
DBusHandlerResult watcher_event_filter(DBusConnection *conn,DBusMessage *msg, void *data) {
    native_data_t *nat = (native_data_t *)data;
    JNIEnv *env;
    char *path;
    char *buffer;
    int len = 0;
    unsigned char result;
    jbyteArray val;
    jstring obj;

    if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_METHOD_CALL) {
        LOGV("%s: not interested (not a method call).", __FUNCTION__);
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
    LOGI("%s: Received method %s:%s", __FUNCTION__,
         dbus_message_get_interface(msg), dbus_message_get_member(msg));

    if (nat == NULL) return DBUS_HANDLER_RESULT_HANDLED;

    nat->vm->GetEnv((void**)&env, nat->envVer);

    if (dbus_message_is_method_call(msg,
            BLUEZ_WATCHER_IFACE, GATT_CHAR_VALUE_CHANGED_METHOD)) {

        if (dbus_message_get_args(msg, NULL,
                DBUS_TYPE_OBJECT_PATH, &path,
                DBUS_TYPE_ARRAY,DBUS_TYPE_BYTE, &buffer, &len,
                DBUS_TYPE_INVALID)) {
            LOGE("%s path=%s buffer[0]=%02X length=%d",__FUNCTION__, path, buffer[0],len);

            val=env->NewByteArray(len);
            if (val == NULL)
                LOGE("%s: val=NULL !",__FUNCTION__);

            env->SetByteArrayRegion(val, 0, len, (jbyte *)buffer);
            obj = env->NewStringUTF(path);
            env->CallVoidMethod(nat->me, method_onValueChanged,
                    obj,
                    (jbyteArray)val);

            env->ReleaseByteArrayElements(val,(jbyte *)buffer, 0);
            env->DeleteLocalRef(val);
            env->DeleteLocalRef(obj);
        }

        // reply
        DBusMessage *reply = dbus_message_new_method_return(msg);
        if (reply) {
            dbus_connection_send(nat->conn, reply, NULL);
            dbus_message_unref(reply);
        }
    } else if (dbus_message_is_method_call(msg,
            BLUEZ_WATCHER_IFACE,GATT_CHAR_WRITE_REQ_RESPONSE_METHOD)) {
        if (dbus_message_get_args(msg, NULL,
                DBUS_TYPE_OBJECT_PATH, &path,
                DBUS_TYPE_BYTE, &result,
                DBUS_TYPE_INVALID)) {
            LOGE("%s path=%s result=%d",__FUNCTION__, path, result);

            env->CallVoidMethod(nat->me, method_WriteRequestComplete,
                    env->NewStringUTF(path),
                    (jbyte)result);
        }

        // reply
        DBusMessage *reply = dbus_message_new_method_return(msg);
        if (reply) {
            dbus_connection_send(nat->conn, reply, NULL);
            dbus_message_unref(reply);
        }
    } else if (dbus_message_is_method_call(msg,
            BLUEZ_WATCHER_IFACE,GATT_CHAR_READ_REQ_RESPONSE_METHOD)) {
        if (dbus_message_get_args(msg, NULL,
                DBUS_TYPE_OBJECT_PATH, &path,
                DBUS_TYPE_BYTE, &result,
                DBUS_TYPE_ARRAY,DBUS_TYPE_BYTE, &buffer, &len,
                DBUS_TYPE_INVALID)) {
            LOGE("%s path=%s result=%d buff[0]=%02X len=%d",__FUNCTION__, path, result,buffer[0],len);

            val=env->NewByteArray(len);
            if (val == NULL)
                LOGE("%s: val=NULL !",__FUNCTION__);

            env->SetByteArrayRegion(val, 0, len, (jbyte *)buffer);

            env->CallVoidMethod(nat->me, method_ReadComplete,
                    env->NewStringUTF(path),
                    (jbyte)result,
                    (jbyteArray)val);

            env->ReleaseByteArrayElements(val,(jbyte *)buffer, 0);

        }

        // reply
        DBusMessage *reply = dbus_message_new_method_return(msg);
        if (reply) {
            dbus_connection_send(nat->conn, reply, NULL);
            dbus_message_unref(reply);
        }
    }


    if (env->ExceptionCheck()) {
        LOGE("VM Exception occurred while handling %s.%s (%s) in %s,"
             " leaving for VM",
             dbus_message_get_interface(msg), dbus_message_get_member(msg),
             dbus_message_get_path(msg), __FUNCTION__);
    }
    return DBUS_HANDLER_RESULT_HANDLED;
}

DBusHandlerResult gattclient_event_filter(DBusMessage *msg, JNIEnv *env) {
    DBusError err;
    DBusHandlerResult ret;
    DBusHandlerResult result = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    dbus_error_init(&err);

    if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_SIGNAL) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    if (dbus_message_is_signal(msg,"org.bluez.Characteristic", "PropertyChanged")) {
            LOGV("org.bluez.Characteristic.PropertyChanged");
            jobjectArray str_array = parse_remote_characteristic_property_change(env, msg);
            if (str_array != NULL) {
                const char *remote_char_path = dbus_message_get_path(msg);
                env->CallVoidMethod(gnat->me,
                                method_onCharacteristicPropertyChanged,
                                env->NewStringUTF(remote_char_path),
                                str_array);
            } else LOG_AND_FREE_DBUS_ERROR_WITH_MSG(&err, msg);
            result = DBUS_HANDLER_RESULT_HANDLED;
    } else {
        LOGV("... ignored");
    }

    if (env->ExceptionCheck()) {
        LOGE("VM Exception occurred while handling %s.%s (%s) in %s,"
             " leaving for VM",
             dbus_message_get_interface(msg), dbus_message_get_member(msg),
             dbus_message_get_path(msg), __FUNCTION__);
    }

    return result;

}
#endif

static JNINativeMethod sMethods[] = {
    {"classInitNative", "()V", (void *)classInitNative},
    {"initNative", "()Z", (void *)initNative},
    {"connectNative", "(Ljava/lang/String;)V", (void *)connectNative},
    {"disconnectNative", "(Ljava/lang/String;)V", (void *)disconnectNative},
    {"cleanupNative", "()V", (void *)cleanupNative},
    {"registerWatcherNative", "(Ljava/lang/String;)Z",(void *)registerWatcherNative},
    {"unregisterWatcherNative", "(Ljava/lang/String;)Z",(void *)unregisterWatcherNative},
    {"discoverServiceCharacteristicsNative", "(Ljava/lang/String;)[Ljava/lang/Object;",(void *)discoverServiceCharacteristicsNative},
    {"getServicePropertiesNative", "(Ljava/lang/String;)[Ljava/lang/Object;",(void *)getServicePropertiesNative},
    {"getCharacteristicPropertiesNative", "(Ljava/lang/String;)[Ljava/lang/Object;",(void *)getCharacteristicPropertiesNative},
    {"setCharacteristicPropertyStringNative", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Z",  (void *)setCharacteristicPropertyStringNative},
    {"setCharacteristicPropertyBooleanNative", "(Ljava/lang/String;Ljava/lang/String;I)Z",  (void *)setCharacteristicPropertyBooleanNative},
    {"setCharacteristicPropertyIntegerNative", "(Ljava/lang/String;Ljava/lang/String;I)Z",  (void *)setCharacteristicPropertyIntegerNative},
    {"writeValueRequestNative", "(Ljava/lang/String;[B)Z", (void*)writeValueRequestNative},
    {"writeValueCommandNative", "(Ljava/lang/String;[B)Z", (void*)writeValueCommandNative},
    {"readValueCommandNative", "(Ljava/lang/String;I)Z", (void*)readValueCommandNative},

};

int register_com_ti_bluetooth_BluetoothGattClient(JNIEnv *env) {
    return AndroidRuntime::registerNativeMethods(env,
        "android/server/BluetoothGattClientService", sMethods, NELEM(sMethods));
}

} /* namespace android */
