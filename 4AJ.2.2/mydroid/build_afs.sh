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
export YOUR_NAME=`whoami`

cd ${P4_MYDROID}

echo -e "\033[1;32m------ Building AFS ------\033[m"

source ./build/envsetup.sh

lunch full_panda-eng
## lunch full_panda-userdebug

if [[ "$1" == *c* ]]
then
  echo -e "\033[1;32mmake clean Build\033[m"
  make clean
fi

make -j$(egrep '^processor' /proc/cpuinfo | wc -l) 2>&1 | tee ${P4_MYDROID}/logs/afs_make.out

if [ ! $? -eq 0 ] ;then
    echo -e "\033[1;31mERROR: make -j1 failed\033[m"
    exit $?
fi

echo -e "\033[1;32m------ DONE Building AFS ------\033[m"

exit 0

