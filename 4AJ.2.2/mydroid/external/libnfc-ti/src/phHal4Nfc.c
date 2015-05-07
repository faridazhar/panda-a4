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
 * \file  phHal4Nfc.c
 * \brief Hal4Nfc source.
 *
 * Project: NFC-FRI 1.1
 *
 * $Date: Fri Jun 11 09:32:23 2010 $
 * $Author: ing07385 $
 * $Revision: 1.192 $
 * $Aliases: NFC_FRI1.1_WK1023_R35_1 $
 *
 */

/* ---------------------------Include files ---------------------------------*/

#include <string.h>
#include <stdio.h>

#include <phHal4Nfc.h>
#include <phDal4Nfc.h>
#include <phOsalNfc.h>
#include <phDal4Nfc_messageQueueLib.h>
#include <phHal4Nfc_Internal.h>


#include <nfc_os_adapt.h>
#include <nfc_trans_adapt.h>
#include <nci_api.h>

#if (NCI_VERSION_IN_USE==NCI_VER_1)
#include <hci.h>
#endif

#define LOG_TAG        "HAL4NFC"
/* --------------------Structures and enumerations --------------------------*/

phHal_sHwReference_t			*gpphHal4Nfc_Hwref;
const unsigned char *nxp_nfc_fw;
uint8_t nxp_nfc_isoxchg_timeout = NXP_ISO_XCHG_TIMEOUT;
uint32_t nxp_nfc_hci_response_timeout = NXP_NFC_HCI_TIMEOUT;
uint8_t nxp_nfc_felica_timeout = NXP_FELICA_XCHG_TIMEOUT;
uint8_t nxp_nfc_mifareraw_timeout = NXP_MIFARE_XCHG_TIMEOUT;
int libnfc_llc_error_count = 0;
Hal4Nfc_BasicContext_t			gHal4Nfc_BasicContext_t;
static void nci_data_handler(nfc_handle_t h_nal, nfc_u8 conn_id, nfc_u8 *p_buff, nfc_u32 buff_len, nci_status_e status);
static void nci_tx_done_handler(nfc_handle_t h_nal, nfc_u8 conn_id, nfc_u8 *p_buff, nfc_u32 buff_len, nci_status_e status);
void nci_register_all_ntf_cb (phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt);
void nci_unregister_all_ntf_cb (phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt);

static void phHal4Nfc_IsNfceeConnected(phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt,
					uint8_t *smx_connected,
					uint8_t *uicc_connected)
{
	int i, j;

	*smx_connected = *uicc_connected = FALSE;
	Hal4Ctxt->smx_id  = SE_NOT_CONNECTED;
	Hal4Ctxt->uicc_id = SE_NOT_CONNECTED;
	Hal4Ctxt->is_hci = NFC_FALSE;

	for(j = 0; j < Hal4Ctxt->nci_start_rsp.nfcee_discover_rsp.num_nfcee; j++)
	{
		nci_ntf_param_u *pu_ntf = (nci_ntf_param_u*)&(Hal4Ctxt->nci_start_rsp.nfcee_discover_ntf[j]);
		for(i = 0; i < pu_ntf->nfcee_discover.num_nfcee_interface_information_entries; i++)
		{
			hal4nfc_report((" nfcee_discover_ntf[%d]:", j));
			#if (NCI_VERSION_IN_USE==NCI_VER_1)
				hal4nfc_report((" nfcee_id = %d", (int) pu_ntf->nfcee_discover.nfcee_id));
			#else
				hal4nfc_report((" target_handle = %d", (int) pu_ntf->nfcee_discover.target_handle));
			#endif
			hal4nfc_report((" nfcee_status = %d", (int) pu_ntf->nfcee_discover.nfcee_status));
			hal4nfc_report((" num_of_supported_NFCEE_Protocols = %d", (int) pu_ntf->nfcee_discover.num_nfcee_interface_information_entries));
			hal4nfc_report((" nfcee_protocols[%d] = 0x%02X", (int) i, (unsigned) pu_ntf->nfcee_discover.nfcee_protocols[i]));

			if (pu_ntf->nfcee_discover.nfcee_protocols[i] == NCI_NFCEE_PROTOCOL_TRANSPARENT ||
				pu_ntf->nfcee_discover.nfcee_protocols[i] == NCI_NFCEE_PROTOCOL_APDU)
			{
				*smx_connected = TRUE;
				#if (NCI_VERSION_IN_USE==NCI_VER_1)
					Hal4Ctxt->smx_id = pu_ntf->nfcee_discover.nfcee_id;
				#else
					Hal4Ctxt->smx_id = pu_ntf->nfcee_discover.target_handle;
				#endif

			}
			else if (pu_ntf->nfcee_discover.nfcee_protocols[i] == NCI_NFCEE_PROTOCOL_HCI_ACCESS ||
					 pu_ntf->nfcee_discover.nfcee_protocols[i] == NCI_NFCEE_PROTOCOL_PROP_BASE)
			{
				/* UICC activation patch - Don't tell the stack that the UICC is connected.
				   We will activate it later in this function.*/
				//*uicc_connected = TRUE;
				#if (NCI_VERSION_IN_USE==NCI_VER_1)
					Hal4Ctxt->uicc_id = pu_ntf->nfcee_discover.nfcee_id;
				#else
					Hal4Ctxt->uicc_id = pu_ntf->nfcee_discover.target_handle;
				#endif

				/* ILAN: FW report HCI workaround with 0x80 */
//				if (pu_ntf->nfcee_discover.nfcee_protocols[i] == NCI_NFCEE_PROTOCOL_HCI_ACCESS)
					Hal4Ctxt->is_hci = NFC_TRUE;
			}
		}
	}

	/* Simulate SmartMx if HCI is discovered */
	if (Hal4Ctxt->is_hci == NFC_TRUE)
		*smx_connected = TRUE;

	hal4nfc_report((" phHal4Nfc_IsNfceeConnected, smx_connected=%d, uicc_connected=%d", (int) *smx_connected, (int) *uicc_connected));


	/* UICC activation Patch - activate UICC from HAL4NFC layer since today the
	   NFC service and JNI layers does not supports UICC (JNI layes uses the SMX handle hard coded).
	   Should be removed once service will support UICC properly */
	if  (Hal4Ctxt->uicc_id != SE_NOT_CONNECTED)
	{
		op_param_u u_param = {0};

		hal4nfc_report((" phHal4Nfc_OpenComplete: UICC connected (UICC activation patch), id=0x%02X starting RF_COMM", (unsigned) Hal4Ctxt->uicc_id));

#if (NCI_VERSION_IN_USE==NCI_DRAFT_9)
		u_param.nfcee.target_hndl = (nfc_u8) Hal4Ctxt->uicc_id;
		u_param.nfcee.new_mode = E_SWITCH_RF_COMM;

		nci_start_operation(Hal4Ctxt->nciHandle,
			NCI_OPERATION_NFCEE_SWITCH_MODE,
			&u_param,
			NULL,
			0,
			NULL,
			NULL);

#elif (NCI_VERSION_IN_USE==NCI_VER_1)
		if (Hal4Ctxt->is_hci == NFC_FALSE) {
			u_param.nfcee.nfcee_id = (nfc_u8) Hal4Ctxt->uicc_id;
			u_param.nfcee.new_mode = E_SWITCH_RF_COMM;

			nci_start_operation(Hal4Ctxt->nciHandle,
				NCI_OPERATION_NFCEE_SWITCH_MODE,
				&u_param,
				NULL,
				0,
				NULL,
				NULL);
		}
#endif
	}


}

#if (NCI_VERSION_IN_USE==NCI_VER_1)

/**
 * Configure the Listen mode routing priority.
 *
 * ToDo:
 *      Currently giving priority always to DH. Need to get the NFCEE id's from
 *      the response and set the priority accordingly.
 */
static nci_status_e phHal4Nfc_ConfigureListenModeRoutingPriority(phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt)
{
	nci_status_e status = NCI_STATUS_OK;
	op_param_u listen_routing_priority;
	configure_listen_routing_t *pListenRouting;
	int i=0;

	pListenRouting = &listen_routing_priority.listen_routing.listen_routing_config;

	/* Make sure that host gets at least F listen - needed for P2P					 */
	/* In any case host will dynamically get any remain tech that SE doesn't require */
	hal4nfc_report((" phHal4Nfc_ConfigureListenModeRoutingPriority, route F Tech tech to DH"));

	pListenRouting->hosts_priority_arr[i].nfcee_id = NCI_DH_NFCEE_ID;
	pListenRouting->hosts_priority_arr[i].techs =
		ROUTE_RF_TECHNOLOGY_F;
	i++;

	// Make sure that SMX will get at least A Tech
	if  (Hal4Ctxt->smx_id != SE_NOT_CONNECTED)
	{
		hal4nfc_report((" phHal4Nfc_ConfigureListenModeRoutingPriority, SMX id=0x%X route A Techs",
			(unsigned) Hal4Ctxt->smx_id));

		pListenRouting->hosts_priority_arr[i].nfcee_id = (nfc_u8) Hal4Ctxt->smx_id;
		pListenRouting->hosts_priority_arr[i].techs =
			ROUTE_RF_TECHNOLOGY_A;
		i++;
	}

	if  (Hal4Ctxt->uicc_id != SE_NOT_CONNECTED)
	{
		hal4nfc_report((" phHal4Nfc_ConfigureListenModeRoutingPriority, UICC id=0x%X route A,B,F,V Techs",
			(unsigned) Hal4Ctxt->uicc_id));

		pListenRouting->hosts_priority_arr[i].nfcee_id = (nfc_u8) Hal4Ctxt->uicc_id;
		pListenRouting->hosts_priority_arr[i].techs =
			ROUTE_RF_TECHNOLOGY_A |
			ROUTE_RF_TECHNOLOGY_B |
			ROUTE_RF_TECHNOLOGY_F;
		i++;
	}

	if  (Hal4Ctxt->smx_id != SE_NOT_CONNECTED)
	{
		hal4nfc_report((" phHal4Nfc_ConfigureListenModeRoutingPriority, SMX id=0x%X route A,B,F,V Techs",
			(unsigned) Hal4Ctxt->smx_id));

		pListenRouting->hosts_priority_arr[i].nfcee_id = (nfc_u8) Hal4Ctxt->smx_id;
		pListenRouting->hosts_priority_arr[i].techs =
			ROUTE_RF_TECHNOLOGY_A |
			ROUTE_RF_TECHNOLOGY_B |
			ROUTE_RF_TECHNOLOGY_F;
		i++;
	}

	pListenRouting->hosts_priority_arr_size = i;
	pListenRouting->aids_arr_size = 0;
	pListenRouting->isPowerOffModeSupported = NFC_FALSE;
	pListenRouting->isSwitchOffModeSupported = NFC_TRUE;	/* BAT LOW state */

	PHDBG_INFO("Hal4:Calling nci_start_operation(NCI_OPERATION_RF_CONFIG_LISTEN_MODE_ROUTING)");
	status = nci_start_operation(Hal4Ctxt->nciHandle,
					NCI_OPERATION_RF_CONFIG_LISTEN_MODE_ROUTING,
					&listen_routing_priority,
					NULL,
					0,
					NULL,
					NULL);

	hal4nfc_report((" phHal4Nfc_ConfigureListenModeRoutingPriority, status=%d", (int) status));
	return status;
}

#endif

/**
 *  The open callback function to be called by the HCI when open (initialization)
 *  sequence is completed  or if there is an error in initialization.
 *  It is passed as a parameter to HCI when calling HCI Init.
 */

static void phHal4Nfc_OpenComplete(nfc_handle_t h_nal, nfc_u16 opcode, op_rsp_param_u *pu_resp)
{
	phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt = (phHal4Nfc_Hal4Ctxt_t*)h_nal;
	pphHal4Nfc_GenCallback_t pUpper_OpenCb
									= Hal4Ctxt->sUpperLayerInfo.pUpperOpenCb;
	void                   *pUpper_Context
								= Hal4Ctxt->sUpperLayerInfo.psUpperLayerCtxt;

	hal4nfc_report((" phHal4Nfc_OpenComplete, status=%d", (int) Hal4Ctxt->nci_start_rsp.status));

	if(Hal4Ctxt->nci_start_rsp.status == NCI_STATUS_OK)
	{
		/* Check if SE and UICC are connected */
		phHal4Nfc_IsNfceeConnected(Hal4Ctxt, &gpphHal4Nfc_Hwref->smx_connected, &gpphHal4Nfc_Hwref->uicc_connected);

		#if (NCI_VERSION_IN_USE==NCI_VER_1)

		/* Config the listen mode routing priority */
		phHal4Nfc_ConfigureListenModeRoutingPriority(Hal4Ctxt);

		#endif

		PHDBG_INFO("Hal4:Open Successful");
		{
			/*Update State*/
			Hal4Ctxt->Hal4CurrentState = Hal4Ctxt->Hal4NextState;
			Hal4Ctxt->Hal4NextState = eHal4StateInvalid;
			Hal4Ctxt->sUpperLayerInfo.pUpperOpenCb = NULL;
			if(NULL != pUpper_OpenCb)
			{
				/*Upper layer's Open Cb*/
				(*pUpper_OpenCb)(Hal4Ctxt->sUpperLayerInfo.psUpperLayerCtxt,
					NFCSTATUS_SUCCESS
					);
			}
		}
	}
	else/*Open did not succeed.Go back to reset state*/
	{
		Hal4Ctxt->Hal4CurrentState = eHal4StateClosed;
		Hal4Ctxt->Hal4NextState = eHal4StateInvalid;
#if (NCI_VERSION_IN_USE==NCI_VER_1)
		hci_destroy(Hal4Ctxt->hciHandle);
#endif
		nci_unregister_all_ntf_cb(Hal4Ctxt);
		nci_destroy(Hal4Ctxt->nciHandle);

		phOsalNfc_FreeMemory((void *)Hal4Ctxt);
		gpphHal4Nfc_Hwref->hal_context = NULL;
		gpphHal4Nfc_Hwref = NULL;
		PHDBG_INFO("Hal4:Open Failed");
		/*Call upper layer's Open Cb with error status*/
		if(NULL != pUpper_OpenCb)
		{
			/*Upper layer's Open Cb*/
			(*pUpper_OpenCb)(pUpper_Context,NFCSTATUS_INVALID_STATE);
		}
	}
	return;
}

/**
 *  The close callback function called by the HCI when close  sequence is
 *  completed or if there is an error in closing.
 *  It is passed as a parameter to HCI when calling HCI Release.
 */
static void phHal4Nfc_CloseComplete(nfc_handle_t h_nal, nfc_u16 opcode, op_rsp_param_u *pu_resp)
{
	phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt = (phHal4Nfc_Hal4Ctxt_t*)h_nal;
	NFCSTATUS   status= NFCSTATUS_SUCCESS;
	pphHal4Nfc_GenCallback_t pUpper_CloseCb;
	void                    *pUpper_Context;
	uint8_t                 RemoteDevNumber = 0;
	pUpper_CloseCb = Hal4Ctxt->sUpperLayerInfo.pUpperCloseCb;
	pUpper_Context = Hal4Ctxt->sUpperLayerInfo.psUpperLayerCtxt;

	hal4nfc_report((" phHal4Nfc_CloseComplete"));

	/*Update state*/
	Hal4Ctxt->Hal4CurrentState = Hal4Ctxt->Hal4NextState;
	Hal4Ctxt->Hal4NextState = eHal4StateInvalid;
	/*If Closed successfully*/
	if(NFCSTATUS_SUCCESS == status)
	{
#if (NCI_VERSION_IN_USE==NCI_VER_1)
		PHDBG_INFO("Hal4:Calling hci_destroy");
		hci_destroy(Hal4Ctxt->hciHandle);
#endif

		PHDBG_INFO("Hal4:Calling nci_close");
		nci_close(Hal4Ctxt->nciHandle, 0);

		nci_unregister_all_ntf_cb(Hal4Ctxt);

		PHDBG_INFO("Hal4:Calling nci_destroy");
		nci_destroy(Hal4Ctxt->nciHandle);

		/*Free all heap allocations*/

		/*Free ADD context info*/
		if(NULL != Hal4Ctxt->psADDCtxtInfo)
		{
			while(RemoteDevNumber < MAX_REMOTE_DEVICES)
			{
				if(NULL != Hal4Ctxt->rem_dev_list[RemoteDevNumber])
				{
					phOsalNfc_FreeMemory((void *)
									(Hal4Ctxt->rem_dev_list[RemoteDevNumber]));
					Hal4Ctxt->rem_dev_list[RemoteDevNumber] = NULL;
				}
				RemoteDevNumber++;
			}
			Hal4Ctxt->psADDCtxtInfo->nbr_of_devices = 0;
			phOsalNfc_FreeMemory(Hal4Ctxt->psADDCtxtInfo);
		}/*if(NULL != Hal4Ctxt->psADDCtxtInfo)*/
		/*Free Trcv context info*/
		if(NULL != Hal4Ctxt->psTrcvCtxtInfo)
		{
			if(NULL != Hal4Ctxt->psTrcvCtxtInfo->sLowerRecvData.buffer)
			{
					phOsalNfc_FreeMemory(
						Hal4Ctxt->psTrcvCtxtInfo->sLowerRecvData.buffer
						);
			}
			if((NULL == Hal4Ctxt->sTgtConnectInfo.psConnectedDevice)
				&& (NULL != Hal4Ctxt->psTrcvCtxtInfo->psUpperSendData))
			{
				phOsalNfc_FreeMemory(Hal4Ctxt->psTrcvCtxtInfo->psUpperSendData);
			}
			phOsalNfc_FreeMemory(Hal4Ctxt->psTrcvCtxtInfo);
		}/*if(NULL != Hal4Ctxt->psTrcvCtxtInfo)*/
		/*Free Hal context and Hardware reference*/
		gpphHal4Nfc_Hwref->hal_context = NULL;
		gpphHal4Nfc_Hwref = NULL;
		phOsalNfc_FreeMemory((void *)Hal4Ctxt);
	}/* if(NFCSTATUS_SUCCESS == status)*/
	/*Call Upper layer's Close Cb with status*/
	(*pUpper_CloseCb)(pUpper_Context,status);
	return;
}

#ifdef ANDROID

const unsigned char *nxp_nfc_full_version = NULL;
const unsigned char *nxp_nfc_fw = NULL;

int dlopen_firmware() {
	return 0;
}
#endif

/* Init script definitions and routines */
static NFCSTATUS phHal4Nfc_Start(phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt)
{
	NFCSTATUS startRetVal = NFCSTATUS_PENDING;
	nci_status_e status;

#if (NCI_VERSION_IN_USE==NCI_VER_1)

#define NCI_VS_NFC_MASKED_DEBUG_TRACES (__OPCODE(NCI_GID_PROPRIETARY,0x0a))
#define NCI_VS_NFC_SASE_CONFIGURATION  (__OPCODE(NCI_GID_PROPRIETARY,0x0f))

	//Send enable traces command - NCI_VS_NFC_MASKED_DEBUG_TRACES
	//{
	//	nfc_u8 buff[256] = {0,};
	//
	//	*((nfc_u32*)(&buff[0]))  = 0xffffffff; //TAG
	//	*((nfc_u32*)(&buff[4]))  = 0xffffffff; //NFC-DEP
	//	*((nfc_u32*)(&buff[8]))  = 0xffffffff; //ISO-DEP
	//	*((nfc_u32*)(&buff[12])) = 0xffffffff; //RF
	//	*((nfc_u32*)(&buff[16])) = 0xffffffff; //SWP
	//	*((nfc_u32*)(&buff[20])) = 0xffffffff; //General
	//	*((nfc_u32*)(&buff[24])) = 0xffffffff; //DCLB
	//	*((nfc_u32*)(&buff[28])) = 0x00000000; //Reserved
	//	nci_send_vendor_specific_cmd(Hal4Ctxt->nciHandle,
	//		NCI_VS_NFC_MASKED_DEBUG_TRACES,
	//		buff,
	//		32,
	//		NULL,
	//		NULL);
	//}

	// Set SASE to WI
	//Send_NCI_VS_NFC_SASE_CONFIGURATION_CMD 0x01, 5000, 0x00, 1, 1000, 5, 0
	//{
	//	nfc_u8 buff[9] = {0x01, 0x88, 0x13, 0x00, 0x01, 0xe8, 0x03, 0x05, 0x00};
	//	nci_send_vendor_specific_cmd(Hal4Ctxt->nciHandle,
	//		NCI_VS_NFC_SASE_CONFIGURATION,
	//		buff,
	//		9,
	//		NULL,
	//		NULL);
	//}
#endif

	PHDBG_INFO("Hal4:Calling nci_start_operation(NCI_OPERATION_START)");
	status = nci_start_operation(Hal4Ctxt->nciHandle,
		NCI_OPERATION_START,
		NULL,
		(op_rsp_param_u*)&Hal4Ctxt->nci_start_rsp,
		sizeof(Hal4Ctxt->nci_start_rsp),
		phHal4Nfc_OpenComplete,
		Hal4Ctxt);

	if(status != NCI_STATUS_OK)
	{
		startRetVal = PHNFCSTVAL(CID_NFC_HAL ,NFCSTATUS_INVALID_STATE);
	}
	return startRetVal;
}

#if (NCI_VERSION_IN_USE==NCI_VER_1)

#define NCI_VS_NFCC_INFO               (__OPCODE(NCI_GID_PROPRIETARY,0x12))

static void phHal4Nfc_ScriptComplete(nfc_handle_t h_cb, nfc_u16 opcode, op_rsp_param_u *pu_resp)
{
	phHal4Nfc_Hal4Ctxt_t* Hal4Ctxt = (phHal4Nfc_Hal4Ctxt_t*) h_cb;
	nci_status_e status = (nci_status_e) pu_resp->generic.status;
	NFCSTATUS startRetVal;

	hal4nfc_report((" phHal4Nfc_ScriptComplete, status=0x%X", (unsigned) status));

	startRetVal = phHal4Nfc_Start(Hal4Ctxt);
	if(startRetVal != NFCSTATUS_PENDING)
	{
		hal4nfc_report((" phHal4Nfc_ScriptComplete, phHal4Nfc_Start failed"));
	}
}

static void phHal4Nfc_ScriptStart(nfc_handle_t usr_param, nfc_u16 opcode, nfc_u8* p_rsp_buff, nfc_u8 rsp_buff_len)
{
	phHal4Nfc_Hal4Ctxt_t* Hal4Ctxt = (phHal4Nfc_Hal4Ctxt_t*) usr_param;

	op_param_u script_param;

	OSA_ASSERT(opcode == NCI_VS_NFCC_INFO);

	hal4nfc_report((" phHal4Nfc_ScriptStart - Len=%d, Status=0x%02X, HwID=%d, SWVer=(Major=%d,Minor=%d,Patch=%d)",
		(int)      rsp_buff_len,
		(unsigned) p_rsp_buff[0],
		(int)      p_rsp_buff[1],
		(int)      p_rsp_buff[2],
		(int)      p_rsp_buff[3],
		(int)      p_rsp_buff[4]));

	if ((nci_status_e) p_rsp_buff[0] != NCI_STATUS_OK)
	{
		hal4nfc_report((" phHal4Nfc_ScriptStart - failed NCI_VS_NFCC_INFO, using version 0.0.0.0"));
		p_rsp_buff[1]=0;
		p_rsp_buff[2]=0;
		p_rsp_buff[3]=0;
		p_rsp_buff[4]=0;
	}

	Hal4Ctxt->scriptFile[sizeof(Hal4Ctxt->scriptFile)-1]='\0';
	snprintf((char*) Hal4Ctxt->scriptFile, sizeof(Hal4Ctxt->scriptFile)-1, NFC_INIT_SCRIPT_FILE_FMT,
		(int) p_rsp_buff[1],
		(int) p_rsp_buff[2],
		(int) p_rsp_buff[3],
		(int) p_rsp_buff[4]);

	/* Send init script file */
	hal4nfc_report((" phHal4Nfc_ScriptStart - loading script file \"%s\"", Hal4Ctxt->scriptFile));

	script_param.script.filename = Hal4Ctxt->scriptFile;
	nci_start_operation(Hal4Ctxt->nciHandle, NCI_OPERATION_SCRIPT, &script_param, NULL, 0, phHal4Nfc_ScriptComplete, Hal4Ctxt);
}

static void phHal4Nfc_hci_activated(nfc_handle_t h_caller, nfc_status status)
{
	phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt = (phHal4Nfc_Hal4Ctxt_t*)h_caller;
	pphHal4Nfc_GenCallback_t    pConfigCallback
								= Hal4Ctxt->sUpperLayerInfo.pConfigCallback;
	NFCSTATUS Status = NFCSTATUS_SUCCESS;
	phHal_sRemoteDevInformation_t remDev;
	phNfc_sCompletionInfo_t info;

	hal4nfc_report((" phHal4Nfc_hci_activated, status 0x%08X", (unsigned) status));

	if(NULL != Hal4Ctxt->sUpperLayerInfo.pConfigCallback)
	{
		hal4nfc_report((" phHal4Nfc_hci_activated calling pConfigCallback"));

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

	if (Hal4Ctxt->is_hci_active == NFC_TRUE) {
		/* HOST COMM */
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
	} else if (Hal4Ctxt->sTgtConnectInfo.pUpperDisconnectCb != NULL) {
		/* HOST COMM is disconnecting */
		info.status = NFCSTATUS_SUCCESS;
		phHal4Nfc_DisconnectComplete(Hal4Ctxt, &info);
	}
}

static void phHal4Nfc_hci_deactivated(nfc_handle_t h_caller, nfc_status status)
{
	phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt = (phHal4Nfc_Hal4Ctxt_t*)h_caller;
	pphHal4Nfc_GenCallback_t    pConfigCallback
								= Hal4Ctxt->sUpperLayerInfo.pConfigCallback;
	NFCSTATUS Status = NFCSTATUS_SUCCESS;
	phNfc_sCompletionInfo_t info;

	hal4nfc_report((" phHal4Nfc_hci_deactivated, status 0x%08X", (unsigned) status));

	if(NULL != Hal4Ctxt->sUpperLayerInfo.pConfigCallback)
	{
		hal4nfc_report((" phHal4Nfc_hci_deactivated calling pConfigCallback"));

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

	info.status = NFCSTATUS_SUCCESS;
	phHal4Nfc_DisconnectComplete(Hal4Ctxt, &info);
}

static void phHal4Nfc_hci_evt_connectivity(nfc_handle_t h_caller)
{
	hal4nfc_report((" phHal4Nfc_hci_evt_connectivity"));
}

static void phHal4Nfc_hci_evt_transaction(nfc_handle_t h_caller, nfc_u8 *aid, nfc_u8 aid_len, nfc_u8 *param, nfc_u8 param_len)
{
	phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt = (phHal4Nfc_Hal4Ctxt_t*)h_caller;
	phHal_sEventInfo_t sEventInfo;
	phNfc_sNotificationInfo_t sNotificationInfo;

	hal4nfc_report((" phHal4Nfc_hci_evt_transaction, aid_len=%d, param_len=%d", (int)aid_len, (int)param_len));

	sEventInfo.eventHost = phHal_eUICCHost;//phHal_eHostController;
	sEventInfo.eventSource = phHal_ePICC_DevType;
	sEventInfo.eventType = NFC_EVT_TRANSACTION;
	sEventInfo.eventInfo.uicc_info.aid.length = aid_len;
	sEventInfo.eventInfo.uicc_info.aid.buffer = aid;
	sEventInfo.eventInfo.uicc_info.param.length = param_len;
	sEventInfo.eventInfo.uicc_info.param.buffer = param;

	sNotificationInfo.info = &sEventInfo;
	sNotificationInfo.status = NFCSTATUS_SUCCESS;
	sNotificationInfo.type = NFC_EVENT_NOTIFICATION;

	phHal4Nfc_HandleEmulationEvent(Hal4Ctxt, &sNotificationInfo);
}

static void phHal4Nfc_hci_gto_apdu_evt_transmit_data(nfc_handle_t h_caller, nfc_u8 *data, nfc_u32 data_len)
{
	phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt = (phHal4Nfc_Hal4Ctxt_t*)h_caller;
	phNfc_sTransactionInfo_t info;

	hal4nfc_report((" phHal4Nfc_hci_gto_apdu_evt_transmit_data, data_len=%d", (int)data_len));

	info.status = NFCSTATUS_SUCCESS;
	info.buffer = data;
	info.length = (uint16_t)data_len;

	phHal4Nfc_TransceiveComplete(Hal4Ctxt, &info);
}

static void phHal4Nfc_hci_gto_apdu_evt_wtx_request(nfc_handle_t h_caller)
{
	hal4nfc_report((" phHal4Nfc_hci_gto_apdu_evt_wtx_request"));
}

static struct hci_callbacks hci_cbacks = {
	.activated = phHal4Nfc_hci_activated,
	.deactivated = phHal4Nfc_hci_deactivated,
	.evt_connectivity = phHal4Nfc_hci_evt_connectivity,
	.evt_transaction = phHal4Nfc_hci_evt_transaction,
	.gto_apdu_evt_transmit_data = phHal4Nfc_hci_gto_apdu_evt_transmit_data,
	.gto_apdu_evt_wtx_request = phHal4Nfc_hci_gto_apdu_evt_wtx_request,
};

#endif

/**
 *  The open function called by the upper HAL when HAL4 is to be opened
 *  (initialized).
 *
 */
NFCSTATUS phHal4Nfc_Open(
						 phHal_sHwReference_t       *psHwReference,
						 phHal4Nfc_InitType_t        InitType,
						 pphHal4Nfc_GenCallback_t    pOpenCallback,
						 void                       *pContext
						 )
{
	NFCSTATUS openRetVal = NFCSTATUS_PENDING;
	phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt = NULL;
	nci_status_e status = NCI_STATUS_OK;

	hal4nfc_report((" phHal4Nfc_Open, InitType=%d", (int) InitType));

	/*NULL checks*/
	if(NULL == psHwReference || NULL == pOpenCallback)
	{
		phOsalNfc_RaiseException(phOsalNfc_e_PrecondFailed,1);
		openRetVal = PHNFCSTVAL(CID_NFC_HAL ,NFCSTATUS_INVALID_PARAMETER);
	}
	else if(NULL != gpphHal4Nfc_Hwref)
	{
		/*Hal4 context is open or open in progress ,return Ctxt already open*/
		openRetVal =  PHNFCSTVAL(CID_NFC_HAL ,NFCSTATUS_ALREADY_INITIALISED);
	}
	else/*Do an initialization*/
	{
		/*If hal4 ctxt in Hwreference is NULL create a new context*/
		if(NULL == ((phHal_sHwReference_t *)psHwReference)->hal_context)
		{
			Hal4Ctxt = (phHal4Nfc_Hal4Ctxt_t *)
								phOsalNfc_GetMemory((uint32_t)sizeof(
										                phHal4Nfc_Hal4Ctxt_t)
										                );
			((phHal_sHwReference_t *)psHwReference)->hal_context = Hal4Ctxt;
		}
		else/*Take context from Hw reference*/
		{
			Hal4Ctxt = ((phHal_sHwReference_t *)psHwReference)->hal_context;
		}
		if(NULL == Hal4Ctxt)
		{
			openRetVal = PHNFCSTVAL(CID_NFC_HAL,
						NFCSTATUS_INSUFFICIENT_RESOURCES);
		}
		else
		{
			(void)memset((void *)Hal4Ctxt,
						0,
						((uint32_t)sizeof(phHal4Nfc_Hal4Ctxt_t)));

			PHDBG_INFO("Hal4:Calling nci_create");
			Hal4Ctxt->nciHandle = nci_create(Hal4Ctxt,
											gHal4Nfc_BasicContext_t.osHandle,
											nci_data_handler,
											nci_tx_done_handler);

#if (NCI_VERSION_IN_USE==NCI_VER_1)

			Hal4Ctxt->hciHandle = hci_create(Hal4Ctxt->nciHandle, gHal4Nfc_BasicContext_t.osHandle, &hci_cbacks, Hal4Ctxt);


			//Clear the Send SENSF_REQ indication
			Hal4Ctxt->nci_send_sensf_req_command = 0;
			//Clear the Send RID command indication
			Hal4Ctxt->nci_send_rid_command = 0;

			/* Clear the selected target parameters */
			Hal4Ctxt->rf_discover_target_selected = FALSE;

#endif

			/* Register NCI notifications callbacks */
			nci_register_all_ntf_cb(Hal4Ctxt);

			PHDBG_INFO("Hal4:Calling nci_open");
			if (NCI_STATUS_OK != nci_open(Hal4Ctxt->nciHandle, 0))
			{
				openRetVal = PHNFCSTVAL(CID_NFC_HAL ,NFCSTATUS_INVALID_STATE);
			}

			if( openRetVal == NFCSTATUS_PENDING )
			{
				/*update Next state*/
				Hal4Ctxt->Hal4NextState = eHal4StateOpenAndReady;

				/*Store callback and context ,and set Default settings in Context*/
				Hal4Ctxt->sUpperLayerInfo.pUpperOpenCb = pOpenCallback;
				Hal4Ctxt->sUpperLayerInfo.psUpperLayerCtxt = pContext;
				Hal4Ctxt->sTgtConnectInfo.EmulationState = NFC_EVT_DEACTIVATED;
				Hal4Ctxt->PresChkTimerId = PH_OSALNFC_INVALID_TIMER_ID;
				gpphHal4Nfc_Hwref = psHwReference;

#if (NCI_VERSION_IN_USE==NCI_VER_1)
				/* Fetch NFC Firmware version info and load init script */
				nci_send_vendor_specific_cmd(Hal4Ctxt->nciHandle,
					NCI_VS_NFCC_INFO,
					NULL,
					0,
					phHal4Nfc_ScriptStart,
					Hal4Ctxt);
#elif (NCI_VERSION_IN_USE==NCI_DRAFT_9)
				openRetVal = phHal4Nfc_Start(Hal4Ctxt);
				if (openRetVal != NFCSTATUS_PENDING)
				{
					openRetVal = PHNFCSTVAL(CID_NFC_HAL ,NFCSTATUS_INVALID_STATE);
					phOsalNfc_FreeMemory(Hal4Ctxt);
					Hal4Ctxt = NULL;
				}
#endif
			}/*if( openRetVal == NFCSTATUS_SUCCESS )*/
			else/*Free the context*/
			{
				phOsalNfc_FreeMemory(Hal4Ctxt);
			}/*else*/
		}
	}
	return openRetVal;
}

static void phHal4Nfc_Ioctl_DeferredCall(void *params)
{
	phHal4Nfc_deferMsg *ioctl_msg;
	phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt;
	pphHal4Nfc_IoctlCallback_t pUpper_IoctlCb;
	void  *pUpper_Context;

	hal4nfc_report((" phHal4Nfc_Ioctl_DeferredCall"));

	if(params == NULL)
		return;

	ioctl_msg = (phHal4Nfc_deferMsg *)params;
	Hal4Ctxt = (phHal4Nfc_Hal4Ctxt_t *)ioctl_msg->Hal4Ctxt;
	pUpper_IoctlCb = Hal4Ctxt->sUpperLayerInfo.pUpperIoctlCb;
	pUpper_Context = Hal4Ctxt->sUpperLayerInfo.psUpperLayerCtxt;
	Hal4Ctxt->sUpperLayerInfo.pUpperIoctlCb = NULL;

	/*Call registered Ioctl callback*/
	if(NULL != pUpper_IoctlCb)
	{
		(*pUpper_IoctlCb)(
			pUpper_Context,
			Hal4Ctxt->sUpperLayerInfo.pIoctlOutParam,
			NFCSTATUS_SUCCESS
			);
	}

	phOsalNfc_FreeMemory(ioctl_msg->defer_msg);
	phOsalNfc_FreeMemory(ioctl_msg);
}

/**  The I/O Control function allows the caller to use (vendor-) specific
*  functionality provided by the lower layer or by the hardware. */
NFCSTATUS phHal4Nfc_Ioctl(
						  phHal_sHwReference_t       *psHwReference,
						  uint32_t                    IoctlCode,
						  phNfc_sData_t              *pInParam,
						  phNfc_sData_t              *pOutParam,
						  pphHal4Nfc_IoctlCallback_t  pIoctlCallback,
						  void                       *pContext
						  )
{
	NFCSTATUS RetStatus = NFCSTATUS_PENDING;
	phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt = NULL;
	uint32_t config_type = 0;
	uint8_t ind = 0;
	phHal4Nfc_deferMsg *ioctl_msg;
	phLibNfc_DeferredCall_t *defer_msg;
	phDal4Nfc_Message_Wrapper_t wrapper;

	hal4nfc_report((" phHal4Nfc_Ioctl, IoctlCode=%d", (int) IoctlCode));

	/*NULL checks*/
	if((NULL == psHwReference)
		|| (NULL == pIoctlCallback)
		)
	{
		phOsalNfc_RaiseException(phOsalNfc_e_PrecondFailed,1);
		RetStatus = PHNFCSTVAL(CID_NFC_HAL ,NFCSTATUS_INVALID_PARAMETER);
	}
	else if(NULL == psHwReference->hal_context)
	{
		RetStatus = PHNFCSTVAL(CID_NFC_HAL ,NFCSTATUS_NOT_INITIALISED);
	}
	else/*Status is Initialised*/
	{
		/* Return an empty buffer for NFC_MEM_READ ioctl */
		if (IoctlCode == NFC_MEM_READ)
		{
			pOutParam->length = 0;
			pOutParam->buffer[0] = 0;
		}

		/*Register upper layer context*/
		Hal4Ctxt = psHwReference->hal_context;
		Hal4Ctxt->sUpperLayerInfo.pIoctlOutParam = pOutParam;
		Hal4Ctxt->sUpperLayerInfo.psUpperLayerCtxt = pContext;
		Hal4Ctxt->sUpperLayerInfo.pUpperIoctlCb= pIoctlCallback;

		/* Simulate async IOCTL function */
		ioctl_msg = phOsalNfc_GetMemory(sizeof(phHal4Nfc_deferMsg));
		if(ioctl_msg != NULL)
		{
			defer_msg = phOsalNfc_GetMemory(sizeof(phLibNfc_DeferredCall_t));
			if(defer_msg != NULL)
			{
				ioctl_msg->Hal4Ctxt = Hal4Ctxt;
				ioctl_msg->defer_msg = defer_msg;

				defer_msg->pCallback = phHal4Nfc_Ioctl_DeferredCall;
				defer_msg->pParameter = ioctl_msg;

				wrapper.mtype = 1;
				wrapper.msg.eMsgType = PH_LIBNFC_DEFERREDCALL_MSG;
				wrapper.msg.pMsgData = defer_msg;
				wrapper.msg.Size = sizeof(phLibNfc_DeferredCall_t);

				phDal4Nfc_msgsnd(nDeferedCallMessageQueueId, (void *)&wrapper,
					sizeof(phLibNfc_Message_t), 0);
			}
			else
			{
				phOsalNfc_FreeMemory(ioctl_msg);
				phOsalNfc_RaiseException(phOsalNfc_e_NoMemory, 0);
				RetStatus = PHNFCSTVAL(CID_NFC_HAL ,NFCSTATUS_INSUFFICIENT_RESOURCES);
			}
		}
		else
		{
			phOsalNfc_RaiseException(phOsalNfc_e_NoMemory, 0);
			RetStatus = PHNFCSTVAL(CID_NFC_HAL ,NFCSTATUS_INSUFFICIENT_RESOURCES);
		}
	}
	return RetStatus;
}


/**
 *  The close function called by the upper layer when HAL4 is to be closed
 *  (shutdown).
 */
NFCSTATUS phHal4Nfc_Close(
						  phHal_sHwReference_t *psHwReference,
						  pphHal4Nfc_GenCallback_t pCloseCallback,
#ifdef HAL_NFC_TI_DEVICE
						  void *pContext,
						  uint8_t mode
#else
						  void *pContext
#endif
						  )
{
	NFCSTATUS closeRetVal = NFCSTATUS_PENDING;
	phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt = NULL;
	op_param_u stop_mode;
	nci_status_e status;

#ifdef HAL_NFC_TI_DEVICE
	hal4nfc_report((" phHal4Nfc_Close, mode %d", mode));
#else
	hal4nfc_report((" phHal4Nfc_Close"));
#endif

	/*NULL checks*/
	if(NULL == psHwReference || NULL == pCloseCallback)
	{
		phOsalNfc_RaiseException(phOsalNfc_e_PrecondFailed,1);
		closeRetVal = PHNFCSTVAL(CID_NFC_HAL , NFCSTATUS_INVALID_PARAMETER);
	}
	else if((NULL == psHwReference->hal_context)
						|| (((phHal4Nfc_Hal4Ctxt_t *)
								psHwReference->hal_context)->Hal4CurrentState
										       < eHal4StateSelfTestMode)
						|| (((phHal4Nfc_Hal4Ctxt_t *)
								psHwReference->hal_context)->Hal4NextState
										       == eHal4StateClosed))
	{
		/*return already closed*/
		closeRetVal= PHNFCSTVAL(CID_NFC_HAL , NFCSTATUS_NOT_INITIALISED);
	}
	else  /*Close the HAL*/
	{
		/*Get Hal4 context from Hw reference*/
		Hal4Ctxt = (phHal4Nfc_Hal4Ctxt_t *)((phHal_sHwReference_t *)
										       psHwReference)->hal_context;
		/*Unregister Tag Listener*/
		if(NULL != Hal4Ctxt->psADDCtxtInfo)
		{
			Hal4Ctxt->sUpperLayerInfo.pTagDiscoveryNotification = NULL;
		}
		/*store Callback and Context*/
		Hal4Ctxt->sUpperLayerInfo.pUpperCloseCb = pCloseCallback;
		Hal4Ctxt->sUpperLayerInfo.psUpperLayerCtxt = pContext;

		PHDBG_INFO("Hal4:Calling nci_start_operation(NCI_OPERATION_STOP)");
#ifdef HAL_NFC_TI_DEVICE
		stop_mode.stop.mode = mode;
#else
		stop_mode.stop.mode = 0;	/* regular NFC OFF */
#endif
		status = nci_start_operation(Hal4Ctxt->nciHandle,
										NCI_OPERATION_STOP,
										&stop_mode,
										NULL,
										0,
										phHal4Nfc_CloseComplete,
										Hal4Ctxt);

		/*Update Next state and exit*/
		if( status == NCI_STATUS_OK )
		{
			Hal4Ctxt->Hal4NextState = eHal4StateClosed;
			Hal4Ctxt->sUpperLayerInfo.pTagDiscoveryNotification = NULL;
		}
	}
	return closeRetVal;
}

/*Forcibly shutdown the HAl4.Frees all Resources in use by Hal4 before shutting
  down*/
void phHal4Nfc_Hal4Reset(
						 phHal_sHwReference_t *pHwRef,
						 void                 *pContext
						 )
{
	phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt = NULL;
	NFCSTATUS             closeRetVal = NFCSTATUS_SUCCESS;
	uint8_t               RemoteDevNumber = 0;

	hal4nfc_report((" phHal4Nfc_Hal4Reset"));

	if(pHwRef ==NULL)
	{
		closeRetVal = PHNFCSTVAL(CID_NFC_HAL , NFCSTATUS_INVALID_PARAMETER);
	}
	else if(pHwRef->hal_context != NULL)
	{
		/*Get the Hal context*/
		Hal4Ctxt = (phHal4Nfc_Hal4Ctxt_t *)pHwRef->hal_context;
		/*store the upper layer context*/
		Hal4Ctxt->sUpperLayerInfo.psUpperLayerCtxt = pContext;
		Hal4Ctxt->Hal4NextState = eHal4StateClosed;
		Hal4Ctxt->sUpperLayerInfo.pTagDiscoveryNotification = NULL;

		// Call nci_start_operation(NCI_OPERATION_STOP)?
#if (NCI_VERSION_IN_USE==NCI_VER_1)
		PHDBG_INFO("Hal4:Calling hci_destroy");
		hci_destroy(Hal4Ctxt->hciHandle);
#endif

		/*Call Nci Release*/
		PHDBG_INFO("Hal4:Calling nci_close");
		nci_close(Hal4Ctxt->nciHandle, 0);

		nci_unregister_all_ntf_cb(Hal4Ctxt);

		PHDBG_INFO("Hal4:Calling nci_destroy");
		nci_destroy(Hal4Ctxt->nciHandle);

		Hal4Ctxt->Hal4CurrentState = eHal4StateClosed;

		/*Free ADD context*/
		if(NULL != Hal4Ctxt->psADDCtxtInfo)
		{
			Hal4Ctxt->sUpperLayerInfo.pTagDiscoveryNotification = NULL;
			while(RemoteDevNumber < MAX_REMOTE_DEVICES)
			{
				if(NULL != Hal4Ctxt->rem_dev_list[RemoteDevNumber])
				{
					phOsalNfc_FreeMemory((void *)
							(Hal4Ctxt->rem_dev_list[RemoteDevNumber]));
					Hal4Ctxt->rem_dev_list[RemoteDevNumber] = NULL;
				}
				RemoteDevNumber++;
			}
			Hal4Ctxt->psADDCtxtInfo->nbr_of_devices = 0;
			phOsalNfc_FreeMemory(Hal4Ctxt->psADDCtxtInfo);
		}
		/*Free Trcv context*/
		if(NULL != Hal4Ctxt->psTrcvCtxtInfo)
		{
			if(NULL != Hal4Ctxt->psTrcvCtxtInfo->sLowerRecvData.buffer)
			{
				phOsalNfc_FreeMemory(Hal4Ctxt->psTrcvCtxtInfo
										            ->sLowerRecvData.buffer);
			}
			if((NULL == Hal4Ctxt->sTgtConnectInfo.psConnectedDevice)
				&& (NULL != Hal4Ctxt->psTrcvCtxtInfo->psUpperSendData))
			{
				phOsalNfc_FreeMemory(Hal4Ctxt->psTrcvCtxtInfo->psUpperSendData);
			}
			phOsalNfc_FreeMemory(Hal4Ctxt->psTrcvCtxtInfo);
		}
		phOsalNfc_FreeMemory(Hal4Ctxt);/*Free the context*/
		pHwRef->hal_context = NULL;
		gpphHal4Nfc_Hwref = NULL;
	}
	else
	{
		/*Hal4 Context is already closed.Return Success*/
	}
	/*Reset Should always return Success*/
	if(closeRetVal != NFCSTATUS_SUCCESS)
	{
		phOsalNfc_RaiseException(phOsalNfc_e_PrecondFailed,1);
	}
	return;
}

/**
 *  \if hal
 *   \ingroup grp_hal_common
 *  \else
 *   \ingroup grp_mw_external_hal_funcs
 *  \endif
 *
 *  Retrieves the capabilities of the device represented by the Hardware
 *  Reference parameter.
 *  The HW, SW versions, the MTU and other mandatory information are located
 *  inside the pDevCapabilities parameter.
 */
NFCSTATUS phHal4Nfc_GetDeviceCapabilities(
							phHal_sHwReference_t          *psHwReference,
							phHal_sDeviceCapabilities_t   *psDevCapabilities,
							void                          *pContext
							)
{
	NFCSTATUS retstatus = NFCSTATUS_SUCCESS;

	hal4nfc_report((" phHal4Nfc_GetDeviceCapabilities"));

	/*NULL checks*/
	if(psDevCapabilities == NULL || psHwReference == NULL || pContext == NULL)
	{
		retstatus = PHNFCSTVAL(CID_NFC_HAL ,NFCSTATUS_INVALID_PARAMETER);
	}
	/*Check for Initialized state*/
	else if((NULL == psHwReference->hal_context)
						|| (((phHal4Nfc_Hal4Ctxt_t *)
								psHwReference->hal_context)->Hal4CurrentState
										       < eHal4StateOpenAndReady)
						|| (((phHal4Nfc_Hal4Ctxt_t *)
								psHwReference->hal_context)->Hal4NextState
										       == eHal4StateClosed))
	{
		retstatus = PHNFCSTVAL(CID_NFC_HAL ,NFCSTATUS_NOT_INITIALISED);
	}
	else/*Provide Device capabilities and Version Info to the caller*/
	{
		(void)memset((void *)psDevCapabilities,
			0,
			sizeof(phHal_sDeviceCapabilities_t));
		psDevCapabilities->ReaderSupProtocol.Felica         = TRUE;
		psDevCapabilities->ReaderSupProtocol.ISO14443_4A    = TRUE;
		psDevCapabilities->ReaderSupProtocol.ISO14443_4B    = TRUE;
		psDevCapabilities->ReaderSupProtocol.ISO15693       = TRUE;
		psDevCapabilities->ReaderSupProtocol.Jewel          = TRUE;
		psDevCapabilities->ReaderSupProtocol.MifareStd      = TRUE;
		psDevCapabilities->ReaderSupProtocol.MifareUL       = TRUE;
		psDevCapabilities->ReaderSupProtocol.NFC            = TRUE;
		psDevCapabilities->EmulationSupProtocol.Felica      = FALSE;
		psDevCapabilities->EmulationSupProtocol.ISO14443_4A = FALSE;
		psDevCapabilities->EmulationSupProtocol.ISO14443_4B = FALSE;
		psDevCapabilities->EmulationSupProtocol.ISO15693    = FALSE;
		psDevCapabilities->EmulationSupProtocol.Jewel       = FALSE;
		psDevCapabilities->EmulationSupProtocol.MifareStd   = FALSE;
		psDevCapabilities->EmulationSupProtocol.MifareUL    = FALSE;
		psDevCapabilities->EmulationSupProtocol.NFC         = TRUE;
		psDevCapabilities->hal_version = (
					(((PH_HAL4NFC_INTERFACE_VERSION << BYTE_SIZE)
					  |(PH_HAL4NFC_INTERFACE_REVISION)<<BYTE_SIZE)
					  |(PH_HAL4NFC_INTERFACE_PATCH)<<BYTE_SIZE)
					  |PH_HAL4NFC_INTERAFECE_BUILD
					  );
	}
	return retstatus;
}

static void nci_data_handler(nfc_handle_t h_nal, nfc_u8 conn_id, nfc_u8 *p_buff, nfc_u32 buff_len, nci_status_e nci_status)
{
	phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt = (phHal4Nfc_Hal4Ctxt_t*)h_nal;
	phNfc_sTransactionInfo_t info;
	NFCSTATUS rxstatus;
	bool_t frame_corrupted = FALSE;

	hal4nfc_report((" nci_data_handler, conn_id=%d", (int) conn_id));

#if (NCI_VERSION_IN_USE==NCI_VER_1)
	if (Hal4Ctxt->nci_send_rid_command == 1 && conn_id == 0)
	{
		pHal4Nfc_Activate_Jewel(h_nal, p_buff, buff_len, nci_status);
	}
	else if (Hal4Ctxt->nci_send_sensf_req_command == 1 && conn_id == 0)
	{
		// Remove the first byte that is always 0x01 (SENSF_RSP) and leave only the payload
		pHal4Nfc_Activate_Felica(h_nal, p_buff+1, buff_len-1, nci_status);
	}
	else
#endif

	if (Hal4Ctxt->Hal4NextState == eHal4StatePresenceCheck)
	{
		info.status = NFCSTATUS_SUCCESS;

		phHal4Nfc_PresenceChkComplete(Hal4Ctxt, &info);
	}
	else if (NULL != Hal4Ctxt->psTrcvCtxtInfo)
	{
#ifdef TRANSACTION_TIMER
		if(Hal4Ctxt->psTrcvCtxtInfo->TransactionTimerId
			!= PH_OSALNFC_INVALID_TIMER_ID)
		{
			phOsalNfc_Timer_Stop(
				Hal4Ctxt->psTrcvCtxtInfo->TransactionTimerId
				);
			phOsalNfc_Timer_Delete(
				Hal4Ctxt->psTrcvCtxtInfo->TransactionTimerId
				);
			Hal4Ctxt->psTrcvCtxtInfo->TransactionTimerId
						= PH_OSALNFC_INVALID_TIMER_ID;
		}
#endif /*TRANSACTION_TIMER*/

		if (nci_status == NCI_STATUS_OK) rxstatus = NFCSTATUS_SUCCESS;
			else rxstatus = NFCSTATUS_FAILED;

		if ((Hal4Ctxt->sTgtConnectInfo.psConnectedDevice) &&
			(Hal4Ctxt->sTgtConnectInfo.psConnectedDevice->RemDevType != phHal_eNfcIP1_Target) &&
			(Hal4Ctxt->sTgtConnectInfo.psConnectedDevice->RemDevType != phHal_eNfcIP1_Initiator))
		{
			// Local device is Reader

			switch (Hal4Ctxt->sTgtConnectInfo.psConnectedDevice->RemDevType)
			{
			case phHal_eISO14443_3A_PICC:
			case phHal_eMifare_PICC:
#if (NCI_VERSION_IN_USE==NCI_VER_1)
			case phHal_eJewel_PICC:
#endif
				if ((p_buff [buff_len-1]) == NCI_STATUS_RF_FRAME_CORRUPTED)frame_corrupted = TRUE;
				buff_len--; /* frame I/F => don't forward the status byte which is at the end of the buffer */
				break;
#if (NCI_VERSION_IN_USE==NCI_VER_1)
			case phHal_eFelica_PICC:
				{
					nfc_u8* p_prev = p_buff;
					if ((p_buff [buff_len-1]) == NCI_STATUS_RF_FRAME_CORRUPTED)frame_corrupted = TRUE;
					buff_len++; /* Add command length bytes at the beginning as implemented in phFriNfc_FelicaMap.c */
					p_buff = (nfc_u8*) phOsalNfc_GetMemory(buff_len);
					p_buff[0] = 0;
					(void)memcpy(
						p_buff+1,
						p_prev,
						buff_len-1
						);
				}
				break;
#endif
			default:
				break;
			}

			if (!frame_corrupted)
			{
				info.status = rxstatus;
				info.buffer = p_buff;
				info.length = (uint16_t)buff_len;

				phHal4Nfc_TransceiveComplete(Hal4Ctxt, &info);
			}

#if (NCI_VERSION_IN_USE==NCI_VER_1)
			switch (Hal4Ctxt->sTgtConnectInfo.psConnectedDevice->RemDevType)
			{
			case phHal_eFelica_PICC:
				phOsalNfc_FreeMemory(p_buff);
				break;
			default:
				break;
			}
#endif
		}
		else
		{
			// Local device is P2P

			info.status = rxstatus;
			info.buffer = p_buff;
			info.length = (uint16_t)buff_len;

			phHal4Nfc_RecvCompleteHandler(Hal4Ctxt, &info);
		}
	}
}

static void nci_tx_done_handler(nfc_handle_t h_nal, nfc_u8 conn_id, nfc_u8 *p_buff, nfc_u32 buff_len, nci_status_e status)
{
	phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt = (phHal4Nfc_Hal4Ctxt_t*)h_nal;
	phNfc_sCompletionInfo_t info;

	hal4nfc_report((" nci_tx_done_handler, conn_id=%d, status 0x%08X", (int) conn_id, (unsigned) status));

	if (((Hal4Ctxt->sTgtConnectInfo.psConnectedDevice) &&
		(Hal4Ctxt->sTgtConnectInfo.psConnectedDevice->RemDevType == phHal_eNfcIP1_Target)) ||
		 (Hal4Ctxt->sTgtConnectInfo.EmulationState == NFC_EVT_ACTIVATED))
	{
		// Local device is P2P

		info.status = NFCSTATUS_SUCCESS;

		phHal4Nfc_SendCompleteHandler(Hal4Ctxt, &info);
	}
}

void* phHal4Nfc_Create(void)
{
	gHal4Nfc_BasicContext_t.osHandle = osa_create();
	gHal4Nfc_BasicContext_t.transHandle = transa_create(gHal4Nfc_BasicContext_t.osHandle);

	hal4nfc_report((" phHal4Nfc_Create"));

	return &gHal4Nfc_BasicContext_t;
}

void phHal4Nfc_Destroy(void* context)
{
	transa_destroy(gHal4Nfc_BasicContext_t.transHandle);
	osa_destroy(gHal4Nfc_BasicContext_t.osHandle);

	hal4nfc_report((" phHal4Nfc_Destroy"));
}

void nci_register_all_ntf_cb (phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt)
{
#if (NCI_VERSION_IN_USE==NCI_DRAFT_9)
	nci_register_ntf_cb(Hal4Ctxt->nciHandle, E_NCI_NTF_RF_ACTIVATE, nci_activate_ntf_handler, Hal4Ctxt);
#elif (NCI_VERSION_IN_USE==NCI_VER_1)

	nci_register_ntf_cb(Hal4Ctxt->nciHandle, E_NCI_NTF_RF_INTF_ACTIVATED, nci_activate_ntf_handler, Hal4Ctxt);
	nci_register_ntf_cb(Hal4Ctxt->nciHandle, E_NCI_NTF_RF_DISCOVER, nci_rf_discover_ntf_handler, Hal4Ctxt);
	nci_register_ntf_cb(Hal4Ctxt->nciHandle, E_NCI_NTF_RF_NFCEE_ACTION, nci_rf_nfcee_action_ntf_handler, Hal4Ctxt);

#endif

	nci_register_ntf_cb(Hal4Ctxt->nciHandle, E_NCI_NTF_RF_DEACTIVATE, nci_deactivate_ntf_handler, Hal4Ctxt);
	nci_register_ntf_cb(Hal4Ctxt->nciHandle, E_NCI_NTF_CORE_GENERIC_ERROR, nci_core_generic_error_ntf_handler, Hal4Ctxt);
	nci_register_ntf_cb(Hal4Ctxt->nciHandle, E_NCI_NTF_CORE_INTERFACE_ERROR, nci_core_interface_error_ntf_handler, Hal4Ctxt);
}

void nci_unregister_all_ntf_cb (phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt)
{
#if (NCI_VERSION_IN_USE==NCI_DRAFT_9)
		nci_unregister_ntf_cb(Hal4Ctxt->nciHandle, E_NCI_NTF_RF_ACTIVATE, nci_activate_ntf_handler);
#elif (NCI_VERSION_IN_USE==NCI_VER_1)

		nci_unregister_ntf_cb(Hal4Ctxt->nciHandle, E_NCI_NTF_RF_INTF_ACTIVATED, nci_activate_ntf_handler);
		nci_unregister_ntf_cb(Hal4Ctxt->nciHandle, E_NCI_NTF_RF_DISCOVER, nci_rf_discover_ntf_handler);
		nci_unregister_ntf_cb(Hal4Ctxt->nciHandle, E_NCI_NTF_RF_NFCEE_ACTION, nci_rf_nfcee_action_ntf_handler);
#endif
		nci_unregister_ntf_cb(Hal4Ctxt->nciHandle, E_NCI_NTF_RF_DEACTIVATE, nci_deactivate_ntf_handler);
		nci_unregister_ntf_cb(Hal4Ctxt->nciHandle, E_NCI_NTF_CORE_GENERIC_ERROR, nci_core_generic_error_ntf_handler);
		nci_unregister_ntf_cb(Hal4Ctxt->nciHandle, E_NCI_NTF_CORE_INTERFACE_ERROR, nci_core_interface_error_ntf_handler);
}
