# Copyright (C) 2009-2011 Texas Instruments, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

ifeq ($(TARGET_ANDROID_VERSION),ICS)
BUILD_ANW:=1
else ifeq ($(TARGET_ANDROID_VERSION), JELLYBEAN)
BUILD_ANW:=1
endif

ifeq ($(BUILD_ANW),1)

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false
LOCAL_ARM_MODE := arm
LOCAL_CPPFLAGS := $(DVP_DEBUGGING) $(DVP_CPPFLAGS)
LOCAL_SRC_FILES := anativewindow.cpp
LOCAL_C_INCLUDES := $(DVP_INCLUDES)
LOCAL_MODULE := libanw
include $(BUILD_STATIC_LIBRARY)

endif

