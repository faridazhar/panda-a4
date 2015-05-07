/*
 * nci_ver_1\nci_rf_sm.h
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

  This header files contains the definitions for the NFC Controller SM Interface.

*******************************************************************************/



#ifndef __NCI_RF_SM_H
#define __NCI_RF_SM_H

#include "nfc_types.h"
#include "nci_api.h"
#include "nci_sm.h"

#if (NCI_VERSION_IN_USE==NCI_VER_1)

typedef enum _rf_sm_events_e
{
	E_EVT_START,
	E_EVT_RF_DISCOVER_CMD,
	E_EVT_RF_DISCOVER_RSP_OK,
	E_EVT_RF_DISCOVER_RSP_FAIL,
	E_EVT_RF_DISCOVER_NTF,
	E_EVT_RF_INTF_ACTIVATED_NTF,
	E_EVT_RF_DEACTIVATE_CMD,
	E_EVT_RF_DEACTIVATE_RSP_OK,
	E_EVT_RF_DEACTIVATE_RSP_FAIL,
	E_EVT_RF_DEACTIVATE_NTF_IDLE,
	E_EVT_RF_DEACTIVATE_NTF_SLEEP,
	E_EVT_RF_DEACTIVATE_NTF_SLEEP_AF,
	E_EVT_RF_DEACTIVATE_NTF_DISCOVERY,
	E_EVT_CORE_SET_CONFIG_CMD,
	E_EVT_CORE_SET_CONFIG_RSP_OK,
	E_EVT_CORE_SET_CONFIG_RSP_FAIL,
	E_EVT_RF_NFCEE_ACTION_NTF,
	E_EVT_RF_NFCEE_DISCOVERY_REQ_NTF,
	E_EVT_CORE_INTF_ERROR_NTF,
	E_EVT_STOP,
	E_EVT_LISTEN_MODE_ROUTING_CONFIG,
	NCI_RF_SM_NUM_EVTS
}rf_sm_events_e;

//External APIs
nfc_handle_t nci_rf_sm_create(nfc_handle_t h_nci);
void nci_rf_sm_destroy(nfc_handle_t h_nci_sm);
void nci_rf_sm_configure_discovery(nfc_handle_t h_nci, nfc_u16 card_proto_mask, nfc_u16 reader_proto_mask);
nci_status_e nci_rf_sm_set_config_params(nfc_handle_t h_nci, struct nci_cmd *p_config_cmd);
void nci_rf_sm_start(nfc_handle_t h_nci);
void nci_rf_sm_stop(nfc_handle_t h_nci);
void nci_rf_sm_process_evt(nfc_handle_t h_nci, nfc_u32 event);
void nci_rf_sm_process_evt_ex(nfc_handle_t h_nci, nfc_u32 event, event_complete_cb_f p_evt_complete_cb, nfc_handle_t h_evt_complete_param);
nfc_u8 nci_rf_sm_is_cmd_allowed(struct nci_context *p_nci, nfc_u16 opcode);
nci_status_e nci_rf_sm_configure_listen_routing(nfc_handle_t h_nci_sm, configure_listen_routing_t* p_config);
void nci_rf_sm_set_nfcc_features(nfc_handle_t h_nci, nfc_u32 nfcc_features);
nfc_u32 nfcee_remove_discovery_request(nfc_handle_t h_nci, nfc_u8 nfcee_id);



nfc_u32 evt_hndlr_start(nfc_handle_t h_nci_sm, nfc_u32 curr_state);
nfc_u32 evt_hndlr_rf_discover_cmd(nfc_handle_t h_nci_sm, nfc_u32 curr_state);
nfc_u32 evt_hndlr_rf_discover_rsp_ok(nfc_handle_t h_nci_sm, nfc_u32 curr_state);
nfc_u32 evt_hndlr_rf_discover_rsp_fail(nfc_handle_t h_nci_sm, nfc_u32 curr_state);
nfc_u32 evt_hndlr_rf_discover_ntf(nfc_handle_t h_nci_sm, nfc_u32 curr_state);
nfc_u32 evt_hndlr_rf_discover_select_cmd(nfc_handle_t h_nci_sm, nfc_u32 curr_state);
nfc_u32 evt_hndlr_rf_discover_select_rsp_ok(nfc_handle_t h_nci_sm, nfc_u32 curr_state);
nfc_u32 evt_hndlr_rf_discover_select_rsp_fail(nfc_handle_t h_nci_sm, nfc_u32 curr_state);
nfc_u32 evt_hndlr_rf_intf_activated_ntf(nfc_handle_t h_nci_sm, nfc_u32 curr_state);
nfc_u32 evt_hndlr_rf_deactivate_cmd(nfc_handle_t h_nci_sm, nfc_u32 curr_state);
nfc_u32 evt_hndlr_rf_deactivate_rsp_ok(nfc_handle_t h_nci_sm, nfc_u32 curr_state);
nfc_u32 evt_hndlr_rf_deactivate_rsp_fail(nfc_handle_t h_nci_sm, nfc_u32 curr_state);
nfc_u32 evt_hndlr_rf_deactivate_ntf_idle(nfc_handle_t h_nci_sm, nfc_u32 curr_state);
nfc_u32 evt_hndlr_rf_deactivate_ntf_sleep(nfc_handle_t h_nci_sm, nfc_u32 curr_state);
nfc_u32 evt_hndlr_rf_deactivate_ntf_sleep_af(nfc_handle_t h_nci_sm, nfc_u32 curr_state);
nfc_u32 evt_hndlr_rf_deactivate_ntf_discovery(nfc_handle_t h_nci_sm, nfc_u32 curr_state);
nfc_u32 evt_hndlr_core_set_config_cmd(nfc_handle_t h_nci_sm, nfc_u32 curr_state);
nfc_u32 evt_hndlr_core_set_config_rsp_ok(nfc_handle_t h_nci_sm, nfc_u32 curr_state);
nfc_u32 evt_hndlr_core_set_config_rsp_fail(nfc_handle_t h_nci_sm, nfc_u32 curr_state);
nfc_u32 evt_hndlr_rf_nfcee_action_ntf(nfc_handle_t h_nci_sm, nfc_u32 curr_state);
nfc_u32 evt_hndlr_rf_nfcee_discovery_req_ntf(nfc_handle_t h_nci_sm, nfc_u32 curr_state);
nfc_u32 evt_hndlr_core_intf_error_ntf(nfc_handle_t h_nci_sm, nfc_u32 curr_state);
nfc_u32 evt_hndlr_stop(nfc_handle_t h_nci_sm, nfc_u32 curr_state);


#else

#error Incorrect NCI version, expected NCI_VER_1

#endif //#if (NCI_VERSION_IN_USE==NCI_VER_1)

#endif /* __NCI_RF_SM_H */























