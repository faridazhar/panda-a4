/*
 * nci_ver_1\nci_api.c
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

#if (NCI_VERSION_IN_USE==NCI_VER_1)

#include <stdio.h>
#include "nfc_types.h"
#include "nci_dev.h"
#include "nfc_os_adapt.h"
#include "nci_int.h"
#include "nci_data_int.h"
#include "nci_rf_sm.h"


nfc_u32 gIsNCIActive = NFC_FALSE;

/* Definitions for nci_send_script routine */
#define ACTION_SEND_COMMAND             1    /* Send out raw data (as is) */
#define ACTION_WAIT_EVENT               2    /* Wait for data */
#define ACTION_SERIAL_PORT_PARAMETERS   3
#define ACTION_DELAY                    4
#define ACTION_RUN_SCRIPT               5
#define ACTION_REMARKS                  6

/* NFC HCI header information */
#define HCI_NFC_OPCODE_NCI_CMD	7

/* The value 0x42535442 stands for (in ASCII) BTSB
 * which is Bluetooth Script Binary */
#define FILE_HEADER_MAGIC    0x42535442

typedef struct tagBTSHeader_t
{
	nfc_u32 m_nMagicNumber;
	nfc_u32 m_nVersion;
	nfc_u8  m_nFuture[24];
} tagBTSHeader;

typedef struct nci_script_context
{
	nfc_handle_t h_nci;
	nfc_u8* pBTSData;
	nfc_u8* pCurrBTSData;
	nfc_s32 BTSDataSize;
	script_rsp_cb_f f_rsp_cb;
	nfc_handle_t	h_rsp_cb;
} nci_script_context;

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

		nci_data_open(p_nci, p_nci->h_data);

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

		nci_data_close(p_nci, p_nci->h_data);

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
		if(NFC_TRUE == nci_rf_sm_is_cmd_allowed(p_nci, opcode))
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
				p_nci_cmd->vendor_specific_cb = NULL;
				p_nci_cmd->f_raw_rsp_cb = NULL;
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
			status = NCI_STATUS_RM_SM_NOT_ALLOWED;
			osa_mem_free(p_nci_cmd);
		}
	}
	else
	{
		status = NCI_STATUS_MEM_FULL;
	}

	if (status != NCI_STATUS_OK)
		osa_report(ERROR, ("NCI send command error (status= 0x%x)", status));

	return status;
}


/***********************************************************************
 *
 * Internal function to send raw NCI command - used by nci_send_script
 *
 ***********************************************************************/
static nci_status_e nci_send_raw_cmd(
	nfc_handle_t h_nci,
	nfc_u8* p_payload,
	nfc_u8 payload_len,
	raw_rsp_cb_f raw_rsp_cb,
	nfc_handle_t raw_cb_param,
	nfc_bool queue)
{
	struct nci_cmd *p_nci_cmd = (struct nci_cmd *)osa_mem_alloc(sizeof(struct nci_cmd));
	struct nci_context *p_nci = (struct nci_context*)h_nci;
	nci_status_e status = NCI_STATUS_FAILED;

	if (p_nci_cmd != NULL)
	{
		struct nci_buff	*p_buff = NULL;
		nfc_u8		*p_data;

		p_buff = nci_buff_alloc(payload_len);

		if (p_buff != NULL)
		{
			p_data = nci_buff_data(p_buff);
			NCI_SET_ARR(p_data, p_payload, payload_len);

			status = NCI_STATUS_OK;
		}
		else
		{
			status = NCI_STATUS_MEM_FULL;
		}

		if (status == NCI_STATUS_OK)
		{
			p_nci_cmd->preamble = NCI_COMMAND_PREAMBLE;
			p_nci_cmd->opcode = 0;
			p_nci_cmd->f_rsp_cb = NULL;
			p_nci_cmd->pu_rsp = NULL;
			p_nci_cmd->rsp_len = NULL;
			p_nci_cmd->vendor_specific_cb = NULL;

			p_nci_cmd->p_buff = p_buff;
			p_nci_cmd->h_rsp_cb = raw_cb_param;
			p_nci_cmd->f_raw_rsp_cb = raw_rsp_cb;

			if (queue)
			{
				/* First time is called by caller thread so we need to queue our request */
				osa_taskq_schedule(p_nci->h_osa, E_NCI_Q_CMD, p_nci_cmd);
			}
			else
			{
				/* Consequent parts are performed directly in NCI task context while NCI_CMD queue remains locked
				   until full script is performed. This ensures the atomic behavior of the script */
				nci_cmd_cb(p_nci, p_nci_cmd);
			}

		}
		else
		{
			osa_mem_free(p_nci_cmd);
		}
	}
	else
	{
		status = NCI_STATUS_MEM_FULL;
	}

	if (status != NCI_STATUS_OK) osa_report(ERROR, ("nci_send_raw_cmd: command error (status= %d)", status));

	return status;
}

/* forward deceleration */
static nfc_status nci_script_continue_send(nci_script_context* p_script);

static void nci_script_raw_rsp_cb(nfc_handle_t usr_param, nfc_u8* p_rsp_buff, nfc_u32 rsp_buff_len)
{
	nci_script_context* p_script = (nci_script_context*) usr_param;
	nfc_status status = nci_script_continue_send(p_script);

	OSA_UNUSED_PARAMETER(p_rsp_buff);
	OSA_UNUSED_PARAMETER(rsp_buff_len);
	OSA_UNUSED_PARAMETER(status);
}

static nfc_status nci_script_continue_send(nci_script_context* p_script)
{
	nfc_u16 wType = 0;
	nfc_status status = NFC_RES_PENDING;
	nci_status_e nStatus;
	nfc_bool bCmdExecuted = NFC_FALSE;
	nfc_u16 actionLen = 0;
	nfc_u8* p_data;
	nfc_bool firstTime = NFC_FALSE;
	struct nci_context *p_nci = (struct nci_context*)p_script->h_nci;

	if (p_script->pCurrBTSData == p_script->pBTSData)
	{
		/* This is the first time that we are sending in this script */
		firstTime = NFC_TRUE;

		p_script->pCurrBTSData += sizeof(tagBTSHeader);
		p_script->BTSDataSize -= sizeof(tagBTSHeader);
	}

	do
	{
		if (p_script->BTSDataSize > 0)
		{
			/*BTS header: 4-byte header which specifies the type of the packet (2 bytes) and the length of its action (2 bytes).*/
			/*Bytes: 1 & 0 - action type*/
			/*Bytes: 3 & 2 - action length*/

			wType = *((nfc_u16*)(p_script->pCurrBTSData));
			actionLen = *((nfc_u16*)(&p_script->pCurrBTSData[2]));

			p_script->pCurrBTSData += 4;
			p_script->BTSDataSize -= 4;

			p_data = p_script->pCurrBTSData;

			/* Prepare context for next command */
			p_script->pCurrBTSData += actionLen;
			p_script->BTSDataSize -= actionLen;

			switch(wType)
			{
				/* We have an HCI command to execute here */
			case ACTION_SEND_COMMAND:
				// Verify this is an NCI command packed inside an HCI command
				if (p_data[0] == HCI_CHANNEL_NFC && p_data[1] == HCI_NFC_OPCODE_NCI_CMD)
				{
					//Now send the command:
					// This is a BT HCI command. Change to NCI command, remove 3 HCI header bytes
					p_data += 4;
					actionLen -= 4;

					osa_report(INFORMATION, ("nci_script_continue_send: Call nci_send_raw_cmd actionLen = %d", actionLen));
					osa_dump_buffer("nci_script_continue_send: ", (nfc_u8 *)p_data, actionLen);
					if (actionLen > 0xFF)
					{
						osa_report(ERROR, ("nci_script_continue_send: actionLen more than 255, aborting script", actionLen));
						status = NFC_RES_ERROR;
					}
					else
					{
						nStatus = nci_send_raw_cmd(
							p_script->h_nci,
							p_data, (nfc_u8) actionLen,
							nci_script_raw_rsp_cb, p_script,
							firstTime); /* On first time we queue the request. Consequent parts are performed directly */

						if (nStatus == NCI_STATUS_OK)
						{
							// break the loop
							bCmdExecuted = NFC_TRUE;
						}
						else
						{
							osa_report(ERROR, ("nci_script_continue_send: sending raw command failed, aborting script", actionLen));
							status = NFC_RES_ERROR;
						}
					}
				}
				break;

			case ACTION_SERIAL_PORT_PARAMETERS:
			case ACTION_WAIT_EVENT:
			case ACTION_DELAY:
			case ACTION_RUN_SCRIPT:
			case ACTION_REMARKS:
				break;

			default:
				break;
			}
		}
		else
		{
			OSA_ASSERT(0 == p_script->BTSDataSize);
			bCmdExecuted = NFC_TRUE;
			status = NFC_RES_COMPLETE;
		}

	} while ((bCmdExecuted == NFC_FALSE) && (status != NFC_RES_ERROR));

	switch(status)
	{
	case NFC_RES_PENDING:
		/* Script execution continues */
		if (!firstTime)
		{
			/* If this is not the first time then we are in NCI task context and we disable the CMD queue
			   to maintain the atomicity of the script */
			osa_taskq_disable(p_nci->h_osa, E_NCI_Q_CMD);
		}
		break;

	case NFC_RES_COMPLETE:
		/* Script execution finished */
		// Call user callback
		osa_report(INFORMATION, ("nci_script_continue_send: Script finished successfully"));
		if (p_script->f_rsp_cb)
		{
			p_script->f_rsp_cb(p_script->h_rsp_cb, NCI_STATUS_OK);
		}
		break;

	case NFC_RES_ERROR:
	default:
		/* Script execution failed in the middle */
		// Call user callback
		osa_report(ERROR, ("nci_script_continue_send: Script failed in the middle, status = 0x%X", (nfc_u32) status));
		if (p_script->f_rsp_cb)
		{
			p_script->f_rsp_cb(p_script->h_rsp_cb, NCI_STATUS_FAILED);
		}
		break;
	}

	if (status != NFC_RES_PENDING)
	{
		if (!firstTime)
		{
			/* We finished the script and if this is not the first time then we need to enable the CMD queue */
			osa_taskq_enable(p_nci->h_osa, E_NCI_Q_CMD);
		}

		// Free memory blocks
		osa_mem_free(p_script->pBTSData);
		osa_mem_free(p_script);
	}

	return status;
}

/******************************************************************************
*
* Name: nci_send_script
*
* Description: Enter script file name to proper queue for processing.
*
* Input: 	h_nci -handle to NCI object
*		pu_filename - script file name
*		f_rsp_cb -Optional callback function to be invoked upon completion of script (or NULL).
*		h_rsp_cb -Optional parameter that is passed to f_rsp_cb, if invoked (caller object handle).
*
* Output: None
*
*******************************************************************************/

void nci_send_script(nfc_handle_t h_nci,
	const nfc_s8* const pu_filename,
	script_rsp_cb_f f_rsp_cb, nfc_handle_t h_rsp_cb)
{
	nci_status_e status = NCI_STATUS_OK;
	nfc_status nStatus;
	nci_script_context* p_script = NULL;
	nfc_u32 filesize,nReadSize;
	FILE *fp = NULL;
	tagBTSHeader* pBTSHeader = NULL;


	if (pu_filename == NULL)
	{
		osa_report(ERROR, ("nci_send_script: Empty BTS filename!"));
		status = NCI_STATUS_INVALID_PARAM;
		goto Cleanup;
	}

	fp = fopen((char*)pu_filename, "rb");
	if (fp == NULL)
	{
		osa_report(ERROR, ("nci_send_script: Failed to open BTS file \"%s\"!", pu_filename));
		status = NCI_STATUS_INVALID_PARAM;
		goto Cleanup;
	}

	p_script = (nci_script_context*) osa_mem_alloc(sizeof(nci_script_context));
	if (p_script == NULL)
	{
		status = NCI_STATUS_MEM_FULL;
		goto Cleanup;
	}

	fseek(fp, 0, SEEK_END);
	filesize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	if (filesize == 0)
	{
		osa_report(ERROR, ("nci_send_script: BTS file \"%s\" is empty!", pu_filename));
		p_script->pBTSData = NULL;
		status = NCI_STATUS_INVALID_PARAM;
		goto Cleanup;
	}

	p_script->pBTSData = (nfc_u8*) osa_mem_alloc(filesize);
	if(p_script->pBTSData == NULL)
	{
		status = NCI_STATUS_MEM_FULL;
		goto Cleanup;
	}

	nReadSize = fread(p_script->pBTSData, 1, filesize, fp);
	OSA_ASSERT(filesize == nReadSize);

	p_script->BTSDataSize = nReadSize;

	pBTSHeader = (tagBTSHeader*)(p_script->pBTSData);
	if (pBTSHeader->m_nMagicNumber != FILE_HEADER_MAGIC)
	{
		osa_report(ERROR, ("nci_send_script: Invalid BTS file magic number 0x%x!", pBTSHeader->m_nMagicNumber));
		status = NCI_STATUS_INVALID_PARAM;
		goto Cleanup;
	}

	/* bts_header to remove out magic number and
		* version
		*/
	p_script->pCurrBTSData = p_script->pBTSData;
	p_script->f_rsp_cb = f_rsp_cb;
	p_script->h_rsp_cb = h_rsp_cb;
	p_script->h_nci = h_nci;

	nStatus = nci_script_continue_send(p_script);

	switch(nStatus)
	{
	case NFC_RES_PENDING:
		/* Script execution started */
		break;

	case NFC_RES_COMPLETE:
		/* Script has no NCI commands */
		osa_report(INFORMATION, ("nci_send_script: BTS file \"%s\" has no NCI commands!", pu_filename));
		break;

	case NFC_RES_ERROR:
	default:
		/* Script execution failed */
		break;
	}

Cleanup:
	if (fp) fclose(fp);

	if (status != NCI_STATUS_OK)
	{
		if (p_script)
		{
			if (p_script->pBTSData) osa_mem_free(p_script->pBTSData);
			osa_mem_free(p_script);
		}

		osa_report(ERROR, ("NCI send script error (status=0x%x)", status));

		/* Inform user cb */
		if (f_rsp_cb)
		{
			f_rsp_cb(h_rsp_cb, status);
		}
	}
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
*		rx_timeout - timeout in ms to wait for rx. 0 means no timeout is required. IMPORTANT: In P2P do not activate the timeout, since LLCP has internal mechanism for that.
*
* Output: None
*
* Return: Success - NCI_STATUS_OK
*		  Failure - NCI_STATUS_REJECTED
*
*******************************************************************************/
nci_status_e nci_send_data(nfc_handle_t h_nci, nfc_u8 conn_id, nfc_u8 *p_user_payload, nfc_u32 user_payload_len, nfc_u32 tx_timeout)
{
	struct nci_context *p_nci = (struct nci_context*)h_nci;

	return nci_data_prepare_2_send(p_nci->h_data, conn_id, p_user_payload, user_payload_len, tx_timeout);
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
	nci_status_e status = NCI_STATUS_OK;

	OSA_ASSERT(p_op);
	OSA_UNUSED_PARAMETER(h_nci);

	p_op->preamble = NCI_COMMAND_PREAMBLE;
	if(pu_param)
	{
		if(NCI_OPERATION_RF_CONFIG == opcode)
		{
			/* In case of RF_CONFIG operation, we generate an NCI command buffer based on core_set_config to be used later */
			pu_param->config.p_cmd = (struct nci_cmd *)osa_mem_alloc(sizeof(struct nci_cmd));
			status = nci_generate_cmd(	NCI_OPCODE_CORE_SET_CONFIG_CMD,
										(nci_cmd_param_u *)(&pu_param->config.core_set_config),
										&pu_param->config.p_cmd->p_buff,
										h_nci);
			if (status != NCI_STATUS_OK)
			{
				osa_mem_free(pu_param->config.p_cmd);
				osa_mem_free(p_op);
				pu_param->config.p_cmd = NULL;
				goto Finish;
			}
		}

		p_op->u_param = *pu_param;
	}

	p_op->f_rsp_cb = f_rsp_cb;
	p_op->h_rsp_cb = h_rsp_cb;
	p_op->pu_rsp = pu_rsp;
	p_op->opcode = opcode;
	p_op->rsp_len = rsp_len;
	osa_taskq_schedule(p_nci->h_osa, E_NCI_Q_OPERATION, p_op);

Finish:
	return status;
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

nci_status_e nci_send_vendor_specific_cmd(	nfc_handle_t h_nci,
											nfc_u16 opcode,
											nfc_u8* p_payload,
											nfc_u8 payload_len,
											vendor_specific_cb_f vendor_specific_rsp_cb,
											nfc_handle_t vendor_specific_cb_param)
{
	struct nci_cmd *p_nci_cmd = (struct nci_cmd *)osa_mem_alloc(sizeof(struct nci_cmd));
	struct nci_context *p_nci = (struct nci_context*)h_nci;
	nci_status_e status = NCI_STATUS_FAILED;

	if (p_nci_cmd != NULL)
	{
		struct nci_buff	*p_buff = NULL;
		nfc_u8		*p_data;

		p_buff = nci_buff_alloc(NCI_CTRL_HEADER_SIZE+payload_len);

		if (p_buff != NULL)
		{
			p_data = nci_buff_data(p_buff) + NCI_CTRL_HEADER_SIZE;
			NCI_SET_ARR(p_data, p_payload, payload_len);


			/* Prepare NCI header */
			p_data = nci_buff_data(p_buff);
			NCI_MT_SET(p_data, NCI_MT_COMMAND);
			NCI_PBF_SET(p_data, 0);
			NCI_OPCODE_SET(p_data, opcode);
			NCI_CTRL_LEN_SET(p_data, (nfc_u8)payload_len);

			status = NCI_STATUS_OK;
		}
		else
		{
			nci_buff_free (p_buff);
			if (status == NCI_STATUS_OK) status = NCI_STATUS_FAILED; // Amir - should be removed once the check of each parameter will be implemented
		}


		if (status == NCI_STATUS_OK)
		{
			p_nci_cmd->preamble = NCI_COMMAND_PREAMBLE;
			p_nci_cmd->opcode = opcode;
			p_nci_cmd->p_buff = p_buff;

			p_nci_cmd->f_rsp_cb = NULL;
			p_nci_cmd->f_raw_rsp_cb = NULL;
			p_nci_cmd->pu_rsp = NULL;
			p_nci_cmd->rsp_len = NULL;
			p_nci_cmd->h_rsp_cb = vendor_specific_cb_param;
			p_nci_cmd->vendor_specific_cb = vendor_specific_rsp_cb;

			osa_taskq_schedule(p_nci->h_osa, E_NCI_Q_CMD, p_nci_cmd);

		}
		else
		{
			osa_mem_free(p_nci_cmd);
		}
	}
	else
	{
		status = NCI_STATUS_MEM_FULL;
	}

	if (status != NCI_STATUS_OK) osa_report(ERROR, ("NCI send VENDOR specific command error (status= %d)", status));

	return status;
}

void nci_register_vendor_specific_ntf_cb(	nfc_handle_t h_nci,
											nfc_u16 opcode,
											vendor_specific_cb_f f_ntf_cb,
											nfc_handle_t vendor_specific_cb_param)
{
	struct nci_context *p_nci = (struct nci_context*)h_nci;
	nci_vendor_specific_ntf *p_ntf = (nci_vendor_specific_ntf*)osa_mem_alloc(sizeof(nci_vendor_specific_ntf));
	nfc_u8 ntf_id = NCI_OPCODE_OID_GET((nfc_u8*)&opcode);


	OSA_ASSERT(p_ntf);
	p_ntf->vendor_specific_ntf_cb = f_ntf_cb;
	p_ntf->vendor_specific_ntf_cb_param = vendor_specific_cb_param;
	p_ntf->p_next = p_nci->p_vendor_specific_ntf_tbl[ntf_id];
	p_nci->p_vendor_specific_ntf_tbl[ntf_id] = p_ntf;

}

void nci_unregister_vendor_specific_ntf_cb(	nfc_handle_t h_nci, nfc_u16 opcode,	vendor_specific_cb_f f_ntf_cb)
{
	struct nci_context *p_nci = (struct nci_context*)h_nci;
	nci_vendor_specific_ntf *p_prev = 0;
	nfc_u8 ntf_id = NCI_OPCODE_OID_GET((nfc_u8*)&opcode);
	nci_vendor_specific_ntf *p_ntf = p_nci->p_vendor_specific_ntf_tbl[ntf_id];

	while(p_ntf)
	{
		if(p_ntf->vendor_specific_ntf_cb == f_ntf_cb)
		{
			if(p_prev)
			{
				p_prev->p_next = p_ntf->p_next;
			}
			else
			{
				p_nci->p_vendor_specific_ntf_tbl[ntf_id] = p_ntf->p_next;
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
		p_nci->h_nci_rf_sm = nci_rf_sm_create(p_nci);
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
	nci_rf_sm_destroy(p_nci->h_nci_rf_sm);
	osa_mem_free(p_nci);

	gIsNCIActive = NFC_FALSE;

	osa_report(INFORMATION, ("NCI destroy 0x%x", p_nci));
}

#endif //#if (NCI_VERSION_IN_USE==NCI_VER_1)

/* EOF */











