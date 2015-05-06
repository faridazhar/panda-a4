#
# Copyright (C) 2011 The Android Open Source Project
# Copyright (C) 2010-2012 Texas Instruments, Inc. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#


ifeq ($(findstring omap, $(TARGET_BOARD_PLATFORM)),omap)

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_SHARED_LIBRARIES := libandroid_runtime
LOCAL_JAVA_LIBRARIES := framework
LOCAL_SRC_FILES := src/board_id/com/ti/Board_id_Activity.java \
		src/board_id/com/ti/BoardIDService.java \
		src/board_id/com/ti/IBoardIDService.aidl
LOCAL_PACKAGE_NAME := Board_id
LOCAL_CERTIFICATE := platform
LOCAL_SDK_VERSION := current
include $(BUILD_PACKAGE)

include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := src/board_id/com/ti/BoardIDAgent.java \
		   src/board_id/com/ti/IBoardIDService.aidl
LOCAL_MODULE := board_id.com.ti
LOCAL_JNI_SHARED_LIBRARIES := libboard_idJNI
LOCAL_CERTIFICATE := platform
LOCAL_SDK_VERSION := current
include $(BUILD_STATIC_JAVA_LIBRARY)

endif
