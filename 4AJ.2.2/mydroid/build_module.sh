export P4_PATH=`pwd`/../../4AJ.2.2
export P4_MYDROID=${P4_PATH}/mydroid
export PATH=${P4_PATH}/toolchain/arm-2010q1/bin:$PATH
export CROSS_COMPILE=arm-none-linux-gnueabi-
export PATH=${P4_PATH}/u-boot/tools:$PATH
export P4_KERNEL_DIR=${P4_PATH}/kernel/android-3.0
export KLIB=${P4_KERNEL_DIR}
export KLIB_BUILD=${P4_KERNEL_DIR}
export P4_BOARD_TYPE=panda
export XLOADER_PATH=${P4_PATH}/x-loader
export UBOOT_PATH=${P4_PATH}/u-boot

cd ${P4_MYDROID}

if [ -n "$1" ]; then
    echo "Build modules--------------------------"
    source ./build/envsetup.sh
    lunch full_panda-eng
    ## lunch full_panda-userdebug
    make -j$(egrep '^processor' /proc/cpuinfo | wc -l) $1 2>&1 |tee ${P4_MYDROID}/logs/afs_modules.out
else
    echo "err: need to provide module name--------------------------"
fi

