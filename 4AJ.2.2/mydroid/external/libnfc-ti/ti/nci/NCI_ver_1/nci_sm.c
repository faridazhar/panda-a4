/*
 * nci_ver_1\nci_sm.c
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

  This source file contains the NCI Notification Parser Utility

*******************************************************************************/
/*******************************************************************************

   Implementation of State Machine infrastructure module

*******************************************************************************/
#include "nci_api.h"

#if (NCI_VERSION_IN_USE==NCI_VER_1)

#include "nfc_types.h"
#include "nfc_os_adapt.h"
#include "nci_sm.h"


extern nfc_u32 gIsNCIActive;

struct nci_sm
{
	nfc_handle_t			h_cb;
	nfc_s8					*p_name;
	nfc_u32					curr_state;
	struct nci_sm_state		*p_state;
	struct nci_sm_event		*p_event;
	event_complete_cb_f		p_event_complete_cb;
	nfc_handle_t			h_evt_complete_param;
};

/******************************************************************************
*
* Name: nci_sm_process_evt
*
* Description: Process State machine coming event upon current state.
*			  It is possible that state machine shall react per action callback
*			  and state callback (if any of them is registered).
*
* Input: 	h_sm - handle to state machine
*		event -event to be handled by state machine
*
* Output: None
*
* Return: None
*
*******************************************************************************/
void nci_sm_process_evt(nfc_handle_t h_sm, nfc_u32 event)
{
	struct nci_sm *p_sm = (struct nci_sm *)h_sm;
	nfc_u32 curr_state = p_sm->curr_state;
	nfc_u32 next_state = p_sm->curr_state;


	/* Note that invocation of both event and state action hanlder is possible!!!
		User (module that registers to SM) shall be responsible to handle certain event
		and state combination appropriately */
	if(p_sm->p_state[curr_state].f_cb)
		next_state = p_sm->p_state[curr_state].f_cb(p_sm->h_cb, event);
	else if(p_sm->p_event[event].f_cb)
		next_state = p_sm->p_event[event].f_cb(p_sm->h_cb, curr_state);

	if(NFC_TRUE == gIsNCIActive)
	{
		p_sm->curr_state = next_state;
		osa_report(INFORMATION, ("NCIX %s SM:: CurrState=%s, Event=%s, NextState=%s", p_sm->p_name,
			p_sm->p_state[curr_state].p_name, p_sm->p_event[event].p_name, p_sm->p_state[p_sm->curr_state].p_name));
	}
}

void nci_sm_process_evt_ex(nfc_handle_t h_sm, nfc_u32 event, event_complete_cb_f p_evt_complete_cb, nfc_handle_t h_evt_complete_param)
{
	struct nci_sm *p_sm = (struct nci_sm *)h_sm;

	if(NULL != p_sm->p_event_complete_cb || NULL != p_sm->h_evt_complete_param)
		OSA_ASSERT(NFC_FALSE && "nci_sm:nci_sm_process_evt_ex");

	p_sm->p_event_complete_cb = p_evt_complete_cb;
	p_sm->h_evt_complete_param = h_evt_complete_param;

	nci_sm_process_evt(h_sm, event);

}

void nci_sm_process_evt_ex_completed(nfc_handle_t h_sm, nfc_u32 status)
{
	struct nci_sm *p_sm = (struct nci_sm *)h_sm;

	if(NULL != p_sm->p_event_complete_cb)
		p_sm->p_event_complete_cb(p_sm->h_evt_complete_param, status);

	p_sm->p_event_complete_cb = NULL;
	p_sm->h_evt_complete_param = NULL;
}

nfc_u32 nci_sm_get_curr_state(nfc_handle_t h_sm)
{
	struct nci_sm *p_sm = (struct nci_sm *)h_sm;

	return p_sm->curr_state;
}

void nci_sm_set_curr_state(nfc_handle_t h_sm, nfc_u8 next_state)
{
	struct nci_sm *p_sm = (struct nci_sm *)h_sm;
	p_sm->curr_state = next_state;
}

/******************************************************************************
*
* Name: ncix_sm_create
*
* Description: Operation State machine module initialization.
*			  Allocate memory and initialize parameters.
*
* Input: 	h_cb - handle to SM object
*		p_sm_name - state machine name
*		initial_state - state machine's initial state
*		a_state - state machine's states table
*		a_event -state machine's events table
*
* Output: None
*
* Return: Handle to created state machine object.
*
*******************************************************************************/
nfc_handle_t nci_sm_create(nfc_handle_t h_cb, nfc_s8 *p_sm_name, nfc_u32 initial_state,
						struct nci_sm_state *a_state, struct nci_sm_event *a_event)
{
	struct nci_sm *p_sm = (struct nci_sm *)osa_mem_alloc(sizeof(struct nci_sm));
	OSA_ASSERT(p_sm);

	p_sm->h_cb = h_cb;
	p_sm->p_name = p_sm_name;
	p_sm->curr_state = initial_state;
	p_sm->p_state = a_state;
	p_sm->p_event = a_event;
	p_sm->p_event_complete_cb = NULL;
	p_sm->h_evt_complete_param = NULL;
	osa_report(INFORMATION, ("NCIX %s SM:: CurrState=%s (Created)", p_sm->p_name,
		p_sm->p_state[p_sm->curr_state].p_name));
	return p_sm;

}

/******************************************************************************
*
* Name: ncix_sm_destroy
*
* Description: Operation State machine module termination.
*			  Free allocated memory.
*
* Input: 	h_sm - handle to SM main object
*
* Output: None
*
* Return: None
*
*******************************************************************************/
void nci_sm_destroy(nfc_handle_t h_sm)
{
	struct nci_sm *p_sm = (struct nci_sm *)h_sm;
	osa_report(INFORMATION, ("NCIX %s SM:: (Destroyed)", p_sm->p_name));
	osa_mem_free(h_sm);
}

void nci_sm_reset(nfc_handle_t h_sm)
{
	struct nci_sm *p_sm = (struct nci_sm *)h_sm;

	p_sm->p_event_complete_cb = NULL;
	p_sm->h_evt_complete_param = NULL;

	osa_report(INFORMATION, ("NCIX %s SM:: (Resetted)", p_sm->p_name));
}


#endif //#if (NCI_VERSION_IN_USE==NCI_VER_1)

/* EOF */





















