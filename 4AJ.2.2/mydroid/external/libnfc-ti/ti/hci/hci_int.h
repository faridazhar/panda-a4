/*
 * hci_int.h
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

 /******************************************************************************
  This header files contains the definitions for the HCI Network.
*******************************************************************************/

#ifndef __HCI_INT_H
#define __HCI_INT_H

#include "nfc_types.h"
#include "nci_utils.h"
#include "nci_api.h"

/* Hosts */
#define HCI_HOST_ID_HOST_CONTROLLER			0x00
#define HCI_HOST_ID_TERMINAL_HOST			0x01
#define HCI_HOST_ID_UICC_HOST				0x02
#define HCI_HOST_ID_GTO_SE				0xC0	/* GTO proprietary secure element host id */

/* Gates */
#define HCI_GATE_ID_LOOP_BACK				0x04
#define HCI_GATE_ID_IDENTITY_MGMT			0x05
#define HCI_GATE_ID_ADMIN				0xFF	/* proprietary local admin gate id */

/* Pipes */
#define HCI_PIPE_ID_LINK_MGMT				0x00
#define HCI_PIPE_ID_ADMIN				0x01
#define HCI_PIPE_ID_DYNAMIC_MIN				0x70
#define HCI_PIPE_ID_DYNAMIC_MAX				0x7C
#define HCI_PIPE_ID_INVALID				0xFF

#define HCI_PIPE_STATE_CLOSED				0x00
#define HCI_PIPE_STATE_OPENING				0x01
#define HCI_PIPE_STATE_OPEN				0x02
#define HCI_PIPE_STATE_CLOSING				0x03

/* HCP packets */
#define HCI_HCP_PKT_HDR_SIZE				1

#define HCI_PID_GET(p_header)				(nfc_u8)((p_header)[0]&0x7f)
#define HCI_PID_SET(p_header, pid)			(p_header)[0] |= (nfc_u8)((pid)&0x7f)

#define HCI_CB_GET(p_header)				(nfc_u8)(((p_header)[0]>>7)&0x01)
#define HCI_CB_SET(p_header, cb)			(p_header)[0] |= (nfc_u8)(((cb)&0x01)<<7)

#define HCI_CB_LAST_FRAG				0x01
#define HCI_CB_CONT					0x00

/* HCP messages */
#define HCI_HCP_MSG_HDR_SIZE				1
#define HCI_HCP_MSG_MAX_SIZE				28

#define HCI_INST_GET(p_header)				(nfc_u8)((p_header)[0]&0x3f)
#define HCI_INST_SET(p_header, inst)			(p_header)[0] |= (nfc_u8)((inst)&0x3f)

#define HCI_TYPE_GET(p_header)				(nfc_u8)(((p_header)[0]>>6)&0x03)
#define HCI_TYPE_SET(p_header, type)			(p_header)[0] |= (nfc_u8)(((type)&0x03)<<6)

#define HCI_TYPE_CMD					0x00
#define HCI_TYPE_EVT					0x01
#define HCI_TYPE_RSP					0x02

#define HCI_CMD_TIMEOUT					2000 /* mili-seconds */

/* Instructions - Commands */
#define HCI_CMD_NONE					0xFF

#define HCI_CMD_ANY_SET_PARAM				0x01
struct hci_cmd_any_set_param {
	nfc_u8 param_id;
	nfc_u8 param_value[0];
} __attribute__ ((packed));

#define HCI_CMD_ANY_GET_PARAM				0x02
struct hci_cmd_any_get_param {
	nfc_u8 param_id;
} __attribute__ ((packed));

#define HCI_CMD_ANY_OPEN_PIPE				0x03
struct hci_rsp_any_open_pipe {
	nfc_u8 num_open_pipes;
} __attribute__ ((packed));

#define HCI_CMD_ANY_CLOSE_PIPE				0x04

#define HCI_CMD_ADM_CREATE_PIPE				0x10
struct hci_cmd_adm_create_pipe {
	nfc_u8 src_gate_id;
	nfc_u8 dst_host_id;
	nfc_u8 dst_gate_id;
} __attribute__ ((packed));

struct hci_rsp_adm_create_pipe {
	nfc_u8 src_host_id;
	nfc_u8 src_gate_id;
	nfc_u8 dst_host_id;
	nfc_u8 dst_gate_id;
	nfc_u8 pipe_id;
} __attribute__ ((packed));

#define HCI_CMD_ADM_DELETE_PIPE				0x11
struct hci_cmd_adm_delete_pipe {
	nfc_u8 pipe_id;
} __attribute__ ((packed));

#define HCI_CMD_ADM_NOTIFY_PIPE_CREATED			0x12
struct hci_cmd_adm_notify_pipe_created {
	nfc_u8 src_host_id;
	nfc_u8 src_gate_id;
	nfc_u8 dst_host_id;
	nfc_u8 dst_gate_id;
	nfc_u8 pipe_id;
} __attribute__ ((packed));

#define HCI_CMD_ADM_NOTIFY_PIPE_DELETED			0x13
struct hci_cmd_adm_notify_pipe_deleted {
	nfc_u8 pipe_id;
} __attribute__ ((packed));

#define HCI_CMD_ADM_CLEAR_ALL_PIPE			0x14
struct hci_cmd_adm_clear_all_pipe {
	nfc_u16 id_ref_data;
} __attribute__ ((packed));

#define HCI_CMD_ADM_NOTIFY_ALL_PIPE_CLEARED		0x15
struct hci_cmd_adm_notify_all_pipe_cleared {
	nfc_u8 requesting_host_id;
} __attribute__ ((packed));

/* Instructions - Responses */
#define HCI_RSP_ANY_OK					0x00
#define HCI_RSP_ANY_E_NOT_CONNECTED			0x01
#define HCI_RSP_ANY_E_CMD_PAR_UNKNOWN			0x02
#define HCI_RSP_ANY_E_NOK				0x03
#define HCI_RSP_ADM_E_NO_PIPES_AVAILABLE		0x04
#define HCI_RSP_ANY_E_REG_PAR_UNKNOWN			0x05
#define HCI_RSP_ANY_E_PIPE_NOT_OPENED			0x06
#define HCI_RSP_ANY_E_CMD_NOT_SUPPORTED			0x07
#define HCI_RSP_ANY_E_INHIBITED				0x08
#define HCI_RSP_ANY_E_TIMEOUT				0x09
#define HCI_RSP_ANY_E_REG_ACCESS_DENIED			0x0A
#define HCI_RSP_ANY_E_PIPE_ACCESS_DENIED		0x0B

/* Instructions - Events */
#define HCI_EVT_HCI_END_OF_OPERATION			0x01
#define HCI_EVT_POST_DATA				0x02
#define HCI_EVT_HOT_PLUG				0x03

/* Host controller administration gate registry */
#define HCI_REGISTRY_PARAM_ID_SESSION_IDENTITY		0x01
#define HCI_REGISTRY_PARAM_ID_MAX_PIPE			0x02
#define HCI_REGISTRY_PARAM_ID_WHITELIST			0x03
#define HCI_REGISTRY_PARAM_ID_HOST_LIST			0x04

/* Host controller / Host link management gate registry */
#define HCI_REGISTRY_PARAM_ID_REC_ERROR			0x01

/* Identity management gate registry */
#define HCI_REGISTRY_PARAM_ID_VERSION_SW		0x01
#define HCI_REGISTRY_PARAM_ID_VERSION_HARD		0x03
#define HCI_REGISTRY_PARAM_ID_VENDOR_NAME		0x04
#define HCI_REGISTRY_PARAM_ID_MODEL_ID			0x05
#define HCI_REGISTRY_PARAM_ID_HCI_VERSION		0x02
#define HCI_REGISTRY_PARAM_ID_GATES_LIST		0x06

/* Connectivity gate */
#define HCI_GATE_ID_CONNECTIVITY			0x41

#define HCI_CMD_PRO_HOST_REQUEST			0x10

#define HCI_EVT_CONNECTIVITY				0x10
#define HCI_EVT_TRANSACTION				0x12
#define HCI_EVT_OPERATION_ENDED				0x13

#define HCI_EVT_TRANSACTION_AID_TAG			0x81
#define HCI_EVT_TRANSACTION_AID_MIN_LEN			5
#define HCI_EVT_TRANSACTION_AID_MAX_LEN			16

#define HCI_EVT_TRANSACTION_PARAMS_TAG			0x82
#define HCI_EVT_TRANSACTION_PARAMS_MAX_LEN		255

/* Connectivity application gate */
#define HCI_EVT_STANDBY					0x10

/* GTO proprietary APDU gate id */
#define HCI_GATE_ID_GTO_APDU				0xF0

#define HCI_EVT_GTO_TRANSMIT_DATA			0x10
#define HCI_EVT_GTO_WTX_REQUEST				0x11

/* HCI DH persistence property */
#define HCI_DH_PERSISTENCE_PROP				((const nfc_s8*)"ti.nfc.hci.dh.persistence")

/* HCI workaround VS commands */
#define NCI_VS_HCI_SEND_HCP_PACKET_CMD			(__OPCODE(NCI_GID_PROPRIETARY,0x36))
#define NCI_VS_HCI_SEND_HCP_PACKET_RSP			(NCI_VS_HCI_SEND_HCP_PACKET_CMD)
#define NCI_VS_HCI_RCV_HCP_PACKET_NTF			(NCI_VS_HCI_SEND_HCP_PACKET_CMD)


/******************************************************************************/
/* HCI Structures and Types                                                   */
/******************************************************************************/

struct hci_pipe;
struct hci_gate;
struct hcp_msg;
struct hci_core_context;
struct hci_admin_context;
struct hci_identity_mgmt_context;
struct hci_loop_back_context;
struct hci_connectivity_context;

struct hci_gate_callbacks {
	void (*cmd_recv)(nfc_handle_t h_caller, struct hci_pipe *pipe, nfc_u8 inst, nfc_u8 *p_data, nfc_u32 data_len);
	void (*evt_recv)(nfc_handle_t h_caller, struct hci_pipe *pipe, nfc_u8 inst, nfc_u8 *p_data, nfc_u32 data_len);
	void (*rsp_recv)(nfc_handle_t h_caller, struct hci_pipe *pipe, nfc_u8 inst, nfc_u8 *p_data, nfc_u32 data_len);
	void (*cmd_timeout)(nfc_handle_t h_caller, struct hci_pipe *pipe);
};

struct hci_core_callbacks {
	void (*activated)(nfc_handle_t h_caller, nfc_status status);
	void (*deactivated)(nfc_handle_t h_caller, nfc_status status);
};

struct hci_admin_callbacks {
	void (*pipe_opened)(nfc_handle_t h_caller, struct hci_pipe *pipe, nfc_status status);
	void (*pipe_closed)(nfc_handle_t h_caller, struct hci_pipe *pipe, nfc_status status);
	void (*pipe_created)(nfc_handle_t h_caller, struct hci_pipe *pipe, nfc_status status);
	void (*pipe_deleted)(nfc_handle_t h_caller, struct hci_pipe *pipe, nfc_status status);
	void (*hot_plug)(nfc_handle_t h_caller, struct hci_pipe *pipe);
	void (*host_list)(nfc_handle_t h_caller, struct hci_pipe *pipe, nfc_status status, nfc_u8 *p_host_list, nfc_u32 host_list_len);
};

struct hci_identity_mgmt_callbacks {
	void (*pipe_opened)(nfc_handle_t h_caller, struct hci_pipe *pipe, nfc_status status);
	void (*pipe_closed)(nfc_handle_t h_caller, struct hci_pipe *pipe, nfc_status status);
};

struct hci_loop_back_callbacks {
	void (*pipe_opened)(nfc_handle_t h_caller, struct hci_pipe *pipe, nfc_status status);
	void (*pipe_closed)(nfc_handle_t h_caller, struct hci_pipe *pipe, nfc_status status);
};

struct hci_connectivity_callbacks {
	void (*pipe_opened)(nfc_handle_t h_caller, struct hci_pipe *pipe, nfc_status status);
	void (*pipe_closed)(nfc_handle_t h_caller, struct hci_pipe *pipe, nfc_status status);
	void (*evt_connectivity)(nfc_handle_t h_caller, struct hci_pipe *pipe);
	void (*evt_transaction)(nfc_handle_t h_caller, struct hci_pipe *pipe, nfc_u8 *aid, nfc_u32 aid_len, nfc_u8 *params, nfc_u32 params_len);
};

struct hci_gto_apdu_callbacks {
	void (*pipe_opened)(nfc_handle_t h_caller, struct hci_pipe *pipe, nfc_status status);
	void (*pipe_closed)(nfc_handle_t h_caller, struct hci_pipe *pipe, nfc_status status);
	void (*evt_transmit_data)(nfc_handle_t h_caller, struct hci_pipe *pipe, nfc_u8 *data, nfc_u32 data_len);
	void (*evt_wtx_request)(nfc_handle_t h_caller, struct hci_pipe *pipe);
};

/******************************************************************************/
/* HCI Gate Function Declarations                                             */
/******************************************************************************/

struct hci_gate* hci_gate_alloc(nfc_u8 id, struct hci_gate_callbacks *callbacks, nfc_handle_t h_caller);
void hci_gate_free(struct hci_gate *gate);

nfc_u8 hci_gate_get_id(struct hci_gate *gate);


/******************************************************************************/
/* HCI Pipe Function Declarations                                             */
/******************************************************************************/

struct hci_pipe* hci_pipe_alloc_and_attach(struct hci_gate *local_gate, nfc_u8 id, nfc_u8 remote_host_id, nfc_u8 initial_state);
void hci_pipe_detach_and_free(struct hci_pipe *pipe);

nfc_u8 hci_pipe_get_id(struct hci_pipe* pipe);
nfc_u8 hci_pipe_get_remote_host_id(struct hci_pipe* pipe);
struct hci_gate* hci_pipe_get_local_gate(struct hci_pipe *pipe);

nfc_u8 hci_pipe_get_state(struct hci_pipe* pipe);
void hci_pipe_set_state(struct hci_pipe* pipe, nfc_u8 new_state);

nfc_u8 hci_pipe_get_pending_cmd(struct hci_pipe* pipe);
void hci_pipe_set_pending_cmd(struct hci_pipe* pipe, nfc_u8 pending_cmd);

/******************************************************************************/
/* HCP Message Function Declarations                                          */
/******************************************************************************/

struct hcp_msg* hcp_msg_alloc(nfc_u8 type, nfc_u8 inst, nfc_u32 data_len);
void hcp_msg_free(struct hcp_msg* msg);

nfc_u8 hcp_msg_get_type(struct hcp_msg *msg);
nfc_u8 hcp_msg_get_inst(struct hcp_msg *msg);
nfc_u32 hcp_msg_get_data_len(struct hcp_msg *msg);
nfc_u8* hcp_msg_get_data(struct hcp_msg *msg);


/******************************************************************************/
/* HCI Core Function Declarations                                             */
/******************************************************************************/

struct hci_core_context* hci_core_create(nfc_handle_t h_nci, nfc_handle_t h_osa, struct hci_core_callbacks *callbacks, nfc_handle_t h_caller);
void hci_core_destroy(struct hci_core_context *core_ctx);

nfc_status hci_core_activate(struct hci_core_context *core_ctx, nfc_u8 nfcee_id);
nfc_status hci_core_deactivate(struct hci_core_context *core_ctx, nfc_u8 nfcee_id);

void hci_core_register_gate(struct hci_core_context *core_ctx, struct hci_gate *gate);
void hci_core_unregister_gate(struct hci_core_context *core_ctx, struct hci_gate *gate);

struct hci_gate* hci_core_find_gate(struct hci_core_context *core_ctx, nfc_u8 gate_id);
struct hci_pipe* hci_core_find_pipe(struct hci_core_context *core_ctx, nfc_u8 pipe_id);
struct hci_pipe** hci_core_list_all_pipes(struct hci_core_context *core_ctx, nfc_u8 remote_host_id, nfc_u8 *num_pipes);

nfc_status hci_core_recv_open_pipe(struct hci_core_context *core_ctx, struct hci_pipe *pipe, nfc_u32 data_len);
nfc_status hci_core_recv_close_pipe(struct hci_core_context *core_ctx, struct hci_pipe *pipe, nfc_u32 data_len);

void hci_core_send_hcp_msg(struct hci_core_context *core_ctx, struct hci_pipe *pipe, struct hcp_msg* msg);
void hci_core_send_rsp_no_param(struct hci_core_context *core_ctx, struct hci_pipe *pipe, nfc_u8 rsp_inst_code);

nfc_status hci_core_send_open_pipe(struct hci_core_context *core_ctx, struct hci_pipe *pipe);
nfc_status hci_core_send_close_pipe(struct hci_core_context *core_ctx, struct hci_pipe *pipe);

nfc_status hci_core_send_get_param(struct hci_core_context *core_ctx, struct hci_pipe *pipe, nfc_u8 param_id);

void hci_core_persistence_store(struct hci_core_context *core_ctx);
void hci_core_persistence_restore(struct hci_core_context *core_ctx);


/******************************************************************************/
/* HCI Admin Function Declarations                                            */
/******************************************************************************/

struct hci_admin_context* hci_admin_create(struct hci_core_context *core_ctx, struct hci_admin_callbacks *callbacks, nfc_handle_t h_caller);
void hci_admin_destroy(struct hci_admin_context *admin_ctx);

nfc_status hci_admin_open(struct hci_admin_context *admin_ctx);
nfc_status hci_admin_close(struct hci_admin_context *admin_ctx);

nfc_status hci_admin_create_pipe(struct hci_admin_context *admin_ctx, struct hci_gate *local_gate, nfc_u8 dst_host_id, nfc_u8 dst_gate_id);
nfc_status hci_admin_delete_pipe(struct hci_admin_context *admin_ctx, struct hci_pipe *pipe);

nfc_status hci_admin_get_host_list(struct hci_admin_context *admin_ctx);

/******************************************************************************/
/* HCI Identity Management Function Declarations                              */
/******************************************************************************/

struct hci_identity_mgmt_context* hci_identity_mgmt_create(struct hci_core_context *core_ctx, struct hci_identity_mgmt_callbacks *callbacks, nfc_handle_t h_caller);
void hci_identity_mgmt_destroy(struct hci_identity_mgmt_context *identity_mgmt_ctx);

void hci_identity_mgmt_set_local_gates_list(struct hci_identity_mgmt_context *identity_mgmt_ctx, nfc_u8 *gates_list, nfc_u32 gates_list_len);


/******************************************************************************/
/* HCI Loop Back Function Declarations                                        */
/******************************************************************************/

struct hci_loop_back_context* hci_loop_back_create(struct hci_core_context *core_ctx, struct hci_loop_back_callbacks *callbacks, nfc_handle_t h_caller);
void hci_loop_back_destroy(struct hci_loop_back_context *loop_back_ctx);


/******************************************************************************/
/* HCI Connectivity Function Declarations                                     */
/******************************************************************************/

struct hci_connectivity_context* hci_connectivity_create(struct hci_core_context *core_ctx, struct hci_connectivity_callbacks *callbacks, nfc_handle_t h_caller);
void hci_connectivity_destroy(struct hci_connectivity_context *connectivity_ctx);

/******************************************************************************/
/* HCI GTO APDU Function Declarations                                         */
/******************************************************************************/

struct hci_gto_apdu_context* hci_gto_apdu_create(struct hci_core_context *core_ctx, struct hci_gto_apdu_callbacks *callbacks, nfc_handle_t h_caller);
void hci_gto_apdu_destroy(struct hci_gto_apdu_context *gto_apdu_ctx);

nfc_status hci_gto_apdu_send_evt_transmit_data(struct hci_gto_apdu_context *gto_apdu_ctx, nfc_u8 *data, nfc_u32 data_len);

#endif /* __HCI_INT_H */
