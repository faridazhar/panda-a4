export P4_PATH=`pwd`/../../../4AJ.2.2
export P4_MYDROID=${P4_PATH}/mydroid
export PATH=${P4_PATH}/toolchain/arm-2010q1/bin:$PATH
export CROSS_COMPILE=arm-none-linux-gnueabi-
export PATH=${P4_PATH}/u-boot/tools:$PATH
export P4_KERNEL_DIR=${P4_PATH}/kernel/android-3.0
export KLIB=${P4_KERNEL_DIR}
export KLIB_BUILD=${P4_KERNEL_DIR}
export P4_BOARD_TYPE=panda

## make ARCH=arm panda_defconfig

def_config=panda_defconfig

echo -e "\033[1;32m------ Creating a .config file based on ${def_config} ------\033[m"
make ARCH=arm $def_config
echo -e "\033[1;32m------ DONE Creating a .config file based on ${def_config} ------\033[m"

