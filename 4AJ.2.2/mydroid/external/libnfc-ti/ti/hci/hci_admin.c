/*
 * hci_admin.c
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
  This source file contains the implementation of HCI Network admin gate.
*******************************************************************************/
#include "hci_int.h"
#include "nfc_types.h"
#include "nci_utils.h"
#include "nfc_os_adapt.h"


/******************************************************************************/
/* HCI Admin Structures and Types                                             */
/******************************************************************************/

struct hci_admin_context {
	struct hci_core_context *core_ctx;
	struct hci_gate *gate;	/* Local Host administration gate */
	struct hci_pipe *pipe;	/* Static Host administration pipe */
	struct hci_admin_callbacks *callbacks;
	nfc_handle_t h_caller;
	nfc_u8 delete_pipe_id;
};


/******************************************************************************/
/* HCI Function Definitions                                                   */
/******************************************************************************/

static void hci_admin_handle_open_pipe(struct hci_admin_context *admin_ctx, struct hci_pipe *pipe, nfc_u8 *p_data, nfc_u32 data_len)
{
	if (hci_core_recv_open_pipe(admin_ctx->core_ctx, pipe, data_len) != NFC_RES_OK)
		return;

	if (admin_ctx->callbacks->pipe_opened)
		admin_ctx->callbacks->pipe_opened(admin_ctx->h_caller, pipe, NFC_RES_OK);
}

static void hci_admin_handle_close_pipe(struct hci_admin_context *admin_ctx, struct hci_pipe *pipe, nfc_u8 *p_data, nfc_u32 data_len)
{
	if (hci_core_recv_close_pipe(admin_ctx->core_ctx, pipe, data_len) != NFC_RES_OK)
		return;

	if (admin_ctx->callbacks->pipe_closed)
		admin_ctx->callbacks->pipe_closed(admin_ctx->h_caller, pipe, NFC_RES_OK);
}

static void hci_admin_handle_notify_pipe_created(struct hci_admin_context *admin_ctx, struct hci_pipe *pipe, nfc_u8 *p_data, nfc_u32 data_len)
{
	struct hci_cmd_adm_notify_pipe_created *cmd = (struct hci_cmd_adm_notify_pipe_created*)p_data;
	struct hci_gate *gate;
	struct hci_pipe* new_pipe;

	if (data_len != sizeof(struct hci_cmd_adm_notify_pipe_created)) {
		hci_core_send_rsp_no_param(admin_ctx->core_ctx, pipe, HCI_RSP_ANY_E_CMD_PAR_UNKNOWN);
		return;
	}

	if (hci_pipe_get_state(pipe) != HCI_PIPE_STATE_OPEN) {
		hci_core_send_rsp_no_param(admin_ctx->core_ctx, pipe, HCI_RSP_ANY_E_PIPE_NOT_OPENED);
		return;
	}

	if (cmd->dst_host_id != HCI_HOST_ID_TERMINAL_HOST) {
		hci_core_send_rsp_no_param(admin_ctx->core_ctx, pipe, HCI_RSP_ANY_E_INHIBITED);
		return;
	}

	gate = hci_core_find_gate(admin_ctx->core_ctx, cmd->dst_gate_id);
	if (!gate) {
		hci_core_send_rsp_no_param(admin_ctx->core_ctx, pipe, HCI_RSP_ANY_E_NOK);
		return;
	}

	new_pipe = hci_pipe_alloc_and_attach(gate, cmd->pipe_id, cmd->src_host_id, HCI_PIPE_STATE_CLOSED);
	if (!new_pipe) {
		hci_core_send_rsp_no_param(admin_ctx->core_ctx, pipe, HCI_RSP_ADM_E_NO_PIPES_AVAILABLE);
		return;
	}

	hci_core_persistence_store(admin_ctx->core_ctx);

	hci_core_send_rsp_no_param(admin_ctx->core_ctx, pipe, HCI_RSP_ANY_OK);

	if (admin_ctx->callbacks->pipe_created)
		admin_ctx->callbacks->pipe_created(admin_ctx->h_caller, new_pipe, NFC_RES_OK);
}

static void hci_admin_handle_notify_pipe_deleted(struct hci_admin_context *admin_ctx, struct hci_pipe *pipe, nfc_u8 *p_data, nfc_u32 data_len)
{
	struct hci_cmd_adm_notify_pipe_deleted *cmd = (struct hci_cmd_adm_notify_pipe_deleted*)p_data;
	struct hci_pipe* pipe_to_delete;

	if (data_len != sizeof(struct hci_cmd_adm_notify_pipe_deleted)) {
		hci_core_send_rsp_no_param(admin_ctx->core_ctx, pipe, HCI_RSP_ANY_E_CMD_PAR_UNKNOWN);
		return;
	}

	if (hci_pipe_get_state(pipe) != HCI_PIPE_STATE_OPEN) {
		hci_core_send_rsp_no_param(admin_ctx->core_ctx, pipe, HCI_RSP_ANY_E_PIPE_NOT_OPENED);
		return;
	}

	pipe_to_delete = hci_core_find_pipe(admin_ctx->core_ctx, cmd->pipe_id);
	if (!pipe_to_delete) {
		hci_core_send_rsp_no_param(admin_ctx->core_ctx, pipe, HCI_RSP_ANY_E_NOK);
		return;
	}

	if (admin_ctx->callbacks->pipe_deleted)
		admin_ctx->callbacks->pipe_deleted(admin_ctx->h_caller, pipe_to_delete, NFC_RES_OK);

	hci_pipe_detach_and_free(pipe_to_delete);

	hci_core_persistence_store(admin_ctx->core_ctx);

	hci_core_send_rsp_no_param(admin_ctx->core_ctx, pipe, HCI_RSP_ANY_OK);
}

static void hci_admin_handle_notify_all_pipe_cleared(struct hci_admin_context *admin_ctx, struct hci_pipe *pipe, nfc_u8 *p_data, nfc_u32 data_len)
{
	struct hci_cmd_adm_notify_all_pipe_cleared *cmd = (struct hci_cmd_adm_notify_all_pipe_cleared*)p_data;
	struct hci_pipe **pipes;
	nfc_u8 num_pipes, i, pipe_id;

	if (data_len != sizeof(struct hci_cmd_adm_notify_all_pipe_cleared)) {
		hci_core_send_rsp_no_param(admin_ctx->core_ctx, pipe, HCI_RSP_ANY_E_CMD_PAR_UNKNOWN);
		return;
	}

	if (hci_pipe_get_state(pipe) != HCI_PIPE_STATE_OPEN) {
		hci_core_send_rsp_no_param(admin_ctx->core_ctx, pipe, HCI_RSP_ANY_E_PIPE_NOT_OPENED);
		return;
	}

	pipes = hci_core_list_all_pipes(admin_ctx->core_ctx, cmd->requesting_host_id, &num_pipes);

	osa_report(INFORMATION, ("hci_admin_handle_notify_all_pipe_cleared: deleting %d pipes", num_pipes));

	for (i=0; i<num_pipes; i++) {
		pipe_id =  hci_pipe_get_id(pipes[i]);

		if (pipe_id != HCI_PIPE_ID_ADMIN) {
			/* dynamic pipe => delete */
			if (admin_ctx->callbacks->pipe_deleted)
				admin_ctx->callbacks->pipe_deleted(admin_ctx->h_caller, pipes[i], NFC_RES_OK);

			hci_pipe_detach_and_free(pipes[i]);
		} else {
			/* static pipe => close */
			hci_pipe_set_state(pipes[i], HCI_PIPE_STATE_CLOSED);

			if (admin_ctx->callbacks->pipe_closed)
				admin_ctx->callbacks->pipe_closed(admin_ctx->h_caller, pipes[i], NFC_RES_OK);
		}
	}

	hci_core_persistence_store(admin_ctx->core_ctx);

	if (pipes)
		osa_mem_free(pipes);

	hci_core_send_rsp_no_param(admin_ctx->core_ctx, pipe, HCI_RSP_ANY_OK);
}

static nfc_u8 hci_admin_get_unused_pipe_id(struct hci_admin_context *admin_ctx)
{
	nfc_u8 pipe_id;

	for (pipe_id = HCI_PIPE_ID_DYNAMIC_MIN; pipe_id <= HCI_PIPE_ID_DYNAMIC_MAX; pipe_id++) {
		if (hci_core_find_pipe(admin_ctx->core_ctx, pipe_id) == 0)
			return pipe_id;
	}

	return HCI_PIPE_ID_INVALID;
}

static void hci_admin_handle_create_pipe(struct hci_admin_context *admin_ctx, struct hci_pipe *pipe, nfc_u8 *p_data, nfc_u32 data_len)
{
	struct hci_cmd_adm_create_pipe *cmd = (struct hci_cmd_adm_create_pipe*)p_data;
	struct hci_rsp_adm_create_pipe *rsp;
	struct hcp_msg* msg;
	struct hci_gate *gate;
	struct hci_pipe* new_pipe;
	nfc_u8 pipe_id;

	if (data_len != sizeof(struct hci_cmd_adm_create_pipe)) {
		hci_core_send_rsp_no_param(admin_ctx->core_ctx, pipe, HCI_RSP_ANY_E_CMD_PAR_UNKNOWN);
		return;
	}

	if (cmd->dst_host_id != HCI_HOST_ID_TERMINAL_HOST) {
		hci_core_send_rsp_no_param(admin_ctx->core_ctx, pipe, HCI_RSP_ANY_E_INHIBITED);
		return;
	}

	gate = hci_core_find_gate(admin_ctx->core_ctx, cmd->dst_gate_id);
	if (!gate) {
		hci_core_send_rsp_no_param(admin_ctx->core_ctx, pipe, HCI_RSP_ANY_E_NOK);
		return;
	}

	pipe_id = hci_admin_get_unused_pipe_id(admin_ctx);
	if (pipe_id == HCI_PIPE_ID_INVALID) {
		hci_core_send_rsp_no_param(admin_ctx->core_ctx, pipe, HCI_RSP_ADM_E_NO_PIPES_AVAILABLE);
		return;
	}

	/* assuming remote host is UICC */
	new_pipe = hci_pipe_alloc_and_attach(gate, pipe_id, HCI_HOST_ID_UICC_HOST, HCI_PIPE_STATE_CLOSED);
	if (!new_pipe) {
		hci_core_send_rsp_no_param(admin_ctx->core_ctx, pipe, HCI_RSP_ADM_E_NO_PIPES_AVAILABLE);
		return;
	}

	msg = hcp_msg_alloc(HCI_TYPE_RSP, HCI_RSP_ANY_OK, sizeof(struct hci_rsp_adm_create_pipe));
	if (!msg) {
		/* unable to reply => free the new pipe */
		hci_pipe_detach_and_free(new_pipe);
		return;
	}

	hci_core_persistence_store(admin_ctx->core_ctx);

	rsp = (struct hci_rsp_adm_create_pipe*)hcp_msg_get_data(msg);

	rsp->src_host_id = HCI_HOST_ID_UICC_HOST;
	rsp->src_gate_id = cmd->src_gate_id;
	rsp->dst_host_id = HCI_HOST_ID_TERMINAL_HOST;
	rsp->dst_gate_id = cmd->dst_gate_id;
	rsp->pipe_id = hci_pipe_get_id(new_pipe);

	hci_core_send_hcp_msg(admin_ctx->core_ctx, pipe, msg);

	if (admin_ctx->callbacks->pipe_created)
		admin_ctx->callbacks->pipe_created(admin_ctx->h_caller, new_pipe, NFC_RES_OK);
}

static void hci_admin_handle_delete_pipe(struct hci_admin_context *admin_ctx, struct hci_pipe *pipe, nfc_u8 *p_data, nfc_u32 data_len)
{
	struct hci_cmd_adm_delete_pipe *cmd = (struct hci_cmd_adm_delete_pipe*)p_data;
	struct hci_pipe* pipe_to_delete;

	if (data_len != sizeof(struct hci_cmd_adm_delete_pipe)) {
		hci_core_send_rsp_no_param(admin_ctx->core_ctx, pipe, HCI_RSP_ANY_E_CMD_PAR_UNKNOWN);
		return;
	}

	pipe_to_delete = hci_core_find_pipe(admin_ctx->core_ctx, cmd->pipe_id);
	if (!pipe_to_delete) {
		hci_core_send_rsp_no_param(admin_ctx->core_ctx, pipe, HCI_RSP_ANY_E_NOK);
		return;
	}

	if (admin_ctx->callbacks->pipe_deleted)
		admin_ctx->callbacks->pipe_deleted(admin_ctx->h_caller, pipe_to_delete, NFC_RES_OK);

	hci_pipe_detach_and_free(pipe_to_delete);

	hci_core_persistence_store(admin_ctx->core_ctx);

	hci_core_send_rsp_no_param(admin_ctx->core_ctx, pipe, HCI_RSP_ANY_OK);
}

static void hci_admin_handle_clear_all_pipe(struct hci_admin_context *admin_ctx, struct hci_pipe *pipe, nfc_u8 *p_data, nfc_u32 data_len)
{
	struct hci_pipe **pipes;
	nfc_u8 num_pipes, i;

	if (data_len != sizeof(struct hci_cmd_adm_clear_all_pipe)) {
		hci_core_send_rsp_no_param(admin_ctx->core_ctx, pipe, HCI_RSP_ANY_E_CMD_PAR_UNKNOWN);
		return;
	}

	/* assuming remote host is UICC */
	pipes = hci_core_list_all_pipes(admin_ctx->core_ctx, HCI_HOST_ID_UICC_HOST, &num_pipes);

	osa_report(INFORMATION, ("hci_admin_handle_clear_all_pipe: deleting %d pipes", num_pipes));

	for (i=0; i<num_pipes; i++) {
		if (admin_ctx->callbacks->pipe_deleted)
			admin_ctx->callbacks->pipe_deleted(admin_ctx->h_caller, pipes[i], NFC_RES_OK);

		hci_pipe_detach_and_free(pipes[i]);
	}

	hci_core_persistence_store(admin_ctx->core_ctx);

	if (pipes)
		osa_mem_free(pipes);

	/* ILAN: FW doesn't expect a response in the HCI workaround */
//	hci_core_send_rsp_no_param(admin_ctx->core_ctx, pipe, HCI_RSP_ANY_OK);
}

static void hci_admin_cmd_recv_cb(nfc_handle_t h_caller, struct hci_pipe *pipe, nfc_u8 inst, nfc_u8 *p_data, nfc_u32 data_len)
{
	struct hci_admin_context *admin_ctx = (struct hci_admin_context*)h_caller;

	switch (inst) {
		case (HCI_CMD_ANY_OPEN_PIPE):
			hci_admin_handle_open_pipe(admin_ctx, pipe, p_data, data_len);
			break;
		case (HCI_CMD_ANY_CLOSE_PIPE):
			hci_admin_handle_close_pipe(admin_ctx, pipe, p_data, data_len);
			break;
		case (HCI_CMD_ADM_NOTIFY_PIPE_CREATED):
			hci_admin_handle_notify_pipe_created(admin_ctx, pipe, p_data, data_len);
			break;
		case (HCI_CMD_ADM_NOTIFY_PIPE_DELETED):
			hci_admin_handle_notify_pipe_deleted(admin_ctx, pipe, p_data, data_len);
			break;
		case (HCI_CMD_ADM_NOTIFY_ALL_PIPE_CLEARED):
			hci_admin_handle_notify_all_pipe_cleared(admin_ctx, pipe, p_data, data_len);
			break;

		/* HCI workaround */
		case (HCI_CMD_ADM_CREATE_PIPE):
			hci_admin_handle_create_pipe(admin_ctx, pipe, p_data, data_len);
			break;
		case (HCI_CMD_ADM_DELETE_PIPE):
			hci_admin_handle_delete_pipe(admin_ctx, pipe, p_data, data_len);
			break;
		case (HCI_CMD_ADM_CLEAR_ALL_PIPE):
			hci_admin_handle_clear_all_pipe(admin_ctx, pipe, p_data, data_len);
			break;
		/* HCI workaround */

		default:
			hci_core_send_rsp_no_param(admin_ctx->core_ctx, pipe, HCI_RSP_ANY_E_CMD_NOT_SUPPORTED);
			break;
	}
}

static void hci_admin_handle_hot_plug(struct hci_admin_context *admin_ctx, struct hci_pipe *pipe, nfc_u8 *p_data, nfc_u32 data_len)
{
	if (admin_ctx->callbacks->hot_plug)
		admin_ctx->callbacks->hot_plug(admin_ctx->h_caller, pipe);
}

static void hci_admin_evt_recv_cb(nfc_handle_t h_caller, struct hci_pipe *pipe, nfc_u8 inst, nfc_u8 *p_data, nfc_u32 data_len)
{
	struct hci_admin_context *admin_ctx = (struct hci_admin_context*)h_caller;

	if (hci_pipe_get_state(pipe) != HCI_PIPE_STATE_OPEN) {
		osa_report(INFORMATION, ("hci_admin_evt_recv_cb: pipe state is not OPEN"));
		return;
	}

	switch (inst) {
		case (HCI_EVT_HOT_PLUG):
			hci_admin_handle_hot_plug(admin_ctx, pipe, p_data, data_len);
			break;
		default:
			osa_report(INFORMATION, ("hci_admin_evt_recv_cb: unsupported inst 0x%x", inst));
			break;
	}
}

static void hci_admin_handle_get_param_rsp(struct hci_admin_context *admin_ctx, struct hci_pipe *pipe, nfc_u8 inst, nfc_u8 *p_data, nfc_u32 data_len)
{
	if (admin_ctx->callbacks->host_list)
		admin_ctx->callbacks->host_list(admin_ctx->h_caller, pipe, ((inst == HCI_RSP_ANY_OK) ? (NFC_RES_OK) : (NFC_RES_ERROR)), p_data, data_len);
}

static void hci_admin_handle_open_rsp(struct hci_admin_context *admin_ctx, struct hci_pipe *pipe, nfc_u8 inst, nfc_u8 *p_data, nfc_u32 data_len)
{
	nfc_status status;

	osa_report(INFORMATION, ("hci_admin_handle_open_rsp: pipe state 0x%x, inst 0x%x", hci_pipe_get_state(pipe), inst));

	if (inst == HCI_RSP_ANY_OK) {
		hci_pipe_set_state(pipe, HCI_PIPE_STATE_OPEN);
		status = NFC_RES_OK;
	} else {
		hci_pipe_set_state(pipe, HCI_PIPE_STATE_CLOSED);
		status = NFC_RES_ERROR;
	}

	hci_core_persistence_store(admin_ctx->core_ctx);

	if (admin_ctx->callbacks->pipe_opened)
		admin_ctx->callbacks->pipe_opened(admin_ctx->h_caller, pipe, status);
}

static void hci_admin_handle_close_rsp(struct hci_admin_context *admin_ctx, struct hci_pipe *pipe, nfc_u8 inst, nfc_u8 *p_data, nfc_u32 data_len)
{
	nfc_status status;

	osa_report(INFORMATION, ("hci_admin_handle_close_rsp: pipe state 0x%x, inst 0x%x", hci_pipe_get_state(pipe), inst));

	if (inst == HCI_RSP_ANY_OK) {
		hci_pipe_set_state(pipe, HCI_PIPE_STATE_CLOSED);
		status = NFC_RES_OK;
	} else {
		hci_pipe_set_state(pipe, HCI_PIPE_STATE_OPEN);
		status = NFC_RES_ERROR;
	}

	hci_core_persistence_store(admin_ctx->core_ctx);

	if (admin_ctx->callbacks->pipe_closed)
		admin_ctx->callbacks->pipe_closed(admin_ctx->h_caller, pipe, status);
}

static void hci_admin_handle_create_rsp(struct hci_admin_context *admin_ctx, struct hci_pipe *pipe, nfc_u8 inst, nfc_u8 *p_data, nfc_u32 data_len)
{
	struct hci_rsp_adm_create_pipe *rsp = (struct hci_rsp_adm_create_pipe*)p_data;
	struct hci_gate *gate;
	struct hci_pipe *new_pipe = NULL;
	nfc_status status = NFC_RES_OK;

	if (data_len != sizeof(struct hci_rsp_adm_create_pipe)) {
		osa_report(ERROR, ("hci_admin_handle_create_rsp: invalid data_len %ld", data_len));
		status = NFC_RES_ERROR;
		goto exit;
	}

	if (rsp->src_host_id != HCI_HOST_ID_TERMINAL_HOST) {
		osa_report(ERROR, ("hci_admin_handle_create_rsp: invalid src_host_id 0x%x", rsp->src_host_id));
		status = NFC_RES_ERROR;
		goto exit;
	}

	gate = hci_core_find_gate(admin_ctx->core_ctx, rsp->src_gate_id);
	if (!gate) {
		osa_report(ERROR, ("hci_admin_handle_create_rsp: unable to find src_gate_id 0x%x", rsp->src_gate_id));
		status = NFC_RES_ERROR;
		goto exit;
	}

	new_pipe = hci_pipe_alloc_and_attach(gate, rsp->pipe_id, rsp->dst_host_id, HCI_PIPE_STATE_CLOSED);
	if (!new_pipe) {
		osa_report(ERROR, ("hci_admin_handle_create_rsp: failed alloc new pipe"));
		status = NFC_RES_ERROR;
		goto exit;
	}

	hci_core_persistence_store(admin_ctx->core_ctx);

exit:
	if (admin_ctx->callbacks->pipe_created)
		admin_ctx->callbacks->pipe_created(admin_ctx->h_caller, new_pipe, status);
}

static void hci_admin_handle_delete_rsp(struct hci_admin_context *admin_ctx, struct hci_pipe *pipe, nfc_u8 inst, nfc_u8 *p_data, nfc_u32 data_len)
{
	struct hci_pipe* pipe_to_delete;
	nfc_status status = NFC_RES_OK;

	pipe_to_delete = hci_core_find_pipe(admin_ctx->core_ctx, admin_ctx->delete_pipe_id);
	if (!pipe_to_delete) {
		osa_report(ERROR, ("hci_admin_handle_delete_rsp: unable to find delete_pipe_id 0x%x", admin_ctx->delete_pipe_id));
		status = NFC_RES_ERROR;
	}

	if (admin_ctx->callbacks->pipe_deleted)
		admin_ctx->callbacks->pipe_deleted(admin_ctx->h_caller, pipe_to_delete, status);

	if (pipe_to_delete) {
		hci_pipe_detach_and_free(pipe_to_delete);

		hci_core_persistence_store(admin_ctx->core_ctx);
	}
}

static void hci_admin_rsp_recv_cb(nfc_handle_t h_caller, struct hci_pipe *pipe, nfc_u8 inst, nfc_u8 *p_data, nfc_u32 data_len)
{
	struct hci_admin_context *admin_ctx = (struct hci_admin_context*)h_caller;
	nfc_u8 pending_cmd = hci_pipe_get_pending_cmd(pipe);

	hci_pipe_set_pending_cmd(pipe, HCI_CMD_NONE);

	switch (pending_cmd) {
		case (HCI_CMD_ANY_GET_PARAM):
			hci_admin_handle_get_param_rsp(admin_ctx, pipe, inst, p_data, data_len);
			break;
		case (HCI_CMD_ANY_OPEN_PIPE):
			hci_admin_handle_open_rsp(admin_ctx, pipe, inst, p_data, data_len);
			break;
		case (HCI_CMD_ANY_CLOSE_PIPE):
			hci_admin_handle_close_rsp(admin_ctx, pipe, inst, p_data, data_len);
			break;
		case (HCI_CMD_ADM_CREATE_PIPE):
			hci_admin_handle_create_rsp(admin_ctx, pipe, inst, p_data, data_len);
			break;
		case (HCI_CMD_ADM_DELETE_PIPE):
			hci_admin_handle_delete_rsp(admin_ctx, pipe, inst, p_data, data_len);
			break;
		default:
			osa_report(ERROR, ("hci_admin_rsp_recv_cb: unexpected pending_cmd 0x%x", pending_cmd));
			break;
	}
}

static struct hci_gate_callbacks hci_admin_gate_callbacks = {
	.cmd_recv = hci_admin_cmd_recv_cb,
	.evt_recv = hci_admin_evt_recv_cb,
	.rsp_recv = hci_admin_rsp_recv_cb,
	.cmd_timeout = 0,
};

struct hci_admin_context* hci_admin_create(struct hci_core_context *core_ctx, struct hci_admin_callbacks *callbacks, nfc_handle_t h_caller)
{
	struct hci_admin_context *admin_ctx;

	osa_report(INFORMATION, ("hci_admin_create"));

	admin_ctx = (struct hci_admin_context*)osa_mem_alloc(sizeof(struct hci_admin_context));
	if (!admin_ctx)
		goto exit;

	osa_mem_zero(admin_ctx, sizeof(struct hci_admin_context));

	admin_ctx->core_ctx = core_ctx;

	admin_ctx->gate = hci_gate_alloc(HCI_GATE_ID_ADMIN, &hci_admin_gate_callbacks, admin_ctx);
	if (!admin_ctx->gate)
		goto exit_free_ctx;

	/* special 1 static admin pipe connected to the host controller */
	admin_ctx->pipe = hci_pipe_alloc_and_attach(admin_ctx->gate, HCI_PIPE_ID_ADMIN, HCI_HOST_ID_HOST_CONTROLLER, HCI_PIPE_STATE_CLOSED);
	if (!admin_ctx->pipe)
		goto exit_free_gate;

	hci_core_register_gate(core_ctx, admin_ctx->gate);

	admin_ctx->callbacks = callbacks;
	admin_ctx->h_caller = h_caller;

	return admin_ctx;

exit_free_gate:
	hci_gate_free(admin_ctx->gate);

exit_free_ctx:
	osa_mem_free(admin_ctx);

exit:
	return 0;
}

void hci_admin_destroy(struct hci_admin_context *admin_ctx)
{
	osa_report(INFORMATION, ("hci_admin_destroy"));

	if (admin_ctx) {
		hci_core_unregister_gate(admin_ctx->core_ctx, admin_ctx->gate);

		hci_pipe_detach_and_free(admin_ctx->pipe);

		hci_gate_free(admin_ctx->gate);

		osa_mem_free(admin_ctx);
	}
}

inline nfc_status hci_admin_open(struct hci_admin_context *admin_ctx)
{
	return hci_core_send_open_pipe(admin_ctx->core_ctx, admin_ctx->pipe);
}

inline nfc_status hci_admin_close(struct hci_admin_context *admin_ctx)
{
	return hci_core_send_close_pipe(admin_ctx->core_ctx, admin_ctx->pipe);
}

nfc_status hci_admin_create_pipe(struct hci_admin_context *admin_ctx, struct hci_gate *local_gate, nfc_u8 dst_host_id, nfc_u8 dst_gate_id)
{
	struct hci_cmd_adm_create_pipe *cmd;
	struct hcp_msg* msg;

	if (hci_pipe_get_state(admin_ctx->pipe) != HCI_PIPE_STATE_OPEN)
		return NFC_RES_STATE_ERROR;

	if (hci_pipe_get_pending_cmd(admin_ctx->pipe) != HCI_CMD_NONE)
		return NFC_RES_ERROR;

	msg = hcp_msg_alloc(HCI_TYPE_CMD, HCI_CMD_ADM_CREATE_PIPE, sizeof(struct hci_cmd_adm_create_pipe));
	if (!msg)
		return NFC_RES_MEM_ERROR;

	cmd = (struct hci_cmd_adm_create_pipe*)hcp_msg_get_data(msg);

	cmd->src_gate_id = hci_gate_get_id(local_gate);
	cmd->dst_host_id = dst_host_id;
	cmd->dst_gate_id = dst_gate_id;

	hci_pipe_set_pending_cmd(admin_ctx->pipe, HCI_CMD_ADM_CREATE_PIPE);

	hci_core_send_hcp_msg(admin_ctx->core_ctx, admin_ctx->pipe, msg);

	return NFC_RES_PENDING;
}

nfc_status hci_admin_delete_pipe(struct hci_admin_context *admin_ctx, struct hci_pipe *pipe)
{
	struct hci_cmd_adm_delete_pipe *cmd;
	struct hcp_msg* msg;

	if (hci_pipe_get_state(admin_ctx->pipe) != HCI_PIPE_STATE_OPEN)
		return NFC_RES_STATE_ERROR;

	if (hci_pipe_get_pending_cmd(admin_ctx->pipe) != HCI_CMD_NONE)
		return NFC_RES_ERROR;

	msg = hcp_msg_alloc(HCI_TYPE_CMD, HCI_CMD_ADM_DELETE_PIPE, sizeof(struct hci_cmd_adm_delete_pipe));
	if (!msg)
		return NFC_RES_MEM_ERROR;

	cmd = (struct hci_cmd_adm_delete_pipe*)hcp_msg_get_data(msg);

	cmd->pipe_id = hci_pipe_get_id(pipe);
	admin_ctx->delete_pipe_id = cmd->pipe_id;

	hci_pipe_set_pending_cmd(admin_ctx->pipe, HCI_CMD_ADM_DELETE_PIPE);

	hci_core_send_hcp_msg(admin_ctx->core_ctx, admin_ctx->pipe, msg);

	return NFC_RES_PENDING;
}

inline nfc_status hci_admin_get_host_list(struct hci_admin_context *admin_ctx)
{
	return hci_core_send_get_param(admin_ctx->core_ctx, admin_ctx->pipe, HCI_REGISTRY_PARAM_ID_HOST_LIST);
}
