/*
 * hci.h
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

 /******************************************************************************
  This header files contains the definitions for the HCI Network.
*******************************************************************************/

#ifndef __HCI_H
#define __HCI_H

#include "nfc_types.h"


/******************************************************************************/
/* HCI Structures and Types                                                   */
/******************************************************************************/

struct hci_callbacks {
	void (*activated)(nfc_handle_t h_caller, nfc_status status);
	void (*deactivated)(nfc_handle_t h_caller, nfc_status status);
	void (*evt_connectivity)(nfc_handle_t h_caller);
	void (*evt_transaction)(nfc_handle_t h_caller, nfc_u8 *aid, nfc_u8 aid_len, nfc_u8 *param, nfc_u8 param_len);
	void (*gto_apdu_evt_transmit_data)(nfc_handle_t h_caller, nfc_u8 *data, nfc_u32 data_len);
	void (*gto_apdu_evt_wtx_request)(nfc_handle_t h_caller);
};


/******************************************************************************/
/* HCI Function Declarations                                                  */
/******************************************************************************/

nfc_handle_t hci_create(nfc_handle_t h_nci, nfc_handle_t h_osa, struct hci_callbacks *callbacks, nfc_handle_t h_caller);
void hci_destroy(nfc_handle_t h_hci);

nfc_status hci_activate(nfc_handle_t h_hci, nfc_u8 nfcee_id);
nfc_status hci_deactivate(nfc_handle_t h_hci, nfc_u8 nfcee_id);

nfc_status hci_gto_apdu_transmit_data(nfc_handle_t h_hci, nfc_u8 *data, nfc_u32 data_len);


#endif /* __HCI_H */
