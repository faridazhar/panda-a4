/*
 * ncix_main.c
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

  This source file contains the Main State Machine (NCI startup/Shutdown)

*******************************************************************************/
#include "nci_api.h"

#if (NCI_VERSION_IN_USE==NCI_DRAFT_9)

#include "nfc_types.h"
#include "nci_dev.h"
#include "nfc_os_adapt.h"
#include "ncix_int.h"



static enum
{
	E_STATE_IDLE,
	E_STATE_WAIT_4_INIT_RSP,
	E_STATE_WAIT_4_NFCEE_DISCOVERY,
	E_STATE_READY,
	NCIX_MAIN_NUM_STATES
};

static enum
{
	E_EVT_START_C,
	E_EVT_STOP_C,
	E_EVT_RSP,
	E_EVT_NFCEE_N,
	E_EVT_RESET_N,
	NCIX_MAIN_NUM_EVTS
};


struct ncix_main
{
	nfc_handle_t h_sm;
	nfc_handle_t h_nci;
	nfc_handle_t h_ncix;
};

static void hndlr_rsp(nfc_handle_t h_main, nfc_u16 opcode, nci_rsp_param_u *pu_rsp);
static void hndlr_reset_rsp(nfc_handle_t h_main, nfc_u16 opcode, nci_rsp_param_u *pu_rsp);


/**************************************/
/*** Static Action helper utilities ***/
/**************************************/
/******************************************************************************
*
* Name: static_send_discover_map
*
* Description: Prepare discover_map command if operation completion status so far
*			  is OK and reset version is 1.0. Send it to command queue.
*
* Input: 	p_main - handle to operation main object
*		f_rsp - callback function to be executed when response is received for the command.
*
* Output: None
*
* Return: None
*
*******************************************************************************/
static void static_send_discover_map(struct ncix_main *p_main, rsp_cb_f f_rsp)
{
	nfc_u8 i=0;
	nfc_u8 j=0;
	nci_cmd_param_u nci_cmd;
	struct nci_op_rsp_start *p_start_rsp = &ncix_get_rsp_ptr(p_main->h_ncix)->start;
	nci_status_e status = p_start_rsp->status;

	if(status == NCI_STATUS_OK)
	{
		if(p_start_rsp->reset.version != NCI_VERSION_1_0)
			status = NCI_STATUS_REJECTED;
		else
		{
			for(j = 0; j < p_start_rsp->init.num_supported_rf_interfaces; j++)
			{
				/* Send NCI_OPCODE_RF_DISCOVER_MAP_CMD */
				if(p_start_rsp->init.supported_rf_interfaces[j] == NCI_RF_INTERFACE_FRAME)
				{
					nci_cmd.discover_map.disc_map_conf[i].rf_protocol = NCI_RF_PROTOCOL_T1T;
					nci_cmd.discover_map.disc_map_conf[i].mode = NCI_DISC_MAP_MODE_POLL;
					nci_cmd.discover_map.disc_map_conf[i++].rf_interface_type = NCI_RF_INTERFACE_FRAME;

					nci_cmd.discover_map.disc_map_conf[i].rf_protocol = NCI_RF_PROTOCOL_T2T;
					nci_cmd.discover_map.disc_map_conf[i].mode = NCI_DISC_MAP_MODE_POLL;
					nci_cmd.discover_map.disc_map_conf[i++].rf_interface_type = NCI_RF_INTERFACE_FRAME;

					nci_cmd.discover_map.disc_map_conf[i].rf_protocol = NCI_RF_PROTOCOL_T3T;
					nci_cmd.discover_map.disc_map_conf[i].mode = NCI_DISC_MAP_MODE_BOTH;
					nci_cmd.discover_map.disc_map_conf[i++].rf_interface_type = NCI_RF_INTERFACE_FRAME;
				}
				else
				{
					if(p_start_rsp->init.supported_rf_interfaces[j] == NCI_RF_INTERFACE_ISO_DEP)
					{
						nci_cmd.discover_map.disc_map_conf[i].rf_protocol = NCI_RF_PROTOCOL_ISO_DEP;
						nci_cmd.discover_map.disc_map_conf[i].mode = NCI_DISC_MAP_MODE_BOTH;
						nci_cmd.discover_map.disc_map_conf[i++].rf_interface_type = NCI_RF_INTERFACE_ISO_DEP;
					}
					else
					{
						if(p_start_rsp->init.supported_rf_interfaces[j] == NCI_RF_INTERFACE_NFC_DEP)
						{
							nci_cmd.discover_map.disc_map_conf[i].rf_protocol = NCI_RF_PROTOCOL_NFC_DEP;
							nci_cmd.discover_map.disc_map_conf[i].mode = NCI_DISC_MAP_MODE_BOTH;
							nci_cmd.discover_map.disc_map_conf[i++].rf_interface_type = NCI_RF_INTERFACE_NFC_DEP;
						}
						else
						{
							/* Signal that we do not support this interface type */
							OSA_ASSERT(0);
						}
					}
				}
			}

			nci_cmd.discover_map.num_of_conf = i;
			status = nci_send_cmd(p_main->h_nci, NCI_OPCODE_RF_DISCOVER_MAP_CMD, &nci_cmd, NULL, 0, f_rsp, p_main);
		}
	}

}

/******************************************************************************
*
* Name: static_send_conn_create_0
*
* Description: Prepare conn_create command if operation completion status so far
*			  is OK. Send it to command queue.
*
* Input: 	p_main - handle to operation main object
*		f_rsp - callback function to be executed when response is received for the command.
*
* Output: None
*
* Return: None
*
*******************************************************************************/
static void static_send_conn_create_0(struct ncix_main *p_main, rsp_cb_f f_rsp)
{
	nci_cmd_param_u nci_cmd;
	struct nci_op_rsp_start *p_start_rsp = &ncix_get_rsp_ptr(p_main->h_ncix)->start;
	nci_status_e status = p_start_rsp->status;


	if(status == NCI_STATUS_OK)
	{
		/* Send NCI_OPCODE_CORE_CONN_CREATE (Static connection 0) */
		nci_cmd.conn_create.target_handle = 0;
		nci_cmd.conn_create.num_of_tlvs = 0;
		nci_cmd.conn_create.p_tlv = NULL;
		nci_cmd.conn_create.total_length = 0;
		status = nci_send_cmd(p_main->h_nci, NCI_OPCODE_CORE_CONN_CREATE_CMD, &nci_cmd,
								(void*)&p_start_rsp->conn_create, sizeof(struct nci_rsp_conn_create), f_rsp, p_main);
	}
}

/******************************************************************************
*
* Name: static_send_nfcee_discover
*
* Description: Prepare nfcee_discover command if operation completion status so far
*			  is OK. Send it to command queue.
*			  If operation completion status is not OK than complete operation.
*
* Input: 	p_main - handle to operation main object
*		f_rsp - callback function to be executed when response is received for the command.
*
* Output: None
*
* Return: None
*
*******************************************************************************/
static void static_send_nfcee_discover(struct ncix_main *p_main, rsp_cb_f f_rsp)
{
	nci_cmd_param_u nci_cmd;
	struct nci_op_rsp_start *p_start_rsp = &ncix_get_rsp_ptr(p_main->h_ncix)->start;
	nci_status_e status = p_start_rsp->status;

	if(status == NCI_STATUS_OK)
	{
		/* Send NCI_OPCODE_NFCEE_DISCOVER_CMD */
		nci_cmd.nfcee_discover.action = NCI_NFCEE_DISCOVER_ACTION_ENABLE;
		status = nci_send_cmd(p_main->h_nci, NCI_OPCODE_NFCEE_DISCOVER_CMD, &nci_cmd,
								(void*)&p_start_rsp->nfcee_discover_rsp, sizeof(p_start_rsp->nfcee_discover_rsp), f_rsp, p_main);
	}
	if(NCI_STATUS_OK != status)
	{
		p_start_rsp->status = NCI_STATUS_FAILED;
		ncix_op_complete(p_main->h_ncix, NCI_STATUS_OK);
	}
}


/******************************************************************************
*
* Name: static_send_nci_reset
*
* Description: Prepare nci_reset command and send it to command queue.
*			  If send failed than assert.
*
* Input: 	p_main - handle to operation main object
*		f_rsp - callback function to be executed when response is received for the command.
*
* Output: None
*
* Return: None
*
*******************************************************************************/
void static_send_nci_reset(struct ncix_main *p_main, rsp_cb_f f_rsp)
{
	nci_status_e	status;
	struct nci_op_rsp_start *p_start_rsp = &ncix_get_rsp_ptr(p_main->h_ncix)->start;


	/* Send NCI_OPCODE_CORE_RESET_CMD */
	status = nci_send_cmd(p_main->h_nci, NCI_OPCODE_CORE_RESET_CMD, NULL,
						(void*)&p_start_rsp->reset, sizeof(struct nci_rsp_reset), f_rsp, p_main);

	if(NCI_STATUS_OK != status)
	{
		OSA_ASSERT(0);
	}
}

/******************************************************************************
*
* Name: static_send_nci_init
*
* Description: Prepare nci_init command and send it to command queue.
*			  If send failed than assert.
*
* Input: 	p_main - handle to operation main object
*		f_rsp - callback function to be executed when response is received for the command.
*
* Output: None
*
* Return: None
*
*******************************************************************************/
void static_send_nci_init(struct ncix_main *p_main, rsp_cb_f f_rsp)
{
	nci_cmd_param_u nci_cmd;
	nci_status_e	status;
	struct nci_op_rsp_start *p_start_rsp = &ncix_get_rsp_ptr(p_main->h_ncix)->start;

	/* Send NCI_OPCODE_CORE_INIT_CMD */
	status = nci_send_cmd(p_main->h_nci, NCI_OPCODE_CORE_INIT_CMD, &nci_cmd,
			(void*)&p_start_rsp->init, sizeof(struct nci_rsp_init), f_rsp, p_main);

	if(NCI_STATUS_OK != status)
	{
		OSA_ASSERT(0);
	}
}

/***************************************/
/*** SM Action Callbacks (per state) ***/
/***************************************/
/******************************************************************************
*
* Name: action_state_idle
*
* Description: Handle operation state Machine idle state's events
*
* Input: 	h_main - handle to operation main object
*		event - event to handle
*
* Output: None
*
* Return: next state in SM according to current state and event
*
*******************************************************************************/
nfc_u32 action_state_idle(nfc_handle_t h_main, nfc_u32 event)
{
	struct ncix_main *p_main = (struct ncix_main *)h_main;
	nfc_u32 next_state = E_STATE_IDLE;

	switch(event)
	{
		case E_EVT_START_C:
			static_send_nci_reset(h_main, NULL);
			static_send_nci_init(h_main, hndlr_rsp);
			next_state = E_STATE_WAIT_4_INIT_RSP;
		break;

		case E_EVT_STOP_C:
			ncix_op_complete(p_main->h_ncix, NCI_STATUS_OK);
			next_state = E_STATE_IDLE;
		break;

		default:
			OSA_ASSERT(0);
	}
	return next_state;
}

/******************************************************************************
*
* Name: action_state_wait_4_init_rsp
*
* Description: Handle operation state Machine wait_4_init_rsp state's events
*
* Input: 	h_main - handle to operation main object
*		event - event to handle
*
* Output: None
*
* Return: next state in SM according to current state and event
*
*******************************************************************************/
nfc_u32 action_state_wait_4_init_rsp(nfc_handle_t h_main, nfc_u32 event)
{
	struct ncix_main *p_main = (struct ncix_main *)h_main;
	nfc_u32 next_state = E_STATE_WAIT_4_INIT_RSP;
	struct nci_op_rsp_start *p_start_rsp = &ncix_get_rsp_ptr(p_main->h_ncix)->start;

	switch(event)
	{
		case E_EVT_RSP:
			static_send_discover_map(p_main, NULL);
#if 1
			static_send_conn_create_0(p_main, NULL);
			static_send_nfcee_discover(p_main, hndlr_rsp);
			next_state = E_STATE_WAIT_4_NFCEE_DISCOVERY;
#else // Should be used when working cable mode - FW bug in NFCEE discovery
			static_send_conn_create_0(p_main, hndlr_rsp);
			next_state = E_STATE_WAIT_4_NFCEE_DISCOVERY;
#endif
			p_start_rsp->nfcee_discover_ntf_number = 0;
		break;
		case E_EVT_RESET_N:
			next_state = E_STATE_IDLE;
		break;
		default:
			OSA_ASSERT(0);
	}
	return next_state;
}

/******************************************************************************
*
* Name: action_state_wait_4_nfcee_disc
*
* Description: Handle operation state Machine wait_4_nfcee_disc state's events
*
* Input: 	h_main - handle to operation main object
*		event - event to handle
*
* Output: None
*
* Return: next state in SM according to current state and event
*
*******************************************************************************/
nfc_u32 action_state_wait_4_nfcee_disc(nfc_handle_t h_main, nfc_u32 event)
{
	struct ncix_main *p_main = (struct ncix_main *)h_main;
	nfc_u32 next_state = E_STATE_WAIT_4_NFCEE_DISCOVERY;
	struct nci_op_rsp_start *p_start_rsp = &ncix_get_rsp_ptr(p_main->h_ncix)->start;

	switch(event)
	{
		case E_EVT_RSP:
			if(p_start_rsp->nfcee_discover_rsp.num_nfcee == 0)
			{
				ncix_op_complete(p_main->h_ncix, NCI_STATUS_OK);
				next_state = E_STATE_READY;
			}
		break;
		case E_EVT_NFCEE_N:
			if(p_start_rsp->nfcee_discover_ntf_number == p_start_rsp->nfcee_discover_rsp.num_nfcee)
			{
				ncix_op_complete(p_main->h_ncix, NCI_STATUS_OK);
				next_state = E_STATE_READY;
			}
		break;
		case E_EVT_RESET_N:
			next_state = E_STATE_IDLE;
		break;
		default:
			OSA_ASSERT(0);
	}
	return next_state;
}

/******************************************************************************
*
* Name: action_state_ready
*
* Description: Handle operation state Machine ready state's events
*
* Input: 	h_main - handle to operation main object
*		event - event to handle
*
* Output: None
*
* Return: next state in SM according to current state and event
*
*******************************************************************************/
nfc_u32 action_state_ready(nfc_handle_t h_main, nfc_u32 event)
{
	nfc_u32 next_state = E_STATE_READY;

	switch(event)
	{
		case E_EVT_STOP_C:
			static_send_nci_reset(h_main, hndlr_reset_rsp);
			next_state = E_STATE_IDLE;
		break;
		case E_EVT_RESET_N:
			next_state = E_STATE_IDLE;
		break;
		default:
			OSA_ASSERT(0);
	}
	return next_state;
}


static struct ncix_state s_state[] =
{
	{ action_state_idle,				"ST_IDLE" },
	{ action_state_wait_4_init_rsp,		"ST_WAIT_4_INIT_RSP" },
	{ action_state_wait_4_nfcee_disc,	"ST_WAIT_4_NFCEE_DISC" },
	{ action_state_ready,				"ST_READY" },
};

static struct ncix_event s_event[] =
{
	{ NULL,	"EVT_START_C" },
	{ NULL,	"EVT_STOP_C" },
	{ NULL,	"EVT_RSP" },
	{ NULL,	"EVT_NFCEE_N" },
	{ NULL,	"EVT_RESET_N" },
};

/***************************************/
/*** Module Input Routines           ***/
/***************************************/
/******************************************************************************
*
* Name: hndlr_op_start
*
* Description: Callback that is installed by operation main module creation. It will be invoked by
*			  operation callback in result of start_operation request.
*
* Input: 	h_main - handle to operation main object
*		pu_param - NCI operation structure
*
* Output: None
*
* Return: None
*
*******************************************************************************/
static void hndlr_op_start(nfc_handle_t h_main, op_param_u *pu_param)
{
	struct ncix_main *p_main = (struct ncix_main *)h_main;
	OSA_UNUSED_PARAMETER(pu_param);
	ncix_sm_process_evt(p_main->h_sm, E_EVT_START_C);
}

/******************************************************************************
*
* Name: hndlr_op_stop
*
* Description: Callback that is installed by operation main module creation. It will be invoked by
*			  operation callback in result of stop_operation request.
*
* Input: 	h_main - handle to operation main object
*		pu_param - NCI operation structure
*
* Output: None
*
* Return: None
*
*******************************************************************************/
static void hndlr_op_stop(nfc_handle_t h_main, op_param_u *pu_param)
{
	struct ncix_main *p_main = (struct ncix_main *)h_main;
	OSA_UNUSED_PARAMETER(pu_param);
	ncix_sm_process_evt(p_main->h_sm, E_EVT_STOP_C);
}

/******************************************************************************
*
* Name: hndlr_reset_ntf
*
* Description: Callback that is installed by operation main module creation. It will be invoked by
*			  operation notification callback in result of CORE_RESET request.
*
* Input: 	h_main - handle to operation main object
*		pu_param - NCI operation structure
*
* Output: None
*
* Return: None
*
*******************************************************************************/
static void hndlr_reset_ntf(nfc_handle_t h_main, nfc_u16 opcode, nci_ntf_param_u *pu_ntf)
{
	struct ncix_main *p_main = (struct ncix_main *)h_main;
	OSA_ASSERT(opcode == NCI_OPCODE_CORE_RESET_NTF);
	OSA_UNUSED_PARAMETER(pu_ntf);
	ncix_sm_process_evt(p_main->h_sm, E_EVT_RESET_N);
}

/******************************************************************************
*
* Name: hndlr_nfcee_ntf
*
* Description: Callback that is installed by operation main module creation. It will be invoked by
*			  operation notification callback in result of NFCEE_DISCOVER request.
*
* Input: 	h_main - handle to operation main object
*		opcode - NFCEE opcode
*		pu_ntf - NCI parameter structure
*
* Output: None
*
* Return: None
*
*******************************************************************************/
static void hndlr_nfcee_ntf(nfc_handle_t h_main, nfc_u16 opcode, nci_ntf_param_u *pu_ntf)
{
	struct ncix_main *p_main = (struct ncix_main *)h_main;
	struct nci_op_rsp_start *p_start_rsp = &ncix_get_rsp_ptr(p_main->h_ncix)->start;

	OSA_ASSERT(opcode == NCI_OPCODE_NFCEE_DISCOVER_NTF);
	if(p_start_rsp->nfcee_discover_ntf_number < NCI_MAX_NUM_NFCEE)
	{
		osa_mem_copy(&p_start_rsp->nfcee_discover_ntf[p_start_rsp->nfcee_discover_ntf_number++] , &pu_ntf->nfcee_discover, sizeof(pu_ntf->nfcee_discover));
	}
	ncix_sm_process_evt(p_main->h_sm, E_EVT_NFCEE_N);
}

/******************************************************************************
*
* Name: hndlr_rsp
*
* Description: Response callback that is called by NCI state machine when handling events.
*
* Input: 	h_main - handle to operation main object
*		opcode - NFCEE opcode
*		pu_ntf - NCI parameter structure
*
* Output: None
*
* Return: None
*
*******************************************************************************/
static void hndlr_rsp(nfc_handle_t h_main, nfc_u16 opcode, nci_rsp_param_u *pu_rsp)
{
	struct ncix_main *p_main = (struct ncix_main *)h_main;
	OSA_UNUSED_PARAMETER(opcode);
	ncix_get_rsp_ptr(p_main->h_ncix)->generic.status = pu_rsp->generic.status;
	ncix_sm_process_evt(p_main->h_sm, E_EVT_RSP);
}

/******************************************************************************
*
* Name: hndlr_reset_rsp
*
* Description: Response callback that is called by NCI state machine when handling events.
*
* Input: 	h_main - handle to operation main object
*		opcode - NFCEE opcode
*		pu_rsp - NCI parameter structure
*
* Output: None
*
* Return: None
*
*******************************************************************************/
static void hndlr_reset_rsp(nfc_handle_t h_main, nfc_u16 opcode, nci_rsp_param_u *pu_rsp)
{
	struct ncix_main *p_main = (struct ncix_main *)h_main;
	OSA_UNUSED_PARAMETER(pu_rsp);
	OSA_UNUSED_PARAMETER(opcode);
	ncix_sm_process_evt(p_main->h_sm, E_EVT_STOP_C);
}



/***************************************/
/*** Module Constructor/Destructor   ***/
/***************************************/
/******************************************************************************
*
* Name: ncix_main_create
*
* Description: Operation main module initialization.
*			  Allocate memory and initialize parameters.
*			  Register operation & notification callbacks.
*
* Input: 	h_ncix - handle to operation main object
*		h_nci - handle to NCI object
*
* Output: None
*
* Return: handle to created operation main object
*
*******************************************************************************/
nfc_handle_t ncix_main_create(nfc_handle_t h_ncix, nfc_handle_t h_nci)
{
	struct ncix_main *p_main = (struct ncix_main *)osa_mem_alloc(sizeof(struct ncix_main));
	OSA_ASSERT(p_main);

	p_main->h_nci = h_nci;
	p_main->h_ncix = h_ncix;
	p_main->h_sm = ncix_sm_create(p_main, "MAIN", E_STATE_IDLE, s_state, s_event);

	ncix_register_op_cb(h_ncix, NCI_OPERATION_START, hndlr_op_start, p_main);
	ncix_register_op_cb(h_ncix, NCI_OPERATION_STOP, hndlr_op_stop, p_main);

	nci_register_ntf_cb(h_nci, E_NCI_NTF_CORE_RESET, hndlr_reset_ntf, p_main);
	nci_register_ntf_cb(h_nci, E_NCI_NTF_NFCEE_DISCOVER, hndlr_nfcee_ntf, p_main);

	return p_main;
}

/******************************************************************************
*
* Name: ncix_main_destroy
*
* Description: Operation main module destruction. Frees object allocated memory.
*
* Input: 	h_main - handle to operation main object
*
* Output: None
*
* Return: None
*
*******************************************************************************/
void ncix_main_destroy(nfc_handle_t h_main)
{
	struct ncix_main *p_main = (struct ncix_main *)h_main;

	nci_unregister_ntf_cb(p_main->h_nci, E_NCI_NTF_CORE_RESET, hndlr_reset_ntf);
	nci_unregister_ntf_cb(p_main->h_nci, E_NCI_NTF_NFCEE_DISCOVER, hndlr_nfcee_ntf);

	ncix_sm_destroy(p_main->h_sm);
	osa_mem_free(h_main);
}

#endif //#if (NCI_VERSION_IN_USE==NCI_DRAFT_9)

/* EOF */





















