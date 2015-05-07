/*
 * nci_api.c
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

  Implementation of the NCI API methods

*******************************************************************************/
#include "nci_api.h"

#if (NCI_VERSION_IN_USE==NCI_DRAFT_9)

#include "nfc_types.h"
#include "nci_dev.h"
#include "nfc_os_adapt.h"
#include "nci_int.h"
#include "nci_data_int.h"


nfc_u32 gIsNCIActive = NFC_FALSE;

/*******************************************************************************
	Global Routines
*******************************************************************************/
/******************************************************************************
*
* Name: nci_set_cmd_timeout
*
* Description: Set command timeout
*
* Input: 	h_nci - handle to NCI object
*		cmd_timeout - Timeout in mili-seconds used to watch the command-response sequence
*
* Output: None
*
* Return: Success - NCI_STATUS_OK
*		  Failure - NCI_STATUS_MESSAGE_CORRUPTED
*
*******************************************************************************/
nci_status_e nci_set_cmd_timeout(nfc_handle_t h_nci, int cmd_timeout)
{
	if(h_nci)
	{
		struct nci_context *p_nci = (struct nci_context*)h_nci;
		osa_mem_set(p_nci, 0, sizeof(struct nci_context));
		p_nci->cmd_timeout = cmd_timeout;
		return NCI_STATUS_OK;
	}
	else
	{
		return NCI_STATUS_MESSAGE_CORRUPTED;
	}
}


/******************************************************************************
*
* Name: nci_open
*
* Description: Connect to transport driver and open shared transport driver
*
* Input: 	h_nci - handle to NCI object to open
*		dev_id - device id to be sent to transport driver
*
* Output: None
*
* Return: Success - NCI_STATUS_OK
*		  Failure - NCI_STATUS_MESSAGE_CORRUPTED \ NCI_STATUS_FAILED
*
*******************************************************************************/
nci_status_e nci_open(nfc_handle_t h_nci, nfc_u32 dev_id)
{
	nci_status_e status;
	nfc_status rc;


	if(h_nci)
	{
		struct nci_context *p_nci = (struct nci_context*)h_nci;

		p_nci->h_ncidev = nci_trans_get_ncidev(dev_id);
		if(p_nci->h_ncidev == NULL)
		{
			osa_report(ERROR, ("No registered NCI Device (%d)", dev_id));
			return NCI_STATUS_FAILED;
		}
		rc = nci_trans_open(p_nci);

		osa_taskq_register(p_nci->h_osa, E_NCI_Q_RX, nci_rx_cb, p_nci, NCI_DEQUEUE_ELEMENT_ENABLED);
		osa_taskq_register(p_nci->h_osa, E_NCI_Q_CMD, nci_cmd_cb, p_nci, NCI_DEQUEUE_ELEMENT_ENABLED);
		osa_taskq_register(p_nci->h_osa, E_NCI_Q_OPERATION, ncix_operation_cb, p_nci->h_ncix, NCI_DEQUEUE_ELEMENT_ENABLED);

		p_nci->h_cmd_watchdog = NULL;

		if(rc == NFC_RES_OK)
			status = NCI_STATUS_OK;
		else
			status = NCI_STATUS_FAILED;
	}
	else
	{
		status =  NCI_STATUS_MESSAGE_CORRUPTED;
	}


	osa_report(INFORMATION, ("NCI open dev.h id (%d) status %d", dev_id, status));

	return status;
}

/******************************************************************************
*
* Name: nci_close
*
* Description: Close Transport channel
*
* Input: 	h_nci - handle to NCI object to close
*		dev_id - device id to be sent to transport driver
*
* Output: None
*
* Return: Success - NCI_STATUS_OK
*		  Failure - NCI_STATUS_MESSAGE_CORRUPTED \ NCI_STATUS_FAILED
*
*******************************************************************************/
nci_status_e nci_close(nfc_handle_t h_nci, nfc_u32 dev_id)
{
	nci_status_e status;
	nfc_status rc;

	if(h_nci)
	{
		struct nci_context *p_nci = (struct nci_context*)h_nci;

		if(p_nci->h_cmd_watchdog != NULL)
		{
			osa_timer_stop(p_nci->h_cmd_watchdog); //Kill watchdog timer
			p_nci->h_cmd_watchdog = NULL;
		}

		if(p_nci->h_ncidev == NULL)
		{
			osa_report(ERROR, ("No registered NCI Device (%d)", dev_id));
			return NCI_STATUS_FAILED;
		}

		osa_taskq_unregister(p_nci->h_osa, E_NCI_Q_RX);
		osa_taskq_unregister(p_nci->h_osa, E_NCI_Q_CMD);
		osa_taskq_unregister(p_nci->h_osa, E_NCI_Q_OPERATION);

		rc = nci_trans_close(p_nci);

		if(rc == NFC_RES_OK)
			status = NCI_STATUS_OK;
		else
			status = NCI_STATUS_FAILED;
	}
	else
	{
		status =  NCI_STATUS_MESSAGE_CORRUPTED;
	}

	osa_report(INFORMATION, ("NCI close dev id (%d) status %d", dev_id, status));

	return status;
}

/******************************************************************************
*
* Name: nci_send_cmd
*
* Description: Prepare single NCI command. If succeeded enter it to proper queue for processing.
*
* Input: 	h_nci -handle to NCI object
*		opcode -Command identifier
*		pu_param -A union representing NCI command payload (length is known by the opcode).
*				     pu_param is being copied inside this callback and can be freed by caller upon return.
* 		pu_resp -Optional buffer for NCI response payload (NULL if response is ignored).
* 		rsp_len -Maximum length of expected response (Assert will be invoked if response exceeds this).
*		f_rsp_cb -Optional callback function to be invoked upon response reception (or NULL).
*		h_rsp_cb -Optional parameter that is passed to f_rsp_cb, if invoked (caller object handle).
*
* Output: None
*
* Return: Success - NCI_STATUS_OK
*		  Failure - NCI_STATUS_BUFFER_FULL \ NCI_STATUS_FAILED
*
*******************************************************************************/
nci_status_e nci_send_cmd(nfc_handle_t h_nci, nfc_u16 opcode,
									  nci_cmd_param_u *pu_cmd,
									  nci_rsp_param_u *pu_rsp, nfc_u8 rsp_len,
									  rsp_cb_f f_rsp_cb, nfc_handle_t h_rsp_cb)
{
	struct nci_cmd *p_nci_cmd = (struct nci_cmd *)osa_mem_alloc(sizeof(struct nci_cmd));
	struct nci_context *p_nci = (struct nci_context*)h_nci;
	nci_status_e status;

	if (p_nci_cmd != NULL)
	{
		status = nci_generate_cmd(opcode, pu_cmd, &p_nci_cmd->p_buff,h_nci);

		if (status == NCI_STATUS_OK)
		{
			p_nci_cmd->preamble = NCI_COMMAND_PREAMBLE;
			p_nci_cmd->f_rsp_cb = f_rsp_cb;
			p_nci_cmd->pu_rsp = pu_rsp;
			p_nci_cmd->opcode = opcode;
			p_nci_cmd->rsp_len = rsp_len;
			p_nci_cmd->h_rsp_cb = h_rsp_cb;
			if(pu_cmd)
				p_nci_cmd->u_cmd = *pu_cmd;

			osa_taskq_schedule(p_nci->h_osa, E_NCI_Q_CMD, p_nci_cmd);

		}
		else
		{
			osa_mem_free(p_nci_cmd);
		}
	}
	else
	{
		status = NCI_STATUS_BUFFER_FULL; // Amir - is this the correct error?
	}

	if (status != NCI_STATUS_OK) osa_report(ERROR, ("NCI send command error (status= %d)", status));

	return status;
}

/******************************************************************************
*
* Name: nci_send_data
*
* Description: Send Raw NCI data packet. Called by nal_send_data.
*
* Input: 	h_nci -handle to NCI object
*		conn_id - NCI connection id to send data
*		p_nci_payload - data packet (payload). Payload is being copied within this routine and can be freed by caller upon return.
*		nci_payload_len - payload length.
*
* Output: None
*
* Return: Success - NCI_STATUS_OK
*		  Failure - NCI_STATUS_REJECTED
*
*******************************************************************************/
nci_status_e nci_send_data(nfc_handle_t h_nci, nfc_u8 conn_id, nfc_u8 *p_user_payload, nfc_u32 user_payload_len)
{
	struct nci_context *p_nci = (struct nci_context*)h_nci;

	return nci_data_prepare_2_send(p_nci->h_data, conn_id, p_user_payload, user_payload_len);
}

/******************************************************************************
*
* Name: nci_start_operation
*
* Description: Start NCI operation (operation - a sequence of NCI commands and responses that can't be interrupted).
*
* Input: 	h_nci -handle to NCI object
*		opcode - operation identifier
*		pu_param - A union representing NCI operation parametrs (an input to the operation)
*		pu_resp - Optional buffer for NCI operation response (NULL if response is ignored).
*		rsp_len -Maximum length of expected response (Assert will be invoked if response exceeds this).
*		f_rsp_cb - Optional callback function to be invoked upon operation completion (or NULL).
*		h_rsp_cb - Optional parameter that is passed to f_rsp_cb, if invoked (caller object handle)
*
* Output: None
*
* Return: NCI_STATUS_OK
*
*******************************************************************************/
nci_status_e nci_start_operation(nfc_handle_t h_nci, nfc_u16 opcode,
								  op_param_u *pu_param,
								  op_rsp_param_u *pu_rsp, nfc_u8 rsp_len,
								  op_rsp_cb_f f_rsp_cb, nfc_handle_t h_rsp_cb)
{
	struct nci_op *p_op = (struct nci_op *)osa_mem_alloc(sizeof(struct nci_op));
	struct nci_context *p_nci = (struct nci_context*)h_nci;

	OSA_ASSERT(p_op);
	OSA_UNUSED_PARAMETER(h_nci);

	p_op->preamble = NCI_COMMAND_PREAMBLE;
	if(pu_param)
		p_op->u_param = *pu_param;
	p_op->f_rsp_cb = f_rsp_cb;
	p_op->h_rsp_cb = h_rsp_cb;
	p_op->pu_rsp = pu_rsp;
	p_op->opcode = opcode;
	p_op->rsp_len = rsp_len;
	osa_taskq_schedule(p_nci->h_osa, E_NCI_Q_OPERATION, p_op);

	return NCI_STATUS_OK;
}


/******************************************************************************
*
* Name: nci_register_ntf_cb
*
* Description: Register a callback to specific notification
*
* Input: 	h_nci -handle to NCI object
*		ntf_id - Notification id from nci_ntf_e enum
*		f_ntf_cb - callback function to be invoked upon notification reception
*		h_ntf_cb - Optional parameter that is passed to f_rsp_cb, if invoked (caller object handle)
*
* Output: None
*
* Return: None
*
*******************************************************************************/
void nci_register_ntf_cb(nfc_handle_t h_nci, nci_ntf_e ntf_id,
						 ntf_cb_f f_ntf_cb, nfc_handle_t h_ntf_cb)
{
	struct nci_context *p_nci = (struct nci_context*)h_nci;
	struct nci_ntf *p_ntf = (struct nci_ntf *)osa_mem_alloc(sizeof(struct nci_ntf));

	OSA_ASSERT(p_ntf);
	p_ntf->f_ntf_cb = f_ntf_cb;
	p_ntf->h_ntf_cb = h_ntf_cb;
	p_ntf->p_next = p_nci->p_ntf_tbl[ntf_id];
	p_nci->p_ntf_tbl[ntf_id] = p_ntf;
}

/******************************************************************************
*
* Name: nci_unregister_ntf_cb
*
* Description: Un-Register a callback to specific notification
*
* Input: 	h_nci -handle to NCI object
*		ntf_id - Notification id from nci_ntf_e enum
*		f_ntf_cb - callback function to be invoked upon notification reception
*
* Output: None
*
* Return: None
*
*******************************************************************************/
void nci_unregister_ntf_cb(nfc_handle_t h_nci, nci_ntf_e ntf_id,
						 ntf_cb_f f_ntf_cb)
{
	struct nci_context *p_nci = (struct nci_context*)h_nci;
	struct nci_ntf *p_prev = 0;
	struct nci_ntf *p_ntf = p_nci->p_ntf_tbl[ntf_id];

	while(p_ntf)
	{
		if(p_ntf->f_ntf_cb == f_ntf_cb)
		{
			if(p_prev)
			{
				p_prev->p_next = p_ntf->p_next;
			}
			else
			{
				p_nci->p_ntf_tbl[ntf_id] = p_ntf->p_next;
			}
			osa_mem_free(p_ntf);
			return;
		}

		p_prev = p_ntf;
		p_ntf = p_ntf->p_next;
	}
	OSA_ASSERT(0);
}

/******************************************************************************
*
* Name: nci_create
*
* Description: Create NCI module - this will involve only SW init (context allocation, callbacks registration)
 * and registration of callback for incoming data. Returns handle to created NCI object
*
* Input: 	h_caller - caller handle (to be used with data rx and tx callbacks)
* 		h_osa - os context
*		f_rx_ind_cb - rx indication callback (for received data packets)
*		f_tx_done_cb - to be called when packet transferring (to NFCC) is completed (when transport send function is done)
*
* Output: None
*
* Return: Success - handle to created NCI object
*		  Failure - NULL
*
*******************************************************************************/
nfc_handle_t nci_create(nfc_handle_t h_data_cb,
					nfc_handle_t h_osa,
					data_cb_f f_rx_ind_cb,
					data_cb_f f_tx_done_cb)
{
	struct nci_context *p_nci = (struct nci_context*)osa_mem_alloc(sizeof(struct nci_context));
	OSA_ASSERT(p_nci);
	osa_mem_zero(p_nci, sizeof(struct nci_context));

	if(p_nci)
	{
		p_nci->h_osa = h_osa;
		p_nci->cmd_timeout = NCI_COMMAND_TIMEOUT; /* default is 1 second watchdog */
		p_nci->h_ncidev = NULL;

		p_nci->h_ncix = ncix_create(p_nci, h_osa);
		p_nci->h_data = nci_data_create(p_nci, h_osa, f_rx_ind_cb, f_tx_done_cb, h_data_cb);
	}

	gIsNCIActive = NFC_TRUE;

	osa_report(INFORMATION, ("NCI create 0x%x", p_nci));

	return (nfc_handle_t)p_nci;
}

/******************************************************************************
*
* Name: nci_destroy
*
* Description: Destroy NCI module and free all its resources.
*
* Input: 	h_nci - handle to NCI object to destoy
*
* Output: None
*
* Return: None
*
*******************************************************************************/
void nci_destroy(nfc_handle_t h_nci)
{
	struct nci_context *p_nci = (struct nci_context*)h_nci;
	ncix_destroy(p_nci->h_ncix);
	nci_data_destroy(p_nci->h_data);
	osa_mem_free(p_nci);

	gIsNCIActive = NFC_FALSE;

	osa_report(INFORMATION, ("NCI destroy 0x%x", p_nci));
}

/* EOF */


#endif //#if (NCI_VERSION_IN_USE==NCI_DRAFT_9)








