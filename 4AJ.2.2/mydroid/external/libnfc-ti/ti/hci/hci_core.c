/*
 * hci_core.c
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
  This source file contains the implementation of HCI Network core.
*******************************************************************************/
#include "hci_int.h"
#include "nfc_types.h"
#include "nci_utils.h"
#include "nfc_os_adapt.h"
#include "nci_api.h"


#define HCI_CORE_DEBUG


/******************************************************************************/
/* HCI Core Structures and Types                                              */
/******************************************************************************/

struct hci_gate {
	struct dll_node node; /* MUST be first field with the name 'node' */
	nfc_u8 id;
	struct hci_gate_callbacks *callbacks;
	nfc_handle_t h_caller;
	struct dll_node pipe_list; /* Holds associated pipes */
};

struct hci_pipe {
	struct dll_node node; /* MUST be first field with the name 'node' */
	struct hci_gate *local_gate;
	nfc_u8 id;
	nfc_u8 remote_host_id;
	nfc_u8 state;
	nfc_u8 *rx;
	nfc_u32 rx_len;
	nfc_u8 pending_cmd;
	nfc_handle_t h_cmd_watchdog;
};

struct hcp_msg {
	nfc_u8 *buff;
	nfc_u32 buff_len;
};

struct hci_core_context {
	nfc_handle_t h_nci;
	nfc_handle_t h_osa;
	struct dll_node gate_list;
	struct hci_core_callbacks *callbacks;
	nfc_handle_t h_caller;
};

/* persistent data stored for each pipe */
struct hci_pipe_persistent_record {
	nfc_u8 gate_id;
	nfc_u8 pipe_id;
	nfc_u8 remote_host_id;
	nfc_u8 state;
} __attribute__ ((packed));


/******************************************************************************/
/* HCI Function Definitions                                                   */
/******************************************************************************/
#ifdef HCI_CORE_DEBUG
static const char* hcp_msg_type(nfc_u8 type)
{
	switch (type) {
		case (HCI_TYPE_CMD):
			return "CMD";
		case (HCI_TYPE_EVT):
			return "EVT";
		case (HCI_TYPE_RSP):
			return "RSP";
		default:
			return "UNKNOWN";
	}
}

static const char* hcp_msg_inst_cmd(nfc_u8 inst)
{
	switch (inst) {
		case (HCI_CMD_ANY_SET_PARAM):
			return "ANY_SET_PARAM";
		case (HCI_CMD_ANY_GET_PARAM):
			return "ANY_GET_PARAM";
		case (HCI_CMD_ANY_OPEN_PIPE):
			return "ANY_OPEN_PIPE";
		case (HCI_CMD_ANY_CLOSE_PIPE):
			return "ANY_CLOSE_PIPE";
		case (HCI_CMD_ADM_CREATE_PIPE):
			return "ADM_CREATE_PIPE or PRO_HOST_REQUEST";
		case (HCI_CMD_ADM_DELETE_PIPE):
			return "ADM_DELETE_PIPE";
		case (HCI_CMD_ADM_NOTIFY_PIPE_CREATED):
			return "ADM_NOTIFY_PIPE_CREATED";
		case (HCI_CMD_ADM_NOTIFY_PIPE_DELETED):
			return "ADM_NOTIFY_PIPE_DELETED";
		case (HCI_CMD_ADM_CLEAR_ALL_PIPE):
			return "ADM_CLEAR_ALL_PIPE";
		case (HCI_CMD_ADM_NOTIFY_ALL_PIPE_CLEARED):
			return "ADM_NOTIFY_ALL_PIPE_CLEARED";
		default:
			return "UNKNOWN";
	}
}

static const char* hcp_msg_inst_evt(nfc_u8 inst)
{
	switch (inst) {
		case (HCI_EVT_HCI_END_OF_OPERATION):
			return "HCI_END_OF_OPERATION";
		case (HCI_EVT_POST_DATA):
			return "POST_DATA";
		case (HCI_EVT_HOT_PLUG):
			return "HOT_PLUG";
		case (HCI_EVT_CONNECTIVITY):
			return "CONNECTIVITY or STANDBY or GTO_TRANSMIT_DATA";
		case (HCI_EVT_TRANSACTION):
			return "TRANSACTION";
		case (HCI_EVT_OPERATION_ENDED):
			return "OPERATION_ENDED";
		default:
			return "UNKNOWN";
	}
}

static const char* hcp_msg_inst_rsp(nfc_u8 inst)
{
	switch (inst) {
		case (HCI_RSP_ANY_OK):
			return "ANY_OK";
		case (HCI_RSP_ANY_E_NOT_CONNECTED):
			return "ANY_E_NOT_CONNECTED";
		case (HCI_RSP_ANY_E_CMD_PAR_UNKNOWN):
			return "ANY_E_CMD_PAR_UNKNOWN";
		case (HCI_RSP_ANY_E_NOK):
			return "ANY_E_NOK";
		case (HCI_RSP_ADM_E_NO_PIPES_AVAILABLE):
			return "ADM_E_NO_PIPES_AVAILABLE";
		case (HCI_RSP_ANY_E_REG_PAR_UNKNOWN):
			return "ANY_E_REG_PAR_UNKNOWN";
		case (HCI_RSP_ANY_E_PIPE_NOT_OPENED):
			return "ANY_E_PIPE_NOT_OPENED";
		case (HCI_RSP_ANY_E_CMD_NOT_SUPPORTED):
			return "ANY_E_CMD_NOT_SUPPORTED";
		case (HCI_RSP_ANY_E_INHIBITED):
			return "ANY_E_INHIBITED";
		case (HCI_RSP_ANY_E_TIMEOUT):
			return "ANY_E_TIMEOUT";
		case (HCI_RSP_ANY_E_REG_ACCESS_DENIED):
			return "ANY_E_REG_ACCESS_DENIED";
		case (HCI_RSP_ANY_E_PIPE_ACCESS_DENIED):
			return "ANY_E_PIPE_ACCESS_DENIED";
		default:
			return "UNKNOWN";
	}
}

static void hci_core_print_data(nfc_u8 *p_data, nfc_u32 data_len)
{
	nfc_u32 i;

	if (data_len == 0)
		return;

	osa_report(INFORMATION, ("HCI: data_len %ld", data_len));

	for (i=0; i<data_len; i+=5) {
		osa_report(INFORMATION, ("HCI: data[%ld]=0x%x, data[%ld]=0x%x, data[%ld]=0x%x, data[%ld]=0x%x, data[%ld]=0x%x",
			(i+0), p_data[i+0], (i+1), p_data[i+1], (i+2), p_data[i+2], (i+3), p_data[i+3], (i+4), p_data[i+4]));
	}
}

static void hci_core_print_hcp_msg(nfc_bool is_rx, nfc_u8 *p_hcp_msg, nfc_u32 hcp_msg_len)
{
	nfc_u8 type, inst;
	nfc_u8 *p_data;
	nfc_u32 data_len;
	const char* direction = ((is_rx) ? ("RX") : ("TX"));
	const char* instruction;

	type = HCI_TYPE_GET(p_hcp_msg);
	inst = HCI_INST_GET(p_hcp_msg);
	p_data = (p_hcp_msg + HCI_HCP_MSG_HDR_SIZE);
	data_len = (hcp_msg_len - HCI_HCP_MSG_HDR_SIZE);

	if (type == HCI_TYPE_CMD)
		instruction = hcp_msg_inst_cmd(inst);
	else if (type == HCI_TYPE_EVT)
		instruction = hcp_msg_inst_evt(inst);
	else if (type == HCI_TYPE_RSP)
		instruction = hcp_msg_inst_rsp(inst);
	else
		instruction = "UNKNOWN";

	osa_report(INFORMATION, ("HCI %s: type %s [0x%x], inst %s [0x%x], data_len %ld", direction, hcp_msg_type(type), type, instruction, inst, data_len));

	if (type == HCI_TYPE_CMD) {
		switch (inst) {
			case (HCI_CMD_ANY_SET_PARAM):
				osa_report(INFORMATION, ("HCI %s: param_id 0x%x, param_len %ld", direction, p_data[0], (data_len-1)));
				hci_core_print_data(p_data+1, (data_len-1));
				break;
			case (HCI_CMD_ANY_GET_PARAM):
				osa_report(INFORMATION, ("HCI %s: param_id 0x%x", direction, p_data[0]));
				break;
			case (HCI_CMD_ADM_CREATE_PIPE):
				osa_report(INFORMATION, ("HCI %s: src_gate_id 0x%x, dst_host_id 0x%x, dst_gate_id 0x%x", direction, p_data[0], p_data[1], p_data[2]));
				break;
			case (HCI_CMD_ADM_DELETE_PIPE):
			case (HCI_CMD_ADM_NOTIFY_PIPE_DELETED):
				osa_report(INFORMATION, ("HCI %s: pipe_id 0x%x", direction, p_data[0]));
				break;
			case (HCI_CMD_ADM_NOTIFY_PIPE_CREATED):
				osa_report(INFORMATION, ("HCI %s: src_host_id 0x%x, src_gate_id 0x%x, dst_host_id 0x%x, dst_gate_id 0x%x, pipe_id 0x%x", direction, p_data[0], p_data[1], p_data[2], p_data[3], p_data[4]));
				break;
			case (HCI_CMD_ADM_CLEAR_ALL_PIPE):
				osa_report(INFORMATION, ("HCI %s: id_ref_data 0x%x", direction, *((nfc_u16*)p_data)));
				break;
			case (HCI_CMD_ADM_NOTIFY_ALL_PIPE_CLEARED):
				osa_report(INFORMATION, ("HCI %s: host_id 0x%x", direction, p_data[0]));
				break;
		}
	}
	else if (type == HCI_TYPE_EVT) {
		switch (inst) {
			case (HCI_EVT_POST_DATA):
				hci_core_print_data(p_data, data_len);
				break;
			case (HCI_EVT_TRANSACTION):
				hci_core_print_data(p_data, data_len);
				break;
		}
	}
	else if (type == HCI_TYPE_RSP) {
		switch (inst) {
			case (HCI_RSP_ANY_OK):
				hci_core_print_data(p_data, data_len);
				break;
		}
	}
}
#else
#define hci_core_print_hcp_msg(is_rx, p_hcp_msg, hcp_msg_len)
#endif

static void hci_core_recv_hcp_msg(struct hci_pipe *pipe, nfc_u8 *p_hcp_msg, nfc_u32 hcp_msg_len)
{
	struct hci_gate *gate;
	nfc_u8 type, inst;
	nfc_u8 *p_data;
	nfc_u32 data_len;

	gate = pipe->local_gate;

	osa_report(INFORMATION, ("hci_core_recv_hcp_msg, gate_id 0x%x, pipe_id 0x%x, hcp_msg_len %ld", gate->id, pipe->id, hcp_msg_len));

	if (hcp_msg_len < HCI_HCP_MSG_HDR_SIZE) {
		osa_report(ERROR, ("hci_core_recv_hcp_msg: too small hcp_msg_len %ld", hcp_msg_len));
		return;
	}

	type = HCI_TYPE_GET(p_hcp_msg);

	if ((type == HCI_TYPE_RSP) && (pipe->h_cmd_watchdog != 0)) {
		/* stop command watchdog on rsp */
		osa_timer_stop(pipe->h_cmd_watchdog);
		pipe->h_cmd_watchdog = 0;
	}

	hci_core_print_hcp_msg(NFC_TRUE, p_hcp_msg, hcp_msg_len);

	inst = HCI_INST_GET(p_hcp_msg);
	p_data = (p_hcp_msg + HCI_HCP_MSG_HDR_SIZE);
	data_len = (hcp_msg_len - HCI_HCP_MSG_HDR_SIZE);

	switch (type) {
		case (HCI_TYPE_CMD):
			if (gate->callbacks->cmd_recv)
				gate->callbacks->cmd_recv(gate->h_caller, pipe, inst, p_data, data_len);
			break;
		case (HCI_TYPE_EVT):
			if (gate->callbacks->evt_recv)
				gate->callbacks->evt_recv(gate->h_caller, pipe, inst, p_data, data_len);
			break;
		case (HCI_TYPE_RSP):
			if (gate->callbacks->rsp_recv)
				gate->callbacks->rsp_recv(gate->h_caller, pipe, inst, p_data, data_len);
			break;
		default:
			osa_report(ERROR, ("hci_core_recv_hcp_msg, invalid type 0x%x", type));
			break;
	}
}

static void hci_core_recv_hcp_pkt(struct hci_core_context *core_ctx, nfc_u8 *p_hcp_pkt, nfc_u32 hcp_pkt_len)
{
	struct hci_pipe *pipe;
	nfc_u32 total_rx_len, frag_len;
	nfc_u8 *p_frag;
	nfc_u8 *total_rx;
	nfc_u8 pipe_id, cb;

	pipe_id = HCI_PID_GET(p_hcp_pkt);
	cb = HCI_CB_GET(p_hcp_pkt);

	osa_report(INFORMATION, ("hci_core_recv_hcp_pkt, pipe_id 0x%x, cb 0x%x, hcp_pkt_len %ld", pipe_id, cb, hcp_pkt_len));

	if (hcp_pkt_len < HCI_HCP_PKT_HDR_SIZE) {
		osa_report(ERROR, ("hci_core_recv_hcp_pkt: too small hcp_pkt_len %ld", hcp_pkt_len));
		return;
	}

	pipe = hci_core_find_pipe(core_ctx, pipe_id);
	if (!pipe) {
		osa_report(ERROR, ("hci_core_recv_hcp_pkt: unable to find pipe_id 0x%x", pipe_id));
		return;
	}

	p_frag = (p_hcp_pkt + HCI_HCP_PKT_HDR_SIZE);
	frag_len = (hcp_pkt_len - HCI_HCP_PKT_HDR_SIZE);

	if (!pipe->rx) {
		/* this is the first fragment */
		if (cb == HCI_CB_LAST_FRAG) {
			hci_core_recv_hcp_msg(pipe, p_frag, frag_len);
		} else {
			/* store data and wait for next fragment */
			pipe->rx = (nfc_u8*)osa_mem_alloc(frag_len);
			if (!pipe->rx) {
				osa_report(ERROR, ("hci_core_recv_hcp_pkt: failed alloc new rx buff for first fragment"));
				return;
			}
			osa_mem_copy(pipe->rx, p_frag, frag_len);
			pipe->rx_len = frag_len;
		}
	} else {
		/* we've a previous rx fragment => combine them together */
		total_rx_len = (pipe->rx_len + frag_len);
		total_rx = (nfc_u8*)osa_mem_alloc(total_rx_len);
		if (!total_rx) {
			osa_report(ERROR, ("hci_core_recv_hcp_pkt: failed alloc new rx buff"));
			return;
		}
		osa_mem_copy(total_rx, pipe->rx, pipe->rx_len);
		osa_mem_copy((total_rx + pipe->rx_len), p_frag, frag_len);
		osa_mem_free(pipe->rx);

		if (cb == HCI_CB_LAST_FRAG) {
			pipe->rx = 0;
			pipe->rx_len = 0;

			hci_core_recv_hcp_msg(pipe, total_rx, total_rx_len);
			osa_mem_free(total_rx);
		} else {
			/* store data and wait for next fragment */
			pipe->rx = total_rx;
			pipe->rx_len = total_rx_len;
		}
	}
}

static void hci_core_vs_ntf_rx_cb(nfc_handle_t usr_param, nfc_u16 opcode, nfc_u8 *p_data, nfc_u8 data_len)
{
	struct hci_core_context *core_ctx = (struct hci_core_context*)usr_param;

	hci_core_recv_hcp_pkt(core_ctx, p_data, data_len);
}

struct hci_core_context* hci_core_create(nfc_handle_t h_nci, nfc_handle_t h_osa, struct hci_core_callbacks *callbacks, nfc_handle_t h_caller)
{
	struct hci_core_context *core_ctx;

	osa_report(INFORMATION, ("hci_core_create"));

	core_ctx = (struct hci_core_context*)osa_mem_alloc(sizeof(struct hci_core_context));
	if (!core_ctx)
		goto exit;

	osa_mem_zero(core_ctx, sizeof(struct hci_core_context));

	core_ctx->h_nci = h_nci;
	core_ctx->h_osa = h_osa;
	dll_initialize_head(&core_ctx->gate_list);

	core_ctx->callbacks = callbacks;
	core_ctx->h_caller = h_caller;

	nci_register_vendor_specific_ntf_cb(h_nci, NCI_VS_HCI_RCV_HCP_PACKET_NTF, hci_core_vs_ntf_rx_cb, core_ctx);

exit:
	return core_ctx;
}

void hci_core_destroy(struct hci_core_context *core_ctx)
{
	osa_report(INFORMATION, ("hci_core_destroy"));

	if (core_ctx) {
		nci_unregister_vendor_specific_ntf_cb(core_ctx->h_nci, NCI_VS_HCI_RCV_HCP_PACKET_NTF, hci_core_vs_ntf_rx_cb);

		osa_mem_free(core_ctx);
	}
}

static void hci_core_nfcee_mode_set_complete(nfc_handle_t h_cb, nfc_u16 opcode, op_rsp_param_u *p_op_rsp)
{
	struct hci_core_context *core_ctx = (struct hci_core_context*)h_cb;

	osa_report(INFORMATION, ("hci_core_nfcee_mode_set_complete, status = %d, new_mode = %d", p_op_rsp->nfcee.status, p_op_rsp->nfcee.new_mode));

	if (p_op_rsp->nfcee.new_mode == E_SWITCH_RF_COMM) {
		if (core_ctx->callbacks->activated)
			core_ctx->callbacks->activated(core_ctx->h_caller, p_op_rsp->nfcee.status);
	} else { /* E_SWITCH_OFF */
		if (core_ctx->callbacks->deactivated)
			core_ctx->callbacks->deactivated(core_ctx->h_caller, p_op_rsp->nfcee.status);
	}
}

static void hci_core_nfcee_mode_set(struct hci_core_context *core_ctx, nfc_u8 nfcee_id, enum nfcee_switch_mode new_mode)
{
	op_param_u u_param;

	u_param.nfcee.nfcee_id = nfcee_id;
	u_param.nfcee.new_mode = new_mode;

	nci_start_operation(core_ctx->h_nci,
		NCI_OPERATION_NFCEE_SWITCH_MODE,
		&u_param,
		NULL,
		0,
		hci_core_nfcee_mode_set_complete,
		core_ctx);
}

static void hci_core_send_hcp_pkt(struct hci_core_context *core_ctx, nfc_u8 pipe_id, nfc_u8 cb, nfc_u8 *p_hcp_pkt, nfc_u32 hcp_pkt_len)
{
	osa_report(INFORMATION, ("hci_core_send_hcp_pkt, pipe_id 0x%x, cb 0x%x, hcp_pkt_len %ld", pipe_id, cb, hcp_pkt_len));

	HCI_PID_SET(p_hcp_pkt, pipe_id);
	HCI_CB_SET(p_hcp_pkt, cb);

	nci_send_vendor_specific_cmd(core_ctx->h_nci, NCI_VS_HCI_SEND_HCP_PACKET_CMD, p_hcp_pkt, hcp_pkt_len, NULL, NULL);
}

static void hci_core_send_evt_hot_plug(struct hci_core_context *core_ctx)
{
	nfc_u8 evt_hot_plug[HCI_HCP_PKT_HDR_SIZE + HCI_HCP_MSG_HDR_SIZE];

	osa_report(INFORMATION, ("hci_core_send_evt_hot_plug"));

	osa_mem_zero(evt_hot_plug, (HCI_HCP_PKT_HDR_SIZE + HCI_HCP_MSG_HDR_SIZE));
	HCI_TYPE_SET((evt_hot_plug + HCI_HCP_PKT_HDR_SIZE), HCI_TYPE_EVT);
	HCI_INST_SET((evt_hot_plug + HCI_HCP_PKT_HDR_SIZE), HCI_EVT_HOT_PLUG);

	hci_core_send_hcp_pkt(core_ctx, HCI_PIPE_ID_ADMIN, HCI_CB_LAST_FRAG, evt_hot_plug, sizeof(evt_hot_plug));
}

nfc_status hci_core_activate(struct hci_core_context *core_ctx, nfc_u8 nfcee_id)
{
	osa_report(INFORMATION, ("hci_core_activate, nfcee_id 0x%x", nfcee_id));

	hci_core_nfcee_mode_set(core_ctx, nfcee_id, E_SWITCH_RF_COMM);

	/* ILAN: evt hot plug is disabled untill FW will support this */
//	hci_core_send_evt_hot_plug(core_ctx);

	return NFC_RES_OK;
}

nfc_status hci_core_deactivate(struct hci_core_context *core_ctx, nfc_u8 nfcee_id)
{
	osa_report(INFORMATION, ("hci_core_deactivate, nfcee_id 0x%x", nfcee_id));

	hci_core_nfcee_mode_set(core_ctx, nfcee_id, E_SWITCH_OFF);

	/* ILAN: evt hot plug is disabled untill FW will support this */
//	hci_core_send_evt_hot_plug(core_ctx);

	return NFC_RES_OK;
}

void hci_core_register_gate(struct hci_core_context *core_ctx, struct hci_gate *gate)
{
	osa_report(INFORMATION, ("hci_core_register_gate, gate_id 0x%x", gate->id));

	dll_insert_tail(&core_ctx->gate_list, &gate->node);
}

void hci_core_unregister_gate(struct hci_core_context *core_ctx, struct hci_gate *gate)
{
	osa_report(INFORMATION, ("hci_core_unregister_gate, gate_id 0x%x", gate->id));

	if (dll_is_node_in_list(&core_ctx->gate_list, &gate->node))
		dll_remove_node(&gate->node);
}

static void hci_core_cmd_watchdog_cb(nfc_handle_t h_cb)
{
	struct hci_pipe *pipe = (struct hci_pipe*)h_cb;
	struct hci_gate *gate = pipe->local_gate;

	osa_report(ERROR, ("hci_core_cmd_watchdog_cb, pipe_id 0x%x", pipe->id));

	pipe->h_cmd_watchdog = 0;

	if (gate->callbacks->cmd_timeout)
		gate->callbacks->cmd_timeout(gate->h_caller, pipe);
}

struct hci_gate* hci_core_find_gate(struct hci_core_context *core_ctx, nfc_u8 gate_id)
{
	struct hci_gate *gate;

	dll_iterate(&core_ctx->gate_list, gate, struct hci_gate *) {
		if (gate->id == gate_id)
			return gate;
	}

	return 0;
}

struct hci_pipe* hci_core_find_pipe(struct hci_core_context *core_ctx, nfc_u8 pipe_id)
{
	struct hci_gate *gate;
	struct hci_pipe *pipe;

	dll_iterate(&core_ctx->gate_list, gate, struct hci_gate *) {
		dll_iterate(&gate->pipe_list, pipe, struct hci_pipe *) {
			if (pipe->id == pipe_id)
				return pipe;
		}
	}

	return 0;
}

struct hci_pipe** hci_core_list_all_pipes(struct hci_core_context *core_ctx, nfc_u8 remote_host_id, nfc_u8 *num_pipes)
{
	struct hci_gate *gate;
	struct hci_pipe *pipe;
	struct hci_pipe **pipes = 0;
	nfc_u8 n_pipes = 0;

	dll_iterate(&core_ctx->gate_list, gate, struct hci_gate *) {
		dll_iterate(&gate->pipe_list, pipe, struct hci_pipe *) {
			if (pipe->remote_host_id == remote_host_id)
				n_pipes++;
		}
	}

	if (n_pipes > 0) {
		pipes = (struct hci_pipe**)osa_mem_alloc(sizeof(struct hci_pipe*) * n_pipes);
		if (pipes == 0) {
			osa_report(ERROR, ("hci_core_list_all_pipes: failed alloc pipes array"));
			*num_pipes = 0;
			return 0;
		}

		n_pipes = 0;

		dll_iterate(&core_ctx->gate_list, gate, struct hci_gate *) {
			dll_iterate(&gate->pipe_list, pipe, struct hci_pipe *) {
				if (pipe->remote_host_id == remote_host_id)
					pipes[n_pipes++] = pipe;
			}
		}
	}

	*num_pipes = n_pipes;

	return pipes;
}

static nfc_u8 hci_core_get_num_open_pipes(struct hci_core_context *core_ctx, struct hci_gate *gate)
{
	struct hci_pipe *pipe;
	nfc_u8 num_open_pipes = 0;

	dll_iterate(&gate->pipe_list, pipe, struct hci_pipe *) {
		if (pipe->state == HCI_PIPE_STATE_OPEN)
			num_open_pipes++;
	}

	return num_open_pipes;
}

nfc_status hci_core_recv_open_pipe(struct hci_core_context *core_ctx, struct hci_pipe *pipe, nfc_u32 data_len)
{
	struct hci_rsp_any_open_pipe *rsp;
	struct hcp_msg* msg;

	if (data_len != 0) {
		hci_core_send_rsp_no_param(core_ctx, pipe, HCI_RSP_ANY_E_CMD_PAR_UNKNOWN);
		return NFC_RES_ERROR;
	}

	if (hci_pipe_get_state(pipe) != HCI_PIPE_STATE_CLOSED) {
		hci_core_send_rsp_no_param(core_ctx, pipe, HCI_RSP_ANY_E_NOK);
		return NFC_RES_STATE_ERROR;
	}

	msg = hcp_msg_alloc(HCI_TYPE_RSP, HCI_RSP_ANY_OK, sizeof(struct hci_rsp_any_open_pipe));
	if (!msg)
		return NFC_RES_MEM_ERROR;

	rsp = (struct hci_rsp_any_open_pipe*)hcp_msg_get_data(msg);

	rsp->num_open_pipes = hci_core_get_num_open_pipes(core_ctx, hci_pipe_get_local_gate(pipe));

	hci_pipe_set_state(pipe, HCI_PIPE_STATE_OPEN);

	hci_core_persistence_store(core_ctx);

	hci_core_send_hcp_msg(core_ctx, pipe, msg);

	return NFC_RES_OK;
}

nfc_status hci_core_recv_close_pipe(struct hci_core_context *core_ctx, struct hci_pipe *pipe, nfc_u32 data_len)
{
	if (data_len != 0) {
		hci_core_send_rsp_no_param(core_ctx, pipe, HCI_RSP_ANY_E_CMD_PAR_UNKNOWN);
		return NFC_RES_ERROR;
	}

	if (hci_pipe_get_state(pipe) != HCI_PIPE_STATE_OPEN) {
		hci_core_send_rsp_no_param(core_ctx, pipe, HCI_RSP_ANY_E_PIPE_NOT_OPENED);
		return NFC_RES_STATE_ERROR;
	}

	hci_pipe_set_state(pipe, HCI_PIPE_STATE_CLOSED);

	hci_core_persistence_store(core_ctx);

	hci_core_send_rsp_no_param(core_ctx, pipe, HCI_RSP_ANY_OK);

	return NFC_RES_OK;
}

void hci_core_send_hcp_msg(struct hci_core_context *core_ctx, struct hci_pipe *pipe, struct hcp_msg* msg)
{
	nfc_u32 total_msg_len = (msg->buff_len - HCI_HCP_PKT_HDR_SIZE);
	nfc_u8 *p_hcp_msg = (msg->buff + HCI_HCP_PKT_HDR_SIZE);
	nfc_u8 *p_hcp_pkt;
	nfc_u32 frag_len, hcp_pkt_len;

	osa_report(INFORMATION, ("hci_core_send_hcp_msg, gate_id 0x%x, pipe_id 0x%x, hcp_msg_len %ld", pipe->local_gate->id, pipe->id, total_msg_len));

	hci_core_print_hcp_msg(NFC_FALSE, p_hcp_msg , total_msg_len);

	if (hcp_msg_get_type(msg) == HCI_TYPE_CMD) {
		/* start command watchdog on cmd */
		pipe->h_cmd_watchdog = osa_timer_start(core_ctx->h_osa, HCI_CMD_TIMEOUT, hci_core_cmd_watchdog_cb, pipe);
	}

	/* check if the message need to be fragmented */
	if (total_msg_len <= HCI_HCP_MSG_MAX_SIZE) {
		/* no need to fragment message */
		hci_core_send_hcp_pkt(core_ctx, pipe->id, HCI_CB_LAST_FRAG, msg->buff, msg->buff_len);
	} else {
		while (total_msg_len) {
			frag_len = OSA_MIN(total_msg_len, HCI_HCP_MSG_MAX_SIZE);
			hcp_pkt_len = (HCI_HCP_PKT_HDR_SIZE + frag_len);

			p_hcp_pkt = (nfc_u8*)osa_mem_alloc(hcp_pkt_len);
			if (!p_hcp_pkt) {
				osa_report(ERROR, ("hci_core_send_hcp_msg: failed alloc new tx buff"));
				return;
			}

			/* zero pkt header */
			osa_mem_zero(p_hcp_pkt, HCI_HCP_PKT_HDR_SIZE);

			/* copy the data */
			osa_mem_copy((p_hcp_pkt + HCI_HCP_PKT_HDR_SIZE), p_hcp_msg, frag_len);

			hci_core_send_hcp_pkt(core_ctx, pipe->id, ((total_msg_len == frag_len) ? (HCI_CB_LAST_FRAG) : (HCI_CB_CONT)), p_hcp_pkt, hcp_pkt_len);

			/* advance to next fragment */
			p_hcp_msg += frag_len;
			total_msg_len -= frag_len;
		}
	}

	hcp_msg_free(msg);
}

void hci_core_send_rsp_no_param(struct hci_core_context *core_ctx, struct hci_pipe *pipe, nfc_u8 rsp_inst_code)
{
	struct hcp_msg* msg;

	msg = hcp_msg_alloc(HCI_TYPE_RSP, rsp_inst_code, 0); /* no data */
	if (!msg)
		return;

	hci_core_send_hcp_msg(core_ctx, pipe, msg);
}

nfc_status hci_core_send_open_pipe(struct hci_core_context *core_ctx, struct hci_pipe *pipe)
{
	struct hcp_msg* msg;

	if (hci_pipe_get_state(pipe) != HCI_PIPE_STATE_CLOSED)
		return NFC_RES_STATE_ERROR;

	if (hci_pipe_get_pending_cmd(pipe) != HCI_CMD_NONE)
		return NFC_RES_ERROR;

	msg = hcp_msg_alloc(HCI_TYPE_CMD, HCI_CMD_ANY_OPEN_PIPE, 0);
	if (!msg)
		return NFC_RES_MEM_ERROR;

	hci_pipe_set_state(pipe, HCI_PIPE_STATE_OPENING);

	hci_pipe_set_pending_cmd(pipe, HCI_CMD_ANY_OPEN_PIPE);

	hci_core_send_hcp_msg(core_ctx, pipe, msg);

	return NFC_RES_PENDING;
}

nfc_status hci_core_send_close_pipe(struct hci_core_context *core_ctx, struct hci_pipe *pipe)
{
	struct hcp_msg* msg;

	if (hci_pipe_get_state(pipe) != HCI_PIPE_STATE_OPEN)
		return NFC_RES_STATE_ERROR;

	if (hci_pipe_get_pending_cmd(pipe) != HCI_CMD_NONE)
		return NFC_RES_ERROR;

	msg = hcp_msg_alloc(HCI_TYPE_CMD, HCI_CMD_ANY_CLOSE_PIPE, 0);
	if (!msg)
		return NFC_RES_MEM_ERROR;

	hci_pipe_set_state(pipe, HCI_PIPE_STATE_CLOSING);

	hci_pipe_set_pending_cmd(pipe, HCI_CMD_ANY_CLOSE_PIPE);

	hci_core_send_hcp_msg(core_ctx, pipe, msg);

	return NFC_RES_PENDING;
}

nfc_status hci_core_send_get_param(struct hci_core_context *core_ctx, struct hci_pipe *pipe, nfc_u8 param_id)
{
	struct hci_cmd_any_get_param *cmd;
	struct hcp_msg* msg;

	if (hci_pipe_get_state(pipe) != HCI_PIPE_STATE_OPEN)
		return NFC_RES_STATE_ERROR;

	if (hci_pipe_get_pending_cmd(pipe) != HCI_CMD_NONE)
		return NFC_RES_ERROR;

	msg = hcp_msg_alloc(HCI_TYPE_CMD, HCI_CMD_ANY_GET_PARAM, sizeof(struct hci_cmd_any_get_param));
	if (!msg)
		return NFC_RES_MEM_ERROR;

	cmd = (struct hci_cmd_any_get_param*)hcp_msg_get_data(msg);

	cmd->param_id = param_id;

	hci_pipe_set_pending_cmd(pipe, HCI_CMD_ANY_GET_PARAM);

	hci_core_send_hcp_msg(core_ctx, pipe, msg);

	return NFC_RES_PENDING;
}

void hci_core_persistence_store(struct hci_core_context *core_ctx)
{
	struct hci_gate *gate;
	struct hci_pipe *pipe;
	struct hci_pipe_persistent_record *record;
	nfc_u8 *buff;
	nfc_u16 buff_len = 0;
	nfc_status rc;

	buff = (nfc_u8*)osa_mem_alloc(MAX_PROP_LEN);
	if (!buff) {
		osa_report(ERROR, ("hci_core_persistence_store: failed alloc persistence buff"));
		return;
	}

	dll_iterate(&core_ctx->gate_list, gate, struct hci_gate *) {
		dll_iterate(&gate->pipe_list, pipe, struct hci_pipe *) {
			if ((buff_len + sizeof(struct hci_pipe_persistent_record)) > MAX_PROP_LEN) {
				osa_report(ERROR, ("hci_core_persistence_store: not enough room in persistence buff"));
				goto exit;
			}

			record = (struct hci_pipe_persistent_record*)(buff + buff_len);

			record->gate_id = pipe->local_gate->id;
			record->pipe_id = pipe->id;
			record->remote_host_id = pipe->remote_host_id;
			record->state = pipe->state;

			osa_report(INFORMATION, ("hci_core_persistence_store: gate_id 0x%x, pipe_id 0x%x, remote_host_id 0x%x, state 0x%x",
					record->gate_id, record->pipe_id, record->remote_host_id, record->state));

			buff_len += sizeof(struct hci_pipe_persistent_record);
		}
	}

	if (buff_len > 0) {
		rc = osa_setprop(HCI_DH_PERSISTENCE_PROP, buff, buff_len);
		if (rc == NFC_RES_OK)
			osa_report(INFORMATION, ("hci_core_persistence_store: osa_setprop succeed"));
		else
			osa_report(ERROR, ("hci_core_persistence_store: osa_setprop failed, rc 0x%x", rc));
	}

exit:
	osa_mem_free(buff);
}

void hci_core_persistence_restore(struct hci_core_context *core_ctx)
{
	struct hci_gate *gate;
	struct hci_pipe *pipe;
	struct hci_pipe_persistent_record *record;
	nfc_u8 *buff;
	nfc_u16 buff_len = MAX_PROP_LEN;
	nfc_status rc;

	buff = (nfc_u8*)osa_mem_alloc(MAX_PROP_LEN);
	if (!buff) {
		osa_report(ERROR, ("hci_core_persistence_restore: failed alloc persistence buff"));
		return;
	}

	rc = osa_getprop(HCI_DH_PERSISTENCE_PROP, buff, &buff_len);
	if (rc == NFC_RES_OK) {
		osa_report(INFORMATION, ("hci_core_persistence_restore: osa_getprop succeed, buff_len %d", buff_len));
	} else {
		osa_report(ERROR, ("hci_core_persistence_restore: osa_getprop failed, rc 0x%x", rc));
		goto exit;
	}

	if (buff_len % sizeof(struct hci_pipe_persistent_record)) {
		osa_report(ERROR, ("hci_core_persistence_restore: invalid buff_len %d", buff_len));
		goto exit_clear_storage;
	}

	record = (struct hci_pipe_persistent_record*)buff;

	while (buff_len) {
		osa_report(INFORMATION, ("hci_core_persistence_restore: gate_id 0x%x, pipe_id 0x%x, remote_host_id 0x%x, state 0x%x",
				record->gate_id, record->pipe_id, record->remote_host_id, record->state));

		gate = hci_core_find_gate(core_ctx, record->gate_id);
		if (!gate) {
			osa_report(ERROR, ("hci_core_persistence_restore: invalid gate_id 0x%x", record->gate_id));
			goto exit_clear_storage;
		}

		pipe = hci_core_find_pipe(core_ctx, record->pipe_id);
		if (pipe) {
			if (pipe->id == HCI_PIPE_ID_ADMIN) {
				/* static admin pipe => only update state */
				pipe->state = record->state;
			} else {
				osa_report(ERROR, ("hci_core_persistence_restore: pipe_id 0x%x already exists", record->pipe_id));
				goto exit_clear_storage;
			}
		} else {
			/* create new dynamic pipe */
			pipe = hci_pipe_alloc_and_attach(gate, record->pipe_id, record->remote_host_id, record->state);
			if (!pipe) {
				osa_report(ERROR, ("hci_core_persistence_restore: failed alloc pipe"));
				goto exit;
			}
		}

		record++;
		buff_len -= sizeof(struct hci_pipe_persistent_record);
	}

	goto exit;

exit_clear_storage:
	osa_setprop(HCI_DH_PERSISTENCE_PROP, buff, 0);
	osa_report(ERROR, ("hci_core_persistence_restore: cleared persistence storage"));

exit:
	osa_mem_free(buff);
}

struct hci_gate* hci_gate_alloc(nfc_u8 id, struct hci_gate_callbacks *callbacks, nfc_handle_t h_caller)
{
	struct hci_gate* gate;

	osa_report(INFORMATION, ("hci_core_alloc_gate, gate_id 0x%x", id));

	gate = (struct hci_gate*)osa_mem_alloc(sizeof(struct hci_gate));
	if (!gate)
		goto exit;

	osa_mem_zero(gate, sizeof(struct hci_gate));

	dll_initialize_node(&gate->node);
	gate->id = id;
	gate->callbacks = callbacks;
	gate->h_caller = h_caller;
	dll_initialize_head(&gate->pipe_list);

exit:
	return gate;
}

void hci_gate_free(struct hci_gate *gate)
{
	struct hci_pipe *pipe;
	struct hci_pipe *pipe_next;

	osa_report(INFORMATION, ("hci_core_free_gate, gate_id 0x%x", gate->id));

	if (gate) {
		/* free all associated pipes */
		dll_iterate_safe(&gate->pipe_list, pipe, pipe_next, struct hci_pipe *) {
			hci_pipe_detach_and_free(pipe);
		}

		osa_mem_free(gate);
	}
}

inline nfc_u8 hci_gate_get_id(struct hci_gate *gate)
{
	return gate->id;
}

struct hci_pipe* hci_pipe_alloc_and_attach(struct hci_gate *local_gate, nfc_u8 id, nfc_u8 remote_host_id, nfc_u8 initial_state)
{
	struct hci_pipe* pipe;

	osa_report(INFORMATION, ("hci_pipe_alloc_and_attach, gate_id 0x%x, pipe_id 0x%x, remote_host_id 0x%x, initial_state 0x%x", local_gate->id, id, remote_host_id, initial_state));

	pipe = (struct hci_pipe*)osa_mem_alloc(sizeof(struct hci_pipe));
	if (!pipe)
		goto exit;

	osa_mem_zero(pipe, sizeof(struct hci_pipe));

	dll_initialize_node(&pipe->node);
	dll_insert_tail(&local_gate->pipe_list, &pipe->node);
	pipe->local_gate = local_gate;
	pipe->id = id;
	pipe->remote_host_id = remote_host_id;
	pipe->state = initial_state;
	pipe->pending_cmd = HCI_CMD_NONE;

exit:
	return pipe;
}

void hci_pipe_detach_and_free(struct hci_pipe *pipe)
{
	struct hci_gate* gate;

	osa_report(INFORMATION, ("hci_pipe_detach_and_free, gate_id 0x%x, pipe_id 0x%x", pipe->local_gate->id, pipe->id));

	if (pipe) {
		if (pipe->h_cmd_watchdog != 0) {
			osa_timer_stop(pipe->h_cmd_watchdog);
			pipe->h_cmd_watchdog = 0;
		}

		gate = pipe->local_gate;

		if (dll_is_node_in_list(&gate->pipe_list, &pipe->node))
			dll_remove_node(&pipe->node);

		if (pipe->rx)
			osa_mem_free(pipe->rx);

		osa_mem_free(pipe);
	}
}

inline nfc_u8 hci_pipe_get_id(struct hci_pipe *pipe)
{
	return pipe->id;
}

inline nfc_u8 hci_pipe_get_remote_host_id(struct hci_pipe *pipe)
{
	return pipe->remote_host_id;
}

inline struct hci_gate* hci_pipe_get_local_gate(struct hci_pipe *pipe)
{
	return pipe->local_gate;
}

inline nfc_u8 hci_pipe_get_state(struct hci_pipe* pipe)
{
	return pipe->state;
}

inline void hci_pipe_set_state(struct hci_pipe* pipe, nfc_u8 new_state)
{
	osa_report(INFORMATION, ("hci_pipe_set_state, pipe_id 0x%x, old_state 0x%x, new_state 0x%x", pipe->id, pipe->state, new_state));

	pipe->state = new_state;
}

inline nfc_u8 hci_pipe_get_pending_cmd(struct hci_pipe* pipe)
{
	return pipe->pending_cmd;
}

inline void hci_pipe_set_pending_cmd(struct hci_pipe* pipe, nfc_u8 pending_cmd)
{
	pipe->pending_cmd = pending_cmd;
}

struct hcp_msg* hcp_msg_alloc(nfc_u8 type, nfc_u8 inst, nfc_u32 data_len)
{
	struct hcp_msg *msg;
	nfc_u8 *msg_hdr;

	msg = (struct hcp_msg*)osa_mem_alloc(sizeof(struct hcp_msg));
	if (!msg)
		goto exit;

	msg->buff_len = (data_len + HCI_HCP_PKT_HDR_SIZE + HCI_HCP_MSG_HDR_SIZE);
	msg->buff = (nfc_u8*)osa_mem_alloc(msg->buff_len);
	if (!msg->buff)
		goto exit_free;

	osa_mem_zero(msg->buff, msg->buff_len);

	msg_hdr = (msg->buff + HCI_HCP_PKT_HDR_SIZE);

	HCI_TYPE_SET(msg_hdr, type);
	HCI_INST_SET(msg_hdr, inst);

	return msg;

exit_free:
	osa_mem_free(msg);

exit:
	return 0;
}

void hcp_msg_free(struct hcp_msg *msg)
{
	if (msg) {
		if (msg->buff)
			osa_mem_free(msg->buff);

		osa_mem_free(msg);
	}
}

inline nfc_u8 hcp_msg_get_type(struct hcp_msg *msg)
{
	return HCI_TYPE_GET(msg->buff + HCI_HCP_PKT_HDR_SIZE);
}

inline nfc_u8 hcp_msg_get_inst(struct hcp_msg *msg)
{
	return HCI_INST_GET(msg->buff + HCI_HCP_PKT_HDR_SIZE);
}

inline nfc_u32 hcp_msg_get_data_len(struct hcp_msg *msg)
{
	return (msg->buff_len - (HCI_HCP_PKT_HDR_SIZE + HCI_HCP_MSG_HDR_SIZE));
}

inline nfc_u8* hcp_msg_get_data(struct hcp_msg *msg)
{
	return (msg->buff + HCI_HCP_PKT_HDR_SIZE + HCI_HCP_MSG_HDR_SIZE);
}
