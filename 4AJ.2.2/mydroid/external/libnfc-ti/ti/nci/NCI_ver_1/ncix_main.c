/*
 * nci_ver_1\ncix_main.c
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

#if (NCI_VERSION_IN_USE==NCI_VER_1)

#include "nfc_types.h"
#include "nci_dev.h"
#include "nci_int.h"
#include "nci_data_int.h"
#include "nfc_os_adapt.h"
#include "ncix_int.h"
#include "nci_rf_sm.h"


enum
{
	E_STATE_IDLE,
	E_STATE_WAIT_4_INIT_RSP,
	E_STATE_WAIT_4_LOAD_PERSISTENCE_RSP,
	E_STATE_WAIT_4_NFCEE_DISCOVERY,
	E_STATE_READY,
	NCIX_MAIN_NUM_STATES
};

enum
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
	nfc_u8 hci_persist_buff[MAX_PROP_LEN];
};

static void hndlr_rsp(nfc_handle_t h_main, nfc_u16 opcode, nci_rsp_param_u *pu_rsp);
static void hndlr_reset_rsp(nfc_handle_t h_main, nfc_u16 opcode, nci_rsp_param_u *pu_rsp);
static void static_set_persistence(nfc_handle_t h_main);

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
							// Signal that we do not support this interface type
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
	nci_cmd_param_u reset_cmd;

	reset_cmd.core_reset.keep_config = NCI_CORE_RESET_CMD_PARAM_RESET_CONFIG;

	/* Send NCI_OPCODE_CORE_RESET_CMD */
	status = nci_send_cmd(p_main->h_nci, NCI_OPCODE_CORE_RESET_CMD, &reset_cmd,
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

/*** VS NFC SET MODE CMD             ***/
/***************************************/
#define NCI_VS_NFC_SET_MODE_CMD		(__OPCODE(NCI_GID_PROPRIETARY,0x11))
#define NCI_VS_NFC_SET_MODE_RSP		(NCI_VS_NFC_SET_MODE_CMD)

/******************************************************************************
*
* Name: static_send_vs_nfc_set_mode_off
*
* Description: Prepare vs nfc set mode command and send it to command queue.
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
void static_send_vs_nfc_set_mode_off(struct ncix_main *p_main, vendor_specific_cb_f f_rsp)
{
	nci_status_e status;
	nfc_u8 mode = 0x00; /* OFF */

	status = nci_send_vendor_specific_cmd(
		p_main->h_nci,
		NCI_VS_NFC_SET_MODE_CMD, &mode, 1,
		f_rsp, p_main);

	if(NCI_STATUS_OK != status)
	{
		OSA_ASSERT(0);
	}
}

/***************************************/
/*** VS NFC PLATFORM OFF CMD         ***/
/***************************************/
#define NCI_VS_NFC_PLATFORM_OFF_CMD		(__OPCODE(NCI_GID_PROPRIETARY,0x0b))
#define NCI_VS_NFC_PLATFORM_OFF_RSP		(NCI_VS_NFC_PLATFORM_OFF_CMD)

/******************************************************************************
*
* Name: static_send_vs_nfc_platform_off
*
* Description: Prepare vs nfc set mode command and send it to command queue.
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
void static_send_vs_nfc_platform_off(struct ncix_main *p_main, vendor_specific_cb_f f_rsp)
{
	nci_status_e status;

	status = nci_send_vendor_specific_cmd(
		p_main->h_nci,
		NCI_VS_NFC_PLATFORM_OFF_CMD, NULL, 0,
		f_rsp, p_main);

	if(NCI_STATUS_OK != status)
	{
		OSA_ASSERT(0);
	}
}

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
			nci_rf_sm_stop(p_main->h_nci);
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
	//struct nci_op_rsp_start *p_start_rsp = &ncix_get_rsp_ptr(p_main->h_ncix)->start;

	switch(event)
	{
		case E_EVT_RSP:
			//From current draft there is no need to send create connection 0 (RF)
			//We just open the data connection
#if 1
			nci_data_open_conn(((struct nci_context*)(p_main->h_nci))->h_data, 0, 0, 0xff/*Actual values for 0 and ff will be set when activate ntf received*/);
			static_send_discover_map(p_main, NULL);
			static_set_persistence(h_main);
			next_state = E_STATE_WAIT_4_LOAD_PERSISTENCE_RSP;
#else // Should be used when working cable mode - FW bug in NFCEE discovery
			nci_data_open_conn(((struct nci_context*)(p_main->h_nci))->h_data, 0, p_start_rsp->init.initial_num_of_credits_rf_conn, p_start_rsp->init.max_data_packet_payload_size);
			static_send_discover_map(p_main, hndlr_rsp);
			next_state = E_STATE_WAIT_4_NFCEE_DISCOVERY;
#endif
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
* Name: action_state_wait_4_persistence_load_config_rsp
*
* Description: Handle operation state Machine wait_4_load_config_rsp state's events
*
* Input: 	h_main - handle to operation main object
*		event - event to handle
*
* Output: None
*
* Return: next state in SM according to current state and event
*
*******************************************************************************/
nfc_u32 action_state_wait_4_persistence_load_config_rsp(nfc_handle_t h_main, nfc_u32 event)
{
	struct ncix_main *p_main = (struct ncix_main *)h_main;
	nfc_u32 next_state = E_STATE_WAIT_4_LOAD_PERSISTENCE_RSP;
	struct nci_op_rsp_start *p_start_rsp = &ncix_get_rsp_ptr(p_main->h_ncix)->start;

	switch(event)
	{
		case E_EVT_RSP:

			/* Continue with the start state machine */
			static_send_nfcee_discover(p_main, hndlr_rsp);
			next_state = E_STATE_WAIT_4_NFCEE_DISCOVERY;
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
				nci_rf_sm_start(p_main->h_nci);
				ncix_op_complete(p_main->h_ncix, NCI_STATUS_OK);
				next_state = E_STATE_READY;
			}
		break;
		case E_EVT_NFCEE_N:
			if(p_start_rsp->nfcee_discover_ntf_number == p_start_rsp->nfcee_discover_rsp.num_nfcee)
			{
				nci_rf_sm_start(p_main->h_nci);
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
	struct ncix_main *p_main = (struct ncix_main *)h_main;
	struct nci_op *p_op = ncix_get_op_ptr(p_main->h_ncix);
	nfc_u32 next_state = E_STATE_READY;

	switch(event)
	{
		case E_EVT_STOP_C:
			/* FINDME: Temporary solution until FW fix bug - currently NCI reset is done only after
			   NCI init */
			static_send_nci_reset(h_main, NULL);
			static_send_nci_init(h_main, hndlr_reset_rsp);
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


static struct nci_sm_state s_state[] =
{
	{ action_state_idle,				(nfc_s8*) "ST_IDLE" },
	{ action_state_wait_4_init_rsp,	 	(nfc_s8*) "ST_WAIT_4_INIT_RSP" },
	{ action_state_wait_4_persistence_load_config_rsp,	(nfc_s8*) "ST_WAIT_4_LOAD_PERSISTENCE_RSP"},
	{ action_state_wait_4_nfcee_disc,	(nfc_s8*) "ST_WAIT_4_NFCEE_DISC" },
	{ action_state_ready,				(nfc_s8*) "ST_READY" },
};

static struct nci_sm_event s_event[] =
{
	{ NULL,	(nfc_s8*) "EVT_START_C" },
	{ NULL,	(nfc_s8*) "EVT_STOP_C" },
	{ NULL,	(nfc_s8*) "EVT_RSP" },
	{ NULL,	(nfc_s8*) "EVT_NFCEE_N" },
	{ NULL,	(nfc_s8*) "EVT_RESET_N" },
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
	nci_sm_process_evt(p_main->h_sm, E_EVT_START_C);
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
	nci_sm_process_evt(p_main->h_sm, E_EVT_STOP_C);
}

/******************************************************************************
*
* Name: hndlr_op_script
*
* Description: Callback that is installed by operation main module creation. It will be invoked by
*			  operation callback in result of script_operation request.
*
* Input: 	h_main - handle to operation main object
*		pu_param - NCI operation structure
*
* Output: None
*
* Return: None
*
*******************************************************************************/
static void script_complete_cb(nfc_handle_t usr_param, nci_status_e status)
{
	struct ncix_main *p_main = (struct ncix_main *)usr_param;
	ncix_op_complete(p_main->h_ncix, status);
}

static void hndlr_op_script(nfc_handle_t h_main, op_param_u *pu_param)
{
	struct ncix_main *p_main = (struct ncix_main *)h_main;
	nci_send_script(p_main->h_nci, pu_param->script.filename, script_complete_cb, h_main);
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
	nci_sm_process_evt(p_main->h_sm, E_EVT_RESET_N);
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
	nci_sm_process_evt(p_main->h_sm, E_EVT_NFCEE_N);
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

	if(NCI_OPCODE_CORE_INIT_RSP == opcode)
	{
		nci_rf_sm_set_nfcc_features(p_main->h_nci, pu_rsp->init.nfcc_features.bit_mask);
	}

	ncix_get_rsp_ptr(p_main->h_ncix)->generic.status = pu_rsp->generic.status;
	nci_sm_process_evt(p_main->h_sm, E_EVT_RSP);
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
	nci_sm_process_evt(p_main->h_sm, E_EVT_STOP_C);
}


/***************************************/
/*** HCI persistence routines       ***/
/***************************************/
/* HCI persistence VS commands */
#define NCI_VS_HCI_PERSISTENT_SET_MODE_CMD (__OPCODE(NCI_GID_PROPRIETARY,0x30))
#define NCI_VS_HCI_PERSISTENT_SET_MODE_RSP (NCI_VS_HCI_PERSISTENT_SET_MODE_CMD)

#define NCI_VS_PERSISTENT_STORE_CONFIG_CMD (__OPCODE(NCI_GID_PROPRIETARY,0x31))
#define NCI_VS_PERSISTENT_STORE_CONFIG_RSP (NCI_VS_PERSISTENT_STORE_CONFIG_CMD)
#define NCI_VS_PERSISTENT_STORE_CONFIG_NTF (NCI_VS_PERSISTENT_STORE_CONFIG_CMD)

#define NCI_VS_PERSISTENT_LOAD_CONFIG_CMD  (__OPCODE(NCI_GID_PROPRIETARY,0x32))
#define NCI_VS_PERSISTENT_LOAD_CONFIG_RSP  (NCI_VS_PERSISTENT_LOAD_CONFIG_CMD)

#define HCI_PERSISTENCE_PROP    ((const nfc_s8*)"ti.nfc.hci.persistence")
#define HCI_PERSISTENCE_SUCCESS (0x00)

#define HCI_PERSISTENCE_ENABLE  (0x01)
#define HCI_PERSISTENCE_DISABLE (0x01)

#define HCI_PERSISTENCE_STORE_SUCCESS (0x00)
#define HCI_PERSISTENCE_STORE_FAILED  (0x01)



static void hndlr_load_config_rsp(nfc_handle_t usr_param, nfc_u16 opcode, nfc_u8* p_rsp_buff, nfc_u8 rsp_buff_len)
{
	struct ncix_main *p_main = (struct ncix_main *)usr_param;

	OSA_ASSERT(opcode == NCI_VS_PERSISTENT_LOAD_CONFIG_RSP);
	OSA_ASSERT(rsp_buff_len == 3);

	if (p_rsp_buff[0] == HCI_PERSISTENCE_SUCCESS)
	{
		osa_report(INFORMATION, ("ncix_main: Loading persistent configuration to NFCC successful"));
	}
	else
	{
		nfc_u16 configLen; osa_mem_copy(&configLen, p_rsp_buff+1, 2);

		osa_report(ERROR, ("ncix_main: Loading persistent configuration to NFCC failed, status=0x%X, configsize=%d",
			(unsigned) p_rsp_buff[0],
			(int) configLen));
	}


	ncix_get_rsp_ptr(p_main->h_ncix)->generic.status = NFC_RES_OK;
	nci_sm_process_evt(p_main->h_sm, E_EVT_RSP);

}

static void hndlr_set_persistent_rsp(nfc_handle_t usr_param, nfc_u16 opcode, nfc_u8* p_rsp_buff, nfc_u8 rsp_buff_len)
{
	struct ncix_main *p_main = (struct ncix_main *)usr_param;

	OSA_ASSERT(opcode == NCI_VS_HCI_PERSISTENT_SET_MODE_RSP);
	OSA_ASSERT(rsp_buff_len == 1);

	if(p_rsp_buff[0] == HCI_PERSISTENCE_SUCCESS)
	{
		nfc_u16 prop_len=0xFFFF;
		nfc_status ret_code;
		nci_status_e status;

		osa_report(INFORMATION, ("ncix_main: Enable HCI Persistent mode success"));

		ret_code = osa_getprop(HCI_PERSISTENCE_PROP, p_main->hci_persist_buff, &prop_len);
		if (ret_code == NFC_RES_OK)
		{
			osa_report(INFORMATION, ("ncix_main: Loading persistent configuration to NFCC(len=%d)",
				(int) prop_len));

			status = nci_send_vendor_specific_cmd(
				p_main->h_nci,
				NCI_VS_PERSISTENT_LOAD_CONFIG_CMD, p_main->hci_persist_buff, (nfc_u8) prop_len,
				hndlr_load_config_rsp, p_main);

			OSA_ASSERT(status == NCI_STATUS_OK);
		}
		else
		{
			osa_report(ERROR, ("ncix_main: Get HCI Persistent property failed, response=0x%X",
				(unsigned) ret_code));

			/* Continue to the next state in the start state machine */
			ncix_get_rsp_ptr(p_main->h_ncix)->generic.status = NFC_RES_OK;
			nci_sm_process_evt(p_main->h_sm, E_EVT_RSP);
		}
	}
	else
	{
		osa_report(ERROR, ("ncix_main: Enable HCI Persistent mode failed, response=0x%X",
			(unsigned) p_rsp_buff[0]));

		/* Continue to the next state in the start state machine */
		ncix_get_rsp_ptr(p_main->h_ncix)->generic.status = NFC_RES_OK;
		nci_sm_process_evt(p_main->h_sm, E_EVT_RSP);
	}
}

static void static_set_persistence(nfc_handle_t h_main)
{
	struct ncix_main *p_main = (struct ncix_main *)h_main;
	nci_status_e status = NCI_STATUS_OK;
	nfc_u8 enable_cmd=0x01;
	struct nci_op_rsp_start *p_start_rsp = &ncix_get_rsp_ptr(p_main->h_ncix)->start;
	nci_status_e init_status = p_start_rsp->status;


	if (init_status == NCI_STATUS_OK)
	{
		osa_report(INFORMATION, ("ncix_main: static_set_persistence - enable HCI persistent mode"));

		status = nci_send_vendor_specific_cmd(	p_main->h_nci,
			NCI_VS_HCI_PERSISTENT_SET_MODE_CMD, (nfc_u8*) &enable_cmd, (nfc_u8) sizeof(enable_cmd),
			hndlr_set_persistent_rsp, h_main);
		OSA_ASSERT(status == NCI_STATUS_OK);
	}
	else
	{
		p_start_rsp->status = NCI_STATUS_FAILED;
		ncix_op_complete(p_main->h_ncix, NCI_STATUS_OK);
	}

}

static void hndlr_persistent_ntf(nfc_handle_t usr_param, nfc_u16 opcode, nfc_u8* p_rsp_buff, nfc_u8 rsp_buff_len)
{
	nfc_handle_t h_nci = usr_param;
	nfc_status ret_code;
	nci_status_e status;

	nfc_u8 store_stat;
	nfc_u16 stored_len;

	nfc_u8 rsp_cmd_buff[sizeof(store_stat)+sizeof(stored_len)];

	osa_report(INFORMATION, ("ncix_main: hndlr_persistent_ntf - received HCI persistent mode update"));

	OSA_ASSERT(opcode == NCI_VS_PERSISTENT_STORE_CONFIG_NTF);

	/* Store new configuration */
	ret_code = osa_setprop(
		HCI_PERSISTENCE_PROP,
		p_rsp_buff,
		(nfc_u16) rsp_buff_len);

	if (ret_code == NFC_RES_OK)
	{
		osa_report(INFORMATION, ("ncix_main: hndlr_persistent_ntf - update success"));
		store_stat = HCI_PERSISTENCE_STORE_SUCCESS;
		stored_len = rsp_buff_len;
	}
	else
	{
		osa_report(ERROR, ("ncix_main: hndlr_persistent_ntf - update failed, stat=0x%X",
			(unsigned) ret_code));

		store_stat = HCI_PERSISTENCE_STORE_FAILED;
		stored_len = 0;
	}

	rsp_cmd_buff[0] = store_stat;
	rsp_cmd_buff[1] = (nfc_u8) (stored_len & 0xff);
	rsp_cmd_buff[2] = (nfc_u8) (stored_len >> 8);

	/* return result to firmware */
	status = nci_send_vendor_specific_cmd(
		h_nci,
		NCI_VS_PERSISTENT_STORE_CONFIG_CMD, (nfc_u8*) &rsp_cmd_buff, (nfc_u8) sizeof(rsp_cmd_buff),
		NULL, NULL);

	OSA_ASSERT(status == NCI_STATUS_OK);
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
	p_main->h_sm = nci_sm_create(p_main, (nfc_s8*) "MAIN", E_STATE_IDLE, s_state, s_event);

	ncix_register_op_cb(h_ncix, NCI_OPERATION_START, hndlr_op_start, p_main);
	ncix_register_op_cb(h_ncix, NCI_OPERATION_STOP, hndlr_op_stop, p_main);
	ncix_register_op_cb(h_ncix, NCI_OPERATION_SCRIPT, hndlr_op_script, p_main);

	nci_register_ntf_cb(h_nci, E_NCI_NTF_CORE_RESET, hndlr_reset_ntf, p_main);
	nci_register_ntf_cb(h_nci, E_NCI_NTF_NFCEE_DISCOVER, hndlr_nfcee_ntf, p_main);

	/* HCI persistent notification - get updated config */
	nci_register_vendor_specific_ntf_cb(h_nci,
	    NCI_VS_PERSISTENT_STORE_CONFIG_NTF, hndlr_persistent_ntf, h_nci);

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

	nci_unregister_vendor_specific_ntf_cb(
		p_main->h_nci, NCI_VS_PERSISTENT_STORE_CONFIG_NTF, hndlr_persistent_ntf);

	nci_unregister_ntf_cb(p_main->h_nci, E_NCI_NTF_CORE_RESET, hndlr_reset_ntf);
	nci_unregister_ntf_cb(p_main->h_nci, E_NCI_NTF_NFCEE_DISCOVER, hndlr_nfcee_ntf);

	nci_sm_destroy(p_main->h_sm);
	osa_mem_free(h_main);
}

#endif //#if (NCI_VERSION_IN_USE==NCI_VER_1)

/* EOF */





















