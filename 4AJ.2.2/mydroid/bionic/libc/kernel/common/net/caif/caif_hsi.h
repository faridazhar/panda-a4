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
#ifndef CAIF_HSI_H_
#define CAIF_HSI_H_
#include <net/caif/caif_layer.h>
#include <net/caif/caif_device.h>
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#include <linux/atomic.h>
#define CFHSI_MAX_PKTS 15
#define CFHSI_MAX_EMB_FRM_SZ 96
#define CFHSI_DBG_PREFILL 0
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#pragma pack(1)  
struct cfhsi_desc {
 u8 header;
 u8 offset;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 u16 cffrm_len[CFHSI_MAX_PKTS];
 u8 emb_frm[CFHSI_MAX_EMB_FRM_SZ];
};
#pragma pack()  
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define CFHSI_DESC_SZ (sizeof(struct cfhsi_desc))
#define CFHSI_DESC_SHORT_SZ (CFHSI_DESC_SZ - CFHSI_MAX_EMB_FRM_SZ)
#define CFHSI_MAX_CAIF_FRAME_SZ 4096
#define CFHSI_MAX_PAYLOAD_SZ (CFHSI_MAX_PKTS * CFHSI_MAX_CAIF_FRAME_SZ)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define CFHSI_BUF_SZ_TX (CFHSI_DESC_SZ + CFHSI_MAX_PAYLOAD_SZ)
#define CFHSI_BUF_SZ_RX (2 * CFHSI_DESC_SZ + CFHSI_MAX_PAYLOAD_SZ)
#define CFHSI_PIGGY_DESC (0x01 << 7)
#define CFHSI_TX_STATE_IDLE 0
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define CFHSI_TX_STATE_XFER 1
#define CFHSI_RX_STATE_DESC 0
#define CFHSI_RX_STATE_PAYLOAD 1
#define CFHSI_WAKE_UP 0
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define CFHSI_WAKE_UP_ACK 1
#define CFHSI_WAKE_DOWN_ACK 2
#define CFHSI_AWAKE 3
#define CFHSI_WAKELOCK_HELD 4
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define CFHSI_SHUTDOWN 5
#define CFHSI_FLUSH_FIFO 6
#ifndef CFHSI_INACTIVITY_TOUT
#define CFHSI_INACTIVITY_TOUT (1 * HZ)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#endif
#ifndef CFHSI_WAKE_TOUT
#define CFHSI_WAKE_TOUT (3 * HZ)
#endif
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#ifndef CFHSI_MAX_RX_RETRIES
#define CFHSI_MAX_RX_RETRIES (10 * HZ)
#endif
#ifndef CFHSI_RX_GRACE_PERIOD
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define CFHSI_RX_GRACE_PERIOD (1 * HZ)
#endif
struct cfhsi_drv {
 void (*tx_done_cb) (struct cfhsi_drv *drv);
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 void (*rx_done_cb) (struct cfhsi_drv *drv);
 void (*wake_up_cb) (struct cfhsi_drv *drv);
 void (*wake_down_cb) (struct cfhsi_drv *drv);
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct cfhsi_dev {
 int (*cfhsi_up) (struct cfhsi_dev *dev);
 int (*cfhsi_down) (struct cfhsi_dev *dev);
 int (*cfhsi_tx) (u8 *ptr, int len, struct cfhsi_dev *dev);
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 int (*cfhsi_rx) (u8 *ptr, int len, struct cfhsi_dev *dev);
 int (*cfhsi_wake_up) (struct cfhsi_dev *dev);
 int (*cfhsi_wake_down) (struct cfhsi_dev *dev);
 int (*cfhsi_get_peer_wake) (struct cfhsi_dev *dev, bool *status);
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 int (*cfhsi_fifo_occupancy)(struct cfhsi_dev *dev, size_t *occupancy);
 int (*cfhsi_rx_cancel)(struct cfhsi_dev *dev);
 struct cfhsi_drv *drv;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct cfhsi_rx_state {
 int state;
 int nfrms;
 int pld_len;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 int retries;
 bool piggy_desc;
};
enum {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 CFHSI_PRIO_CTL = 0,
 CFHSI_PRIO_VI,
 CFHSI_PRIO_VO,
 CFHSI_PRIO_BEBK,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 CFHSI_PRIO_LAST,
};
struct cfhsi {
 struct caif_dev_common cfdev;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 struct net_device *ndev;
 struct platform_device *pdev;
 struct sk_buff_head qhead[CFHSI_PRIO_LAST];
 struct cfhsi_drv drv;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 struct cfhsi_dev *dev;
 int tx_state;
 struct cfhsi_rx_state rx_state;
 unsigned long inactivity_timeout;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 int rx_len;
 u8 *rx_ptr;
 u8 *tx_buf;
 u8 *rx_buf;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 u8 *rx_flip_buf;
 spinlock_t lock;
 int flow_off_sent;
 u32 q_low_mark;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 u32 q_high_mark;
 struct list_head list;
 struct work_struct wake_up_work;
 struct work_struct wake_down_work;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 struct work_struct out_of_sync_work;
 struct workqueue_struct *wq;
 wait_queue_head_t wake_up_wait;
 wait_queue_head_t wake_down_wait;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 wait_queue_head_t flush_fifo_wait;
 struct timer_list inactivity_timer;
 struct timer_list rx_slowpath_timer;
 unsigned long aggregation_timeout;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 int aggregation_len;
 struct timer_list aggregation_timer;
 unsigned long bits;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
enum ifla_caif_hsi {
 __IFLA_CAIF_HSI_UNSPEC,
 IFLA_CAIF_HSI_INACTIVITY_TOUT,
 __IFLA_CAIF_HSI_HEAD_ALIGN,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 __IFLA_CAIF_HSI_TAIL_ALIGN,
 __IFLA_CAIF_HSI_QHIGH_WATERMARK,
 __IFLA_CAIF_HSI_QLOW_WATERMARK,
 __IFLA_CAIF_HSI_MAX
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
#endif
