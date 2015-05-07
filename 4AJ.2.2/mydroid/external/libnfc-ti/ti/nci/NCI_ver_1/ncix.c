/*
 * nci_ver_1\ncix.c
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

  Implementation of NCIX infrastructure module

*******************************************************************************/
#include "nci_api.h"

#if (NCI_VERSION_IN_USE==NCI_VER_1)

#include "nfc_types.h"
#include "nci_dev.h"
#include "nfc_os_adapt.h"
#include "nci_int.h"
#include "ncix_int.h"



struct ncix_op_entry
{
	operation_cb_f		f_cb;
	nfc_handle_t			h_cb;
};

struct ncix_context
{
	nfc_handle_t				h_nci;
	nfc_handle_t				h_osa;
	struct nci_op			curr_op;
	op_rsp_param_u			curr_rsp;

	nfc_handle_t  				h_nci_main;
	nfc_handle_t				h_rf_mgmt;
	nfc_handle_t				h_nfcee;

	struct ncix_op_entry	op_table[NCI_OPERATION_MAX];
};


/*******************************************************************************
	EXtended Commands Handler
*******************************************************************************/

/******************************************************************************
*
* Name: ncix_operation_cb
*
* Description: Operation callback is executed when item is dequeued from operation queue.
*			  Before f_cb callback (hndlr_op_xxx()) is operated the queue is disabled to
*			  prevent performing other operations in parallel.
*
* Input: 	h_ncix - handle to Operation object
*		p_param - parameter to callback function
*
* Output: None
*
* Return: None
*
*******************************************************************************/
void ncix_operation_cb(nfc_handle_t h_ncix, nfc_handle_t p_param)
{
	struct ncix_context *p_ncix = (struct ncix_context *)h_ncix;
	struct ncix_op_entry *p_op_entry;
	struct nci_op *p_op = (struct nci_op *)p_param;

	p_ncix->curr_op = *p_op;

	/* clear temporary response buffer */
	osa_mem_zero(&p_ncix->curr_rsp, sizeof(p_ncix->curr_rsp));

	osa_taskq_disable(p_ncix->h_osa, E_NCI_Q_OPERATION);

	p_op_entry = &p_ncix->op_table[p_op->opcode];

	OSA_ASSERT(p_op_entry->f_cb);
	p_op_entry->f_cb(p_op_entry->h_cb, &p_op->u_param);

	osa_mem_free(p_op);
}

/******************************************************************************
*
* Name: ncix_register_op_cb
*
* Description: Registration function for operation specific callback.
*
* Input: 	h_ncix - handle to Operation object
*		opcode - operation opcode
*		f_cb -Callback (hndlr_op_xxx()) installed by each of Operation module upon module creation.
*			   It will be invoked by operation CB (in NCI single context) in
*			   result of start_operation request.
*		h_cb -handle of the specific operation module that installs the callback
*
* Output: None
*
* Return: None
*
*******************************************************************************/
void ncix_register_op_cb(nfc_handle_t h_ncix, nfc_u32 opcode, operation_cb_f f_cb, nfc_handle_t h_cb)
{
	struct ncix_context *p_ncix = (struct ncix_context *)h_ncix;

	p_ncix->op_table[opcode].f_cb = f_cb;
	p_ncix->op_table[opcode].h_cb = h_cb;

}

/******************************************************************************
*
* Name: ncix_get_rsp_ptr
*
* Description: Helper method to be called from a specific operation module when it needs
*			  to write/read from user response buffer.
*
* Input: 	h_ncix - handle to operation object
*
* Output: None
*
* Return: Response buffer
*
*******************************************************************************/
op_rsp_param_u *ncix_get_rsp_ptr(nfc_handle_t h_ncix)
{
	struct ncix_context *p_ncix = (struct ncix_context *)h_ncix;
	return &(p_ncix->curr_rsp);
}

/******************************************************************************
*
* Name: ncix_get_op_ptr
*
* Description: Helper method to be called from a specific operation module when it needs
			to access current operation.
*
* Input: 	h_ncix - handle to operation object
*
* Output: None
*
* Return: Response buffer
*
*******************************************************************************/
struct nci_op *ncix_get_op_ptr(nfc_handle_t h_ncix)
{
	struct ncix_context *p_ncix = (struct ncix_context *)h_ncix;
	return &(p_ncix->curr_op);
}

/******************************************************************************
*
* Name: ncix_op_complete
*
* Description: Helper method to be called from SM action handler upon
*			  operation completion. It will be used to enable OPERATION taskq
*			  and to respond to caller.
*
* Input: 	h_ncix - handle to operation object
*		status - Operation completion status - NCI_STATUS_OK \ NCI_STATUS_FAILED
*
* Output: None
*
* Return: None
*
*******************************************************************************/
void ncix_op_complete(nfc_handle_t h_ncix, nci_status_e status)
{
	struct ncix_context *p_ncix = (struct ncix_context *)h_ncix;

	p_ncix->curr_rsp.generic.status = (nfc_u8)status;
	osa_taskq_enable(p_ncix->h_osa, E_NCI_Q_OPERATION);

	/* copy temporary response to user response buffer */
	if(p_ncix->curr_op.pu_rsp)
	{
		osa_mem_copy(p_ncix->curr_op.pu_rsp, &(p_ncix->curr_rsp), p_ncix->curr_op.rsp_len);
	}
	/* call user response callback */
	if(p_ncix->curr_op.f_rsp_cb)
		p_ncix->curr_op.f_rsp_cb(p_ncix->curr_op.h_rsp_cb, p_ncix->curr_op.opcode, &p_ncix->curr_rsp);
}

/******************************************************************************
*
* Name: ncix_disable_operation_taskq
*
* Description: Helper method to be called from a specific operation module when it needs
*			  to block execution of operation queue.
*
* Input: 	h_ncix - handle to operation object
*
* Output: None
*
* Return: None
*
*******************************************************************************/
void ncix_disable_operation_taskq(nfc_handle_t h_ncix)
{
	struct ncix_context *p_ncix = (struct ncix_context *)h_ncix;
	osa_taskq_disable(p_ncix->h_osa, E_NCI_Q_OPERATION);

}

/******************************************************************************
*
* Name: ncix_create
*
* Description: Create operation object. Called at NCI module initialization.
*
* Input: 	h_ncix - handle to operation object
*		h_osa - handle to OSA object
*
* Output: None
*
* Return: handle to operation context object
*
*******************************************************************************/
nfc_handle_t ncix_create(nfc_handle_t h_nci, nfc_handle_t h_osa)
{
	struct ncix_context* p_ncix = osa_mem_alloc(sizeof(struct ncix_context));
	OSA_ASSERT(p_ncix);

	p_ncix->h_nci = h_nci;
	p_ncix->h_osa = h_osa;
	p_ncix->h_nci_main = ncix_main_create(p_ncix, p_ncix->h_nci);
	p_ncix->h_rf_mgmt = ncix_rf_mgmt_create(p_ncix, p_ncix->h_nci);
	p_ncix->h_nfcee = ncix_nfcee_create(p_ncix, p_ncix->h_nci);

	return p_ncix;
}

 /******************************************************************************
 *
 * Name: ncix_destroy
 *
 * Description: Destroy operation object. Called at NCI module destruction.
 *
 * Input:	 h_ncix - handle to operation object
 *
 * Output: None
 *
 * Return: None
 *
 *******************************************************************************/
void ncix_destroy(nfc_handle_t h_ncix)
{
	struct ncix_context *p_ncix = (struct ncix_context *)h_ncix;

	ncix_main_destroy(p_ncix->h_nci_main);
	ncix_rf_mgmt_destroy(p_ncix->h_rf_mgmt);
	ncix_nfcee_destroy(p_ncix->h_nfcee);

	osa_mem_free(h_ncix);
}

#endif //#if (NCI_VERSION_IN_USE==NCI_VER_1)

/* EOF */











