/*
 * nci_ver_1\ncix_rf_mgmt.c
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

struct ncix_rf_mgmt
{
	nfc_handle_t	h_nci;
	nfc_handle_t	h_ncix;
	nfc_u16		card_proto_mask;
	nfc_u16		reader_proto_mask;
};






/***************************************/
/*** Module Input Routines           ***/
/***************************************/

nfc_u32 hndlr_op_rf_enable_cb(nfc_handle_t h_rf_mgmt, nfc_u32 status)
{
	struct ncix_rf_mgmt *p_rf_mgmt = (struct ncix_rf_mgmt *)h_rf_mgmt;
	ncix_op_complete(p_rf_mgmt->h_ncix, status);

	return NCI_STATUS_OK;
}
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
		nci_rf_sm_configure_discovery(p_rf_mgmt->h_nci, p_rf_mgmt->card_proto_mask, p_rf_mgmt->reader_proto_mask);
		nci_rf_sm_process_evt_ex(p_rf_mgmt->h_nci, E_EVT_RF_DISCOVER_CMD, hndlr_op_rf_enable_cb, h_rf_mgmt);
	}
	else
	{
		OSA_ASSERT(0);
	}
}

nfc_u32 hndlr_op_rf_disable_cb(nfc_handle_t h_rf_mgmt, nfc_u32 status)
{
	struct ncix_rf_mgmt *p_rf_mgmt = (struct ncix_rf_mgmt *)h_rf_mgmt;
	ncix_op_complete(p_rf_mgmt->h_ncix, status);

	return NCI_STATUS_OK;
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
	nci_rf_sm_configure_discovery(p_rf_mgmt->h_nci, p_rf_mgmt->card_proto_mask, p_rf_mgmt->reader_proto_mask);
	nci_rf_sm_process_evt_ex(p_rf_mgmt->h_nci, E_EVT_RF_DEACTIVATE_CMD, hndlr_op_rf_disable_cb, h_rf_mgmt);
}


nfc_u32 hndlr_op_rf_config_cb(nfc_handle_t h_rf_mgmt, nfc_u32 status)
{
	struct ncix_rf_mgmt *p_rf_mgmt = (struct ncix_rf_mgmt *)h_rf_mgmt;
	ncix_op_complete(p_rf_mgmt->h_ncix, status);

	return NCI_STATUS_OK;
}

void hndlr_op_rf_config(nfc_handle_t h_rf_mgmt, op_param_u *pu_param)
{
	struct ncix_rf_mgmt *p_rf_mgmt = (struct ncix_rf_mgmt *)h_rf_mgmt;
	nci_status_e status = NCI_STATUS_FAILED;

	if(pu_param)
	{
		status = nci_rf_sm_set_config_params(p_rf_mgmt->h_nci, pu_param->config.p_cmd);
		if (NCI_STATUS_OK == status)
		{
			nci_rf_sm_process_evt_ex(p_rf_mgmt->h_nci, E_EVT_CORE_SET_CONFIG_CMD, hndlr_op_rf_config_cb, h_rf_mgmt);
		}
		else
		{
			osa_mem_free(pu_param->config.p_cmd);
			ncix_op_complete(p_rf_mgmt->h_ncix, status);
		}
	}
	else
	{
		OSA_ASSERT(0);
	}
}

void hndlr_op_rf_configure_listen_routing(nfc_handle_t h_rf_mgmt, op_param_u *pu_param)
{
	struct ncix_rf_mgmt *p_rf_mgmt = (struct ncix_rf_mgmt *)h_rf_mgmt;
	nci_status_e status = NCI_STATUS_FAILED;


	if(pu_param)
	{
		status = nci_rf_sm_configure_listen_routing(p_rf_mgmt->h_nci, &(pu_param->listen_routing.listen_routing_config));
		ncix_op_complete(p_rf_mgmt->h_ncix, status);

	}
	else
	{
		OSA_ASSERT(0);
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
* Input: 	h_nci - handle to main object
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
	p_rf_mgmt->card_proto_mask = 0;
	p_rf_mgmt->reader_proto_mask = 0;

	ncix_register_op_cb(h_ncix, NCI_OPERATION_RF_ENABLE, hndlr_op_rf_enable, p_rf_mgmt);
	ncix_register_op_cb(h_ncix, NCI_OPERATION_RF_DISABLE, hndlr_op_rf_disable, p_rf_mgmt);
	ncix_register_op_cb(h_ncix, NCI_OPERATION_RF_CONFIG, hndlr_op_rf_config, p_rf_mgmt);
	ncix_register_op_cb(h_ncix, NCI_OPERATION_RF_CONFIG_LISTEN_MODE_ROUTING, hndlr_op_rf_configure_listen_routing, p_rf_mgmt);

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
	osa_mem_free(h_rf_mgmt);
}

#endif //#if (NCI_VERSION_IN_USE==NCI_VER_1)

/* EOF */
