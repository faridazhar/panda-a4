/*
 * ncix_nfcee.c
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

#include "nci_api.h"

#if (NCI_VERSION_IN_USE==NCI_DRAFT_9)

#include "nfc_types.h"
#include "nci_dev.h"
#include "nfc_os_adapt.h"
#include "ncix_int.h"


#define NFCEE_MAX_LOGICAL_CONNS (4) // TBD - usage should be replaced by API from Kobi
#define NFCEE_INVALID_TARGET_HNDL (0xff)

/*******************************************************************************

  Implementation of the NFC HAL Stratup Process (Reset and Connet Functions)

*******************************************************************************/


static enum nfcee_states
{
	E_STATE_REMOVED,
	E_STATE_OFF,
	E_STATE_ACTIVATING_RF_COMM,
	E_STATE_ACTIVATING_HOST_COMM,
	E_STATE_RF_COMM,
	E_STATE_CONNECTING,
	E_STATE_HOST_COMM,
	E_STATE_DIS_CONNECTING,
	E_STATE_DE_ACTIVATING,
	NCIX_NFCEE_NUM_STATES
};


static enum nfcee_events
{
	E_EVT_SWITCH_TO_RF_COMM_C,
	E_EVT_SWITCH_TO_HOST_COMM_C,
	E_EVT_SWITCH_TO_OFF_C,
	E_EVT_NFCEE_NTF_REMOVED_N,
	E_EVT_NFCEE_MODE_SET_RSP_OK,
	E_EVT_NFCEE_MODE_SET_RSP_FAIL,
	E_EVT_CORE_CONN_CREATE_RSP_OK,
	E_EVT_CORE_CONN_CREATE_RSP_FAIL,
	E_EVT_CORE_CONN_CLOSE_RSP_OK,
	E_EVT_CORE_CONN_CLOSE_RSP_FAIL,
	E_EVT_NCIX_NFCEE_NUM_EVTS
};

struct ncix_nfcee
{
	nfc_handle_t						h_nci;
	nfc_handle_t						h_ncix;
	nfc_u8							target_hndl;
	nfc_u8							num_nfcee_protocols;  /* Number of supported NFCEE Protocols */
	nfc_u8							nfcee_protocols[NCI_MAX_NFCEE_PROTOCOLS];
	nfc_handle_t						h_sm;
	struct nci_rsp_conn_create		host_comm_params;
};

struct ncix_nfcees_db
{
	nfc_handle_t			h_nci;
	nfc_handle_t			h_ncix;
	struct ncix_nfcee	*p_nfcees;
	nfc_u8				num_nfcees;
};


/**************************************/
/*** Static Action helper utilities ***/
/**************************************/

//typedef void (*rsp_cb_f)(nfc_handle_t, nfc_u16, nci_rsp_param_u*)

/******************************************************************************
*
* Name: static_send_nfcee_mode_set
*
* Description: Prepares nfcee_mode_set command and send it.
*
* Input: 	p_nfcee - handle to NFCEE object
*		target_hndl -Target handle received from NAL (SE \ UICC)
*		mode - NFCEE mode_set command opcode
*		f_rsp - Response callback function
*
* Output: None
*
* Return: Success - NCI_STATUS_OK
*		  Failure - NCI_STATUS_BUFFER_FULL \ NCI_STATUS_FAILED
*
*******************************************************************************/
static void static_send_nfcee_mode_set(struct ncix_nfcee *p_nfcee, nfc_u8 target_hndl, nfc_u8 mode, rsp_cb_f f_rsp)
{
	nci_cmd_param_u nci_cmd;
	nci_status_e status = NCI_STATUS_OK;

/* Send NCI_OPCODE_NFCEE_MODE_SET_CMD */
	nci_cmd.nfcee_mode_set.nfcee_mode = mode;
	nci_cmd.nfcee_mode_set.target_handle = target_hndl;
	status = nci_send_cmd(p_nfcee->h_nci, NCI_OPCODE_NFCEE_MODE_SET_CMD, &nci_cmd, NULL, 0, f_rsp, p_nfcee);

	if(NCI_STATUS_OK != status)
	{
		OSA_ASSERT(0);
	}

}

/******************************************************************************
*
* Name: static_mode_set_rsp
*
* Description: Response callback that is called by NFCEE state machine when handling events.
*
* Input: 	h_nfcee - handle to NFCEE object
*		opcode - NFCEE mode_set command opcode
*		pu_rsp - NCI parameter structure
*
* Output: None
*
* Return: None
*
*******************************************************************************/
static void static_mode_set_rsp(nfc_handle_t h_nfcee, nfc_u16 opcode, nci_rsp_param_u *pu_rsp)
{
	struct ncix_nfcee *p_nfcee = (struct ncix_nfcee*)h_nfcee;
	OSA_UNUSED_PARAMETER(opcode);

	ncix_get_rsp_ptr(p_nfcee->h_ncix)->generic.status = pu_rsp->generic.status;

	if(NCI_STATUS_OK == pu_rsp->generic.status)
	{
		ncix_sm_process_evt(p_nfcee->h_sm, E_EVT_NFCEE_MODE_SET_RSP_OK);
	}
	else
	{
		ncix_sm_process_evt(p_nfcee->h_sm, E_EVT_NFCEE_MODE_SET_RSP_FAIL);
	}
}

/******************************************************************************
*
* Name: static_send_nfcee_conn_create
*
* Description: Prepares nfcee_conn_create command and send it.
*
* Input: 	p_nfcee - handle to NFCEE object
*		target_hndl -Target handle received from NAL (SE \ UICC)
*		f_rsp - Response callback function
*
* Output: None
*
* Return: None (maybe it should?)
*
*******************************************************************************/
static void static_send_nfcee_conn_create(struct ncix_nfcee *p_nfcee, nfc_u8 target_hndl, rsp_cb_f f_rsp)
{
	nci_cmd_param_u nci_cmd;
	nci_status_e status = NCI_STATUS_OK;

	/* Send NCI_OPCODE_CORE_CONN_CREATE_CMD */

	nci_cmd.conn_create.target_handle = target_hndl;

	nci_cmd.conn_create.num_of_tlvs = 0;
	nci_cmd.conn_create.p_tlv = NULL;
	nci_cmd.conn_create.total_length = 0;

	status = nci_send_cmd(p_nfcee->h_nci, NCI_OPCODE_CORE_CONN_CREATE_CMD, &nci_cmd, NULL, 0, f_rsp, p_nfcee);

	if(NCI_STATUS_OK != status)
	{
		OSA_ASSERT(0);
	}

}

/******************************************************************************
*
* Name: static_conn_create_rsp
*
* Description: Response callback that is called by NFCEE state machine when handling events.
*
* Input: 	h_nfcee - handle to NFCEE object
*		opcode - NFCEE mode_set command opcode
*		pu_rsp - NCI parameter structure
*
* Output: None
*
* Return: None
*
*******************************************************************************/
static void static_conn_create_rsp(nfc_handle_t h_nfcee, nfc_u16 opcode, nci_rsp_param_u *pu_rsp)
{
	struct ncix_nfcee *p_nfcee = (struct ncix_nfcee*)h_nfcee;
	struct nci_rsp_conn_create *p_conn_rsp = (struct nci_rsp_conn_create*)pu_rsp;
	OSA_UNUSED_PARAMETER(opcode);

	ncix_get_rsp_ptr(p_nfcee->h_ncix)->generic.status = pu_rsp->generic.status;

	if(NCI_STATUS_OK == pu_rsp->generic.status)
	{
		//Prepare params for response
		ncix_get_rsp_ptr(p_nfcee->h_ncix)->nfcee.target_hndl = p_nfcee->target_hndl;
		ncix_get_rsp_ptr(p_nfcee->h_ncix)->nfcee.host_comm_params = *p_conn_rsp;

		//Store connection params. localy
		p_nfcee->host_comm_params = *p_conn_rsp;

		ncix_sm_process_evt(p_nfcee->h_sm, E_EVT_CORE_CONN_CREATE_RSP_OK);
	}
	else
	{
		ncix_sm_process_evt(p_nfcee->h_sm, E_EVT_CORE_CONN_CREATE_RSP_FAIL);
	}
}

/******************************************************************************
*
* Name: static_send_nfcee_conn_close
*
* Description: Prepares nfcee_conn_close command and send it.
*
* Input: 	p_nfcee - handle to NFCEE object
*		conn_id -Connection ID
*		f_rsp - Response callback function
*
* Output: None
*
* Return: None (maybe it should?)
*
*******************************************************************************/
static void static_send_nfcee_conn_close(struct ncix_nfcee *p_nfcee, nfc_u8 conn_id, rsp_cb_f f_rsp)
{
	nci_cmd_param_u nci_cmd;
	nci_status_e status = NCI_STATUS_OK;

	/* Send NCI_OPCODE_CORE_CONN_CLOSE_CMD */

	nci_cmd.conn_close.conn_id = conn_id;

	status = nci_send_cmd(p_nfcee->h_nci, NCI_OPCODE_CORE_CONN_CLOSE_CMD, &nci_cmd, NULL, 0, f_rsp, p_nfcee);

	if(NCI_STATUS_OK != status)
	{
		OSA_ASSERT(0);
	}

}

/******************************************************************************
*
* Name: static_conn_close_rsp
*
* Description: Response callback that is called by NFCEE state machine when handling events.
*
* Input: 	h_nfcee - handle to NFCEE object
*		opcode - NFCEE mode_set command opcode
*		pu_rsp - NCI parameter structure
*
* Output: None
*
* Return: None
*
*******************************************************************************/
static void static_conn_close_rsp(nfc_handle_t h_nfcee, nfc_u16 opcode, nci_rsp_param_u *pu_rsp)
{
	struct ncix_nfcee *p_nfcee = (struct ncix_nfcee*)h_nfcee;
	OSA_UNUSED_PARAMETER(opcode);

	ncix_get_rsp_ptr(p_nfcee->h_ncix)->generic.status = pu_rsp->generic.status;

	if(NCI_STATUS_OK == pu_rsp->generic.status)
	{
		ncix_sm_process_evt(p_nfcee->h_sm, E_EVT_CORE_CONN_CLOSE_RSP_OK);
	}
	else
	{
		ncix_sm_process_evt(p_nfcee->h_sm, E_EVT_CORE_CONN_CLOSE_RSP_FAIL);
	}
}

/***************************************/
/*** SM Action Callbacks (per event) ***/
/***************************************/
/******************************************************************************
*
* Name: action_evt_switch_to_rf_comm
*
* Description: Called by NCI core when event EVT_SWITCH_TO_RF_COMM_C occurs.
*
* Input: 	h_nfcee - handle to NFCEE object
*		curr_state - event to handle
*
* Output: None
*
* Return: Next state in SM according to current state and event
*
*******************************************************************************/
nfc_u32 action_evt_switch_to_rf_comm(nfc_handle_t h_nfcee, nfc_u32 curr_state)
{
	struct ncix_nfcee *p_nfcee = (struct ncix_nfcee*)h_nfcee;
	nfc_u32 next_state = curr_state;

	switch(curr_state)
	{
	case E_STATE_OFF:
		static_send_nfcee_mode_set(p_nfcee, p_nfcee->target_hndl, NCI_NFCEE_MODE_ACTIVATE, static_mode_set_rsp);
		next_state = E_STATE_ACTIVATING_RF_COMM;
		break;
	case E_STATE_RF_COMM:
		//Do nothing, send complete operation with status OK
		ncix_op_complete(p_nfcee->h_ncix, NCI_STATUS_OK);
		next_state = E_STATE_RF_COMM;
		break;
	case E_STATE_HOST_COMM:
		//Send CORE_CONN_CLOSE_CMD(conn_id)
		static_send_nfcee_conn_close(p_nfcee, p_nfcee->host_comm_params.conn_id, static_conn_close_rsp);
		next_state = E_STATE_DIS_CONNECTING;
		break;
	default:
		OSA_ASSERT(0);
	}
	return next_state;
}

/******************************************************************************
*
* Name: action_evt_switch_to_host_comm
*
* Description: Called by NCI core when event EVT_SWITCH_TO_HOST_COMM_C occurs.
*
* Input: 	h_nfcee - handle to NFCEE object
*		curr_state - event to handle
*
* Output: None
*
* Return: Next state in SM according to current state and event
*
*******************************************************************************/
nfc_u32 action_evt_switch_to_host_comm(nfc_handle_t h_nfcee, nfc_u32 curr_state)
{
	struct ncix_nfcee *p_nfcee = (struct ncix_nfcee*)h_nfcee;
	nfc_u32 next_state = curr_state;

	switch(curr_state)
	{
	case E_STATE_OFF:
		static_send_nfcee_mode_set(h_nfcee, p_nfcee->target_hndl, NCI_NFCEE_MODE_ACTIVATE, static_mode_set_rsp);
		next_state = E_STATE_ACTIVATING_HOST_COMM;
		break;
	case E_STATE_RF_COMM:
		//Send CORE_CONN_CREATE_CMD(tgt_hndl)
		static_send_nfcee_conn_create(p_nfcee, p_nfcee->target_hndl, static_conn_create_rsp);
		next_state = E_STATE_CONNECTING;
		break;
	case E_STATE_HOST_COMM:
		//Do nothing, send complete operation with status OK and existing connection ID
		ncix_op_complete(p_nfcee->h_ncix, NCI_STATUS_OK);
		next_state = E_STATE_HOST_COMM;
		break;
	default:
		OSA_ASSERT(0);
	}
	return next_state;
}

/******************************************************************************
*
* Name: action_evt_switch_to_off
*
* Description: Called by NCI core when event EVT_SWITCH_TO_OFF_C occurs.
*
* Input: 	h_nfcee - handle to NFCEE object
*		curr_state - event to handle
*
* Output: None
*
* Return: Next state in SM according to current state and event
*
*******************************************************************************/
nfc_u32 action_evt_switch_to_off(nfc_handle_t h_nfcee, nfc_u32 curr_state)
{
	struct ncix_nfcee *p_nfcee = (struct ncix_nfcee*)h_nfcee;
	nfc_u32 next_state = curr_state;

	switch(curr_state)
	{
	case E_STATE_OFF:
		//Do nothing, send complete operation with status OK
		ncix_op_complete(p_nfcee->h_ncix, NCI_STATUS_OK);
		next_state = E_STATE_OFF;
		break;
	case E_STATE_RF_COMM:
		static_send_nfcee_mode_set(p_nfcee, p_nfcee->target_hndl, NCI_NFCEE_MODE_DEACTIVATE, static_mode_set_rsp);
		next_state = E_STATE_DE_ACTIVATING;
		break;
	case E_STATE_HOST_COMM:
		//HACK - remove - Send CORE_CONN_CLOSE_CMD(conn_id) - without waiting for response
		static_send_nfcee_conn_close(p_nfcee, p_nfcee->host_comm_params.conn_id, NULL);
		static_send_nfcee_mode_set(p_nfcee, p_nfcee->target_hndl, NCI_NFCEE_MODE_DEACTIVATE, static_mode_set_rsp);
		next_state = E_STATE_DE_ACTIVATING;
		break;
	default:
		OSA_ASSERT(0);
	}
	return next_state;
}


/******************************************************************************
*
* Name: action_evt_mode_set_rsp_ok
*
* Description: Called by NCI core when event EVT_NFCEE_MODE_SET_RSP_OK occurs.
*
* Input: 	h_nfcee - handle to NFCEE object
*		curr_state - event to handle
*
* Output: None
*
* Return: Next state in SM according to current state and event
*
*******************************************************************************/
nfc_u32 action_evt_mode_set_rsp_ok(nfc_handle_t h_nfcee, nfc_u32 curr_state)
{
	struct ncix_nfcee *p_nfcee = (struct ncix_nfcee*)h_nfcee;
	nfc_u32 next_state = curr_state;

	switch(curr_state)
	{
	case E_STATE_ACTIVATING_RF_COMM:
		//Send operation complete
		ncix_op_complete(p_nfcee->h_ncix, NCI_STATUS_OK);
		next_state = E_STATE_RF_COMM;
		break;

	case E_STATE_ACTIVATING_HOST_COMM:
		//Send CORE_CONN_CREATE_CMD(tgt_hndl)
		static_send_nfcee_conn_create(p_nfcee, p_nfcee->target_hndl, static_conn_create_rsp);
		next_state = E_STATE_CONNECTING;
		break;

	case E_STATE_DE_ACTIVATING:
		//Send operation complete
		ncix_op_complete(p_nfcee->h_ncix, NCI_STATUS_OK);
		next_state = E_STATE_OFF;
		break;
	default:
		OSA_ASSERT(0);
	}
	return next_state;
}

/******************************************************************************
*
* Name: action_evt_mode_set_rsp_fail
*
* Description: Called by NCI core when event EVT_NFCEE_MODE_SET_RSP_FAIL occurs.
*
* Input: 	h_nfcee - handle to NFCEE object
*		curr_state - event to handle
*
* Output: None
*
* Return: Next state in SM according to current state and event
*
*******************************************************************************/
nfc_u32 action_evt_mode_set_rsp_fail(nfc_handle_t h_nfcee, nfc_u32 curr_state)
{
	struct ncix_nfcee *p_nfcee = (struct ncix_nfcee*)h_nfcee;
	nfc_u32 next_state = curr_state;

	switch(curr_state)
	{
	case E_STATE_ACTIVATING_RF_COMM:
	case E_STATE_ACTIVATING_HOST_COMM:
	case E_STATE_DE_ACTIVATING:
		//Send operation complete with status fail
		ncix_op_complete(p_nfcee->h_ncix, NCI_STATUS_FAILED);
		next_state = E_STATE_OFF;
		break;
	default:
		OSA_ASSERT(0);
	}

	return next_state;
}

/******************************************************************************
*
* Name: action_evt_conn_create_rsp_ok
*
* Description: Called by NCI core when event EVT_CORE_CONN_CREATE_RSP_OK occurs.
*
* Input: 	h_nfcee - handle to NFCEE object
*		curr_state - event to handle
*
* Output: None
*
* Return: Next state in SM according to current state and event
*
*******************************************************************************/
nfc_u32 action_evt_conn_create_rsp_ok(nfc_handle_t h_nfcee, nfc_u32 curr_state)
{
	struct ncix_nfcee *p_nfcee = (struct ncix_nfcee*)h_nfcee;
	nfc_u32 next_state = curr_state;

	switch(curr_state)
	{
	case E_STATE_CONNECTING:
		//Send operation complete
		ncix_op_complete(p_nfcee->h_ncix, NCI_STATUS_OK);
		next_state = E_STATE_HOST_COMM;
		break;

	default:
		OSA_ASSERT(0);
	}
	return next_state;
}

/******************************************************************************
*
* Name: action_evt_conn_create_rsp_fail
*
* Description: Called by NCI core when event EVT_CORE_CONN_CREATE_RSP_FAIL occurs.
*
* Input: 	h_nfcee - handle to NFCEE object
*		curr_state - event to handle
*
* Output: None
*
* Return: Next state in SM according to current state and event
*
*******************************************************************************/
nfc_u32 action_evt_conn_create_rsp_fail(nfc_handle_t h_nfcee, nfc_u32 curr_state)
{
	struct ncix_nfcee *p_nfcee = (struct ncix_nfcee*)h_nfcee;
	nfc_u32 next_state = curr_state;

	switch(curr_state)
	{
	case E_STATE_CONNECTING:
		//Send operation complete with failure
		ncix_op_complete(p_nfcee->h_ncix, NCI_STATUS_FAILED);
		next_state = E_STATE_HOST_COMM;
		break;

	default:
		OSA_ASSERT(0);
	}
	return next_state;
}

/******************************************************************************
*
* Name: action_evt_conn_close_rsp_ok
*
* Description: Called by NCI core when event EVT_CORE_CONN_CLOSE_RSP_OK occurs.
*
* Input: 	h_nfcee - handle to NFCEE object
*		curr_state - event to handle
*
* Output: None
*
* Return: Next state in SM according to current state and event
*
*******************************************************************************/
nfc_u32 action_evt_conn_close_rsp_ok(nfc_handle_t h_nfcee, nfc_u32 curr_state)
{
	struct ncix_nfcee *p_nfcee = (struct ncix_nfcee*)h_nfcee;
	nfc_u32 next_state = curr_state;

	switch(curr_state)
	{
	case E_STATE_DIS_CONNECTING:
		//Send operation complete
		ncix_op_complete(p_nfcee->h_ncix, NCI_STATUS_OK);
		next_state = E_STATE_RF_COMM;
		break;

	default:
		OSA_ASSERT(0);
	}
	return next_state;
}

/******************************************************************************
*
* Name: action_evt_conn_close_rsp_fail
*
* Description: Called by NCI core when event EVT_CORE_CONN_CLOSE_RSP_FAIL occurs.
*
* Input: 	h_nfcee - handle to NFCEE object
*		curr_state - event to handle
*
* Output: None
*
* Return: Next state in SM according to current state and event
*
*******************************************************************************/
nfc_u32 action_evt_conn_close_rsp_fail(nfc_handle_t h_nfcee, nfc_u32 curr_state)
{
	struct ncix_nfcee *p_nfcee = (struct ncix_nfcee*)h_nfcee;
	nfc_u32 next_state = curr_state;

	switch(curr_state)
	{
	case E_STATE_DIS_CONNECTING:
		//Send operation complete
		ncix_op_complete(p_nfcee->h_ncix, NCI_STATUS_FAILED);
		next_state = E_STATE_RF_COMM;
		OSA_ASSERT(0); //TBD - decide what to do in that case
		break;

	default:
		OSA_ASSERT(0);
	}
	return next_state;
}

static struct ncix_state s_nfcee_state[] =
{
	{ NULL,	"NFCEE_STATE_REMOVED" },
	{ NULL,	"NFCEE_STATE_OFF" },
	{ NULL,	"NFCEE_STATE_ACTIVATING_RF_COMM" },
	{ NULL,	"NFCEE_STATE_ACTIVATING_HOST_COMM" },
	{ NULL,	"E_STATE_RF_COMM" },
	{ NULL,	"E_STATE_RF_CONNECTING" },
	{ NULL,	"NFCEE_STATE_HOST_COMM" },
	{ NULL,	"NFCEE_STATE_DIS_CONNECTING" },
	{ NULL,	"NFCEE_STATE_DE_ACTIVATING" },
};

static struct ncix_event s_nfcee_event[] =
{
	{ action_evt_switch_to_rf_comm,		"EVT_SWITCH_TO_RF_COMM_C" },
	{ action_evt_switch_to_host_comm,	"EVT_SWITCH_TO_HOST_COMM_C" },
	{ action_evt_switch_to_off,			"EVT_SWITCH_TO_OFF_C" },
	{ NULL,								"EVT_NFCEE_NTF_REMOVED_N" },
	{ action_evt_mode_set_rsp_ok,		"EVT_NFCEE_MODE_SET_RSP_OK" },
	{ action_evt_mode_set_rsp_fail,		"EVT_NFCEE_MODE_SET_RSP_FAIL" },
	{ action_evt_conn_create_rsp_ok,	"EVT_CORE_CONN_CREATE_RSP_OK" },
	{ action_evt_conn_create_rsp_fail,	"EVT_CORE_CONN_CREATE_RSP_FAIL" },
	{ action_evt_conn_close_rsp_ok,		"EVT_CORE_CONN_CLOSE_RSP_OK" },
	{ action_evt_conn_close_rsp_fail,	"EVT_CORE_CONN_CLOSE_RSP_FAIL" },
};

/******************************************************************************
* __DORON_ASSA__
*
* Name: find_free_nfcee
*
* Description: find free entry (invalid target_hndl) in NFCEE interfaces database
*
* Input: 	p_db - Handle to NFCEE interfaces database
*
* Output: None
*
* Return: Handle to free entry or NULL if no free entry is found
*
*******************************************************************************/
static struct ncix_nfcee* find_free_nfcee(struct ncix_nfcees_db *p_db)
{
	nfc_u8 i = 0;
	for(i = 0; i < p_db->num_nfcees; i++)
	{
		if(NULL == p_db->p_nfcees[i].h_sm && NFCEE_INVALID_TARGET_HNDL == p_db->p_nfcees[i].target_hndl)
		{
			return &(p_db->p_nfcees[i]);
		}
	}

	OSA_ASSERT(0); //TBD

	return NULL;
}

/******************************************************************************
* __DORON_ASSA__
*
* Name: find_nfcee
*
* Description: find entry in NFCEE interfaces database that its target_hndl is same as incoming param target_hndl
*
* Input: 	p_db - Handle to NFCEE interfaces database
*		target_hndl - target handle to find
*
* Output: None
*
* Return: Handle to found entry or NULL if no such entry is found
*
*******************************************************************************/
static struct ncix_nfcee* find_nfcee(struct ncix_nfcees_db *p_db, nfc_u8 target_hndl)
{
	nfc_u8 i = 0;
	for(i = 0; i < p_db->num_nfcees; i++)
	{
		if(target_hndl == p_db->p_nfcees[i].target_hndl)
		{
			return &(p_db->p_nfcees[i]);
		}
	}

	OSA_ASSERT(0); //TBD

	return NULL;
}



/***************************************/
/*** Module Input Routines           ***/
/***************************************/

/******************************************************************************
* __DORON_ASSA__
*
* Name: hndlr_nfcee_discover_ntf
*
* Description: Callback that is installed by NCIX NFCEE main module creation. It will be invoked by
*			  NCIX NFCEE notification callback in result of NFCEE_DISCOVER request.
*
* Input: 	h_nfcee_db - handle to NFCEE interfaces database
*		opcode - NFCEE opcode
*		pu_ntf - NCI operation structure
*
* Output: None
*
* Return: None
*
*******************************************************************************/
static void hndlr_nfcee_discover_ntf(nfc_handle_t h_nfcee_db, nfc_u16 opcode, nci_ntf_param_u *pu_ntf)
{
	struct ncix_nfcees_db *p_db = (struct ncix_nfcees_db *)h_nfcee_db;
	struct ncix_nfcee *p_nfcee = NULL;

	OSA_ASSERT(opcode == NCI_OPCODE_NFCEE_DISCOVER_NTF);

	switch (pu_ntf->nfcee_discover.nfcee_status)
	{
	case NFCEE_STATUS_CONNECTED_AND_ACTIVE:
		{
			//Now we need to allocate new NFCEE and to initialize its SM
			p_nfcee = find_free_nfcee(p_db);
			if(NULL != p_nfcee)
			{
				p_nfcee->h_sm = ncix_sm_create(p_nfcee, "NFCEE", E_STATE_RF_COMM, s_nfcee_state, s_nfcee_event);
			}
			else
			{
				OSA_ASSERT(0);
			}

			//Copy all nfcee params
			p_nfcee->num_nfcee_protocols = pu_ntf->nfcee_discover.num_nfcee_interface_information_entries;
			osa_mem_copy(p_nfcee->nfcee_protocols, pu_ntf->nfcee_discover.nfcee_protocols, sizeof(nfc_u8)*NCI_MAX_NFCEE_PROTOCOLS);
			p_nfcee->target_hndl = pu_ntf->nfcee_discover.target_handle;

			break;
		}
	case NFCEE_STATUS_CONNECTED_AND_INACTIVE:
		{
			//Now we need to allocate new NFCEE and to initialize its SM
			p_nfcee = find_free_nfcee(p_db);
			if(NULL != p_nfcee)
			{
				p_nfcee->h_sm = ncix_sm_create(p_nfcee, "NFCEE", E_STATE_OFF, s_nfcee_state, s_nfcee_event);
			}
			else
			{
				OSA_ASSERT(0);
			}

			//Copy all nfcee params
			p_nfcee->num_nfcee_protocols = pu_ntf->nfcee_discover.num_nfcee_interface_information_entries;
			osa_mem_copy(p_nfcee->nfcee_protocols, pu_ntf->nfcee_discover.nfcee_protocols, sizeof(nfc_u8)*NCI_MAX_NFCEE_PROTOCOLS);
			p_nfcee->target_hndl = pu_ntf->nfcee_discover.target_handle;

			break;
		}
	case NFCEE_STATUS_REMOVED:
		{
			//just clear the nfcee instance with specified target handle
			//Signal the NAL -??? //TBD
			p_nfcee = find_nfcee(p_db, pu_ntf->nfcee_discover.target_handle);
			if(NULL != p_nfcee)
			{
				ncix_sm_destroy(p_nfcee->h_sm);
				osa_mem_zero(p_nfcee, sizeof(struct ncix_nfcee));
				p_nfcee->target_hndl = NFCEE_INVALID_TARGET_HNDL;

			}
			break;
		}
	default:
		OSA_ASSERT(0);
	}

}

/******************************************************************************
* __DORON_ASSA__
*
* Name: hndlr_op_nfcee
*
* Description: Callback that is installed by NCIX NFCEE main module creation. It will be
*			  invoked by NCIX NFCEE operation callback in result of NFCEE_SWITCH_MODE request.
*
* Input: 	h_nfcee_db - handle to NFCEE interfaces database
*		pu_param - NCI operation structure
*
* Output: None
*
* Return: None
*
*******************************************************************************/
static void hndlr_op_nfcee(nfc_handle_t h_nfcee_db, op_param_u *pu_param)
{
	struct ncix_nfcees_db *p_db = (struct ncix_nfcees_db *)h_nfcee_db;
	struct nci_op_nfcee *p_op_param = (struct nci_op_nfcee*)pu_param;
	struct ncix_nfcee *p_nfcee = NULL;

	//Find correct nfcee according to requested target handle
	p_nfcee = find_nfcee(p_db, p_op_param->target_hndl);
	if(NULL == p_nfcee)
	{
		//if requested nfcee doesn't exists - return error to caller - TBD - change callback prototype from void to nfc_status
		OSA_ASSERT(0);
	}


	ncix_get_rsp_ptr(p_nfcee->h_ncix)->nfcee.new_mode = p_op_param->new_mode;
	ncix_get_rsp_ptr(p_nfcee->h_ncix)->nfcee.target_hndl = p_op_param->target_hndl;
	//Start execution of operation and switch state machine of found nfcee to requested state
	switch(p_op_param->new_mode)
	{
	case E_SWITCH_RF_COMM:
		{
			ncix_sm_process_evt(p_nfcee->h_sm, E_EVT_SWITCH_TO_RF_COMM_C);
			break;
		}
	case E_SWITCH_HOST_COMM:
		{
			ncix_sm_process_evt(p_nfcee->h_sm, E_EVT_SWITCH_TO_HOST_COMM_C);
			break;
		}
	case E_SWITCH_OFF:
		{
			ncix_sm_process_evt(p_nfcee->h_sm, E_EVT_SWITCH_TO_OFF_C);
			break;
		}
	default:
		{
			OSA_ASSERT(0);
		}
	}

}



/***************************************/
/*** Module Constructor/Destructor   ***/
/***************************************/

/******************************************************************************
* __DORON_ASSA__
*
* Name: ncix_nfcee_create
*
* Description: NCIX NFCEE main module initialization.
*			  Allocate memory and initialize parameters.
*			  Register operation & notification callbacks.
*
* Input: 	h_ncix - handle to NCIX main object
*		h_nci - handle to NCI object
*
* Output: None
*
* Return: handle to created NCIX main object
*
*******************************************************************************/
nfc_handle_t ncix_nfcee_create(nfc_handle_t h_ncix, nfc_handle_t h_nci)
{
	nfc_u8 i = 0;
	struct ncix_nfcees_db *p_db = (struct ncix_nfcees_db *)osa_mem_alloc(sizeof(struct ncix_nfcees_db));
	OSA_ASSERT(p_db);

	p_db->h_nci = h_nci;
	p_db->h_ncix = h_ncix;
	p_db->p_nfcees = (struct ncix_nfcee *)osa_mem_alloc(sizeof(struct ncix_nfcee)*NFCEE_MAX_LOGICAL_CONNS);// TBD - usage should be replaced by API from Kobi
	OSA_ASSERT(p_db->p_nfcees);
	osa_mem_zero(p_db->p_nfcees, sizeof(struct ncix_nfcee)*NFCEE_MAX_LOGICAL_CONNS);//TBD
	p_db->num_nfcees = NFCEE_MAX_LOGICAL_CONNS;//TBD


	for(i = 0; i < p_db->num_nfcees; i++)
	{
		p_db->p_nfcees[i].target_hndl = NFCEE_INVALID_TARGET_HNDL;
		p_db->p_nfcees[i].h_nci = h_nci;
		p_db->p_nfcees[i].h_ncix = h_ncix;
	}

	//state machine is per nfcee and will be initiated per NFCEE allocation

	ncix_register_op_cb(h_ncix, NCI_OPERATION_NFCEE_SWITCH_MODE, hndlr_op_nfcee, p_db);
	nci_register_ntf_cb(h_nci, E_NCI_NTF_NFCEE_DISCOVER, hndlr_nfcee_discover_ntf, p_db);

	return p_db;
}

/******************************************************************************
* __DORON_ASSA__
*
* Name: ncix_nfcee_destroy
*
* Description: NCIX RF main module destruction. Frees object allocated memory.
*
* Input:	  h_nfcee_db - handle to NCIX NFCEE main object
*
* Output: None
*
* Return: None
*
*******************************************************************************/
void ncix_nfcee_destroy(nfc_handle_t h_nfcee_db)
{
	nfc_u8 i = 0;
	struct ncix_nfcees_db *p_db = (struct ncix_nfcees_db *)h_nfcee_db;


	//destroy each NFCEE including their sm
	for(i = 0; i < p_db->num_nfcees; i++)
	{
		if(p_db->p_nfcees[i].h_sm != NULL)
		{
			ncix_sm_destroy(p_db->p_nfcees[i].h_sm);
		}
	}

	nci_unregister_ntf_cb(p_db->h_nci, E_NCI_NTF_NFCEE_DISCOVER, hndlr_nfcee_discover_ntf);

	osa_mem_free(p_db->p_nfcees);
	osa_mem_free(p_db);
}

#endif //#if (NCI_VERSION_IN_USE==NCI_DRAFT_9)

/* EOF */
