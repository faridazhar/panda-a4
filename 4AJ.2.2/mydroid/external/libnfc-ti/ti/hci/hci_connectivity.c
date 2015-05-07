/*
 * hci_connectivity.c
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
  This source file contains the implementation of HCI Network connectivity gate.
*******************************************************************************/
#include "hci_int.h"
#include "nfc_types.h"
#include "nci_utils.h"
#include "nfc_os_adapt.h"


/******************************************************************************/
/* HCI Connectivity Structures and Types                                      */
/******************************************************************************/

struct hci_connectivity_context {
	struct hci_core_context *core_ctx;
	struct hci_gate *gate;	/* Local Connectivity gate */
	struct hci_connectivity_callbacks *callbacks;
	nfc_handle_t h_caller;
};


/******************************************************************************/
/* HCI Function Definitions                                                   */
/******************************************************************************/

static void hci_connectivity_handle_open_pipe(struct hci_connectivity_context *connectivity_ctx, struct hci_pipe *pipe, nfc_u8 *p_data, nfc_u32 data_len)
{
	if (hci_core_recv_open_pipe(connectivity_ctx->core_ctx, pipe, data_len) != NFC_RES_OK)
		return;

	if (connectivity_ctx->callbacks->pipe_opened)
		connectivity_ctx->callbacks->pipe_opened(connectivity_ctx->h_caller, pipe, NFC_RES_OK);
}

static void hci_connectivity_handle_close_pipe(struct hci_connectivity_context *connectivity_ctx, struct hci_pipe *pipe, nfc_u8 *p_data, nfc_u32 data_len)
{
	if (hci_core_recv_close_pipe(connectivity_ctx->core_ctx, pipe, data_len) != NFC_RES_OK)
		return;

	if (connectivity_ctx->callbacks->pipe_closed)
		connectivity_ctx->callbacks->pipe_closed(connectivity_ctx->h_caller, pipe, NFC_RES_OK);
}

static void hci_connectivity_cmd_recv_cb(nfc_handle_t h_caller, struct hci_pipe *pipe, nfc_u8 inst, nfc_u8 *p_data, nfc_u32 data_len)
{
	struct hci_connectivity_context *connectivity_ctx = (struct hci_connectivity_context*)h_caller;

	switch (inst) {
		case (HCI_CMD_ANY_OPEN_PIPE):
			hci_connectivity_handle_open_pipe(connectivity_ctx, pipe, p_data, data_len);
			break;
		case (HCI_CMD_ANY_CLOSE_PIPE):
			hci_connectivity_handle_close_pipe(connectivity_ctx, pipe, p_data, data_len);
			break;
		default:
			hci_core_send_rsp_no_param(connectivity_ctx->core_ctx, pipe, HCI_RSP_ANY_E_CMD_NOT_SUPPORTED);
			break;
	}
}

static void hci_connectivity_handle_connectivity(struct hci_connectivity_context *connectivity_ctx, struct hci_pipe *pipe, nfc_u8 *p_data, nfc_u32 data_len)
{
	if (data_len != 0) {
		osa_report(INFORMATION, ("hci_connectivity_handle_connectivity: data_len is not zero"));
		return;
	}

	if (connectivity_ctx->callbacks->evt_connectivity)
		connectivity_ctx->callbacks->evt_connectivity(connectivity_ctx->h_caller, pipe);
}

static void hci_connectivity_handle_transaction(struct hci_connectivity_context *connectivity_ctx, struct hci_pipe *pipe, nfc_u8 *p_data, nfc_u32 data_len)
{
	nfc_u8 *p_aid = 0;
	nfc_u8 *p_params = 0;
	nfc_u32 aid_len = 0, params_len = 0;
	nfc_u32 i = 0;
	nfc_u8 tag;

	if (data_len == 0) {
		osa_report(INFORMATION, ("hci_connectivity_handle_transaction: data_len is zero"));
		return;
	}

	while (i < data_len) {
		tag = p_data[i++];

		switch (tag) {
			case (HCI_EVT_TRANSACTION_AID_TAG):
				aid_len = p_data[i++];

				if ((aid_len < HCI_EVT_TRANSACTION_AID_MIN_LEN) ||
					(aid_len > HCI_EVT_TRANSACTION_AID_MAX_LEN)) {
					osa_report(INFORMATION, ("hci_connectivity_handle_transaction: wrong aid_len %ld", aid_len));
					return;
				}

				osa_report(INFORMATION, ("hci_connectivity_handle_transaction: AID_TAG, aid_len %ld", aid_len));

				p_aid = (p_data + i);
				i += aid_len;
				break;
			case (HCI_EVT_TRANSACTION_PARAMS_TAG):
				params_len = p_data[i++];

				if (params_len > 0x7F) {
					/* 2 or 3 or 4 byte encoding of length */
					if (params_len == 0x81) {
						/* 2 byte encoding of length */
						params_len = p_data[i++];
					} else {
						osa_report(INFORMATION, ("hci_connectivity_handle_transaction: invalid params_len %ld", params_len));
						return;
					}
				}

				if (params_len > HCI_EVT_TRANSACTION_PARAMS_MAX_LEN) {
					osa_report(INFORMATION, ("hci_connectivity_handle_transaction: wrong params_len %ld", params_len));
					return;
				}

				osa_report(INFORMATION, ("hci_connectivity_handle_transaction: PARAMS_TAG, params_len %ld", params_len));

				p_params = (p_data + i);
				i += params_len;
				break;
			default:
				osa_report(INFORMATION, ("hci_connectivity_handle_transaction: invalid tag 0x%x", tag));
				return;
		}
	}

	if (!p_aid || !p_params) {
		osa_report(INFORMATION, ("hci_connectivity_handle_transaction: missing aid or params tag"));
		return;
	}

	if (connectivity_ctx->callbacks->evt_transaction)
		connectivity_ctx->callbacks->evt_transaction(connectivity_ctx->h_caller, pipe, p_aid, aid_len, p_params, params_len);
}

static void hci_connectivity_evt_recv_cb(nfc_handle_t h_caller, struct hci_pipe *pipe, nfc_u8 inst, nfc_u8 *p_data, nfc_u32 data_len)
{
	struct hci_connectivity_context *connectivity_ctx = (struct hci_connectivity_context*)h_caller;

	if (hci_pipe_get_state(pipe) != HCI_PIPE_STATE_OPEN) {
		osa_report(INFORMATION, ("hci_connectivity_handle_evt: pipe state is not OPEN"));
		return;
	}

	switch (inst) {
		case (HCI_EVT_CONNECTIVITY):
			hci_connectivity_handle_connectivity(connectivity_ctx, pipe, p_data, data_len);
			break;
		case (HCI_EVT_TRANSACTION):
			hci_connectivity_handle_transaction(connectivity_ctx, pipe, p_data, data_len);
			break;
		default:
			osa_report(INFORMATION, ("hci_connectivity_handle_evt: unsupported inst 0x%x", inst));
			break;
	}
}

static struct hci_gate_callbacks hci_connectivity_gate_callbacks = {
	.cmd_recv = hci_connectivity_cmd_recv_cb,
	.evt_recv = hci_connectivity_evt_recv_cb,
	.rsp_recv = 0,
	.cmd_timeout = 0,
};

struct hci_connectivity_context* hci_connectivity_create(struct hci_core_context *core_ctx, struct hci_connectivity_callbacks *callbacks, nfc_handle_t h_caller)
{
	struct hci_connectivity_context *connectivity_ctx;

	osa_report(INFORMATION, ("hci_connectivity_create"));

	connectivity_ctx = (struct hci_connectivity_context*)osa_mem_alloc(sizeof(struct hci_connectivity_context));
	if (!connectivity_ctx)
		goto exit;

	osa_mem_zero(connectivity_ctx, sizeof(struct hci_connectivity_context));

	connectivity_ctx->core_ctx = core_ctx;

	connectivity_ctx->gate = hci_gate_alloc(HCI_GATE_ID_CONNECTIVITY, &hci_connectivity_gate_callbacks, connectivity_ctx);
	if (!connectivity_ctx->gate)
		goto exit_free_ctx;

	hci_core_register_gate(core_ctx, connectivity_ctx->gate);

	connectivity_ctx->callbacks = callbacks;
	connectivity_ctx->h_caller = h_caller;

	return connectivity_ctx;

exit_free_ctx:
	osa_mem_free(connectivity_ctx);

exit:
	return 0;
}

void hci_connectivity_destroy(struct hci_connectivity_context *connectivity_ctx)
{
	osa_report(INFORMATION, ("hci_connectivity_destroy"));

	if (connectivity_ctx) {
		hci_core_unregister_gate(connectivity_ctx->core_ctx, connectivity_ctx->gate);

		hci_gate_free(connectivity_ctx->gate);

		osa_mem_free(connectivity_ctx);
	}
}
