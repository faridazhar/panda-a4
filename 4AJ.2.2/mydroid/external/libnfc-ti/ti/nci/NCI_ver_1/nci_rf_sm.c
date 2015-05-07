/*
 * nci_ver_1\nci_rf_fm.c
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

#if (NCI_VERSION_IN_USE==NCI_VER_1)

#include "nfc_os_adapt.h"
#include "ncix_int.h"
#include "nci_int.h"
#include "nci_rf_sm.h"


/*TBD - used as a patch, since our FW doesn't support more than 1 reader at a time, so for now use only A in P2P*/
extern char bUseP2PFTech;

#define NCI_PROTOCOL_POLL_A		(NCI_PROTOCOL_READER_TYPE_1_CHIP|			\
								 NCI_PROTOCOL_READER_ISO_14443_3_A|		\
								 NCI_PROTOCOL_READER_ISO_14443_4_A|		\
								 NCI_PROTOCOL_READER_P2P_INITIATOR_A)
#define NCI_PROTOCOL_POLL_B		(NCI_PROTOCOL_READER_ISO_14443_3_B|		\
								 NCI_PROTOCOL_READER_ISO_14443_4_B)

#define NCI_PROTOCOL_POLL_F     (NCI_PROTOCOL_READER_FELICA|           \
					             NCI_PROTOCOL_READER_P2P_INITIATOR_F)

#define NCI_PROTOCOL_POLL_15    (NCI_PROTOCOL_READER_ISO_15693_2|      \
					             NCI_PROTOCOL_READER_ISO_15693_3)

#define NCI_PROTOCOL_LISTEN_A   (NCI_PROTOCOL_CARD_ISO_14443_4_A|      \
					             NCI_PROTOCOL_CARD_P2P_TARGET_A)

#define NCI_PROTOCOL_LISTEN_B   (NCI_PROTOCOL_CARD_ISO_14443_4_B)

#define NCI_PROTOCOL_LISTEN_F   (NCI_PROTOCOL_CARD_P2P_TARGET_F)

#define NCI_CFG_PROTOCOL_TYPE_SIZE	1

///////////////////////////////////////////////////////////////////////////

typedef enum _rf_sm_states_e
{
	E_RFST_UNINITIALIZED,						/*0*/
	E_RFST_IDLE,								/*1*/
	E_RFST_W4_DISCOVERY,						/*2*/
	E_RFST_W4_CORE_SET_CONFIG_RSP,				/*3*/
	E_RFST_DEACTIVATING_2_IDLE,					/*4*/
	E_RFST_DISCOVERY,							/*5*/
	E_RFST_W4_ALL_DISCOVERIES,					/*6*/
	E_RFST_DEACTIVATING_2_DISCOVERY,			/*7*/
	E_RFST_POLL_ACTIVE,							/*8*/
	E_RFST_DEACTIVATING_2_POLL_SLEEP,			/*9*/
	E_RFST_W4_HOST_SELECT,						/*10*/
	E_RFST_LISTEN_ACTIVE,						/*11*/
	E_RFST_DEACTIVATING_2_LISTEN_SLEEP,			/*12*/
	E_RFST_LISTEN_SLEEP,						/*13*/
	E_NCI_RF_SM_NUM_STATES
}rf_sm_states_e;

typedef struct
{
	nfc_u8 nfcee_id;
	nfc_u8 isSe;
	nfc_u8 num_tech_and_mode_requests;
	nfc_u8 requested_tech_and_mode[NUM_OF_TECH_AND_MODE_LISTEN_TECHS];
	nfc_u8 num_protocol_requests;
	nfc_u8 requested_protocol[MAX_SUPPORTED_PROTOCOLS_NUM];
	nfc_u8 isActive;
}nfcee_discovery_req_info_t;

typedef struct _nci_rf_sm_s
{
	nfc_handle_t	h_sm;
	nfc_handle_t	h_nci;
	nfc_u16		card_proto_mask;
	nfc_u16		reader_proto_mask;
	nci_ntf_param_u curr_ntf;
	nfc_handle_t	h_nfcee_req_timer;
	nfc_u16		nfcee_card_proto_mask;
	nfc_u16		nfcee_reader_proto_mask;
	struct nci_cmd *p_config_cmd;
	nfc_u8 num_nfcees;
	nfcee_discovery_req_info_t nfcee_discovery_req_info[NCI_MAX_NUM_NFCEE];
	configure_listen_routing_t routing_config;
	nfc_u32						nfcc_features;
}nci_rf_sm_s;

struct nci_sm_state rf_state[] =
{
	{ NULL,		"RFST_UNINITIALIZED"},
	{ NULL,		"RFST_IDLE"},
	{ NULL,		"RFST_W4_DISCOVERY"},
	{ NULL,		"RFST_W4_CORE_SET_CONFIG_RSP"},
	{ NULL,		"RFST_DEACTIVATING_2_IDLE"},
	{ NULL,		"RFST_DISCOVERY"},
	{ NULL, 	"RFST_W4_ALL_DISCOVERIES"},
	{ NULL, 	"RFST_DEACTIVATING_2_DISCOVERY"},
	{ NULL, 	"RFST_POLL_ACTIVE"},
	{ NULL, 	"RFST_DEACTIVATING_2_POLL_SLEEP"},
	{ NULL, 	"RFST_W4_HOST_SELECT"},
	{ NULL, 	"RFST_LISTEN_ACTIVE"},
	{ NULL, 	"RFST_DEACTIVATING_2_LISTEN_SLEEP"},
	{ NULL, 	"RFST_LISTEN_SLEEP"},
};


struct nci_sm_event rf_event[] =
{
	{ evt_hndlr_start,						"E_EVT_START"},
	{ evt_hndlr_rf_discover_cmd,			"E_EVT_RF_DISCOVER_CMD"},
	{ evt_hndlr_rf_discover_rsp_ok,			"E_EVT_RF_DISCOVER_RSP_OK"},
	{ evt_hndlr_rf_discover_rsp_fail,		"E_EVT_RF_DISCOVER_RSP_FAIL"},
	{ evt_hndlr_rf_discover_ntf,			"E_EVT_RF_DISCOVER_NTF"},
	{ evt_hndlr_rf_intf_activated_ntf,		"E_EVT_RF_INTF_ACTIVATED_NTF"},
	{ evt_hndlr_rf_deactivate_cmd,			"E_EVT_RF_DEACTIVATE_CMD"},
	{ evt_hndlr_rf_deactivate_rsp_ok,		"E_EVT_RF_DEACTIVATE_RSP_OK"},
	{ evt_hndlr_rf_deactivate_rsp_fail,		"E_EVT_RF_DEACTIVATE_RSP_FAIL"},
	{ evt_hndlr_rf_deactivate_ntf_idle,		"E_EVT_RF_DEACTIVATE_NTF_IDLE"},
	{ evt_hndlr_rf_deactivate_ntf_sleep,	"E_EVT_RF_DEACTIVATE_NTF_SLEEP"},
	{ evt_hndlr_rf_deactivate_ntf_sleep_af,	"E_EVT_RF_DEACTIVATE_NTF_SLEEP_AF"},
	{ evt_hndlr_rf_deactivate_ntf_discovery,"E_EVT_RF_DEACTIVATE_NTF_DISCOVERY"},
	{ evt_hndlr_core_set_config_cmd,		"E_EVT_CORE_SET_CONFIG_CMD"},
	{ evt_hndlr_core_set_config_rsp_ok,		"E_EVT_CORE_SET_CONFIG_RSP_OK"},
	{ evt_hndlr_core_set_config_rsp_fail,	"E_EVT_CORE_SET_CONFIG_RSP_FAIL"},
	{ evt_hndlr_rf_nfcee_action_ntf,		"E_EVT_RF_NFCEE_ACTION_NTF"},
	{ evt_hndlr_rf_nfcee_discovery_req_ntf,	"E_EVT_RF_NFCEE_DISCOVERY_REQ_NTF"},
	{ evt_hndlr_core_intf_error_ntf,		"E_EVT_CORE_INTF_ERROR_NTF"},
	{ evt_hndlr_stop,						"E_EVT_STOP"},
};

void hndlr_rf_deactivate_rsp(nfc_handle_t h_nci_rf_sm, nfc_u16 opcode, nci_rsp_param_u *p_nci_rsp);
void hndlr_discover_ntf(nfc_handle_t h_nci_sm, nfc_u16 opcode, nci_ntf_param_u *pu_ntf);
void hndlr_deactivate_ntf(nfc_handle_t h_nci_sm, nfc_u16 opcode, nci_ntf_param_u *pu_ntf);
void hndlr_nfcee_action_ntf(nfc_handle_t h_nci_sm, nfc_u16 opcode, nci_ntf_param_u *pu_ntf);
void hndlr_activate_ntf(nfc_handle_t h_nci_sm, nfc_u16 opcode, nci_ntf_param_u *pu_ntf);
void hndlr_nfcee_discovery_req_ntf(nfc_handle_t h_nci_sm, nfc_u16 opcode, nci_ntf_param_u *pu_ntf);
void hndlr_core_intf_err_ntf(nfc_handle_t h_nci_sm, nfc_u16 opcode, nci_ntf_param_u *pu_ntf);
void hndlr_nfcee_ntf(nfc_handle_t h_nci_sm, nfc_u16 opcode, nci_ntf_param_u *pu_ntf);
void nfcee_req_timer_cb(nfc_handle_t h_nci_rf_sm);
nci_status_e nci_rf_sm_build_and_send_listen_routing_cmd(nfc_handle_t h_nci_sm);

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
nfc_handle_t nci_rf_sm_create(nfc_handle_t h_nci)
{
	nci_rf_sm_s *p_nci_rf_sm = (nci_rf_sm_s *)osa_mem_alloc(sizeof(nci_rf_sm_s));
	nfc_u8 i =0;

	OSA_ASSERT(p_nci_rf_sm);


	p_nci_rf_sm->h_nci = h_nci;
	p_nci_rf_sm->h_sm = nci_sm_create(p_nci_rf_sm, "NCI_RF", E_RFST_UNINITIALIZED, rf_state, rf_event);
	p_nci_rf_sm->card_proto_mask = 0;
	p_nci_rf_sm->reader_proto_mask = 0;
	p_nci_rf_sm->h_nfcee_req_timer = NULL;
	p_nci_rf_sm->nfcee_card_proto_mask = 0;
	p_nci_rf_sm->nfcee_reader_proto_mask = 0;
	p_nci_rf_sm->p_config_cmd = NULL;
	p_nci_rf_sm->num_nfcees = 0;

	for(i = 0; i < NCI_MAX_NUM_NFCEE; i++)
	{
		p_nci_rf_sm->nfcee_discovery_req_info[i].nfcee_id = NCI_INVALID_NFCEE_ID;
		p_nci_rf_sm->nfcee_discovery_req_info[i].isSe = NFC_FALSE;
		p_nci_rf_sm->nfcee_discovery_req_info[i].num_tech_and_mode_requests = 0;
		osa_mem_set(p_nci_rf_sm->nfcee_discovery_req_info[i].requested_tech_and_mode, NFC_FALSE, NUM_OF_TECH_AND_MODE_LISTEN_TECHS*sizeof(nfc_u8));
		p_nci_rf_sm->nfcee_discovery_req_info[i].num_protocol_requests = 0;
		osa_mem_set(p_nci_rf_sm->nfcee_discovery_req_info[i].requested_protocol, NFC_FALSE, MAX_SUPPORTED_PROTOCOLS_NUM*sizeof(nfc_u8));
	}
	osa_mem_set(&p_nci_rf_sm->routing_config, 0x00, sizeof(configure_listen_routing_t));
	p_nci_rf_sm->nfcc_features = 0;


	nci_register_ntf_cb(h_nci, E_NCI_NTF_RF_DISCOVER, hndlr_discover_ntf, p_nci_rf_sm);
	nci_register_ntf_cb(h_nci, E_NCI_NTF_RF_INTF_ACTIVATED, hndlr_activate_ntf, p_nci_rf_sm);
	nci_register_ntf_cb(h_nci, E_NCI_NTF_RF_DEACTIVATE, hndlr_deactivate_ntf, p_nci_rf_sm);
	nci_register_ntf_cb(h_nci, E_NCI_NTF_RF_NFCEE_ACTION, hndlr_nfcee_action_ntf, p_nci_rf_sm);
	nci_register_ntf_cb(h_nci, E_NCI_NTF_RF_NFCEE_DISCOVERY_REQ, hndlr_nfcee_discovery_req_ntf, p_nci_rf_sm);
	nci_register_ntf_cb(h_nci, E_NCI_NTF_CORE_INTERFACE_ERROR, hndlr_core_intf_err_ntf, p_nci_rf_sm);
	nci_register_ntf_cb(h_nci, E_NCI_NTF_NFCEE_DISCOVER, hndlr_nfcee_ntf, p_nci_rf_sm);

	return p_nci_rf_sm;
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
void nci_rf_sm_destroy(nfc_handle_t h_nci_sm)
{
	nci_rf_sm_s* p_nci_rf_sm = (nci_rf_sm_s*)h_nci_sm;
	nfc_u8 i = 0;

	p_nci_rf_sm->card_proto_mask = 0;
	p_nci_rf_sm->reader_proto_mask = 0;
	p_nci_rf_sm->p_config_cmd = NULL;
	p_nci_rf_sm->num_nfcees = 0;
	for(i = 0; i < NCI_MAX_NUM_NFCEE; i++)
	{
		p_nci_rf_sm->nfcee_discovery_req_info[i].nfcee_id = NCI_INVALID_NFCEE_ID;
		p_nci_rf_sm->nfcee_discovery_req_info[i].isSe = NFC_FALSE;
		p_nci_rf_sm->nfcee_discovery_req_info[i].num_tech_and_mode_requests = 0;
		osa_mem_set(p_nci_rf_sm->nfcee_discovery_req_info[i].requested_tech_and_mode, NFC_FALSE, NUM_OF_TECH_AND_MODE_LISTEN_TECHS*sizeof(nfc_u8));
		p_nci_rf_sm->nfcee_discovery_req_info[i].num_protocol_requests = 0;
		osa_mem_set(p_nci_rf_sm->nfcee_discovery_req_info[i].requested_protocol, NFC_FALSE, MAX_SUPPORTED_PROTOCOLS_NUM*sizeof(nfc_u8));
	}
	osa_mem_set(&p_nci_rf_sm->routing_config, 0x00, sizeof(configure_listen_routing_t));
	p_nci_rf_sm->nfcc_features = 0;

	if(NULL != p_nci_rf_sm->h_nfcee_req_timer)
	{
		osa_timer_stop(p_nci_rf_sm->h_nfcee_req_timer);
		p_nci_rf_sm->h_nfcee_req_timer = NULL;
	}

	p_nci_rf_sm->nfcee_card_proto_mask = 0;
	p_nci_rf_sm->nfcee_reader_proto_mask = 0;
	nci_sm_destroy(p_nci_rf_sm->h_sm);

	nci_unregister_ntf_cb(p_nci_rf_sm->h_nci, E_NCI_NTF_RF_DISCOVER, hndlr_discover_ntf);
	nci_unregister_ntf_cb(p_nci_rf_sm->h_nci, E_NCI_NTF_RF_INTF_ACTIVATED, hndlr_activate_ntf);
	nci_unregister_ntf_cb(p_nci_rf_sm->h_nci, E_NCI_NTF_RF_DEACTIVATE, hndlr_deactivate_ntf);
	nci_unregister_ntf_cb(p_nci_rf_sm->h_nci, E_NCI_NTF_RF_NFCEE_ACTION, hndlr_nfcee_action_ntf);
	nci_unregister_ntf_cb(p_nci_rf_sm->h_nci, E_NCI_NTF_RF_NFCEE_DISCOVERY_REQ, hndlr_nfcee_discovery_req_ntf);
	nci_unregister_ntf_cb(p_nci_rf_sm->h_nci, E_NCI_NTF_CORE_INTERFACE_ERROR, hndlr_core_intf_err_ntf);
	nci_unregister_ntf_cb(p_nci_rf_sm->h_nci, E_NCI_NTF_NFCEE_DISCOVER, hndlr_nfcee_ntf);

	osa_mem_free(h_nci_sm);
}

void nci_rf_sm_configure_discovery(nfc_handle_t h_nci, nfc_u16 card_proto_mask, nfc_u16 reader_proto_mask)
{
	struct nci_context *p_nci = (struct nci_context *)h_nci;
	nci_rf_sm_s* p_nci_rf_sm = (nci_rf_sm_s*)p_nci->h_nci_rf_sm;

	p_nci_rf_sm->card_proto_mask = card_proto_mask;
	p_nci_rf_sm->reader_proto_mask = reader_proto_mask;
}

nci_status_e nci_rf_sm_set_config_params(nfc_handle_t h_nci, struct nci_cmd *p_config_cmd)
{
	nci_status_e status = NCI_STATUS_OK;
	struct nci_context *p_nci = (struct nci_context *)h_nci;
	nci_rf_sm_s* p_nci_rf_sm = (nci_rf_sm_s*)p_nci->h_nci_rf_sm;

	//Check if we can do the config now
	if(E_RFST_IDLE != nci_sm_get_curr_state(p_nci_rf_sm->h_sm) && E_RFST_DISCOVERY != nci_sm_get_curr_state(p_nci_rf_sm->h_sm))
	{
		return NCI_STATUS_RM_SM_NOT_ALLOWED;
	}

	//Store command
	if(NULL != p_nci_rf_sm->p_config_cmd)
	{
		status = NCI_STATUS_RM_SM_NOT_ALLOWED;
	}
	else
		p_nci_rf_sm->p_config_cmd = p_config_cmd;

	return status;
}

void nci_rf_sm_start(nfc_handle_t h_nci)
{
	struct nci_context *p_nci = (struct nci_context *)h_nci;
	nci_rf_sm_s* p_nci_rf_sm = (nci_rf_sm_s*)p_nci->h_nci_rf_sm;

	nci_sm_process_evt(p_nci_rf_sm->h_sm, E_EVT_START);
}

void nci_rf_sm_stop(nfc_handle_t h_nci)
{
	struct nci_context *p_nci = (struct nci_context *)h_nci;
	nci_rf_sm_s* p_nci_rf_sm = (nci_rf_sm_s*)p_nci->h_nci_rf_sm;

	nci_sm_process_evt(p_nci_rf_sm->h_sm, E_EVT_STOP);
}

void nci_rf_sm_process_evt(nfc_handle_t h_nci, nfc_u32 event)
{
	struct nci_context *p_nci = (struct nci_context *)h_nci;
	nci_rf_sm_s* p_nci_rf_sm = (nci_rf_sm_s*)p_nci->h_nci_rf_sm;

	nci_sm_process_evt(p_nci_rf_sm->h_sm, event);
}

void nci_rf_sm_process_evt_ex(nfc_handle_t h_nci, nfc_u32 event, event_complete_cb_f p_evt_complete_cb, nfc_handle_t h_evt_complete_param)
{
	struct nci_context *p_nci = (struct nci_context *)h_nci;
	nci_rf_sm_s* p_nci_rf_sm = (nci_rf_sm_s*)p_nci->h_nci_rf_sm;

	nci_sm_process_evt_ex(p_nci_rf_sm->h_sm, event, p_evt_complete_cb, h_evt_complete_param);
}


void hndlr_discover_ntf(nfc_handle_t h_nci_sm, nfc_u16 opcode, nci_ntf_param_u *pu_ntf)
{
	nci_rf_sm_s *p_nci_sm = (nci_rf_sm_s*)h_nci_sm;
	OSA_UNUSED_PARAMETER(opcode);

	osa_mem_copy(&p_nci_sm->curr_ntf, pu_ntf, sizeof(struct nci_ntf_discover));
	nci_sm_process_evt(p_nci_sm->h_sm, E_EVT_RF_DISCOVER_NTF);
}

void hndlr_activate_ntf(nfc_handle_t h_nci_sm, nfc_u16 opcode, nci_ntf_param_u *pu_ntf)
{
	nci_rf_sm_s *p_nci_sm = (nci_rf_sm_s*)h_nci_sm;
	OSA_UNUSED_PARAMETER(opcode);

	osa_mem_copy(&p_nci_sm->curr_ntf, pu_ntf, sizeof(struct nci_ntf_activate));
	nci_sm_process_evt(p_nci_sm->h_sm, E_EVT_RF_INTF_ACTIVATED_NTF);
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
void hndlr_deactivate_ntf(nfc_handle_t h_nci_sm, nfc_u16 opcode, nci_ntf_param_u *pu_ntf)
{
	nci_rf_sm_s *p_nci_sm = (nci_rf_sm_s*)h_nci_sm;
	OSA_UNUSED_PARAMETER(opcode);

	osa_mem_copy(&p_nci_sm->curr_ntf, pu_ntf, sizeof(struct nci_ntf_deactivate));

	switch (pu_ntf->deactivate.type)
	{
	case NCI_DEACTIVATE_TYPE_IDLE_MODE:
		nci_sm_process_evt(p_nci_sm->h_sm, E_EVT_RF_DEACTIVATE_NTF_IDLE);
		break;
	case NCI_DEACTIVATE_TYPE_SLEEP_MODE:
		nci_sm_process_evt(p_nci_sm->h_sm, E_EVT_RF_DEACTIVATE_NTF_SLEEP);
		break;
	case NCI_DEACTIVATE_TYPE_SLEEP_AF_MODE:
		nci_sm_process_evt(p_nci_sm->h_sm, E_EVT_RF_DEACTIVATE_NTF_SLEEP_AF);
		break;
	case NCI_DEACTIVATE_TYPE_DISCOVERY:
		nci_sm_process_evt(p_nci_sm->h_sm, E_EVT_RF_DEACTIVATE_NTF_DISCOVERY);
		break;
	default:
		OSA_ASSERT(NFC_FALSE && "nci_rm_sm:hndlr_deactivate_ntf:");
	}

}

void hndlr_nfcee_action_ntf(nfc_handle_t h_nci_sm, nfc_u16 opcode, nci_ntf_param_u *pu_ntf)
{
	nci_rf_sm_s *p_nci_sm = (nci_rf_sm_s*)h_nci_sm;
	OSA_UNUSED_PARAMETER(opcode);

	osa_mem_copy(&p_nci_sm->curr_ntf, pu_ntf, sizeof(struct nci_ntf_rf_nfcee_action));
	nci_sm_process_evt(p_nci_sm->h_sm, E_EVT_RF_NFCEE_ACTION_NTF);
}

void hndlr_nfcee_discovery_req_ntf(nfc_handle_t h_nci_sm, nfc_u16 opcode, nci_ntf_param_u *pu_ntf)
{
	nci_rf_sm_s *p_nci_sm = (nci_rf_sm_s*)h_nci_sm;
	OSA_UNUSED_PARAMETER(opcode);

	osa_mem_copy(&p_nci_sm->curr_ntf, pu_ntf, sizeof(struct nci_ntf_rf_nfcee_discovery_req));
	nci_sm_process_evt(p_nci_sm->h_sm, E_EVT_RF_NFCEE_DISCOVERY_REQ_NTF);
}

void hndlr_core_intf_err_ntf(nfc_handle_t h_nci_sm, nfc_u16 opcode, nci_ntf_param_u *pu_ntf)
{
	nci_rf_sm_s *p_nci_sm = (nci_rf_sm_s*)h_nci_sm;
	OSA_UNUSED_PARAMETER(opcode);

	osa_mem_copy(&p_nci_sm->curr_ntf, pu_ntf, sizeof(struct nci_ntf_core_intf_error));
	nci_sm_process_evt(p_nci_sm->h_sm, E_EVT_CORE_INTF_ERROR_NTF);
}

void hndlr_nfcee_ntf(nfc_handle_t h_nci_sm, nfc_u16 opcode, nci_ntf_param_u *pu_ntf)
{
	nci_rf_sm_s *p_nci_sm = (nci_rf_sm_s*)h_nci_sm;
	nfc_u8 bIsSe = 0;
	nfc_u8 i = 0;

	OSA_UNUSED_PARAMETER(opcode);

	switch (pu_ntf->nfcee_discover.nfcee_status)
	{
	case NFCEE_STATUS_CONNECTED_AND_ACTIVE:
	case NFCEE_STATUS_CONNECTED_AND_INACTIVE:
		{
			//Check if this is SE or UICC - check what protocols are supported - 0x03 == Transparent for SE and 0x80 for UICC
			for(i = 0; i < pu_ntf->nfcee_discover.num_nfcee_interface_information_entries; i++)
			{
				if(pu_ntf->nfcee_discover.nfcee_protocols[i] == NCI_NFCEE_PROTOCOL_TRANSPARENT)
				{
					bIsSe = NFC_TRUE;
					break;
				}
			}
			p_nci_sm->nfcee_discovery_req_info[p_nci_sm->num_nfcees].nfcee_id = pu_ntf->nfcee_discover.nfcee_id;
			p_nci_sm->nfcee_discovery_req_info[p_nci_sm->num_nfcees].isSe = bIsSe;
			if(NFCEE_STATUS_CONNECTED_AND_ACTIVE == pu_ntf->nfcee_discover.nfcee_status)
				p_nci_sm->nfcee_discovery_req_info[p_nci_sm->num_nfcees].isActive = NFC_TRUE;
			else
				p_nci_sm->nfcee_discovery_req_info[p_nci_sm->num_nfcees].isActive = NFC_FALSE;

			p_nci_sm->num_nfcees++;

			break;
		}
	case NFCEE_STATUS_REMOVED:
		{
			//TBD - remove from DB
			OSA_ASSERT(0);
			break;
		}
	default:
		OSA_ASSERT(0);
	}


}

nfc_u32 evt_hndlr_start(nfc_handle_t h_nci_sm, nfc_u32 curr_state)
{
	nfc_u32 next_state = curr_state;
	OSA_UNUSED_PARAMETER(h_nci_sm);

	switch(curr_state)
	{
	case E_RFST_UNINITIALIZED:
		{
			next_state = E_RFST_IDLE;
			break;
		}
	default:
		{
			OSA_ASSERT(NFC_FALSE && "nci_rm_sm:evt_hndlr_start");
		}
	}

	return next_state;
}

nfc_u32 evt_hndlr_stop(nfc_handle_t h_nci_sm, nfc_u32 curr_state)
{
	nci_rf_sm_s* p_nci_rf_sm = (nci_rf_sm_s*)h_nci_sm;
	nfc_u32 next_state = curr_state;
	nfc_u8 i = 0;

	p_nci_rf_sm->card_proto_mask = 0;
	p_nci_rf_sm->reader_proto_mask = 0;
	p_nci_rf_sm->num_nfcees = 0;
	for(i = 0; i < NCI_MAX_NUM_NFCEE; i++)
	{
		p_nci_rf_sm->nfcee_discovery_req_info[i].nfcee_id = NCI_INVALID_NFCEE_ID;
		p_nci_rf_sm->nfcee_discovery_req_info[i].isSe = NFC_FALSE;
		p_nci_rf_sm->nfcee_discovery_req_info[i].num_tech_and_mode_requests = 0;
		osa_mem_set(p_nci_rf_sm->nfcee_discovery_req_info[i].requested_tech_and_mode, NFC_FALSE, NUM_OF_TECH_AND_MODE_LISTEN_TECHS*sizeof(nfc_u8));
		p_nci_rf_sm->nfcee_discovery_req_info[i].num_protocol_requests = 0;
		osa_mem_set(p_nci_rf_sm->nfcee_discovery_req_info[i].requested_protocol, NFC_FALSE, MAX_SUPPORTED_PROTOCOLS_NUM*sizeof(nfc_u8));
	}
	osa_mem_set(&p_nci_rf_sm->routing_config, 0x00, sizeof(configure_listen_routing_t));
	p_nci_rf_sm->nfcc_features = 0;

	if(NULL != p_nci_rf_sm->p_config_cmd)
		osa_mem_free(p_nci_rf_sm->p_config_cmd);
	p_nci_rf_sm->p_config_cmd = NULL;

	if(NULL != p_nci_rf_sm->h_nfcee_req_timer)
	{
		osa_timer_stop(p_nci_rf_sm->h_nfcee_req_timer);
		p_nci_rf_sm->h_nfcee_req_timer = NULL;
	}

	p_nci_rf_sm->nfcee_card_proto_mask = 0;
	p_nci_rf_sm->nfcee_reader_proto_mask = 0;

	next_state = E_RFST_UNINITIALIZED;
	nci_sm_reset(p_nci_rf_sm->h_sm);

	return next_state;
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
void hndlr_core_set_config_rsp(nfc_handle_t h_nci_rf_sm, nfc_u16 opcode, nci_rsp_param_u *p_nci_rsp)
{
	nci_rf_sm_s *p_nci_rf_sm = (nci_rf_sm_s *)h_nci_rf_sm;
	OSA_UNUSED_PARAMETER(opcode);

	if(NCI_STATUS_OK == p_nci_rsp->generic.status)
	{
		nci_sm_process_evt(p_nci_rf_sm->h_sm, E_EVT_CORE_SET_CONFIG_RSP_OK);
	}
	else
	{
		nci_sm_process_evt(p_nci_rf_sm->h_sm, E_EVT_CORE_SET_CONFIG_RSP_FAIL);
	}
}

/******************************************************************************
*
* Name: static_send_core_set_config
*
* Description: Send CORE_SET_CONFIG command - according to card protocol type
*			  configure TLV structure and send it.
*			  NOTE: this function assumes that p_nci_rf_sm->card_proto_mask != 0. This
*			  verfication should be done by calling task.
*
* Input: 	p_nci_rf_sm - handle to RF Management object
*
* Output: None
*
* Return: Success -NFC_RES_OK
*		  Failure -NFC_RES_ERROR
*
*******************************************************************************/
nfc_status send_core_set_config(nci_rf_sm_s *p_nci_rf_sm)
{
	nci_status_e status = NCI_STATUS_OK;
	nfc_u8 la_proto = 0;
	nfc_u8 lb_proto = 0;
	nfc_u8 lf_proto = 0;
	struct nci_tlv tlv[NCI_MAX_TLV_IN_CMD];
	nfc_u8 index = 0;
	struct nci_cmd_core_set_config core_set_config;

	/* set la_protocol  - configure as card */
	if(p_nci_rf_sm->card_proto_mask & NCI_PROTOCOL_CARD_P2P_TARGET_A)
	{
		la_proto |= NCI_CFG_TYPE_LA_PROTOCOL_TYPE_NFC_DEP;
	}

	/* set lf_protocol  - configure as card */
	if(p_nci_rf_sm->card_proto_mask & NCI_PROTOCOL_CARD_P2P_TARGET_F)
	{
		lf_proto |= NCI_CFG_TYPE_LF_PROTOCOL_TYPE_NFC_DEP;
	}

	if(p_nci_rf_sm->card_proto_mask & NCI_PROTOCOL_CARD_ISO_14443_4_A)
	{
		la_proto |= NCI_CFG_TYPE_LA_PROTOCOL_TYPE_ISO_DEP;
	}

	if(p_nci_rf_sm->card_proto_mask & NCI_PROTOCOL_CARD_ISO_14443_4_B)
	{
		lb_proto |= NCI_CFG_TYPE_LB_PROTOCOL_TYPE_ISO_DEP;
	}

	tlv[index].type = NCI_CFG_TYPE_LA_SEL_INFO;
	tlv[index].length = NCI_CFG_PROTOCOL_TYPE_SIZE;
	tlv[index++].p_value = &la_proto;

	tlv[index].type = NCI_CFG_TYPE_LB_SENSB_INFO;
	tlv[index].length = NCI_CFG_PROTOCOL_TYPE_SIZE;
	tlv[index++].p_value = &lb_proto;

	tlv[index].type = NCI_CFG_TYPE_LF_PROTOCOL_TYPE; //TBD - what param should be here?
	tlv[index].length = NCI_CFG_PROTOCOL_TYPE_SIZE;
	tlv[index++].p_value = &lf_proto;

	core_set_config.num_of_tlvs = index;
	OSA_ASSERT(core_set_config.num_of_tlvs > 0);
	core_set_config.p_tlv = tlv;

	status = nci_send_cmd(p_nci_rf_sm->h_nci, NCI_OPCODE_CORE_SET_CONFIG_CMD,
		(nci_cmd_param_u *)&core_set_config, NULL, 0, hndlr_core_set_config_rsp, p_nci_rf_sm);

	return (status == NCI_STATUS_OK) ? NFC_RES_OK:NFC_RES_ERROR;
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
void hndlr_discover_rsp(nfc_handle_t h_nci_rf_sm, nfc_u16 opcode, void *p_nci_rsp)
{
	nci_rf_sm_s* p_nci_rf_sm = (nci_rf_sm_s *)h_nci_rf_sm;
	nci_rsp_param_u* p_rsp = (nci_rsp_param_u*)p_nci_rsp;

	OSA_UNUSED_PARAMETER(opcode);
	OSA_UNUSED_PARAMETER(p_nci_rsp);


	if(p_rsp->generic.status == NCI_STATUS_OK)
		nci_sm_process_evt(p_nci_rf_sm->h_sm, E_EVT_RF_DISCOVER_RSP_OK);
	else
	{
		//TBD - add log with error code
		nci_sm_process_evt(p_nci_rf_sm->h_sm, E_EVT_RF_DISCOVER_RSP_FAIL);
	}
}

/******************************************************************************
*
* Name: static_send_discover
*
* Description: Send send_discover command -according to protocol type configure
*			  command structure and send it.
*
* Input: 	p_nci_rf_sm - handle to RF Management object
*
* Output: None
*
* Return: Success -NFC_RES_OK
*		  Failure -NFC_RES_ERROR
*
*******************************************************************************/
nfc_status send_discover_cmd(nci_rf_sm_s* p_nci_rf_sm)
{
	nci_status_e status;
	nci_cmd_param_u	cmd;

	/* set discovery */
	cmd.discover.num_of_conf = 0;
	if((p_nci_rf_sm->nfcee_card_proto_mask != 0 || p_nci_rf_sm->nfcee_reader_proto_mask != 0) &&
	  (p_nci_rf_sm->reader_proto_mask == 0 && p_nci_rf_sm->card_proto_mask == 0))
	{
		osa_report(INFORMATION, ("send_discover_cmd: Send nfcee only discovery"));
		nci_rf_sm_build_and_send_listen_routing_cmd(p_nci_rf_sm);
		status = nci_send_cmd(p_nci_rf_sm->h_nci, NCI_OPCODE_RF_DISCOVER_CMD, &cmd, NULL, 0, hndlr_discover_rsp, p_nci_rf_sm);
		return (status == NCI_STATUS_OK) ? NFC_RES_OK:NFC_RES_ERROR;
	}

	if(p_nci_rf_sm->reader_proto_mask & NCI_PROTOCOL_POLL_A)
	{
		osa_report(INFORMATION, ("send_discover_cmd: Set POLL_A_PASSIVE discovery"));
		cmd.discover.disc_conf[cmd.discover.num_of_conf].type = NFC_A_PASSIVE_POLL_MODE;
		cmd.discover.disc_conf[cmd.discover.num_of_conf].frequency = E_NCI_DISCOVERY_FREQUENCY_EVERY_1_DISCOVERY_PERIOD;
		cmd.discover.num_of_conf++;
	}

	if(p_nci_rf_sm->reader_proto_mask & NCI_PROTOCOL_POLL_B)
	{
		osa_report(INFORMATION, ("send_discover_cmd: Set POLL_B_PASSIVE discovery"));
		cmd.discover.disc_conf[cmd.discover.num_of_conf].type = NFC_B_PASSIVE_POLL_MODE;
		cmd.discover.disc_conf[cmd.discover.num_of_conf].frequency = E_NCI_DISCOVERY_FREQUENCY_EVERY_1_DISCOVERY_PERIOD;
		cmd.discover.num_of_conf++;
	}

	if(p_nci_rf_sm->reader_proto_mask & NCI_PROTOCOL_POLL_F)
	{
		osa_report(INFORMATION, ("send_discover_cmd: Set POLL_F_PASSIVE discovery"));
		cmd.discover.disc_conf[cmd.discover.num_of_conf].type = NFC_F_PASSIVE_POLL_MODE;
		cmd.discover.disc_conf[cmd.discover.num_of_conf].frequency = E_NCI_DISCOVERY_FREQUENCY_EVERY_1_DISCOVERY_PERIOD;
		cmd.discover.num_of_conf++;
	}

	if(p_nci_rf_sm->reader_proto_mask & NCI_PROTOCOL_POLL_15)
	{
		cmd.discover.disc_conf[cmd.discover.num_of_conf].type = NFC_15693_PASSIVE_POLL_MODE;
		cmd.discover.disc_conf[cmd.discover.num_of_conf].frequency = E_NCI_DISCOVERY_FREQUENCY_EVERY_1_DISCOVERY_PERIOD;
		cmd.discover.num_of_conf++;
	}

	if(p_nci_rf_sm->card_proto_mask & NCI_PROTOCOL_LISTEN_A)
	{
		osa_report(INFORMATION, ("send_discover_cmd: Set LISTEN_A_PASSIVE discovery"));
		cmd.discover.disc_conf[cmd.discover.num_of_conf].type = NFC_A_PASSIVE_LISTEN_MODE;

		cmd.discover.disc_conf[cmd.discover.num_of_conf].frequency = E_NCI_DISCOVERY_FREQUENCY_EVERY_1_DISCOVERY_PERIOD;
		cmd.discover.num_of_conf++;
	}

	if(p_nci_rf_sm->card_proto_mask & NCI_PROTOCOL_LISTEN_B)
	{
		/*nfc_u8 buff[5] = {0x84, 0x01, 0x02, 0x20, 0x03};
		osa_report(INFORMATION, ("send_discover_cmd: Set LISTEN_B_PASSIVE discovery"));
		//TBD - Patch - send NCI vendor specific to disable P2P F target

		nci_send_vendor_specific_cmd(	p_nci_rf_sm->h_nci,
										__OPCODE(NCI_GID_PROPRIETARY,0x2e),
										buff,
										5,
										NULL,
										NULL);*/
		cmd.discover.disc_conf[cmd.discover.num_of_conf].type = NFC_B_PASSIVE_LISTEN_MODE;
		cmd.discover.disc_conf[cmd.discover.num_of_conf].frequency = E_NCI_DISCOVERY_FREQUENCY_EVERY_1_DISCOVERY_PERIOD;
		cmd.discover.num_of_conf++;
	}

	if(p_nci_rf_sm->card_proto_mask & NCI_PROTOCOL_LISTEN_F)
	{
		osa_report(INFORMATION, ("send_discover_cmd: Set LISTEN_F_PASSIVE discovery"));

		/*{
			//TBD - Patch - send NCI vendor specific to enable P2P F target
			nfc_u8 buff[5] = {0x84, 0x01, 0x02, 0x20, 0x01};
			nci_send_vendor_specific_cmd(	p_nci_rf_sm->h_nci,
											__OPCODE(NCI_GID_PROPRIETARY,0x2e),
											buff,
											5,
											NULL,
											NULL);
		}*/

			cmd.discover.disc_conf[cmd.discover.num_of_conf].type = NFC_F_PASSIVE_LISTEN_MODE;
		cmd.discover.disc_conf[cmd.discover.num_of_conf].frequency = E_NCI_DISCOVERY_FREQUENCY_EVERY_1_DISCOVERY_PERIOD;
		cmd.discover.num_of_conf++;
	}

	osa_report(INFORMATION, ("send_discover_cmd: Send discovery command"));
	nci_rf_sm_build_and_send_listen_routing_cmd(p_nci_rf_sm);
	status = nci_send_cmd(p_nci_rf_sm->h_nci, NCI_OPCODE_RF_DISCOVER_CMD, &cmd, NULL, 0, hndlr_discover_rsp, p_nci_rf_sm);
	return (status == NCI_STATUS_OK) ? NFC_RES_OK:NFC_RES_ERROR;
}


nfc_u32 evt_hndlr_rf_discover_cmd(nfc_handle_t h_nci_sm, nfc_u32 curr_state)
{
	nci_rf_sm_s* p_nci_rf_sm = (nci_rf_sm_s*)h_nci_sm;
	struct nci_context *p_nci = (struct nci_context*)p_nci_rf_sm->h_nci;
	nci_status_e status = NCI_STATUS_OK;
	nfc_u32 next_state = curr_state;

	switch(curr_state)
	{
	case E_RFST_IDLE:
		{
			if(NULL != p_nci_rf_sm->h_nfcee_req_timer)
			{
				osa_timer_stop(p_nci_rf_sm->h_nfcee_req_timer);
				p_nci_rf_sm->h_nfcee_req_timer = NULL;
			}

			if(p_nci_rf_sm->card_proto_mask != 0)
			{
				status = send_core_set_config(p_nci_rf_sm);
				if(NFC_RES_OK == status)
				{
					next_state = E_RFST_W4_CORE_SET_CONFIG_RSP;
				}
				else
				{
					//TBD - add log with error
				}
			}
			else if(p_nci_rf_sm->reader_proto_mask != 0)
			{
				status = send_discover_cmd(p_nci_rf_sm);
				if(NFC_RES_OK == status)
				{
					next_state = E_RFST_W4_DISCOVERY;
				}
				else
				{
					//TBD - add log with error
				}
			}
			else if(p_nci_rf_sm->nfcee_card_proto_mask != 0 || p_nci_rf_sm->nfcee_reader_proto_mask != 0)
			{
				status = send_discover_cmd(p_nci_rf_sm);
				if(NFC_RES_OK == status)
				{
					next_state = E_RFST_W4_DISCOVERY;
				}
				else
				{
					//TBD - add log with error
				}
			}
			else
			{
				//We got here - it means that we got discovery request without valid parameters
				OSA_ASSERT(NFC_FALSE && "nci_rm_sm:evt_hndlr_rf_discover_cmd(1)");
			}
			break;
		}

	case E_RFST_DISCOVERY:
	case E_RFST_W4_ALL_DISCOVERIES:
	case E_RFST_W4_HOST_SELECT:
	case E_RFST_LISTEN_SLEEP:
		{
			nci_cmd_param_u	cmd;

			//Send deactivate command and when response is received, we will issue discover command with new parameters.
			cmd.deactivate.type = NCI_DEACTIVATE_TYPE_IDLE_MODE;
			status = nci_send_cmd(p_nci_rf_sm->h_nci, NCI_OPCODE_RF_DEACTIVATE_CMD, &cmd, NULL, 0, hndlr_rf_deactivate_rsp, p_nci_rf_sm);

			next_state = E_RFST_DEACTIVATING_2_IDLE;
			break;
		}

	case E_RFST_POLL_ACTIVE:
	case E_RFST_LISTEN_ACTIVE:
		{
			nci_cmd_param_u	cmd;

			//Send deactivate command
			cmd.deactivate.type = NCI_DEACTIVATE_TYPE_IDLE_MODE;
			status = nci_send_cmd(p_nci_rf_sm->h_nci, NCI_OPCODE_RF_DEACTIVATE_CMD, &cmd, NULL, 0, hndlr_rf_deactivate_rsp, p_nci_rf_sm);

			next_state = curr_state;
			break;
		}

	case E_RFST_W4_DISCOVERY:
		{
			next_state = curr_state;
			if(NULL == p_nci_rf_sm->h_nfcee_req_timer)
			{
				osa_timer_start(p_nci->h_osa, 1000, nfcee_req_timer_cb, p_nci_rf_sm);
			}
			break;
		}

	default:
		{
			osa_report(INFORMATION, ("RF curr state = %d", curr_state));
			OSA_ASSERT(NFC_FALSE && "nci_rm_sm:evt_hndlr_rf_discover_cmd(2)");
		}
	}

	return next_state;
}


nfc_u32 evt_hndlr_rf_discover_rsp_ok(nfc_handle_t h_nci_sm, nfc_u32 curr_state)
{
	nci_rf_sm_s* p_nci_rf_sm = (nci_rf_sm_s*)h_nci_sm;
	nfc_u32 next_state = curr_state;

	switch(curr_state)
	{
	case E_RFST_W4_DISCOVERY:
		{
			if((p_nci_rf_sm->nfcee_card_proto_mask == 0 && p_nci_rf_sm->nfcee_reader_proto_mask == 0) &&
			   (p_nci_rf_sm->card_proto_mask == 0 && p_nci_rf_sm->reader_proto_mask == 0))
			{
				//If proto masks are 0, it means we got deactivate command in between - so we need to wait for deactivate response, and state is
				// wait for deactivate rsp.
				next_state = E_RFST_DEACTIVATING_2_IDLE; //TBD - when other deactivation types are available - should be changed
				OSA_ASSERT(NFC_FALSE && "nci_rm_sm:evt_hndlr_rf_discover_rsp_ok1");
			}
			else
			{
				next_state = E_RFST_DISCOVERY;
				nci_sm_process_evt_ex_completed(p_nci_rf_sm->h_sm, NCI_STATUS_OK);
			}
			break;
		}
	default:
		{
			OSA_ASSERT(NFC_FALSE && "nci_rm_sm:evt_hndlr_rf_discover_rsp_ok");
		}
	}

	return next_state;
}


nfc_u32 evt_hndlr_rf_discover_rsp_fail(nfc_handle_t h_nci_sm, nfc_u32 curr_state)
{
	nci_rf_sm_s* p_nci_rf_sm = (nci_rf_sm_s*)h_nci_sm;
	nfc_u32 next_state = curr_state;

	switch(curr_state)
	{
	case E_RFST_W4_DISCOVERY:
		{
			next_state = E_RFST_IDLE;
			nci_sm_process_evt_ex_completed(p_nci_rf_sm->h_sm, NCI_STATUS_FAILED);
			break;
		}
	default:
		{
			OSA_ASSERT(NFC_FALSE && "nci_rm_sm:evt_hndlr_rf_discover_rsp_fail");
		}
	}

	return next_state;
}


nfc_u32 evt_hndlr_rf_discover_ntf(nfc_handle_t h_nci_sm, nfc_u32 curr_state)
{
	nci_rf_sm_s* p_nci_rf_sm = (nci_rf_sm_s*)h_nci_sm;
	nfc_u32 next_state = curr_state;

	switch(curr_state)
	{
	case E_RFST_DISCOVERY:
		{
			if(p_nci_rf_sm->curr_ntf.discover.notification_type == 2)
			{
				next_state = E_RFST_W4_ALL_DISCOVERIES;
			}
			else
				OSA_ASSERT(NFC_FALSE && "nci_rm_sm:evt_hndlr_rf_discover_ntf(1)");

			break;
		}

	case E_RFST_W4_ALL_DISCOVERIES:
		{
			if(p_nci_rf_sm->curr_ntf.discover.notification_type == 2)
			{
				next_state = curr_state;
			}
			else if(p_nci_rf_sm->curr_ntf.discover.notification_type == 0 ||
					p_nci_rf_sm->curr_ntf.discover.notification_type == 1)
			{
				next_state = E_RFST_W4_HOST_SELECT;
			}
			else
				OSA_ASSERT(NFC_FALSE && "nci_rm_sm:evt_hndlr_rf_discover_ntf(2)");

			break;
		}

	default:
		{
			OSA_ASSERT(NFC_FALSE && "nci_rm_sm:evt_hndlr_rf_discover_ntf(3)");
		}
	}

	return next_state;
}

nfc_u32 evt_hndlr_rf_intf_activated_ntf(nfc_handle_t h_nci_sm, nfc_u32 curr_state)
{
	nci_rf_sm_s* p_nci_rf_sm = (nci_rf_sm_s*)h_nci_sm;
	nfc_u32 next_state = curr_state;

	switch(curr_state)
	{
	case E_RFST_DISCOVERY:
		{
			if(NCI_RF_INTERFACE_NFCEE_DIRECT == p_nci_rf_sm->curr_ntf.activate.rf_interface_type)
			{
				next_state = E_RFST_LISTEN_ACTIVE;
			}
			else if(p_nci_rf_sm->curr_ntf.activate.activated_rf_technology_and_mode == NFC_A_PASSIVE_POLL_MODE ||
					p_nci_rf_sm->curr_ntf.activate.activated_rf_technology_and_mode == NFC_B_PASSIVE_POLL_MODE ||
					p_nci_rf_sm->curr_ntf.activate.activated_rf_technology_and_mode == NFC_F_PASSIVE_POLL_MODE ||
					p_nci_rf_sm->curr_ntf.activate.activated_rf_technology_and_mode == NFC_15693_PASSIVE_POLL_MODE)
			{
				next_state = E_RFST_POLL_ACTIVE;
			}
			else if(p_nci_rf_sm->curr_ntf.activate.activated_rf_technology_and_mode == NFC_A_PASSIVE_LISTEN_MODE ||
					p_nci_rf_sm->curr_ntf.activate.activated_rf_technology_and_mode == NFC_B_PASSIVE_LISTEN_MODE ||
					p_nci_rf_sm->curr_ntf.activate.activated_rf_technology_and_mode == NFC_F_PASSIVE_LISTEN_MODE )
			{
				next_state = E_RFST_LISTEN_ACTIVE;
			}
			else
				OSA_ASSERT(NFC_FALSE && "nci_rm_sm:evt_hndlr_rf_intf_activated_ntf(1)");

			break;
		}

	case E_RFST_W4_HOST_SELECT:
		{
			if( p_nci_rf_sm->curr_ntf.activate.activated_rf_technology_and_mode == NFC_A_PASSIVE_POLL_MODE ||
				p_nci_rf_sm->curr_ntf.activate.activated_rf_technology_and_mode == NFC_B_PASSIVE_POLL_MODE ||
				p_nci_rf_sm->curr_ntf.activate.activated_rf_technology_and_mode == NFC_F_PASSIVE_POLL_MODE ||
				p_nci_rf_sm->curr_ntf.activate.activated_rf_technology_and_mode == NFC_15693_PASSIVE_POLL_MODE )
			{
				next_state = E_RFST_POLL_ACTIVE;
			}
			else
				OSA_ASSERT(NFC_FALSE && "nci_rm_sm:evt_hndlr_rf_intf_activated_ntf(2)");

			break;
		}

	case E_RFST_LISTEN_SLEEP:
		{
			if(NCI_RF_INTERFACE_NFCEE_DIRECT == p_nci_rf_sm->curr_ntf.activate.rf_interface_type)
			{
				next_state = E_RFST_LISTEN_ACTIVE;
			}
			else if( p_nci_rf_sm->curr_ntf.activate.activated_rf_technology_and_mode == NFC_A_PASSIVE_LISTEN_MODE ||
					 p_nci_rf_sm->curr_ntf.activate.activated_rf_technology_and_mode == NFC_B_PASSIVE_LISTEN_MODE ||
					 p_nci_rf_sm->curr_ntf.activate.activated_rf_technology_and_mode == NFC_F_PASSIVE_LISTEN_MODE )
			{
				next_state = E_RFST_LISTEN_ACTIVE;
			}
			else
				OSA_ASSERT(NFC_FALSE && "nci_rm_sm:evt_hndlr_rf_intf_activated_ntf(3)");

			break;
		}

	default:
		{
			//In all other states - just ignore the activate notification, since it can happen
			//because of different race conditions with FW
			break;
		}
	}

	return next_state;
}

void hndlr_rf_deactivate_rsp(nfc_handle_t h_nci_rf_sm, nfc_u16 opcode, nci_rsp_param_u *p_nci_rsp)
{
	nci_rf_sm_s *p_nci_rf_sm = (nci_rf_sm_s *)h_nci_rf_sm;
	OSA_UNUSED_PARAMETER(opcode);

	if(NCI_STATUS_OK == p_nci_rsp->generic.status)
	{
		nci_sm_process_evt(p_nci_rf_sm->h_sm, E_EVT_RF_DEACTIVATE_RSP_OK);
	}
	else
	{
		nci_sm_process_evt(p_nci_rf_sm->h_sm, E_EVT_RF_DEACTIVATE_RSP_FAIL);
	}
}

nfc_u32 evt_hndlr_rf_deactivate_cmd(nfc_handle_t h_nci_sm, nfc_u32 curr_state)
{
	nci_rf_sm_s* p_nci_rf_sm = (nci_rf_sm_s*)h_nci_sm;
	nci_status_e status = NCI_STATUS_OK;
	nfc_u32 next_state = curr_state;
	nci_cmd_param_u	cmd;

	switch(curr_state)
	{
	case E_RFST_IDLE:
		{
			//We already idle - do nothing (TBD - when other modes except idle will be supported - should be changed)
			nci_sm_process_evt_ex_completed(p_nci_rf_sm->h_sm, NCI_STATUS_OK);
			break;
		}
	case E_RFST_W4_DISCOVERY:
	case E_RFST_W4_CORE_SET_CONFIG_RSP:
		{
			//Send deactivate command, when received responses to commands above - stay in E_RFST_DEACTIVATING_2_IDLE state
			cmd.deactivate.type = NCI_DEACTIVATE_TYPE_IDLE_MODE;
			status = nci_send_cmd(p_nci_rf_sm->h_nci, NCI_OPCODE_RF_DEACTIVATE_CMD, &cmd, NULL, 0, hndlr_rf_deactivate_rsp, p_nci_rf_sm);

			next_state = curr_state;
			break;
		}
	case E_RFST_W4_ALL_DISCOVERIES:
	case E_RFST_W4_HOST_SELECT:
		{
			//Send deactivate command
			cmd.deactivate.type = NCI_DEACTIVATE_TYPE_IDLE_MODE;
			status = nci_send_cmd(p_nci_rf_sm->h_nci, NCI_OPCODE_RF_DEACTIVATE_CMD, &cmd, NULL, 0, hndlr_rf_deactivate_rsp, p_nci_rf_sm);

			next_state = E_RFST_DEACTIVATING_2_IDLE;
			break;
		}

	case E_RFST_DEACTIVATING_2_IDLE:
	case E_RFST_DEACTIVATING_2_DISCOVERY:
	case E_RFST_DEACTIVATING_2_POLL_SLEEP:
	case E_RFST_DEACTIVATING_2_LISTEN_SLEEP:
		{
			//We already deactivating - do nothing
			break;
		}

	case E_RFST_POLL_ACTIVE:
	case E_RFST_LISTEN_ACTIVE:
		{
			//Send deactivate command
			cmd.deactivate.type = NCI_DEACTIVATE_TYPE_IDLE_MODE;
			status = nci_send_cmd(p_nci_rf_sm->h_nci, NCI_OPCODE_RF_DEACTIVATE_CMD, &cmd, NULL, 0, hndlr_rf_deactivate_rsp, p_nci_rf_sm);

			next_state = curr_state;
			break;
		}
	case E_RFST_DISCOVERY:
	case E_RFST_LISTEN_SLEEP:
		{
			//Send deactivate command
			cmd.deactivate.type = NCI_DEACTIVATE_TYPE_IDLE_MODE;
			status = nci_send_cmd(p_nci_rf_sm->h_nci, NCI_OPCODE_RF_DEACTIVATE_CMD, &cmd, NULL, 0, hndlr_rf_deactivate_rsp, p_nci_rf_sm);

			next_state = E_RFST_DEACTIVATING_2_IDLE;
			break;
		}
	default:
		{
			OSA_ASSERT(NFC_FALSE && "nci_rm_sm:evt_hndlr_rf_deactivate_cmd");
		}
	}

	return next_state;
}

nfc_u32 handle_deactivate_states(nfc_handle_t h_nci_sm)
{
	nci_rf_sm_s* p_nci_rf_sm = (nci_rf_sm_s*)h_nci_sm;
	nfc_u32 next_state = E_RFST_IDLE;
	nci_status_e status = NCI_STATUS_OK;

	if(NULL != p_nci_rf_sm->h_nfcee_req_timer)
	{
		osa_timer_stop(p_nci_rf_sm->h_nfcee_req_timer);
		p_nci_rf_sm->h_nfcee_req_timer = NULL;
	}

	nci_sm_set_curr_state(p_nci_rf_sm->h_sm, E_RFST_IDLE);

	if(p_nci_rf_sm->card_proto_mask != 0)
	{
		status = send_core_set_config(p_nci_rf_sm);
		if(NFC_RES_OK == status)
		{
			next_state = E_RFST_W4_CORE_SET_CONFIG_RSP;
		}
		else
		{
			//TBD - add log with error
		}
	}
	else if(p_nci_rf_sm->reader_proto_mask != 0)
	{
		status = send_discover_cmd(p_nci_rf_sm);
		if(NFC_RES_OK == status)
		{
			next_state = E_RFST_W4_DISCOVERY;
		}
		else
		{
			//TBD - add log with error
		}
	}
	else if(p_nci_rf_sm->nfcee_card_proto_mask != 0 || p_nci_rf_sm->nfcee_reader_proto_mask != 0)
	{
		status = send_discover_cmd(p_nci_rf_sm);
		if(NFC_RES_OK == status)
		{
			next_state = E_RFST_W4_DISCOVERY;
		}
		else
		{
			//TBD - add log with error
		}
	}
	else
	{
		//We got here - it means that we got discovery request without valid parameters
		OSA_ASSERT(NFC_FALSE && "nci_rm_sm:handle_deactivate_states");
	}

	return next_state;
}

nfc_u32 evt_hndlr_rf_deactivate_rsp_ok(nfc_handle_t h_nci_sm, nfc_u32 curr_state)
{
	nci_rf_sm_s* p_nci_rf_sm = (nci_rf_sm_s*)h_nci_sm;
	//struct nci_context *p_nci = (struct nci_context*)p_nci_rf_sm->h_nci;
	nfc_u32 next_state = curr_state;
	nci_status_e status = NCI_STATUS_OK;

	switch(curr_state)
	{
	case E_RFST_POLL_ACTIVE:
	case E_RFST_LISTEN_ACTIVE:
		{
			next_state = E_RFST_DEACTIVATING_2_IDLE;//TBD - in the future it may be to sleep or discovery
			break;
		}
	case E_RFST_DEACTIVATING_2_IDLE:
	case E_RFST_DEACTIVATING_2_POLL_SLEEP:
	case E_RFST_DEACTIVATING_2_LISTEN_SLEEP:
		{
			next_state = E_RFST_IDLE;

			if(NULL != p_nci_rf_sm->p_config_cmd)
			{
				status = nci_send_internal_cmd(p_nci_rf_sm->h_nci, NCI_OPCODE_CORE_SET_CONFIG_CMD, p_nci_rf_sm->p_config_cmd, NULL, 0, hndlr_core_set_config_rsp, p_nci_rf_sm);
				p_nci_rf_sm->p_config_cmd = NULL;
			}
			else if(p_nci_rf_sm->card_proto_mask != 0 || p_nci_rf_sm->reader_proto_mask != 0 ||p_nci_rf_sm->nfcee_card_proto_mask != 0 || p_nci_rf_sm->nfcee_reader_proto_mask != 0)
			{
				next_state = handle_deactivate_states(h_nci_sm);
			}
			else
				nci_sm_process_evt_ex_completed(p_nci_rf_sm->h_sm, NCI_STATUS_OK);

			break;
		}

	case E_RFST_DISCOVERY:
		{
			if(NULL != p_nci_rf_sm->h_nfcee_req_timer)
			{
				osa_timer_stop(p_nci_rf_sm->h_nfcee_req_timer);
				p_nci_rf_sm->h_nfcee_req_timer = NULL;
			}

			if(p_nci_rf_sm->card_proto_mask != 0)
			{
				status = send_core_set_config(p_nci_rf_sm);
				if(NFC_RES_OK == status)
				{
					next_state = E_RFST_W4_CORE_SET_CONFIG_RSP;
				}
				else
				{
					//TBD - add log with error
				}
			}
			else if(p_nci_rf_sm->reader_proto_mask != 0)
			{
				status = send_discover_cmd(p_nci_rf_sm);
				if(NFC_RES_OK == status)
				{
					next_state = E_RFST_W4_DISCOVERY;
				}
				else
				{
					//TBD - add log with error
				}
			}
			else if(p_nci_rf_sm->nfcee_card_proto_mask != 0 || p_nci_rf_sm->nfcee_reader_proto_mask != 0)
			{
				status = send_discover_cmd(p_nci_rf_sm);
				if(NFC_RES_OK == status)
				{
					next_state = E_RFST_W4_DISCOVERY;
				}
				else
				{
					//TBD - add log with error
				}
			}
			else
			{
				//We got here - it means that we got discovery request without valid parameters
				OSA_ASSERT(NFC_FALSE && "nci_rm_sm:evt_hndlr_rf_deactivate_rsp_ok(1)");
			}
			break;
		}

	case E_RFST_DEACTIVATING_2_DISCOVERY:
		{
			next_state = curr_state;
			break;
		}
	default:
		{
			OSA_ASSERT(NFC_FALSE && "nci_rm_sm:evt_hndlr_rf_deactivate_rsp_ok(2)");
		}
	}

	return next_state;
}


nfc_u32 evt_hndlr_rf_deactivate_rsp_fail(nfc_handle_t h_nci_sm, nfc_u32 curr_state)
{
	nci_rf_sm_s* p_nci_rf_sm = (nci_rf_sm_s*)h_nci_sm;
	struct nci_context *p_nci = (struct nci_context*)p_nci_rf_sm->h_nci;
	nfc_u32 next_state = curr_state;

	OSA_ASSERT(NFC_FALSE && "nci_rm_sm:evt_hndlr_rf_deactivate_rsp_fail(1)"); //Should be checked if correct

	switch(curr_state)
	{
	case E_RFST_POLL_ACTIVE:
	case E_RFST_LISTEN_ACTIVE:
		{
			next_state = E_RFST_IDLE;
			break;
		}
	case E_RFST_DEACTIVATING_2_IDLE:
	case E_RFST_DEACTIVATING_2_DISCOVERY:
	case E_RFST_DEACTIVATING_2_POLL_SLEEP:
	case E_RFST_DEACTIVATING_2_LISTEN_SLEEP:
		{
			next_state = E_RFST_IDLE;

			if(p_nci_rf_sm->nfcee_card_proto_mask > 0)
			{
				if(NULL != p_nci_rf_sm->h_nfcee_req_timer)
				{
					osa_timer_stop(p_nci_rf_sm->h_nfcee_req_timer);
					p_nci_rf_sm->h_nfcee_req_timer = NULL;
				}
				p_nci_rf_sm->h_nfcee_req_timer = osa_timer_start(p_nci->h_osa, 1000, nfcee_req_timer_cb, p_nci_rf_sm);
			}

			nci_sm_process_evt_ex_completed(p_nci_rf_sm->h_sm, NCI_STATUS_FAILED);
		}
	default:
		{
			OSA_ASSERT(NFC_FALSE && "nci_rm_sm:evt_hndlr_rf_deactivate_rsp_fail(2)");
		}
	}

	return next_state;
}


nfc_u32 evt_hndlr_rf_deactivate_ntf_idle(nfc_handle_t h_nci_sm, nfc_u32 curr_state)
{
	nci_rf_sm_s* p_nci_rf_sm = (nci_rf_sm_s*)h_nci_sm;
	struct nci_context *p_nci = (struct nci_context*)p_nci_rf_sm->h_nci;
	nfc_u32 next_state = curr_state;

	switch(curr_state)
	{
	case E_RFST_IDLE:
		{
			if(p_nci_rf_sm->nfcee_card_proto_mask > 0)
			{
				if(NULL != p_nci_rf_sm->h_nfcee_req_timer)
				{
					osa_timer_stop(p_nci_rf_sm->h_nfcee_req_timer);
					p_nci_rf_sm->h_nfcee_req_timer = NULL;
				}
				p_nci_rf_sm->h_nfcee_req_timer = osa_timer_start(p_nci->h_osa, 1000, nfcee_req_timer_cb, p_nci_rf_sm);
			}

			//This can happen, if there was a race between deactivate command sent to FW and activated NTF received from them.
			//Scenario: Send deactivate cmd, than activated ntf received, that dectivater rsp received -> this will move us to idle, but FW already was
			//in active state, this will cause deacitvate ntf to be issued
			nci_sm_process_evt_ex_completed(p_nci_rf_sm->h_sm, NCI_STATUS_OK);
			break;
		}
	case E_RFST_DEACTIVATING_2_IDLE:
	case E_RFST_DEACTIVATING_2_DISCOVERY:
	case E_RFST_DEACTIVATING_2_POLL_SLEEP:
	case E_RFST_DEACTIVATING_2_LISTEN_SLEEP:
		{
			nci_status_e status = NCI_STATUS_OK;

			next_state = E_RFST_IDLE;


			if(NULL != p_nci_rf_sm->p_config_cmd)
			{
				status = nci_send_internal_cmd(p_nci_rf_sm->h_nci, NCI_OPCODE_CORE_SET_CONFIG_CMD, p_nci_rf_sm->p_config_cmd, NULL, 0, hndlr_core_set_config_rsp, p_nci_rf_sm);
				p_nci_rf_sm->p_config_cmd = NULL;
			}
			else if(p_nci_rf_sm->card_proto_mask != 0 || p_nci_rf_sm->reader_proto_mask != 0 ||p_nci_rf_sm->nfcee_card_proto_mask != 0 || p_nci_rf_sm->nfcee_reader_proto_mask != 0)
			{
				next_state = handle_deactivate_states(h_nci_sm);
			}
			else
			{
				if(p_nci_rf_sm->nfcee_card_proto_mask > 0)
				{
					if(NULL != p_nci_rf_sm->h_nfcee_req_timer)
					{
						osa_timer_stop(p_nci_rf_sm->h_nfcee_req_timer);
						p_nci_rf_sm->h_nfcee_req_timer = NULL;
					}
					p_nci_rf_sm->h_nfcee_req_timer = osa_timer_start(p_nci->h_osa, 1000, nfcee_req_timer_cb, p_nci_rf_sm);
				}

				nci_sm_process_evt_ex_completed(p_nci_rf_sm->h_sm, NCI_STATUS_OK);
			}
			break;
		}
	case E_RFST_W4_DISCOVERY:
		{
			next_state = curr_state;
			break;
		}

	default:
		{
			OSA_ASSERT(NFC_FALSE && "nci_rm_sm:evt_hndlr_rf_deactivate_ntf_idle");
		}
	}

	return next_state;
}


nfc_u32 evt_hndlr_rf_deactivate_ntf_sleep(nfc_handle_t h_nci_sm, nfc_u32 curr_state)
{
	nfc_u32 next_state = curr_state;

	OSA_UNUSED_PARAMETER(h_nci_sm);

	switch(curr_state)
	{
	case E_RFST_LISTEN_ACTIVE:
		{
			next_state =  E_RFST_LISTEN_SLEEP;
			break;
		}

	default:
		{
			OSA_ASSERT(NFC_FALSE && "nci_rm_sm:evt_hndlr_rf_deactivate_ntf_sleep");
		}
	}

	return next_state;
}


nfc_u32 evt_hndlr_rf_deactivate_ntf_sleep_af(nfc_handle_t h_nci_sm, nfc_u32 curr_state)
{
	nfc_u32 next_state = curr_state;

	OSA_UNUSED_PARAMETER(h_nci_sm);

	switch(curr_state)
	{
	case E_RFST_LISTEN_ACTIVE:
		{
			next_state =  E_RFST_LISTEN_SLEEP;
			break;
		}

	default:
		{
			OSA_ASSERT(NFC_FALSE && "nci_rm_sm:evt_hndlr_rf_deactivate_ntf_sleep_af");
		}
	}

	return next_state;
}


nfc_u32 evt_hndlr_rf_deactivate_ntf_discovery(nfc_handle_t h_nci_sm, nfc_u32 curr_state)
{
	nci_rf_sm_s* p_nci_rf_sm = (nci_rf_sm_s*)h_nci_sm;
	nfc_u32 next_state = curr_state;

	switch(curr_state)
	{
	case E_RFST_IDLE:
		{
			if(NULL != p_nci_rf_sm->h_nfcee_req_timer)
			{
				osa_timer_stop(p_nci_rf_sm->h_nfcee_req_timer);
				p_nci_rf_sm->h_nfcee_req_timer = NULL;
			}
			nci_sm_process_evt_ex_completed(p_nci_rf_sm->h_sm, NCI_STATUS_OK);
			break;
		}
	case E_RFST_DEACTIVATING_2_IDLE:
	case E_RFST_DEACTIVATING_2_POLL_SLEEP:
	case E_RFST_DEACTIVATING_2_LISTEN_SLEEP:
		{
			next_state = E_RFST_IDLE;
			nci_sm_process_evt_ex_completed(p_nci_rf_sm->h_sm, NCI_STATUS_FAILED);
		}

	case E_RFST_DEACTIVATING_2_DISCOVERY:
		{
			nci_sm_process_evt_ex_completed(p_nci_rf_sm->h_sm, NCI_STATUS_OK);
			next_state = E_RFST_DISCOVERY;
			break;
		}
	case E_RFST_POLL_ACTIVE:
	case E_RFST_LISTEN_ACTIVE:
	case E_RFST_LISTEN_SLEEP:
		{
			next_state = E_RFST_DISCOVERY;
			break;
		}

	default:
		{
			OSA_ASSERT(NFC_FALSE && "nci_rm_sm:evt_hndlr_rf_deactivate_ntf_discovery");
		}
	}

	return next_state;
}

nfc_u32 evt_hndlr_core_set_config_cmd(nfc_handle_t h_nci_sm, nfc_u32 curr_state)
{
	nci_rf_sm_s* p_nci_rf_sm = (nci_rf_sm_s*)h_nci_sm;
	nfc_u32 next_state = curr_state;
	nci_status_e status;
	nci_cmd_param_u	cmd;

	switch(curr_state)
	{
	case E_RFST_IDLE:
		{
			status = nci_send_internal_cmd(p_nci_rf_sm->h_nci, NCI_OPCODE_CORE_SET_CONFIG_CMD, p_nci_rf_sm->p_config_cmd, NULL, 0, hndlr_core_set_config_rsp, p_nci_rf_sm);
			p_nci_rf_sm->p_config_cmd = NULL;
			next_state = E_RFST_IDLE;
			break;
		}
	case E_RFST_DISCOVERY:
		{
			//Send deactivate command
			cmd.deactivate.type = NCI_DEACTIVATE_TYPE_IDLE_MODE;
			status = nci_send_cmd(p_nci_rf_sm->h_nci, NCI_OPCODE_RF_DEACTIVATE_CMD, &cmd, NULL, 0, hndlr_rf_deactivate_rsp, p_nci_rf_sm);

			next_state = E_RFST_DEACTIVATING_2_IDLE;

			break;
		}
	default:
		{
			osa_mem_free(p_nci_rf_sm->p_config_cmd);
			nci_sm_process_evt_ex_completed(p_nci_rf_sm->h_sm, NCI_STATUS_FAILED);
		}
	}

	return next_state;
}

nfc_u32 evt_hndlr_core_set_config_rsp_ok(nfc_handle_t h_nci_sm, nfc_u32 curr_state)
{
	nci_rf_sm_s* p_nci_rf_sm = (nci_rf_sm_s*)h_nci_sm;
	nfc_u32 next_state = curr_state;

	switch(curr_state)
	{
	case E_RFST_IDLE:
		{
			next_state = E_RFST_IDLE;
			nci_sm_process_evt_ex_completed(p_nci_rf_sm->h_sm, NCI_STATUS_OK);
			break;
		}

	case E_RFST_W4_CORE_SET_CONFIG_RSP:
		{
			//If proto masks are 0, it means we got deactivate command in between - so we need to wait for deactivate response, and state is
			// wait for deactivate rsp.
			if(p_nci_rf_sm->card_proto_mask == 0 && p_nci_rf_sm->reader_proto_mask == 0)
			{
				next_state = E_RFST_DEACTIVATING_2_IDLE; //TBD - when other deactivation types are available - should be changed
			}
			else
			{
				next_state = E_RFST_W4_DISCOVERY;
				send_discover_cmd(p_nci_rf_sm);
			}
			break;
		}
	default:
		{
			OSA_ASSERT(NFC_FALSE && "nci_rm_sm:evt_hndlr_core_set_config_rsp_ok");
		}
	}

	return next_state;
}


nfc_u32 evt_hndlr_core_set_config_rsp_fail(nfc_handle_t h_nci_sm, nfc_u32 curr_state)
{
	nci_rf_sm_s* p_nci_rf_sm = (nci_rf_sm_s*)h_nci_sm;
	nfc_u32 next_state = curr_state;

	switch(curr_state)
	{
	case E_RFST_IDLE:
	case E_RFST_W4_CORE_SET_CONFIG_RSP:
		{
			next_state = E_RFST_IDLE;
			nci_sm_process_evt_ex_completed(p_nci_rf_sm->h_sm, NCI_STATUS_FAILED);
			break;
		}
	default:
		{
			OSA_ASSERT(NFC_FALSE && "nci_rm_sm:evt_hndlr_core_set_config_rsp_fail");
		}
	}

	return next_state;
}


nfc_u32 evt_hndlr_rf_nfcee_action_ntf(nfc_handle_t h_nci_sm, nfc_u32 curr_state)
{
	nci_rf_sm_s* p_nci_rf_sm = (nci_rf_sm_s*)h_nci_sm;
	nfc_u32 next_state = curr_state;

	switch(curr_state)
	{
	case E_RFST_DEACTIVATING_2_IDLE:
	case E_RFST_LISTEN_ACTIVE:
		{
			osa_report(ERROR, (	"RF SM -> RF_NFCEE_ACTION_NTF: ID = %d, trigger = %d", p_nci_rf_sm->curr_ntf.rf_nfcee_action.id, p_nci_rf_sm->curr_ntf.rf_nfcee_action.trigger));
			next_state = curr_state;
			break;
		}

	default:
		{
			OSA_ASSERT(NFC_FALSE && "nci_rm_sm:evt_hndlr_rf_nfcee_action_ntf");
		}
	}

	return next_state;
}

void nfcee_req_timer_cb(nfc_handle_t h_nci_rf_sm)
{
	nci_rf_sm_s* p_nci_rf_sm = (nci_rf_sm_s*)h_nci_rf_sm;
	//struct nci_context *p_nci = (struct nci_context*)p_nci_rf_sm->h_nci;

	osa_report(ERROR, ("NFCEE request timer Expired"));
	p_nci_rf_sm->h_nfcee_req_timer = NULL;

	if(E_RFST_IDLE == nci_sm_get_curr_state(p_nci_rf_sm->h_sm))
		nci_sm_process_evt(p_nci_rf_sm->h_sm, E_EVT_RF_DISCOVER_CMD);
	else if(E_RFST_DISCOVERY == nci_sm_get_curr_state(p_nci_rf_sm->h_sm))
	{

			nci_sm_process_evt(p_nci_rf_sm->h_sm, E_EVT_RF_DEACTIVATE_CMD);
	}

}
nfc_u32 nfcee_remove_discovery_request(nfc_handle_t h_nci, nfc_u8 nfcee_id)
{
	struct nci_context *p_nci = (struct nci_context*)h_nci;
	nci_rf_sm_s* p_nci_rf_sm = p_nci->h_nci_rf_sm;
	nfc_u8 j = 0;
	nfc_u8 i = 0;

	/*First - find relevant NFCEE.*/
	for(j = 0; j < p_nci_rf_sm->num_nfcees; j++)
	{
		if(p_nci_rf_sm->nfcee_discovery_req_info[j].nfcee_id == nfcee_id)
			break;
	}

	if(j == p_nci_rf_sm->num_nfcees)
	{
		/*If we got here - it means that discovery request holds invalid NFCEE ID*/
		OSA_ASSERT("If we got here - it means that remove request holds invalid NFCEE ID"&&NFC_FALSE);
	}

	if(0 == p_nci_rf_sm->nfcee_card_proto_mask)
	{
		//this can happen if mode set off cmd was issued during init process
		return NCI_STATUS_OK;
	}
	p_nci_rf_sm->nfcee_discovery_req_info[j].isActive = NFC_FALSE; //TBD - this is incorrect, should be changed after nfcee handling reimplementation


	for(i = 0; i < NUM_OF_TECH_AND_MODE_LISTEN_TECHS; i++)
	{
		if(NFC_TRUE == p_nci_rf_sm->nfcee_discovery_req_info[j].requested_tech_and_mode[i])
		{
			p_nci_rf_sm->nfcee_discovery_req_info[j].num_tech_and_mode_requests--;
			p_nci_rf_sm->nfcee_card_proto_mask--;
			p_nci_rf_sm->nfcee_discovery_req_info[j].num_protocol_requests--;
		}
		p_nci_rf_sm->nfcee_discovery_req_info[j].requested_tech_and_mode[i] = NFC_FALSE;
	}

	osa_mem_set(p_nci_rf_sm->nfcee_discovery_req_info[j].requested_protocol, NFC_FALSE, MAX_SUPPORTED_PROTOCOLS_NUM*sizeof(nfc_u8));

	OSA_ASSERT(p_nci_rf_sm->nfcee_card_proto_mask >= 0);

	if(NULL != p_nci_rf_sm->h_nfcee_req_timer)
	{
		osa_timer_stop(p_nci_rf_sm->h_nfcee_req_timer);
		p_nci_rf_sm->h_nfcee_req_timer = NULL;
	}

	if(p_nci_rf_sm->nfcee_card_proto_mask > 0)
		p_nci_rf_sm->h_nfcee_req_timer = osa_timer_start(p_nci->h_osa, 1000, nfcee_req_timer_cb, p_nci_rf_sm);
	else
		nci_sm_process_evt(p_nci_rf_sm->h_sm, E_EVT_RF_DEACTIVATE_CMD);

	return NCI_STATUS_OK;
}


nfc_u32 evt_hndlr_rf_nfcee_discovery_req_ntf(nfc_handle_t h_nci_sm, nfc_u32 curr_state)
{
	nci_rf_sm_s* p_nci_rf_sm = (nci_rf_sm_s*)h_nci_sm;
	struct nci_context *p_nci = (struct nci_context*)p_nci_rf_sm->h_nci;
	nfc_u32 next_state = curr_state;
	nfc_u8 j = 0;
	nfc_u8 i = 0;

	/*First - find relevant NFCEE.*/
	for(j = 0; j < p_nci_rf_sm->num_nfcees; j++)
	{
		if(p_nci_rf_sm->nfcee_discovery_req_info[j].nfcee_id == p_nci_rf_sm->curr_ntf.rf_nfcee_discovery_req.tlvs_arr[0].nfceeID)
			break;
	}

	if(j == p_nci_rf_sm->num_nfcees)
	{
		/*If we got here - it means that discovery request holds invalid NFCEE ID*/
		OSA_ASSERT("If we got here - it means that discovery request holds invalid NFCEE ID"&&NFC_FALSE);
	}


	/* Update requests table */
	for(i = 0; i < p_nci_rf_sm->curr_ntf.rf_nfcee_discovery_req.num_tlvs; i++)
	{
		switch(p_nci_rf_sm->curr_ntf.rf_nfcee_discovery_req.tlvs_arr[i].discovery_request_type)
		{
		case NCI_NFCEE_DISCOVERY_REQUEST_TYPE_ADD:
			{
				nfc_u8 requested_tech_indx = p_nci_rf_sm->curr_ntf.rf_nfcee_discovery_req.tlvs_arr[i].rf_tech_and_mode - NFC_A_PASSIVE_LISTEN_MODE;
				nfc_u8 requested_protocol_indx = p_nci_rf_sm->curr_ntf.rf_nfcee_discovery_req.tlvs_arr[i].rf_protocol - NCI_RF_PROTOCOL_UNKNOWN;

				p_nci_rf_sm->nfcee_discovery_req_info[j].isActive = NFC_TRUE; //TBD - this is incorrect, should be changed after nfcee handling reimplementation

				p_nci_rf_sm->nfcee_discovery_req_info[j].num_tech_and_mode_requests++;
				p_nci_rf_sm->nfcee_discovery_req_info[j].requested_tech_and_mode[requested_tech_indx] = NFC_TRUE;
				p_nci_rf_sm->nfcee_discovery_req_info[j].num_protocol_requests++;
				p_nci_rf_sm->nfcee_discovery_req_info[j].requested_protocol[requested_protocol_indx] = NFC_TRUE;

				p_nci_rf_sm->nfcee_card_proto_mask++;
				OSA_ASSERT(p_nci_rf_sm->nfcee_card_proto_mask >= 0);
				break;
			}
		case NCI_NFCEE_DISCOVERY_REQUEST_TYPE_REMOVE:
			{
				nfc_u8 requested_tech_indx = p_nci_rf_sm->curr_ntf.rf_nfcee_discovery_req.tlvs_arr[i].rf_tech_and_mode - NFC_A_PASSIVE_LISTEN_MODE;
				nfc_u8 requested_protocol_indx = p_nci_rf_sm->curr_ntf.rf_nfcee_discovery_req.tlvs_arr[i].rf_protocol - NCI_RF_PROTOCOL_UNKNOWN;

				if(0 == p_nci_rf_sm->nfcee_card_proto_mask)
				{
					//this can happen if mode set off cmd was issued during init process
					break;
				}
				p_nci_rf_sm->nfcee_discovery_req_info[j].isActive = NFC_FALSE; //TBD - this is incorrect, should be changed after nfcee handling reimplementation

				p_nci_rf_sm->nfcee_discovery_req_info[j].num_tech_and_mode_requests--;
				p_nci_rf_sm->nfcee_discovery_req_info[j].requested_tech_and_mode[requested_tech_indx] = NFC_FALSE;
				p_nci_rf_sm->nfcee_discovery_req_info[j].num_protocol_requests--;
				p_nci_rf_sm->nfcee_discovery_req_info[j].requested_protocol[requested_protocol_indx] = NFC_FALSE;

				p_nci_rf_sm->nfcee_card_proto_mask--;
				OSA_ASSERT(p_nci_rf_sm->nfcee_card_proto_mask >= 0);
				break;
			}

		default:
			OSA_ASSERT(NFC_FALSE && "nci_rm_sm:evt_hndlr_rf_nfcee_discovery_req_ntf");

		}
	}

	/* Handle only in IDLE state - should generate discovery command with empty request, in any other state - just ignore it*/
	/* If we are in idle state - start timer, to eliminate possibility for transient situations.*/
	/* When timer expires and we are still in IDLE state - send discovery command*/

	/* In other cases - it is NAL responsibility to handle this event - i.e. Stop/Start RF */

	switch(curr_state)
	{
	case E_RFST_IDLE:
	case E_RFST_DISCOVERY:
	case E_RFST_W4_DISCOVERY:
		{
			if(NULL != p_nci_rf_sm->h_nfcee_req_timer)
			{
				osa_timer_stop(p_nci_rf_sm->h_nfcee_req_timer);
				p_nci_rf_sm->h_nfcee_req_timer = NULL;
			}

			if(p_nci_rf_sm->nfcee_card_proto_mask > 0)
				p_nci_rf_sm->h_nfcee_req_timer = osa_timer_start(p_nci->h_osa, 1000, nfcee_req_timer_cb, p_nci_rf_sm);
			else//TBD - check if correct
			{
				nci_sm_process_evt(p_nci_rf_sm->h_sm, E_EVT_RF_DEACTIVATE_CMD);
				next_state = E_RFST_DEACTIVATING_2_IDLE;
			}



			break;
		}


	default:
		{
			osa_report(ERROR, ("NFCEE discovery request received in state!!!! state %d", curr_state));
		}
	}

	return next_state;
}


nfc_u32 evt_hndlr_core_intf_error_ntf(nfc_handle_t h_nci_sm, nfc_u32 curr_state)
{
	nci_rf_sm_s* p_nci_rf_sm = (nci_rf_sm_s*)h_nci_sm;
	nfc_u32 next_state = curr_state;

	osa_report(ERROR, (	"RF SM -> CORE_INTF_ERR_NTF: state = %s code = 0x%x on connection %d",
						rf_state[curr_state].p_name, p_nci_rf_sm->curr_ntf.core_iface_error.status, p_nci_rf_sm->curr_ntf.core_iface_error.connID));

#if 0 //May be used in next NCI version
	switch(curr_state)
	{

	case E_RFST_DISCOVERY:
	case E_RFST_W4_HOST_SELECT:
	case E_RFST_LISTEN_ACTIVE:
	case E_RFST_POLL_ACTIVE:
		{
			next_state = curr_state;
			break;
		}

	default:
		{
			OSA_ASSERT(NFC_FALSE && "nci_rm_sm:evt_hndlr_core_intf_error_ntf");
		}
	}
#endif

	return next_state;
}

nfc_u8 nci_rf_sm_is_cmd_allowed(struct nci_context* p_nci, nfc_u16 opcode)
{
	nci_rf_sm_s* p_nci_rf_sm = (nci_rf_sm_s*)(p_nci->h_nci_rf_sm);
	nfc_u8 isAllowed = NFC_TRUE;

	if((NCI_OPCODE_CORE_SET_CONFIG_CMD == opcode) && (E_RFST_IDLE != nci_sm_get_curr_state(p_nci_rf_sm->h_sm)))
	{
		isAllowed = NFC_FALSE;
	}

	return isAllowed;
}

nci_status_e nci_rf_sm_configure_listen_routing(nfc_handle_t h_nci, configure_listen_routing_t* p_config)
{
	struct nci_context* p_nci = (struct nci_context*)h_nci;
	nci_rf_sm_s* p_nci_rf_sm = (nci_rf_sm_s*)p_nci->h_nci_rf_sm;
	nci_status_e status = NCI_STATUS_OK;

	if(NULL != p_config)
	{
		osa_mem_copy(&p_nci_rf_sm->routing_config, p_config, sizeof(configure_listen_routing_t));
	}
	else
		status = NCI_STATUS_INVALID_PARAM;

	return status;
}

nfc_u8 convert_tech_and_mode_to_rf_tech(nfc_u8 tech_and_mode)
{
	switch(tech_and_mode)
	{
	case NFC_A_PASSIVE_LISTEN_MODE:
		return NCI_NFC_RF_TECHNOLOGY_A;
	case NFC_B_PASSIVE_LISTEN_MODE:
		return NCI_NFC_RF_TECHNOLOGY_B;
	case NFC_F_PASSIVE_LISTEN_MODE:
		return NCI_NFC_RF_TECHNOLOGY_F;
	case NFC_15693_PASSIVE_LISTEN_MODE:
		return NCI_NFC_RF_TECHNOLOGY_15693;
	}

	return NCI_INVALID_TECH_AND_MODE;
}

nfc_u8 convert_rf_tech_to_hosts_priority_tech(nfc_u8 tech_and_mode)
{
	switch(tech_and_mode)
	{
	case NCI_NFC_RF_TECHNOLOGY_A:
		return ROUTE_RF_TECHNOLOGY_A;
	case NCI_NFC_RF_TECHNOLOGY_B:
		return ROUTE_RF_TECHNOLOGY_B;
	case NCI_NFC_RF_TECHNOLOGY_F:
		return ROUTE_RF_TECHNOLOGY_F;
	case NCI_NFC_RF_TECHNOLOGY_15693:
		return ROUTE_RF_TECHNOLOGY_15693;
	}

	return ROUTE_RF_TECHNOLOGY_INVALID;
}

nci_status_e nci_rf_sm_build_and_send_listen_routing_cmd(nfc_handle_t h_nci_sm)
{
	nci_rf_sm_s* p_nci_rf_sm = (nci_rf_sm_s*)h_nci_sm;
	nci_status_e status = NCI_STATUS_OK;
	struct nci_cmd_rf_set_listen_mode_routing table = {0};
	nfc_s8 i = 0;
	nfc_u8 num_routing_entries = 0;

	struct nci_tlv tlvs [NCI_NFC_RF_NUM_TECHNOLOGIES*2];//TBD - add additional tlvs when protocol and AID routing available
	listen_mode_routing_type_tech_value_t listen_mode_routing_type_tech_value[NCI_NFC_RF_NUM_TECHNOLOGIES];

	if(0 == p_nci_rf_sm->routing_config.hosts_priority_arr_size)
	{
		osa_report(ERROR, (	"RF SM -> Routing table Not configured!!!"));
		return NCI_STATUS_FAILED;
	}

	osa_mem_set(listen_mode_routing_type_tech_value, 0x00, NCI_NFC_RF_NUM_TECHNOLOGIES*sizeof(listen_mode_routing_type_tech_value_t));

	table.p_routing_entries = tlvs;

#if 0	//TBD - add later
	if(p_nci_rf_sm->nfcc_features & NCI_FEATURE_LISTEN_MODE_ROUTING_AID)
	{
		//First add AID routing entires to the table
		for(i = 0; i < p_nci_rf_sm->routing_config.aids_arr_size; i++)
		{
			//TBD
		}
	}

	if(p_nci_rf_sm->nfcc_features & NCI_FEATURE_LISTEN_MODE_ROUTING_PROTOCOL)
	{
		//Configure all protocols that were not configured to be routed to host.
		for(i = 0; i < MAX_SUPPORTED_PROTOCOLS_NUM; i++)
		{
			//TBD
		}

		//Go over all configured hosts and build protocol entries
		for(i = 0; i < p_nci_rf_sm->routing_config.hosts_priority_arr_size; i++)
		{
			//TBD
		}
	}
#endif

	if(p_nci_rf_sm->nfcc_features & NCI_FEATURE_LISTEN_MODE_ROUTING_TECH)
	{
		//Configure all techs. to host. only for SWITCHED_ON. Configuring host For SWITCHED_OFF is meaningless.
		//The configuration below will eventually cause DH to be configured to all techs that requested by user
		//together with all techs that no one requested
		for(i = 0; i < NCI_NFC_RF_NUM_TECHNOLOGIES; i++)
		{
			listen_mode_routing_type_tech_value[i].route_to = NCI_DH_NFCEE_ID; //all unrequested techs. will be routed to host
			listen_mode_routing_type_tech_value[i].power_state = NCI_ROUTING_POWER_STATE_SWITCHED_ON;
			listen_mode_routing_type_tech_value[i].tech = i;

			/* Currently 15693 tech is not supported by our device. Don't include it on the routing table */
			if (i!= NCI_NFC_RF_TECHNOLOGY_15693)
			{
				table.p_routing_entries[num_routing_entries].type = NCI_ROUTING_TYPE_TECH;
				table.p_routing_entries[num_routing_entries].length = 3 * sizeof(nfc_u8);
				table.p_routing_entries[num_routing_entries].p_value = (nfc_u8*)(&listen_mode_routing_type_tech_value[i]);
				num_routing_entries++;
			}
		}

		//Go over all configured hosts and build tech entries
		//Since we go from lowest to highest, highest priority will override lowest
		for(i = p_nci_rf_sm->routing_config.hosts_priority_arr_size - 1; i >= 0; i--)
		{
			nfc_u8 j = 0;
			nfc_u8 k = 0;
			nfc_u8 requested_rf_tech = 0xff;

			if(NCI_DH_NFCEE_ID != p_nci_rf_sm->routing_config.hosts_priority_arr[i].nfcee_id)
			{
				for(j = 0; j < p_nci_rf_sm->num_nfcees; j++)
				{
					if( (NFC_TRUE == p_nci_rf_sm->nfcee_discovery_req_info[j].isActive) &&
						(p_nci_rf_sm->nfcee_discovery_req_info[j].nfcee_id == p_nci_rf_sm->routing_config.hosts_priority_arr[i].nfcee_id))
					{
						break;
					}
				}

				if(j == p_nci_rf_sm->num_nfcees)
				{
					/*If we got here - it means that this host didn't send any discovery request or it is inactive*/
					continue;
				}

				//Current lowest priority host is located in i
				//Now we need to add its tech. requests to routing table.
				for(k = 0; k < NUM_OF_TECH_AND_MODE_LISTEN_TECHS; k++)
				{
					requested_rf_tech = convert_tech_and_mode_to_rf_tech(k + NFC_A_PASSIVE_LISTEN_MODE);
					if((NFC_TRUE == p_nci_rf_sm->nfcee_discovery_req_info[j].requested_tech_and_mode[k]) &&
					   (p_nci_rf_sm->routing_config.hosts_priority_arr[i].techs & convert_rf_tech_to_hosts_priority_tech(requested_rf_tech)))
					{
						//Since, command table entries already configured above - we just need to override the values
						listen_mode_routing_type_tech_value[requested_rf_tech].route_to = p_nci_rf_sm->nfcee_discovery_req_info[j].nfcee_id;
						listen_mode_routing_type_tech_value[requested_rf_tech].power_state = NCI_ROUTING_POWER_STATE_SWITCHED_ON;
						if((p_nci_rf_sm->nfcc_features & NCI_FEATURE_SWITCHED_OFF) && (NFC_TRUE == p_nci_rf_sm->routing_config.isSwitchOffModeSupported))
						{
							listen_mode_routing_type_tech_value[requested_rf_tech].power_state |= NCI_ROUTING_POWER_STATE_SWITCHED_OFF;
						}
						listen_mode_routing_type_tech_value[requested_rf_tech].tech = requested_rf_tech;
					}
				}
			}
			else
			{
				//Assume that host can support all techs.
				for(k = 0; k < NUM_OF_TECH_AND_MODE_LISTEN_TECHS; k++)
				{
					requested_rf_tech = convert_tech_and_mode_to_rf_tech(k + NFC_A_PASSIVE_LISTEN_MODE);
					if(p_nci_rf_sm->routing_config.hosts_priority_arr[i].techs & convert_rf_tech_to_hosts_priority_tech(requested_rf_tech))
					{
						//Since, command table entries already configured above - we just need to override the values
						listen_mode_routing_type_tech_value[requested_rf_tech].route_to = NCI_DH_NFCEE_ID;
						listen_mode_routing_type_tech_value[requested_rf_tech].power_state = NCI_ROUTING_POWER_STATE_SWITCHED_ON;
						listen_mode_routing_type_tech_value[requested_rf_tech].tech = requested_rf_tech;
					}
				}
			}
		}
	}

	table.num_of_routing_entries = num_routing_entries;

	status = nci_send_cmd(p_nci_rf_sm->h_nci, NCI_OPCODE_RF_SET_LISTEN_MODE_ROUTING_CMD, (nci_cmd_param_u *)&table, NULL, 0, NULL, NULL);
	osa_report(INFORMATION, ("nci_rf_sm_build_and_send_listen_routing_cmd: Routing command sent. status = 0x%x", status));

	return status;
}

void nci_rf_sm_set_nfcc_features(nfc_handle_t h_nci, nfc_u32 nfcc_features)
{
	struct nci_context* p_nci = (struct nci_context*)h_nci;
	nci_rf_sm_s* p_nci_rf_sm = (nci_rf_sm_s*)p_nci->h_nci_rf_sm;

	p_nci_rf_sm->nfcc_features = nfcc_features;

}

#endif //#if (NCI_VERSION_IN_USE==NCI_VER_1)

/* EOF */
