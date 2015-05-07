/*
 * nci_ver_1\nci_int.h
 *
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

 /*******************************************************************************

  This header files contains the definitions for the NFC Controller Interface.

*******************************************************************************/



#ifndef __NCI_INT_H
#define __NCI_INT_H

#include "nfc_types.h"

#if (NCI_VERSION_IN_USE==NCI_VER_1)

#define NCI_DBG

/*********************************************************************************/
/* Macros for NCI Header Manipulation                                                          */
/*********************************************************************************/

#define NCI_MT_DATA				0
#define NCI_MT_COMMAND			1
#define NCI_MT_RESPONSE			2
#define NCI_MT_NOTIFICATION		3

/* NCI Header Macros */
#define NCI_MT_GET(p_header)				(((p_header)[0]>>5)&0x07)
#define NCI_MT_SET(p_header, mt)			(p_header)[0] |= (nfc_u8)(((mt)&0x07)<<5)
#define NCI_GID_GET(p_header)				((p_header)[0]&0xf)
#define NCI_OID_GET(p_header)				((p_header)[1]&0x3f)
#define NCI_MT_CLR(p_header)				(p_header)[0] &= ~(0x07<<5);
#define NCI_PBF_GET(p_header)				(nfc_u8)(((p_header)[0]>>4)&0x01)
#define NCI_PBF_SET(p_header, pbf)			(p_header)[0] |= (nfc_u8)(((pbf)&0x01)<<4)

/* NCI Control Macros */
#define NCI_OPCODE_GET(p_header)			__OPCODE(p_header[0],p_header[1])
#define NCI_OPCODE_SET(p_header, opcode)	(p_header)[0] |= (nfc_u8)(((opcode)&0xf00)>>8);		\
											(p_header)[1] |= (nfc_u8)((opcode)&0x3f);
#define NCI_OPCODE_GID_GET(p_opcode)			((p_opcode)[1]&0xf)
#define NCI_OPCODE_OID_GET(p_opcode)			((p_opcode)[0]&0x3f)
#define NCI_CTRL_LEN_GET(p_header)			(nfc_u8)(p_header)[2]
#define NCI_CTRL_LEN_SET(p_header, len)		(p_header)[2] = (nfc_u8)len

/* NCI Data Macros */
#define NCI_CONNID_GET(p_header)			(nfc_u8)(((p_header)[0]>>0)&0x0f)
#define NCI_CONNID_SET(p_header, connid)	(p_header)[0] |= (nfc_u8)(((connid)&0x0f)<<0)
#define NCI_DATA_LEN_GET(p_header)			(p_header)[2]
#define NCI_DATA_LEN_SET(p_header, len)		(p_header)[2] = (nfc_u8)len


/* NCI Parsing Utilities */
#define NCI_SET_4B(p,v32)			        {*((nfc_u32*)p)=v32; p+=4;}
#define NCI_SET_2B(p,v16)			        {*((nfc_u16*)p)=v16; p+=2;}
#define NCI_SET_1B(p,v8)			        {*p++=v8;}
#define NCI_GET_4B(p,v32)			        {v32=*((nfc_u32*)p); p+=4;}
#define NCI_GET_2B(p,v16)			        {v16=*((nfc_u16*)p); p+=2;};
#define NCI_GET_1B(p,v8)			        {v8=*p++;}
#define NCI_SET_ARR(p,varr,len)		        {int i_i; for(i_i=0;i_i<len;i_i++) {*p=varr[i_i];p++;}}
#define NCI_GET_ARR(p,varr,len)		        {int i_i; for(i_i=0;i_i<len;i_i++) {varr[i_i]=*p;p++;}}
#define NCI_SET_ARR_REV(p,varr,len)		    {int i_i; for(i_i=len-1;i_i>=0;i_i--) {*p=varr[i_i];p++;}}
#define NCI_GET_ARR_REV(p,varr,len)		    {int i_i; for(i_i=len-1;i_i>=0;i_i--) {varr[i_i]=*p;p++;}}


enum nci_discovery_frequency_period
{
	E_NCI_DISCOVERY_FREQUENCY_EVERY_1_DISCOVERY_PERIOD = 1,
	E_NCI_DISCOVERY_FREQUENCY_EVERY_2_DISCOVERY_PERIOD,
	E_NCI_DISCOVERY_FREQUENCY_EVERY_3_DISCOVERY_PERIOD,
	E_NCI_DISCOVERY_FREQUENCY_EVERY_4_DISCOVERY_PERIOD,
	E_NCI_DISCOVERY_FREQUENCY_EVERY_5_DISCOVERY_PERIOD,
	E_NCI_DISCOVERY_FREQUENCY_EVERY_6_DISCOVERY_PERIOD,
	E_NCI_DISCOVERY_FREQUENCY_EVERY_7_DISCOVERY_PERIOD,
	E_NCI_DISCOVERY_FREQUENCY_EVERY_8_DISCOVERY_PERIOD,
	E_NCI_DISCOVERY_FREQUENCY_EVERY_9_DISCOVERY_PERIOD,
	E_NCI_DISCOVERY_FREQUENCY_EVERY_10_DISCOVERY_PERIOD
};

/* NCI Command parameters size */
#define	NCI_CORE_RESET_CMD_SIZE			1
#define	NCI_CORE_INIT_CMD_SIZE				0
#define	NCI_NFCEE_DISCOVER_CMD_SIZE		1
#define	NCI_NFCEE_MODE_SET_CMD_SIZE		2
#define	NCI_CORE_SET_CONFIG_CMD_MIN_SIZE	1
#define	NCI_CORE_GET_CONFIG_CMD_MIN_SIZE	1
#define	NCI_CORE_CONN_CREATE_CMD_MIN_SIZE	2
#define	NCI_CORE_CONN_CLOSE_CMD_SIZE		1
#define NCI_RF_DISCOVER_SEL_CMD_SIZE		3
#define	NCI_RF_DISCOVER_CMD_MIN_SIZE		1
#define	NCI_RF_DISCOVER_CMD_CONF_SIZE		2
#define	NCI_RF_DISCOVER_MAP_CMD_MIN_SIZE	1
#define	NCI_RF_DISCOVER_MAP_CMD_CONF_SIZE	3
#define	NCI_RF_DEACTIVATE_CMD_SIZE			1

/* NCI Response parameters size */
#define	NCI_CORE_RESET_RSP_SIZE					3
#define	NCI_CORE_INIT_RSP_FEATURES_SIZE			4
#define	NCI_CORE_INIT_RSP_MANUFACTURER_SPECIFIC_SIZE	4
#define	NCI_NFCEE_DISCOVER_RSP_SIZE				2
#define	NCI_NFCEE_MODE_SET_RSP_SIZE			1
#define	NCI_CORE_CONN_CREATE_RSP_SIZE			4
#define	NCI_CORE_CONN_CLOSE_RSP_SIZE			1
#define	NCI_CORE_SET_CONFIG_RSP_MIN_SIZE		2
#define	NCI_CORE_GET_CONFIG_RSP_MIN_SIZE		2
#define	NCI_CORE_DEFAULT_RSP_SIZE				1

/* NCI Notification parameters size */
#define	NCI_CORE_RESET_NTF_SIZE					2
#define	NCI_CORE_RF_FIELD_INFO_NTF_SIZE			1
#define	NCI_CORE_CONN_CREDITS_NTF_MIN_SIZE		1
#define	NCI_CORE_CONN_CREDITS_NTF_CONF_SIZE	2
#define	NCI_RF_ACTIVATE_NTF_A_PASSIVE_POLL_SENS_RES_SIZE	2
#define	NCI_RF_DEACTIVATE_NTF_SIZE				2


#define	NCI_MAX_CONTROL_PACKET_SIZE	255
#define	NCI_COMMAND_PREAMBLE			0xabcd
#define	NCI_COMMAND_TIMEOUT			1000 /* mili-seconds */

#define	NCI_DEQUEUE_ELEMENT_DISABLED		0
#define	NCI_DEQUEUE_ELEMENT_ENABLED		1

#define	NCI_CONNECTION_DISABLED			0
#define	NCI_CONNECTION_ENABLED				1

#define NCI_CORE_RESET_CMD_PARAM_KEEP_CONFIG	0
#define NCI_CORE_RESET_CMD_PARAM_RESET_CONFIG	1

/*********************************************************************************/
/* NCI Private Structures                                                           */
/*********************************************************************************/
/* NCI Command structure - stores parameters of NCI command
		Built during send_cmd (p_buff payload is generated within cmd_builder and passed to NCI CMD task queue */

/* Definition of the raw command response callback. Raw commands are currently only implemented internally */
typedef void (*raw_rsp_cb_f)(nfc_handle_t usr_param, nfc_u8* p_rsp_buff, nfc_u32 rsp_buff_len);

struct nci_cmd
{
	nfc_u16				preamble;
	nfc_u16				opcode;
	rsp_cb_f			f_rsp_cb;
	nci_rsp_param_u		*pu_rsp;
	nfc_u16				rsp_len;
	struct nci_buff		*p_buff;
	nfc_handle_t		h_rsp_cb; /* handle for the NCI Response */
	nci_cmd_param_u		u_cmd; /* storage (duplication) of user command params - used upon response */

	vendor_specific_cb_f vendor_specific_cb;
	raw_rsp_cb_f		f_raw_rsp_cb;
};


/* NCI notification structure - stores callback registration parameters. */
struct nci_ntf
{
	ntf_cb_f			f_ntf_cb;
	nfc_handle_t		h_ntf_cb;
	struct nci_ntf		*p_next;
};

typedef struct nci_vendor_specific_ntf_
{
	vendor_specific_cb_f			vendor_specific_ntf_cb;
	nfc_handle_t					vendor_specific_ntf_cb_param;
	struct nci_vendor_specific_ntf_	*p_next;
}nci_vendor_specific_ntf;

/* NCI operation structure - stores parameters of NCI Operation.
		Built during start_operation and passed to OPERATION task queue */
struct nci_op
{
	nfc_u16				preamble;
	nfc_u16				opcode;
	op_param_u			u_param;
	op_rsp_cb_f			f_rsp_cb;
	nfc_handle_t			h_rsp_cb;
	op_rsp_param_u		*pu_rsp;
	nfc_u16				rsp_len;
};

#define NCI_VENDOR_SPECIFIC_NTF_MAX (64) //According to NCI spec, max. of 64 proprietary ntfs. can be defined

struct nci_context
{
	nfc_handle_t			h_osa;			/* handle for os adaptation context */
	nfc_handle_t			h_ncidev;

	struct nci_cmd		curr_cmd;			/* NCI structure for the command in process */
	nfc_u32				cmd_timeout;		/* timeout used to watch the command-response sequence */
	nfc_handle_t			h_cmd_watchdog;		/* handle of watchdog timer (will be used to cancel timer upon response reception) */
	nfc_u8				max_control_packet_size; /* Retrived by Init response. Used by control path SAR algorithm */


	struct nci_ntf		*p_ntf_tbl[E_NCI_NTF_MAX]; /* array of lists of notification callbacks, index=ntf_id
													list includes all the callbacks that were registered per specific notfication
													(each will be invoked when a notification is received) */


	nfc_handle_t			h_data;			/* handle of NCI Data module */
	nfc_handle_t			h_ncix;			/* handle of ncix sub-component */
	nfc_handle_t			h_nci_rf_sm;	/* handle of nci rf state machine*/

	nci_vendor_specific_ntf *p_vendor_specific_ntf_tbl[NCI_VENDOR_SPECIFIC_NTF_MAX];
};



/*********************************************************************************/
/* NCI Global Routines                                                           */
/*********************************************************************************/
void nci_cmd_cb(nfc_handle_t h_nci, nfc_handle_t h_nci_cmd);
void nci_data_cb(nfc_handle_t h_nci, nfc_handle_t h_nci_data);
void nci_rx_cb(nfc_handle_t h_nci, nfc_handle_t h_nci_buff);
void ncix_operation_cb(nfc_handle_t h_ncix, nfc_handle_t h_nci_op);

nci_status_e nci_generate_cmd(nfc_u16 opcode, nci_cmd_param_u *pu_cmd, struct nci_buff **pp_nci_buff, nfc_handle_t h_nci);
nfc_u32 nci_parse_rsp(nfc_u16 opcode, nfc_u8 *p_data, nci_rsp_param_u *pu_rsp, nfc_u32 len);
nfc_u32 nci_parse_ntf(nfc_u16 opcode, nfc_u8 *p_data, nci_ntf_e *p_ntf_id, nci_ntf_param_u *pu_ntf, nfc_u32 len);

/* Get the handle of registered transport device */
nfc_handle_t	nci_trans_get_ncidev(nfc_u32 dev_id);
nfc_status	nci_trans_open(nfc_handle_t h_nci);
nfc_status	nci_trans_close(nfc_handle_t h_nci);
nfc_status	nci_trans_send(nfc_handle_t h_nci, struct nci_buff *p_buff);

/* Creator/Destructor of NCIX sub-component */
nfc_handle_t ncix_create(nfc_handle_t h_nci, nfc_handle_t h_osa);
void ncix_destroy(nfc_handle_t h_ncix);

/* nci connections initialization (triggered by init response) */
void nci_set_max_connections(nfc_handle_t h_nci, nfc_u8 max_logical_connections);

/**

 * Send single NCI command and register response handler
 *
 * @h_nci:			handle to NCi object
 * @opcode:			Command identifier
 * @p_nci_cmd:		Command buffer - hold command in binary format
 * @pu_resp:		Optional buffer for NCI response payload (NULL if response is ignored).
 * @rsp_len:		Maximum length of expected response (Assert will be invoked if response exceeds this).
 * @f_rsp_cb:		Optional callback function to be invoked upon response reception (or NULL).
 * @h_rsp_cb:		Optional parameter that is passed to f_rsp_cb, if invoked (caller object handle)
 *

 **/
nci_status_e nci_send_internal_cmd(	nfc_handle_t h_nci, nfc_u16 opcode,
									struct nci_cmd *p_nci_cmd,
									nci_rsp_param_u *pu_rsp,
									nfc_u8 rsp_len,
									rsp_cb_f f_rsp_cb,
									nfc_handle_t h_rsp_cb);

nci_status_e nci_configure_listen_routing(nfc_handle_t h_nci, configure_listen_routing_t* p_config);


#endif //#if (NCI_VERSION_IN_USE==NCI_VER_1)

#endif /* __NCI_INT_H */






















