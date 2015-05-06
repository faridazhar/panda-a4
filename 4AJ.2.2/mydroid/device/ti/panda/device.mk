#
# Copyright (C) 2011 The Android Open-Source Project
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

# define OMAP_ENHANCEMENT variables
include device/ti/panda/Config.mk

ifeq ($(TARGET_PREBUILT_KERNEL),)
LOCAL_KERNEL := device/ti/panda/kernel
else
LOCAL_KERNEL := $(TARGET_PREBUILT_KERNEL)
endif

DEVICE_PACKAGE_OVERLAYS := device/ti/panda/overlay

# This device is xhdpi.  However the platform doesn't
# currently contain all of the bitmaps at xhdpi density so
# we do this little trick to fall back to the hdpi version
# if the xhdpi doesn't exist.
PRODUCT_AAPT_CONFIG := normal hdpi xhdpi
PRODUCT_AAPT_PREF_CONFIG := xhdpi

PRODUCT_COPY_FILES := \
	$(LOCAL_KERNEL):kernel \
	device/ti/panda/init.omap4pandaboard.rc:root/init.omap4pandaboard.rc \
	device/ti/panda/init.omap4pandaboard.usb.rc:root/init.omap4pandaboard.usb.rc \
        device/ti/panda/fstab.omap4pandaboard:root/fstab.omap4pandaboard \
	device/ti/panda/ueventd.omap4pandaboard.rc:root/ueventd.omap4pandaboard.rc \
	device/ti/panda/media_profiles.xml:system/etc/media_profiles.xml \
	device/ti/panda/android.hardware.bluetooth.xml:system/etc/permissions/android.hardware.bluetooth.xml \
	frameworks/native/data/etc/android.hardware.usb.host.xml:system/etc/permissions/android.hardware.usb.host.xml \
	frameworks/native/data/etc/android.hardware.wifi.xml:system/etc/permissions/android.hardware.wifi.xml \
   frameworks/native/data/etc/android.hardware.usb.accessory.xml:system/etc/permissions/android.hardware.usb.accessory.xml \
   packages/wallpapers/LivePicker/android.software.live_wallpaper.xml:system/etc/permissions/android.software.live_wallpaper.xml

PRODUCT_PACKAGES := \
   ti_omap4_ducati_bins \
   libOMX_Core \
   libOMX.TI.DUCATI1.VIDEO.DECODER

# Tiler
PRODUCT_PACKAGES += \
   libtimemmgr

# HWC HAL
PRODUCT_PACKAGES += \
   hwcomposer.omap4

PRODUCT_PACKAGES += \
	com.android.future.usb.accessory

PRODUCT_PACKAGES += \
   boardidentity \
   libboardidentity \
   libboard_idJNI \
   Board_id

PRODUCT_PROPERTY_OVERRIDES := \
	wifi.interface=wlan0 \
	hwui.render_dirty_regions=false

PRODUCT_CHARACTERISTICS := tablet,nosdcard

#HWC Hal
PRODUCT_PACKAGES += \
    hwcomposer.omap4

PRODUCT_TAGS += dalvik.gc.type-precise

PRODUCT_PACKAGES += \
	librs_jni \
	com.android.future.usb.accessory

# Wi-Fi
PRODUCT_PACKAGES += \
	dhcpcd.conf \
	hostapd.conf \
	wifical.sh \
	TQS_D_1.7.ini \
	TQS_D_1.7_127x.ini \
	crda \
	regulatory.bin \
	calibrator

# Audio
PRODUCT_PACKAGES += \
	audio.primary.panda \
	audio.a2dp.default

PRODUCT_COPY_FILES += \
	device/ti/panda/audio/audio_policy.conf:system/etc/audio_policy.conf

PRODUCT_PACKAGES += \
	dhcpcd.conf \
	TQS_D_1.7.ini \
	calibrator

# Filesystem management tools
PRODUCT_PACKAGES += \
	make_ext4fs

# Audio HAL module
PRODUCT_PACKAGES += \
	audio.primary.panda

# Tinyalsa utils
PRODUCT_PACKAGES += \
	tinymix \
	tinyplay \
	tinycap

# BlueZ test tools
PRODUCT_PACKAGES += \
	hciconfig \
	hcitool

# Live Wallpapers
PRODUCT_PACKAGES += \
	LiveWallpapers \
	LiveWallpapersPicker \
	MagicSmokeWallpapers \
	VisualizationWallpapers \
	librs_jni

$(call inherit-product-if-exists, vendor/ti/panda/device-vendor.mk)
$(call inherit-product, frameworks/native/build/tablet-dalvik-heap.mk)
$(call inherit-product, hardware/ti/omap4xxx/omap4.mk)
$(call inherit-product-if-exists, vendor/ti/proprietary/omap4/ti-omap4-vendor.mk)
$(call inherit-product, device/ti/panda/wl12xx/ti-wl12xx-vendor.mk)
$(call inherit-product, device/ti/panda/wl12xx/ti-wpan-products.mk)
$(call inherit-product-if-exists, vendor/ti/panda/device-vendor.mk)
$(call inherit-product-if-exists, device/ti/proprietary-open/omap4/ti-omap4-vendor.mk)
$(call inherit-product-if-exists, device/ti/proprietary-open/wl12xx/wlan/wl12xx-wlan-fw-products.mk)
$(call inherit-product-if-exists, device/ti/proprietary-open/omap4/ducati-full_panda.mk)
