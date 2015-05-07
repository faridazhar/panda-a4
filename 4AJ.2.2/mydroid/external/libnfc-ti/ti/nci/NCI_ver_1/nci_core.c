/*
 * nci_ver_1\nci_core.c
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

  This source file contains the implementation of NCI Single Context Taksq Callbacks

*******************************************************************************/
#include "nci_api.h"

#if (NCI_VERSION_IN_USE==NCI_VER_1)

#include "nfc_types.h"
#include "nci_dev.h"
#include "nfc_os_adapt.h"
#include "nci_int.h"
#include "nci_data_int.h"
#include "ncix_int.h"
#include "nci_utils.h"
#include "nci_rf_sm.h"


/*******************************************************************************
	Command Watchdog Expiration Callback
*******************************************************************************/
/******************************************************************************
*
* Name: cmd_watchdog_cb
*
* Description: Watchdog callback called when command timeout expires
*
* Input:	h_nci -handle to NCI object
*
* Output: None
*
* Return: None
*
*******************************************************************************/
void cmd_watchdog_cb(nfc_handle_t h_nci)
{
	OSA_UNUSED_PARAMETER(h_nci);
	osa_report(ERROR, ("Command Watchdog Expired"));
	OSA_ASSERT(0);

	/* In this case we kill the NFC service process.
	   In Android it will automatically restarted  */
	osa_exit(NFC_TRUE);
}

/*******************************************************************************
	(TX) Command Handler
*******************************************************************************/
/******************************************************************************
*
* Name: nci_cmd_cb
*
* Description: Callback function that is activated when sending command
*
* Input:	h_nci -handle to NCI object
*		h_nci_cmd - handle to NCI command
*
* Output: None
*
* Return: None
*
*******************************************************************************/
void nci_cmd_cb(nfc_handle_t h_nci, nfc_handle_t h_nci_cmd)
{
	struct nci_context *p_nci = (struct nci_context*)h_nci;
	struct nci_cmd *p_nci_cmd = (struct nci_cmd *)h_nci_cmd;

	p_nci->curr_cmd = *p_nci_cmd;

	/* Disable command queue (Flow-control) */
	osa_taskq_disable(p_nci->h_osa, E_NCI_Q_CMD);

	nci_trans_send(p_nci, p_nci_cmd->p_buff);

	/* Start command watchdog */
	p_nci->h_cmd_watchdog = osa_timer_start(p_nci->h_osa, p_nci->cmd_timeout, cmd_watchdog_cb, p_nci);

	// Free command buffer
	nci_buff_free(p_nci_cmd->p_buff);
	/* Free internal command structure */
	osa_mem_free(p_nci_cmd);
}

/*******************************************************************************
	(TX) DATA Handler
*******************************************************************************/
/******************************************************************************
 *
* Name: nci_data_cb
*
* Description: Callback function that is activated when sending data
*
* Input:	h_nci -handle to NCI object
*		h_nci_data - handle to NCI data
*
* Output: None
*
* Return: None
*
*******************************************************************************/
void nci_data_cb(nfc_handle_t h_nci, nfc_handle_t h_nci_data)
{
	struct nci_context *p_nci = (struct nci_context*)h_nci;

	/* Invoke connection specific "Send" handler */
	nci_data_send(p_nci->h_data, h_nci_data);

}

/*******************************************************************************
	RX (Data, Response and Notification) Handler
*******************************************************************************/
/******************************************************************************
*
* Name: nci_rx_cb
*
* Description: Callback function that is activated when receiving data \ response \ notification
*
* Input:	h_nci -handle to NCI object
*		p_param - handle to received buffer containing data \ response \ notification message
*
* Output: None
*
* Return: None
*
*******************************************************************************/
void nci_rx_cb (nfc_handle_t h_nci, nfc_handle_t p_param)
{
	struct nci_context *p_nci = (struct nci_context*)h_nci;
	struct nci_buff *p_buff = (struct nci_buff *)p_param;
	nfc_u8 *p_payload, *p_header = nci_buff_data(p_buff);
	nfc_u8 mt = NCI_MT_GET(p_header);
	nfc_u8 gid = NCI_GID_GET(p_header);
	nfc_u32 payload_len;

	if(mt == NCI_MT_DATA)
	{
		nfc_u8 conn_id = NCI_CONNID_GET(p_header);
		nfc_u8 b_last_fragment = (NCI_PBF_GET(p_header) == 0)?1:0;

		nci_buff_cut_prefix(p_buff, NCI_DATA_HEADER_SIZE); /* Cut NCI header (received in each fragment)
															   	and keep payload only */
		nci_data_receive(p_nci->h_data, conn_id, p_buff, b_last_fragment);
		return;
	}
	else if (mt == NCI_MT_RESPONSE && p_nci->curr_cmd.f_raw_rsp_cb != NULL)
	{
		/* stop command watchdog */
		if(p_nci->h_cmd_watchdog != NULL)
			osa_timer_stop(p_nci->h_cmd_watchdog);
		p_nci->h_cmd_watchdog = NULL;

		/* Handle RAW command response */
		payload_len = nci_buff_length(p_buff);

		/* enable command flow-control */
		osa_taskq_enable(p_nci->h_osa, E_NCI_Q_CMD);

		p_nci->curr_cmd.f_raw_rsp_cb(p_nci->curr_cmd.h_rsp_cb, p_header, payload_len);
	}
	else
	{
		nfc_u16	opcode = NCI_OPCODE_GET(p_header);
		p_payload = nci_buff_cut_prefix(p_buff, NCI_CTRL_HEADER_SIZE); /* Cut NCI header (received in each fragment)
																	   	and keep payload only */
		if (p_payload)
		{
			payload_len = nci_buff_length(p_buff);

			switch(mt)
			{
			case NCI_MT_RESPONSE:
				{
					nci_rsp_param_u rsp = {0};
					rsp.generic.status = NCI_STATUS_OK;

					if(gid != NCI_GID_PROPRIETARY)
						payload_len = nci_parse_rsp(opcode, p_payload, &rsp, payload_len);
					else
					{
						//Here we are handling vendor specific responses
						payload_len = nci_buff_length(p_buff);
						if(p_nci->curr_cmd.vendor_specific_cb)
						{
							p_nci->curr_cmd.vendor_specific_cb(p_nci->curr_cmd.h_rsp_cb, opcode, p_payload, (nfc_u8)payload_len);
						}
					}


					/* copy response to user buffer */
					if(p_nci->curr_cmd.pu_rsp)
					{
						OSA_ASSERT(p_nci->curr_cmd.rsp_len >= payload_len);
						osa_mem_copy(p_nci->curr_cmd.pu_rsp, &rsp, payload_len);
					}

					/* Todo - remove this when all error cases are handled */
					if(NCI_STATUS_OK != rsp.generic.status)
					{
						osa_report(ERROR, ("Received ERROR in response! status= 0x%x", rsp.generic.status));
						OSA_ASSERT(NFC_FALSE && "nci_core:nci_rx_cb");
					}

					/* stop command watchdog */
					if(p_nci->h_cmd_watchdog != NULL)
						osa_timer_stop(p_nci->h_cmd_watchdog);
					p_nci->h_cmd_watchdog = NULL;

					/* invoke specific handlers for internal events */
					if(opcode == NCI_OPCODE_CORE_INIT_RSP)
					{
						nci_data_init_conns(p_nci->h_data, rsp.init.max_logical_connections);
						p_nci->max_control_packet_size = rsp.init.max_control_packet_payload_length;
					}
					if(opcode == NCI_OPCODE_CORE_CONN_CREATE_RSP)
					{
						struct nci_rsp_conn_create *p_conn = &rsp.conn_create;
						nci_data_open_conn(p_nci->h_data, p_conn->conn_id, p_conn->initial_num_of_credits, p_conn->maximum_packet_payload_size);
					}
					if(opcode == NCI_OPCODE_CORE_CONN_CLOSE_RSP)
					{
						nci_data_close_conn(p_nci->h_data, p_nci->curr_cmd.u_cmd.conn_close.conn_id);
					}


					/* enable command flow-control */
					osa_taskq_enable(p_nci->h_osa, E_NCI_Q_CMD);

					/* invoke user response callback */
					if(p_nci->curr_cmd.f_rsp_cb)
					{
						p_nci->curr_cmd.f_rsp_cb(p_nci->curr_cmd.h_rsp_cb, opcode, &rsp);
					}
				}
				break;
			case NCI_MT_NOTIFICATION:
				{
					if(gid != NCI_GID_PROPRIETARY)
					{
						nci_ntf_param_u		ntf;
						nci_ntf_e			ntf_id = E_NCI_NTF_MAX;
						struct nci_ntf *p_ntf;

						payload_len = nci_parse_ntf(opcode, p_payload, &ntf_id, &ntf, payload_len);

						// Process all the callbacks registered on this notification
						OSA_ASSERT(ntf_id != E_NCI_NTF_MAX);
						for (p_ntf = p_nci->p_ntf_tbl[ntf_id]; p_ntf; p_ntf=p_ntf->p_next)
						{
							if(p_ntf->f_ntf_cb)
								p_ntf->f_ntf_cb(p_ntf->h_ntf_cb, opcode, &ntf);
						}
						for (p_ntf = p_nci->p_ntf_tbl[E_NCI_NTF_ALL]; p_ntf; p_ntf=p_ntf->p_next)
						{
							if(p_ntf->f_ntf_cb)
								p_ntf->f_ntf_cb(p_ntf->h_ntf_cb, opcode, &ntf);
						}
					}
					else
					{
						nci_vendor_specific_ntf *p_ntf = NULL;
						nfc_u8 ntf_id = NCI_OPCODE_OID_GET((nfc_u8 *)&opcode);

						//Here we are handling vendor specific notifications
						payload_len = nci_buff_length(p_buff);

						OSA_ASSERT(ntf_id < NCI_VENDOR_SPECIFIC_NTF_MAX);

						for (p_ntf = p_nci->p_vendor_specific_ntf_tbl[ntf_id]; p_ntf; p_ntf=p_ntf->p_next)
						{
							if(p_ntf->vendor_specific_ntf_cb)
								p_ntf->vendor_specific_ntf_cb(p_ntf->vendor_specific_ntf_cb_param, opcode, p_payload, (nfc_u8)payload_len);
						}
					}

				}
				break;
			}
		}
		nci_buff_free(p_buff);
	}
}

nci_status_e nci_send_internal_cmd(	nfc_handle_t h_nci, nfc_u16 opcode,
									struct nci_cmd *p_nci_cmd,
									nci_rsp_param_u *pu_rsp,
									nfc_u8 rsp_len,
									rsp_cb_f f_rsp_cb,
									nfc_handle_t h_rsp_cb)
{
	struct nci_context *p_nci = (struct nci_context*)h_nci;
	nci_status_e status = NCI_STATUS_OK;

	if (p_nci_cmd != NULL)
	{

			p_nci_cmd->preamble = NCI_COMMAND_PREAMBLE;
			p_nci_cmd->f_rsp_cb = f_rsp_cb;
			p_nci_cmd->pu_rsp = pu_rsp;
			p_nci_cmd->opcode = opcode;
			p_nci_cmd->rsp_len = rsp_len;
			p_nci_cmd->h_rsp_cb = h_rsp_cb;
			p_nci_cmd->vendor_specific_cb = NULL;
			p_nci_cmd->f_raw_rsp_cb = NULL;
			osa_mem_set(&(p_nci_cmd->u_cmd), 0, sizeof(nci_cmd_param_u));

			osa_taskq_schedule(p_nci->h_osa, E_NCI_Q_CMD, p_nci_cmd);

	}
	else
	{
		status = NCI_STATUS_MESSAGE_CORRUPTED;
	}

	if (status != NCI_STATUS_OK)
		osa_report(ERROR, ("NCI send command error (status= 0x%x)", status));

	return status;
}

nci_status_e nci_configure_listen_routing(nfc_handle_t h_nci, configure_listen_routing_t* p_config)
{
	struct nci_context *p_nci = (struct nci_context*)h_nci;
	nci_status_e status = NCI_STATUS_OK;

	nci_rf_sm_configure_listen_routing(p_nci->h_nci_rf_sm, p_config);

	return status;
}

#endif //#if (NCI_VERSION_IN_USE==NCI_VER_1)

/* EOF */
