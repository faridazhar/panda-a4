/*
 * nci_utils.h
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

  This header files contains the definitions of common utilities to be used by NCI and adpaters

*******************************************************************************/

#ifndef __NCI_UTILS_H
#define __NCI_UTILS_H

#include "nfc_types.h"

/* Double Linked List API Definition */
struct dll_node
{
	struct  dll_node *next;
	struct  dll_node *prev;
};

/*
	Initially, the head is initialized to point to itself in both directions => empty list
*/
void dll_initialize_head(struct dll_node *p_head);

/*
	Initialize the node's next / prev pointers to NULL => Not on any list
*/
void dll_initialize_node(struct dll_node *p_node);

nfc_bool dll_is_empty(struct dll_node *p_head);
#define dll_is_empty(p_head) ((p_head)->next == (p_head))

void dll_insert_tail(struct dll_node *p_head, struct dll_node *p_node);

struct dll_node *dll_remove_head(struct dll_node *p_head);

void dll_remove_node(struct dll_node *p_node);

nfc_bool dll_is_node_in_list(struct dll_node *p_head, struct dll_node *p_node);

/*
	Iterate over the list.
	Note: Each item MUST have a 'node' field as its first field.
	Note: The 'p_node' variable MUST not be removed during the iteration.

	Params:
	p_head - pointer to the head of the list.
	p_node - pointer to a variable containing the current item.
	type - type of struct of the 'p_node'.

	Example:
	Item *item;
	dll_iterate(&item_list, item, Item *) {
		// use 'item', don't remove it
	}
*/
#define dll_iterate(p_head, p_node, type) \
	for ((p_node) = (type)((p_head)->next); \
		(p_node) != (type)(p_head); \
		(p_node) = (type)((&((p_node)->node))->next))

/*
	Iterate safely over the list.
	Note: Each item MUST have a 'node' field as its FIRST field.
	Note: The 'p_node' variable MAY be removed during the iteration.

	Params:
	p_head - pointer to the head of the list.
	p_node - pointer to a variable containing the current item.
	p_next - pointer to a variable containing the next item.
	type - type of struct of the 'p_node' and 'p_next'.

	Example:
	Item *item, *item_next;
	dll_iterate_safe(&item_list, item, item_next, Item *) {
		// use 'item', may remove it
	}
*/
#define dll_iterate_safe(p_head, p_node, p_next, type) \
	for ((p_node) = (type)((p_head)->next); \
		(p_next) = (type)((&((p_node)->node))->next), \
		(p_node) != (type)(p_head); \
		(p_node) = (p_next))


/* Queue API Definition */
nfc_handle_t	que_create ();
nfc_status	que_destroy (nfc_handle_t h_que);
nfc_status	que_enqueue (nfc_handle_t h_que, nfc_handle_t h_item);
nfc_handle_t	que_dequeue (nfc_handle_t h_que);
nfc_u32		que_size (nfc_handle_t h_que);


#endif /* __NCI_UTILS_H */
