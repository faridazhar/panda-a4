LOCAL_PATH:= $(call my-dir)

ifeq ($(ARCH_ARM_HAVE_NEON),true)
ANDROID_JPEG_USE_VENUM :=true
endif

include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm

LOCAL_SRC_FILES := \
	jcapimin.c jcapistd.c jccoefct.c jccolor.c jcdctmgr.c jchuff.c \
	jcinit.c jcmainct.c jcmarker.c jcmaster.c jcomapi.c jcparam.c \
	jcphuff.c jcprepct.c jcsample.c jctrans.c jdapimin.c jdapistd.c \
	jdatadst.c jdatasrc.c jdcoefct.c jdcolor.c jddctmgr.c jdhuff.c \
	jdinput.c jdmainct.c jdmarker.c jdmaster.c jdmerge.c jdphuff.c \
	jdpostct.c jdsample.c jdtrans.c jerror.c jfdctflt.c jfdctfst.c \
	jfdctint.c jidctflt.c jquant1.c \
	jquant2.c jutils.c jmemmgr.c

# use ashmem as libjpeg decoder's backing store
LOCAL_CFLAGS += -DUSE_ANDROID_ASHMEM
LOCAL_SRC_FILES += \
	jmem-ashmem.c

# the original android memory manager.
# use sdcard as libjpeg decoder's backing store
#LOCAL_SRC_FILES += \
#	jmem-android.c

# the assembler is only for the ARM version, don't break the Linux sim
ifneq ($(TARGET_ARCH),arm)
ANDROID_JPEG_NO_ASSEMBLER := true
endif

# temp fix until we understand why this broke cnn.com
#ANDROID_JPEG_NO_ASSEMBLER := true

ifeq ($(strip $(ANDROID_JPEG_NO_ASSEMBLER)),true)
LOCAL_SRC_FILES += jidctint.c jidctfst.c jidctred.c
else
ifeq ($(ANDROID_JPEG_USE_VENUM),true)
LOCAL_SRC_FILES += jidctvenum.c jdcolor-neon.S jidct-neon.S armv6_idct.S
else
LOCAL_SRC_FILES += jidctint.c jidctfst.c jidctred.c armv6_idct.S
endif
endif

LOCAL_CFLAGS += -DAVOID_TABLES 
LOCAL_CFLAGS += -O3 -fstrict-aliasing -fprefetch-loop-arrays
ifeq ($(ANDROID_JPEG_USE_VENUM),true)
LOCAL_CFLAGS += -D__ARM_HAVE_NEON
endif
#LOCAL_CFLAGS += -march=armv6j

# enable tile based decode
LOCAL_CFLAGS += -DANDROID_TILE_BASED_DECODE

# enable armv6 idct assembly
LOCAL_CFLAGS += -DANDROID_ARMV6_IDCT

LOCAL_MODULE:= libjpeg

LOCAL_SHARED_LIBRARIES := \
	libcutils

include $(BUILD_SHARED_LIBRARY)
