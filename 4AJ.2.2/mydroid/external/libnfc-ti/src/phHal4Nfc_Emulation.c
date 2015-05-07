/*
 * Copyright (C) 2010 NXP Semiconductors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/*!
* \file  phHal4Nfc_Emulation.c
* \brief Hal4 Emulation source.
*
* Project: NFC-FRI 1.1
*
* $Date: Wed May 26 18:03:59 2010 $
* $Author: ing07385 $
* $Revision: 1.35 $
* $Aliases: NFC_FRI1.1_WK1023_R35_1 $
*
*/

/* ---------------------------Include files ------------------------------------*/
#include <phDal4Nfc.h>
#include <phHal4Nfc.h>
#include <phHal4Nfc_Internal.h>
#include <phOsalNfc.h>

/* ------------------------------- Macros ------------------------------------*/
#define LOG_TAG               "HAL4NFC_EMUL"

/* Note : Macros required and used  only in this module to be declared here*/


/* --------------------Structures and enumerations --------------------------*/


/*Event Notification handler for emulation*/
void phHal4Nfc_HandleEmulationEvent(
						phHal4Nfc_Hal4Ctxt_t  *Hal4Ctxt,
						void *pInfo
						)
{
	phNfc_sNotificationInfo_t *psNotificationInfo =
		(phNfc_sNotificationInfo_t *) pInfo;
	
	phHal4Nfc_NotificationInfo_t uNotificationInfo = {NULL};

	hal4nfc_report((" phHal4Nfc_HandleEmulationEvent"));

	/*Pass on Event notification info from Hci to Upper layer*/
	uNotificationInfo.psEventInfo = psNotificationInfo->info;
	if(NULL != Hal4Ctxt->sUpperLayerInfo.pEventNotification)
	{
		Hal4Ctxt->sUpperLayerInfo.pEventNotification(
			Hal4Ctxt->sUpperLayerInfo.EventNotificationCtxt,
			psNotificationInfo->type,
			uNotificationInfo,
			NFCSTATUS_SUCCESS
			);
	}
	else/*No Event notification handler registered*/
	{
		/*Use default handler to notify to the upper layer*/
		if(NULL != Hal4Ctxt->sUpperLayerInfo.pDefaultEventHandler)
		{
			Hal4Ctxt->sUpperLayerInfo.pDefaultEventHandler(
				Hal4Ctxt->sUpperLayerInfo.DefaultListenerCtxt,
				psNotificationInfo->type,
				uNotificationInfo,
				NFCSTATUS_SUCCESS
				);
		}
	}
	return;
}

#if (NCI_VERSION_IN_USE==NCI_VER_1)
void nci_rf_nfcee_action_ntf_handler(nfc_handle_t h_nal, nfc_u16 opcode, nci_ntf_param_u *pu_ntf)
{
	phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt = (phHal4Nfc_Hal4Ctxt_t*)h_nal;
	phHal_sEventInfo_t sEventInfo;
	phNfc_sNotificationInfo_t sNotificationInfo;

	hal4nfc_report((" nci_rf_nfcee_action_ntf_handler, id %d, trigger %d, data_len %d",
			pu_ntf->rf_nfcee_action.id,
			pu_ntf->rf_nfcee_action.trigger,
			pu_ntf->rf_nfcee_action.supporting_data_length));

	/* SELECT command with an AID */
	if (pu_ntf->rf_nfcee_action.trigger == 0) {
		sEventInfo.eventHost = phHal_eHostController;
		sEventInfo.eventSource = phHal_ePICC_DevType;
		sEventInfo.eventType = NFC_EVT_TRANSACTION;
		sEventInfo.eventInfo.aid.length = pu_ntf->rf_nfcee_action.supporting_data_length;
		sEventInfo.eventInfo.aid.buffer = pu_ntf->rf_nfcee_action.supporting_data;

		sNotificationInfo.info = &sEventInfo;
		sNotificationInfo.status = NFCSTATUS_SUCCESS;
		sNotificationInfo.type = NFC_EVENT_NOTIFICATION;

		phHal4Nfc_HandleEmulationEvent(Hal4Ctxt, &sNotificationInfo);
	}
}
#endif

static void phHal4Nfc_NFCEE_Switch_Mode_Complete(nfc_handle_t h_nal, nfc_u16 opcode, op_rsp_param_u *p_op_rsp)
{
	phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt = (phHal4Nfc_Hal4Ctxt_t*)h_nal;
	pphHal4Nfc_GenCallback_t    pConfigCallback
								= Hal4Ctxt->sUpperLayerInfo.pConfigCallback;
	NFCSTATUS Status = NFCSTATUS_SUCCESS;

	hal4nfc_report((" phHal4Nfc_NFCEE_Switch_Mode_Complete, status = %d, new_mode = %d",
			p_op_rsp->nfcee.status, p_op_rsp->nfcee.new_mode));

	if(NULL != Hal4Ctxt->sUpperLayerInfo.pConfigCallback)
	{
	hal4nfc_report((" phHal4Nfc_NFCEE_Switch_Mode_Complete calling pConfigCallback"));

		Hal4Ctxt->Hal4NextState = eHal4StateInvalid;
		Hal4Ctxt->sUpperLayerInfo.pConfigCallback = NULL;
		(*pConfigCallback)(
#ifdef LLCP_DISCON_CHANGES
			Hal4Ctxt->sUpperLayerInfo.psUpperLayerCfgDiscCtxt,
#else /* #ifdef LLCP_DISCON_CHANGES */
			Hal4Ctxt->sUpperLayerInfo.psUpperLayerCtxt,
#endif /* #ifdef LLCP_DISCON_CHANGES */
			Status
			);
	}

	if (p_op_rsp->nfcee.new_mode == E_SWITCH_HOST_COMM)
	{
		phHal_sRemoteDevInformation_t remDev;
		phNfc_sCompletionInfo_t info;

		/* Store the NFCEE conn_id */
		Hal4Ctxt->active_conn_id = p_op_rsp->nfcee.host_comm_params.conn_id;
		hal4nfc_report((" HOST_COMM status = %d, NFCEE conn_id = %d",
			p_op_rsp->nfcee.host_comm_params.status, Hal4Ctxt->active_conn_id));

		remDev.SessionOpened = 0;
		remDev.RemDevType = phHal_eISO14443_A_PICC;
		remDev.RemoteDevInfo.Iso14443A_Info.UidLength = 0;
		remDev.RemoteDevInfo.Iso14443A_Info.Sak = ISO_14443_BITMASK;
		remDev.RemoteDevInfo.Iso14443A_Info.AtqA[0] = 0;
		remDev.RemoteDevInfo.Iso14443A_Info.AtqA[1] = 0;
		remDev.RemoteDevInfo.Iso14443A_Info.AppDataLength = 0;
		remDev.RemoteDevInfo.Iso14443A_Info.MaxDataRate = 0;
		remDev.RemoteDevInfo.Iso14443A_Info.Fwi_Sfgt = 0x40;

		info.status = NFCSTATUS_SUCCESS;
		info.info = &remDev;

		Hal4Ctxt->psADDCtxtInfo->nbr_of_devices = 0;
		phHal4Nfc_TargetDiscoveryComplete(Hal4Ctxt, &info);
	}
	else if (Hal4Ctxt->active_conn_id != 0)
	{
	hal4nfc_report((" phHal4Nfc_NFCEE_Switch_Mode_Complete revert active_conn_id to 0"));
		/* Revert back to rf static conn */
		Hal4Ctxt->active_conn_id = 0;
		phNfc_sCompletionInfo_t info;

		info.status = NFCSTATUS_SUCCESS;
		phHal4Nfc_DisconnectComplete(Hal4Ctxt, &info);
	}
}

NFCSTATUS phHal4Nfc_NFCEE_Switch_Mode(phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt,
						enum nfcee_switch_mode mode,
						int isSmx)
{
	NFCSTATUS rc = NFCSTATUS_SMART_TAG_FUNC_NOT_SUPPORTED;
	nci_status_e status;
	int i, j;
	op_param_u u_param = {0};

	hal4nfc_report((" phHal4Nfc_NFCEE_Switch_Mode, mode=%d, isSmx=%d", mode, isSmx));

	if ((isSmx == 1) && (Hal4Ctxt->is_hci == NFC_TRUE)) {
		if (mode == E_SWITCH_HOST_COMM) {
			Hal4Ctxt->is_hci_active = NFC_TRUE;
			hci_activate(Hal4Ctxt->hciHandle, (nfc_u8)Hal4Ctxt->uicc_id);
		} else if (mode == E_SWITCH_RF_COMM) {
			Hal4Ctxt->is_hci_active = NFC_FALSE;
			hci_activate(Hal4Ctxt->hciHandle, (nfc_u8)Hal4Ctxt->uicc_id);
		} else {
			Hal4Ctxt->is_hci_active = NFC_FALSE;
			hci_deactivate(Hal4Ctxt->hciHandle, (nfc_u8)Hal4Ctxt->uicc_id);
		}

		return NFCSTATUS_PENDING;
	}

	for(j = 0; j < Hal4Ctxt->nci_start_rsp.nfcee_discover_rsp.num_nfcee; j++)
	{
		nci_ntf_param_u *pu_ntf = (nci_ntf_param_u*)&(Hal4Ctxt->nci_start_rsp.nfcee_discover_ntf[j]);
		for(i = 0; i < pu_ntf->nfcee_discover.num_nfcee_interface_information_entries; i++)
		{
			if (((isSmx == 1) && ((pu_ntf->nfcee_discover.nfcee_protocols[i] == 0x00)|| (pu_ntf->nfcee_discover.nfcee_protocols[i] == 0x03)) ) || 	// APDU or transparent
				((isSmx == 0) && (pu_ntf->nfcee_discover.nfcee_protocols[i] == 0x80)))	// UICC
			{
				u_param.nfcee.new_mode = mode;

				#if (NCI_VERSION_IN_USE==NCI_VER_1)
					u_param.nfcee.nfcee_id = pu_ntf->nfcee_discover.nfcee_id;
				#else
					u_param.nfcee.target_hndl = pu_ntf->nfcee_discover.target_handle;
				#endif

				status = nci_start_operation(Hal4Ctxt->nciHandle,
							NCI_OPERATION_NFCEE_SWITCH_MODE,
							&u_param,
							NULL,
							0,
							phHal4Nfc_NFCEE_Switch_Mode_Complete,
							Hal4Ctxt);

				if (status == NCI_STATUS_OK)
				{
					rc = NFCSTATUS_PENDING;
					/* we support only 1 NFCEE at a time */
					goto exit;
				}
			}
		}
	}

exit:
	return rc;
}

/*  Switch mode from Virtual to Wired or Vice Versa for SMX.
*/
NFCSTATUS phHal4Nfc_Switch_SMX_Mode(
									phHal_sHwReference_t      *psHwReference,
									phHal_eSmartMX_Mode_t      smx_mode,
									pphHal4Nfc_GenCallback_t   pSwitchModecb,
									void                      *pContext
									)
{
	NFCSTATUS CfgStatus = NFCSTATUS_PENDING;
	phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt = NULL;

	hal4nfc_report((" phHal4Nfc_Switch_SMX_Mode, smx_mode=%d", smx_mode));

	/*NULL  checks*/
	if((NULL == psHwReference) || (NULL == pSwitchModecb))
	{
	hal4nfc_report((" phHal4Nfc_Switch_SMX_Mode 1"));
		phOsalNfc_RaiseException(phOsalNfc_e_PrecondFailed,1);
		CfgStatus = PHNFCSTVAL(CID_NFC_HAL , NFCSTATUS_INVALID_PARAMETER);
	}
	/*Check Initialised state*/
	else if((NULL == psHwReference->hal_context)
						|| (((phHal4Nfc_Hal4Ctxt_t *)
								psHwReference->hal_context)->Hal4CurrentState
										       < eHal4StateOpenAndReady)
						|| (((phHal4Nfc_Hal4Ctxt_t *)
								psHwReference->hal_context)->Hal4NextState
										       == eHal4StateClosed))
	{
	hal4nfc_report((" phHal4Nfc_Switch_SMX_Mode 2"));
		phOsalNfc_RaiseException(phOsalNfc_e_PrecondFailed,1);
		CfgStatus = PHNFCSTVAL(CID_NFC_HAL , NFCSTATUS_NOT_INITIALISED);
	}
	else
	{
	hal4nfc_report((" phHal4Nfc_Switch_SMX_Mode 3"));
		Hal4Ctxt = psHwReference->hal_context;

		/*Previous POLL Config has not completed or device is connected,
		  do not allow poll*/
		if(Hal4Ctxt->Hal4NextState == eHal4StateConfiguring)
		{
	hal4nfc_report((" phHal4Nfc_Switch_SMX_Mode 4"));
			PHDBG_INFO("Hal4:Configuration in progress.Returning status Busy");
			CfgStatus= PHNFCSTVAL(CID_NFC_HAL , NFCSTATUS_BUSY);
		}
		else if(Hal4Ctxt->Hal4CurrentState >= eHal4StateOpenAndReady)
		{
	hal4nfc_report((" phHal4Nfc_Switch_SMX_Mode 5"));
			/**If config discovery has not been called prior to this ,allocate
			   ADD Context here*/
			if (NULL == Hal4Ctxt->psADDCtxtInfo)
			{
				Hal4Ctxt->psADDCtxtInfo= (pphHal4Nfc_ADDCtxtInfo_t)
					phOsalNfc_GetMemory((uint32_t)
										(sizeof(phHal4Nfc_ADDCtxtInfo_t)));
				if(NULL != Hal4Ctxt->psADDCtxtInfo)
				{
					(void)memset(Hal4Ctxt->psADDCtxtInfo,
									0,sizeof(phHal4Nfc_ADDCtxtInfo_t));
				}
			}
			if(NULL == Hal4Ctxt->psADDCtxtInfo)
			{
	hal4nfc_report((" phHal4Nfc_Switch_SMX_Mode 6"));
				phOsalNfc_RaiseException(phOsalNfc_e_NoMemory,0);
				CfgStatus= PHNFCSTVAL(CID_NFC_HAL ,
										NFCSTATUS_INSUFFICIENT_RESOURCES);
			}
			else
			{
	hal4nfc_report((" phHal4Nfc_Switch_SMX_Mode 7"));
				/* Switch request to Wired mode */
				if(eSmartMx_Wired == smx_mode)
				{
					if(Hal4Ctxt->Hal4CurrentState
									== eHal4StateTargetConnected)
					{
						PHDBG_INFO("Hal4:In Connected state.Returning Busy");
						CfgStatus= PHNFCSTVAL(CID_NFC_HAL , NFCSTATUS_BUSY);
					}
					/*It is Mandatory to register a listener before switching
					  to wired mode*/
					else if(NULL ==
							Hal4Ctxt->sUpperLayerInfo.pTagDiscoveryNotification)
					{
						CfgStatus = PHNFCSTVAL(CID_NFC_HAL ,
							NFCSTATUS_FAILED);
					}
					else
					{
						Hal4Ctxt->psADDCtxtInfo->nbr_of_devices = 0;
						Hal4Ctxt->psADDCtxtInfo->smx_discovery = TRUE;
						/*Switch mode to wired*/
						CfgStatus = phHal4Nfc_NFCEE_Switch_Mode(Hal4Ctxt, E_SWITCH_HOST_COMM, 1);
					}
				}
				else if((eSmartMx_Default == smx_mode) || (eSmartMx_Virtual == smx_mode))
				{
					Hal4Ctxt->psADDCtxtInfo->smx_discovery = FALSE;
					/*Switch mode to virtual */
					CfgStatus = phHal4Nfc_NFCEE_Switch_Mode(Hal4Ctxt, E_SWITCH_RF_COMM, 1);
				}
		else	// eSmartMx_Off
		{
					Hal4Ctxt->psADDCtxtInfo->smx_discovery = FALSE;
					/*Switch mode to off */
					CfgStatus = phHal4Nfc_NFCEE_Switch_Mode(Hal4Ctxt, E_SWITCH_OFF, 1);
				}
				/* Change the State of the HAL only if Switch mode Returns
				   Success*/
				if ( NFCSTATUS_PENDING == CfgStatus )
				{
					Hal4Ctxt->Hal4NextState = eHal4StateConfiguring;
					Hal4Ctxt->sUpperLayerInfo.pConfigCallback
						= pSwitchModecb;
				}
			}
		}
		else/*Return Status not initialised*/
		{
	hal4nfc_report((" phHal4Nfc_Switch_SMX_Mode 8"));
			phOsalNfc_RaiseException(phOsalNfc_e_PrecondFailed,1);
			CfgStatus= PHNFCSTVAL(CID_NFC_HAL , NFCSTATUS_NOT_INITIALISED);
		}
	}
	hal4nfc_report((" phHal4Nfc_Switch_SMX_Mode 9"));
	return CfgStatus;
}



/*  Switch mode for Swp.*/
NFCSTATUS phHal4Nfc_Switch_Swp_Mode(
									phHal_sHwReference_t      *psHwReference,
									phHal_eSWP_Mode_t          swp_mode,
									pphHal4Nfc_GenCallback_t   pSwitchModecb,
									void                      *pContext
									)
{
	NFCSTATUS CfgStatus = NFCSTATUS_PENDING;
	phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt = NULL;

	hal4nfc_report((" phHal4Nfc_Switch_Swp_Mode, swp_mode=%d", swp_mode));

	/*NULL checks*/
	if(NULL == psHwReference
		|| NULL == pSwitchModecb
		)
	{
		phOsalNfc_RaiseException(phOsalNfc_e_PrecondFailed,1);
		CfgStatus = PHNFCSTVAL(CID_NFC_HAL , NFCSTATUS_INVALID_PARAMETER);
	}
	/*Check Initialised state*/
	else if((NULL == psHwReference->hal_context)
						|| (((phHal4Nfc_Hal4Ctxt_t *)
								psHwReference->hal_context)->Hal4CurrentState
										       < eHal4StateOpenAndReady)
						|| (((phHal4Nfc_Hal4Ctxt_t *)
								psHwReference->hal_context)->Hal4NextState
										       == eHal4StateClosed))
	{
		phOsalNfc_RaiseException(phOsalNfc_e_PrecondFailed,1);
		CfgStatus = PHNFCSTVAL(CID_NFC_HAL , NFCSTATUS_NOT_INITIALISED);
	}
	else
	{
		Hal4Ctxt = psHwReference->hal_context;
		/*Previous POLL CFG has not completed or device is connected,
		  do not allow poll*/
		if(Hal4Ctxt->Hal4NextState == eHal4StateConfiguring)
		{
			PHDBG_INFO("Hal4:Configuration in progress.Returning status Busy");
			CfgStatus= PHNFCSTVAL(CID_NFC_HAL , NFCSTATUS_BUSY);
		}
		else if(Hal4Ctxt->Hal4CurrentState >= eHal4StateOpenAndReady)
		{
			 /**If config discovery has not been called prior to this ,allocate
			   ADD Context here*/
			if (NULL == Hal4Ctxt->psADDCtxtInfo)
			{
				Hal4Ctxt->psADDCtxtInfo= (pphHal4Nfc_ADDCtxtInfo_t)
					phOsalNfc_GetMemory((uint32_t)
										(sizeof(phHal4Nfc_ADDCtxtInfo_t)));
				if(NULL != Hal4Ctxt->psADDCtxtInfo)
				{
					(void)memset(Hal4Ctxt->psADDCtxtInfo,
									0,sizeof(phHal4Nfc_ADDCtxtInfo_t));
				}
			}
			if(NULL == Hal4Ctxt->psADDCtxtInfo)
			{
				phOsalNfc_RaiseException(phOsalNfc_e_NoMemory,0);
				CfgStatus= PHNFCSTVAL(CID_NFC_HAL ,
										NFCSTATUS_INSUFFICIENT_RESOURCES);
			}
			else
			{
				/*Switch mode to On or off*/
				CfgStatus = phHal4Nfc_NFCEE_Switch_Mode(Hal4Ctxt,
				((swp_mode==eSWP_Switch_Off)?(E_SWITCH_OFF):(E_SWITCH_RF_COMM)), 0);

				/* Change the State of the HAL only if Switch mode Returns
				   Success*/
				if ( NFCSTATUS_PENDING == CfgStatus )
				{
					Hal4Ctxt->Hal4NextState = eHal4StateConfiguring;
					Hal4Ctxt->sUpperLayerInfo.pConfigCallback
						= pSwitchModecb;
				}
			}
		}
		else/*Return Status not initialised*/
		{
			phOsalNfc_RaiseException(phOsalNfc_e_PrecondFailed,1);
			CfgStatus= PHNFCSTVAL(CID_NFC_HAL , NFCSTATUS_NOT_INITIALISED);
		}
	}
	return CfgStatus;
}

