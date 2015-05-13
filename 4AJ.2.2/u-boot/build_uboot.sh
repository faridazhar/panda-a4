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

def_config=omap44XXpanda_config

cd ${UBOOT_PATH}

echo -e "\033[1;32m------ Cleaning uBoot ------\033[m"
make distclean
echo -e "\033[1;32m------ DONE Cleaning uBoot ------\033[m"

echo -e "\033[1;32m------ Building uBoot (${def_config}) ------\033[m"
make ARCH=arm $def_config
make -j$(egrep '^processor' /proc/cpuinfo | wc -l) 2>&1 |tee ${P4_MYDROID}/logs/u-boot_make.out
echo -e "\033[1;32m------ DONE Building uBoot (${def_config}) ------\033[m"

exit 0

