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
* \file  phHal4Nfc_Reader.c
* \brief Hal4Nfc Reader source.
*
* Project: NFC-FRI 1.1
*
* $Date: Mon May 31 11:43:43 2010 $
* $Author: ing07385 $
* $Revision: 1.120 $
* $Aliases: NFC_FRI1.1_WK1023_R35_1 $
*
*/

/* ---------------------------Include files ------------------------------------*/
#include <phHal4Nfc.h>
#include <phHal4Nfc_Internal.h>
#include <phOsalNfc.h>
#include <phDal4Nfc.h>

#define LOG_TAG "HAL4NFC_READER"
/* ------------------------------- Macros ------------------------------------*/

/* --------------------Structures and enumerations --------------------------*/

static void phHal4Nfc_Connect_DeferredCall(void *params);
static void phHal4Nfc_DisconnectCb(nfc_handle_t *h_nal, nfc_u16 opcode, op_rsp_param_u *p_rsp);

/* Send deferred connect message to simulate async Connect function */
static NFCSTATUS phHal4Nfc_SendDefferedConnectMsg(phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt)
{
	NFCSTATUS RetStatus = NFCSTATUS_FAILED;

	phHal4Nfc_deferMsg *connect_msg;
	phLibNfc_DeferredCall_t *defer_msg;
	phDal4Nfc_Message_Wrapper_t wrapper;


	/* Simulate async Connect function */
	connect_msg = phOsalNfc_GetMemory(sizeof(phHal4Nfc_deferMsg));
	if(connect_msg != NULL)
	{
		defer_msg = phOsalNfc_GetMemory(sizeof(phLibNfc_DeferredCall_t));
		if(defer_msg != NULL)
		{
			connect_msg->Hal4Ctxt = Hal4Ctxt;
			connect_msg->defer_msg = defer_msg;

			defer_msg->pCallback = phHal4Nfc_Connect_DeferredCall;
			defer_msg->pParameter = connect_msg;

			wrapper.mtype = 1;
			wrapper.msg.eMsgType = PH_LIBNFC_DEFERREDCALL_MSG;
			wrapper.msg.pMsgData = defer_msg;
			wrapper.msg.Size = sizeof(phLibNfc_DeferredCall_t);

			phDal4Nfc_msgsnd(nDeferedCallMessageQueueId, (void *)&wrapper,
				sizeof(phLibNfc_Message_t), 0);

			RetStatus = NFCSTATUS_PENDING;
			Hal4Ctxt->Hal4NextState = eHal4StateTargetConnected;
		}
		else
		{
			phOsalNfc_FreeMemory(connect_msg);
			phOsalNfc_RaiseException(phOsalNfc_e_NoMemory, 0);
			RetStatus = PHNFCSTVAL(CID_NFC_HAL ,NFCSTATUS_INSUFFICIENT_RESOURCES);
		}
	}
	else
	{
		phOsalNfc_RaiseException(phOsalNfc_e_NoMemory, 0);
		RetStatus = PHNFCSTVAL(CID_NFC_HAL ,NFCSTATUS_INSUFFICIENT_RESOURCES);
	}

	return RetStatus;
}

/*Allows to connect to a single, specific, already known Remote Device.*/
NFCSTATUS phHal4Nfc_Connect(
							phHal_sHwReference_t          *psHwReference,
							phHal_sRemoteDevInformation_t *psRemoteDevInfo,
							pphHal4Nfc_ConnectCallback_t   pNotifyConnectCb,
							void                          *pContext
							)
{
	NFCSTATUS RetStatus = NFCSTATUS_FAILED;
	phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt = NULL;
	uint8_t RemoteDevCount = 0;
	int32_t MemCmpRet = 0;

	hal4nfc_report((" phHal4Nfc_Connect START"));

	/*NULL chks*/
	if(NULL == psHwReference
		|| NULL == pNotifyConnectCb
		|| NULL == psRemoteDevInfo)
	{
		phOsalNfc_RaiseException(phOsalNfc_e_PrecondFailed,1);
		RetStatus = PHNFCSTVAL(CID_NFC_HAL ,NFCSTATUS_INVALID_PARAMETER);
	}
	/*Check initialised state*/
	else if((NULL == psHwReference->hal_context)
						|| (((phHal4Nfc_Hal4Ctxt_t *)
								psHwReference->hal_context)->Hal4CurrentState
										       < eHal4StateOpenAndReady)
						|| (((phHal4Nfc_Hal4Ctxt_t *)
								psHwReference->hal_context)->Hal4NextState
										       == eHal4StateClosed))
	{
		RetStatus = PHNFCSTVAL(CID_NFC_HAL ,NFCSTATUS_NOT_INITIALISED);
	}
	else if ((psRemoteDevInfo ==
			 ((phHal4Nfc_Hal4Ctxt_t *)psHwReference->hal_context)->
				sTgtConnectInfo.psConnectedDevice)
			 &&((phHal_eNfcIP1_Target == psRemoteDevInfo->RemDevType)
				||(phHal_eJewel_PICC == psRemoteDevInfo->RemDevType)))
	{
		RetStatus = PHNFCSTVAL(CID_NFC_HAL ,NFCSTATUS_FEATURE_NOT_SUPPORTED);
	}
	else
	{
		/*Get Hal ctxt from hardware reference*/
		Hal4Ctxt = (phHal4Nfc_Hal4Ctxt_t *)psHwReference->hal_context;
		/*Register upper layer context*/
		Hal4Ctxt->sUpperLayerInfo.psUpperLayerCtxt = pContext;
		/*Register upper layer callback*/
		Hal4Ctxt->sTgtConnectInfo.pUpperConnectCb = pNotifyConnectCb;
		/*Allow Connect only if no other remote device is connected*/
		if((eHal4StateTargetDiscovered == Hal4Ctxt->Hal4CurrentState)
			&& (NULL == Hal4Ctxt->sTgtConnectInfo.psConnectedDevice))
		{
			RemoteDevCount = Hal4Ctxt->psADDCtxtInfo->nbr_of_devices;
			while(0 != RemoteDevCount)
			{
				RemoteDevCount--;
				/*Check if handle provided by upper layer matches with any
				  remote device in the list*/
				if(psRemoteDevInfo
					== (Hal4Ctxt->rem_dev_list[RemoteDevCount]))
				{

					Hal4Ctxt->sTgtConnectInfo.psConnectedDevice
								  = Hal4Ctxt->rem_dev_list[RemoteDevCount];
					break;
				}
			}/*End of while*/

			if(NULL == Hal4Ctxt->sTgtConnectInfo.psConnectedDevice)
			{
				/*No matching device handle in list*/
				RetStatus = PHNFCSTVAL(CID_NFC_HAL ,
										NFCSTATUS_INVALID_REMOTE_DEVICE);
			}
			else
			{
				MemCmpRet = phOsalNfc_MemCompare(
					(void *)&(psRemoteDevInfo->RemoteDevInfo),
					(void *)&(Hal4Ctxt->rem_dev_list[Hal4Ctxt
					->psADDCtxtInfo->nbr_of_devices - 1]->RemoteDevInfo),
					sizeof(phHal_uRemoteDevInfo_t));

				/*If device is already selected issue connect from here*/
				if(0 == MemCmpRet)
				{
					RetStatus = phHal4Nfc_SendDefferedConnectMsg(Hal4Ctxt);
#ifndef HAL_NFC_TI_DEVICE
					RetStatus = phHciNfc_Connect(Hal4Ctxt->psHciHandle,
						(void *)psHwReference,
						Hal4Ctxt->rem_dev_list[RemoteDevCount]);
					if(NFCSTATUS_PENDING == RetStatus)
					{
						Hal4Ctxt->Hal4NextState = eHal4StateTargetConnected;
					}
#endif
				}
				else/*Select the matching device to connect to*/
				{
#ifndef HAL_NFC_TI_DEVICE
					RetStatus = phHciNfc_Reactivate (
						Hal4Ctxt->psHciHandle,
						(void *)psHwReference,
						Hal4Ctxt->rem_dev_list[RemoteDevCount]
						);
					Hal4Ctxt->Hal4NextState = eHal4StateTargetActivate;
#endif
				}
				if(NFCSTATUS_PENDING != RetStatus)
				{
					/*Rollback state*/
					Hal4Ctxt->Hal4CurrentState = eHal4StateOpenAndReady;
					Hal4Ctxt->sTgtConnectInfo.psConnectedDevice =  NULL;
					Hal4Ctxt->sUpperLayerInfo.psUpperLayerCtxt = NULL;
					Hal4Ctxt->sTgtConnectInfo.pUpperConnectCb = NULL;
				}
			}
		}
		/*Issue Reconnect*/
		else if(psRemoteDevInfo ==
					Hal4Ctxt->sTgtConnectInfo.psConnectedDevice)
		{
			/* Although we already connected, upper layer expect to 'reconnect', so simulate the connect again */
			hal4nfc_report((" phHal4Nfc_Connect - issue reconnect to same connected device"));
			RetStatus = phHal4Nfc_SendDefferedConnectMsg(Hal4Ctxt);
#ifndef HAL_NFC_TI_DEVICE
			RetStatus = phHciNfc_Reactivate (
				Hal4Ctxt->psHciHandle,
				(void *)psHwReference,
				psRemoteDevInfo
				);
				Hal4Ctxt->Hal4NextState = eHal4StateTargetActivate;
#endif
		}
#ifdef RECONNECT_SUPPORT
		else if (psRemoteDevInfo !=
					Hal4Ctxt->sTgtConnectInfo.psConnectedDevice)
		{
			phHal_sRemoteDevInformation_t           *ps_store_connected_device =
										        Hal4Ctxt->sTgtConnectInfo.psConnectedDevice;

			RemoteDevCount = Hal4Ctxt->psADDCtxtInfo->nbr_of_devices;

			while (0 != RemoteDevCount)
			{
				RemoteDevCount--;
				/*Check if handle provided by upper layer matches with any
				  remote device in the list*/
				if(psRemoteDevInfo == (Hal4Ctxt->rem_dev_list[RemoteDevCount]))
				{
					break;
				}
			}/*End of while*/

			if (ps_store_connected_device ==
				Hal4Ctxt->sTgtConnectInfo.psConnectedDevice)
			{
#ifndef HAL_NFC_TI_DEVICE
				RetStatus = phHciNfc_Reactivate (Hal4Ctxt->psHciHandle,
										        (void *)psHwReference,
										        psRemoteDevInfo);

				if (NFCSTATUS_PENDING == RetStatus)
				{
					Hal4Ctxt->sTgtConnectInfo.psConnectedDevice =
									Hal4Ctxt->rem_dev_list[RemoteDevCount];
					Hal4Ctxt->Hal4NextState = eHal4StateTargetActivate;
				}
#endif
			}
		}
#endif /* #ifdef RECONNECT_SUPPORT */
		else if(NULL == Hal4Ctxt->sTgtConnectInfo.psConnectedDevice)
		{
			/*Wrong state to issue connect*/
			RetStatus = PHNFCSTVAL(CID_NFC_HAL,
									NFCSTATUS_INVALID_REMOTE_DEVICE);
		}
		else/*No Target or already connected to device*/
		{
			RetStatus = PHNFCSTVAL(CID_NFC_HAL ,NFCSTATUS_FAILED);
		}

	}

	hal4nfc_report((" phHal4Nfc_Connect END(stat=0x%X)", RetStatus));
	return RetStatus;
}


/*  The phHal4Nfc_Transceive function allows the Initiator to send and receive
 *  data to and from the Remote Device selected by the caller.*/
NFCSTATUS phHal4Nfc_Transceive(
							   phHal_sHwReference_t          *psHwReference,
							   phHal_sTransceiveInfo_t       *psTransceiveInfo,
							   phHal_sRemoteDevInformation_t  *psRemoteDevInfo,
							   pphHal4Nfc_TransceiveCallback_t pTrcvCallback,
							   void                           *pContext
							   )
{
	NFCSTATUS RetStatus = NFCSTATUS_PENDING;
	phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt = (phHal4Nfc_Hal4Ctxt_t *)pContext;
	nci_status_e status;
	nfc_u8 *p_user_payload;
	nfc_u32 user_payload_len;

	hal4nfc_report((" phHal4Nfc_Transceive"));

	/*NULL checks*/
	if((NULL == psHwReference)
		||( NULL == pTrcvCallback )
		|| (NULL == psRemoteDevInfo)
		|| (NULL == psTransceiveInfo)
		|| (NULL == psTransceiveInfo->sRecvData.buffer)
		|| (NULL == psTransceiveInfo->sSendData.buffer)
		)
	{
		phOsalNfc_RaiseException(phOsalNfc_e_PrecondFailed,1);
		RetStatus = PHNFCSTVAL(CID_NFC_HAL ,NFCSTATUS_INVALID_PARAMETER);
	}
#ifdef HAL_TRCV_LIMIT
	else if(PH_HAL4NFC_MAX_TRCV_LEN < psTransceiveInfo->sSendData.length)
	{
		RetStatus = PHNFCSTVAL(CID_NFC_HAL ,NFCSTATUS_NOT_ALLOWED);
	}
#endif/*#ifdef HAL_TRCV_LIMIT*/
	/*Check initialised state*/
	else if((NULL == psHwReference->hal_context)
						|| (((phHal4Nfc_Hal4Ctxt_t *)
								psHwReference->hal_context)->Hal4CurrentState
										       < eHal4StateOpenAndReady)
						|| (((phHal4Nfc_Hal4Ctxt_t *)
								psHwReference->hal_context)->Hal4NextState
										       == eHal4StateClosed))
	{
		RetStatus = PHNFCSTVAL(CID_NFC_HAL ,NFCSTATUS_NOT_INITIALISED);
	}
	else
	{
		Hal4Ctxt = (phHal4Nfc_Hal4Ctxt_t *)psHwReference->hal_context;
		gpphHal4Nfc_Hwref = (phHal_sHwReference_t *)psHwReference;
		if((eHal4StateTargetConnected != Hal4Ctxt->Hal4CurrentState)
			||(eHal4StateInvalid != Hal4Ctxt->Hal4NextState))
		{
			/*Hal4 state Busy*/
			RetStatus = PHNFCSTVAL(CID_NFC_HAL,NFCSTATUS_BUSY);
			PHDBG_INFO("HAL4:Trcv Failed.Returning Busy");
		}
		else if(psRemoteDevInfo != Hal4Ctxt->sTgtConnectInfo.psConnectedDevice)
		{
			/*No such Target connected*/
			RetStatus = PHNFCSTVAL(CID_NFC_HAL,NFCSTATUS_INVALID_REMOTE_DEVICE);
		}
		else
		{
			/*allocate Trcv context*/
			if(NULL == Hal4Ctxt->psTrcvCtxtInfo)
			{
				Hal4Ctxt->psTrcvCtxtInfo= (pphHal4Nfc_TrcvCtxtInfo_t)
				phOsalNfc_GetMemory((uint32_t)(sizeof(phHal4Nfc_TrcvCtxtInfo_t)));
				if(NULL != Hal4Ctxt->psTrcvCtxtInfo)
				{
					(void)memset(Hal4Ctxt->psTrcvCtxtInfo,0,
										sizeof(phHal4Nfc_TrcvCtxtInfo_t));
					Hal4Ctxt->psTrcvCtxtInfo->RecvDataBufferStatus
						= NFCSTATUS_PENDING;
					Hal4Ctxt->psTrcvCtxtInfo->TransactionTimerId
										        = PH_OSALNFC_INVALID_TIMER_ID;
				}
			}
			if(NULL == Hal4Ctxt->psTrcvCtxtInfo)
			{
				phOsalNfc_RaiseException(phOsalNfc_e_NoMemory,0);
				RetStatus= PHNFCSTVAL(CID_NFC_HAL ,
										    NFCSTATUS_INSUFFICIENT_RESOURCES);
			}
			else
			{
				/*Process transceive based on Remote device type*/
				switch(Hal4Ctxt->sTgtConnectInfo.psConnectedDevice->RemDevType)
				{
				case phHal_eISO14443_3A_PICC:
#ifndef HAL_NFC_TI_DEVICE
					phHal4Nfc_Iso_3A_Transceive(
										psTransceiveInfo,
										Hal4Ctxt
										);
#endif
					break;
				case phHal_eMifare_PICC:
					PHDBG_INFO("Mifare Cmd received");
#ifndef HAL_NFC_TI_DEVICE
					phHal4Nfc_MifareTransceive(
										psTransceiveInfo,
										psRemoteDevInfo,
										Hal4Ctxt
										);

					Hal4Ctxt->psTrcvCtxtInfo->
						XchangeInfo.params.tag_info.cmd_type
										= (uint8_t)psTransceiveInfo->cmd.MfCmd;
#endif
					break;
				case phHal_eISO14443_A_PICC:
				case phHal_eISO14443_B_PICC:
					PHDBG_INFO("ISO14443 Cmd received");
#ifndef HAL_NFC_TI_DEVICE
					Hal4Ctxt->psTrcvCtxtInfo->
						XchangeInfo.params.tag_info.cmd_type
							= (uint8_t)psTransceiveInfo->cmd.Iso144434Cmd;
#endif
					break;
				case phHal_eISO15693_PICC:
					PHDBG_INFO("ISO15693 Cmd received");
#ifndef HAL_NFC_TI_DEVICE
					Hal4Ctxt->psTrcvCtxtInfo->
						XchangeInfo.params.tag_info.cmd_type
							= (uint8_t)psTransceiveInfo->cmd.Iso15693Cmd;
#endif
					break;
				case phHal_eNfcIP1_Target:
					{
						PHDBG_INFO("NfcIP1 Transceive");
						Hal4Ctxt->psTrcvCtxtInfo->psUpperSendData
							= &(psTransceiveInfo->sSendData);
						Hal4Ctxt->psTrcvCtxtInfo->psUpperRecvData =
							&(psTransceiveInfo->sRecvData);
					}
					break;
				case phHal_eFelica_PICC:
					PHDBG_INFO("Felica Cmd received");
#ifndef HAL_NFC_TI_DEVICE
					Hal4Ctxt->psTrcvCtxtInfo->
						XchangeInfo.params.tag_info.cmd_type
						= (uint8_t)psTransceiveInfo->cmd.FelCmd;
#endif
					break;
				case phHal_eJewel_PICC:
					PHDBG_INFO("Jewel Cmd received");
#ifndef HAL_NFC_TI_DEVICE
					Hal4Ctxt->psTrcvCtxtInfo->
						XchangeInfo.params.tag_info.cmd_type
						= (uint8_t)psTransceiveInfo->cmd.JewelCmd;
#endif
					break;
				case phHal_eISO14443_BPrime_PICC:
					RetStatus = PHNFCSTVAL(CID_NFC_HAL ,
										      NFCSTATUS_FEATURE_NOT_SUPPORTED);
					break;
				default:
					PHDBG_WARNING("Invalid Device type received");
					RetStatus = PHNFCSTVAL(CID_NFC_HAL ,NFCSTATUS_FAILED);
					break;

				}
			}
		}
		/*If status is anything other than NFCSTATUS_PENDING ,an error has
		  already occured, so dont process any further and return*/
		if(RetStatus == NFCSTATUS_PENDING)
		{
			p_user_payload = psTransceiveInfo->sSendData.buffer;
			user_payload_len = psTransceiveInfo->sSendData.length;

			if(phHal_eNfcIP1_Target ==
				  Hal4Ctxt->sTgtConnectInfo.psConnectedDevice->RemDevType)
			{
				Hal4Ctxt->sUpperLayerInfo.psUpperLayerCtxt = pContext;
				/*Register upper layer callback*/
				Hal4Ctxt->psTrcvCtxtInfo->pUpperTranceiveCb  = pTrcvCallback;
#ifndef HAL_NFC_TI_DEVICE
                               if(psRemoteDevInfo->RemoteDevInfo.NfcIP_Info.MaxFrameLength
					>= psTransceiveInfo->sSendData.length)
				{
					Hal4Ctxt->psTrcvCtxtInfo->
						XchangeInfo.params.nfc_info.more_info = FALSE;
					Hal4Ctxt->psTrcvCtxtInfo->XchangeInfo.tx_length
								= (uint8_t)psTransceiveInfo->sSendData.length;
					Hal4Ctxt->psTrcvCtxtInfo->XchangeInfo.tx_buffer
						= psTransceiveInfo->sSendData.buffer;
					/*Number of bytes remaining for next send*/
					Hal4Ctxt->psTrcvCtxtInfo->psUpperSendData->length = 0;
				}
				else
				{
					Hal4Ctxt->psTrcvCtxtInfo->
						XchangeInfo.params.nfc_info.more_info = TRUE;
					Hal4Ctxt->psTrcvCtxtInfo->XchangeInfo.tx_buffer
						= Hal4Ctxt->psTrcvCtxtInfo->psUpperSendData->buffer;
					Hal4Ctxt->psTrcvCtxtInfo->XchangeInfo.tx_length
                                                = psRemoteDevInfo->RemoteDevInfo.NfcIP_Info.MaxFrameLength;
#if 0
					Hal4Ctxt->psTrcvCtxtInfo->psUpperSendData->buffer
                                                += psRemoteDevInfo->RemoteDevInfo.NfcIP_Info.MaxFrameLength;
#else
					Hal4Ctxt->psTrcvCtxtInfo->NumberOfBytesSent
                        += psRemoteDevInfo->RemoteDevInfo.NfcIP_Info.MaxFrameLength;
#endif
					/*Number of bytes remaining for next send*/
					Hal4Ctxt->psTrcvCtxtInfo->psUpperSendData->length
                                               -= psRemoteDevInfo->RemoteDevInfo.NfcIP_Info.MaxFrameLength;
				}
#endif
				Hal4Ctxt->Hal4NextState = eHal4StateTransaction;
#ifdef TRANSACTION_TIMER
				/**Create a timer to keep track of transceive timeout*/
				if(Hal4Ctxt->psTrcvCtxtInfo->TransactionTimerId
					== PH_OSALNFC_INVALID_TIMER_ID)
				{
					PHDBG_INFO("HAL4: Transaction Timer Create for transceive");
					Hal4Ctxt->psTrcvCtxtInfo->TransactionTimerId
						= phOsalNfc_Timer_Create();
				}
				if(Hal4Ctxt->psTrcvCtxtInfo->TransactionTimerId
					== PH_OSALNFC_INVALID_TIMER_ID)
				{
					RetStatus = PHNFCSTVAL(CID_NFC_HAL ,
						NFCSTATUS_INSUFFICIENT_RESOURCES);
				}
				else
#endif/*TRANSACTION_TIMER*/
				{
					PHDBG_INFO("Hal4:Calling nci_send_data from Hal4_Transceive()");

#if (NCI_VERSION_IN_USE==NCI_DRAFT_9)
					status = nci_send_data(Hal4Ctxt->nciHandle, 0, p_user_payload, user_payload_len);
#elif (NCI_VERSION_IN_USE==NCI_VER_1)
					status = nci_send_data(Hal4Ctxt->nciHandle, 0, p_user_payload, user_payload_len, 0);
#endif

					if (status == NCI_STATUS_OK)
						RetStatus = NFCSTATUS_PENDING;

					if(NFCSTATUS_PENDING == RetStatus)
					{
						Hal4Ctxt->psTrcvCtxtInfo->P2P_Send_In_Progress = TRUE;
					}
				}
			}
			else if((psTransceiveInfo->sSendData.length == 0)
					&& (Hal4Ctxt->psTrcvCtxtInfo->sLowerRecvData.length != 0))
			{
				PHDBG_INFO("Hal4:Read remaining bytes");
				Hal4Ctxt->psTrcvCtxtInfo->psUpperRecvData
										    = &(psTransceiveInfo->sRecvData);
				/*Number of read bytes left is greater than bytes requested
					by upper layer*/
				if(Hal4Ctxt->psTrcvCtxtInfo->psUpperRecvData->length
					< Hal4Ctxt->psTrcvCtxtInfo->sLowerRecvData.length)
				{
					(void)memcpy(Hal4Ctxt->psTrcvCtxtInfo
										        ->psUpperRecvData->buffer,
						(Hal4Ctxt->psTrcvCtxtInfo->sLowerRecvData.buffer
						+ Hal4Ctxt->psTrcvCtxtInfo->LowerRecvBufferOffset)
						,Hal4Ctxt->psTrcvCtxtInfo->psUpperRecvData->length);
					Hal4Ctxt->psTrcvCtxtInfo->sLowerRecvData.length -=
						Hal4Ctxt->psTrcvCtxtInfo->psUpperRecvData->length;
					Hal4Ctxt->psTrcvCtxtInfo->LowerRecvBufferOffset
						+= Hal4Ctxt->psTrcvCtxtInfo->psUpperRecvData->length;
					RetStatus = PHNFCSTVAL(CID_NFC_HAL ,
										        NFCSTATUS_MORE_INFORMATION);
				}
				else/*Number of read bytes left is smaller.Copy all bytes
					  and free Hal's buffer*/
				{
					Hal4Ctxt->psTrcvCtxtInfo->psUpperRecvData->length
						= Hal4Ctxt->psTrcvCtxtInfo->sLowerRecvData.length;
					(void)memcpy(Hal4Ctxt->psTrcvCtxtInfo
										                ->psUpperRecvData
										                            ->buffer,
						(Hal4Ctxt->psTrcvCtxtInfo->sLowerRecvData.buffer
							+ Hal4Ctxt->psTrcvCtxtInfo->LowerRecvBufferOffset)
						,Hal4Ctxt->psTrcvCtxtInfo->psUpperRecvData->length);
					phOsalNfc_FreeMemory(Hal4Ctxt->psTrcvCtxtInfo
										            ->sLowerRecvData.buffer);
					Hal4Ctxt->psTrcvCtxtInfo->sLowerRecvData.buffer = NULL;
					Hal4Ctxt->psTrcvCtxtInfo->sLowerRecvData.length = 0;
					Hal4Ctxt->psTrcvCtxtInfo->LowerRecvBufferOffset   = 0;
					RetStatus = NFCSTATUS_SUCCESS;
				}
			}
			else/*No more bytes remaining in Hal.Read from device*/
			{
				 /*Register upper layer context*/
				Hal4Ctxt->sUpperLayerInfo.psUpperLayerCtxt = pContext;
				/*Register upper layer callback*/
				Hal4Ctxt->psTrcvCtxtInfo->pUpperTranceiveCb  = pTrcvCallback;
#ifndef HAL_NFC_TI_DEVICE
				Hal4Ctxt->psTrcvCtxtInfo->XchangeInfo.params.tag_info.addr
										            = psTransceiveInfo->addr;
				Hal4Ctxt->psTrcvCtxtInfo->XchangeInfo.tx_length
								= (uint16_t)psTransceiveInfo->sSendData.length;
				Hal4Ctxt->psTrcvCtxtInfo->XchangeInfo.tx_buffer
										= psTransceiveInfo->sSendData.buffer;
#endif
				Hal4Ctxt->psTrcvCtxtInfo->psUpperRecvData
										    = &(psTransceiveInfo->sRecvData);
#ifdef TRANSACTION_TIMER
				/**Create a timer to keep track of transceive timeout*/
				if(Hal4Ctxt->psTrcvCtxtInfo->TransactionTimerId
									== PH_OSALNFC_INVALID_TIMER_ID)
				{
					PHDBG_INFO("HAL4: Transaction Timer Create for transceive");
					Hal4Ctxt->psTrcvCtxtInfo->TransactionTimerId
										    = phOsalNfc_Timer_Create();
				}
				if(Hal4Ctxt->psTrcvCtxtInfo->TransactionTimerId
					== PH_OSALNFC_INVALID_TIMER_ID)
				{
					RetStatus = PHNFCSTVAL(CID_NFC_HAL ,
										   NFCSTATUS_INSUFFICIENT_RESOURCES);
				}
				else
#endif /*TRANSACTION_TIMER*/
				{
					switch (Hal4Ctxt->sTgtConnectInfo.psConnectedDevice->RemDevType)
					{
					case phHal_eISO14443_3A_PICC:
					case phHal_eMifare_PICC:
						{
							user_payload_len = (2+psTransceiveInfo->sSendData.length);
							p_user_payload = phOsalNfc_GetMemory(user_payload_len);

							p_user_payload[0] = psTransceiveInfo->cmd.MfCmd;
							p_user_payload[1] = psTransceiveInfo->addr;
							if (psTransceiveInfo->sSendData.length > 0)
							{
								(void)memcpy(
									p_user_payload+2,
									psTransceiveInfo->sSendData.buffer,
									psTransceiveInfo->sSendData.length
									);
							}
						}
						break;

#if (NCI_VERSION_IN_USE==NCI_VER_1)
					case phHal_eFelica_PICC:
						/* Android phFriFelicaMap.c adds a first "command length" byte which is not required */
						p_user_payload++;
						user_payload_len--;

						break;
#endif
					}

					PHDBG_INFO("Calling nci_send_data");
					hal4nfc_report((" phHal4Nfc_Transceive, active_conn_id %d", Hal4Ctxt->active_conn_id));

#if (NCI_VERSION_IN_USE==NCI_DRAFT_9)
					status = nci_send_data(Hal4Ctxt->nciHandle, Hal4Ctxt->active_conn_id, p_user_payload, user_payload_len);
#elif (NCI_VERSION_IN_USE==NCI_VER_1)
					if (Hal4Ctxt->is_hci_active == NFC_FALSE)
						status = nci_send_data(Hal4Ctxt->nciHandle, Hal4Ctxt->active_conn_id, p_user_payload, user_payload_len, 0);
					else
						status = hci_gto_apdu_transmit_data(Hal4Ctxt->hciHandle, p_user_payload, user_payload_len);
#endif

					if (status == NCI_STATUS_OK)
						RetStatus = NFCSTATUS_PENDING;

					switch (Hal4Ctxt->sTgtConnectInfo.psConnectedDevice->RemDevType)
					{
					case phHal_eISO14443_3A_PICC:
					case phHal_eMifare_PICC:
						{
							phOsalNfc_FreeMemory(p_user_payload);
						}
						break;
					}

					if(NFCSTATUS_PENDING == RetStatus)
					{
						Hal4Ctxt->Hal4NextState = eHal4StateTransaction;
#ifdef TRANSACTION_TIMER
						if (Hal4Ctxt->is_hci_active == NFC_FALSE) {
							/**Start timer to keep track of transceive timeout*/
							phOsalNfc_Timer_Start(
								Hal4Ctxt->psTrcvCtxtInfo->TransactionTimerId,
								PH_HAL4NFC_TRANSCEIVE_TIMEOUT,
								phHal4Nfc_TrcvTimeoutHandler,
								0
								);
						} else if (Hal4Ctxt->psTrcvCtxtInfo->TransactionTimerId != PH_OSALNFC_INVALID_TIMER_ID) {
							phOsalNfc_Timer_Delete(Hal4Ctxt->psTrcvCtxtInfo->TransactionTimerId);
							Hal4Ctxt->psTrcvCtxtInfo->TransactionTimerId = PH_OSALNFC_INVALID_TIMER_ID;
						}
#endif/*#ifdef TRANSACTION_TIMER*/
					}
					else
					{
						Hal4Ctxt->Hal4NextState = eHal4StateInvalid;
					}
				}
			}
		}
	}
	return RetStatus;
}

#ifdef TRANSACTION_TIMER
/**Handle transceive timeout*/
void phHal4Nfc_TrcvTimeoutHandler(uint32_t TrcvTimerId, void* context)
{
	phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt = gpphHal4Nfc_Hwref->hal_context;
	pphHal4Nfc_ReceiveCallback_t pUpperRecvCb = NULL;
	pphHal4Nfc_TransceiveCallback_t pUpperTrcvCb = NULL;

	hal4nfc_report((" phHal4Nfc_TrcvTimeoutHandler"));

	phOsalNfc_Timer_Stop(TrcvTimerId);
	phOsalNfc_Timer_Delete(TrcvTimerId);
	Hal4Ctxt->psTrcvCtxtInfo->TransactionTimerId = PH_OSALNFC_INVALID_TIMER_ID;
	Hal4Ctxt->Hal4NextState = eHal4StateInvalid;
	/*For a P2P target*/
	if(Hal4Ctxt->psTrcvCtxtInfo->pP2PRecvCb != NULL)
	{
		pUpperRecvCb = Hal4Ctxt->psTrcvCtxtInfo->pP2PRecvCb;
		Hal4Ctxt->psTrcvCtxtInfo->pP2PRecvCb = NULL;
		(*pUpperRecvCb)(
			Hal4Ctxt->sUpperLayerInfo.psUpperLayerCtxt,
			NULL,
			NFCSTATUS_RF_TIMEOUT
			);
	}
	else
	{
		/*For a P2P Initiator and tags*/
		if(Hal4Ctxt->psTrcvCtxtInfo->pUpperTranceiveCb != NULL)
		{
			pUpperTrcvCb = Hal4Ctxt->psTrcvCtxtInfo->pUpperTranceiveCb;
			Hal4Ctxt->psTrcvCtxtInfo->pUpperTranceiveCb = NULL;
			(*pUpperTrcvCb)(
						Hal4Ctxt->sUpperLayerInfo.psUpperLayerCtxt,
						Hal4Ctxt->sTgtConnectInfo.psConnectedDevice,
						NULL,
						NFCSTATUS_RF_TIMEOUT
						);
		}
	}
}
#endif /*TRANSACTION_TIMER*/

/**Handle presence check timeout*/
void phHal4Nfc_PresChkTimeoutHandler(uint32_t PresChkTimerId, void* context)
{
	phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt = gpphHal4Nfc_Hwref->hal_context;

	phNfc_sCompletionInfo_t info;

	hal4nfc_report((" phHal4Nfc_PresChkTimeoutHandler"));

	info.status = NFCSTATUS_RF_ERROR;

	phHal4Nfc_PresenceChkComplete(Hal4Ctxt, &info);
}

/**The function allows to disconnect from a specific Remote Device.*/
NFCSTATUS phHal4Nfc_Disconnect(
						phHal_sHwReference_t          *psHwReference,
						phHal_sRemoteDevInformation_t *psRemoteDevInfo,
						phHal_eReleaseType_t           ReleaseType,
						pphHal4Nfc_DiscntCallback_t    pDscntCallback,
						void                             *pContext
						)
{
	NFCSTATUS RetStatus = NFCSTATUS_PENDING;
	phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt = NULL;

	hal4nfc_report((" phHal4Nfc_Disconnect, ReleaseType = %d", ReleaseType));

	/*NULL checks*/
	if(NULL == psHwReference || NULL == pDscntCallback
		|| NULL == psRemoteDevInfo)
	{
		phOsalNfc_RaiseException(phOsalNfc_e_PrecondFailed,1);
		RetStatus = PHNFCSTVAL(CID_NFC_HAL ,NFCSTATUS_INVALID_PARAMETER);
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
		RetStatus = PHNFCSTVAL(CID_NFC_HAL ,NFCSTATUS_NOT_INITIALISED);
	}
	else if(((phHal4Nfc_Hal4Ctxt_t *)
					psHwReference->hal_context)->Hal4CurrentState
					!= eHal4StateTargetConnected)
	{
		PHDBG_INFO("Hal4:Current sate is not connect.Release returning failed");
		RetStatus = PHNFCSTVAL(CID_NFC_HAL,NFCSTATUS_FAILED);
	}
	else
	{
		Hal4Ctxt = (phHal4Nfc_Hal4Ctxt_t *)psHwReference->hal_context;
		if((Hal4Ctxt->sTgtConnectInfo.psConnectedDevice == NULL)
			|| (psRemoteDevInfo != Hal4Ctxt->sTgtConnectInfo.psConnectedDevice))
		{
			PHDBG_INFO("Hal4:disconnect returning INVALID_REMOTE_DEVICE");
			RetStatus = PHNFCSTVAL(CID_NFC_HAL,NFCSTATUS_INVALID_REMOTE_DEVICE);
		}
		else
		{
			/*Register upper layer context*/
			Hal4Ctxt->sUpperLayerInfo.psUpperLayerDisconnectCtxt = pContext;
			/*Register upper layer callback*/
			Hal4Ctxt->sTgtConnectInfo.pUpperDisconnectCb  = pDscntCallback;
			/*Register Release Type*/
			Hal4Ctxt->sTgtConnectInfo.ReleaseType = ReleaseType;
			if((eHal4StateTransaction == Hal4Ctxt->Hal4NextState)
				&&((phHal_eNfcIP1_Target != psRemoteDevInfo->RemDevType)
				||((NFC_DISCOVERY_CONTINUE != ReleaseType)
				   &&(NFC_DISCOVERY_RESTART != ReleaseType))))
			{
				Hal4Ctxt->sTgtConnectInfo.ReleaseType
										          = NFC_INVALID_RELEASE_TYPE;
				PHDBG_INFO("Hal4:disconnect returning NFCSTATUS_NOT_ALLOWED");
				RetStatus = PHNFCSTVAL(CID_NFC_HAL,NFCSTATUS_NOT_ALLOWED);
			}
			else if((eHal4StateTransaction == Hal4Ctxt->Hal4NextState)
					&&(NULL != Hal4Ctxt->psTrcvCtxtInfo)
					&&(TRUE == Hal4Ctxt->psTrcvCtxtInfo->P2P_Send_In_Progress))
			{
				/*store the hardware reference for executing disconnect later*/
				gpphHal4Nfc_Hwref = psHwReference;
				PHDBG_INFO("Hal4:disconnect deferred");
			}
			else/*execute disconnect*/
			{
				RetStatus = phHal4Nfc_Disconnect_Execute(psHwReference);
			}
		}
	}
	return RetStatus;
}

/**Execute Hal4 Disconnect*/
NFCSTATUS phHal4Nfc_Disconnect_Execute(
							phHal_sHwReference_t  *psHwReference
							)
{
	NFCSTATUS RetStatus = NFCSTATUS_PENDING;
	phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt =
		(phHal4Nfc_Hal4Ctxt_t *)psHwReference->hal_context;
	phHal_eSmartMX_Mode_t SmxMode = eSmartMx_Default;
	op_param_u rf_enable;

	hal4nfc_report((" phHal4Nfc_Disconnect_Execute"));

	switch(Hal4Ctxt->sTgtConnectInfo.ReleaseType)
	{
		/*Switch mode to Default*/
		case NFC_SMARTMX_RELEASE:
		phHal4Nfc_NFCEE_Switch_Mode(Hal4Ctxt, E_SWITCH_RF_COMM, 1);
			break;
		/*Disconnect and continue polling wheel*/
		case NFC_DISCOVERY_CONTINUE:
		case NFC_DISCOVERY_RESTART:
		{
#if (NCI_VERSION_IN_USE==NCI_VER_1)
			/* Clear the target selected (for multiple targets selection) */
			Hal4Ctxt->rf_discover_target_selected = FALSE;
#endif

			phHal4Nfc_Convert2NciRfDiscovery(Hal4Ctxt, &rf_enable);
			nci_start_operation(Hal4Ctxt->nciHandle, NCI_OPERATION_RF_ENABLE, &rf_enable, NULL, 0, phHal4Nfc_DisconnectCb, Hal4Ctxt);

			RetStatus = NFCSTATUS_PENDING;
			break;
		}
		default:
			RetStatus = PHNFCSTVAL(CID_NFC_HAL,
				NFCSTATUS_FEATURE_NOT_SUPPORTED);
			break;
	}
	Hal4Ctxt->sTgtConnectInfo.ReleaseType = NFC_INVALID_RELEASE_TYPE;
	/*Update or rollback next state*/
	Hal4Ctxt->Hal4NextState = (NFCSTATUS_PENDING == RetStatus?
					eHal4StateOpenAndReady:Hal4Ctxt->Hal4NextState);

	return  RetStatus;
}

/*The function allows to check for presence in vicinity of connected remote
  device.*/
NFCSTATUS phHal4Nfc_PresenceCheck(
								phHal_sHwReference_t     *psHwReference,
								pphHal4Nfc_GenCallback_t  pPresenceChkCb,
								void *context
								)
{
	NFCSTATUS RetStatus = NFCSTATUS_FAILED;
	phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt = NULL;

	hal4nfc_report((" phHal4Nfc_PresenceCheck"));

	/*NULL  checks*/
	if((NULL == pPresenceChkCb) || (NULL == psHwReference))
	{
		RetStatus = PHNFCSTVAL(CID_NFC_HAL,NFCSTATUS_INVALID_PARAMETER);
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
		PHDBG_INFO("HAL4:Context not Open");
		RetStatus = PHNFCSTVAL(CID_NFC_HAL,NFCSTATUS_NOT_INITIALISED);
	}
	/*check connected state and session alive*/
	else if((((phHal4Nfc_Hal4Ctxt_t *)
			 psHwReference->hal_context)->Hal4CurrentState
								< eHal4StateTargetConnected)||
			(NULL == ((phHal4Nfc_Hal4Ctxt_t *)
			  psHwReference->hal_context)->sTgtConnectInfo.psConnectedDevice)||
			(FALSE == ((phHal4Nfc_Hal4Ctxt_t *)
						psHwReference->hal_context)->sTgtConnectInfo.
									psConnectedDevice->SessionOpened))
	{
		PHDBG_INFO("HAL4:No target connected");
		RetStatus = PHNFCSTVAL(CID_NFC_HAL,NFCSTATUS_RELEASED);
	}
	else
	{
		Hal4Ctxt = (phHal4Nfc_Hal4Ctxt_t *)psHwReference->hal_context;
		/*allow only one Presence chk command at any point in time*/
		if (eHal4StatePresenceCheck != Hal4Ctxt->Hal4NextState)
		{
			nfc_u8 p_nci_payload[15];
			nfc_u32 nci_payload_len;

			Hal4Ctxt->sUpperLayerInfo.psUpperLayerCtxt = context;
			Hal4Ctxt->sTgtConnectInfo.pPresenceChkCb = pPresenceChkCb;

			switch (Hal4Ctxt->sTgtConnectInfo.psConnectedDevice->RemDevType)
			{
			case phNfc_eISO14443_A_PICC:	// type 4a
			case phNfc_eISO14443_B_PICC:	// type 4b
			case phNfc_eISO14443_4B_PICC:	// type 4b
				// Dummy 1 byte
				nci_payload_len = 1;
				p_nci_payload[0] = 0xFF;
				break;

			case phHal_eISO14443_3A_PICC:	// type 2
			case phHal_eMifare_PICC:		// MifareUL
				// Dummy read block #2
				nci_payload_len = 2;
				p_nci_payload[0] = 0x30;
				p_nci_payload[1] = 0x02;
				break;

#if (NCI_VERSION_IN_USE==NCI_VER_1)
			case phHal_eFelica_PICC:
				// Dummy fetch block #0
				nci_payload_len = 15;
				p_nci_payload [0] = 0x06;
				osa_mem_copy(&p_nci_payload[1],
					Hal4Ctxt->sTgtConnectInfo.psConnectedDevice->RemoteDevInfo.Felica_Info.IDm, PHHAL_FEL_ID_LEN); // 8 bytes
				p_nci_payload [9] = 0x01;
				p_nci_payload[10] = 0x0b;
				p_nci_payload[11] = 0x00;
				p_nci_payload[12] = 0x01;
				p_nci_payload[13] = 0x80;
				p_nci_payload[14] = 0x00;
				break;

			case phHal_eJewel_PICC:
				nci_payload_len = 7;
				p_nci_payload[0] = 0x78;
				p_nci_payload[1] = 0x00;
				p_nci_payload[2] = 0x00;
				p_nci_payload[3] = 0x00;
				p_nci_payload[4] = 0x00;
				p_nci_payload[5] = 0x00;
				p_nci_payload[6] = 0x00;
				break;
#endif
			}

			// send dummy TX data, just to see if the card answer something (error)

#if (NCI_VERSION_IN_USE==NCI_DRAFT_9)
			nci_send_data(Hal4Ctxt->nciHandle, 0, p_nci_payload, nci_payload_len);
#elif (NCI_VERSION_IN_USE==NCI_VER_1)
			nci_send_data(Hal4Ctxt->nciHandle, 0, p_nci_payload, nci_payload_len, 0);
#endif

			RetStatus = NFCSTATUS_PENDING;

			Hal4Ctxt->Hal4NextState = (NFCSTATUS_PENDING == RetStatus?
				eHal4StatePresenceCheck:Hal4Ctxt->Hal4NextState);

			Hal4Ctxt->PresChkTimerId
				= phOsalNfc_Timer_Create();

			/*Start timer to keep track of presence check timeout*/
			phOsalNfc_Timer_Start(
				Hal4Ctxt->PresChkTimerId,
				PH_HAL4NFC_TRANSCEIVE_TIMEOUT,
				phHal4Nfc_PresChkTimeoutHandler,
				0
				);
		}
		else/*Ongoing presence chk*/
		{
			RetStatus = PHNFCSTVAL(CID_NFC_HAL,NFCSTATUS_BUSY);
		}
	}
	return RetStatus;
}

void phHal4Nfc_PresenceChkComplete(
								   phHal4Nfc_Hal4Ctxt_t  *Hal4Ctxt,
								   void *pInfo
								   )
{
	NFCSTATUS RetStatus = ((phNfc_sCompletionInfo_t *)pInfo)->status;
	Hal4Ctxt->Hal4NextState = eHal4StateInvalid;

	if (Hal4Ctxt->PresChkTimerId != PH_OSALNFC_INVALID_TIMER_ID)
	{
		phOsalNfc_Timer_Stop(Hal4Ctxt->PresChkTimerId);
		phOsalNfc_Timer_Delete(Hal4Ctxt->PresChkTimerId);
		Hal4Ctxt->PresChkTimerId = PH_OSALNFC_INVALID_TIMER_ID;
	}

	hal4nfc_report((" phHal4Nfc_PresenceChkComplete"));

	/*Notify to upper layer*/
	if(NULL != Hal4Ctxt->sTgtConnectInfo.pPresenceChkCb)
	{
		pphHal4Nfc_GenCallback_t cb = Hal4Ctxt->sTgtConnectInfo.pPresenceChkCb;
		Hal4Ctxt->sTgtConnectInfo.pPresenceChkCb = NULL;
		Hal4Ctxt->sTgtConnectInfo.psConnectedDevice->SessionOpened
					 =(uint8_t)(NFCSTATUS_SUCCESS == RetStatus?TRUE:FALSE);
		(*cb)(
				Hal4Ctxt->sUpperLayerInfo.psUpperLayerCtxt,
				RetStatus
				);
	}
	return;
}

static void phHal4Nfc_DisconnectCb(nfc_handle_t *h_nal, nfc_u16 opcode, op_rsp_param_u *p_rsp)
{
	phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt = (phHal4Nfc_Hal4Ctxt_t*)h_nal;
	phNfc_sCompletionInfo_t info;

	info.status = NFCSTATUS_SUCCESS;

	phHal4Nfc_DisconnectComplete(Hal4Ctxt, &info);
}

static void phHal4Nfc_Connect_DeferredCall(void *params)
{
	phHal4Nfc_deferMsg *connect_msg;
	phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt;
	pphHal4Nfc_ConnectCallback_t pUpperConnectCb;

	hal4nfc_report((" phHal4Nfc_Connect_DeferredCall"));

	if(params == NULL)
		return;

	connect_msg = (phHal4Nfc_deferMsg *)params;
	Hal4Ctxt = (phHal4Nfc_Hal4Ctxt_t *)connect_msg->Hal4Ctxt;
	pUpperConnectCb = Hal4Ctxt->sTgtConnectInfo.pUpperConnectCb;

	PHDBG_INFO("Hal4:Connect status Success");
	Hal4Ctxt->Hal4CurrentState = eHal4StateTargetConnected;
	Hal4Ctxt->Hal4NextState = eHal4StateInvalid;
	/* Open the Session */
	Hal4Ctxt->sTgtConnectInfo.psConnectedDevice->SessionOpened = TRUE;
	Hal4Ctxt->sTgtConnectInfo.pUpperConnectCb = NULL;

	/*Call registered Connect callback*/
	if(NULL != pUpperConnectCb)
	{
		PHDBG_INFO("Hal4:Calling Connect callback");
		/*Notify to the upper layer*/
		(*pUpperConnectCb)(
					Hal4Ctxt->sUpperLayerInfo.psUpperLayerCtxt,
					Hal4Ctxt->sTgtConnectInfo.psConnectedDevice,
					NFCSTATUS_SUCCESS
					);
	}

	phOsalNfc_FreeMemory(connect_msg->defer_msg);
	phOsalNfc_FreeMemory(connect_msg);
}


void phHal4Nfc_DisconnectComplete(
								  phHal4Nfc_Hal4Ctxt_t  *Hal4Ctxt,
								  void *pInfo
								  )
{
	NFCSTATUS ConnectStatus = ((phNfc_sCompletionInfo_t *)pInfo)->status;
	phHal_sRemoteDevInformation_t *psConnectedDevice = NULL;
	pphHal4Nfc_DiscntCallback_t pUpperDisconnectCb = NULL;
	pphHal4Nfc_TransceiveCallback_t pUpperTrcvCb = NULL;

	hal4nfc_report((" phHal4Nfc_DisconnectComplete"));

	if(NULL == Hal4Ctxt)
	{
		phOsalNfc_RaiseException(phOsalNfc_e_InternalErr,1);
	}
	else if (Hal4Ctxt->sTgtConnectInfo.psConnectedDevice != NULL) /*Remote device Disconnect successful*/
	{
		/* If we are in the middle of Presence Check */
		if (Hal4Ctxt->Hal4NextState == eHal4StatePresenceCheck)
		{
			phNfc_sCompletionInfo_t info;

			hal4nfc_report((" phHal4Nfc_DisconnectComplete - calling phHal4Nfc_PresenceChkComplete"));

			info.status = NFCSTATUS_RF_ERROR;

			phHal4Nfc_PresenceChkComplete(Hal4Ctxt, &info);
		}

		psConnectedDevice = Hal4Ctxt->sTgtConnectInfo.psConnectedDevice;
		pUpperDisconnectCb = Hal4Ctxt->sTgtConnectInfo.pUpperDisconnectCb;
		/*Deallocate psTrcvCtxtInfo*/
		if(NULL != Hal4Ctxt->psTrcvCtxtInfo)
		{
			if(NULL == Hal4Ctxt->sTgtConnectInfo.psConnectedDevice)
			{
				if(NULL != Hal4Ctxt->psTrcvCtxtInfo->psUpperSendData)
				{
					phOsalNfc_FreeMemory(
						Hal4Ctxt->psTrcvCtxtInfo->psUpperSendData);
				}
			}
			else
			{
				if(phHal_eNfcIP1_Target
					== Hal4Ctxt->sTgtConnectInfo.psConnectedDevice->RemDevType)
				{
					pUpperTrcvCb = Hal4Ctxt->psTrcvCtxtInfo->pUpperTranceiveCb;
					Hal4Ctxt->psTrcvCtxtInfo->psUpperRecvData->length = 0;
					pUpperTrcvCb = Hal4Ctxt->psTrcvCtxtInfo->pUpperTranceiveCb;
					Hal4Ctxt->psTrcvCtxtInfo->pUpperTranceiveCb = NULL;
				}
			}
			if(NULL != Hal4Ctxt->psTrcvCtxtInfo->sLowerRecvData.buffer)
			{
				phOsalNfc_FreeMemory(
					Hal4Ctxt->psTrcvCtxtInfo->sLowerRecvData.buffer);
			}

			phOsalNfc_FreeMemory(Hal4Ctxt->psTrcvCtxtInfo);
			Hal4Ctxt->psTrcvCtxtInfo = NULL;
		}
#ifndef HAL_NFC_TI_DEVICE
		/*Free the remote device list*/
		do
		{
			if(NULL != Hal4Ctxt->rem_dev_list[Hal4Ctxt->
				psADDCtxtInfo->nbr_of_devices-1])
			{
				phOsalNfc_FreeMemory((void *)
					(Hal4Ctxt->rem_dev_list[Hal4Ctxt->
					psADDCtxtInfo->nbr_of_devices-1]));
				Hal4Ctxt->rem_dev_list[Hal4Ctxt->
					psADDCtxtInfo->nbr_of_devices-1] = NULL;
			}
		}while(--(Hal4Ctxt->psADDCtxtInfo->nbr_of_devices));
#endif

		Hal4Ctxt->sTgtConnectInfo.psConnectedDevice = NULL;
		/*Disconnect successful.Go to Ready state*/
		Hal4Ctxt->Hal4CurrentState = Hal4Ctxt->Hal4NextState;
		Hal4Ctxt->sTgtConnectInfo.pUpperDisconnectCb = NULL;
		Hal4Ctxt->Hal4NextState = (
			eHal4StateOpenAndReady == Hal4Ctxt->Hal4NextState?
			eHal4StateInvalid:Hal4Ctxt->Hal4NextState);
		/*Issue any pending Trcv callback*/
		if(NULL != pUpperTrcvCb)
		{
			(*pUpperTrcvCb)(
				Hal4Ctxt->sUpperLayerInfo.psUpperLayerCtxt,
				psConnectedDevice,
				NULL,
				NFCSTATUS_FAILED
				);
		}

		/*Notify upper layer*/
		if(NULL != pUpperDisconnectCb)
		{
			PHDBG_INFO("Hal4:Calling Upper layer disconnect callback");
			(*pUpperDisconnectCb)(
						Hal4Ctxt->sUpperLayerInfo.psUpperLayerDisconnectCtxt,
						psConnectedDevice,
						ConnectStatus
						);
		}
	}
	return;
}


/*Transceive complete handler function*/
void phHal4Nfc_TransceiveComplete(
								  phHal4Nfc_Hal4Ctxt_t  *Hal4Ctxt,
								  void *pInfo
								  )
{
	/*Copy status code*/
	NFCSTATUS TrcvStatus = ((phNfc_sCompletionInfo_t *)pInfo)->status;

	hal4nfc_report((" phHal4Nfc_TransceiveComplete"));

	/*Update next state*/
	Hal4Ctxt->Hal4NextState = (eHal4StateTransaction
			 == Hal4Ctxt->Hal4NextState?eHal4StateInvalid:Hal4Ctxt->Hal4NextState);

	if(NULL == Hal4Ctxt->psTrcvCtxtInfo->psUpperRecvData)
	{
		/*if recv buffer is Null*/
		phOsalNfc_RaiseException(phOsalNfc_e_PrecondFailed,1);
		TrcvStatus = NFCSTATUS_FAILED;
	}
	else if(TrcvStatus == NFCSTATUS_SUCCESS)
	{
		/*Check if recvdata buffer given by upper layer is big enough to
		receive all response bytes.If it is not big enough ,copy number
		of bytes requested by upper layer to the buffer.Remaining
		bytes are retained in Hal4 and upper layer has to issue another
		transceive call to read the same.*/
		if(((phNfc_sTransactionInfo_t *)pInfo)
			->length  > Hal4Ctxt->psTrcvCtxtInfo->psUpperRecvData->length )
		{
			TrcvStatus = PHNFCSTVAL(CID_NFC_HAL ,NFCSTATUS_MORE_INFORMATION);
			Hal4Ctxt->psTrcvCtxtInfo->sLowerRecvData.length
				= ((phNfc_sTransactionInfo_t *)pInfo)->length
				- Hal4Ctxt->psTrcvCtxtInfo->psUpperRecvData->length;

			Hal4Ctxt->psTrcvCtxtInfo->sLowerRecvData.buffer
				= (uint8_t *)phOsalNfc_GetMemory(
				Hal4Ctxt->psTrcvCtxtInfo->sLowerRecvData.length
				);
			if(NULL != Hal4Ctxt->psTrcvCtxtInfo->sLowerRecvData.buffer)
			{
				(void)memcpy(
					Hal4Ctxt->psTrcvCtxtInfo->sLowerRecvData.buffer,
					(((phNfc_sTransactionInfo_t *)pInfo)->buffer
					+ Hal4Ctxt->psTrcvCtxtInfo->psUpperRecvData
					->length)
					,Hal4Ctxt->psTrcvCtxtInfo->sLowerRecvData.length
					);
			}
			else
			{
				TrcvStatus = PHNFCSTVAL(CID_NFC_HAL,
					NFCSTATUS_INSUFFICIENT_RESOURCES);
				phOsalNfc_RaiseException(phOsalNfc_e_NoMemory,0);
			}

		}
		else/*Buffer provided by upper layer is big enough to hold all read
			  bytes*/
		{
			Hal4Ctxt->psTrcvCtxtInfo->psUpperRecvData->length
				= ((phNfc_sTransactionInfo_t *)pInfo)->length;
			Hal4Ctxt->psTrcvCtxtInfo->sLowerRecvData.length = 0;
		}
		(void)memcpy(Hal4Ctxt->psTrcvCtxtInfo->psUpperRecvData->buffer,
			((phNfc_sTransactionInfo_t *)pInfo)->buffer,
			Hal4Ctxt->psTrcvCtxtInfo->psUpperRecvData->length
			);

	}
	else/*Error scenario.Set received bytes length to zero*/
	{
		Hal4Ctxt->psTrcvCtxtInfo->psUpperRecvData->length = 0;
	}

	Hal4Ctxt->psTrcvCtxtInfo->LowerRecvBufferOffset = 0;
	/*Issue transceive callback*/
	(*Hal4Ctxt->psTrcvCtxtInfo->pUpperTranceiveCb)(
		Hal4Ctxt->sUpperLayerInfo.psUpperLayerCtxt,
		Hal4Ctxt->sTgtConnectInfo.psConnectedDevice,
		Hal4Ctxt->psTrcvCtxtInfo->psUpperRecvData,
		TrcvStatus
		);
	return;
}
