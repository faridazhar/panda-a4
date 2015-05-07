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
 * \file  phHal4Nfc_ADD.c
 * \brief Hal4Nfc_ADD source.
 *
 * Project: NFC-FRI 1.1
 *
 * $Date: Mon May 31 11:43:42 2010 $
 * $Author: ing07385 $
 * $Revision: 1.151 $
 * $Aliases: NFC_FRI1.1_WK1023_R35_1 $
 *
 */

/* ---------------------------Include files ----------------------------------*/
#include <phDal4Nfc.h>
#include <phHal4Nfc.h>
#include <phHal4Nfc_Internal.h>
#include <phOsalNfc.h>

#define LOG_TAG "HAL4NFC_ADD"
/* ------------------------------- Macros ------------------------------------*/
#define     NFCIP_ACTIVE_SHIFT      0x03U
#define     NXP_UID                 0x04U
#define     NXP_MIN_UID_LEN         0x07U
#define     LOG_TAG                 "HAL4NFC_ADD"
/* --------------------Structures and enumerations --------------------------*/

static void phHal4Nfc_ConfigureComplete_Op_Rsp(nfc_handle_t h_nal, nfc_u16 opcode, op_rsp_param_u *p_rsp);
static void phHal4Nfc_ConfigureComplete_Cmd_Rsp(nfc_handle_t h_nal, nfc_u16 opcode, nci_rsp_param_u *p_rsp);


NFCSTATUS phHal4Nfc_ConfigParameters(
						phHal_sHwReference_t     *psHwReference,
						phHal_eConfigType_t       CfgType,
						phHal_uConfig_t          *puConfig,
						pphHal4Nfc_GenCallback_t  pConfigCallback,
						void                     *pContext
						)
{
	NFCSTATUS CfgStatus = NFCSTATUS_SUCCESS;
	phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt = NULL;
	nci_status_e status;
	struct nci_tlv tlv[2];
	nfc_u8 tlv_num;
	struct nci_cmd_core_set_config core_set_config;
	enum nfcee_switch_mode mode;
	int isSmx;

	hal4nfc_report((" phHal4Nfc_ConfigParameters, CfgType=%d", CfgType));

	/*NULL checks*/
	if(NULL == psHwReference
		|| NULL == pConfigCallback
		|| NULL == puConfig
		)
	{
		phOsalNfc_RaiseException(phOsalNfc_e_PrecondFailed,1);
		CfgStatus = PHNFCSTVAL(CID_NFC_HAL , NFCSTATUS_INVALID_PARAMETER);
	}
	/*Check if initialised*/
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
		/*If previous Configuration request has not completed,do not allow new
		  configuration*/
		if(Hal4Ctxt->Hal4NextState == eHal4StateConfiguring)
		{
			PHDBG_INFO("Hal4:PollCfg in progress.Returning status Busy");
			CfgStatus= PHNFCSTVAL(CID_NFC_HAL , NFCSTATUS_BUSY);
		}
		else if(Hal4Ctxt->Hal4CurrentState >= eHal4StateOpenAndReady)
		{
			/*Allocate ADD context*/
			if (NULL == Hal4Ctxt->psADDCtxtInfo)
			{
				Hal4Ctxt->psADDCtxtInfo= (pphHal4Nfc_ADDCtxtInfo_t)
					phOsalNfc_GetMemory((uint32_t)
					(sizeof(phHal4Nfc_ADDCtxtInfo_t)));
				if(NULL != Hal4Ctxt->psADDCtxtInfo)
				{
					(void)memset(Hal4Ctxt->psADDCtxtInfo,0,
						sizeof(phHal4Nfc_ADDCtxtInfo_t)
						);
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
				/*Register Upper layer context*/
#ifdef LLCP_DISCON_CHANGES
				Hal4Ctxt->sUpperLayerInfo.psUpperLayerCfgDiscCtxt = pContext;
#else /* #ifdef LLCP_DISCON_CHANGES */
				Hal4Ctxt->sUpperLayerInfo.psUpperLayerCtxt = pContext;
#endif /* #ifdef LLCP_DISCON_CHANGES */
				switch(CfgType)
				{
				/*NFC_EMULATION_CONFIG*/
				case NFC_EMULATION_CONFIG:
				{
					if (puConfig->emuConfig.emuType == NFC_SMARTMX_EMULATION)
					{
						isSmx = 1;
						if (puConfig->emuConfig.config.smartMxCfg.enableEmulation == TRUE)
							mode = E_SWITCH_RF_COMM;
						else
							mode = E_SWITCH_OFF;
					}
					else if (puConfig->emuConfig.emuType == NFC_UICC_EMULATION)
					{
						isSmx = 0;
						if (puConfig->emuConfig.config.uiccEmuCfg.enableUicc == TRUE)
							mode = E_SWITCH_RF_COMM;
						else
							mode = E_SWITCH_OFF;
					}
					else
					{
						CfgStatus = NFCSTATUS_FAILED;
					}
					break;
				}
				/*P2P Configuration*/
				case NFC_P2P_CONFIG:
				{
#if (NCI_VERSION_IN_USE == NCI_DRAFT_9)
					tlv[0].type = NCI_CFG_TYPE_ATR_REQ_GEN_BYTES; //Poll
					tlv[1].type = NCI_CFG_TYPE_ATR_RES_GEN_BYTES; //Listen
#elif (NCI_VERSION_IN_USE == NCI_VER_1)
					tlv[0].type = NCI_CFG_TYPE_PN_ATR_REQ_GEN_BYTES; //Poll
					tlv[1].type = NCI_CFG_TYPE_LN_ATR_RES_GEN_BYTES; //Listen
#endif

					tlv[0].length = tlv[1].length = puConfig->nfcIPConfig.generalBytesLength;
					tlv[0].p_value = tlv[1].p_value = puConfig->nfcIPConfig.generalBytes;

					tlv_num = 2;
					break;
				}
				/*Protection config*/
				case NFC_SE_PROTECTION_CONFIG:
				{
#ifndef HAL_NFC_TI_DEVICE
#ifdef IGNORE_EVT_PROTECTED
					Hal4Ctxt->Ignore_Event_Protected = FALSE;
#endif/*#ifdef IGNORE_EVT_PROTECTED*/
					(void)memcpy((void *)&Hal4Ctxt->uConfig,
						(void *)puConfig,
						sizeof(phHal_uConfig_t)
						);
#else
					CfgStatus = NFCSTATUS_FAILED;
#endif
					break;
				}
				default:
					CfgStatus = NFCSTATUS_FAILED;
					break;
				}
				if ( NFCSTATUS_SUCCESS == CfgStatus )
				{
					if (CfgType == NFC_P2P_CONFIG)
					{
						/*Issue configure with given configuration*/
						core_set_config.num_of_tlvs = tlv_num;
						OSA_ASSERT(core_set_config.num_of_tlvs > 0);
						core_set_config.p_tlv = tlv;

#if (NCI_VERSION_IN_USE == NCI_DRAFT_9)
						status = nci_send_cmd(Hal4Ctxt->nciHandle, NCI_OPCODE_CORE_SET_CONFIG_CMD,
									(nci_cmd_param_u *)&core_set_config, NULL, 0, phHal4Nfc_ConfigureComplete_Cmd_Rsp, Hal4Ctxt);
#elif (NCI_VERSION_IN_USE == NCI_VER_1)
						{
							op_param_u op;
							op.config.core_set_config = core_set_config;
							status = nci_start_operation(Hal4Ctxt->nciHandle, NCI_OPERATION_RF_CONFIG, &op, NULL, 0, phHal4Nfc_ConfigureComplete_Op_Rsp, Hal4Ctxt);
						}
#endif

						if (NCI_STATUS_OK == status);
							CfgStatus = NFCSTATUS_PENDING;
					}
					else if (CfgType == NFC_EMULATION_CONFIG)
					{
						CfgStatus = phHal4Nfc_NFCEE_Switch_Mode(Hal4Ctxt, mode, isSmx);
					}

					/* Change the State of the HAL only if status is Pending */
					if ( NFCSTATUS_PENDING == CfgStatus )
					{
						Hal4Ctxt->Hal4NextState = eHal4StateConfiguring;
						Hal4Ctxt->sUpperLayerInfo.pConfigCallback
							= pConfigCallback;
					}
				}
			}
		}
		else
		{
			phOsalNfc_RaiseException(phOsalNfc_e_PrecondFailed,1);
			CfgStatus= PHNFCSTVAL(CID_NFC_HAL , NFCSTATUS_NOT_INITIALISED);
		}
	}
	return CfgStatus;
}


/**Configure the discovery*/
NFCSTATUS phHal4Nfc_ConfigureDiscovery(
						phHal_sHwReference_t          *psHwReference,
						phHal_eDiscoveryConfigMode_t   discoveryMode,
						phHal_sADD_Cfg_t              *discoveryCfg,
						pphHal4Nfc_GenCallback_t       pConfigCallback,
						void                          *pContext
						)
{
	NFCSTATUS CfgStatus = NFCSTATUS_PENDING;
	phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt = NULL;
	op_param_u rf_enable;

	hal4nfc_report((" phHal4Nfc_ConfigureDiscovery, discoveryMode=%d", discoveryMode));

	if(NULL == psHwReference
		|| NULL == pConfigCallback
		|| NULL == discoveryCfg
		)
	{
		phOsalNfc_RaiseException(phOsalNfc_e_PrecondFailed,1);
		CfgStatus = PHNFCSTVAL(CID_NFC_HAL , NFCSTATUS_INVALID_PARAMETER);
	}
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
		/*If previous Configuration request has not completed ,do not allow
		  new configuration*/
		if(Hal4Ctxt->Hal4NextState == eHal4StateConfiguring)
		{
			PHDBG_INFO("Hal4:PollCfg in progress.Returning status Busy");
			CfgStatus= PHNFCSTVAL(CID_NFC_HAL , NFCSTATUS_BUSY);
		}
		else if(Hal4Ctxt->Hal4CurrentState >= eHal4StateOpenAndReady)
		{
			if (NULL == Hal4Ctxt->psADDCtxtInfo)
			{
				Hal4Ctxt->psADDCtxtInfo= (pphHal4Nfc_ADDCtxtInfo_t)
					phOsalNfc_GetMemory((uint32_t)
					(sizeof(phHal4Nfc_ADDCtxtInfo_t)));
				if(NULL != Hal4Ctxt->psADDCtxtInfo)
				{
					(void)memset(Hal4Ctxt->psADDCtxtInfo,0,
						sizeof(phHal4Nfc_ADDCtxtInfo_t)
						);
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
				/*Register Upper layer context*/
#ifdef LLCP_DISCON_CHANGES
				Hal4Ctxt->sUpperLayerInfo.psUpperLayerCfgDiscCtxt = pContext;
#else /* #ifdef LLCP_DISCON_CHANGES */
				Hal4Ctxt->sUpperLayerInfo.psUpperLayerCtxt = pContext;
#endif /* #ifdef LLCP_DISCON_CHANGES */
				switch(discoveryMode)
				{
				case NFC_DISCOVERY_CONFIG:
					PHDBG_INFO("Hal4:Call to NFC_DISCOVERY_CONFIG");
					/*Since sADDCfg is allocated in stack ,copy the ADD
					  configuration structure to HAL4 context*/
					(void)memcpy((void *)
						&(Hal4Ctxt->psADDCtxtInfo->sADDCfg),
						(void *)discoveryCfg,
						sizeof(phHal_sADD_Cfg_t)
						);
					PHDBG_INFO("Hal4:Finished copying sADDCfg");
					Hal4Ctxt->psADDCtxtInfo->smx_discovery = FALSE;
					Hal4Ctxt->psADDCtxtInfo->IsPollConfigured = TRUE;

#if (NCI_VERSION_IN_USE==NCI_VER_1)
					/* Clear the target selected (for multiple targets selection) */
					Hal4Ctxt->rf_discover_target_selected = FALSE;
#endif

					/* information system_code(Felica) and
					AFI(ReaderB) to be populated later */
					phHal4Nfc_Convert2NciRfDiscovery(Hal4Ctxt, &rf_enable);
					nci_start_operation(Hal4Ctxt->nciHandle, NCI_OPERATION_RF_ENABLE, &rf_enable, NULL, 0, phHal4Nfc_ConfigureComplete_Op_Rsp, Hal4Ctxt);
					break;
				case NFC_DISCOVERY_STOP:
					Hal4Ctxt->psADDCtxtInfo->IsPollConfigured = FALSE;
					nci_start_operation(Hal4Ctxt->nciHandle, NCI_OPERATION_RF_DISABLE, NULL, NULL, 0, phHal4Nfc_ConfigureComplete_Op_Rsp, Hal4Ctxt);
					break;
				/*Restart Discovery wheel*/
				case NFC_DISCOVERY_START:
				case NFC_DISCOVERY_RESUME:
#if (NCI_VERSION_IN_USE==NCI_VER_1)
					/* Clear the target selected (for multiple targets selection) */
					Hal4Ctxt->rf_discover_target_selected = FALSE;
#endif
					phHal4Nfc_Convert2NciRfDiscovery(Hal4Ctxt, &rf_enable);
					nci_start_operation(Hal4Ctxt->nciHandle, NCI_OPERATION_RF_ENABLE, &rf_enable, NULL, 0, phHal4Nfc_ConfigureComplete_Op_Rsp, Hal4Ctxt);
					break;
				default:
					break;
				}
				/* Change the State of the HAL only if HCI Configure
				   Returns status as Pending */
				if ( NFCSTATUS_PENDING == CfgStatus )
				{
					(void)memcpy((void *)
						&(Hal4Ctxt->psADDCtxtInfo->sCurrentPollConfig),
						(void *)&(discoveryCfg->PollDevInfo.PollCfgInfo),
						sizeof(phHal_sPollDevInfo_t)
						);
					PHDBG_INFO("Hal4:Finished copying PollCfgInfo");
					PHDBG_INFO("Hal4:Configure returned NFCSTATUS_PENDING");
					Hal4Ctxt->Hal4NextState = eHal4StateConfiguring;
					Hal4Ctxt->sUpperLayerInfo.pConfigCallback
						= pConfigCallback;
				}
				else/*Configure failed.Restore old poll dev info*/
				{
					(void)memcpy((void *)
						&(Hal4Ctxt->psADDCtxtInfo->sADDCfg.PollDevInfo.PollCfgInfo),
						(void *)&(Hal4Ctxt->psADDCtxtInfo->sCurrentPollConfig),
						sizeof(phHal_sPollDevInfo_t)
						);
				}
			}
		}
		else
		{
			phOsalNfc_RaiseException(phOsalNfc_e_PrecondFailed,1);
			CfgStatus= PHNFCSTVAL(CID_NFC_HAL , NFCSTATUS_NOT_INITIALISED);
		}
	}
	return CfgStatus;
}

static void phHal4Nfc_ConfigureComplete_Cmd_Rsp(nfc_handle_t h_nal, nfc_u16 opcode, nci_rsp_param_u *p_rsp)
{
	phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt = (phHal4Nfc_Hal4Ctxt_t*)h_nal;
	pphHal4Nfc_GenCallback_t    pConfigCallback
								= Hal4Ctxt->sUpperLayerInfo.pConfigCallback;

	NFCSTATUS Status = NFCSTATUS_SUCCESS;

	hal4nfc_report((" phHal4Nfc_ConfigureComplete_Cmd_Rsp"));

	if(NULL != Hal4Ctxt->sUpperLayerInfo.pConfigCallback)
	{
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
}

/*Configuration completion handler*/
static void phHal4Nfc_ConfigureComplete_Op_Rsp(nfc_handle_t h_nal, nfc_u16 opcode, op_rsp_param_u *p_rsp)
{
	/*
	  * Call phHal4Nfc_ConfigureComplete_Cmd_Rsp because currently
	  * nci_start_operation & nci_send_cmd pass the same callback function
	  * but they declare a different type of response buffer.
	  * This should be changed because the status returned in p_rsp is not
	  * handled at all.
	  */
	phHal4Nfc_ConfigureComplete_Cmd_Rsp(h_nal,opcode, (nci_rsp_param_u *)p_rsp);
}

/**Handler for Target discovery completion for all remote device types*/
void phHal4Nfc_TargetDiscoveryComplete(
									   phHal4Nfc_Hal4Ctxt_t  *Hal4Ctxt,
									   void                  *pInfo
									   )
{
	static phHal4Nfc_DiscoveryInfo_t sDiscoveryInfo;
	NFCSTATUS status = NFCSTATUS_SUCCESS;
	/**SAK byte*/
	uint8_t Sak = 0;
	/*Union type to encapsulate and return the discovery info*/
	phHal4Nfc_NotificationInfo_t uNotificationInfo;
	/*All the following types will be discovered as type A ,and differentiation
	  will have to be done within this module based on SAK byte and UID info*/
	phHal_eRemDevType_t  aRemoteDevTypes[3] = {
										       phHal_eISO14443_A_PICC,
										       phHal_eNfcIP1_Target,
										       phHal_eMifare_PICC
										      };
	/*Count is used to add multiple info into remote dvice list for devices that
	  support multiple protocols*/
	uint8_t Count = 0,
		NfcIpDeviceCount = 0;/**<Number of NfcIp devices discovered*/
	uint16_t nfc_id = 0;
	/*remote device info*/
	phHal_sRemoteDevInformation_t *psRemoteDevInfo = NULL;
	status = ((phNfc_sCompletionInfo_t *)pInfo)->status;
	/*Update Hal4 state*/
	Hal4Ctxt->Hal4CurrentState = eHal4StateTargetDiscovered;
	Hal4Ctxt->Hal4NextState  = eHal4StateInvalid;

	hal4nfc_report((" phHal4Nfc_TargetDiscoveryComplete START"));

	 PHDBG_INFO("Hal4:Remotedevice Discovered");
	if(NULL != ((phNfc_sCompletionInfo_t *)pInfo)->info)
	{
		/*Extract Remote device Info*/
		psRemoteDevInfo = (phHal_sRemoteDevInformation_t *)
									((phNfc_sCompletionInfo_t *)pInfo)->info;

		switch(psRemoteDevInfo->RemDevType)
		{
			case phHal_eISO14443_A_PICC:/*for TYPE A*/
			{
				Sak = psRemoteDevInfo->RemoteDevInfo.Iso14443A_Info.Sak;
				if((Hal4Ctxt->psADDCtxtInfo->sCurrentPollConfig.EnableIso14443A)
					|| (TRUE == Hal4Ctxt->psADDCtxtInfo->smx_discovery))
				{
					/*Check if Iso is Supported*/
					if(Sak & ISO_14443_BITMASK)
					{
						Count++;
					}
					/*Check for Mifare Supported*/
					switch( Sak )
					{
                      case 0x01: // 1K Classic
					  case 0x09: // Mini
					  case 0x08: // 1K
					  case 0x18: // 4K
					  case 0x88: // Infineon 1K
					  case 0x98: // Pro 4K
					  case 0xB8: // Pro 4K
					  case 0x28: // 1K emulation
					  case 0x38: // 4K emulation
						aRemoteDevTypes[Count] = phHal_eMifare_PICC;
						Count++;
						break;
					}
					if((0 == Sak)&& (0 == Count))
					{
						/*Mifare check*/
						if((NXP_UID ==
						psRemoteDevInfo->RemoteDevInfo.Iso14443A_Info.Uid[0])
						&&(NXP_MIN_UID_LEN <=
						psRemoteDevInfo->RemoteDevInfo.Iso14443A_Info.UidLength))
						{
							aRemoteDevTypes[Count] = phHal_eMifare_PICC;
							Count++;
						}
					}
					if ( !(Sak & NFCIP_BITMASK) )
					{
						aRemoteDevTypes[Count] = phHal_eISO14443_3A_PICC;
						Count++;
					}
				}
				/*Check for P2P target passive*/
				if((Sak & NFCIP_BITMASK) &&
					(NULL != Hal4Ctxt->sUpperLayerInfo.pP2PNotification)&&
					(Hal4Ctxt->psADDCtxtInfo->sADDCfg.NfcIP_Mode
					& phHal_ePassive106))
				{
				  if( Sak == 0x53 // Fudan card incompatible to ISO18092
					  && psRemoteDevInfo->RemoteDevInfo.Iso14443A_Info.AtqA[0] == 0x04
					  && psRemoteDevInfo->RemoteDevInfo.Iso14443A_Info.AtqA[1] == 0x00
					)
				  {
					aRemoteDevTypes[Count] = phHal_eISO14443_3A_PICC;
					Count++;
				  }
				  else
				  {
					aRemoteDevTypes[Count] = phHal_eNfcIP1_Target;
					Count++;
									}
				}
			}/*case phHal_eISO14443_A_PICC:*/
				break;
			case phHal_eNfcIP1_Target:/*P2P target detected*/
				aRemoteDevTypes[Count] = phHal_eNfcIP1_Target;
				Count++;
				break;
			 case phHal_eISO14443_B_PICC: /*TYPE_B*/
#ifdef TYPE_B
				aRemoteDevTypes[Count] = phHal_eISO14443_B_PICC;
				Count++;
				hal4nfc_report((" phHal4Nfc_TargetDiscoveryComplete case phHal_eISO14443_B_PICC --> Count = %d", Count));
				break;
#endif
			case phHal_eFelica_PICC: /*Felica*/
#ifdef TYPE_FELICA
			{
				/*nfc_id is used to differentiate between Felica and NfcIp target
				  discovered in Type F*/
				nfc_id = (((uint16_t)psRemoteDevInfo->RemoteDevInfo.Felica_Info.IDm[0])
					<< BYTE_SIZE) |
					psRemoteDevInfo->RemoteDevInfo.Felica_Info.IDm[1];
				/*check  for NfcIp target*/
				if(NXP_NFCIP_NFCID2_ID  == nfc_id)
				{
					if((NULL != Hal4Ctxt->sUpperLayerInfo.pP2PNotification)
						&&((Hal4Ctxt->psADDCtxtInfo->sADDCfg.NfcIP_Mode
							 & phHal_ePassive212) ||
							(Hal4Ctxt->psADDCtxtInfo->sADDCfg.NfcIP_Mode
							 & phHal_ePassive424)))
					{
						aRemoteDevTypes[Count] = phHal_eNfcIP1_Target;
						Count++;
					}
				}
				else/*Felica*/
				{
					if(Hal4Ctxt->psADDCtxtInfo->sCurrentPollConfig.EnableFelica212
					|| Hal4Ctxt->psADDCtxtInfo->sCurrentPollConfig.EnableFelica424)
					{
						aRemoteDevTypes[Count] = phHal_eFelica_PICC;
						Count++;
					}
				}
				break;
			}
#endif
			case phHal_eJewel_PICC: /*Jewel*/
#ifdef TYPE_JEWEL
			{
				/*Report Jewel tags only if TYPE A is enabled*/
				if(Hal4Ctxt->psADDCtxtInfo->sCurrentPollConfig.EnableIso14443A)
				{
					aRemoteDevTypes[Count] = phHal_eJewel_PICC;
					Count++;
				}
				break;
			}
#endif
#ifdef  TYPE_ISO15693
			case phHal_eISO15693_PICC: /*ISO15693*/
			{
				if(Hal4Ctxt->psADDCtxtInfo->sCurrentPollConfig.EnableIso15693)
				{
					aRemoteDevTypes[Count] = phHal_eISO15693_PICC;
					Count++;
				}
				break;
			}
#endif /* #ifdef    TYPE_ISO15693 */
			/*Types currently not supported*/
			case phHal_eISO14443_BPrime_PICC:
			default:
				PHDBG_WARNING("Hal4:Notification for Not supported types");
				break;
		}/*End of switch*/
		/*Update status code to success if atleast one device info is available*/
		status = (((NFCSTATUS_SUCCESS != status)
				  && (NFCSTATUS_MULTIPLE_TAGS != status))
				   &&(Hal4Ctxt->psADDCtxtInfo->nbr_of_devices != 0))?
					NFCSTATUS_SUCCESS:status;

		/*Update status to NFCSTATUS_MULTIPLE_PROTOCOLS if count > 1 ,and this
		  is first discovery notification from Hci*/
		status = ((NFCSTATUS_SUCCESS == status)
					&&(Hal4Ctxt->psADDCtxtInfo->nbr_of_devices == 0)
					&&(Count > 1)?NFCSTATUS_MULTIPLE_PROTOCOLS:status);
		 /*If multiple protocols are supported ,allocate separate remote device
		  information for each protocol supported*/
		/*Allocate and copy Remote device info into Hal4 Context*/
		while(Count)
		{
			PHDBG_INFO("Hal4:Count is not zero");
			--Count;
			/*Allocate memory for each of Count number of
			  devices*/
			if(NULL == Hal4Ctxt->rem_dev_list[
				Hal4Ctxt->psADDCtxtInfo->nbr_of_devices])
			{
				Hal4Ctxt->rem_dev_list[
					Hal4Ctxt->psADDCtxtInfo->nbr_of_devices]
				= (phHal_sRemoteDevInformation_t *)
					phOsalNfc_GetMemory(
					(uint32_t)(
					sizeof(phHal_sRemoteDevInformation_t))
					);
			}
			if(NULL == Hal4Ctxt->rem_dev_list[
				Hal4Ctxt->psADDCtxtInfo->nbr_of_devices])
			{
				status =  PHNFCSTVAL(CID_NFC_HAL,
					NFCSTATUS_INSUFFICIENT_RESOURCES);
				phOsalNfc_RaiseException(phOsalNfc_e_NoMemory,0);
				break;
			}
			else
			{
				(void)memcpy(
					(void *)Hal4Ctxt->rem_dev_list[
						Hal4Ctxt->psADDCtxtInfo->nbr_of_devices],
						(void *)psRemoteDevInfo,
						sizeof(phHal_sRemoteDevInformation_t)
						);
				/*Now copy appropriate device type from aRemoteDevTypes array*/
				Hal4Ctxt->rem_dev_list[
						Hal4Ctxt->psADDCtxtInfo->nbr_of_devices]->RemDevType
							=   aRemoteDevTypes[Count];
				/*Increment number of devices*/
				Hal4Ctxt->psADDCtxtInfo->nbr_of_devices++;
			}/*End of else*/
		}/*End of while*/

		hal4nfc_report((" phHal4Nfc_TargetDiscoveryComplete After while --> Count = %d", Count));

		/*If Upper layer is interested only in P2P notifications*/
		if((NULL != Hal4Ctxt->sUpperLayerInfo.pP2PNotification)
		   &&(((Hal4Ctxt->psADDCtxtInfo->nbr_of_devices == 1)
			  &&(phHal_eNfcIP1_Target == Hal4Ctxt->rem_dev_list[0]->RemDevType))
			  ||(NULL == Hal4Ctxt->sUpperLayerInfo.pTagDiscoveryNotification))
			)
		{
			PHDBG_INFO("Hal4:Trying to notify P2P Listener");
			/*NFCSTATUS_SUCCESS or NFCSTATUS_MULTIPLE_PROTOCOLS*/
			if((NFCSTATUS_SUCCESS == status)
				||(NFCSTATUS_MULTIPLE_PROTOCOLS == status))
			{
				/*Pick only the P2P target device info from the list*/
				for(Count = Hal4Ctxt->psADDCtxtInfo->nbr_of_devices;
					Count > 0;--Count)
				{
					/*Only one P2P target can be detected in one discovery*/
					if(phHal_eNfcIP1_Target ==
						Hal4Ctxt->rem_dev_list[Count-1]->RemDevType)
					{
						(void)memcpy(
							(void *)Hal4Ctxt->rem_dev_list[0],
							(void *)Hal4Ctxt->rem_dev_list[Count-1],
									sizeof(phHal_sRemoteDevInformation_t)
									);
						NfcIpDeviceCount = 1;
						break;
					}
				}
				/*If any P2p devices are discovered free other device info*/
				while(Hal4Ctxt->psADDCtxtInfo->nbr_of_devices > NfcIpDeviceCount)
				{
					phOsalNfc_FreeMemory(Hal4Ctxt->rem_dev_list[
						--Hal4Ctxt->psADDCtxtInfo->nbr_of_devices]);
					Hal4Ctxt->rem_dev_list[
						Hal4Ctxt->psADDCtxtInfo->nbr_of_devices] = NULL;
				}
				/*Issue P2P notification*/
				if(NfcIpDeviceCount == 1)
				{
					sDiscoveryInfo.NumberOfDevices
						= Hal4Ctxt->psADDCtxtInfo->nbr_of_devices;
					sDiscoveryInfo.ppRemoteDevInfo = Hal4Ctxt->rem_dev_list;
					uNotificationInfo.psDiscoveryInfo = &sDiscoveryInfo;
					PHDBG_INFO("Hal4:Calling P2P listener");
					(*Hal4Ctxt->sUpperLayerInfo.pP2PNotification)(
							(void *)(Hal4Ctxt->sUpperLayerInfo.P2PDiscoveryCtxt),
							NFC_DISCOVERY_NOTIFICATION,
							uNotificationInfo,
							NFCSTATUS_SUCCESS
							);
				}
				else/*Restart Discovery wheel*/
				{
					PHDBG_INFO("Hal4:No P2P device in list");
					Hal4Ctxt->psADDCtxtInfo->nbr_of_devices = 0;
					PHDBG_INFO("Hal4:Restart discovery1");
#ifndef HAL_NFC_TI_DEVICE
					status = phHciNfc_Restart_Discovery (
										(void *)Hal4Ctxt->psHciHandle,
										(void *)gpphHal4Nfc_Hwref,
										FALSE
										);
					Hal4Ctxt->Hal4NextState = (NFCSTATUS_PENDING == status?
										                eHal4StateConfiguring:
										            Hal4Ctxt->Hal4NextState);
#endif
				}
			}
			/*More discovery info available ,get next info from HCI*/
			else if((NFCSTATUS_MULTIPLE_TAGS == status)
				&&(Hal4Ctxt->psADDCtxtInfo->nbr_of_devices
					 < MAX_REMOTE_DEVICES))
			{
#ifndef HAL_NFC_TI_DEVICE
				status = phHciNfc_Select_Next_Target (
					Hal4Ctxt->psHciHandle,
					(void *)gpphHal4Nfc_Hwref
					);
#endif
			}
			else/*Failed discovery ,restart discovery*/
			{
				Hal4Ctxt->psADDCtxtInfo->nbr_of_devices = 0;
				PHDBG_INFO("Hal4:Restart discovery2");
#ifndef HAL_NFC_TI_DEVICE
				status = phHciNfc_Restart_Discovery (
										(void *)Hal4Ctxt->psHciHandle,
										(void *)gpphHal4Nfc_Hwref,
										FALSE
										);/*Restart Discovery wheel*/
				Hal4Ctxt->Hal4NextState = (NFCSTATUS_PENDING == status?
										        eHal4StateConfiguring:
										        Hal4Ctxt->Hal4NextState);
#endif
			}
		}/*if((NULL != Hal4Ctxt->sUpperLayerInfo.pP2PNotification)...*/
		/*Notify if Upper layer is interested in tag notifications,also notify
		  P2p if its in the list with other tags*/
		else if(NULL != Hal4Ctxt->sUpperLayerInfo.pTagDiscoveryNotification)
		{
			hal4nfc_report((" phHal4Nfc_TargetDiscoveryComplete Trying to notify Tag notification"));
			PHDBG_INFO("Hal4:Trying to notify Tag notification");
			/*Multiple tags in field, get discovery info a second time for the
			  other devices*/
			if((NFCSTATUS_MULTIPLE_TAGS == status)
				&&(Hal4Ctxt->psADDCtxtInfo->nbr_of_devices < MAX_REMOTE_DEVICES))
			{
				PHDBG_INFO("Hal4:select next target1");
#ifndef HAL_NFC_TI_DEVICE
				status = phHciNfc_Select_Next_Target (
										            Hal4Ctxt->psHciHandle,
										            (void *)gpphHal4Nfc_Hwref
										            );
#endif
			}
			/*Single tag multiple protocols scenario,Notify Multiple Protocols
			  status to upper layer*/
			else if(status == NFCSTATUS_MULTIPLE_PROTOCOLS)
			{
				PHDBG_INFO("Hal4:Multiple Tags or protocols");
				sDiscoveryInfo.NumberOfDevices
					= Hal4Ctxt->psADDCtxtInfo->nbr_of_devices;
				sDiscoveryInfo.ppRemoteDevInfo = Hal4Ctxt->rem_dev_list;
				uNotificationInfo.psDiscoveryInfo = &sDiscoveryInfo;
				(*Hal4Ctxt->sUpperLayerInfo.pTagDiscoveryNotification)(
							(void *)(Hal4Ctxt->sUpperLayerInfo.DiscoveryCtxt),
							NFC_DISCOVERY_NOTIFICATION,
							uNotificationInfo,
							status
							);
			}
			else /*NFCSTATUS_SUCCESS*/
			{
				if(((Hal4Ctxt->psADDCtxtInfo->nbr_of_devices == 1)
					 &&(phHal_eNfcIP1_Target
					 == Hal4Ctxt->rem_dev_list[0]->RemDevType))
					 ||(NFCSTATUS_SUCCESS != status)
					 || (Hal4Ctxt->psADDCtxtInfo->nbr_of_devices == 0)
					 )/*device detected but upper layer is not interested
					   in the type(P2P) or activate next failed*/
				{
					while(Hal4Ctxt->psADDCtxtInfo->nbr_of_devices > 0)
					{
						phOsalNfc_FreeMemory(Hal4Ctxt->rem_dev_list[
							--Hal4Ctxt->psADDCtxtInfo->nbr_of_devices]);
						Hal4Ctxt->rem_dev_list[
							Hal4Ctxt->psADDCtxtInfo->nbr_of_devices] = NULL;
					}
					PHDBG_INFO("Hal4:Restart discovery3");
#ifndef HAL_NFC_TI_DEVICE
					status = phHciNfc_Restart_Discovery (
						(void *)Hal4Ctxt->psHciHandle,
						(void *)gpphHal4Nfc_Hwref,
						FALSE
						);/*Restart Discovery wheel*/
					Hal4Ctxt->Hal4NextState = (
						NFCSTATUS_PENDING == status?eHal4StateConfiguring
										            :Hal4Ctxt->Hal4NextState
										            );
#endif
				}
				else/*All remote device info available.Notify to upper layer*/
				{
					hal4nfc_report((" phHal4Nfc_TargetDiscoveryComplete All remote device info available"));
					/*Update status for MULTIPLE_TAGS here*/
					status = (Hal4Ctxt->psADDCtxtInfo->nbr_of_devices > 1?
								NFCSTATUS_MULTIPLE_TAGS:status);
					/*If listener is registered ,call it*/
					sDiscoveryInfo.NumberOfDevices
						= Hal4Ctxt->psADDCtxtInfo->nbr_of_devices;
					sDiscoveryInfo.ppRemoteDevInfo
						= Hal4Ctxt->rem_dev_list;
					uNotificationInfo.psDiscoveryInfo = &sDiscoveryInfo;
					PHDBG_INFO("Hal4:Calling Discovery Handler1");
					hal4nfc_report((" phHal4Nfc_TargetDiscoveryComplete Calling Discovery Handler1 Before"));
					(*Hal4Ctxt->sUpperLayerInfo.pTagDiscoveryNotification)(
						(void *)(Hal4Ctxt->sUpperLayerInfo.DiscoveryCtxt),
						NFC_DISCOVERY_NOTIFICATION,
						uNotificationInfo,
						status
						);
					hal4nfc_report((" phHal4Nfc_TargetDiscoveryComplete Calling Discovery Handler1 After"));
				}
			}
		} /*else if(NULL != Hal4Ctxt->sUpperLayerInfo.pTagDiscoveryNotification)*/
		else/*listener not registered ,Restart Discovery wheel*/
		{
			hal4nfc_report((" phHal4Nfc_TargetDiscoveryComplete No listener registered"));
			PHDBG_INFO("Hal4:No listener registered.Ignoring Discovery  \
						Notification");
			Hal4Ctxt->psADDCtxtInfo->nbr_of_devices = 0;
			PHDBG_INFO("Hal4:Restart discovery4");
#ifndef HAL_NFC_TI_DEVICE
			status = phHciNfc_Restart_Discovery (
								(void *)Hal4Ctxt->psHciHandle,
								(void *)gpphHal4Nfc_Hwref,
								FALSE
								);
			Hal4Ctxt->Hal4NextState = (NFCSTATUS_PENDING == status?
										        eHal4StateConfiguring:
										    Hal4Ctxt->Hal4NextState);
#endif
		}
	}/*if(NULL != ((phNfc_sCompletionInfo_t *)pInfo)->info)*/
	else/*NULL info received*/
	{
		hal4nfc_report((" phHal4Nfc_TargetDiscoveryComplete NULL info received"));
		sDiscoveryInfo.NumberOfDevices
			= Hal4Ctxt->psADDCtxtInfo->nbr_of_devices;
		sDiscoveryInfo.ppRemoteDevInfo = Hal4Ctxt->rem_dev_list;
		uNotificationInfo.psDiscoveryInfo = &sDiscoveryInfo;
		/*If Discovery info is available from previous notifications try to
		  notify that to the upper layer*/
		if((NULL != Hal4Ctxt->sUpperLayerInfo.pTagDiscoveryNotification)
#ifdef NFC_RF_NOISE_SW
		  &&((NFCSTATUS_SUCCESS == status)
			|| (NFCSTATUS_MULTIPLE_TAGS == status))
#endif /* #ifdef NFC_RF_NOISE_SW */
			)
		{
#ifndef NFC_RF_NOISE_SW
			status = (((NFCSTATUS_SUCCESS != status)
				  && (NFCSTATUS_MULTIPLE_TAGS != status))
				   &&(Hal4Ctxt->psADDCtxtInfo->nbr_of_devices != 0))?
					NFCSTATUS_SUCCESS:status;
#endif/*#ifndef NFC_RF_NOISE_SW*/
			hal4nfc_report((" phHal4Nfc_TargetDiscoveryComplete Calling Discovery Handler2 before"));
			PHDBG_INFO("Hal4:Calling Discovery Handler2");
			(*Hal4Ctxt->sUpperLayerInfo.pTagDiscoveryNotification)(
				(void *)(Hal4Ctxt->sUpperLayerInfo.DiscoveryCtxt),
				NFC_DISCOVERY_NOTIFICATION,
				uNotificationInfo,
				status
				);
			hal4nfc_report((" phHal4Nfc_TargetDiscoveryComplete Calling Discovery Handler2 after"));
		}
		else/*Restart Discovery wheel*/
		{
			Hal4Ctxt->psADDCtxtInfo->nbr_of_devices = 0;
			PHDBG_INFO("Hal4:Restart discovery5");
#ifndef HAL_NFC_TI_DEVICE
			status = phHciNfc_Restart_Discovery (
				(void *)Hal4Ctxt->psHciHandle,
				(void *)gpphHal4Nfc_Hwref,
				FALSE
				);
			Hal4Ctxt->Hal4NextState = (NFCSTATUS_PENDING == status?
								   eHal4StateConfiguring:Hal4Ctxt->Hal4NextState);
#endif
		}
	}/*else*/

	hal4nfc_report((" phHal4Nfc_TargetDiscoveryComplete END"));
	return;
}


/**Register Notification handlers*/
NFCSTATUS phHal4Nfc_RegisterNotification(
						phHal_sHwReference_t         *psHwReference,
						phHal4Nfc_RegisterType_t      eRegisterType,
						pphHal4Nfc_Notification_t     pNotificationHandler,
						void                         *Context
						)
{
	NFCSTATUS RetStatus = NFCSTATUS_SUCCESS;
	phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt = NULL;

	hal4nfc_report((" phHal4Nfc_RegisterNotification, eRegisterType=%d", eRegisterType));

	if(NULL == pNotificationHandler || NULL == psHwReference)
	{
		phOsalNfc_RaiseException(phOsalNfc_e_PrecondFailed,1);
		RetStatus = PHNFCSTVAL(CID_NFC_HAL , NFCSTATUS_INVALID_PARAMETER);
	}
	else if((NULL == psHwReference->hal_context)
						|| (((phHal4Nfc_Hal4Ctxt_t *)
								psHwReference->hal_context)->Hal4CurrentState
										       < eHal4StateOpenAndReady)
						|| (((phHal4Nfc_Hal4Ctxt_t *)
								psHwReference->hal_context)->Hal4NextState
										       == eHal4StateClosed))
	{
		phOsalNfc_RaiseException(phOsalNfc_e_PrecondFailed,1);
		RetStatus = PHNFCSTVAL(CID_NFC_HAL , NFCSTATUS_NOT_INITIALISED);
	}
	else
	{
		/*Extract context from hardware reference*/
		Hal4Ctxt = (phHal4Nfc_Hal4Ctxt_t *)psHwReference->hal_context;
		switch(eRegisterType)
		{
			case eRegisterTagDiscovery:
				Hal4Ctxt->sUpperLayerInfo.DiscoveryCtxt = Context;
				Hal4Ctxt->sUpperLayerInfo.pTagDiscoveryNotification
					= pNotificationHandler;  /*Register the tag Notification*/
				break;
			case eRegisterP2PDiscovery:
				Hal4Ctxt->sUpperLayerInfo.P2PDiscoveryCtxt = Context;
				Hal4Ctxt->sUpperLayerInfo.pP2PNotification
					= pNotificationHandler;  /*Register the P2P Notification*/
				break;
			case eRegisterHostCardEmulation:
				RetStatus = NFCSTATUS_FEATURE_NOT_SUPPORTED;
				break;
			case eRegisterSecureElement:
				Hal4Ctxt->sUpperLayerInfo.EventNotificationCtxt = Context;
				Hal4Ctxt->sUpperLayerInfo.pEventNotification
					   = pNotificationHandler; /*Register the Se Notification*/
				break;
			default:
				Hal4Ctxt->sUpperLayerInfo.DefaultListenerCtxt = Context;
				Hal4Ctxt->sUpperLayerInfo.pDefaultEventHandler
					= pNotificationHandler;  /*Register the default Notification*/
				break;
		}
		PHDBG_INFO("Hal4:listener registered");
	}
	return RetStatus;
}


/**Unregister Notification handlers*/
NFCSTATUS phHal4Nfc_UnregisterNotification(
								  phHal_sHwReference_t     *psHwReference,
								  phHal4Nfc_RegisterType_t  eRegisterType,
								  void                     *Context
								  )
{
	NFCSTATUS RetStatus = NFCSTATUS_SUCCESS;
	phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt = NULL;

	hal4nfc_report((" phHal4Nfc_UnregisterNotification, eRegisterType=%d", eRegisterType));

	if(psHwReference == NULL)
	{
		phOsalNfc_RaiseException(phOsalNfc_e_PrecondFailed,1);
		RetStatus = PHNFCSTVAL(CID_NFC_HAL , NFCSTATUS_INVALID_PARAMETER);
	}
	else if((NULL == psHwReference->hal_context)
						|| (((phHal4Nfc_Hal4Ctxt_t *)
								psHwReference->hal_context)->Hal4CurrentState
										       < eHal4StateOpenAndReady)
						|| (((phHal4Nfc_Hal4Ctxt_t *)
								psHwReference->hal_context)->Hal4NextState
										       == eHal4StateClosed))
	{
		phOsalNfc_RaiseException(phOsalNfc_e_PrecondFailed,1);
		RetStatus = PHNFCSTVAL(CID_NFC_HAL , NFCSTATUS_NOT_INITIALISED);
	}
	else
	{
		/*Extract context from hardware reference*/
		Hal4Ctxt = (phHal4Nfc_Hal4Ctxt_t *)psHwReference->hal_context;
		switch(eRegisterType)
		{
		case eRegisterTagDiscovery:
			Hal4Ctxt->sUpperLayerInfo.psUpperLayerCtxt = Context;
			Hal4Ctxt->sUpperLayerInfo.DiscoveryCtxt = NULL;
			/*UnRegister the tag Notification*/
			Hal4Ctxt->sUpperLayerInfo.pTagDiscoveryNotification = NULL;
			PHDBG_INFO("Hal4:Tag Discovery Listener Unregistered");
			break;
		case eRegisterP2PDiscovery:
			Hal4Ctxt->sUpperLayerInfo.P2PDiscoveryCtxt = NULL;
			/*UnRegister the p2p Notification*/
			Hal4Ctxt->sUpperLayerInfo.pP2PNotification = NULL;
			PHDBG_INFO("Hal4:P2P Discovery Listener Unregistered");
			break;
		case eRegisterHostCardEmulation:/*RFU*/
			RetStatus = NFCSTATUS_FEATURE_NOT_SUPPORTED;
			break;
			/*UnRegister the Se Notification*/
		case eRegisterSecureElement:
			Hal4Ctxt->sUpperLayerInfo.EventNotificationCtxt = NULL;
			Hal4Ctxt->sUpperLayerInfo.pEventNotification = NULL;
			PHDBG_INFO("Hal4:SE Listener Unregistered");
			break;
		default:
			Hal4Ctxt->sUpperLayerInfo.DefaultListenerCtxt = NULL;
			/*UnRegister the default Notification*/
			Hal4Ctxt->sUpperLayerInfo.pDefaultEventHandler = NULL;
			PHDBG_INFO("Hal4:Default Listener Unregistered");
			break;
		}
	}
	return RetStatus;
}


void phHal4Nfc_Convert2NciRfDiscovery(phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt, op_param_u *op)
{
	const phHal_sADD_Cfg_t * const discoveryCfg = &(Hal4Ctxt->psADDCtxtInfo->sADDCfg);
	nfc_u16 ce_bit_mask = 0;

	op->rf_enable.ce_bit_mask = 0;
	op->rf_enable.rw_bit_mask = 0;

#if (NCI_VERSION_IN_USE==NCI_DRAFT_9)
	if (discoveryCfg->PollDevInfo.PollCfgInfo.EnableIso14443A == TRUE)
	{
		op->rf_enable.rw_bit_mask |= NAL_PROTOCOL_READER_ISO_14443_4_A;	// Type 4A
		op->rf_enable.rw_bit_mask |= NAL_PROTOCOL_READER_ISO_14443_3_A;	// Type 2 (or MifareUL)
	}

	if (discoveryCfg->PollDevInfo.PollCfgInfo.EnableIso14443B == TRUE)
		op->rf_enable.rw_bit_mask |= (NAL_PROTOCOL_READER_ISO_14443_4_B/*|NAL_PROTOCOL_READER_ISO_14443_3_B*/); //Type 4B

	//if (discoveryCfg->PollDevInfo.PollCfgInfo.EnableFelica212 == TRUE)
	//	op->rf_enable.rw_bit_mask |= NAL_PROTOCOL_READER_FELICA;

	//if (discoveryCfg->PollDevInfo.PollCfgInfo.EnableFelica424 == TRUE)
	//	op->rf_enable.rw_bit_mask |= NAL_PROTOCOL_READER_FELICA;

	//if (discoveryCfg->PollDevInfo.PollCfgInfo.EnableIso15693 == TRUE)
	//	op->rf_enable.rw_bit_mask |= (NAL_PROTOCOL_READER_ISO_15693_3|NAL_PROTOCOL_READER_ISO_15693_2);

	//if (discoveryCfg->PollDevInfo.PollCfgInfo.EnableNfcActive == TRUE)
	//	op->rf_enable.rw_bit_mask |= 0;	// We don't support Active mode yet

	//if (discoveryCfg->PollDevInfo.PollCfgInfo.DisableCardEmulation == FALSE)
	//	op->rf_enable.ce_bit_mask |= 0;

	if (discoveryCfg->NfcIP_Mode == phNfc_eInvalidP2PMode)
	{
		// Disable P2P initiator & target - do not set bits
		hal4nfc_report((" NfcIP_Mode = phNfc_eInvalidP2PMode, P2P disabled"));
	}
	else
	{
		op->rf_enable.rw_bit_mask |= NAL_PROTOCOL_READER_P2P_INITIATOR;
		ce_bit_mask |= NAL_PROTOCOL_CARD_P2P_TARGET;
		hal4nfc_report((" NfcIP_Mode != phNfc_eInvalidP2PMode, P2P enabled"));
	}

	// In case of NCI draft 9, we only enable P2P target if SIM is not present
	if (Hal4Ctxt->uicc_id == SE_NOT_CONNECTED &&
		Hal4Ctxt->smx_id  == SE_NOT_CONNECTED &&
		discoveryCfg->NfcIP_Tgt_Disable == FALSE)
	{
		op->rf_enable.ce_bit_mask |= ce_bit_mask;
		hal4nfc_report((" NfcIP_Tgt_Disable is FALSE, enabling P2P Target"));
	}

#elif (NCI_VERSION_IN_USE==NCI_VER_1)
	if (discoveryCfg->PollDevInfo.PollCfgInfo.EnableIso14443A == TRUE)
	{
		op->rf_enable.rw_bit_mask |= NCI_PROTOCOL_READER_ISO_14443_4_A;	// Type 4A
		op->rf_enable.rw_bit_mask |= NCI_PROTOCOL_READER_ISO_14443_3_A;	// Type 2 (or MifareUL)
		op->rf_enable.rw_bit_mask |= NCI_PROTOCOL_READER_TYPE_1_CHIP;	// Type 1 (or Jewel/Topaz)
	}

	if (discoveryCfg->PollDevInfo.PollCfgInfo.EnableIso14443B == TRUE)
		op->rf_enable.rw_bit_mask |= (NCI_PROTOCOL_READER_ISO_14443_4_B/*|NCI_PROTOCOL_READER_ISO_14443_3_B*/); //Type 4B

	if (discoveryCfg->PollDevInfo.PollCfgInfo.EnableFelica212 == TRUE)
		op->rf_enable.rw_bit_mask |= NCI_PROTOCOL_READER_FELICA;

	if (discoveryCfg->PollDevInfo.PollCfgInfo.EnableFelica424 == TRUE)
		op->rf_enable.rw_bit_mask |= NCI_PROTOCOL_READER_FELICA;

	//	if (discoveryCfg->PollDevInfo.PollCfgInfo.EnableIso15693 == TRUE)
	//		op->rf_enable.rw_bit_mask |= (NCI_PROTOCOL_READER_ISO_15693_3|NCI_PROTOCOL_READER_ISO_15693_2);

	//	if (discoveryCfg->PollDevInfo.PollCfgInfo.EnableNfcActive == TRUE)
	//		op->rf_enable.rw_bit_mask |= 0;	// We don't support Active mode yet

	//	if (discoveryCfg->PollDevInfo.PollCfgInfo.DisableCardEmulation == FALSE)
	//		op->rf_enable.ce_bit_mask |= 0;

	if (discoveryCfg->NfcIP_Mode == phNfc_eInvalidP2PMode)
	{
		// Disable P2P initiator & target - do not set bits
		hal4nfc_report((" NfcIP_Mode = phNfc_eInvalidP2PMode, P2P disabled"));
	}
	else if (discoveryCfg->NfcIP_Mode == phNfc_eDefaultP2PMode)
	{
		// By default enable both A and F technologies for initiator and target
		op->rf_enable.rw_bit_mask |= (NCI_PROTOCOL_READER_P2P_INITIATOR_A | NCI_PROTOCOL_READER_P2P_INITIATOR_F);
		ce_bit_mask |= (NCI_PROTOCOL_CARD_P2P_TARGET_A | NCI_PROTOCOL_CARD_P2P_TARGET_F);
		hal4nfc_report((" NfcIP_Mode = phNfc_eDefaultP2PMode, P2P A and F enabled"));
	}
	else
	{
		// Otherwise, treat the bits sets in NfcIp_Mode separately

		if (discoveryCfg->NfcIP_Mode & phNfc_ePassive106)
		{
			// Enable P2P A
			op->rf_enable.rw_bit_mask |= NCI_PROTOCOL_READER_P2P_INITIATOR_A;
			ce_bit_mask |= NCI_PROTOCOL_CARD_P2P_TARGET_A;
			hal4nfc_report((" NfcIP_Mode & phNfc_ePassive106, P2P A enabled"));
		}

		if (discoveryCfg->NfcIP_Mode & phNfc_ePassive212)
		{
			// Enable P2P F - we do not distinguish between 212 and 424 speeds
			op->rf_enable.rw_bit_mask |= NCI_PROTOCOL_READER_P2P_INITIATOR_F;
			ce_bit_mask |= NCI_PROTOCOL_CARD_P2P_TARGET_F;
			hal4nfc_report((" NfcIP_Mode & phNfc_ePassive212, P2P F enabled"));
		}

		if (discoveryCfg->NfcIP_Mode & phNfc_ePassive424)
		{
			// Enable P2P F - we do not distinguish between 212 and 424 speeds
			op->rf_enable.rw_bit_mask |= NCI_PROTOCOL_READER_P2P_INITIATOR_F;
			ce_bit_mask |= NCI_PROTOCOL_CARD_P2P_TARGET_F;
			hal4nfc_report((" NfcIP_Mode & phNfc_ePassive424, P2P F enabled"));
		}

		if (discoveryCfg->NfcIP_Mode & phNfc_eActive106)
		{
			// We do not support Active mode currently
			hal4nfc_report((" NfcIP_Mode & phNfc_eActive106, not supported"));
		}

		if (discoveryCfg->NfcIP_Mode & phNfc_eActive212)
		{
			// We do not support Active mode currently
			hal4nfc_report((" NfcIP_Mode & phNfc_eActive212, not supported"));
		}

		if (discoveryCfg->NfcIP_Mode & phNfc_eActive424)
		{
			// We do not support Active mode currently
			hal4nfc_report((" NfcIP_Mode & phNfc_eActive424, not supported"));
		}
	}

	if (discoveryCfg->NfcIP_Tgt_Disable == FALSE)
	{
		op->rf_enable.ce_bit_mask |= ce_bit_mask;
		hal4nfc_report((" NfcIP_Tgt_Disable is FALSE, enabling P2P Target"));
	}

#endif /* NCI_VERSION_IN_USE==... */

	hal4nfc_report((" phHal4Nfc_Convert2NciRfDiscovery, ce_bit_mask=0x%x, rw_bit_mask=0x%x",
					op->rf_enable.ce_bit_mask, op->rf_enable.rw_bit_mask));
}


static void phHal4Nfc_Convert2RemoteDevInfo(nci_ntf_param_u *pu_ntf, phHal_sRemoteDevInformation_t *remDev)
{
#if (NCI_VERSION_IN_USE==NCI_DRAFT_9)
	struct nci_activate_nfca_poll *p_nfca_poll = &pu_ntf->activate.rf_technology_specific_params.nfca_poll;
	struct nci_activate_nfcb_poll *p_nfcb_poll = &pu_ntf->activate.rf_technology_specific_params.nfcb_poll;
	const nfc_u8 rf_technology_and_mode = pu_ntf->activate.detected_rf_technology_and_mode;
#elif (NCI_VERSION_IN_USE==NCI_VER_1)
	struct nci_specific_nfca_poll   *p_nfca_poll   = &pu_ntf->activate.rf_technology_specific_params.nfca_poll;
	struct nci_specific_nfcb_poll   *p_nfcb_poll   = &pu_ntf->activate.rf_technology_specific_params.nfcb_poll;
	struct nci_specific_nfcf_poll   *p_nfcf_poll   = &pu_ntf->activate.rf_technology_specific_params.nfcf_poll;
	const nfc_u8 rf_technology_and_mode = pu_ntf->activate.activated_rf_technology_and_mode;
#endif

	struct nci_activate_nfca_poll_iso_dep *p_nfca_poll_iso_dep = &pu_ntf->activate.activation_params.nfca_poll_iso_dep;

	struct nci_activate_nfca_poll_nfc_dep *p_nfca_poll_nfc_dep = &pu_ntf->activate.activation_params.nfca_poll_nfc_dep;
	struct nci_activate_nfca_listen_nfc_dep *p_nfca_listen_nfc_dep = &pu_ntf->activate.activation_params.nfca_listen_nfc_dep;

#if (NCI_VERSION_IN_USE==NCI_VER_1)
	struct nci_activate_nfcf_poll_nfc_dep *p_nfcf_poll_nfc_dep = &pu_ntf->activate.activation_params.nfcf_poll_nfc_dep;
	struct nci_activate_nfcf_listen_nfc_dep *p_nfcf_listen_nfc_dep = &pu_ntf->activate.activation_params.nfcf_listen_nfc_dep;
#endif

	remDev->SessionOpened = 0;

	if (rf_technology_and_mode == NFC_A_PASSIVE_POLL_MODE)
	{
		// Local device is in A passive poll mode

		if ((pu_ntf->activate.selected_rf_protocol == NCI_RF_PROTOCOL_ISO_DEP) ||	// remote is type 4a
			(pu_ntf->activate.selected_rf_protocol == NCI_RF_PROTOCOL_T2T))			// remote is type 2 (or MifareUL)
		{
			remDev->RemDevType = phHal_eISO14443_A_PICC;	// tech a

			// Set UID (nfcid1)
			remDev->RemoteDevInfo.Iso14443A_Info.UidLength = p_nfca_poll->nfcid1_len;
			osa_mem_copy(remDev->RemoteDevInfo.Iso14443A_Info.Uid, p_nfca_poll->nfcid1, p_nfca_poll->nfcid1_len);

			// Set Sak (sel_res)
			remDev->RemoteDevInfo.Iso14443A_Info.Sak = p_nfca_poll->sel_res;

#if (NCI_VERSION_IN_USE==NCI_DRAFT_9)
			// Set AtqA (sens_res) [note: copied reversed!]
			remDev->RemoteDevInfo.Iso14443A_Info.AtqA[0] = p_nfca_poll->sens_res[1];
			remDev->RemoteDevInfo.Iso14443A_Info.AtqA[1] = p_nfca_poll->sens_res[0];
#elif (NCI_VERSION_IN_USE==NCI_VER_1)
			// Set AtqA (sens_res)
			remDev->RemoteDevInfo.Iso14443A_Info.AtqA[0] = p_nfca_poll->sens_res[0];
			remDev->RemoteDevInfo.Iso14443A_Info.AtqA[1] = p_nfca_poll->sens_res[1];
#endif

			// Set default values

			// Set Historical bytes to default - bytes T1 � Tk from RATS Response (rats_res)
			remDev->RemoteDevInfo.Iso14443A_Info.AppDataLength = 0;

			// Set MaxDataRate to default - Interface Byte TA(1) from RATS Response (rats_res)
			remDev->RemoteDevInfo.Iso14443A_Info.MaxDataRate = 0;

			// Set Fwi_Sfgt to default - Interface Byte TB(1) from RATS Response (rats_res)
			remDev->RemoteDevInfo.Iso14443A_Info.Fwi_Sfgt = 0x40;

			if (pu_ntf->activate.selected_rf_protocol == NCI_RF_PROTOCOL_ISO_DEP)	// remote is type 4a
			{
				if (p_nfca_poll_iso_dep->rats_res_length > 1)
				{
					uint8_t idx, remaining, T0;

					remaining = (p_nfca_poll_iso_dep->rats_res_length - 2);
					T0 = p_nfca_poll_iso_dep->rats_res[0];
					idx = 1;

					if ((remaining > 0) && (T0 & 0x10))
					{
						// Set Interface Byte TA(1) from RATS Response (rats_res)
						remDev->RemoteDevInfo.Iso14443A_Info.MaxDataRate = p_nfca_poll_iso_dep->rats_res[idx];

						remaining--;
						idx++;
					}

					if ((remaining > 0) && (T0 & 0x20))
					{
						// Set Interface Byte TB(1) from RATS Response (rats_res)
						remDev->RemoteDevInfo.Iso14443A_Info.Fwi_Sfgt = p_nfca_poll_iso_dep->rats_res[idx];

						remaining--;
						idx++;
					}

					if ((remaining > 0) && (T0 & 0x40))
					{
						// No need for Interface Byte TC(1) from RATS Response (rats_res)
						remaining--;
						idx++;
					}

					if (remaining > 0)
					{
						// Set Historical bytes - bytes T1 � Tk from RATS Response (rats_res)
						remDev->RemoteDevInfo.Iso14443A_Info.AppDataLength = remaining;
						osa_mem_copy(remDev->RemoteDevInfo.Iso14443A_Info.AppData, p_nfca_poll_iso_dep->rats_res+idx, remaining);
					}
				}
			}
		}
		else if (pu_ntf->activate.selected_rf_protocol == NCI_RF_PROTOCOL_NFC_DEP)
		{
			// remote is P2P Target
			int i;

			remDev->RemDevType = phHal_eNfcIP1_Target;

			hal4nfc_report((" phHal4Nfc_Convert2RemoteDevInfo RemoteDev P2P Target, NFC_A, ATR_RES len=%d", p_nfca_poll_nfc_dep->atr_res_length));
			for (i=0; i<p_nfca_poll_nfc_dep->atr_res_length; i++) {
				hal4nfc_report(("ATR_RES[%d]=0x%X", i, p_nfca_poll_nfc_dep->atr_res[i]));
			}

			// Set NFCID3I (last 10 bytes of ATR_RES)
			remDev->RemoteDevInfo.NfcIP_Info.NFCID_Length = 10;
			osa_mem_copy(remDev->RemoteDevInfo.NfcIP_Info.NFCID, p_nfca_poll_nfc_dep->atr_res, remDev->RemoteDevInfo.NfcIP_Info.NFCID_Length);

			// Set General bytes (not including last 15 bytes of ATR_RES)
			remDev->RemoteDevInfo.NfcIP_Info.ATRInfo_Length = (p_nfca_poll_nfc_dep->atr_res_length-15);
			osa_mem_copy(remDev->RemoteDevInfo.NfcIP_Info.ATRInfo, p_nfca_poll_nfc_dep->atr_res+15, remDev->RemoteDevInfo.NfcIP_Info.ATRInfo_Length);

			// Set Sak (sel_res)
			remDev->RemoteDevInfo.NfcIP_Info.SelRes = p_nfca_poll->sel_res;

#if (NCI_VERSION_IN_USE==NCI_DRAFT_9)
			// Set AtqA (sens_res) [note: copied reversed!]
			remDev->RemoteDevInfo.NfcIP_Info.SenseRes[0] = p_nfca_poll->sens_res[1];
			remDev->RemoteDevInfo.NfcIP_Info.SenseRes[1] = p_nfca_poll->sens_res[0];
#elif (NCI_VERSION_IN_USE==NCI_VER_1)
			// Set AtqA (sens_res)
			remDev->RemoteDevInfo.NfcIP_Info.SenseRes[0] = p_nfca_poll->sens_res[0];
			remDev->RemoteDevInfo.NfcIP_Info.SenseRes[1] = p_nfca_poll->sens_res[1];
#endif

			remDev->RemoteDevInfo.NfcIP_Info.Nfcip_Active = 0;

			switch ((p_nfca_poll_nfc_dep->atr_res[14] >> 4) & 0x03)
			{
			case 0:
				remDev->RemoteDevInfo.NfcIP_Info.MaxFrameLength = 64;
				break;
			case 1:
				remDev->RemoteDevInfo.NfcIP_Info.MaxFrameLength = 128;
				break;
			case 2:
				remDev->RemoteDevInfo.NfcIP_Info.MaxFrameLength = 192;
				break;
			case 3:
				remDev->RemoteDevInfo.NfcIP_Info.MaxFrameLength = 254;
				break;
			}
			hal4nfc_report((" phHal4Nfc_Convert2RemoteDevInfo NfcIP_Info.MaxFrameLength = %d", remDev->RemoteDevInfo.NfcIP_Info.MaxFrameLength));

			remDev->RemoteDevInfo.NfcIP_Info.Nfcip_Datarate = phNfc_eDataRate_106;
#if (NCI_VERSION_IN_USE==NCI_VER_1)
			hal4nfc_report((" phHal4Nfc_Convert2RemoteDevInfo NfcIP_Info.Nfcip_Datarate = phNfc_eDataRate_106, RXbps = %d, TXbps=%d",
				pu_ntf->activate.data_exchange_receive_bitrate,
				pu_ntf->activate.data_exchange_transmit_bitrate));
#endif
		}
	}
	else if (rf_technology_and_mode == NFC_B_PASSIVE_POLL_MODE)
	{
		// Local device is in B passive poll mode

		if (pu_ntf->activate.selected_rf_protocol == NCI_RF_PROTOCOL_ISO_DEP) 	// remote is type 4b
		{
			nfc_u8 sensb_res_len = 0;
			remDev->RemDevType = phHal_eISO14443_B_PICC;	// tech b

			hal4nfc_report((" phHal4Nfc_Convert2RemoteDevInfo attrib_resp_len = %d sensb_len = %d", p_nfcb_poll->attrib_resp_len, p_nfcb_poll->sensb_len));

			//attrib_resp
			if(p_nfcb_poll->attrib_resp_len > 0)
			{
				remDev->RemoteDevInfo.Iso14443B_Info.HiLayerRespLength = p_nfcb_poll->attrib_resp_len - 1;
				osa_mem_copy(remDev->RemoteDevInfo.Iso14443B_Info.HiLayerResp, &(p_nfcb_poll->attrib_resp[1]), remDev->RemoteDevInfo.Iso14443B_Info.HiLayerRespLength);
			}


			//sensb_res
			if(p_nfcb_poll->sensb_len > PHHAL_ATQB_LENGTH)
				sensb_res_len = PHHAL_ATQB_LENGTH;
			else
				sensb_res_len = p_nfcb_poll->sensb_len;
			remDev->RemoteDevInfo.Iso14443B_Info.Afi = p_nfcb_poll->sensb_res[5];//byte 0 of Application Data of sensb_res
			osa_mem_copy(remDev->RemoteDevInfo.Iso14443B_Info.AtqB.AtqRes, p_nfcb_poll->sensb_res, PHHAL_ATQB_LENGTH);


			remDev->RemoteDevInfo.Iso14443B_Info.MaxDataRate = phNfc_eDataRate_106;


		}
	}
#if (NCI_VERSION_IN_USE==NCI_VER_1)
	else if (rf_technology_and_mode == NFC_F_PASSIVE_POLL_MODE)
	{
		// Local device is in F passive poll mode
		hal4nfc_report((" phHal4Nfc_Convert2RemoteDevInfo RemoteDev F_PASSIVE_POLL"));

		if (pu_ntf->activate.selected_rf_protocol == NCI_RF_PROTOCOL_T3T)			// remote is type 3
		{
			remDev->RemDevType = phHal_eFelica_PICC;	// tech f

			osa_mem_zero(&remDev->RemoteDevInfo.Felica_Info, sizeof(phNfc_sFelicaInfo_t));

			remDev->RemoteDevInfo.Felica_Info.IDmLength = PHHAL_FEL_ID_LEN;
			osa_mem_copy(remDev->RemoteDevInfo.Felica_Info.IDm, p_nfcf_poll->sensf_res,                    PHHAL_FEL_ID_LEN);
			osa_mem_copy(remDev->RemoteDevInfo.Felica_Info.PMm, p_nfcf_poll->sensf_res + PHHAL_FEL_ID_LEN, PHHAL_FEL_PM_LEN);

			hal4nfc_report((" IDm = %02X%02X%02X%02X%02X%02X%02X%02X PMm = %02X%02X%02X%02X%02X%02X%02X%02X",
				(nfc_u32) remDev->RemoteDevInfo.Felica_Info.IDm[0],
				(nfc_u32) remDev->RemoteDevInfo.Felica_Info.IDm[1],
				(nfc_u32) remDev->RemoteDevInfo.Felica_Info.IDm[2],
				(nfc_u32) remDev->RemoteDevInfo.Felica_Info.IDm[3],
				(nfc_u32) remDev->RemoteDevInfo.Felica_Info.IDm[4],
				(nfc_u32) remDev->RemoteDevInfo.Felica_Info.IDm[5],
				(nfc_u32) remDev->RemoteDevInfo.Felica_Info.IDm[6],
				(nfc_u32) remDev->RemoteDevInfo.Felica_Info.IDm[7],
				(nfc_u32) remDev->RemoteDevInfo.Felica_Info.PMm[0],
				(nfc_u32) remDev->RemoteDevInfo.Felica_Info.PMm[1],
				(nfc_u32) remDev->RemoteDevInfo.Felica_Info.PMm[2],
				(nfc_u32) remDev->RemoteDevInfo.Felica_Info.PMm[3],
				(nfc_u32) remDev->RemoteDevInfo.Felica_Info.PMm[4],
				(nfc_u32) remDev->RemoteDevInfo.Felica_Info.PMm[5],
				(nfc_u32) remDev->RemoteDevInfo.Felica_Info.PMm[6],
				(nfc_u32) remDev->RemoteDevInfo.Felica_Info.PMm[7]
			));

			if (p_nfcf_poll->sensf_len == 18) {
				// We have System code
				osa_mem_copy(remDev->RemoteDevInfo.Felica_Info.SystemCode, p_nfcf_poll->sensf_res + PHHAL_FEL_ID_LEN
										                                                          + PHHAL_FEL_PM_LEN, 2);
				hal4nfc_report((" SystemCode = %02X%02X",
					(nfc_u32) remDev->RemoteDevInfo.Felica_Info.SystemCode[0],
					(nfc_u32) remDev->RemoteDevInfo.Felica_Info.SystemCode[1]
				));
			}
			else
			{
				remDev->RemoteDevInfo.Felica_Info.SystemCode[0] = 0xff;
				remDev->RemoteDevInfo.Felica_Info.SystemCode[1] = 0xff;

				hal4nfc_report((" SystemCode = default"));
			}
		}
		else if (pu_ntf->activate.selected_rf_protocol == NCI_RF_PROTOCOL_NFC_DEP)
		{
			// remote is P2P Target
			int i;

			remDev->RemDevType = phHal_eNfcIP1_Target;

			hal4nfc_report((" phHal4Nfc_Convert2RemoteDevInfo RemoteDev P2P Target, NFC_F, ATR_RES len=%d", p_nfcf_poll_nfc_dep->atr_res_length));
			for (i=0; i<p_nfcf_poll_nfc_dep->atr_res_length; i++) {
				hal4nfc_report(("ATR_RES[%d]=0x%X", i, p_nfcf_poll_nfc_dep->atr_res[i]));
			}

			// Set NFCID3I (last 10 bytes of ATR_RES)
			remDev->RemoteDevInfo.NfcIP_Info.NFCID_Length = 10;
			osa_mem_copy(remDev->RemoteDevInfo.NfcIP_Info.NFCID, p_nfcf_poll_nfc_dep->atr_res, remDev->RemoteDevInfo.NfcIP_Info.NFCID_Length);

			// Set General bytes (not including last 15 bytes of ATR_RES)
			remDev->RemoteDevInfo.NfcIP_Info.ATRInfo_Length = (p_nfcf_poll_nfc_dep->atr_res_length-15);
			osa_mem_copy(remDev->RemoteDevInfo.NfcIP_Info.ATRInfo, p_nfcf_poll_nfc_dep->atr_res+15, remDev->RemoteDevInfo.NfcIP_Info.ATRInfo_Length);

			// Set Sak (sel_res)
			remDev->RemoteDevInfo.NfcIP_Info.SelRes = 0x80;

			// Set AtqA (sens_res)
			remDev->RemoteDevInfo.NfcIP_Info.SenseRes[0] = 0xff;
			remDev->RemoteDevInfo.NfcIP_Info.SenseRes[1] = 0xff;

			remDev->RemoteDevInfo.NfcIP_Info.Nfcip_Active = 0;

			switch ((p_nfcf_poll_nfc_dep->atr_res[14] >> 4) & 0x03)
			{
			case 0:
				remDev->RemoteDevInfo.NfcIP_Info.MaxFrameLength = 64;
				break;
			case 1:
				remDev->RemoteDevInfo.NfcIP_Info.MaxFrameLength = 128;
				break;
			case 2:
				remDev->RemoteDevInfo.NfcIP_Info.MaxFrameLength = 192;
				break;
			case 3:
				remDev->RemoteDevInfo.NfcIP_Info.MaxFrameLength = 254;
				break;
			}
			hal4nfc_report((" phHal4Nfc_Convert2RemoteDevInfo NfcIP_Info.MaxFrameLength = %d", remDev->RemoteDevInfo.NfcIP_Info.MaxFrameLength));

			switch(p_nfcf_poll->bitrate)
			{
			case 1:
				remDev->RemoteDevInfo.NfcIP_Info.Nfcip_Datarate = phNfc_eDataRate_212;
				hal4nfc_report((" phHal4Nfc_Convert2RemoteDevInfo NfcIP_Info.Nfcip_Datarate = phNfc_eDataRate_212, RXbps = %d, TXbps=%d",
					pu_ntf->activate.data_exchange_receive_bitrate,
					pu_ntf->activate.data_exchange_transmit_bitrate));
				break;
			case 2:
				remDev->RemoteDevInfo.NfcIP_Info.Nfcip_Datarate = phNfc_eDataRate_424;
				hal4nfc_report((" phHal4Nfc_Convert2RemoteDevInfo NfcIP_Info.Nfcip_Datarate = phNfc_eDataRate_424, RXbps = %d, TXbps=%d",
					pu_ntf->activate.data_exchange_receive_bitrate,
					pu_ntf->activate.data_exchange_transmit_bitrate));
				break;
			default:
				remDev->RemoteDevInfo.NfcIP_Info.Nfcip_Datarate = phNfc_eDataRate_RFU;
				hal4nfc_report((" phHal4Nfc_Convert2RemoteDevInfo NfcIP_Info.Nfcip_Datarate = phNfc_eDataRate_RFU, RXbps = %d, TXbps=%d",
					pu_ntf->activate.data_exchange_receive_bitrate,
					pu_ntf->activate.data_exchange_transmit_bitrate));
				break;
			}
		}
	}
	else if (rf_technology_and_mode == NFC_F_PASSIVE_LISTEN_MODE)
	{
		// Local device is in A passive listen mode

		if (pu_ntf->activate.selected_rf_protocol == NCI_RF_PROTOCOL_NFC_DEP)
		{
			// remote is P2P Initiator
			int i;
			remDev->RemDevType = phHal_eNfcIP1_Initiator;

			hal4nfc_report((" phHal4Nfc_Convert2RemoteDevInfo RemoteDev P2P Initiator, NFC_F, ATR_REQ len=%d", p_nfcf_listen_nfc_dep->atr_req_length));
			for (i=0; i<p_nfcf_listen_nfc_dep->atr_req_length; i++) {
				hal4nfc_report(("ATR_REQ[%d]=0x%X", i, p_nfcf_listen_nfc_dep->atr_req[i]));
			}

			// Set NFCID3I (last 10 bytes of ATR_REQ)
			remDev->RemoteDevInfo.NfcIP_Info.NFCID_Length = 10;
			osa_mem_copy(remDev->RemoteDevInfo.NfcIP_Info.NFCID, p_nfcf_listen_nfc_dep->atr_req, remDev->RemoteDevInfo.NfcIP_Info.NFCID_Length);

			// Set General bytes (not including last 14 bytes of ATR_REQ)
			remDev->RemoteDevInfo.NfcIP_Info.ATRInfo_Length = (p_nfcf_listen_nfc_dep->atr_req_length-14);
			osa_mem_copy(remDev->RemoteDevInfo.NfcIP_Info.ATRInfo, p_nfcf_listen_nfc_dep->atr_req+14, remDev->RemoteDevInfo.NfcIP_Info.ATRInfo_Length);

			// Set Sak (sel_res)
			remDev->RemoteDevInfo.NfcIP_Info.SelRes = 0x80;

			// Set AtqA (sens_res)
			remDev->RemoteDevInfo.NfcIP_Info.SenseRes[0] = 0xff;
			remDev->RemoteDevInfo.NfcIP_Info.SenseRes[1] = 0xff;

			remDev->RemoteDevInfo.NfcIP_Info.Nfcip_Active = 0;

			switch ((p_nfcf_listen_nfc_dep->atr_req[13] >> 4) & 0x03)
			{
			case 0:
				remDev->RemoteDevInfo.NfcIP_Info.MaxFrameLength = 64;
				break;
			case 1:
				remDev->RemoteDevInfo.NfcIP_Info.MaxFrameLength = 128;
				break;
			case 2:
				remDev->RemoteDevInfo.NfcIP_Info.MaxFrameLength = 192;
				break;
			case 3:
				remDev->RemoteDevInfo.NfcIP_Info.MaxFrameLength = 254;
				break;
			}
			hal4nfc_report((" phHal4Nfc_Convert2RemoteDevInfo NfcIP_Info.MaxFrameLength = %d", remDev->RemoteDevInfo.NfcIP_Info.MaxFrameLength));

			remDev->RemoteDevInfo.NfcIP_Info.Nfcip_Datarate = phNfc_eDataRate_RFU;
			hal4nfc_report((" phHal4Nfc_Convert2RemoteDevInfo NfcIP_Info.Nfcip_Datarate = phNfc_eDataRate_RFU, RXbps = %d, TXbps=%d",
				pu_ntf->activate.data_exchange_receive_bitrate,
				pu_ntf->activate.data_exchange_transmit_bitrate));
		}
	}
#endif
	else if (rf_technology_and_mode == NFC_A_PASSIVE_LISTEN_MODE)
	{
		// Local device is in A passive listen mode

		if (pu_ntf->activate.selected_rf_protocol == NCI_RF_PROTOCOL_NFC_DEP)
		{
			// remote is P2P Initiator
			int i;
			remDev->RemDevType = phHal_eNfcIP1_Initiator;

			hal4nfc_report((" phHal4Nfc_Convert2RemoteDevInfo RemoteDev P2P Initiator, NFC_A, ATR_REQ len=%d", p_nfca_listen_nfc_dep->atr_req_length));
			for (i=0; i<p_nfca_listen_nfc_dep->atr_req_length; i++) {
				hal4nfc_report(("ATR_REQ[%d]=0x%X", i, p_nfca_listen_nfc_dep->atr_req[i]));
			}

			// Set NFCID3I (last 10 bytes of ATR_REQ)
			remDev->RemoteDevInfo.NfcIP_Info.NFCID_Length = 10;
			osa_mem_copy(remDev->RemoteDevInfo.NfcIP_Info.NFCID, p_nfca_listen_nfc_dep->atr_req, remDev->RemoteDevInfo.NfcIP_Info.NFCID_Length);

			// Set General bytes (not including last 14 bytes of ATR_REQ)
			remDev->RemoteDevInfo.NfcIP_Info.ATRInfo_Length = (p_nfca_listen_nfc_dep->atr_req_length-14);
			osa_mem_copy(remDev->RemoteDevInfo.NfcIP_Info.ATRInfo, p_nfca_listen_nfc_dep->atr_req+14, remDev->RemoteDevInfo.NfcIP_Info.ATRInfo_Length);

			// Set Sak (sel_res)
			remDev->RemoteDevInfo.NfcIP_Info.SelRes = p_nfca_poll->sel_res;

#if (NCI_VERSION_IN_USE==NCI_DRAFT_9)
			// Set AtqA (sens_res) [note: copied reversed!]
			remDev->RemoteDevInfo.NfcIP_Info.SenseRes[0] = p_nfca_poll->sens_res[1];
			remDev->RemoteDevInfo.NfcIP_Info.SenseRes[1] = p_nfca_poll->sens_res[0];
#elif (NCI_VERSION_IN_USE==NCI_VER_1)
			// Set AtqA (sens_res)
			remDev->RemoteDevInfo.NfcIP_Info.SenseRes[0] = p_nfca_poll->sens_res[0];
			remDev->RemoteDevInfo.NfcIP_Info.SenseRes[1] = p_nfca_poll->sens_res[1];
#endif

			remDev->RemoteDevInfo.NfcIP_Info.Nfcip_Active = 0;

			switch ((p_nfca_listen_nfc_dep->atr_req[13] >> 4) & 0x03)
			{
			case 0:
				remDev->RemoteDevInfo.NfcIP_Info.MaxFrameLength = 64;
				break;
			case 1:
				remDev->RemoteDevInfo.NfcIP_Info.MaxFrameLength = 128;
				break;
			case 2:
				remDev->RemoteDevInfo.NfcIP_Info.MaxFrameLength = 192;
				break;
			case 3:
				remDev->RemoteDevInfo.NfcIP_Info.MaxFrameLength = 254;
				break;
			}
			hal4nfc_report((" phHal4Nfc_Convert2RemoteDevInfo NfcIP_Info.MaxFrameLength = %d", remDev->RemoteDevInfo.NfcIP_Info.MaxFrameLength));

			remDev->RemoteDevInfo.NfcIP_Info.Nfcip_Datarate = phNfc_eDataRate_106;
#if (NCI_VERSION_IN_USE==NCI_VER_1)
			hal4nfc_report((" phHal4Nfc_Convert2RemoteDevInfo NfcIP_Info.Nfcip_Datarate = phNfc_eDataRate_106, RXbps = %d, TXbps=%d",
				pu_ntf->activate.data_exchange_receive_bitrate,
				pu_ntf->activate.data_exchange_transmit_bitrate));
#endif
		}
	}
}

#if (NCI_VERSION_IN_USE==NCI_VER_1)

/* Handle Jewel (type 1) RID response and activate */
void pHal4Nfc_Activate_Jewel(nfc_handle_t h_nal, nfc_u8 *p_rid_rsp, nfc_u32 rid_rsp_len, nci_status_e nci_status)
{
	phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt = (phHal4Nfc_Hal4Ctxt_t*)h_nal;
	phHal_sRemoteDevInformation_t remDev;
	phNfc_sCompletionInfo_t info;

	Hal4Ctxt->nci_send_rid_command = 0;

	if (nci_status == NCI_STATUS_OK)
	{
		remDev.RemDevType = phHal_eJewel_PICC;	// type 1 Jewel/Topaz
		remDev.RemoteDevInfo.Jewel_Info.HeaderRom0 = p_rid_rsp[0];
		remDev.RemoteDevInfo.Jewel_Info.HeaderRom1 = p_rid_rsp[1];
		remDev.RemoteDevInfo.Jewel_Info.UidLength  = 4;
		osa_mem_copy(remDev.RemoteDevInfo.Jewel_Info.Uid, &p_rid_rsp[2], remDev.RemoteDevInfo.Jewel_Info.UidLength);

		hal4nfc_report((" pHal4Nfc_Activate_Jewel: Received Type 1 RID response with HR=%02X%02X UID=%02X%02X%02X%02X",
			(nfc_u32) p_rid_rsp[0],
			(nfc_u32) p_rid_rsp[1],
			(nfc_u32) p_rid_rsp[2],
			(nfc_u32) p_rid_rsp[3],
			(nfc_u32) p_rid_rsp[4],
			(nfc_u32) p_rid_rsp[5]));

		hal4nfc_report((" pHal4Nfc_Activate_Jewel: Calling phHal4Nfc_TargetDiscoveryComplete with Type 1 RemDev"));

		info.status = NFCSTATUS_SUCCESS;
	}
	else
	{
		remDev.RemDevType = phHal_eJewel_PICC;	// type 1 Jewel/Topaz
		osa_mem_zero(&remDev.RemoteDevInfo.Jewel_Info, sizeof(remDev.RemoteDevInfo.Jewel_Info));

		hal4nfc_report((" pHal4Nfc_Activate_Jewel: Calling phHal4Nfc_TargetDiscoveryComplete with nci_send_rid_command TIMEOUT (Type 1)"));

		info.status = NFCSTATUS_SUCCESS;
	}

	info.info = &remDev;

	Hal4Ctxt->psADDCtxtInfo->nbr_of_devices = 0;
	phHal4Nfc_TargetDiscoveryComplete(Hal4Ctxt, &info);
}

/* Handle Felica (type 3) SENSF response and activate */
void pHal4Nfc_Activate_Felica(nfc_handle_t h_nal, nfc_u8 *p_sensf_rsp, nfc_u32 sensf_rsp_len, nci_status_e nci_status)
{
	phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt = (phHal4Nfc_Hal4Ctxt_t*)h_nal;
	phHal_sRemoteDevInformation_t remDev;
	phNfc_sCompletionInfo_t info;

	Hal4Ctxt->nci_send_sensf_req_command = 0;

	if (nci_status == NCI_STATUS_OK)
	{
		// Local device is in F passive poll mode
		hal4nfc_report((" pHal4Nfc_Activate_Felica, RemoteDev F_PASSIVE_POLL, sensf_rsp_len=%d", sensf_rsp_len));

		remDev.RemDevType = phHal_eFelica_PICC;	// tech f

		osa_mem_zero(&remDev.RemoteDevInfo.Felica_Info, sizeof(phNfc_sFelicaInfo_t));

		remDev.RemoteDevInfo.Felica_Info.IDmLength = PHHAL_FEL_ID_LEN;
		osa_mem_copy(remDev.RemoteDevInfo.Felica_Info.IDm, p_sensf_rsp,                    PHHAL_FEL_ID_LEN);
		osa_mem_copy(remDev.RemoteDevInfo.Felica_Info.PMm, p_sensf_rsp + PHHAL_FEL_ID_LEN, PHHAL_FEL_PM_LEN);

		hal4nfc_report((" IDm = %02X%02X%02X%02X%02X%02X%02X%02X PMm = %02X%02X%02X%02X%02X%02X%02X%02X",
			(nfc_u32) remDev.RemoteDevInfo.Felica_Info.IDm[0],
			(nfc_u32) remDev.RemoteDevInfo.Felica_Info.IDm[1],
			(nfc_u32) remDev.RemoteDevInfo.Felica_Info.IDm[2],
			(nfc_u32) remDev.RemoteDevInfo.Felica_Info.IDm[3],
			(nfc_u32) remDev.RemoteDevInfo.Felica_Info.IDm[4],
			(nfc_u32) remDev.RemoteDevInfo.Felica_Info.IDm[5],
			(nfc_u32) remDev.RemoteDevInfo.Felica_Info.IDm[6],
			(nfc_u32) remDev.RemoteDevInfo.Felica_Info.IDm[7],
			(nfc_u32) remDev.RemoteDevInfo.Felica_Info.PMm[0],
			(nfc_u32) remDev.RemoteDevInfo.Felica_Info.PMm[1],
			(nfc_u32) remDev.RemoteDevInfo.Felica_Info.PMm[2],
			(nfc_u32) remDev.RemoteDevInfo.Felica_Info.PMm[3],
			(nfc_u32) remDev.RemoteDevInfo.Felica_Info.PMm[4],
			(nfc_u32) remDev.RemoteDevInfo.Felica_Info.PMm[5],
			(nfc_u32) remDev.RemoteDevInfo.Felica_Info.PMm[6],
			(nfc_u32) remDev.RemoteDevInfo.Felica_Info.PMm[7]
		));

		if (sensf_rsp_len >= 18) {
			// We have System code
			osa_mem_copy(remDev.RemoteDevInfo.Felica_Info.SystemCode, p_sensf_rsp + PHHAL_FEL_ID_LEN
				+ PHHAL_FEL_PM_LEN, 2);
			hal4nfc_report((" SystemCode = %02X%02X",
				(nfc_u32) remDev.RemoteDevInfo.Felica_Info.SystemCode[0],
				(nfc_u32) remDev.RemoteDevInfo.Felica_Info.SystemCode[1]
			));
		}
		else
		{
			remDev.RemoteDevInfo.Felica_Info.SystemCode[0] = 0xff;
			remDev.RemoteDevInfo.Felica_Info.SystemCode[1] = 0xff;

			hal4nfc_report((" SystemCode = default"));
		}

		info.status = NFCSTATUS_SUCCESS;
	}
	else
	{
		remDev.RemDevType = phHal_eFelica_PICC;	// type 3 Felica
		osa_mem_zero(&remDev.RemoteDevInfo.Felica_Info, sizeof(remDev.RemoteDevInfo.Felica_Info));

		hal4nfc_report((" pHal4Nfc_Activate_Felica: Calling phHal4Nfc_TargetDiscoveryComplete with nci_send_sensf_req_command TIMEOUT (Type 3)"));

		info.status = NFCSTATUS_SUCCESS;
	}

	info.info = &remDev;

	Hal4Ctxt->psADDCtxtInfo->nbr_of_devices = 0;
	phHal4Nfc_TargetDiscoveryComplete(Hal4Ctxt, &info);
}

#endif


#if (NCI_VERSION_IN_USE==NCI_VER_1)

/* RF discover notification handler.
   We get this notification in case of multiple tags.
   If NFC_DEP is supported by one of the targets, we will select it. 
   If not, we will choose the last target 				*/
void nci_rf_discover_ntf_handler(nfc_handle_t h_nal, nfc_u16 opcode, nci_ntf_param_u *pu_ntf)
{

	phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt = (phHal4Nfc_Hal4Ctxt_t*)h_nal;
	nci_cmd_param_u rf_discover_select_params;
	nci_status_e status;
	op_param_u rf_enable;
	nfc_u8 rf_interface;
	nfc_bool target_found = FALSE;

	hal4nfc_report((" nci_rf_discover_ntf_handler"));
	hal4nfc_report((" RF_Discover_ID %d, RF Protocol %d, RF technology %d",pu_ntf->discover.rf_discover_id,pu_ntf->discover.rf_protocol, pu_ntf->discover.rf_tech_and_mode));
	hal4nfc_report((" Notification type %d", pu_ntf->discover.notification_type));

	if (pu_ntf->discover.notification_type == 2)
	{
		// Discovery notification - more to follow...

		// If target supports NFC DEP; select it, if not, ignore.
		if (pu_ntf->discover.rf_protocol == NCI_RF_PROTOCOL_NFC_DEP)
		{
			rf_interface = return_rf_interface(h_nal, pu_ntf->discover.rf_protocol);
			if (rf_interface != NFCSTATUS_FAILED)
			{
				target_found = TRUE;
			}
		}
	}
	else if (pu_ntf->discover.notification_type < 2)
	{
		// Discovery notification - last notification

		if (!Hal4Ctxt->rf_discover_target_selected)
		{
			//Target not selected yet, select the last one.
			rf_interface = return_rf_interface(h_nal, pu_ntf->discover.rf_protocol);
			if (rf_interface != NFCSTATUS_FAILED)
			{
				target_found = TRUE;
			}
		}
	}

	if (target_found)
	{
		Hal4Ctxt->rf_discover_target_selected = TRUE;
		Hal4Ctxt->rf_discover_selected_discovery_id = pu_ntf->discover.rf_discover_id;
		Hal4Ctxt->rf_discover_selected_protocol = pu_ntf->discover.rf_protocol;
		Hal4Ctxt->rf_discover_selected_interface = rf_interface;
	}

	if (pu_ntf->discover.notification_type < 2)
	{
		// Last notification - either select target or return to RF discovery

		if (Hal4Ctxt->rf_discover_target_selected)
		{
			rf_discover_select_params.rf_discover_select.rf_discovery_id = Hal4Ctxt->rf_discover_selected_discovery_id;
			rf_discover_select_params.rf_discover_select.rf_protocol = Hal4Ctxt->rf_discover_selected_protocol;
			rf_discover_select_params.rf_discover_select.rf_interface = Hal4Ctxt->rf_discover_selected_interface;

			PHDBG_INFO("Hal4:Calling nci_send_command(NCI_OPCODE_RF_DISCOVER_SEL_CMD)");
			status = nci_send_cmd(Hal4Ctxt->nciHandle,
							NCI_OPCODE_RF_DISCOVER_SEL_CMD,
							&rf_discover_select_params,
							NULL,
							0,
							NULL,
							NULL);
		}
		else
		{
			//Error - no target selected. Go back to RF_discovery
			hal4nfc_report((" Error - no Target selected"));

			nci_start_operation(Hal4Ctxt->nciHandle, NCI_OPERATION_RF_DISABLE, NULL, NULL, 0, NULL, Hal4Ctxt);

			Hal4Ctxt->psADDCtxtInfo->IsPollConfigured = TRUE;
			phHal4Nfc_Convert2NciRfDiscovery(Hal4Ctxt, &rf_enable);
			nci_start_operation(Hal4Ctxt->nciHandle, NCI_OPERATION_RF_ENABLE, &rf_enable, NULL, 0, NULL, Hal4Ctxt);
		}

	}

	return;



}
#endif

/* 	Return the appropriate rf interface according to the rf protocol.

	return value: NCI_RF_INTERFACE_FRAME /
		      NCI_RF_INTERFACE_ISO_DEP /
		      NCI_RF_INTERFACE_NFC_DEP /
		      NFCSTATUS_FAILED for error or not supported interface. */
nfc_u8 return_rf_interface (nfc_handle_t h_nal, nfc_u8 rf_protocol)
{
	nfc_u8 rf_interface;

	if ((rf_protocol == NCI_RF_PROTOCOL_T1T) ||
		(rf_protocol == NCI_RF_PROTOCOL_T2T) ||
		(rf_protocol == NCI_RF_PROTOCOL_T3T))
	{
		rf_interface = NCI_RF_INTERFACE_FRAME;
	}
	else if (rf_protocol == NCI_RF_PROTOCOL_ISO_DEP)
	{
		rf_interface = NCI_RF_INTERFACE_ISO_DEP;

	}
	else if (rf_protocol == NCI_RF_PROTOCOL_NFC_DEP)
	{
		rf_interface = NCI_RF_INTERFACE_NFC_DEP;
	}
	else
	{
		rf_interface = NFCSTATUS_FAILED;
		goto finish;
	}

	if (!is_rf_interface_supported(h_nal, rf_interface))
		rf_interface = NFCSTATUS_FAILED;

finish:
	return rf_interface;

}


nfc_bool is_rf_interface_supported(nfc_handle_t h_nal, nfc_u8 rf_interface)
{
	phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt = (phHal4Nfc_Hal4Ctxt_t*)h_nal;
	nfc_bool supported = FALSE;
	nfc_u8 i;

	for (i=0; i < Hal4Ctxt->nci_start_rsp.init.num_supported_rf_interfaces; i++)
	{
		if (rf_interface == Hal4Ctxt->nci_start_rsp.init.supported_rf_interfaces[i])
		{
			supported = TRUE;
			break;
		}
	}
	return supported;
}


void nci_activate_ntf_handler(nfc_handle_t h_nal, nfc_u16 opcode, nci_ntf_param_u *pu_ntf)
{
	phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt = (phHal4Nfc_Hal4Ctxt_t*)h_nal;
	phHal_sRemoteDevInformation_t remDev;

	hal4nfc_report((" nci_activate_ntf_handler"));

#if (NCI_VERSION_IN_USE==NCI_VER_1)
	// We currently ignore NFCEE RF activation
	if(NCI_RF_INTERFACE_NFCEE_DIRECT == pu_ntf->activate.rf_interface_type)
	{
		hal4nfc_report((" nci_activate_ntf_handler: Ignore NFCEE_DIRECT activation"));
		goto Finish;
	}

	// Handle Type 1 activation - in this case we need to issue and RID command to fetch device UID and HeaderRom bytes
	if (pu_ntf->activate.activated_rf_technology_and_mode == NFC_A_PASSIVE_POLL_MODE &&
		pu_ntf->activate.selected_rf_protocol             == NCI_RF_PROTOCOL_T1T)
	{
		nfc_u8 rid_buff[] = {0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
		// We need to send here the RID command to the tag, before notifying the stack regarding the activation,
		// since stack required HR bytes and UID that are queried by RID command over data
		Hal4Ctxt->nci_send_rid_command = 1;
		nci_send_data(Hal4Ctxt->nciHandle, 0, rid_buff, sizeof(rid_buff), PH_HAL4NFC_TRANSCEIVE_TIMEOUT);
		hal4nfc_report((" nci_activate_ntf_handler: Send RID command to fetch Type 1 info"));

		goto Finish;
	}

	// Handle Type 3 (Felica) activation - in this case we need to issue a SENSF_REQ command to fetch device info including System Code
	// Note!!! This is a temporary solution that will work only with 1 tag. A real solution should be implemented inside the CLF and also
	// support multiple tags.
	// In case of multiple tags (Discovery_Ntf) we will need to use also the last byte (TSN - Time Slot Number) and this should be
	// done in the CLF firmware
	if (pu_ntf->activate.activated_rf_technology_and_mode == NFC_F_PASSIVE_POLL_MODE &&
		pu_ntf->activate.selected_rf_protocol             == NCI_RF_PROTOCOL_T3T)
	{
		nfc_u8 sensf_req_buff[] = {0x00, 0xFF, 0xFF, 0x01, 0x00};
		// We need to send here the SENSF_REQ command to the tag, before notifying the stack regarding the activation,
		// since stack required full SENSF information including System Code bytes that are not returned by Activation Notification and
		// need to be queried by SENSF_REQ command over data
		Hal4Ctxt->nci_send_sensf_req_command = 1;
		nci_send_data(Hal4Ctxt->nciHandle, 0, sensf_req_buff, sizeof(sensf_req_buff), PH_HAL4NFC_TRANSCEIVE_TIMEOUT);
		hal4nfc_report((" nci_activate_ntf_handler: Send SENSF_REQ command to fetch Type 3 info"));

		goto Finish;
	}
#endif

	phHal4Nfc_Convert2RemoteDevInfo(pu_ntf, &remDev);

	if (remDev.RemDevType != phHal_eNfcIP1_Initiator)
	{
		// Local device is Reader or P2P initiator

		phNfc_sCompletionInfo_t info;

		info.status = NFCSTATUS_SUCCESS;
		info.info = &remDev;

		Hal4Ctxt->psADDCtxtInfo->nbr_of_devices = 0;
		phHal4Nfc_TargetDiscoveryComplete(Hal4Ctxt, &info);
	}
	else
	{
		// Local device is P2P target

		phHal_sEventInfo_t info;

		info.eventInfo.pRemoteDevInfo = &remDev;
		phHal4Nfc_P2PActivateComplete(Hal4Ctxt, &info);
	}

#if (NCI_VERSION_IN_USE==NCI_VER_1)
Finish:
	return;
#endif

}


void nci_deactivate_ntf_handler(nfc_handle_t h_nal, nfc_u16 opcode, nci_ntf_param_u *pu_ntf)
{
	phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt = (phHal4Nfc_Hal4Ctxt_t*)h_nal;

	hal4nfc_report((" nci_deactivate_ntf_handler"));

#ifdef TRANSACTION_TIMER
	if((Hal4Ctxt->psTrcvCtxtInfo != NULL) &&
	   (Hal4Ctxt->psTrcvCtxtInfo->TransactionTimerId != PH_OSALNFC_INVALID_TIMER_ID))
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

	if (Hal4Ctxt->Hal4NextState == eHal4StatePresenceCheck)
	{
		phNfc_sCompletionInfo_t info;

		info.status = NFCSTATUS_RF_ERROR;

		phHal4Nfc_PresenceChkComplete(Hal4Ctxt, &info);
	}

	/* Check if we got disconnected during a transaction */
	if (Hal4Ctxt->Hal4NextState == eHal4StateTransaction)
	{
		pphHal4Nfc_ReceiveCallback_t pUpperRecvCb = NULL;
		pphHal4Nfc_TransceiveCallback_t pUpperTrcvCb = NULL;

		hal4nfc_report((" during transaction => simulate RF_TIMEOUT"));

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

	if (Hal4Ctxt->sTgtConnectInfo.EmulationState == NFC_EVT_ACTIVATED)
	{
		// Local device is P2P target

		phHal_sEventInfo_t info;

		info.eventHost = phHal_eHostController;
		info.eventSource = phHal_eNfcIP1_Target;
		info.eventType = NFC_EVT_DEACTIVATED;

		phHal4Nfc_HandleP2PDeActivate(Hal4Ctxt, &info);
	}
}



void nci_core_generic_error_ntf_handler(nfc_handle_t h_nal, nfc_u16 opcode, nci_ntf_param_u *pu_ntf)
{
	phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt = (phHal4Nfc_Hal4Ctxt_t*)h_nal;
	op_param_u rf_enable;

	hal4nfc_report((" nci_core_generic_error_ntf: status=0x%x", pu_ntf->core_generic_error.status));

	switch (pu_ntf->core_generic_error.status)
	{
		case NCI_STATUS_DISCOVERY_TARGET_ACTIVATION_FAILED:

			Hal4Ctxt->rf_discover_target_selected = FALSE;

			/* Restart Discovery*/
			nci_start_operation(Hal4Ctxt->nciHandle, NCI_OPERATION_RF_DISABLE, NULL, NULL, 0, NULL, Hal4Ctxt);
			Hal4Ctxt->psADDCtxtInfo->IsPollConfigured = TRUE;
			phHal4Nfc_Convert2NciRfDiscovery(Hal4Ctxt, &rf_enable);
			nci_start_operation(Hal4Ctxt->nciHandle, NCI_OPERATION_RF_ENABLE, &rf_enable, NULL, 0, NULL, Hal4Ctxt);
			break;
		default:
			/*Currently do nothing... */
			break;
	}
}


void nci_core_interface_error_ntf_handler(nfc_handle_t h_nal, nfc_u16 opcode, nci_ntf_param_u *pu_ntf)
{
	phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt = (phHal4Nfc_Hal4Ctxt_t*)h_nal;

	hal4nfc_report((" nci_core_interface_error_ntf: status=0x%x connID=0x%x",
		pu_ntf->core_iface_error.status, pu_ntf->core_iface_error.connID));

	/* Perform RF Deactivate - upper layers will restart discovery, as needed */
	nci_start_operation(Hal4Ctxt->nciHandle, NCI_OPERATION_RF_DISABLE, NULL, NULL, 0, NULL, Hal4Ctxt);
}
