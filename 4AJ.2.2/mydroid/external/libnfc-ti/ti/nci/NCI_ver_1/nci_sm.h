/*
 * nci_ver_1\nci_sm.h
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

  This header files contains the definitions for the NFC Controller Interface.

*******************************************************************************/



#ifndef __NCI_SM_H
#define __NCI_SM_H

#include "nfc_types.h"
#include "nci_api.h"

#if (NCI_VERSION_IN_USE==NCI_VER_1)

#define NCI_DBG

/*********************************************************************************/
/* NCI Private Structures                                                           */
/*********************************************************************************/

/* Callbacks for state machine in which the action handlers are per state */
typedef nfc_u32 (*state_cb_f)(nfc_handle_t h_cb, nfc_u32 event);
struct nci_sm_state
{
	state_cb_f				f_cb;
	nfc_s8					*p_name;
};

/* Callbacks for state machine in which the action handlers are per event */
typedef nfc_u32 (*event_cb_f)(nfc_handle_t h_cb, nfc_u32 curr_state);
struct nci_sm_event
{
	event_cb_f				f_cb;
	nfc_s8					*p_name;
};


/*** NCI SM Utilities ***/
nfc_handle_t nci_sm_create(nfc_handle_t h_cb, nfc_s8 *p_sm_name, nfc_u32 initial_state,
						struct nci_sm_state *a_state, struct nci_sm_event *a_event);
void nci_sm_destroy(nfc_handle_t h_sm);
void nci_sm_reset(nfc_handle_t h_sm);
/* SM Processing method will be called by module specific event (operation, notification or response handler)
	to trigger action per current state and event */
void nci_sm_process_evt(nfc_handle_t h_sm, nfc_u32 event);

typedef nfc_u32 (*event_complete_cb_f)(nfc_handle_t h_caller_cb, nfc_u32 status);

void nci_sm_process_evt_ex(nfc_handle_t h_sm, nfc_u32 event, event_complete_cb_f p_evt_complete_cb, nfc_handle_t h_evt_complete_param);
void nci_sm_process_evt_ex_completed(nfc_handle_t h_sm, nfc_u32 status);

nfc_u32 nci_sm_get_curr_state(nfc_handle_t h_sm);
void nci_sm_set_curr_state(nfc_handle_t h_sm, nfc_u8 next_state);


#else

#error Incorrect NCI version, expected NCI_VER_1

#endif //#if (NCI_VERSION_IN_USE==NCI_VER_1)

#endif /* __NCI_SM_H */























