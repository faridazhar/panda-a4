export P4_PATH=`pwd`/../../../4AJ.2.2
export P4_MYDROID=${P4_PATH}/mydroid
export PATH=${P4_PATH}/toolchain/arm-2010q1/bin:$PATH
export CROSS_COMPILE=arm-none-linux-gnueabi-
export PATH=${P4_PATH}/u-boot/tools:$PATH
export P4_KERNEL_DIR=${P4_PATH}/kernel/android-3.0
export KLIB=${P4_KERNEL_DIR}
export KLIB_BUILD=${P4_KERNEL_DIR}
export P4_BOARD_TYPE=panda

LIB_OUT_DIR=${P4_MYDROID}/out/target/product/$P4_BOARD_TYPE/system/lib/modules/

if [ -n "$1" ]; then
    mkdir -p $1
fi

echo "build wifi module---------------------------"
## //FA fails. 
cd ${P4_MYDROID}/hardware/ti/wlan/mac80211/compat_wl12xx
make -j$(egrep '^processor' /proc/cpuinfo | wc -l)  ARCH=arm 2>&1 | tee ${P4_MYDROID}/logs/wilink_wifi.out

if [ -n "$1" ]; then
    #cp ./compat/compat.ko $OUT
    cp ./net/wireless/cfg80211.ko $1
    cp ./net/mac80211/mac80211.ko $1
    cp ./drivers/net/wireless/wl12xx/wl12xx.ko $1
    cp ./drivers/net/wireless/wl12xx/wl12xx_sdio.ko $1
fi

echo "build bluetooth module---------------------"
cd ${P4_MYDROID}/hardware/ti/wpan/bluetooth-compat
make -j$(egrep '^processor' /proc/cpuinfo | wc -l)  ARCH=arm 2>&1 | tee ${P4_MYDROID}/logs/wilink_bt.out

if [ -n "$1" ]; then
    cp ./compat/sch_fq_codel.ko $1
    cp ./compat/sch_codel.ko $1
    cp ./compat/compat.ko $1
    cp ./drivers/bluetooth/btwilink.ko $1
    cp ./net/bluetooth/bnep/bnep.ko $1
    cp ./net/bluetooth/hidp/hidp.ko $1
    cp ./net/bluetooth/rfcomm/rfcomm.ko $1
    cp ./net/bluetooth/bluetooth.ko $1
    #find . -name \*.ko |while read f; do cp $f $1; done
fi

cd ${P4_KERNEL_DIR}

