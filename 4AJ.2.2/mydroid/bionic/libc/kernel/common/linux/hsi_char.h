/****************************************************************************
 ****************************************************************************
 ***
 ***   This header was automatically generated from a Linux kernel header
 ***   of the same name, to make information necessary for userspace to
 ***   call into the kernel available to libc.  It contains only constants,
 ***   structures, and macros generated from the original header, and thus,
 ***   contains no copyrightable information.
 ***
 ***   To edit the content of this header, modify the corresponding
 ***   source file (e.g. under external/kernel-headers/original/) then
 ***   run bionic/libc/kernel/tools/update_all.py
 ***
 ***   Any manual change here will be lost the next time this script will
 ***   be run. You've been warned!
 ***
 ****************************************************************************
 ****************************************************************************/
#ifndef HSI_CHAR_H
#define HSI_CHAR_H
#define HSI_CHAR_BASE 'S'
#define CS_IOW(num, dtype) _IOW(HSI_CHAR_BASE, num, dtype)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define CS_IOR(num, dtype) _IOR(HSI_CHAR_BASE, num, dtype)
#define CS_IOWR(num, dtype) _IOWR(HSI_CHAR_BASE, num, dtype)
#define CS_IO(num) _IO(HSI_CHAR_BASE, num)
#define CS_SEND_BREAK CS_IO(1)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define CS_FLUSH_RX CS_IOR(2, size_t)
#define CS_FLUSH_TX CS_IOR(3, size_t)
#define CS_BOOTSTRAP CS_IO(4)
#define CS_SET_ACWAKELINE CS_IOW(5, unsigned int)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define CS_GET_ACWAKELINE CS_IOR(6, unsigned int)
#define CS_SET_RX CS_IOW(7, struct hsi_rx_config)
#define CS_GET_RX CS_IOW(8, struct hsi_rx_config)
#define CS_SET_TX CS_IOW(9, struct hsi_tx_config)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define CS_GET_TX CS_IOW(10, struct hsi_tx_config)
#define CS_SW_RESET CS_IO(11)
#define CS_GET_FIFO_OCCUPANCY CS_IOR(12, size_t)
#define CS_GET_CAWAKELINE CS_IOR(13, unsigned int)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define CS_SET_WAKE_RX_3WIRES_MODE CS_IOR(14, unsigned int)
#define CS_SET_HI_SPEED CS_IOR(15, unsigned int)
#define CS_GET_SPEED CS_IOW(16, unsigned long)
#define HSI_MODE_SLEEP 0
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define HSI_MODE_STREAM 1
#define HSI_MODE_FRAME 2
#define HSI_ARBMODE_RR 0
#define HSI_ARBMODE_PRIO 1
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define WAKE_UP 1
#define WAKE_DOWN 0
struct hsi_tx_config {
 __u32 mode;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 __u32 flow;
 __u32 frame_size;
 __u32 channels;
 __u32 divisor;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 __u32 arb_mode;
};
struct hsi_rx_config {
 __u32 mode;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 __u32 flow;
 __u32 frame_size;
 __u32 channels;
 __u32 divisor;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 __u32 counters;
};
#endif
