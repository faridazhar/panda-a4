/*
 * hci.c
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
#include "hci.h"
#include "hci_int.h"
#include "nfc_types.h"
#include "nci_utils.h"
#include "nfc_os_adapt.h"


/******************************************************************************/
/* HCI Structures and Types                                                   */
/******************************************************************************/

typedef enum {
	HCI_OP_ACTIVATE = 0,
	HCI_OP_DEACTIVATE,

	HCI_OP_MAX
} hci_op_code;

struct hci_op_activate {
	nfc_u8 nfcee_id;
};

struct hci_op_deactivate {
	nfc_u8 nfcee_id;
};

typedef union {
	struct hci_op_activate activate;	/* HCI_OP_ACTIVATE */
	struct hci_op_deactivate deactivate;	/* HCI_OP_DEACTIVATE */
} hci_op_param;

struct hci_op {
	hci_op_code code;
	hci_op_param param;
};

struct hci_context {
	struct hci_core_context *core_ctx;
	struct hci_admin_context *admin_ctx;
	struct hci_identity_mgmt_context *identity_mgmt_ctx;
	struct hci_loop_back_context *loop_back_ctx;
	struct hci_connectivity_context *connectivity_ctx;
	struct hci_gto_apdu_context *gto_apdu_ctx;
	struct hci_callbacks *callbacks;
	nfc_handle_t h_caller;
	nfc_handle_t h_osa;
	struct hci_op *curr_op;
};


/******************************************************************************/
/* HCI Function Definitions                                                   */
/******************************************************************************/

static void hci_op_complete(struct hci_context *hci_ctx)
{
	if (hci_ctx->curr_op) {
		osa_mem_free(hci_ctx->curr_op);
		hci_ctx->curr_op = 0;
	}

	/* allow next operation */
	osa_taskq_enable(hci_ctx->h_osa, E_HCI_Q_OPERATION);
}

static void hci_op_activate(struct hci_context *hci_ctx, nfc_u8 nfcee_id)
{
	hci_core_activate(hci_ctx->core_ctx, nfcee_id);
}

static void hci_op_deactivate(struct hci_context *hci_ctx, nfc_u8 nfcee_id)
{
	hci_core_deactivate(hci_ctx->core_ctx, nfcee_id);
}

static void hci_op_cb(nfc_handle_t h_cb, nfc_handle_t param)
{
	struct hci_context *hci_ctx = (struct hci_context*)h_cb;
	struct hci_op *op = (struct hci_op *)param;

	osa_report(INFORMATION, ("hci_op_cb, opcode %d", op->code));

	/* stop any further operations */
	osa_taskq_disable(hci_ctx->h_osa, E_HCI_Q_OPERATION);

	/* store current operation */
	hci_ctx->curr_op = op;

	switch (op->code) {
		case (HCI_OP_ACTIVATE):
			hci_op_activate(hci_ctx, op->param.activate.nfcee_id);
			return;
		case (HCI_OP_DEACTIVATE):
			hci_op_deactivate(hci_ctx, op->param.deactivate.nfcee_id);
			return;
		default:
			osa_report(ERROR, ("hci_op_cb, invalid opcode %d", op->code));
			break;
	}

	hci_op_complete(hci_ctx);
}

static nfc_status hci_op_schedule(struct hci_context *hci_ctx, hci_op_code code, hci_op_param *param)
{
	struct hci_op *op = (struct hci_op *)osa_mem_alloc(sizeof(struct hci_op));

	osa_report(INFORMATION, ("hci_op_schedule, opcode %d", code));

	if (!op) {
		osa_report(ERROR, ("hci_op_schedule, failed alloc new op"));
		return NFC_RES_MEM_ERROR;
	}

	op->code = code;
	op->param = *param;

	if (osa_taskq_schedule(hci_ctx->h_osa, E_HCI_Q_OPERATION, op) != NFC_RES_OK) {
		osa_mem_free(op);
		return NFC_RES_ERROR;
	}

	return NFC_RES_PENDING;
}

static void hci_activated_cb(nfc_handle_t h_caller, nfc_status status)
{
	struct hci_context *hci_ctx = (struct hci_context*)h_caller;

	osa_report(INFORMATION, ("hci_activated_cb, status %d", status));

	hci_op_complete(hci_ctx);

	if (hci_ctx->callbacks->activated)
		hci_ctx->callbacks->activated(hci_ctx->h_caller, status);
}

static void hci_deactivated_cb(nfc_handle_t h_caller, nfc_status status)
{
	struct hci_context *hci_ctx = (struct hci_context*)h_caller;

	osa_report(INFORMATION, ("hci_deactivated_cb, status %d", status));

	hci_op_complete(hci_ctx);

	if (hci_ctx->callbacks->deactivated)
		hci_ctx->callbacks->deactivated(hci_ctx->h_caller, status);
}

static void hci_pipe_opened_admin_cb(nfc_handle_t h_caller, struct hci_pipe *pipe, nfc_status status)
{
	struct hci_context *hci_ctx = (struct hci_context*)h_caller;

	osa_report(INFORMATION, ("hci_pipe_opened_admin_cb, gate_id 0x%x, pipe_id 0x%x, status %d",
		hci_gate_get_id(hci_pipe_get_local_gate(pipe)), hci_pipe_get_id(pipe), status));
}

static void hci_pipe_closed_admin_cb(nfc_handle_t h_caller, struct hci_pipe *pipe, nfc_status status)
{
	struct hci_context *hci_ctx = (struct hci_context*)h_caller;

	osa_report(INFORMATION, ("hci_pipe_closed_admin_cb, gate_id 0x%x, pipe_id 0x%x, status %d",
		hci_gate_get_id(hci_pipe_get_local_gate(pipe)), hci_pipe_get_id(pipe), status));
}

static void hci_pipe_created_cb(nfc_handle_t h_caller, struct hci_pipe *pipe, nfc_status status)
{
	struct hci_context *hci_ctx = (struct hci_context*)h_caller;

	if (status == NFC_RES_OK) {
		osa_report(INFORMATION, ("hci_pipe_created_cb, gate_id 0x%x, pipe_id 0x%x, status %d",
			hci_gate_get_id(hci_pipe_get_local_gate(pipe)), hci_pipe_get_id(pipe), status));
	} else {
		osa_report(INFORMATION, ("hci_pipe_created_cb, status %d", status));
	}
}

static void hci_pipe_deleted_cb(nfc_handle_t h_caller, struct hci_pipe *pipe, nfc_status status)
{
	struct hci_context *hci_ctx = (struct hci_context*)h_caller;

	if (status == NFC_RES_OK) {
		osa_report(INFORMATION, ("hci_pipe_deleted_cb, gate_id 0x%x, pipe_id 0x%x, status %d",
			hci_gate_get_id(hci_pipe_get_local_gate(pipe)), hci_pipe_get_id(pipe), status));
	} else {
		osa_report(INFORMATION, ("hci_pipe_deleted_cb, status %d", status));
	}
}

static void hci_hot_plug_cb(nfc_handle_t h_caller, struct hci_pipe *pipe)
{
	struct hci_context *hci_ctx = (struct hci_context*)h_caller;

	osa_report(INFORMATION, ("hci_hot_plug_cb, gate_id 0x%x, pipe_id 0x%x",
		hci_gate_get_id(hci_pipe_get_local_gate(pipe)), hci_pipe_get_id(pipe)));
}

static void hci_host_list_cb(nfc_handle_t h_caller, struct hci_pipe *pipe, nfc_status status, nfc_u8 *p_host_list, nfc_u32 host_list_len)
{
	struct hci_context *hci_ctx = (struct hci_context*)h_caller;

	osa_report(INFORMATION, ("hci_host_list_cb, gate_id 0x%x, pipe_id 0x%x, status %d, host_list_len %ld",
		hci_gate_get_id(hci_pipe_get_local_gate(pipe)), hci_pipe_get_id(pipe), status, host_list_len));
}

static void hci_pipe_opened_identity_mgmt_cb(nfc_handle_t h_caller, struct hci_pipe *pipe, nfc_status status)
{
	struct hci_context *hci_ctx = (struct hci_context*)h_caller;

	osa_report(INFORMATION, ("hci_pipe_opened_identity_mgmt_cb, gate_id 0x%x, pipe_id 0x%x, status %d",
		hci_gate_get_id(hci_pipe_get_local_gate(pipe)), hci_pipe_get_id(pipe), status));
}

static void hci_pipe_closed_identity_mgmt_cb(nfc_handle_t h_caller, struct hci_pipe *pipe, nfc_status status)
{
	struct hci_context *hci_ctx = (struct hci_context*)h_caller;

	osa_report(INFORMATION, ("hci_pipe_closed_identity_mgmt_cb, gate_id 0x%x, pipe_id 0x%x, status %d",
		hci_gate_get_id(hci_pipe_get_local_gate(pipe)), hci_pipe_get_id(pipe), status));
}

static void hci_pipe_opened_loop_back_cb(nfc_handle_t h_caller, struct hci_pipe *pipe, nfc_status status)
{
	struct hci_context *hci_ctx = (struct hci_context*)h_caller;

	osa_report(INFORMATION, ("hci_pipe_opened_loop_back_cb, gate_id 0x%x, pipe_id 0x%x, status %d",
		hci_gate_get_id(hci_pipe_get_local_gate(pipe)), hci_pipe_get_id(pipe), status));
}

static void hci_pipe_closed_loop_back_cb(nfc_handle_t h_caller, struct hci_pipe *pipe, nfc_status status)
{
	struct hci_context *hci_ctx = (struct hci_context*)h_caller;

	osa_report(INFORMATION, ("hci_pipe_closed_loop_back_cb, gate_id 0x%x, pipe_id 0x%x, status %d",
		hci_gate_get_id(hci_pipe_get_local_gate(pipe)), hci_pipe_get_id(pipe), status));
}

static void hci_pipe_opened_connectivity_cb(nfc_handle_t h_caller, struct hci_pipe *pipe, nfc_status status)
{
	struct hci_context *hci_ctx = (struct hci_context*)h_caller;

	osa_report(INFORMATION, ("hci_pipe_opened_connectivity_cb, gate_id 0x%x, pipe_id 0x%x, status %d",
		hci_gate_get_id(hci_pipe_get_local_gate(pipe)), hci_pipe_get_id(pipe), status));
}

static void hci_pipe_closed_connectivity_cb(nfc_handle_t h_caller, struct hci_pipe *pipe, nfc_status status)
{
	struct hci_context *hci_ctx = (struct hci_context*)h_caller;

	osa_report(INFORMATION, ("hci_pipe_closed_connectivity_cb, gate_id 0x%x, pipe_id 0x%x, status %d",
		hci_gate_get_id(hci_pipe_get_local_gate(pipe)), hci_pipe_get_id(pipe), status));
}

static void hci_evt_connectivity_cb(nfc_handle_t h_caller, struct hci_pipe *pipe)
{
	struct hci_context *hci_ctx = (struct hci_context*)h_caller;

	osa_report(INFORMATION, ("hci_evt_connectivity_cb, gate_id 0x%x, pipe_id 0x%x",
		hci_gate_get_id(hci_pipe_get_local_gate(pipe)), hci_pipe_get_id(pipe)));

	if (hci_ctx->callbacks->evt_connectivity)
		hci_ctx->callbacks->evt_connectivity(hci_ctx->h_caller);
}

static void hci_evt_transaction_cb(nfc_handle_t h_caller, struct hci_pipe *pipe, nfc_u8 *aid, nfc_u32 aid_len, nfc_u8 *params, nfc_u32 params_len)
{
	struct hci_context *hci_ctx = (struct hci_context*)h_caller;

	osa_report(INFORMATION, ("hci_evt_transaction_cb, gate_id 0x%x, pipe_id 0x%x, aid_len %ld, params_len %ld",
		hci_gate_get_id(hci_pipe_get_local_gate(pipe)), hci_pipe_get_id(pipe), aid_len, params_len));

	if (hci_ctx->callbacks->evt_transaction)
		hci_ctx->callbacks->evt_transaction(hci_ctx->h_caller, aid, aid_len, params, params_len);
}

static void hci_pipe_opened_gto_apdu_cb(nfc_handle_t h_caller, struct hci_pipe *pipe, nfc_status status)
{
	struct hci_context *hci_ctx = (struct hci_context*)h_caller;

	osa_report(INFORMATION, ("hci_pipe_opened_gto_apdu_cb, gate_id 0x%x, pipe_id 0x%x, status %d",
		hci_gate_get_id(hci_pipe_get_local_gate(pipe)), hci_pipe_get_id(pipe), status));
}

static void hci_pipe_closed_gto_apdu_cb(nfc_handle_t h_caller, struct hci_pipe *pipe, nfc_status status)
{
	struct hci_context *hci_ctx = (struct hci_context*)h_caller;

	osa_report(INFORMATION, ("hci_pipe_closed_gto_apdu_cb, gate_id 0x%x, pipe_id 0x%x, status %d",
		hci_gate_get_id(hci_pipe_get_local_gate(pipe)), hci_pipe_get_id(pipe), status));
}

static void hci_gto_apdu_evt_transmit_data_cb(nfc_handle_t h_caller, struct hci_pipe *pipe, nfc_u8 *data, nfc_u32 data_len)
{
	struct hci_context *hci_ctx = (struct hci_context*)h_caller;

	osa_report(INFORMATION, ("hci_gto_apdu_evt_transmit_data_cb, gate_id 0x%x, pipe_id 0x%x, data_len %ld",
		hci_gate_get_id(hci_pipe_get_local_gate(pipe)), hci_pipe_get_id(pipe), data_len));

	if (hci_ctx->callbacks->gto_apdu_evt_transmit_data)
		hci_ctx->callbacks->gto_apdu_evt_transmit_data(hci_ctx->h_caller, data, data_len);
}

static void hci_gto_apdu_evt_wtx_request_cb(nfc_handle_t h_caller, struct hci_pipe *pipe)
{
	struct hci_context *hci_ctx = (struct hci_context*)h_caller;

	osa_report(INFORMATION, ("hci_gto_apdu_evt_wtx_request_cb, gate_id 0x%x, pipe_id 0x%x",
		hci_gate_get_id(hci_pipe_get_local_gate(pipe)), hci_pipe_get_id(pipe)));

	if (hci_ctx->callbacks->gto_apdu_evt_wtx_request)
		hci_ctx->callbacks->gto_apdu_evt_wtx_request(hci_ctx->h_caller);
}

static struct hci_core_callbacks core_callbacks = {
	.activated = hci_activated_cb,
	.deactivated = hci_deactivated_cb,
};

static struct hci_admin_callbacks admin_callbacks = {
	.pipe_opened = hci_pipe_opened_admin_cb,
	.pipe_closed = hci_pipe_closed_admin_cb,
	.pipe_created = hci_pipe_created_cb,
	.pipe_deleted = hci_pipe_deleted_cb,
	.hot_plug = hci_hot_plug_cb,
	.host_list = hci_host_list_cb,
};

static struct hci_identity_mgmt_callbacks identity_mgmt_callbacks = {
	.pipe_opened = hci_pipe_opened_identity_mgmt_cb,
	.pipe_closed = hci_pipe_closed_identity_mgmt_cb,
};

static struct hci_loop_back_callbacks loop_back_callbacks = {
	.pipe_opened = hci_pipe_opened_loop_back_cb,
	.pipe_closed = hci_pipe_closed_loop_back_cb,
};

static struct hci_connectivity_callbacks connectivity_callbacks = {
	.pipe_opened = hci_pipe_opened_connectivity_cb,
	.pipe_closed = hci_pipe_closed_connectivity_cb,
	.evt_connectivity = hci_evt_connectivity_cb,
	.evt_transaction = hci_evt_transaction_cb,
};

static struct hci_gto_apdu_callbacks gto_apdu_callbacks = {
	.pipe_opened = hci_pipe_opened_gto_apdu_cb,
	.pipe_closed = hci_pipe_closed_gto_apdu_cb,
	.evt_transmit_data = hci_gto_apdu_evt_transmit_data_cb,
	.evt_wtx_request = hci_gto_apdu_evt_wtx_request_cb,
};

/* List of all gates that accept dynamic pipes as an array of gate identifiers */
static nfc_u8 local_gates_list[] = {
	HCI_GATE_ID_LOOP_BACK,
	HCI_GATE_ID_IDENTITY_MGMT,
	HCI_GATE_ID_CONNECTIVITY,
	HCI_GATE_ID_GTO_APDU,
};

nfc_handle_t hci_create(nfc_handle_t h_nci, nfc_handle_t h_osa, struct hci_callbacks *callbacks, nfc_handle_t h_caller)
{
	struct hci_context *hci_ctx;

	osa_report(INFORMATION, ("hci_create"));

	hci_ctx = (struct hci_context*)osa_mem_alloc(sizeof(struct hci_context));
	if (!hci_ctx)
		goto exit;

	osa_mem_zero(hci_ctx, sizeof(struct hci_context));

	hci_ctx->core_ctx = hci_core_create(h_nci, h_osa, &core_callbacks, hci_ctx);
	if (!hci_ctx->core_ctx)
		goto exit_free_ctx;

	hci_ctx->admin_ctx = hci_admin_create(hci_ctx->core_ctx, &admin_callbacks, hci_ctx);
	if (!hci_ctx->admin_ctx)
		goto exit_destroy_core;

	hci_ctx->identity_mgmt_ctx = hci_identity_mgmt_create(hci_ctx->core_ctx, &identity_mgmt_callbacks, hci_ctx);
	if (!hci_ctx->identity_mgmt_ctx)
		goto exit_destroy_admin;

	hci_identity_mgmt_set_local_gates_list(hci_ctx->identity_mgmt_ctx, local_gates_list, sizeof(local_gates_list));

	hci_ctx->loop_back_ctx = hci_loop_back_create(hci_ctx->core_ctx, &loop_back_callbacks, hci_ctx);
	if (!hci_ctx->loop_back_ctx)
		goto exit_destroy_identity_mgmt;

	hci_ctx->connectivity_ctx = hci_connectivity_create(hci_ctx->core_ctx, &connectivity_callbacks, hci_ctx);
	if (!hci_ctx->connectivity_ctx)
		goto exit_destroy_loop_back;

	hci_ctx->gto_apdu_ctx = hci_gto_apdu_create(hci_ctx->core_ctx, &gto_apdu_callbacks, hci_ctx);
	if (!hci_ctx->gto_apdu_ctx)
		goto exit_destroy_connectivity;

	hci_ctx->callbacks = callbacks;
	hci_ctx->h_caller = h_caller;
	hci_ctx->h_osa = h_osa;

	hci_core_persistence_restore(hci_ctx->core_ctx);

	osa_taskq_register(h_osa, E_HCI_Q_OPERATION, hci_op_cb, hci_ctx, 1);

	return (nfc_handle_t)hci_ctx;

exit_destroy_connectivity:
	hci_connectivity_destroy(hci_ctx->connectivity_ctx);

exit_destroy_loop_back:
	hci_loop_back_destroy(hci_ctx->loop_back_ctx);

exit_destroy_identity_mgmt:
	hci_identity_mgmt_destroy(hci_ctx->identity_mgmt_ctx);

exit_destroy_admin:
	hci_admin_destroy(hci_ctx->admin_ctx);

exit_destroy_core:
	hci_core_destroy(hci_ctx->core_ctx);

exit_free_ctx:
	osa_mem_free(hci_ctx);

exit:
	return 0;
}

void hci_destroy(nfc_handle_t h_hci)
{
	struct hci_context *hci_ctx = (struct hci_context*)h_hci;

	osa_report(INFORMATION, ("hci_destroy"));

	if (hci_ctx) {
		hci_op_complete(hci_ctx); /* cleanup pending op, if exists */

		osa_taskq_unregister(hci_ctx->h_osa, E_HCI_Q_OPERATION);

		hci_core_persistence_store(hci_ctx->core_ctx);

		hci_gto_apdu_destroy(hci_ctx->gto_apdu_ctx);

		hci_connectivity_destroy(hci_ctx->connectivity_ctx);

		hci_loop_back_destroy(hci_ctx->loop_back_ctx);

		hci_identity_mgmt_destroy(hci_ctx->identity_mgmt_ctx);

		hci_admin_destroy(hci_ctx->admin_ctx);

		hci_core_destroy(hci_ctx->core_ctx);

		osa_mem_free(hci_ctx);
	}
}

nfc_status hci_activate(nfc_handle_t h_hci, nfc_u8 nfcee_id)
{
	struct hci_context *hci_ctx = (struct hci_context*)h_hci;
	hci_op_param param;

	osa_report(INFORMATION, ("hci_activate, nfcee_id 0x%x", nfcee_id));

	param.activate.nfcee_id = nfcee_id;

	return hci_op_schedule(hci_ctx, HCI_OP_ACTIVATE, &param);
}

nfc_status hci_deactivate(nfc_handle_t h_hci, nfc_u8 nfcee_id)
{
	struct hci_context *hci_ctx = (struct hci_context*)h_hci;
	hci_op_param param;

	osa_report(INFORMATION, ("hci_deactivate, nfcee_id 0x%x", nfcee_id));

	param.deactivate.nfcee_id = nfcee_id;

	return hci_op_schedule(hci_ctx, HCI_OP_DEACTIVATE, &param);
}

nfc_status hci_gto_apdu_transmit_data(nfc_handle_t h_hci, nfc_u8 *data, nfc_u32 data_len)
{
	struct hci_context *hci_ctx = (struct hci_context*)h_hci;

	osa_report(INFORMATION, ("hci_gto_apdu_transmit_data, data_len %ld", data_len));

	return hci_gto_apdu_send_evt_transmit_data(hci_ctx->gto_apdu_ctx, data, data_len);
}
