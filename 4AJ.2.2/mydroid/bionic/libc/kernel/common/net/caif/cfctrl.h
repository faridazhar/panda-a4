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
#ifndef CFCTRL_H_
#define CFCTRL_H_
#include <net/caif/caif_layer.h>
#include <net/caif/cfsrvl.h>
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
enum cfctrl_cmd {
 CFCTRL_CMD_LINK_SETUP = 0,
 CFCTRL_CMD_LINK_DESTROY = 1,
 CFCTRL_CMD_LINK_ERR = 2,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 CFCTRL_CMD_ENUM = 3,
 CFCTRL_CMD_SLEEP = 4,
 CFCTRL_CMD_WAKE = 5,
 CFCTRL_CMD_LINK_RECONF = 6,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 CFCTRL_CMD_START_REASON = 7,
 CFCTRL_CMD_RADIO_SET = 8,
 CFCTRL_CMD_MODEM_SET = 9,
 CFCTRL_CMD_MASK = 0xf
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
enum cfctrl_srv {
 CFCTRL_SRV_DECM = 0,
 CFCTRL_SRV_VEI = 1,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 CFCTRL_SRV_VIDEO = 2,
 CFCTRL_SRV_DBG = 3,
 CFCTRL_SRV_DATAGRAM = 4,
 CFCTRL_SRV_RFM = 5,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 CFCTRL_SRV_UTIL = 6,
 CFCTRL_SRV_MASK = 0xf
};
#define CFCTRL_RSP_BIT 0x20
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define CFCTRL_ERR_BIT 0x10
struct cfctrl_rsp {
 void (*linksetup_rsp)(struct cflayer *layer, u8 linkid,
 enum cfctrl_srv serv, u8 phyid,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 struct cflayer *adapt_layer);
 void (*linkdestroy_rsp)(struct cflayer *layer, u8 linkid);
 void (*linkerror_ind)(void);
 void (*enum_rsp)(void);
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 void (*sleep_rsp)(void);
 void (*wake_rsp)(void);
 void (*restart_rsp)(void);
 void (*radioset_rsp)(void);
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 void (*reject_rsp)(struct cflayer *layer, u8 linkid,
 struct cflayer *client_layer);
};
struct cfctrl_link_param {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 enum cfctrl_srv linktype;
 u8 priority;
 u8 phyid;
 u8 endpoint;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 u8 chtype;
 union {
 struct {
 u8 connid;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 } video;
 struct {
 u32 connid;
 } datagram;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 struct {
 u32 connid;
 char volume[20];
 } rfm;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 struct {
 u16 fifosize_kb;
 u16 fifosize_bufs;
 char name[16];
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 u8 params[255];
 u16 paramlen;
 } utility;
 } u;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
struct cfctrl_request_info {
 int sequence_no;
 enum cfctrl_cmd cmd;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 u8 channel_id;
 struct cfctrl_link_param param;
 struct cflayer *client_layer;
 struct list_head list;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
struct cfctrl {
 struct cfsrvl serv;
 struct cfctrl_rsp res;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 atomic_t req_seq_no;
 atomic_t rsp_seq_no;
 struct list_head list;
 spinlock_t info_list_lock;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#ifndef CAIF_NO_LOOP
 u8 loop_linkid;
 int loop_linkused[256];
 spinlock_t loop_linkid_lock;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#endif
};
struct cflayer *cfctrl_create(void);
struct cfctrl_rsp *cfctrl_get_respfuncs(struct cflayer *layer);
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#endif
