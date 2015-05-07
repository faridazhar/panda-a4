/*
 * hci_loop_back.c
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
  This source file contains the implementation of HCI Network loop back gate.
*******************************************************************************/
#include "hci_int.h"
#include "nfc_types.h"
#include "nci_utils.h"
#include "nfc_os_adapt.h"


/******************************************************************************/
/* HCI Loop Back Structures and Types                                             */
/******************************************************************************/

struct hci_loop_back_context {
	struct hci_core_context *core_ctx;
	struct hci_gate *gate;	/* Local Loop Back gate */
	struct hci_loop_back_callbacks *callbacks;
	nfc_handle_t h_caller;
};


/******************************************************************************/
/* HCI Function Definitions                                                   */
/******************************************************************************/

static void hci_loop_back_handle_open_pipe(struct hci_loop_back_context *loop_back_ctx, struct hci_pipe *pipe, nfc_u8 *p_data, nfc_u32 data_len)
{
	if (hci_core_recv_open_pipe(loop_back_ctx->core_ctx, pipe, data_len) != NFC_RES_OK)
		return;

	if (loop_back_ctx->callbacks->pipe_opened)
		loop_back_ctx->callbacks->pipe_opened(loop_back_ctx->h_caller, pipe, NFC_RES_OK);
}

static void hci_loop_back_handle_close_pipe(struct hci_loop_back_context *loop_back_ctx, struct hci_pipe *pipe, nfc_u8 *p_data, nfc_u32 data_len)
{
	if (hci_core_recv_close_pipe(loop_back_ctx->core_ctx, pipe, data_len) != NFC_RES_OK)
		return;

	if (loop_back_ctx->callbacks->pipe_closed)
		loop_back_ctx->callbacks->pipe_closed(loop_back_ctx->h_caller, pipe, NFC_RES_OK);
}

static void hci_loop_back_cmd_recv_cb(nfc_handle_t h_caller, struct hci_pipe *pipe, nfc_u8 inst, nfc_u8 *p_data, nfc_u32 data_len)
{
	struct hci_loop_back_context *loop_back_ctx = (struct hci_loop_back_context*)h_caller;

	switch (inst) {
		case (HCI_CMD_ANY_OPEN_PIPE):
			hci_loop_back_handle_open_pipe(loop_back_ctx, pipe, p_data, data_len);
			break;
		case (HCI_CMD_ANY_CLOSE_PIPE):
			hci_loop_back_handle_close_pipe(loop_back_ctx, pipe, p_data, data_len);
			break;
		default:
			hci_core_send_rsp_no_param(loop_back_ctx->core_ctx, pipe, HCI_RSP_ANY_E_CMD_NOT_SUPPORTED);
			break;
	}
}

static void hci_loop_back_handle_post_data(struct hci_loop_back_context *loop_back_ctx, struct hci_pipe *pipe, nfc_u8 *p_data, nfc_u32 data_len)
{
	struct hcp_msg* msg;
	nfc_u8* p_data_copy;

	msg = hcp_msg_alloc(HCI_TYPE_EVT, HCI_EVT_POST_DATA, data_len);
	if (!msg)
		return;

	if (data_len > 0) {
		p_data_copy = hcp_msg_get_data(msg);
		osa_mem_copy(p_data_copy, p_data, data_len);
	}

	hci_core_send_hcp_msg(loop_back_ctx->core_ctx, pipe, msg);
}

static void hci_loop_back_evt_recv_cb(nfc_handle_t h_caller, struct hci_pipe *pipe, nfc_u8 inst, nfc_u8 *p_data, nfc_u32 data_len)
{
	struct hci_loop_back_context *loop_back_ctx = (struct hci_loop_back_context*)h_caller;

	if (hci_pipe_get_state(pipe) != HCI_PIPE_STATE_OPEN) {
		osa_report(INFORMATION, ("hci_loop_back_handle_evt: pipe state is not OPEN"));
		return;
	}

	switch (inst) {
		case (HCI_EVT_POST_DATA):
			hci_loop_back_handle_post_data(loop_back_ctx, pipe, p_data, data_len);
			break;
		default:
			osa_report(INFORMATION, ("hci_loop_back_handle_evt: unsupported inst 0x%x", inst));
			break;
	}
}

static struct hci_gate_callbacks hci_loop_back_gate_callbacks = {
	.cmd_recv = hci_loop_back_cmd_recv_cb,
	.evt_recv = hci_loop_back_evt_recv_cb,
	.rsp_recv = 0,
	.cmd_timeout = 0,
};

struct hci_loop_back_context* hci_loop_back_create(struct hci_core_context *core_ctx, struct hci_loop_back_callbacks *callbacks, nfc_handle_t h_caller)
{
	struct hci_loop_back_context *loop_back_ctx;

	osa_report(INFORMATION, ("hci_loop_back_create"));

	loop_back_ctx = (struct hci_loop_back_context*)osa_mem_alloc(sizeof(struct hci_loop_back_context));
	if (!loop_back_ctx)
		goto exit;

	osa_mem_zero(loop_back_ctx, sizeof(struct hci_loop_back_context));

	loop_back_ctx->core_ctx = core_ctx;

	loop_back_ctx->gate = hci_gate_alloc(HCI_GATE_ID_LOOP_BACK, &hci_loop_back_gate_callbacks, loop_back_ctx);
	if (!loop_back_ctx->gate)
		goto exit_free_ctx;

	hci_core_register_gate(core_ctx, loop_back_ctx->gate);

	loop_back_ctx->callbacks = callbacks;
	loop_back_ctx->h_caller = h_caller;

	return loop_back_ctx;

exit_free_ctx:
	osa_mem_free(loop_back_ctx);

exit:
	return 0;
}

void hci_loop_back_destroy(struct hci_loop_back_context *loop_back_ctx)
{
	osa_report(INFORMATION, ("hci_loop_back_destroy"));

	if (loop_back_ctx) {
		hci_core_unregister_gate(loop_back_ctx->core_ctx, loop_back_ctx->gate);

		hci_gate_free(loop_back_ctx->gate);

		osa_mem_free(loop_back_ctx);
	}
}
