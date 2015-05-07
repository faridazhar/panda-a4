/*
 * os_dl_list.c
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

  Implementation of the Queue and Double-linked-list

*******************************************************************************/

/** \file   queue.c
 *  \brief  This module provides generic queueing services, including enqueue, dequeue
 *            and requeue of any object that contains que_node_hdr in its structure.
 *
 *  \see    queue.h
 */

#include "nfc_os_adapt.h"
#include "nci_utils.h"


/*
	Initially, the head is initialized to point to itself in both directions => empty list
*/
void dll_initialize_head(struct dll_node *p_head)
{
	p_head->next = p_head;
	p_head->prev = p_head;
}

/*
	Initialize the node's next / prev pointers to NULL => Not on any list
*/
void dll_initialize_node(struct dll_node *p_node)
{
	p_node->next = 0;
	p_node->prev = 0;
}

void dll_insert_tail(struct dll_node *p_head, struct dll_node *p_node)
{
	/* set the new node's links */
	p_node->next = p_head;
	p_node->prev = p_head->prev;

	/* Make current last node one before last */
	p_head->prev->next = p_node;

	/* Head's prev should point to last element*/
	p_head->prev = p_node;
}

struct dll_node *dll_remove_head(struct dll_node *p_head)
{
	struct dll_node* p_first = NULL;

	p_first = p_head->next;
	p_first->next->prev = p_head;
	p_head->next = p_first->next;

	dll_initialize_node(p_first);

	return p_first;
}

void dll_remove_node(struct dll_node *p_node)
{
	p_node->prev->next = p_node->next;
	p_node->next->prev = p_node->prev;
	dll_initialize_node(p_node);
}

nfc_bool dll_is_node_in_list(struct dll_node *p_head, struct dll_node *p_node)
{
	struct dll_node* p_cur;

	p_cur = p_head->next;

	while (p_cur != p_head) {
		if (p_cur == p_node)
			return(NFC_TRUE);

		p_cur = p_cur->next;
	}

	return(NFC_FALSE);
}

