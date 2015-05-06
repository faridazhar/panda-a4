#Artifacts associated with S3D support

# S3D platform library
PRODUCT_PACKAGES += \
    com.ti.s3d \
	libs3dview_jni

PRODUCT_COPY_FILES += \
	device/ti/common-open/s3d/frameworks/com.ti.s3d.xml:system/etc/permissions/com.ti.s3d.xml

# S3D Apps
PRODUCT_PACKAGES += \
    S3DCowboids