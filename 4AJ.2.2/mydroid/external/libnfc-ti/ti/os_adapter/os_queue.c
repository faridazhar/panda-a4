/*
 * os_queue.c
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

  Implementation of the Queue

*******************************************************************************/
#include "nfc_types.h"
#include "nfc_os_adapt.h"
#include "nci_utils.h"

/* Queue Structure */
struct queue
{
	struct	dll_node	head;				/* The queue first node */
	nfc_u32   			u_count;			/* Current number of nodes in queue */
	nfc_u32   			u_max_count;		/* Maximum u_count value (for debug) */
};

struct q_item
{
	struct	dll_node	head;				/* The queue first node */
	nfc_handle_t			h_item;
};

/*
 *              EXTERNAL  FUNCTIONS
 *        ===============================
 */

/**
 * \fn     que_Create
 * \brief  Create a queue.
 *
 * Allocate and init a queue object.
 *
 * \note
 * \return Handle to the allocated queue
 * \sa     que_Destroy
 */
nfc_handle_t que_create ()
{
	struct queue *p_que;


	/* allocate queue module */
	p_que = osa_mem_alloc(sizeof(struct queue));

	if (!p_que)
	{
		osa_report(ERROR, ("Error allocating the Queue Module\n"));
		return NULL;
	}

	osa_mem_zero(p_que, sizeof(struct queue));

	dll_initialize_head (&p_que->head);

	/* Set the Queue parameters */
	return (nfc_handle_t)p_que;
}


/**
 * \fn     que_Destroy
 * \brief  Destroy the queue.
 *
 * Free the queue memory.
 *
 * \note   The queue's owner should first free the queued items!
 * \param  h_que - The queue object
 * \return RES_COMPLETE on success or RES_ERROR on failure
 * \sa     que_Create
 */
nfc_status que_destroy (nfc_handle_t h_que)
{
	struct queue *p_que = (struct queue *)h_que;

	/* Alert if the queue is unloaded before it was cleared from items */
	if (p_que->u_count)
	{
		osa_report(ERROR, ("que_Destroy() Queue Not Empty!!"));
	}
	/* free Queue object */
	osa_mem_free(p_que);

	return NFC_RES_COMPLETE;
}


/**
 * \fn     que_Enqueue
 * \brief  Enqueue an item
 *
 * Enqueue an item at the queue's tail
 *
 * \note
 * \param  h_que   - The queue object
 * \param  h_item  - Handle to queued item
 * \return RES_COMPLETE if item was queued, or RES_ERROR if not queued due to overflow
 * \sa     que_Dequeue, que_Requeue
 */
nfc_status que_enqueue (nfc_handle_t h_que, nfc_handle_t h_item)
{
	struct queue	*p_que = (struct queue *)h_que;
	struct q_item	*q_item = 	(struct q_item *)osa_mem_alloc(sizeof(struct q_item));

	OSA_ASSERT(q_item);

	q_item->h_item = h_item;

	/* Enqueue item to the queue tail and increment items counter */
	dll_insert_tail (&p_que->head, &q_item->head);
	p_que->u_count++;
	return NFC_RES_COMPLETE;

}


/**
 * \fn     que_Dequeue
 * \brief  Dequeue an item
 *
 * Dequeue an item from the queue's head
 *
 * \note
 * \param  h_que - The queue object
 * \return pointer to dequeued item or NULL if queue is empty
 * \sa     que_Enqueue, que_Requeue
 */
nfc_handle_t que_dequeue (nfc_handle_t h_que)
{
	struct queue   	*p_que = (struct queue *)h_que;
	nfc_handle_t 		h_item;
	struct  dll_node 	*p_node;

	if (p_que->u_count)
	{
		/* Queue is not empty, take packet from the queue head and
		 * find pointer to the node entry
		 */
		p_node = dll_remove_head (&p_que->head);
		h_item = ((struct q_item *)p_node)->h_item;
		osa_mem_free (p_node);
		p_que->u_count--;
		return (h_item);
	}

	/* Queue is empty */
	return NULL;
}




/**
 * \fn     que_Size
 * \brief  Return queue size
 *
 * Return number of items in queue.
 *
 * \note
 * \param  h_que - The queue object
 * \return nfc_u32 - the items count
 * \sa
 */
nfc_u32 que_size (nfc_handle_t h_que)
{
	struct queue *p_que = (struct queue *)h_que;

	return (p_que->u_count);
}






