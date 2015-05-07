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


/**
* \file phHal4Nfc_Internal.h
* \brief HAL callback Function Prototypes
*
*  The HAL4.0 Internal header file
*
* Project: NFC-FRI-1.1 / HAL4.0
*
* $Date: Mon May 31 11:43:42 2010 $
* $Author: ing07385 $
* $Revision: 1.40 $
* $Aliases: NFC_FRI1.1_WK1023_R35_1 $
*
*/

/*@{*/
#ifndef PHHAL4NFC_INTERNAL_H
#define PHHAL4NFC_INTERNAL_H
/*@}*/

#include <phHal4Nfc.h>
#include <phDal4Nfc.h>
#include <phOsalNfc.h>
#include <phOsalNfc_Timer.h>
#include <phDal4Nfc_messageQueueLib.h>

#include <nfc_os_adapt.h>
#include <nfc_trans_adapt.h>
#include <nci_api.h>
#include "nfc_types.h"

#if (NCI_VERSION_IN_USE != NCI_DRAFT_9) && (NCI_VERSION_IN_USE != NCI_VER_1)
#error Illegal NCI_VERSION_IN_USE
#endif

#define hal4nfc_report(msg)                     osa_report(INFORMATION, msg)



/**
*  \name HAL4
*
* File: \ref phHal4Nfc_Internal.h
*
*/


/*---------------- Hal4 Internal Data Structures -------------------------*/
/**HAL4 states*/
typedef enum{
	eHal4StateClosed = 0x00,  /**<closed state*/
	eHal4StateSelfTestMode, /**<Self test mode*/
	eHal4StateOpenAndReady ,/**<Fully initialised*/
	eHal4StateConfiguring ,  /**<configuration ongoing,transient state*/
	eHal4StateTargetDiscovered,/**<target discovered*/
	eHal4StateTargetActivate,/**<state during a select or reactivate*/
	eHal4StateEmulation,/**<Emulation state*/
	eHal4StateTargetConnected,/**<Connected state*/
	eHal4StateTransaction,/**<configuration ongoing,transient state*/
	eHal4StatePresenceCheck,/**<Presence Check state*/
	eHal4StateInvalid
} phHal4Nfc_Hal4state_t;


/**Global Pointer to hardware reference used in timer callbacks to get the
   context pointer*/
extern phHal_sHwReference_t *gpphHal4Nfc_Hwref;


extern int nDeferedCallMessageQueueId;



/**Context info for HAL4 transceive*/
typedef struct phHal4Nfc_TrcvCtxtInfo{
	/*Upper layer's Transceive callback*/
	pphHal4Nfc_TransceiveCallback_t  pUpperTranceiveCb;
	 /*Upper layer's Send callback*/
	pphHal4Nfc_SendCallback_t        pP2PSendCb;
	 /*Upper layer's receive callback*/
	pphHal4Nfc_ReceiveCallback_t     pP2PRecvCb;
	/**Flag to check if a P2P Send is ongoing when target release is issued by
	   the upper layer.If this flag is set ,then a remote device disconnect call
	   will be deferred*/
	uint8_t                          P2P_Send_In_Progress;
	/*Data structure to provide transceive info to Hci*/
//    phHciNfc_XchgInfo_t              XchangeInfo;
	/*sData pointer to point to upper layer's send data*/
	phNfc_sData_t                   *psUpperSendData;
	/*Maintains the offset of number of bytes sent in one go ,so that the
	  remaining bytes can be sent during the next transceive*/
	uint32_t                         NumberOfBytesSent;
	/*Number of bytes received during a P2p receive*/
	uint32_t                         P2PRecvLength;
	/*sData pointer to point to upper layer's recv data*/
	phNfc_sData_t                   *psUpperRecvData;
	/*structure to hold data received from lower layer*/
	phNfc_sData_t                    sLowerRecvData;
	/*Offset for Lower Recv Data buffer*/
	uint32_t                         LowerRecvBufferOffset;
	/*Holds the status of the RecvDataBuffer:
	NFCSTATUS_SUCCESS:Receive data buffer is complete with data & P2P receive has
					  not yet been called
	NFCSTATUS_PENDING:RecvDataBuffer is yet to receive the data from lower layer
	*/
	NFCSTATUS                        RecvDataBufferStatus;
	/*Transaction timer ,currently used only for P2P receive on target*/
	uint32_t                         TransactionTimerId;
}phHal4Nfc_TrcvCtxtInfo_t,*pphHal4Nfc_TrcvCtxtInfo_t;


/**Context info for HAL4 Device discovery feature*/
typedef struct phHal4Nfc_ADDCtxtInfo{
	/*total number of devices discovered*/
	uint8_t                          nbr_of_devices;
	/*smx_discovery*/
	uint8_t                          smx_discovery;
	/*Most recently used ADD configuration*/
	phHal_sADD_Cfg_t                 sADDCfg;
	/*Most recently used Poll configuration*/
	phHal_sPollDevInfo_t             sCurrentPollConfig;
	/*Set when Poll Configured and reset when polling is disabled.*/
	uint8_t                          IsPollConfigured;
}phHal4Nfc_ADDCtxtInfo_t,*pphHal4Nfc_ADDCtxtInfo_t;

/**Context info for HAL4 connect/disconnect*/
typedef struct phHal4Nfc_TargetConnectInfo{
	/*connect callback*/
	pphHal4Nfc_ConnectCallback_t     pUpperConnectCb;
	/*Disconnect callback*/
	pphHal4Nfc_DiscntCallback_t      pUpperDisconnectCb;
	/*used when a release call is pending in HAL*/
	phHal_eReleaseType_t             ReleaseType;
	/*Points to Remote device info of a connected device*/
	phHal_sRemoteDevInformation_t   *psConnectedDevice;
	/*Emulation state Activated/Deactivated*/
	phHal_Event_t                    EmulationState;
	/*Presence check callback*/
	pphHal4Nfc_GenCallback_t         pPresenceChkCb;
}phHal4Nfc_TargetConnectInfo_t,*pphHal4Nfc_TargetConnectInfo_t;

/**Context info for HAL4 connect & disconnect*/
typedef struct phHal4Nfc_UpperLayerInfo{
	/*Upper layer Context for discovery call*/
	void                            *DiscoveryCtxt;
	/*Upper layer Context for P2P discovery call*/
	void                            *P2PDiscoveryCtxt;
	/**Context and function pointer for default event handler registered
	  by upper layer during initialization*/
	void                            *DefaultListenerCtxt;
	/*Default event handler*/
	pphHal4Nfc_Notification_t        pDefaultEventHandler;
	/**Upper layer has to register this listener for receiving info about
		discovered tags*/
	pphHal4Nfc_Notification_t        pTagDiscoveryNotification;
	/**Upper layer has to register this  listener for receiving info about
		discovered P2P devices*/
	pphHal4Nfc_Notification_t        pP2PNotification;
	/*Event Notification Context*/
	void                            *EventNotificationCtxt;
	/**Notification handler for emulation and other events*/
	pphHal4Nfc_Notification_t        pEventNotification;
	/**Upper layer's Config discovery/Emulation callback registry*/
	pphHal4Nfc_GenCallback_t         pConfigCallback;
	void                            *psUpperLayerCtxt;
	void                            *psUpperLayerDisconnectCtxt;
#ifdef LLCP_DISCON_CHANGES
	void                            *psUpperLayerCfgDiscCtxt;
#endif /* #ifdef LLCP_DISCON_CHANGES */
	 /**Upper layer's Open Callback registry*/
	pphHal4Nfc_GenCallback_t         pUpperOpenCb;
	/**Upper layer's Close Callback registry */
	pphHal4Nfc_GenCallback_t         pUpperCloseCb;
	/*Ioctl out param pointer ,points to buffer provided by upper layer during
	  a ioctl call*/
	phNfc_sData_t                   *pIoctlOutParam;
	/*Ioctl callback*/
	pphHal4Nfc_IoctlCallback_t       pUpperIoctlCb;
}phHal4Nfc_UpperLayerInfo_t;

/**Context structure for HAL4.0*/
typedef struct phHal4Nfc_Hal4Ctxt{

	nfc_handle_t                    nciHandle;
	nfc_handle_t                    hciHandle;
	nfc_u8                     active_conn_id;

	/* phHal4Nfc_Open */
	struct  nci_op_rsp_start    nci_start_rsp;

	/**Hci handle obtained in Hci_Init*/
//    void                            *psHciHandle;
	/**Layer configuration*/
//    pphNfcLayer_sCfg_t               pHal4Nfc_LayerCfg;
	/**Device capabilities*/
//    phHal_sDeviceCapabilities_t      Hal4Nfc_DevCaps;
	/*Current state of HAL4.Updated generally in callbacks*/
	phHal4Nfc_Hal4state_t            Hal4CurrentState;
	/*Next state of HAL.Updated during calls*/
	phHal4Nfc_Hal4state_t            Hal4NextState;
	/**Info related to upper layer*/
	phHal4Nfc_UpperLayerInfo_t       sUpperLayerInfo;
	 /*ADD context info*/
	pphHal4Nfc_ADDCtxtInfo_t         psADDCtxtInfo;
	/*union for different configurations ,used in a config_parameters()call*/
//    phHal_uConfig_t                  uConfig;
	 /*Event info*/
//    phHal_sEventInfo_t              *psEventInfo;
	/*Select sector flag*/
//    uint8_t                          SelectSectorFlag;
	/**List of pointers to remote device information for all discovered
	   targets*/
	phHal_sRemoteDevInformation_t   *rem_dev_list[MAX_REMOTE_DEVICES];
	/*Transceive context info*/
	pphHal4Nfc_TrcvCtxtInfo_t        psTrcvCtxtInfo;
	/*Connect context info*/
	phHal4Nfc_TargetConnectInfo_t    sTgtConnectInfo;
	/*Last called Ioctl_type*/
//    uint32_t                         Ioctl_Type;
#ifdef IGNORE_EVT_PROTECTED
	/*used to ignore multiple Protected events*/
	uint8_t                          Ignore_Event_Protected;
#endif/*#ifdef IGNORE_EVT_PROTECTED*/

	uint32_t                         PresChkTimerId;

#define SE_NOT_CONNECTED ((nfc_s32) -1)

	/* Contains the UICC nfcee_id or SE_NOT_CONNECTED */
	nfc_s32							 uicc_id;
	nfc_bool						 is_hci;	/* true if uicc_id is HCI_ACCESS*/
	nfc_bool						 is_hci_active;

	/* Contains the SMX nfcee_id or SE_NOT_CONNECTED */
	nfc_s32							 smx_id;

#if (NCI_VERSION_IN_USE==NCI_VER_1)
	/* Felica/Type3 activation - sending SENSF_REQ command*/
	nfc_u8						nci_send_sensf_req_command;

	/* Topaz/Jewel activation - sending RID command */
	nfc_u8						nci_send_rid_command;

	/* The target that was eventually selected in RF discovery 
	    selection process in multiple targets situation. */
	nfc_bool rf_discover_target_selected;
	nfc_u8 rf_discover_selected_discovery_id;
	nfc_u8 rf_discover_selected_protocol;
	nfc_u8 rf_discover_selected_interface;

	/* Script File Name */
#ifdef ANDROID
#define NFC_INIT_SCRIPT_FILE_FMT "/system/etc/firmware/TINfcInit_%d.%d.%d.%d.bts"
#else
#define NFC_INIT_SCRIPT_FILE_FMT "TINfcInit_%d.%d.%d.%d.bts"
#endif

	// TINfcInit_%d.%d.%d.%d.bts where each number is u8 so max is:
	// TINfcInit_255.255.255.255.bts
#define NFC_INIT_SCRIPT_FILE_MAX_LEN (sizeof(NFC_INIT_SCRIPT_FILE_FMT)+4)

	nfc_s8							 scriptFile[NFC_INIT_SCRIPT_FILE_MAX_LEN];
#endif

}phHal4Nfc_Hal4Ctxt_t;


/*---------------- Function Prototypes ----------------------------------------------*/

/*Callback completion routine for Disconnect*/
extern void phHal4Nfc_DisconnectComplete(
							phHal4Nfc_Hal4Ctxt_t  *Hal4Ctxt,
							void *pInfo
							);

/*Callback completion routine for Transceive*/
extern void phHal4Nfc_TransceiveComplete(
						phHal4Nfc_Hal4Ctxt_t  *Hal4Ctxt,
						void *pInfo
						);

/*Callback completion routine for Presence check*/
extern void phHal4Nfc_PresenceChkComplete(
						phHal4Nfc_Hal4Ctxt_t  *Hal4Ctxt,
						void *pInfo
						);

/*Callback completion routine for ADD*/
extern void phHal4Nfc_TargetDiscoveryComplete(
							phHal4Nfc_Hal4Ctxt_t  *Hal4Ctxt,
							void *pInfo
							);

/*Callback completion routine for NFCIP1 Receive*/
extern void phHal4Nfc_RecvCompleteHandler(phHal4Nfc_Hal4Ctxt_t  *Hal4Ctxt,void *pInfo);

/*Callback completion routine for Send*/
extern void phHal4Nfc_SendCompleteHandler(phHal4Nfc_Hal4Ctxt_t  *Hal4Ctxt,void *pInfo);

/*Callback completion routine for P2P Activate Event received from HCI*/
extern void phHal4Nfc_P2PActivateComplete(
					phHal4Nfc_Hal4Ctxt_t  *Hal4Ctxt,
					void *pInfo
					);
/*Callback completion routine for P2P Deactivate Event received from HCI*/
extern void phHal4Nfc_HandleP2PDeActivate(
						phHal4Nfc_Hal4Ctxt_t  *Hal4Ctxt,
						void *pInfo
						);

/**Execute Hal4 Disconnect*/
extern NFCSTATUS phHal4Nfc_Disconnect_Execute(
							phHal_sHwReference_t  *psHwReference
							);

/**Handle transceive timeout*/
#define TRANSACTION_TIMER
#define PH_HAL4NFC_TRANSCEIVE_TIMEOUT (2000)

#ifdef TRANSACTION_TIMER
extern void phHal4Nfc_TrcvTimeoutHandler(uint32_t TrcvTimerId, void* context);
#endif /*TRANSACTION_TIMER*/


typedef struct Hal4Nfc_BasicContext
{
	nfc_handle_t                    osHandle;
	nfc_handle_t                    transHandle;
} Hal4Nfc_BasicContext_t;


typedef struct phHal4Nfc_deferMsg_t
{
	phHal4Nfc_Hal4Ctxt_t        *Hal4Ctxt;
	void                        *defer_msg;
} phHal4Nfc_deferMsg;

extern void nci_activate_ntf_handler(nfc_handle_t h_nal, nfc_u16 opcode, nci_ntf_param_u *pu_ntf);
extern void nci_rf_discover_ntf_handler(nfc_handle_t h_nal, nfc_u16 opcode, nci_ntf_param_u *pu_ntf);
extern void nci_rf_nfcee_action_ntf_handler(nfc_handle_t h_nal, nfc_u16 opcode, nci_ntf_param_u *pu_ntf);
extern nfc_u8 return_rf_interface (nfc_handle_t h_nal, nfc_u8 rf_protocol);
extern nfc_bool is_rf_interface_supported(nfc_handle_t h_nal, nfc_u8 rf_interface);
extern void nci_deactivate_ntf_handler(nfc_handle_t h_nal, nfc_u16 opcode, nci_ntf_param_u *pu_ntf);
extern void nci_core_generic_error_ntf_handler(nfc_handle_t h_nal, nfc_u16 opcode, nci_ntf_param_u *pu_ntf);
extern void nci_core_interface_error_ntf_handler(nfc_handle_t h_nal, nfc_u16 opcode, nci_ntf_param_u *pu_ntf);
extern void phHal4Nfc_Convert2NciRfDiscovery(phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt, op_param_u *op);
extern void phHal4Nfc_HandleEmulationEvent(phHal4Nfc_Hal4Ctxt_t  *Hal4Ctxt, void *pInfo);
extern NFCSTATUS phHal4Nfc_NFCEE_Switch_Mode(phHal4Nfc_Hal4Ctxt_t *Hal4Ctxt, enum nfcee_switch_mode mode, int isSmx);

#if (NCI_VERSION_IN_USE==NCI_VER_1)

/* Handle Jewel (type 1) RID response and activate */
extern void pHal4Nfc_Activate_Jewel(nfc_handle_t h_nal, nfc_u8 *p_rid_rsp, nfc_u32 rid_rsp_len, nci_status_e nci_status);

/* Handle Felica (type 3) SENSF response and activate */
extern void pHal4Nfc_Activate_Felica(nfc_handle_t h_nal, nfc_u8 *p_sensf_rsp, nfc_u32 sensf_rsp_len, nci_status_e nci_status);

#endif


#endif/*PHHAL4NFC_INTERNAL_H*/
