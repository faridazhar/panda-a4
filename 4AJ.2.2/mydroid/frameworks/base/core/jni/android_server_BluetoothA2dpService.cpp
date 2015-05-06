/*
** Copyright 2008, The Android Open Source Project
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

#define LOG_TAG "BluetoothA2dpService.cpp"

#include "android_bluetooth_common.h"
#include "android_runtime/AndroidRuntime.h"
#include "JNIHelp.h"
#include "jni.h"
#include "utils/Log.h"
#include "utils/misc.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef HAVE_BLUETOOTH
#include <dbus/dbus.h>
#endif

namespace android {

#ifdef HAVE_BLUETOOTH
static jmethodID method_onSinkPropertyChanged;
static jmethodID method_onConnectSinkResult;

#ifdef BLUETI_ENHANCEMENT
static jmethodID method_onMediaPlayerPropertyChanged;

static jfieldID field_mEventLoop;
#endif /*BLUETI_ENHANCEMENT*/

typedef struct {
    JavaVM *vm;
    int envVer;
    DBusConnection *conn;
    jobject me;  // for callbacks to java
} native_data_t;

static native_data_t *nat = NULL;  // global native data
static void onConnectSinkResult(DBusMessage *msg, void *user, void *n);

static Properties sink_properties[] = {
        {"State", DBUS_TYPE_STRING},
        {"Connected", DBUS_TYPE_BOOLEAN},
        {"Playing", DBUS_TYPE_BOOLEAN},
      };

#ifdef BLUETI_ENHANCEMENT
static Properties media_player_properties[] = {
    {"Equalizer", DBUS_TYPE_STRING},
    {"Repeat", DBUS_TYPE_STRING},
    {"Shuffle", DBUS_TYPE_STRING},
    {"Scan", DBUS_TYPE_STRING},
    {"Volume", DBUS_TYPE_STRING},
    {"AVRCP_1_4", DBUS_TYPE_STRING},
};

extern event_loop_native_data_t *get_EventLoop_native_data(JNIEnv *,
                                                           jobject);
#endif /*BLUETI_ENHANCEMENT*/

#endif

/* Returns true on success (even if adapter is present but disabled).
 * Return false if dbus is down, or another serious error (out of memory)
*/
static bool initNative(JNIEnv* env, jobject object) {
    ALOGV("%s", __FUNCTION__);
#ifdef HAVE_BLUETOOTH
    nat = (native_data_t *)calloc(1, sizeof(native_data_t));
    if (NULL == nat) {
        ALOGE("%s: out of memory!", __FUNCTION__);
        return false;
    }
    env->GetJavaVM( &(nat->vm) );
    nat->envVer = env->GetVersion();
    nat->me = env->NewGlobalRef(object);

    DBusError err;
    dbus_error_init(&err);
    dbus_threads_init_default();
    nat->conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
    if (dbus_error_is_set(&err)) {
        ALOGE("Could not get onto the system bus: %s", err.message);
        dbus_error_free(&err);
        return false;
    }
    dbus_connection_set_exit_on_disconnect(nat->conn, FALSE);
#endif  /*HAVE_BLUETOOTH*/
    return true;
}

static void cleanupNative(JNIEnv* env, jobject object) {
#ifdef HAVE_BLUETOOTH
    ALOGV("%s", __FUNCTION__);
    if (nat) {
        dbus_connection_close(nat->conn);
        env->DeleteGlobalRef(nat->me);
        free(nat);
        nat = NULL;
    }
#endif
}

static jobjectArray getSinkPropertiesNative(JNIEnv *env, jobject object,
                                            jstring path) {
#ifdef HAVE_BLUETOOTH
    ALOGV("%s", __FUNCTION__);
    if (nat) {
        DBusMessage *msg, *reply;
        DBusError err;
        dbus_error_init(&err);

        const char *c_path = env->GetStringUTFChars(path, NULL);
        reply = dbus_func_args_timeout(env,
                                   nat->conn, -1, c_path,
                                   "org.bluez.AudioSink", "GetProperties",
                                   DBUS_TYPE_INVALID);
        env->ReleaseStringUTFChars(path, c_path);
        if (!reply && dbus_error_is_set(&err)) {
            LOG_AND_FREE_DBUS_ERROR_WITH_MSG(&err, reply);
            return NULL;
        } else if (!reply) {
            ALOGE("DBus reply is NULL in function %s", __FUNCTION__);
            return NULL;
        }
        DBusMessageIter iter;
        if (dbus_message_iter_init(reply, &iter))
            return parse_properties(env, &iter, (Properties *)&sink_properties,
                                 sizeof(sink_properties) / sizeof(Properties));
    }
#endif
    return NULL;
}


static jboolean connectSinkNative(JNIEnv *env, jobject object, jstring path) {
#ifdef HAVE_BLUETOOTH
    ALOGV("%s", __FUNCTION__);
    if (nat) {
        const char *c_path = env->GetStringUTFChars(path, NULL);
        int len = env->GetStringLength(path) + 1;
        char *context_path = (char *)calloc(len, sizeof(char));
        strlcpy(context_path, c_path, len);  // for callback

        bool ret = dbus_func_args_async(env, nat->conn, -1, onConnectSinkResult, context_path,
                                    nat, c_path, "org.bluez.AudioSink", "Connect",
                                    DBUS_TYPE_INVALID);

        env->ReleaseStringUTFChars(path, c_path);
        return ret ? JNI_TRUE : JNI_FALSE;
    }
#endif
    return JNI_FALSE;
}

static jboolean disconnectSinkNative(JNIEnv *env, jobject object,
                                     jstring path) {
#ifdef HAVE_BLUETOOTH
    ALOGV("%s", __FUNCTION__);
    if (nat) {
        const char *c_path = env->GetStringUTFChars(path, NULL);

        bool ret = dbus_func_args_async(env, nat->conn, -1, NULL, NULL, nat,
                                    c_path, "org.bluez.AudioSink", "Disconnect",
                                    DBUS_TYPE_INVALID);

        env->ReleaseStringUTFChars(path, c_path);
        return ret ? JNI_TRUE : JNI_FALSE;
    }
#endif
    return JNI_FALSE;
}

static jboolean suspendSinkNative(JNIEnv *env, jobject object,
                                     jstring path) {
#ifdef HAVE_BLUETOOTH
    ALOGV("%s", __FUNCTION__);
    if (nat) {
        const char *c_path = env->GetStringUTFChars(path, NULL);
        bool ret = dbus_func_args_async(env, nat->conn, -1, NULL, NULL, nat,
                           c_path, "org.bluez.audio.Sink", "Suspend",
                           DBUS_TYPE_INVALID);
        env->ReleaseStringUTFChars(path, c_path);
        return ret ? JNI_TRUE : JNI_FALSE;
    }
#endif
    return JNI_FALSE;
}

static jboolean resumeSinkNative(JNIEnv *env, jobject object,
                                     jstring path) {
#ifdef HAVE_BLUETOOTH
    ALOGV("%s", __FUNCTION__);
    if (nat) {
        const char *c_path = env->GetStringUTFChars(path, NULL);
        bool ret = dbus_func_args_async(env, nat->conn, -1, NULL, NULL, nat,
                           c_path, "org.bluez.audio.Sink", "Resume",
                           DBUS_TYPE_INVALID);
        env->ReleaseStringUTFChars(path, c_path);
        return ret ? JNI_TRUE : JNI_FALSE;
    }
#endif
    return JNI_FALSE;
}

static jboolean avrcpVolumeUpNative(JNIEnv *env, jobject object,
                                     jstring path) {
#ifdef HAVE_BLUETOOTH
    ALOGV("%s", __FUNCTION__);
    if (nat) {
        const char *c_path = env->GetStringUTFChars(path, NULL);
        bool ret = dbus_func_args_async(env, nat->conn, -1, NULL, NULL, nat,
                           c_path, "org.bluez.Control", "VolumeUp",
                           DBUS_TYPE_INVALID);
        env->ReleaseStringUTFChars(path, c_path);
        return ret ? JNI_TRUE : JNI_FALSE;
    }
#endif
    return JNI_FALSE;
}

static jboolean avrcpVolumeDownNative(JNIEnv *env, jobject object,
                                     jstring path) {
#ifdef HAVE_BLUETOOTH
    ALOGV("%s", __FUNCTION__);
    if (nat) {
        const char *c_path = env->GetStringUTFChars(path, NULL);
        bool ret = dbus_func_args_async(env, nat->conn, -1, NULL, NULL, nat,
                           c_path, "org.bluez.Control", "VolumeDown",
                           DBUS_TYPE_INVALID);
        env->ReleaseStringUTFChars(path, c_path);
        return ret ? JNI_TRUE : JNI_FALSE;
    }
#endif
    return JNI_FALSE;
}

#ifdef BLUETI_ENHANCEMENT
static jboolean avrcpSetMediaPlaybackStatusNative(JNIEnv *env,
                                                  jobject object, jstring status,
                                                  jlong elapsed) {
#ifdef HAVE_BLUETOOTH
    DBusMessage *msg;
    DBusMessageIter iter;
    DBusMessageIter dict;

    LOGV("%s", __FUNCTION__);
    if (nat) {
        const char *status_prop = "Status";
        const char *elapsed_prop = "Position";

        dbus_bool_t ret;
        DBusMessage *msg = dbus_message_new_signal(BLUEZ_DBUS_MEDIA_PLAYER_PATH,
                                                   BLUEZ_DBUS_MEDIA_PLAYER_IFC,
                                                   "PropertyChanged");

        if (msg == NULL) {
            LOGE("%s: Can't allocate new method call for ChangeTrack!", __FUNCTION__);
            return FALSE;
        }

        LOGV("%s: %s %s %u", __FUNCTION__, c_status, (unsigned int) elapsed);

        const char *c_status = env->GetStringUTFChars(status, NULL);
        dbus_message_iter_init_append(msg, &iter);
        dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, (void *) &status_prop);
        append_variant(&iter, DBUS_TYPE_STRING, &c_status);
        ret = dbus_connection_send(nat->conn, msg, NULL);
        dbus_message_unref(msg);
        env->ReleaseStringUTFChars(status, c_status);

        if (!ret)
            return FALSE;;

        msg = dbus_message_new_signal(BLUEZ_DBUS_MEDIA_PLAYER_PATH,
                                                   BLUEZ_DBUS_MEDIA_PLAYER_IFC,
                                                   "PropertyChanged");

        dbus_message_iter_init_append(msg, &iter);
        dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &elapsed_prop);
        append_variant(&iter, DBUS_TYPE_UINT32, (void *) &elapsed);
        ret = dbus_connection_send(nat->conn, msg, NULL);
        dbus_message_unref(msg);

        return ret ? JNI_TRUE : JNI_FALSE;
    }
#endif
    return JNI_FALSE;
}

#ifdef HAVE_BLUETOOTH
static void dict_append_entry(DBusMessageIter *dict,
                    const char *key,
                    int type,
                    void *val)
{
    DBusMessageIter entry;

    LOGV("%s: key: %s type: %d val: %p", __FUNCTION__, key, type, val);

    if (type == DBUS_TYPE_STRING) {
        const char *s = *((const char **) val);
        if (s == NULL)
            return;
    }

    dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
    dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
    append_variant(&entry, type, val);
    dbus_message_iter_close_container(dict, &entry);
}

static int avrcp_get_metadata_type(const char *key) {
    if (strcmp(key, "Title") == 0) {
        return DBUS_TYPE_STRING;
    } else if (strcmp(key, "Artist") == 0) {
        return DBUS_TYPE_STRING;
    } else if (strcmp(key, "Album") == 0) {
        return DBUS_TYPE_STRING;
    } else if (strcmp(key, "Genre") == 0) {
        return DBUS_TYPE_STRING;
    } else if (strcmp(key, "NumberOfTracks") == 0) {
        return DBUS_TYPE_UINT32;
    } else if (strcmp(key, "Number") == 0) {
        return DBUS_TYPE_UINT32;
    } else if (strcmp(key, "Duration") == 0) {
        return DBUS_TYPE_UINT32;
    }

    LOGE("%s: Unknown metadata: %s", __FUNCTION__, key);
    return -ENOENT;
}

static bool avrcp_dbus_write_metadata(DBusMessageIter *iter, char * const c_metadata[])
{
    DBusMessageIter dict;

    dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY,
                                     DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
                                     DBUS_TYPE_STRING_AS_STRING
                                     DBUS_TYPE_VARIANT_AS_STRING
                                     DBUS_DICT_ENTRY_END_CHAR_AS_STRING, &dict);

    for (int j = 0; c_metadata[j] != NULL; j += 2) {
        int type;

        type = avrcp_get_metadata_type(c_metadata[j]);
        if (type < 0)
            continue;

        if (type == DBUS_TYPE_STRING) {
            dict_append_entry(&dict, c_metadata[j], DBUS_TYPE_STRING,
                              (void *) &c_metadata[j + 1]);
        } else if (type == DBUS_TYPE_UINT32) {
            uint32_t val = atol(c_metadata[j + 1]);
            dict_append_entry(&dict, c_metadata[j], DBUS_TYPE_UINT32,
                              (void *) &val);
        }
    }

    dbus_message_iter_close_container(iter, &dict);

    return true;
}

static bool avrcp_dbus_write_settings(DBusMessageIter *iter,
                                      char * const c_properties[])
{
    DBusMessageIter dict;

    dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY,
                                     DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
                                     DBUS_TYPE_STRING_AS_STRING
                                     DBUS_TYPE_VARIANT_AS_STRING
                                     DBUS_DICT_ENTRY_END_CHAR_AS_STRING, &dict);

    for (int j = 0; c_properties[j]; j += 2) {
        const char *c_prop = c_properties[j];
        const char *c_value = c_properties[j + 1];

        dict_append_entry(&dict, c_prop, DBUS_TYPE_STRING, (void *) &c_value);
    }

    dbus_message_iter_close_container(iter, &dict);

    return true;
}

static jsize parse_properties_array(JNIEnv *env, jobject object,
                                  jobjectArray properties,
                                  jsize n_prop, char **c_properties)
{
    jsize j, len;

    for (j = 0, len = 0; j < n_prop; j += 2) {
        jstring obj_key = (jstring)env->GetObjectArrayElement(properties, j);

        if (obj_key == NULL)
            break;

        jstring obj_val = (jstring)env->GetObjectArrayElement(properties, j + 1);
        const char *key = env->GetStringUTFChars(obj_key, NULL);

        if (obj_val == NULL) {
            LOGE("%s: Invalid NULL value for key=%s", __FUNCTION__, key);
            env->ReleaseStringUTFChars(obj_key, key);
            env->DeleteLocalRef((jobject) obj_key);
            break;
        }

        const char *val = env->GetStringUTFChars(obj_val, NULL);

        c_properties[j] = strdup(key);
        c_properties[j + 1] = strdup(val);

        env->ReleaseStringUTFChars(obj_val, val);
        env->DeleteLocalRef((jobject) obj_val);
        env->ReleaseStringUTFChars(obj_key, key);
        env->DeleteLocalRef((jobject) obj_key);
        len += 2;
    }

    return len;
}
#endif

static jboolean avrcpSetMediaNative(JNIEnv *env, jobject object,
                                    jobjectArray properties)
{
#ifdef HAVE_BLUETOOTH
    LOGV("%s", __FUNCTION__);
    if (nat) {
        jsize n_prop = env->GetArrayLength(properties);
        char *c_metadata[n_prop + 1];
        DBusMessage *msg = dbus_message_new_signal(BLUEZ_DBUS_MEDIA_PLAYER_PATH,
                                               BLUEZ_DBUS_MEDIA_PLAYER_IFC,
                                               "TrackChanged");
        DBusMessageIter iter;

        if (msg == NULL) {
            LOGE("%s: Can't allocate new method call for ChangeTrack!", __FUNCTION__);
            return JNI_FALSE;
        }

        n_prop = parse_properties_array(env, object, properties, n_prop,
                                        c_metadata);
        c_metadata[n_prop] = NULL;

        dbus_message_iter_init_append(msg, &iter);
        bool ret = avrcp_dbus_write_metadata(&iter, c_metadata);
        dbus_connection_send(nat->conn, msg, NULL);

        dbus_message_unref(msg);

        for (int j = 0; c_metadata[j]; j++)
            free(c_metadata[j]);

        return ret ? JNI_TRUE : JNI_FALSE;
    }
#endif
    return JNI_FALSE;
}

static jboolean avrcpSetMediaPlayerPropertyNative(JNIEnv *env,
                                                  jobject object,
                                                  jobjectArray properties) {
#ifdef HAVE_BLUETOOTH
    LOGV("%s", __FUNCTION__);
    if (nat) {
        jsize j, n_prop = env->GetArrayLength(properties);
        char *c_properties[n_prop + 1];
        bool ret;

        n_prop = parse_properties_array(env, object, properties, n_prop,
                                        c_properties);
        c_properties[n_prop] = NULL;

        for (j = 0; c_properties[j]; j += 2) {
            DBusMessage *msg = dbus_message_new_signal(BLUEZ_DBUS_MEDIA_PLAYER_PATH,
                                                   BLUEZ_DBUS_MEDIA_PLAYER_IFC,
                                                   "PropertyChanged");
            DBusMessageIter iter;

            if (msg == NULL) {
                ret = false;
                LOGE("%s: Can't allocate new method call for PropertyChanged!", __FUNCTION__);
                break;
            }

            const char *c_prop = c_properties[j];
            const char *c_value = c_properties[j + 1];

            dbus_message_iter_init_append(msg, &iter);
            dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &c_prop);
            append_variant(&iter, DBUS_TYPE_STRING, &c_value);

            LOGV("prop: %s value: %s", c_prop, c_value);

            ret = dbus_connection_send(nat->conn, msg, NULL);

            dbus_message_unref(msg);

            if (!ret) {
                LOGE("%s: D-Bus message failed", __FUNCTION__);
                break;
            }
        }

        for (j = 0; c_properties[j]; j++)
            free(c_properties[j]);

        return ret;
    }
#endif
    return JNI_FALSE;
}

#ifdef HAVE_BLUETOOTH
static const char *get_adapter_path(JNIEnv* env, jobject object) {
    event_loop_native_data_t *event_nat =
        get_EventLoop_native_data(env, env->GetObjectField(object,
                                                           field_mEventLoop));
    if (event_nat == NULL)
        return NULL;
    return event_nat->adapter;
}

DBusHandlerResult media_player_agent_event_filter(DBusConnection *conn,
                                                   DBusMessage *msg,
                                                   void *data)
{
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
    env->PushLocalFrame(10);

    if (dbus_message_is_method_call(msg,
                        BLUEZ_DBUS_MEDIA_PLAYER_IFC, "SetProperty")) {
        jobjectArray str_array =
                    parse_property_change(env, msg, (Properties *)&media_player_properties,
                                sizeof(media_player_properties) / sizeof(Properties));
        env->CallVoidMethod(nat->me,
                            method_onMediaPlayerPropertyChanged,
                            str_array);
        avrcpSetMediaPlayerPropertyNative(env, nat->me, str_array);
        env->PopLocalFrame(NULL);
        return DBUS_HANDLER_RESULT_HANDLED;
    } else {
        LOGV("%s.%s is ignored", dbus_message_get_interface(msg), dbus_message_get_member(msg));
    }

    env->PopLocalFrame(NULL);
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static const DBusObjectPathVTable media_player_agent_vtable = {
    NULL, media_player_agent_event_filter, NULL, NULL, NULL, NULL
};

static int register_media_player_agent(const char *adapter_path,
                                       char * const c_metadata[],
                                       char * const c_settings[])
{
    DBusMessage *msg, *reply;
    DBusMessageIter iter;
    DBusError err;
    const char *path = BLUEZ_DBUS_MEDIA_PLAYER_PATH;

    if (!dbus_connection_register_object_path(nat->conn,
                                              BLUEZ_DBUS_MEDIA_PLAYER_PATH,
                                              &media_player_agent_vtable, nat)) {
        LOGE("%s: Can't register object path %s for media player agent!",
                                                    __FUNCTION__, BLUEZ_DBUS_MEDIA_PLAYER_PATH);
        return false;
    }

    if (adapter_path == NULL) {
        LOGE("%s: Adapter and its agent should be registered first", __FUNCTION__);
        return false;
    }

    msg = dbus_message_new_method_call(BLUEZ_DBUS_BASE_IFC, adapter_path,
                                       BLUEZ_DBUS_MEDIA_IFC, "RegisterPlayer");

    dbus_message_iter_init_append(msg, &iter);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_OBJECT_PATH, &path);

    avrcp_dbus_write_settings(&iter, c_settings);
    avrcp_dbus_write_metadata(&iter, c_metadata);

    dbus_error_init(&err);
    reply = dbus_connection_send_with_reply_and_block(nat->conn, msg, -1, &err);
    dbus_message_unref(msg);

    if (!reply) {
        LOGE("%s: Can't register media player agent!", __FUNCTION__);
        if (dbus_error_is_set(&err)) {
            LOG_AND_FREE_DBUS_ERROR(&err);
        }
        return false;
    }

    dbus_message_unref(reply);
    dbus_connection_flush(nat->conn);
    return true;
}
#endif

static jboolean avrcpRegisterMediaPlayerNative(JNIEnv *env, jobject object,
                                               jobject serviceObject,
                                               jobjectArray metadata,
                                               jobjectArray settings)
{
#ifdef HAVE_BLUETOOTH
    LOGV("%s", __FUNCTION__);
    if (nat) {
        jsize j;
        jsize n_metadata = env->GetArrayLength(metadata);
        jsize n_settings = env->GetArrayLength(settings);
        char *c_metadata[n_metadata + 1];
        char *c_settings[n_settings + 1];
        bool ret;

        n_metadata = parse_properties_array(env, object, metadata, n_metadata,
                                            c_metadata);
        c_metadata[n_metadata] = NULL;

        n_settings = parse_properties_array(env, object, settings, n_settings,
                                            c_settings);
        c_settings[n_settings] = NULL;

        const char *adapter_path = get_adapter_path(env, serviceObject);
        if (adapter_path == NULL) {
            ret = false;
            goto done;
        }

        ret = register_media_player_agent(adapter_path, c_metadata, c_settings);

done:
        for (j = 0; c_settings[j]; j++)
            free(c_settings[j]);

        for (j = 0; c_metadata[j]; j++)
            free(c_metadata[j]);

        return ret;
    }
#endif
    return JNI_FALSE;
}

static jboolean avrcpUnregisterMediaPlayerNative(JNIEnv *env, jobject object,
                                                        jobject serviceObject)
{
#ifdef HAVE_BLUETOOTH
    LOGV("%s", __FUNCTION__);
    if (nat) {
        const char *adapter_path = get_adapter_path(env, serviceObject);

        if (adapter_path == NULL)
            return JNI_FALSE;

        if (!dbus_connection_unregister_object_path(nat->conn,
                                                  BLUEZ_DBUS_MEDIA_PLAYER_PATH)) {
            LOGE("%s: Can't unregister object path %s for media player agent!",
                                         __FUNCTION__, BLUEZ_DBUS_MEDIA_PLAYER_PATH);
            return JNI_FALSE;
        }

        /*
         * We only unregister the D-Bus path. bluetoothd is going down so
         * there's no need to call the Unregister method. It could race with
         * the shutdown procedure, so we just don't call it.
         */
        return JNI_TRUE;
    }
#endif
    return JNI_FALSE;
}
#endif /*BLUETI_ENHANCEMENT*/

#ifdef HAVE_BLUETOOTH
DBusHandlerResult a2dp_event_filter(DBusMessage *msg, JNIEnv *env) {
    DBusError err;

    if (!nat) {
        ALOGV("... skipping %s\n", __FUNCTION__);
        ALOGV("... ignored\n");
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    dbus_error_init(&err);

    if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_SIGNAL) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    DBusHandlerResult result = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    if (dbus_message_is_signal(msg, "org.bluez.AudioSink",
                                      "PropertyChanged")) {
        jobjectArray str_array =
                    parse_property_change(env, msg, (Properties *)&sink_properties,
                                sizeof(sink_properties) / sizeof(Properties));
        const char *c_path = dbus_message_get_path(msg);
        jstring path = env->NewStringUTF(c_path);
        env->CallVoidMethod(nat->me,
                            method_onSinkPropertyChanged,
                            path,
                            str_array);
        env->DeleteLocalRef(path);
        result = DBUS_HANDLER_RESULT_HANDLED;
        return result;
    } else {
        ALOGV("... ignored");
    }
    if (env->ExceptionCheck()) {
        ALOGE("VM Exception occurred while handling %s.%s (%s) in %s,"
             " leaving for VM",
             dbus_message_get_interface(msg), dbus_message_get_member(msg),
             dbus_message_get_path(msg), __FUNCTION__);
    }

    return result;
}

void onConnectSinkResult(DBusMessage *msg, void *user, void *n) {
    ALOGV("%s", __FUNCTION__);

    native_data_t *nat = (native_data_t *)n;
    const char *path = (const char *)user;
    DBusError err;
    dbus_error_init(&err);
    JNIEnv *env;
    nat->vm->GetEnv((void**)&env, nat->envVer);


    bool result = JNI_TRUE;
    if (dbus_set_error_from_message(&err, msg)) {
        LOG_AND_FREE_DBUS_ERROR(&err);
        result = JNI_FALSE;
    }
    ALOGV("... Device Path = %s, result = %d", path, result);

    jstring jPath = env->NewStringUTF(path);
    env->CallVoidMethod(nat->me,
                        method_onConnectSinkResult,
                        jPath,
                        result);
    env->DeleteLocalRef(jPath);
    free(user);
}


#endif


static JNINativeMethod sMethods[] = {
    {"initNative", "()Z", (void *)initNative},
    {"cleanupNative", "()V", (void *)cleanupNative},

    /* Bluez audio 4.47 API */
    {"connectSinkNative", "(Ljava/lang/String;)Z", (void *)connectSinkNative},
    {"disconnectSinkNative", "(Ljava/lang/String;)Z", (void *)disconnectSinkNative},
    {"suspendSinkNative", "(Ljava/lang/String;)Z", (void*)suspendSinkNative},
    {"resumeSinkNative", "(Ljava/lang/String;)Z", (void*)resumeSinkNative},
    {"getSinkPropertiesNative", "(Ljava/lang/String;)[Ljava/lang/Object;",
                                    (void *)getSinkPropertiesNative},
    {"avrcpVolumeUpNative", "(Ljava/lang/String;)Z", (void*)avrcpVolumeUpNative},
    {"avrcpVolumeDownNative", "(Ljava/lang/String;)Z", (void*)avrcpVolumeDownNative},
#ifdef BLUETI_ENHANCEMENT
    {"avrcpSetMediaPlaybackStatusNative", "(Ljava/lang/String;J)Z", (void*)avrcpSetMediaPlaybackStatusNative},
    {"avrcpSetMediaNative", "([Ljava/lang/Object;)Z", (void*)avrcpSetMediaNative},
    {"avrcpSetMediaPlayerPropertyNative", "([Ljava/lang/Object;)Z", (void*)avrcpSetMediaPlayerPropertyNative},
    {"avrcpRegisterMediaPlayerNative", "(Ljava/lang/Object;[Ljava/lang/Object;[Ljava/lang/Object;)Z", (void*)avrcpRegisterMediaPlayerNative},
    {"avrcpUnregisterMediaPlayerNative", "(Ljava/lang/Object;)Z", (void*)avrcpUnregisterMediaPlayerNative},
#endif /*BLUETI_ENHANCEMENT*/
};

int register_android_server_BluetoothA2dpService(JNIEnv *env) {
    jclass clazz = env->FindClass("android/server/BluetoothA2dpService");
    if (clazz == NULL) {
        ALOGE("Can't find android/server/BluetoothA2dpService");
        return -1;
    }

#ifdef BLUETI_ENHANCEMENT
    jclass service_clazz = env->FindClass("android/server/BluetoothService");
    if (service_clazz == NULL) {
        LOGE("Can't find android/server/BluetoothService");
        return -1;
    }
#endif /*BLUETI_ENHANCEMENT*/

#ifdef HAVE_BLUETOOTH
    method_onSinkPropertyChanged = env->GetMethodID(clazz, "onSinkPropertyChanged",
                                          "(Ljava/lang/String;[Ljava/lang/String;)V");
    method_onConnectSinkResult = env->GetMethodID(clazz, "onConnectSinkResult",
                                                         "(Ljava/lang/String;Z)V");
#ifdef BLUETI_ENHANCEMENT
    method_onMediaPlayerPropertyChanged = env->GetMethodID(clazz, "onMediaPlayerPropertyChanged",
                                          "([Ljava/lang/String;)V");

    field_mEventLoop = get_field(env, service_clazz, "mEventLoop",
                                 "Landroid/server/BluetoothEventLoop;");
#endif /*BLUETI_ENHANCEMENT*/

#endif

    return AndroidRuntime::registerNativeMethods(env,
                "android/server/BluetoothA2dpService", sMethods, NELEM(sMethods));
}

} /* namespace android */
