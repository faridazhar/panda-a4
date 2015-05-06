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
#ifndef CAIF_SPI_H_
#define CAIF_SPI_H_
#include <net/caif/caif_device.h>
#define SPI_CMD_WR 0x00
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define SPI_CMD_RD 0x01
#define SPI_CMD_EOT 0x02
#define SPI_CMD_IND 0x04
#define SPI_DMA_BUF_LEN 8192
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define WL_SZ 2  
#define SPI_CMD_SZ 4  
#define SPI_IND_SZ 4  
#define SPI_XFER 0
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define SPI_SS_ON 1
#define SPI_SS_OFF 2
#define SPI_TERMINATE 3
#define MIN_TRANSITION_TIME_USEC 50
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define SPI_MASTER_CLK_MHZ 13
#define SPI_XFER_TIME_USEC(bytes, clk) (((bytes) * 8) / clk)
#define CAIF_MAX_SPI_FRAME 4092
#define CAIF_MAX_SPI_PKTS 9
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define CFSPI_DBG_PREFILL 0
struct cfspi_xfer {
 u16 tx_dma_len;
 u16 rx_dma_len;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 void *va_tx[2];
 dma_addr_t pa_tx[2];
 void *va_rx;
 dma_addr_t pa_rx;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
struct cfspi_ifc {
 void (*ss_cb) (bool assert, struct cfspi_ifc *ifc);
 void (*xfer_done_cb) (struct cfspi_ifc *ifc);
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 void *priv;
};
struct cfspi_dev {
 int (*init_xfer) (struct cfspi_xfer *xfer, struct cfspi_dev *dev);
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 void (*sig_xfer) (bool xfer, struct cfspi_dev *dev);
 struct cfspi_ifc *ifc;
 char *name;
 u32 clk_mhz;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 void *priv;
};
enum cfspi_state {
 CFSPI_STATE_WAITING = 0,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 CFSPI_STATE_AWAKE,
 CFSPI_STATE_FETCH_PKT,
 CFSPI_STATE_GET_NEXT,
 CFSPI_STATE_INIT_XFER,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 CFSPI_STATE_WAIT_ACTIVE,
 CFSPI_STATE_SIG_ACTIVE,
 CFSPI_STATE_WAIT_XFER_DONE,
 CFSPI_STATE_XFER_DONE,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 CFSPI_STATE_WAIT_INACTIVE,
 CFSPI_STATE_SIG_INACTIVE,
 CFSPI_STATE_DELIVER_PKT,
 CFSPI_STATE_MAX,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
struct cfspi {
 struct caif_dev_common cfdev;
 struct net_device *ndev;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 struct platform_device *pdev;
 struct sk_buff_head qhead;
 struct sk_buff_head chead;
 u16 cmd;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 u16 tx_cpck_len;
 u16 tx_npck_len;
 u16 rx_cpck_len;
 u16 rx_npck_len;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 struct cfspi_ifc ifc;
 struct cfspi_xfer xfer;
 struct cfspi_dev *dev;
 unsigned long state;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 struct work_struct work;
 struct workqueue_struct *wq;
 struct list_head list;
 int flow_off_sent;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 u32 qd_low_mark;
 u32 qd_high_mark;
 struct completion comp;
 wait_queue_head_t wait;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 spinlock_t lock;
 bool flow_stop;
 bool slave;
 bool slave_talked;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
#endif
