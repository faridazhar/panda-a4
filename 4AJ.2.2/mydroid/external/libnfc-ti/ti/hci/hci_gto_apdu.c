/*
 * hci_gto_apdu.c
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
  This source file contains the implementation of HCI Network GTO APDU gate.
*******************************************************************************/
#include "hci_int.h"
#include "nfc_types.h"
#include "nci_utils.h"
#include "nfc_os_adapt.h"


/******************************************************************************/
/* HCI GTO APDU Structures and Types                                      */
/******************************************************************************/

struct hci_gto_apdu_context {
	struct hci_core_context *core_ctx;
	struct hci_gate *gate;	/* Local APDU reader gate */
	struct hci_gto_apdu_callbacks *callbacks;
	nfc_handle_t h_caller;
};


/******************************************************************************/
/* HCI Function Definitions                                                   */
/******************************************************************************/

static void hci_gto_apdu_handle_open_pipe(struct hci_gto_apdu_context *gto_apdu_ctx, struct hci_pipe *pipe, nfc_u8 *p_data, nfc_u32 data_len)
{
	if (hci_core_recv_open_pipe(gto_apdu_ctx->core_ctx, pipe, data_len) != NFC_RES_OK)
		return;

	if (gto_apdu_ctx->callbacks->pipe_opened)
		gto_apdu_ctx->callbacks->pipe_opened(gto_apdu_ctx->h_caller, pipe, NFC_RES_OK);
}

static void hci_gto_apdu_handle_close_pipe(struct hci_gto_apdu_context *gto_apdu_ctx, struct hci_pipe *pipe, nfc_u8 *p_data, nfc_u32 data_len)
{
	if (hci_core_recv_close_pipe(gto_apdu_ctx->core_ctx, pipe, data_len) != NFC_RES_OK)
		return;

	if (gto_apdu_ctx->callbacks->pipe_closed)
		gto_apdu_ctx->callbacks->pipe_closed(gto_apdu_ctx->h_caller, pipe, NFC_RES_OK);
}

static void hci_gto_apdu_cmd_recv_cb(nfc_handle_t h_caller, struct hci_pipe *pipe, nfc_u8 inst, nfc_u8 *p_data, nfc_u32 data_len)
{
	struct hci_gto_apdu_context *gto_apdu_ctx = (struct hci_gto_apdu_context*)h_caller;

	switch (inst) {
		case (HCI_CMD_ANY_OPEN_PIPE):
			hci_gto_apdu_handle_open_pipe(gto_apdu_ctx, pipe, p_data, data_len);
			break;
		case (HCI_CMD_ANY_CLOSE_PIPE):
			hci_gto_apdu_handle_close_pipe(gto_apdu_ctx, pipe, p_data, data_len);
			break;
		default:
			hci_core_send_rsp_no_param(gto_apdu_ctx->core_ctx, pipe, HCI_RSP_ANY_E_CMD_NOT_SUPPORTED);
			break;
	}
}

static void hci_gto_apdu_handle_transmit_data(struct hci_gto_apdu_context *gto_apdu_ctx, struct hci_pipe *pipe, nfc_u8 *p_data, nfc_u32 data_len)
{
	if (gto_apdu_ctx->callbacks->evt_transmit_data)
		gto_apdu_ctx->callbacks->evt_transmit_data(gto_apdu_ctx->h_caller, pipe, p_data, data_len);
}

static void hci_gto_apdu_handle_wtx_request(struct hci_gto_apdu_context *gto_apdu_ctx, struct hci_pipe *pipe, nfc_u8 *p_data, nfc_u32 data_len)
{
	if (gto_apdu_ctx->callbacks->evt_wtx_request)
		gto_apdu_ctx->callbacks->evt_wtx_request(gto_apdu_ctx->h_caller, pipe);
}

static void hci_gto_apdu_evt_recv_cb(nfc_handle_t h_caller, struct hci_pipe *pipe, nfc_u8 inst, nfc_u8 *p_data, nfc_u32 data_len)
{
	struct hci_gto_apdu_context *gto_apdu_ctx = (struct hci_gto_apdu_context*)h_caller;

	if (hci_pipe_get_state(pipe) != HCI_PIPE_STATE_OPEN) {
		osa_report(INFORMATION, ("hci_gto_apdu_evt_recv_cb: pipe state is not OPEN"));
		return;
	}

	switch (inst) {
		case (HCI_EVT_GTO_TRANSMIT_DATA):
			hci_gto_apdu_handle_transmit_data(gto_apdu_ctx, pipe, p_data, data_len);
			break;
		case (HCI_EVT_GTO_WTX_REQUEST):
			hci_gto_apdu_handle_wtx_request(gto_apdu_ctx, pipe, p_data, data_len);
			break;
		default:
			osa_report(INFORMATION, ("hci_gto_apdu_evt_recv_cb: unsupported inst 0x%x", inst));
			break;
	}
}

static struct hci_gate_callbacks hci_gto_apdu_gate_callbacks = {
	.cmd_recv = hci_gto_apdu_cmd_recv_cb,
	.evt_recv = hci_gto_apdu_evt_recv_cb,
	.rsp_recv = 0,
	.cmd_timeout = 0,
};

struct hci_gto_apdu_context* hci_gto_apdu_create(struct hci_core_context *core_ctx, struct hci_gto_apdu_callbacks *callbacks, nfc_handle_t h_caller)
{
	struct hci_gto_apdu_context *gto_apdu_ctx;

	osa_report(INFORMATION, ("hci_gto_apdu_create"));

	gto_apdu_ctx = (struct hci_gto_apdu_context*)osa_mem_alloc(sizeof(struct hci_gto_apdu_context));
	if (!gto_apdu_ctx)
		goto exit;

	osa_mem_zero(gto_apdu_ctx, sizeof(struct hci_gto_apdu_context));

	gto_apdu_ctx->core_ctx = core_ctx;

	gto_apdu_ctx->gate = hci_gate_alloc(HCI_GATE_ID_GTO_APDU, &hci_gto_apdu_gate_callbacks, gto_apdu_ctx);
	if (!gto_apdu_ctx->gate)
		goto exit_free_ctx;

	hci_core_register_gate(core_ctx, gto_apdu_ctx->gate);

	gto_apdu_ctx->callbacks = callbacks;
	gto_apdu_ctx->h_caller = h_caller;

	return gto_apdu_ctx;

exit_free_ctx:
	osa_mem_free(gto_apdu_ctx);

exit:
	return 0;
}

void hci_gto_apdu_destroy(struct hci_gto_apdu_context *gto_apdu_ctx)
{
	osa_report(INFORMATION, ("hci_gto_apdu_destroy"));

	if (gto_apdu_ctx) {
		hci_core_unregister_gate(gto_apdu_ctx->core_ctx, gto_apdu_ctx->gate);

		hci_gate_free(gto_apdu_ctx->gate);

		osa_mem_free(gto_apdu_ctx);
	}
}

nfc_status hci_gto_apdu_send_evt_transmit_data(struct hci_gto_apdu_context *gto_apdu_ctx, nfc_u8 *data, nfc_u32 data_len)
{
	struct hci_pipe **pipes;
	nfc_u8 num_pipes, i;
	struct hcp_msg* msg;
	nfc_u8* p_data;
	nfc_status rc = NFC_RES_OK;

	/* with HCI workaround, the remote host is always UICC */
//	pipes = hci_core_list_all_pipes(gto_apdu_ctx->core_ctx, HCI_HOST_ID_GTO_SE, &num_pipes);
	pipes = hci_core_list_all_pipes(gto_apdu_ctx->core_ctx, HCI_HOST_ID_UICC_HOST, &num_pipes);

	osa_report(INFORMATION, ("hci_gto_apdu_send_evt_transmit_data, data_len %ld, num_pipes %d", data_len, num_pipes));

	/* send msg only to the first open pipe associated with APDU gate*/
	for (i=0; i<num_pipes; i++) {
		if ((hci_pipe_get_local_gate(pipes[i]) == gto_apdu_ctx->gate) &&
			(hci_pipe_get_state(pipes[i]) == HCI_PIPE_STATE_OPEN)) {
			msg = hcp_msg_alloc(HCI_TYPE_EVT, HCI_EVT_GTO_TRANSMIT_DATA, data_len);
			if (!msg) {
				rc = NFC_RES_MEM_ERROR;
				goto exit;
			}

			p_data = hcp_msg_get_data(msg);
			osa_mem_copy(p_data, data, data_len);

			hci_core_send_hcp_msg(gto_apdu_ctx->core_ctx, pipes[i], msg);
			goto exit;
		}
	}

	rc = NFC_RES_ERROR;

exit:
	if (pipes)
		osa_mem_free(pipes);

	osa_report(INFORMATION, ("hci_gto_apdu_send_evt_transmit_data, rc %d", rc));
	return rc;
}
