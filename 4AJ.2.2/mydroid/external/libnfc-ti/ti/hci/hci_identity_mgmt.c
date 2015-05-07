/*
 * hci_identity_mgmt.c
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
  This source file contains the implementation of HCI Network identity mgmt gate.
*******************************************************************************/
#include "hci_int.h"
#include "nfc_types.h"
#include "nci_utils.h"
#include "nfc_os_adapt.h"


/******************************************************************************/
/* HCI Identity Mgmt Structures and Types                                             */
/******************************************************************************/

struct hci_identity_mgmt_context {
	struct hci_core_context *core_ctx;
	struct hci_gate *gate;	/* Local Identity Management gate */
	struct hci_identity_mgmt_callbacks *callbacks;
	nfc_handle_t h_caller;
	nfc_u8 *gates_list;
	nfc_u32 gates_list_len;
};


/******************************************************************************/
/* HCI Function Definitions                                                   */
/******************************************************************************/

static void hci_identity_mgmt_handle_get_param(struct hci_identity_mgmt_context *identity_mgmt_ctx, struct hci_pipe *pipe, nfc_u8 *p_data, nfc_u32 data_len)
{
	struct hci_cmd_any_get_param *cmd = (struct hci_cmd_any_get_param*)p_data;
	struct hcp_msg* msg;
	nfc_u8* p_value;

	if (data_len != sizeof(struct hci_cmd_any_get_param)) {
		hci_core_send_rsp_no_param(identity_mgmt_ctx->core_ctx, pipe, HCI_RSP_ANY_E_CMD_PAR_UNKNOWN);
		return;
	}

	if (hci_pipe_get_state(pipe) != HCI_PIPE_STATE_OPEN) {
		hci_core_send_rsp_no_param(identity_mgmt_ctx->core_ctx, pipe, HCI_RSP_ANY_E_PIPE_NOT_OPENED);
		return;
	}

	if (cmd->param_id != HCI_REGISTRY_PARAM_ID_GATES_LIST) {
		hci_core_send_rsp_no_param(identity_mgmt_ctx->core_ctx, pipe, HCI_RSP_ANY_E_REG_PAR_UNKNOWN);
		return;
	}

	msg = hcp_msg_alloc(HCI_TYPE_RSP, HCI_RSP_ANY_OK, identity_mgmt_ctx->gates_list_len);
	if (!msg)
		return;

	p_value = hcp_msg_get_data(msg);
	osa_mem_copy(p_value, identity_mgmt_ctx->gates_list, identity_mgmt_ctx->gates_list_len);

	hci_core_send_hcp_msg(identity_mgmt_ctx->core_ctx, pipe, msg);
}

static void hci_identity_mgmt_handle_open_pipe(struct hci_identity_mgmt_context *identity_mgmt_ctx, struct hci_pipe *pipe, nfc_u8 *p_data, nfc_u32 data_len)
{
	if (hci_core_recv_open_pipe(identity_mgmt_ctx->core_ctx, pipe, data_len) != NFC_RES_OK)
		return;

	if (identity_mgmt_ctx->callbacks->pipe_opened)
		identity_mgmt_ctx->callbacks->pipe_opened(identity_mgmt_ctx->h_caller, pipe, NFC_RES_OK);
}

static void hci_identity_mgmt_handle_close_pipe(struct hci_identity_mgmt_context *identity_mgmt_ctx, struct hci_pipe *pipe, nfc_u8 *p_data, nfc_u32 data_len)
{
	if (hci_core_recv_close_pipe(identity_mgmt_ctx->core_ctx, pipe, data_len) != NFC_RES_OK)
		return;

	if (identity_mgmt_ctx->callbacks->pipe_closed)
		identity_mgmt_ctx->callbacks->pipe_closed(identity_mgmt_ctx->h_caller, pipe, NFC_RES_OK);
}

static void hci_identity_mgmt_cmd_recv_cb(nfc_handle_t h_caller, struct hci_pipe *pipe, nfc_u8 inst, nfc_u8 *p_data, nfc_u32 data_len)
{
	struct hci_identity_mgmt_context *identity_mgmt_ctx = (struct hci_identity_mgmt_context*)h_caller;

	switch (inst) {
		case (HCI_CMD_ANY_GET_PARAM):
			hci_identity_mgmt_handle_get_param(identity_mgmt_ctx, pipe, p_data, data_len);
			break;
		case (HCI_CMD_ANY_OPEN_PIPE):
			hci_identity_mgmt_handle_open_pipe(identity_mgmt_ctx, pipe, p_data, data_len);
			break;
		case (HCI_CMD_ANY_CLOSE_PIPE):
			hci_identity_mgmt_handle_close_pipe(identity_mgmt_ctx, pipe, p_data, data_len);
			break;
		default:
			hci_core_send_rsp_no_param(identity_mgmt_ctx->core_ctx, pipe, HCI_RSP_ANY_E_CMD_NOT_SUPPORTED);
			break;
	}
}

static struct hci_gate_callbacks hci_identity_mgmt_gate_callbacks = {
	.cmd_recv = hci_identity_mgmt_cmd_recv_cb,
	.evt_recv = 0,
	.rsp_recv = 0,
	.cmd_timeout = 0,
};

struct hci_identity_mgmt_context* hci_identity_mgmt_create(struct hci_core_context *core_ctx, struct hci_identity_mgmt_callbacks *callbacks, nfc_handle_t h_caller)
{
	struct hci_identity_mgmt_context *identity_mgmt_ctx;

	osa_report(INFORMATION, ("hci_identity_mgmt_create"));

	identity_mgmt_ctx = (struct hci_identity_mgmt_context*)osa_mem_alloc(sizeof(struct hci_identity_mgmt_context));
	if (!identity_mgmt_ctx)
		goto exit;

	osa_mem_zero(identity_mgmt_ctx, sizeof(struct hci_identity_mgmt_context));

	identity_mgmt_ctx->core_ctx = core_ctx;

	identity_mgmt_ctx->gate = hci_gate_alloc(HCI_GATE_ID_IDENTITY_MGMT, &hci_identity_mgmt_gate_callbacks, identity_mgmt_ctx);
	if (!identity_mgmt_ctx->gate)
		goto exit_free_ctx;

	hci_core_register_gate(core_ctx, identity_mgmt_ctx->gate);

	identity_mgmt_ctx->callbacks = callbacks;
	identity_mgmt_ctx->h_caller = h_caller;

	return identity_mgmt_ctx;

exit_free_ctx:
	osa_mem_free(identity_mgmt_ctx);

exit:
	return 0;
}

void hci_identity_mgmt_destroy(struct hci_identity_mgmt_context *identity_mgmt_ctx)
{
	osa_report(INFORMATION, ("hci_identity_mgmt_destroy"));

	if (identity_mgmt_ctx) {
		hci_core_unregister_gate(identity_mgmt_ctx->core_ctx, identity_mgmt_ctx->gate);

		hci_gate_free(identity_mgmt_ctx->gate);

		osa_mem_free(identity_mgmt_ctx);
	}
}

void hci_identity_mgmt_set_local_gates_list(struct hci_identity_mgmt_context *identity_mgmt_ctx, nfc_u8 *gates_list, nfc_u32 gates_list_len)
{
	osa_report(INFORMATION, ("hci_identity_mgmt_set_local_gates_list, gates_list_len %ld", gates_list_len));

	identity_mgmt_ctx->gates_list = gates_list;
	identity_mgmt_ctx->gates_list_len = gates_list_len;
}
