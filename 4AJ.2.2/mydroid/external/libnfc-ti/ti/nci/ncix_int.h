/*
 * ncix_int.h
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

#ifndef __NCIX_INT_H
#define __NCIX_INT_H

#include "nfc_types.h"
#include "nci_api.h"

#if (NCI_VERSION_IN_USE==NCI_DRAFT_9)

#define NCI_DBG

/*********************************************************************************/
/* NCI Private Structures                                                           */
/*********************************************************************************/

struct ncix_context;

/* Callbacks for state machine in which the action handlers are per state */
typedef nfc_u32 (*state_cb_f)(nfc_handle_t h_cb, nfc_u32 event);
struct ncix_state
{
	state_cb_f				f_cb;
	nfc_s8					*p_name;
};

/* Callbacks for state machine in which the action handlers are per event */
typedef nfc_u32 (*event_cb_f)(nfc_handle_t h_cb, nfc_u32 curr_state);
struct ncix_event
{
	event_cb_f				f_cb;
	nfc_s8					*p_name;
};




/*********************************************************************************/
/* NCI Global Routines                                                           */
/*********************************************************************************/

/*** NCIX (Infrastructure) Utilities ***/

/* the following calllback will be installed by each of NCIX module upon module creation
   (see "constructor per module" below). It will be invoked by NCIX operation CB (in NCI
   single context) inre sult of start_operation request) */
typedef void (*operation_cb_f)(nfc_handle_t h_cb, op_param_u*);

/* registration funtion for operation specific callback (see "operation_cb_f" above), h_cb is handle
   of the specific NCIX module that installs the callback */
void ncix_register_op_cb(nfc_handle_t h_ncix, nfc_u32 opcode, operation_cb_f f_cb, nfc_handle_t h_cb);

/* NCIX helper method to be called from SM action handler upon operation completion.
	It will be used to enable OPERATION taskq and to respond to caller */
void ncix_op_complete(nfc_handle_t h_ncix, nci_status_e status);

/* NCIX helper method to be called from a specific NCIX module when it needs to write/read from
	user response buffer */
op_rsp_param_u *ncix_get_rsp_ptr(nfc_handle_t h_ncix);

/* NCIX helper method to be called from a specific NCIX module when it needs to block execution of operation queue */
void ncix_disable_operation_taskq(nfc_handle_t h_ncix);

/*** NCIX SM Utilities ***/
nfc_handle_t ncix_sm_create(nfc_handle_t h_cb, nfc_s8 *p_sm_name, nfc_u32 initial_state,
						struct ncix_state *a_state, struct ncix_event *a_event);
void ncix_sm_destroy(nfc_handle_t h_sm);
/* SM Processing method will be called by module specific event (operation, notification or response handler)
	to trigger action per current state and event */
void ncix_sm_process_evt(nfc_handle_t h_sm, nfc_u32 event);

/* Constructor per module */
nfc_handle_t ncix_main_create(nfc_handle_t h_ncix, nfc_handle_t h_nci);
nfc_handle_t ncix_nfcee_create(nfc_handle_t h_ncix, nfc_handle_t h_nci);
nfc_handle_t ncix_rf_mgmt_create(nfc_handle_t h_ncix, nfc_handle_t h_nci);

/* Destructor per module */
void ncix_main_destroy(nfc_handle_t h_main);
void ncix_nfcee_destroy(nfc_handle_t h_main);
void ncix_rf_mgmt_destroy(nfc_handle_t h_main);


#endif //#if (NCI_VERSION_IN_USE==NCI_DRAFT_9)

#endif /* __NCIX_INT_H */























