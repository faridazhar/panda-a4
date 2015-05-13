export P4_PATH=`pwd`/../../../4AJ.2.2
export P4_MYDROID=${P4_PATH}/mydroid
export PATH=${P4_PATH}/toolchain/arm-2010q1/bin:$PATH
export CROSS_COMPILE=arm-none-linux-gnueabi-
export PATH=${P4_PATH}/u-boot/tools:$PATH
export P4_KERNEL_DIR=${P4_PATH}/kernel/android-3.0
export KLIB=${P4_KERNEL_DIR}
export KLIB_BUILD=${P4_KERNEL_DIR}
export P4_BOARD_TYPE=panda

make -j$(egrep '^processor' /proc/cpuinfo | wc -l) ARCH=arm uImage 2>&1 | tee ${P4_MYDROID}/logs/kernel_make.out
cd ${P4_KERNEL_DIR}


