/*
 * ncix_rf_mgmt.c
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

  This source file contains the RF Management State Machine Implementation

*******************************************************************************/
#include "nci_api.h"

#if (NCI_VERSION_IN_USE==NCI_DRAFT_9)

#include "nfc_os_adapt.h"
#include "ncix_int.h"
#include "nci_int.h"


/*TBD - used as a patch, since our FW doesn't support more than 1 reader at a time, so for now use only A in P2P*/
char bUseP2PFTech = 0;

#define NAL_PROTOCOL_POLL_A		(NAL_PROTOCOL_READER_TYPE_1_CHIP|			\
								 NAL_PROTOCOL_READER_ISO_14443_3_A|		\
								 NAL_PROTOCOL_READER_ISO_14443_4_A|		\
								 NAL_PROTOCOL_READER_P2P_INITIATOR)
#define NAL_PROTOCOL_POLL_B		(NAL_PROTOCOL_READER_ISO_14443_3_B|		\
								 NAL_PROTOCOL_READER_ISO_14443_4_B)

#if 0 /* Our FW doesn't support more than 1 reader at a time, so for now use only A in P2P */
#define NAL_PROTOCOL_POLL_F     (NAL_PROTOCOL_READER_FELICA|           \
					             NAL_PROTOCOL_READER_P2P_INITIATOR)
#else
#define NAL_PROTOCOL_POLL_F     (NAL_PROTOCOL_READER_FELICA)
#endif

#define NAL_PROTOCOL_POLL_15    (NAL_PROTOCOL_READER_ISO_15693_2|      \
					             NAL_PROTOCOL_READER_ISO_15693_3)

#define NAL_PROTOCOL_LISTEN_A   (NAL_PROTOCOL_CARD_ISO_14443_4_A|      \
					             NAL_PROTOCOL_CARD_P2P_TARGET)

#define NAL_PROTOCOL_LISTEN_B   (NAL_PROTOCOL_CARD_ISO_14443_4_B)

#if 0 /* Our FW doesn't support more than 1 card at a time, so for now use only A in P2P */
#define NAL_PROTOCOL_LISTEN_F   (NAL_PROTOCOL_CARD_P2P_TARGET)
#else
#define NAL_PROTOCOL_LISTEN_F   (0)
#endif

#define NCI_CFG_PROTOCOL_TYPE_SIZE	1

///////////////////////////////////////////////////////////////////////////

static enum
{
	E_STATE_IDLE,
	E_STATE_ACTIVATING,
	E_STATE_ACTIVE,
	E_STATE_DE_ACTIVATING,
	E_STATE_WAIT_4_CORE_SET_CONFIG_RSP,
	NCI_RF_MGMT_NUM_STATES
};

static enum
{
	E_EVT_ENABLE_C,
	E_EVT_DISABLE_C,
	E_EVT_DISCOVER_R,
	E_EVT_DEACTIVATE_N,
	E_EVT_CORE_SET_CONFIG_RSP_OK,
	E_EVT_CORE_SET_CONFIG_RSP_FAIL,
	NCI_RF_MGMT_NUM_EVTS
};


struct ncix_rf_mgmt
{
	nfc_handle_t	h_sm;
	nfc_handle_t	h_nci;
	nfc_handle_t	h_ncix;
	nfc_u16		card_proto_mask;
	nfc_u16		reader_proto_mask;
};


//Internal nci ex functions
static void hndlr_discover_rsp(nfc_handle_t h_rf_mgmt, nfc_u16 opcode, void *p_nci_rsp);
static void hndlr_core_set_config_rsp(nfc_handle_t h_rf_mgmt, nfc_u16 opcode, nci_rsp_param_u *p_nci_rsp);

/******************************************************************************
*
* Name: static_send_core_set_config
*
* Description: Send CORE_SET_CONFIG command - according to card protocol type
*			  configure TLV structure and send it.
*			  NOTE: this function assumes that p_rf_mgmt->card_proto_mask != 0. This
*			  verfication should be done by calling task.
*
* Input: 	p_rf_mgmt - handle to RF Management object
*
* Output: None
*
* Return: Success -NFC_RES_OK
*		  Failure -NFC_RES_ERROR
*
*******************************************************************************/
static nfc_status static_send_core_set_config(struct ncix_rf_mgmt *p_rf_mgmt)
{
	nci_status_e status = NCI_STATUS_OK;
	nfc_u8 la_proto = 0;
	nfc_u8 lb_proto = 0;
	nfc_u8 lf_proto = 0;
	struct nci_tlv tlv[NCI_MAX_TLV_IN_CMD];
	nfc_u8 index = 0;
	struct nci_cmd_core_set_config core_set_config;

	/* set la_protocol  - configure as card */
	if(p_rf_mgmt->card_proto_mask & NAL_PROTOCOL_CARD_P2P_TARGET)
	{
		if(bUseP2PFTech == 0)
			la_proto |= NCI_CFG_TYPE_LA_PROTOCOL_TYPE_NFC_DEP;
		else
			lf_proto |= NCI_CFG_TYPE_LF_PROTOCOL_TYPE_NFC_DEP;
	}

	if(p_rf_mgmt->card_proto_mask & NAL_PROTOCOL_CARD_ISO_14443_4_A)
	{
		la_proto |= NCI_CFG_TYPE_LA_PROTOCOL_TYPE_ISO_DEP;
	}

	if(p_rf_mgmt->card_proto_mask & NAL_PROTOCOL_CARD_ISO_14443_4_B)
	{
		lb_proto |= NCI_CFG_TYPE_LB_PROTOCOL_TYPE_ISO_DEP;
	}

	tlv[index].type = NCI_CFG_TYPE_LA_PROTOCOL_TYPE;
	tlv[index].length = NCI_CFG_PROTOCOL_TYPE_SIZE;
	tlv[index++].p_value = &la_proto;

	tlv[index].type = NCI_CFG_TYPE_LB_PROTOCOL_TYPE;
	tlv[index].length = NCI_CFG_PROTOCOL_TYPE_SIZE;
	tlv[index++].p_value = &lb_proto;

	tlv[index].type = NCI_CFG_TYPE_LF_PROTOCOL_TYPE1;
	tlv[index].length = NCI_CFG_PROTOCOL_TYPE_SIZE;
	tlv[index++].p_value = &lf_proto;

	core_set_config.num_of_tlvs = index;
	OSA_ASSERT(core_set_config.num_of_tlvs > 0);
	core_set_config.p_tlv = tlv;

	status = nci_send_cmd(p_rf_mgmt->h_nci, NCI_OPCODE_CORE_SET_CONFIG_CMD,
				(nci_cmd_param_u *)&core_set_config, NULL, 0, hndlr_core_set_config_rsp, p_rf_mgmt);

	return (status == NCI_STATUS_OK) ? NFC_RES_OK:NFC_RES_ERROR;
}

/******************************************************************************
*
* Name: static_send_discover
*
* Description: Send send_discover command -according to protocol type configure
*			  command structure and send it.
*
* Input: 	p_rf_mgmt - handle to RF Management object
*
* Output: None
*
* Return: Success -NFC_RES_OK
*		  Failure -NFC_RES_ERROR
*
*******************************************************************************/
static nfc_status static_send_discover(struct ncix_rf_mgmt *p_rf_mgmt)
{
	nci_status_e status;
	nci_cmd_param_u	cmd;

	/* set discovery */
	cmd.discover.num_of_conf = 0;

	if(p_rf_mgmt->reader_proto_mask & NAL_PROTOCOL_POLL_A)
	{
		if(bUseP2PFTech == 0)
		{
			cmd.discover.disc_conf[cmd.discover.num_of_conf].type = NCI_DISCOVERY_TYPE_POLL_A_PASSIVE;
			cmd.discover.disc_conf[cmd.discover.num_of_conf].frequency = E_NCI_DISCOVERY_FREQUENCY_EVERY_1_DISCOVERY_PERIOD;
			cmd.discover.num_of_conf++;
		}
	}
	if(p_rf_mgmt->reader_proto_mask & NAL_PROTOCOL_POLL_B)
	{
		cmd.discover.disc_conf[cmd.discover.num_of_conf].type = NCI_DISCOVERY_TYPE_POLL_B_PASSIVE;
		cmd.discover.disc_conf[cmd.discover.num_of_conf].frequency = E_NCI_DISCOVERY_FREQUENCY_EVERY_1_DISCOVERY_PERIOD;
		cmd.discover.num_of_conf++;
	}
	if(p_rf_mgmt->reader_proto_mask & NAL_PROTOCOL_POLL_F)
	{
		if(bUseP2PFTech != 0)
		{
			cmd.discover.disc_conf[cmd.discover.num_of_conf].type = NCI_DISCOVERY_TYPE_POLL_F_PASSIVE;
			cmd.discover.disc_conf[cmd.discover.num_of_conf].frequency = E_NCI_DISCOVERY_FREQUENCY_EVERY_1_DISCOVERY_PERIOD;
			cmd.discover.num_of_conf++;
		}
	}
	if(p_rf_mgmt->reader_proto_mask & NAL_PROTOCOL_POLL_15)
	{
		cmd.discover.disc_conf[cmd.discover.num_of_conf].type = NCI_DISCOVERY_TYPE_POLL_15693;
		cmd.discover.disc_conf[cmd.discover.num_of_conf].frequency = E_NCI_DISCOVERY_FREQUENCY_EVERY_1_DISCOVERY_PERIOD;
		cmd.discover.num_of_conf++;
	}
	if(p_rf_mgmt->card_proto_mask & NAL_PROTOCOL_LISTEN_A)
	{
		if(bUseP2PFTech == 0)
		{
			cmd.discover.disc_conf[cmd.discover.num_of_conf].type = NCI_DISCOVERY_TYPE_LISTEN_A_PASSIVE;
			cmd.discover.disc_conf[cmd.discover.num_of_conf].frequency = E_NCI_DISCOVERY_FREQUENCY_EVERY_1_DISCOVERY_PERIOD;
			cmd.discover.num_of_conf++;
		}
	}
	if(p_rf_mgmt->card_proto_mask & NAL_PROTOCOL_LISTEN_B)
	{
		//nfc_u8 bail_out = 1;

		/*tlv[2].type = NCI_CFG_TYPE_PB_H_INFO;
		tlv[2].length = 1;
		tlv[2].p_value = &la_proto;

		bail_out = 1;
		tlv[i].type = NCI_CFG_TYPE_PB_BAIL_OUT;
		tlv[i].length = 1;
		tlv[i++].p_value = &bail_out;

		nci_set_param(p_rf_mgmt->h_nci, tlv, 3, NULL, NULL);*/

		cmd.discover.disc_conf[cmd.discover.num_of_conf].type = NCI_DISCOVERY_TYPE_LISTEN_B_PASSIVE;
		cmd.discover.disc_conf[cmd.discover.num_of_conf].frequency = E_NCI_DISCOVERY_FREQUENCY_EVERY_1_DISCOVERY_PERIOD;
		cmd.discover.num_of_conf++;
	}
	if(p_rf_mgmt->card_proto_mask & NAL_PROTOCOL_LISTEN_F)
	{
		if(bUseP2PFTech != 0)
		{
			cmd.discover.disc_conf[cmd.discover.num_of_conf].type = NCI_DISCOVERY_TYPE_LISTEN_F_PASSIVE;
			cmd.discover.disc_conf[cmd.discover.num_of_conf].frequency = E_NCI_DISCOVERY_FREQUENCY_EVERY_1_DISCOVERY_PERIOD;
			cmd.discover.num_of_conf++;
		}
	}

	status = nci_send_cmd(p_rf_mgmt->h_nci, NCI_OPCODE_RF_DISCOVER_CMD, &cmd, NULL, 0, hndlr_discover_rsp, p_rf_mgmt);
	return (status == NCI_STATUS_OK) ? NFC_RES_OK:NFC_RES_ERROR;
}


//Called by nci core when NCI_OPCODE_RF_IF_DEACTIVATE_NTF event occur
/******************************************************************************
*
* Name: action_evt_deactivate_n
*
* Description: Called by NCI core when event EVT_DEACTIVATE_N occurs.
*
* Input: 	h_rf_mgmt - handle to RF Management object
*		curr_state - event to handle
*
* Output: None
*
* Return: Next state in SM according to current state and event
*
*******************************************************************************/
nfc_u32 action_evt_deactivate_n(nfc_handle_t h_rf_mgmt, nfc_u32 curr_state)
{
	struct ncix_rf_mgmt *p_rf_mgmt = (struct ncix_rf_mgmt *)h_rf_mgmt;
	nci_status_e status = NCI_STATUS_OK;

	nfc_u32 next_state = curr_state;

	switch(curr_state)
	{
		case E_STATE_ACTIVE:
		{
			ncix_disable_operation_taskq(p_rf_mgmt->h_ncix);
			break;
		}
		case E_STATE_DE_ACTIVATING:
		{
			//Do nothing
			break;
		}
		default:
		{
			/* De-activate notification cannot be received in IDLE or ACTIVATING states */
			OSA_ASSERT(0);
		}
	}

	if(0 == (p_rf_mgmt->card_proto_mask | p_rf_mgmt->reader_proto_mask))
	{
		ncix_op_complete(p_rf_mgmt->h_ncix, NCI_STATUS_OK);
		next_state = E_STATE_IDLE;
	}
	else
	{
		if(p_rf_mgmt->card_proto_mask != 0)
		{
			status = static_send_core_set_config(p_rf_mgmt);
			next_state = E_STATE_WAIT_4_CORE_SET_CONFIG_RSP;
		}
		else
		{
			status = static_send_discover(p_rf_mgmt);
			next_state = E_STATE_ACTIVATING;
		}
	}
	return next_state;
}


/******************************************************************************
*
* Name: action_evt_enable_c
*
* Description: Called by NCI core when event EVT_ENABLE_C occurs.
*
* Input: 	h_rf_mgmt - handle to RF Management object
*		curr_state - event to handle
*
* Output: None
*
* Return: Next state in SM according to current state and event
*
*******************************************************************************/
nfc_u32 action_evt_enable_c(nfc_handle_t h_rf_mgmt, nfc_u32 curr_state)
{
	struct ncix_rf_mgmt *p_rf_mgmt = (struct ncix_rf_mgmt *)h_rf_mgmt;
	nci_status_e status = NCI_STATUS_OK;
	nci_cmd_param_u	cmd;
	nfc_u32 next_state = curr_state;

	switch(curr_state)
	{
		case E_STATE_IDLE:
		{
			//sanity check - we are not supposed to get here if c or r = 0
			if((p_rf_mgmt->card_proto_mask != 0) || (p_rf_mgmt->reader_proto_mask != 0))
			{
				if(p_rf_mgmt->card_proto_mask != 0)
				{
					status = static_send_core_set_config(p_rf_mgmt);
					next_state = E_STATE_WAIT_4_CORE_SET_CONFIG_RSP;
				}
				else
				{
					status = static_send_discover(p_rf_mgmt);
					next_state = E_STATE_ACTIVATING;
				}
			}
			else
			{
				OSA_ASSERT(0);
			}

			break;
		}
		case E_STATE_ACTIVE:
		{
			cmd.deactivate.type = NCI_DEACTIVATE_TYPE_IDLE_MODE;
			status = nci_send_cmd(p_rf_mgmt->h_nci, NCI_OPCODE_RF_DEACTIVATE_CMD, &cmd, NULL, 0, NULL, NULL);
			next_state = E_STATE_DE_ACTIVATING;
			break;
		}

		default:
		{
			OSA_ASSERT(0);
		}
	}

	return 	next_state;
}


/******************************************************************************
*
* Name: action_evt_discover_r
*
* Description: Called by NCI core when event EVT_DISCOVER_R occurs.
*
* Input: 	h_rf_mgmt - handle to RF Management object
*		curr_state - event to handle
*
* Output: None
*
* Return: Next state in SM according to current state and event
*
*******************************************************************************/
nfc_u32 action_evt_discover_r(nfc_handle_t h_rf_mgmt, nfc_u32 curr_state)
{
	struct ncix_rf_mgmt *p_rf_mgmt = (struct ncix_rf_mgmt *)h_rf_mgmt;
	nfc_u32 next_state = curr_state;

	switch(curr_state)
	{
		case E_STATE_ACTIVATING:
		{
			next_state = E_STATE_ACTIVE;
			ncix_op_complete(p_rf_mgmt->h_ncix, NCI_STATUS_OK);
			break;
		}
		default:
		{
			OSA_ASSERT(0);
		}
	}
	return 	next_state;

}

/******************************************************************************
*
* Name: action_evt_disable_c
*
* Description: Called by NCI core when event EVT_DISABLE_C occurs.
*
* Input: 	h_rf_mgmt - handle to RF Management object
*		curr_state - event to handle
*
* Output: None
*
* Return: Next state in SM according to current state and event
*
*******************************************************************************/
nfc_u32 action_evt_disable_c(nfc_handle_t h_rf_mgmt, nfc_u32 curr_state)
{
	struct ncix_rf_mgmt *p_rf_mgmt = (struct ncix_rf_mgmt *)h_rf_mgmt;
	nci_status_e status = NCI_STATUS_OK;
	nci_cmd_param_u	cmd;
	nfc_u32 next_state = curr_state;

	p_rf_mgmt->card_proto_mask = 0;
	p_rf_mgmt->reader_proto_mask = 0;

	switch(curr_state)
	{
		case E_STATE_IDLE:
			{
				ncix_op_complete(p_rf_mgmt->h_ncix, NCI_STATUS_OK);
				next_state = E_STATE_IDLE;
				break;
			}
		case E_STATE_ACTIVE:
		{
			cmd.deactivate.type = NCI_DEACTIVATE_TYPE_IDLE_MODE;
			status = nci_send_cmd(p_rf_mgmt->h_nci, NCI_OPCODE_RF_DEACTIVATE_CMD, &cmd, NULL, 0, NULL, NULL);
			next_state = E_STATE_DE_ACTIVATING;
			break;
		}

		default:
		{
			OSA_ASSERT(0);
		}
	}

	return 	next_state;
}

/******************************************************************************
*
* Name: action_evt_core_set_config_rsp_ok
*
* Description: Called by NCI core when event EVT_CORE_SET_CONFIG_RSP_OK occurs.
*
* Input: 	h_rf_mgmt - handle to RF main object
*		curr_state - event to handle
*
* Output: None
*
* Return: Next state in SM according to current state and event
*
*******************************************************************************/
nfc_u32 action_evt_core_set_config_rsp_ok(nfc_handle_t h_rf_mgmt, nfc_u32 curr_state)
{
	struct ncix_rf_mgmt *p_rf_mgmt = (struct ncix_rf_mgmt *)h_rf_mgmt;
	nci_status_e status = NCI_STATUS_OK;
		nfc_u32 next_state = curr_state;

	switch(curr_state)
	{
		case E_STATE_WAIT_4_CORE_SET_CONFIG_RSP:
		{
			//sanity check - we are not supposed to get here if c or r = 0
			if((p_rf_mgmt->card_proto_mask != 0) || (p_rf_mgmt->reader_proto_mask != 0))
			{
				status = static_send_discover(p_rf_mgmt);
				next_state = E_STATE_ACTIVATING;
			}
			else
			{
				OSA_ASSERT(0);
			}

			break;
		}

		default:
		{
			OSA_ASSERT(0);
		}
	}

	return 	next_state;
}

/******************************************************************************
*
* Name: action_evt_mode_set_rsp_fail
*
* Description: Called by NCI core when event EVT_CORE_SET_CONFIG_RSP_FAIL occurs.
*
* Input: 	h_rf_mgmt - handle to RF main object
*		curr_state - event to handle
*
* Output: None
*
* Return: Next state in SM according to current state and event
*
*******************************************************************************/
nfc_u32 action_evt_core_set_config_rsp_fail(nfc_handle_t h_rf_mgmt, nfc_u32 curr_state)
{
	struct ncix_rf_mgmt *p_rf_mgmt = (struct ncix_rf_mgmt *)h_rf_mgmt;
		nfc_u32 next_state = curr_state;

	switch(curr_state)
	{
		case E_STATE_WAIT_4_CORE_SET_CONFIG_RSP:
		{
			ncix_op_complete(p_rf_mgmt->h_ncix, NCI_STATUS_FAILED);
			next_state = E_STATE_IDLE;
			break;
		}

		default:
		{
			OSA_ASSERT(0);
		}
	}

	return 	next_state;
}


static struct ncix_state s_state[] =
{
	{ NULL,		"ST_IDLE"},
	{ NULL,		"ST_ACTIVATING"},
	{ NULL,		"ST_ACTIVE"},
	{ NULL,		"ST_DEACTIVATING"},
	{ NULL, 		"ST_CORE_SET_CONFIG"},
};

static struct ncix_event s_event[] =
{
	{ action_evt_enable_c,				"EVT_ENABLE_C"},
	{ action_evt_disable_c,				"EVT_DISABLE_C"},
	{ action_evt_discover_r,				"EVT_DISCOVER_R"},
	{ action_evt_deactivate_n,			"EVT_DEACTIVATE_N"},
	{ action_evt_core_set_config_rsp_ok,	"EVT_CORE_SET_CONFIG_RSP_OK" },
	{ action_evt_core_set_config_rsp_fail, 	"EVT_CORE_SET_CONFIG_RSP_FAIL" },
};

/***************************************/
/*** Module Input Routines           ***/
/***************************************/
/******************************************************************************
*
* Name: hndlr_op_rf_enable
*
* Description: Callback that is installed by RF main module creation. It will be
*			  invoked by RF operation callback in result of RF_ENABLE request.
*
* Input: 	h_rf_mgmt - handle to RF main object
*		pu_param - NCI operation structure
*
* Output: None
*
* Return: None
*
*******************************************************************************/
void hndlr_op_rf_enable(nfc_handle_t h_rf_mgmt, op_param_u *pu_param)
{
	struct ncix_rf_mgmt *p_rf_mgmt = (struct ncix_rf_mgmt *)h_rf_mgmt;
	if(pu_param)
	{
		p_rf_mgmt->card_proto_mask = pu_param->rf_enable.ce_bit_mask;
		p_rf_mgmt->reader_proto_mask = pu_param->rf_enable.rw_bit_mask;
		ncix_sm_process_evt(p_rf_mgmt->h_sm, E_EVT_ENABLE_C);
	}
	else
	{
		OSA_ASSERT(0);
	}
}

/******************************************************************************
*
* Name: hndlr_op_rf_disable
*
* Description: Callback that is installed by RF main module creation. It will be
*			  invoked by RF operation callback in result of RF_DISABLE request.
*
* Input: 	h_rf_mgmt - handle to RF main object
*		pu_param - NCI operation structure
*
* Output: None
*
* Return: None
*
*******************************************************************************/
void hndlr_op_rf_disable(nfc_handle_t h_rf_mgmt, op_param_u *pu_param)
{
	struct ncix_rf_mgmt *p_rf_mgmt = (struct ncix_rf_mgmt *)h_rf_mgmt;
	OSA_UNUSED_PARAMETER(pu_param);

	p_rf_mgmt->card_proto_mask = 0;
	p_rf_mgmt->reader_proto_mask = 0;
	ncix_sm_process_evt(p_rf_mgmt->h_sm, E_EVT_DISABLE_C);
}


/******************************************************************************
*
* Name: hndlr_deactivate_ntf
*
* Description: Callback that is installed by RF main module creation. It will be invoked by
*			  RF notification callback in result of RF_DEACTIVATE request.
*
* Input: 	h_main - handle to RF main object
*		opcode - NFCEE opcode
*		pu_ntf - NCI operation structure
*
* Output: None
*
* Return: None
*
*******************************************************************************/
static void hndlr_deactivate_ntf(nfc_handle_t h_rf_mgmt, nfc_u16 opcode, nci_ntf_param_u *pu_ntf)
{
	struct ncix_rf_mgmt *p_rf_mgmt = (struct ncix_rf_mgmt *)h_rf_mgmt;
	OSA_UNUSED_PARAMETER(opcode);
	OSA_UNUSED_PARAMETER(pu_ntf);
	ncix_sm_process_evt(p_rf_mgmt->h_sm, E_EVT_DEACTIVATE_N);
}

/******************************************************************************
*
* Name: hndlr_discover_rsp
*
* Description: Response callback that is called by RF state machine when handling events.
*
* Input: 	h_rf_mgmt - handle to RF main object
*		opcode - NFCEE opcode
*		p_nci_rsp - NCI parameter structure
*
* Output: None
*
* Return: None
*
*******************************************************************************/
static void hndlr_discover_rsp(nfc_handle_t h_rf_mgmt, nfc_u16 opcode, void *p_nci_rsp)
{
	struct ncix_rf_mgmt *p_rf_mgmt = (struct ncix_rf_mgmt *)h_rf_mgmt;
	OSA_UNUSED_PARAMETER(opcode);
	OSA_UNUSED_PARAMETER(p_nci_rsp);
	ncix_sm_process_evt(p_rf_mgmt->h_sm, E_EVT_DISCOVER_R);
}

/******************************************************************************
*
* Name: hndlr_core_set_config_rsp
*
* Description: Response callback that is called by RF state machine when handling events.
*
* Input: 	h_rf_mgmt - handle to RF main object
*		opcode - NFCEE opcode
*		p_nci_rsp - NCI parameter structure
*
* Output: None
*
* Return: None
*
*******************************************************************************/
static void hndlr_core_set_config_rsp(nfc_handle_t h_rf_mgmt, nfc_u16 opcode, nci_rsp_param_u *p_nci_rsp)
{
	struct ncix_rf_mgmt *p_rf_mgmt = (struct ncix_rf_mgmt *)h_rf_mgmt;
	OSA_UNUSED_PARAMETER(opcode);
	OSA_UNUSED_PARAMETER(p_nci_rsp);
	ncix_get_rsp_ptr(p_rf_mgmt->h_ncix)->generic.status = p_nci_rsp->generic.status;

	if(NCI_STATUS_OK == p_nci_rsp->generic.status)
	{
		ncix_sm_process_evt(p_rf_mgmt->h_sm, E_EVT_CORE_SET_CONFIG_RSP_OK);
	}
	else
	{
		ncix_sm_process_evt(p_rf_mgmt->h_sm, E_EVT_CORE_SET_CONFIG_RSP_FAIL);
	}
}

/***************************************/
/*** Module Constructor/Destructor   ***/
/***************************************/
/******************************************************************************
*
* Name: ncix_rf_mgmt_create
*
* Description: RF main module initialization.
*			  Allocate memory and initialize parameters.
*			  Register operation & notification callbacks.
*
* Input: 	h_ncix - handle to main object
*		h_nci - handle to NCI object
*
* Output: None
*
* Return: handle to created main object
*
*******************************************************************************/
nfc_handle_t ncix_rf_mgmt_create(nfc_handle_t h_ncix, nfc_handle_t h_nci)
{
	struct ncix_rf_mgmt *p_rf_mgmt = (struct ncix_rf_mgmt *)osa_mem_alloc(sizeof(struct ncix_rf_mgmt));
	OSA_ASSERT(p_rf_mgmt);

	p_rf_mgmt->h_nci = h_nci;
	p_rf_mgmt->h_ncix = h_ncix;
	p_rf_mgmt->h_sm = ncix_sm_create(p_rf_mgmt, "RF MGMT", E_STATE_IDLE, s_state, s_event);
	p_rf_mgmt->card_proto_mask = 0;
	p_rf_mgmt->reader_proto_mask = 0;

	ncix_register_op_cb(h_ncix, NCI_OPERATION_RF_ENABLE, hndlr_op_rf_enable, p_rf_mgmt);
	ncix_register_op_cb(h_ncix, NCI_OPERATION_RF_DISABLE, hndlr_op_rf_disable, p_rf_mgmt);

	nci_register_ntf_cb(h_nci, E_NCI_NTF_RF_DEACTIVATE, hndlr_deactivate_ntf, p_rf_mgmt);

	return p_rf_mgmt;
}

/******************************************************************************
*
* Name: ncix_rf_mgmt_destroy
*
* Description: RF main module destruction. Frees object allocated memory.
*
* Input:	  h_main - handle to RF main object
*
* Output: None
*
* Return: None
*
*******************************************************************************/
void ncix_rf_mgmt_destroy(nfc_handle_t h_rf_mgmt)
{
	struct ncix_rf_mgmt *p_rf_mgmt = (struct ncix_rf_mgmt *)h_rf_mgmt;
	ncix_sm_destroy(p_rf_mgmt->h_sm);
	osa_mem_free(h_rf_mgmt);
}


#endif //#if (NCI_VERSION_IN_USE==NCI_DRAFT_9)

/* EOF */
