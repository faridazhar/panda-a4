LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)



LOCAL_SRC_FILES:= \
    com_android_nfc_NativeLlcpConnectionlessSocket.cpp \
    com_android_nfc_NativeLlcpServiceSocket.cpp \
    com_android_nfc_NativeLlcpSocket.cpp \
    com_android_nfc_NativeNfcManager.cpp \
    com_android_nfc_NativeNfcTag.cpp \
    com_android_nfc_NativeP2pDevice.cpp \
    com_android_nfc_NativeNfcSecureElement.cpp \
    com_android_nfc_list.cpp \
    com_android_nfc.cpp

LOCAL_C_INCLUDES += \
    $(JNI_H_INCLUDE)
ifneq ($(NFC_TI_DEVICE), true)
    LOCAL_C_INCLUDES += external/libnfc-nxp/src \
    external/libnfc-nxp/inc
else
    LOCAL_C_INCLUDES += external/libnfc-ti/src \
    external/libnfc-ti/inc
endif
LOCAL_SHARED_LIBRARIES := \
    libnativehelper \
    libcutils \
    libutils \
    libnfc
ifneq ($(NFC_TI_DEVICE), true)
    LOCAL_SHARED_LIBRARIES +=libhardware
endif

#LOCAL_CFLAGS += -O0 -g

LOCAL_MODULE := libnfc_jni
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
