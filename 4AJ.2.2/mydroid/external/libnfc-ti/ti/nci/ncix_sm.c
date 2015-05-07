/*
 * ncix_sm.c
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

  This source file contains the NCI Notfication Parser Utility

*******************************************************************************/
/*******************************************************************************

   Implementation of State MAchine infrastructure module

*******************************************************************************/
#include "nci_api.h"

#if (NCI_VERSION_IN_USE==NCI_DRAFT_9)

#include "nfc_types.h"
#include "nfc_os_adapt.h"
#include "ncix_int.h"


extern nfc_u32 gIsNCIActive;

struct ncix_sm
{
	nfc_handle_t				h_cb;
	nfc_s8					*p_name;
	nfc_u32					curr_state;
	struct ncix_state		*p_state;
	struct ncix_event		*p_event;
};

/******************************************************************************
*
* Name: ncix_sm_process_evt
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
void ncix_sm_process_evt(nfc_handle_t h_sm, nfc_u32 event)
{
	struct ncix_sm *p_sm = (struct ncix_sm *)h_sm;
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
nfc_handle_t ncix_sm_create(nfc_handle_t h_cb, nfc_s8 *p_sm_name, nfc_u32 initial_state,
						struct ncix_state *a_state, struct ncix_event *a_event)
{
	struct ncix_sm *p_sm = (struct ncix_sm *)osa_mem_alloc(sizeof(struct ncix_sm));
	OSA_ASSERT(p_sm);

	p_sm->h_cb = h_cb;
	p_sm->p_name = p_sm_name;
	p_sm->curr_state = initial_state;
	p_sm->p_state = a_state;
	p_sm->p_event = a_event;
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
void ncix_sm_destroy(nfc_handle_t h_sm)
{
	struct ncix_sm *p_sm = (struct ncix_sm *)h_sm;
	osa_report(INFORMATION, ("NCIX %s SM:: (Destroyed)", p_sm->p_name));
	osa_mem_free(h_sm);
}

#endif //#if (NCI_VERSION_IN_USE==NCI_DRAFT_9)

/* EOF */





















