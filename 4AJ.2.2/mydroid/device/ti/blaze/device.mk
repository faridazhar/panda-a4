#
# Copyright (C) 2011 Texas Instruments Inc.
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
include device/ti/blaze/Config.mk

DEVICE_PACKAGE_OVERLAYS := device/ti/blaze/overlay

ifeq ($(TARGET_PREBUILT_KERNEL),)
LOCAL_KERNEL := device/ti/blaze/boot/zImage
else
LOCAL_KERNEL := $(TARGET_PREBUILT_KERNEL)
endif

#to flow down to ti-wpan-products.mk
BLUETI_ENHANCEMENT := true

PRODUCT_COPY_FILES := \
	$(LOCAL_KERNEL):kernel \
        device/ti/blaze/boot/fastboot.sh:fastboot.sh \
	device/ti/blaze/boot/fastboot:fastboot \
        $(LOCAL_KERNEL):boot/zImage \
        device/ti/blaze/boot/MLO_es2.2_emu:boot/MLO_es2.2_emu \
        device/ti/blaze/boot/MLO_es2.2_gp:boot/MLO_es2.2_gp \
        device/ti/blaze/boot/u-boot.bin:boot/u-boot.bin \
	device/ti/blaze/init.omap4blazeboard.rc:root/init.omap4blazeboard.rc \
	device/ti/blaze/init.omap4blazeboard.usb.rc:root/init.omap4blazeboard.usb.rc \
	device/ti/blaze/ueventd.omap4blazeboard.rc:root/ueventd.omap4blazeboard.rc \
	device/ti/blaze/omap4-keypad.kl:system/usr/keylayout/omap4-keypad.kl \
	device/ti/blaze/omap4-keypad.kcm:system/usr/keychars/omap4-keypad.kcm \
        device/ti/blaze/twl6030_pwrbutton.kl:system/usr/keylayout/twl6030_pwrbutton.kl \
	device/ti/common-open/audio/audio_policy.conf:system/etc/audio_policy.conf \
	device/ti/blaze/media_profiles.xml:system/etc/media_profiles.xml \
        device/ti/blaze/media_codecs.xml:system/etc/media_codecs.xml \
        device/ti/blaze/syn_tm12xx_ts_1.idc:system/usr/idc/syn_tm12xx_ts_1.idc \
        device/ti/blaze/syn_tm12xx_ts_2.idc:system/usr/idc/syn_tm12xx_ts_2.idc

# to mount the external storage (sdcard)
PRODUCT_COPY_FILES += \
        device/ti/blaze/vold.fstab:system/etc/vold.fstab

# These are the hardware-specific features
PRODUCT_COPY_FILES += \
	frameworks/native/data/etc/android.hardware.sensor.compass.xml:system/etc/permissions/android.hardware.sensor.compass.xml \
	frameworks/native/data/etc/android.hardware.camera.xml:system/etc/permissions/android.hardware.camera.xml \
	frameworks/native/data/etc/android.hardware.camera.autofocus.xml:system/etc/permissions/android.hardware.camera.autofocus.xml \
	frameworks/native/data/etc/android.hardware.camera.flash-autofocus.xml:system/etc/permissions/android.hardware.camera.flash-autofocus.xml \
	frameworks/native/data/etc/android.hardware.camera.front.xml:system/etc/permissions/android.hardware.camera.front.xml \
	frameworks/native/data/etc/handheld_core_hardware.xml:system/etc/permissions/handheld_core_hardware.xml \
	frameworks/native/data/etc/android.hardware.telephony.gsm.xml:system/etc/permissions/android.hardware.telephony.gsm.xml \
	frameworks/native/data/etc/android.software.sip.voip.xml:system/etc/permissions/android.software.sip.voip.xml \
	device/ti/blaze/android.hardware.location.gps.xml:system/etc/permissions/android.hardware.location.gps.xml \
        frameworks/native/data/etc/android.hardware.nfc.xml:system/etc/permissions/android.hardware.nfc.xml \
        frameworks/base/nfc-extras/com.android.nfc_extras.xml:system/etc/permissions/com.android.nfcextras.xml \
        frameworks/native/data/etc/com.nxp.mifare.xml:system/etc/permissions/com.nxp.mifare.xml \
        device/ti/blaze/nfcee_access.xml:system/etc/nfcee_access.xml \
	frameworks/native/data/etc/android.hardware.usb.host.xml:system/etc/permissions/android.hardware.usb.host.xml \
	frameworks/native/data/etc/android.hardware.usb.accessory.xml:system/etc/permissions/android.hardware.usb.accessory.xml \
	frameworks/native/data/etc/android.hardware.sensor.barometer.xml:system/etc/permissions/android.hardware.sensor.barometer.xml \
	frameworks/native/data/etc/android.hardware.wifi.xml:system/etc/permissions/android.hardware.wifi.xml \
	frameworks/native/data/etc/android.hardware.wifi.direct.xml:system/etc/permissions/android.hardware.wifi.direct.xml \
	device/ti/blaze/android.hardware.bluetooth.xml:system/etc/permissions/android.hardware.bluetooth.xml \
	frameworks/native/data/etc/android.hardware.sensor.proximity.xml:system/etc/permissions/android.hardware.sensor.proximity.xml \
	frameworks/native/data/etc/android.hardware.sensor.accelerometer.xml:system/etc/permissions/android.hardware.sensor.accelerometer.xml \
	frameworks/native/data/etc/android.hardware.sensor.light.xml:system/etc/permissions/android.hardware.sensor.light.xml \
	frameworks/native/data/etc/android.hardware.touchscreen.multitouch.jazzhand.xml:system/etc/permissions/android.hardware.touchscreen.multitouch.jazzhand.xml \
	packages/wallpapers/LivePicker/android.software.live_wallpaper.xml:system/etc/permissions/android.software.live_wallpaper.xml

PRODUCT_PACKAGES := \
    ti_omap4_ducati_bins \
    libOMX_Core \
    libOMX.TI.DUCATI1.VIDEO.DECODER

# Tiler
PRODUCT_PACKAGES += \
    libtimemmgr

#Lib Skia test
PRODUCT_PACKAGES += \
    SkLibTiJpeg_Test

# Camera
PRODUCT_PACKAGES += \
    Camera \
    CameraOMAP \
    camera_test

PRODUCT_PACKAGES += \
	com.android.future.usb.accessory

PRODUCT_PACKAGES += \
	boardidentity \
	libboardidentity \
	libboard_idJNI \
	Board_id

PRODUCT_PROPERTY_OVERRIDES := \
	hwui.render_dirty_regions=false

PRODUCT_DEFAULT_PROPERTY_OVERRIDES += \
	persist.sys.usb.config=mtp

PRODUCT_PROPERTY_OVERRIDES += \
	ro.opengles.version=131072

PRODUCT_PROPERTY_OVERRIDES += \
	ro.sf.lcd_density=240

PRODUCT_TAGS += dalvik.gc.type-precise

PRODUCT_PACKAGES += \
	librs_jni \
	com.android.future.usb.accessory

# Filesystem management tools
PRODUCT_PACKAGES += \
	make_ext4fs

# Generated kcm keymaps
PRODUCT_PACKAGES += \
        omap4-keypad.kcm

# WI-Fi
PRODUCT_PACKAGES += \
	dhcpcd.conf \
	hostapd.conf \
	wifical.sh \
	wilink7.sh \
	TQS_D_1.7.ini \
	TQS_D_1.7_127x.ini \
	crda \
	regulatory.bin \
	calibrator

# Add modem scripts
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/modem-detect.sh:system/vendor/bin/modem-detect.sh
# Audio HAL module
PRODUCT_PACKAGES += audio.primary.omap4
PRODUCT_PACKAGES += audio.hdmi.omap4

# tinyalsa utils
PRODUCT_PACKAGES += \
	tinymix \
	tinyplay \
	tinycap

# BlueZ a2dp Audio HAL module
PRODUCT_PACKAGES += audio.a2dp.default

# Audioout libs
PRODUCT_PACKAGES += libaudioutils

# Lights
PRODUCT_PACKAGES += \
        lights.blaze

# Sensors
PRODUCT_PACKAGES += \
        sensors.blaze \
        sensor.test

# BlueZ test tools & Shared Transport user space mgr
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

# SMC components for secure services like crypto, secure storage
PRODUCT_PACKAGES += \
        smc_pa.ift \
        smc_normal_world_android_cfg.ini \
        libsmapi.so \
        libtf_crypto_sst.so \
        libtfsw_jce_provider.so \
        tfsw_jce_provider.jar \
        tfctrl

# Enable AAC 5.1 decode (decoder)
PRODUCT_PROPERTY_OVERRIDES += \
	media.aac_51_output_enabled=true

# for bugmailer
PRODUCT_PACKAGES += send_bug
PRODUCT_COPY_FILES += \
	system/extras/bugmailer/bugmailer.sh:system/bin/bugmailer.sh \
	system/extras/bugmailer/send_bug:system/bin/send_bug

PRODUCT_PACKAGES += \
	blaze_hdcp_keys

$(call inherit-product, frameworks/native/build/tablet-dalvik-heap.mk)
$(call inherit-product, hardware/ti/omap4xxx/omap4.mk)
$(call inherit-product-if-exists, hardware/ti/wpan/ti-wpan-products.mk)
$(call inherit-product-if-exists, vendor/ti/blaze/device-vendor.mk)
$(call inherit-product-if-exists, device/ti/proprietary-open/omap4/ti-omap4-vendor.mk)
$(call inherit-product-if-exists, device/ti/proprietary-open/wl12xx/wlan/wl12xx-wlan-fw-products.mk)
$(call inherit-product-if-exists, device/ti/common-open/s3d/s3d-products.mk)
$(call inherit-product-if-exists, device/ti/proprietary-open/omap4/ducati-full_blaze.mk)
$(call inherit-product-if-exists, device/ti/proprietary-open/omap4/dsp_fw.mk)
$(call inherit-product-if-exists, device/modem-vendor/device.mk)
$(call inherit-product-if-exists, hardware/ti/dvp/dvp-products.mk)
$(call inherit-product-if-exists, hardware/ti/arx/arx-products.mk)

# clear OMAP_ENHANCEMENT variables
$(call ti-clear-vars)
