# List of apps and optional libraries (Java and native) to put in the add-on system image.
PRODUCT_PACKAGES := \
    com.ti.s3d \
    libs3dview_jni \
    S3DCowboids

# name of the add-on
PRODUCT_SDK_ADDON_NAME := s3d

# Manually copy the optional library XML files in the system image.
PRODUCT_COPY_FILES := \
    device/ti/common-open/s3d/frameworks/com.ti.s3d.xml:system/etc/permissions/com.ti.s3d.xml

# Copy the manifest and hardware files for the SDK add-on.
# The content of those files is manually created for now.
PRODUCT_SDK_ADDON_COPY_FILES := \
    device/ti/common-open/s3d/sdk_addon/manifest.ini:manifest.ini \
    device/ti/common-open/s3d/sdk_addon/hardware.ini:hardware.ini \
    $(call find-copy-subdir-files,*,device/ti/common-open/s3d/apps/Cowboids,samples/S3DCowboids)

# Copy the jar files for the optional libraries that are exposed as APIs.
PRODUCT_SDK_ADDON_COPY_MODULES := \
    com.ti.s3d:libs/s3d.jar

# Build system considers this a global variable, not a part of this product def
PRODUCT_SDK_ADDON_STUB_DEFS += $(LOCAL_PATH)/stub_defs.txt

# Name of the doc to generate and put in the add-on. This must match the name defined
# in the optional library with the tag
#    LOCAL_MODULE:= platform_library
# in the documentation section.
PRODUCT_SDK_ADDON_DOC_MODULES := s3d_api

# This add-on extends the default sdk product.
$(call inherit-product, $(SRC_TARGET_DIR)/product/sdk.mk)

# Real name of the add-on. This is the name used to build the add-on.
# Use 'make PRODUCT-s3d-sdk_addon' to build the add-on.
PRODUCT_NAME := s3d
