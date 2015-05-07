/*
 * nci_data.c
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

  This source file contains the NCI Data Connections Handlers (Send, Receive
	including segmentation and reassembly hanlding)

*******************************************************************************/
#include "nci_api.h"

#if (NCI_VERSION_IN_USE==NCI_DRAFT_9)


#include "nfc_types.h"
#include "nci_dev.h"
#include "nfc_os_adapt.h"
#include "nci_int.h"
#include "nci_data_int.h"
#include "nci_utils.h"


#define MAX_LOGICAL_CONNS 16
/* NCI data structure - stores parameters of TX data
		Built during send_data and passed to specific DATA connection task queue */
struct nci_data
{
	nfc_u8				conn_id;
	nfc_u8				*p_user_payload;		/* user buffer pointer is being stored in send_data and will be retrieved in
													tx_done callback */
	nfc_u16				conn_instance;
	nfc_u32				user_payload_len;		/* user buffer length is being stored in send_data and will be retrieved in
													tx_done callback */
	struct nci_buff		*p_buff;
};


/* NCI Connection structure - stores connection parameters negotiated upon conn creation */
struct nci_conn
{
	/* Context for Packet Reassembly handling */
	nfc_handle_t			h_rx_fragments;	/* head of received fragment (nci buffers) list*/
	nfc_u32				rx_accumulated_length; /* accumulated length of incoming fragments */

	/* Context for Segmentation handling */
	nfc_u8				maximum_packet_payload_size;

	/* Context for Flow Control handling */
	nfc_u8				initial_fc_credit;
	nfc_u8				current_fc_credit;

	/* active connection flags (validity check) */
	nfc_u8				b_active_conn;
	nfc_u16				instance;
	nfc_u16				padding; /* 32 bits alignment */
};

struct nci_data_context
{
	nfc_handle_t			h_osa;
	nfc_handle_t			h_nci;
	nfc_handle_t			h_data_cb;		/* handle for the NCI received Data and transmission complete indication callbacks */
	data_cb_f			f_rx_ind_cb;	/* received data inidcation callback */
	data_cb_f			f_tx_done_cb;	/* transmission complete indication callback */
	nfc_u8				max_logical_connections;

	struct nci_conn		*p_conn_tbl;
};


#define enable_conn_taskq(p_ctx, conn_id)	osa_taskq_enable(p_ctx->h_osa, (nfc_u8)(E_NCI_Q_DATA + conn_id));
#define disable_conn_taskq(p_ctx, conn_id)	osa_taskq_disable(p_ctx->h_osa, (nfc_u8)(E_NCI_Q_DATA + conn_id));

/*******************************************************************************
********************** Static Routine ******************************************
*******************************************************************************/

/******************************************************************************
*
* Name: static_add_rx_fragment
*
* Description: Add received fragment to a data connection's fragment queue
*			  and update the total bytes length of data connection's queue.
*
* Input:	p_ctx - handle to NCI DATA context object
*		conn_id - Connection ID
*		p_buff - Received data buffer
*
* Output: None
*
* Return: Accumulated length of incoming fragments in the connection
*
*******************************************************************************/
static nfc_u32 static_add_rx_fragment(struct nci_data_context *p_ctx, nfc_u8 conn_id, struct nci_buff *p_buff)
{
	struct nci_conn *p_conn = &p_ctx->p_conn_tbl[conn_id];

	que_enqueue(p_conn->h_rx_fragments, p_buff);
	p_conn->rx_accumulated_length += nci_buff_length(p_buff);

	return p_conn->rx_accumulated_length;
}

/******************************************************************************
*
* Name: static_get_rx_packet
*
* Description: Prepare received packet from all fragments of data connection queue.
*			  Dequeue each fragment from data connection queue and concatenate
*			  it to received buffer p_rx_payload (this buffer has memory allocated
*			  before calling this function). At end set queue's accumulated_length to 0.
*
* Input:	p_ctx - handle to NCI DATA context object
*		conn_id - Connection ID
*
* Output: p_rx_payload - pointer of raw buffer for complete packet payload
*
* Return: None
*
*******************************************************************************/
static void static_get_rx_packet(struct nci_data_context *p_ctx, nfc_u8 conn_id, nfc_u8 *p_rx_payload)
{
	struct nci_conn *p_conn = &p_ctx->p_conn_tbl[conn_id];
	nfc_u32 offset = 0;
	struct nci_buff *p_buff = (struct nci_buff *)que_dequeue(p_conn->h_rx_fragments);

	while(p_buff)
	{

#ifndef NCI_DATA_BIG_ENDIAN
		nfc_u8 *p_temp = p_rx_payload + offset;
		NCI_SET_ARR_REV(p_temp, nci_buff_data(p_buff), nci_buff_length(p_buff));
#else
		osa_mem_copy(p_rx_payload + offset, nci_buff_data(p_buff), nci_buff_length(p_buff));
#endif
		offset += nci_buff_length(p_buff);
		nci_buff_free(p_buff);
		p_buff = (struct nci_buff *)que_dequeue(p_conn->h_rx_fragments);
	}
	OSA_ASSERT(offset == p_conn->rx_accumulated_length);
	p_conn->rx_accumulated_length = 0;
}

/******************************************************************************
*
* Name: static_fc_decrement_credit
*
* Description: Decrement flow-control credit (by 1) per-connection upon sending
*			  of frame. If new credit balance is now 0 than perform flow control -
*			  block connection queue.
*
* Input:	p_ctx - handle to NCI DATA context object
*		conn_id - Connection ID
*
* Output: None
*
* Return: None
*
*******************************************************************************/
static void static_fc_decrement_credit(struct nci_data_context *p_ctx, nfc_u8 conn_id)
{
	struct nci_conn *p_conn = &p_ctx->p_conn_tbl[conn_id];

	/* connection queue callback should be invoked only when credits are available
		(see flow control handling, below) */
	OSA_ASSERT(p_conn->current_fc_credit > 0);

	/* FLow Control Handling - Block connection queue when credit is exhausted */
	if(--p_conn->current_fc_credit == 0)
	{
		disable_conn_taskq(p_ctx, conn_id);
	}
}

/******************************************************************************
*
* Name: static_fc_increment_credit
*
* Description: Increment flow-control credit per-connection upon reception of credit
* 			  notification and release connection queue from blocking.
*
* Input:	p_ctx - handle to NCI DATA context object
*		conn_id - Connection ID
*		credit - credits to add
*
* Output: None
*
* Return: None
*
*******************************************************************************/
static void static_fc_increment_credit(struct nci_data_context *p_ctx, nfc_u8 conn_id, nfc_u8 credit)
{
	struct nci_conn *p_conn = &p_ctx->p_conn_tbl[conn_id];
	if(credit > 0)
	{

		p_conn->current_fc_credit = (nfc_u8)(p_conn->current_fc_credit + credit);
		osa_taskq_enable(p_ctx->h_osa, (nfc_u8)(E_NCI_Q_DATA + conn_id));
	}
}

/*******************************************************************************
	(static) notfication handlers:
		- conn_credit (for flow-control management)
		- activate (for static (RF) connection openning)
		- deactivate (for connection closure)
		- close conn
 *******************************************************************************/
/******************************************************************************
*
* Name: static_conn_credit_ntf
*
* Description: Callback that is installed by NCI DATA module creation. It will be
*			  invoked by NCI DATA notification callback in result of CORE_CONN_CREDITS request.
*
* Input:	h_data - handle to NCI data context object
*		opcode - NFCEE opcode
*		pu_ntf - NCI operation structure
*
* Output: None
*
* Return: None
*
*******************************************************************************/
static void static_conn_credit_ntf(nfc_handle_t h_data, nfc_u16 opcode, nci_ntf_param_u *pu_ntf)
{
	nfc_u8 i;
	OSA_UNUSED_PARAMETER(opcode);

	for(i=0; i<pu_ntf->conn_credit.num_of_entries; i++)
	{
		struct conn_credit *p_conn_credit =  &pu_ntf->conn_credit.conn_entries[i];
		static_fc_increment_credit(h_data, p_conn_credit->conn_id, p_conn_credit->credit);
	}

}

/******************************************************************************
*
* Name: static_activate_ntf
*
* Description: Callback that is installed by NCI DATA module creation. It will be
*			  invoked by NCI DATA notification callback in result of RF_ACTIVATE request.
*
* Input:	h_data - handle to NCI data context object
*		opcode - NFCEE opcode
*		pu_ntf - NCI operation structure
*
* Output: None
*
* Return: None
*
*******************************************************************************/
static void static_activate_ntf(nfc_handle_t h_data, nfc_u16 opcode, nci_ntf_param_u *pu_ntf)
{
	OSA_UNUSED_PARAMETER(opcode);
	OSA_UNUSED_PARAMETER(pu_ntf);

	nci_data_activate_rf_conn(h_data);
}

/******************************************************************************
*
* Name: static_deactivate_ntf
*
* Description: Callback that is installed by NCI DATA module creation. It will be
*			  invoked by NCI DATA notification callback in result of RF_DEACTIVATE request.
*
* Input:	h_data - handle to NCI data context object
*		opcode - NFCEE opcode
*		pu_ntf - NCI operation structure
*
* Output: None
*
* Return: None
*
*******************************************************************************/
static void static_deactivate_ntf(nfc_handle_t h_data, nfc_u16 opcode, nci_ntf_param_u *pu_ntf)
{
	OSA_UNUSED_PARAMETER(opcode);
	OSA_UNUSED_PARAMETER(pu_ntf);

	nci_data_deactivate_rf_conn(h_data);
}

/******************************************************************************
*
* Name: static_conn_close_ntf
*
* Description: Callback that is installed by NCI DATA module creation. It will be
*			  invoked by NCI DATA notification callback in result of CORE_CONN_CLOSE request.
*
* Input:	h_data - handle to NCI data context object
*		opcode - NFCEE opcode
*		pu_ntf - NCI operation structure
*
* Output: None
*
* Return: None
*
*******************************************************************************/
static void static_conn_close_ntf(nfc_handle_t h_data, nfc_u16 opcode, nci_ntf_param_u *pu_ntf)
{
	OSA_UNUSED_PARAMETER(opcode);
	OSA_UNUSED_PARAMETER(pu_ntf);

	nci_data_close_conn(h_data, 0);
}
/*******************************************************************************
********************** Exported Method ******************************************
*******************************************************************************/

/******************************************************************************
*
* Name: nci_data_prepare_2_send
*
* Description: Prepare data for transmission (including segmentation if needed) and
*			  insert packet(s) to connection queue.
* 			  If connection is not active than do not prepare packet and notify upper stack on failure.
*			  NOTE: The packet is not sent yet, only prepared!!
*			  Triggered when user calls nci_send_data.
*
*
* Input:	h_data - handle to NCI data context object
*		conn_id - Connection ID
*		p_user_payload - handle to data packet
*		user_payload_len -length of data packet
*
* Output: None
*
* Return: Success - NCI_STATUS_OK
*		  Failure - NCI_STATUS_REJECTED
*
*******************************************************************************/
nci_status_e nci_data_prepare_2_send(nfc_handle_t h_data, nfc_u8 conn_id, nfc_u8 *p_user_payload, nfc_u32 user_payload_len)
{
	struct nci_data_context *p_ctx = (struct nci_data_context *)h_data;
	nfc_u8 *p_data;
	nfc_u32 total_len = user_payload_len;
#ifndef NCI_DATA_BIG_ENDIAN
	nfc_u8 *p_offset = p_user_payload + user_payload_len;
#else
	nfc_u8 *p_offset = p_user_payload;
#endif
	struct nci_conn *p_conn = &p_ctx->p_conn_tbl[conn_id];
	nfc_u8 b_active_conn = p_conn->b_active_conn;
	nfc_u8 maximum_packet_payload_size = p_conn->maximum_packet_payload_size;


	if((b_active_conn == NCI_CONNECTION_DISABLED) && (maximum_packet_payload_size > 0))
	{
		/* Connection is not active - Notify the upper stack on failure */
		if(p_ctx->f_tx_done_cb)
		{
			p_ctx->f_tx_done_cb(p_ctx->h_data_cb, conn_id, p_user_payload, user_payload_len, NCI_STATUS_REJECTED);
		}
		return NCI_STATUS_REJECTED;
	}

	while(total_len)
	{
		nfc_u8 *p_temp = NULL;
		struct nci_data *p_nci_data = (struct nci_data *)osa_mem_alloc(sizeof(struct nci_data));
		nfc_u32 frame_len = (total_len < maximum_packet_payload_size) ?
						total_len : maximum_packet_payload_size;

		OSA_ASSERT(p_nci_data);
		OSA_ASSERT(maximum_packet_payload_size > 0);

		p_nci_data->conn_id = conn_id;
		p_nci_data->conn_instance = p_conn->instance;
		p_nci_data->p_buff = nci_buff_alloc(frame_len + NCI_DATA_HEADER_SIZE);
		p_data = nci_buff_data(p_nci_data->p_buff);

		NCI_MT_SET(p_data, NCI_MT_DATA);
		NCI_CONNID_SET(p_data, ((nfc_u8)conn_id));
		NCI_DATA_LEN_SET(p_data, (nfc_u8)frame_len);
		if(frame_len == total_len)
		{
			p_nci_data->p_user_payload = p_user_payload; /* User pointer marks the last (or only) fragment */
			p_nci_data->user_payload_len = user_payload_len;
			NCI_PBF_SET(p_data, 0);
		}
		else
		{
			p_nci_data->p_user_payload = NULL; /* NULL pointer marks an intermediate fragment */
			p_nci_data->user_payload_len = 0;
			NCI_PBF_SET(p_data, 1);
		}

#ifndef NCI_DATA_BIG_ENDIAN
		p_offset -= frame_len;
		p_temp = p_data+NCI_DATA_HEADER_SIZE;
		NCI_SET_ARR_REV(p_temp, p_offset, frame_len);
#else
		osa_mem_copy(p_data+NCI_DATA_HEADER_SIZE, p_offset, frame_len);
		p_offset += frame_len;
#endif
		osa_taskq_schedule(p_ctx->h_osa, (nfc_u8)(E_NCI_Q_DATA+conn_id), p_nci_data);

		total_len -= frame_len;
	}
	return NCI_STATUS_OK;
}

/******************************************************************************
*
* Name: nci_data_send
*
* Description: Send data packet, per connection, to transport driver.
*			  Packet is sent only if connection is active else NCI Internal
*			  Data strcuture is freed.
*			  If packet sent successfully than upper layer is notified.
*			  Called by nci_data_cb.
*
* Input: 	h_data - handle to NCI DATA context object
*		h_nci_data - handle to NCI DATA object
*
* Output: None
*
* Return: None - should return send result to user
*
*******************************************************************************/
void nci_data_send(nfc_handle_t h_data, nfc_handle_t h_nci_data)
{
	struct nci_data_context *p_ctx = (struct nci_data_context *)h_data;
	struct nci_data *p_nci_data = (struct nci_data *)h_nci_data;
	struct nci_conn *p_conn = &p_ctx->p_conn_tbl[p_nci_data->conn_id];
	nci_status_e status = NCI_STATUS_FAILED;

	/* send data only if it was sent on existing open connection (marked by conn_instance) */
	if(p_conn->b_active_conn && (p_conn->instance == p_nci_data->conn_instance))
	{
		/* Now, Send buffer to transport driver */
		if(nci_trans_send(p_ctx->h_nci, p_nci_data->p_buff) == NFC_RES_OK)
		{
			/*
			  * Buffer was sent successfully to transport driver
			  */
			status = NCI_STATUS_OK;
		}

		/*
		  * Flow control - decrement credits even if buffer was not sent
		  */
		static_fc_decrement_credit(p_ctx, p_nci_data->conn_id);
	}
	else
	{
		status = NCI_STATUS_FLUSHED;
	}

	/*
	  * Notify the upper stack. Note that tx_done callback is invoked only
	  * after sending of the last fragment (that is marked by non-NULL
	  * p_user_payload).
	  * NOTE: If we have several fragments than this mechanism reports
	  * the status of last sent fragment without any indication on previous
	  * fragments' sent status, i.e. we may report status OK when one or
	  * more fragments were not sent at all.
	  */
	if(p_ctx->f_tx_done_cb && p_nci_data->p_user_payload)
	{
		p_ctx->f_tx_done_cb(p_ctx->h_data_cb, p_nci_data->conn_id,
						p_nci_data->p_user_payload, p_nci_data->user_payload_len, status);
	}

	/*
	 * Free data buffer here because transport driver does not do it
	 */
	nci_buff_free(p_nci_data->p_buff);

	/* Free the NCI Internal Data strcuture */
	osa_mem_free(p_nci_data);
}

/******************************************************************************
*
* Name: nci_data_receive
*
* Description: Function is called (by nci_rx_cb) when receiving fragment data packet.
*			  The fragment is added to a data connection's frgament queue.
*			  If this is the last fragment than memory is allocated, all fragments are
*			  reassembed and copied to it and it's passed to higher level by calling callback routine.
*			  Allocated memory is freed.
*
* Input: 	h_data - handle to NCI DATA context object
*		conn_id - Connection ID
*		p_buff - buffer containing fragment data received
*		b_last_fragment -If not 0 than it's last fragment
*
* Output: None
*
* Return: None
*
*******************************************************************************/
void nci_data_receive (nfc_handle_t h_data, nfc_u8 conn_id, struct nci_buff *p_buff, nfc_u8 b_last_fragment)
{
	struct nci_data_context *p_ctx = (struct nci_data_context *)h_data;
	nfc_u32 pending_rx_length;
	nfc_u8 *p_rx_data;

	/* Append incoming buffer to Rx_buff and increment bytes counter*/
	pending_rx_length = static_add_rx_fragment(p_ctx, conn_id, p_buff);

	/* for the last fragment - serialize (copy) fragments into single buffer and send to user */
	if(b_last_fragment)
	{
		p_rx_data = (nfc_u8 *)osa_mem_alloc(pending_rx_length);
		OSA_ASSERT(p_rx_data);
		static_get_rx_packet(p_ctx, conn_id, p_rx_data);
		p_ctx->f_rx_ind_cb(p_ctx->h_data_cb, conn_id, p_rx_data, pending_rx_length, NCI_STATUS_OK);
		osa_mem_free(p_rx_data);
	}
}

/******************************************************************************
*
* Name: nci_data_open_conn
*
* Description: Open Data Connection upon CONN_CREATE response.
*			  Initial connection parameters.
*			  Connection is now enabled to send\receive data if it is not RF data connection.
*
* Input: 	h_data - handle to NCI DATA context object
*		conn_id - Connection ID
*		initial_num_of_credits - credits initial value
*		maximum_packet_payload_size -max payload size
*
* Output: None
*
* Return: None
*
*******************************************************************************/
void nci_data_open_conn(nfc_handle_t h_data, nfc_u8 conn_id, nfc_u8 initial_num_of_credits, nfc_u8 maximum_packet_payload_size)
{
	struct nci_data_context *p_ctx = (struct nci_data_context *)h_data;
	struct nci_conn *p_conn = &p_ctx->p_conn_tbl[conn_id];

	p_conn->rx_accumulated_length = 0;
	p_conn->h_rx_fragments = que_create();

	p_conn->initial_fc_credit = initial_num_of_credits;
	p_conn->current_fc_credit = initial_num_of_credits;
	p_conn->maximum_packet_payload_size = maximum_packet_payload_size;

	p_conn->instance ++;

	/* Enable only if it's a dynamic connection */
	if ((conn_id != 0) && (p_conn->current_fc_credit>0))
	{
		enable_conn_taskq(p_ctx, conn_id);
		p_conn->b_active_conn = NCI_CONNECTION_ENABLED;
	}
	else
	{
		/* RF Conn (=0) is activated only after ACTIVATE NTF
			calls nci_data_enable_rf_conn */
		p_conn->b_active_conn = NCI_CONNECTION_DISABLED;
	}
}

/******************************************************************************
*
* Name: nci_data_close_conn
*
* Description: Close Data Connection upon CONN_CLOSE response or notification and module destruction.
*			  Connection is marked as closed.
*			  All fragments memory is freed.
*			  Fragment's queue is released.
*
* Input: 	h_data - handle to NCI DATA context object
*		conn_id - Connection ID
*
* Output: None
*
* Return: None
*
*******************************************************************************/
void nci_data_close_conn(nfc_handle_t h_data, nfc_u8 conn_id)
{
	struct nci_data_context *p_ctx = (struct nci_data_context *)h_data;
	struct nci_conn *p_conn = &p_ctx->p_conn_tbl[conn_id];
	struct nci_buff *p_buff;

	p_conn->b_active_conn = NCI_CONNECTION_DISABLED; /* Marks connection as closed
								 (prepare_2_send will use this to reject new send requests) */
	p_conn->maximum_packet_payload_size = 0;

	/* Flush RX fragment queue */
	if(p_conn->h_rx_fragments != NULL)
	{
		p_buff = (struct nci_buff *)que_dequeue(p_conn->h_rx_fragments);
		while(p_buff)
		{
			nci_buff_free(p_buff);
			p_buff = (struct nci_buff *)que_dequeue(p_conn->h_rx_fragments);
		}
		p_conn->rx_accumulated_length = 0;
		que_destroy(p_conn->h_rx_fragments);
		p_conn->h_rx_fragments = NULL;
	}

	/* Flush pending TX packets */
	p_ctx->p_conn_tbl[conn_id].instance ++; /* Increment connection instance */
	/* nci_data_send CB will drop packets from previous connection, based on conn_instance */
}

/******************************************************************************
*
* Name: nci_data_activate_rf_conn
*
* Description: Enable RF data connection. RF Connection ID is always 0 in p_conn_tbl.
*			  Connection actual credit is set to initial value.
*			  Connection is now enabled to send\receive data.
*
* Input: 	h_data - handle to NCI DATA context object
*
* Output: None
*
* Return: None
*
*******************************************************************************/
void nci_data_activate_rf_conn(nfc_handle_t h_data)
{
	struct nci_data_context *p_ctx = (struct nci_data_context *)h_data;
	struct nci_conn *p_conn = &p_ctx->p_conn_tbl[0];

	p_conn->instance ++; /* Increment connection instance */

	p_conn->current_fc_credit = p_conn->initial_fc_credit;
	enable_conn_taskq(p_ctx, 0);

	p_conn->b_active_conn = NCI_CONNECTION_ENABLED;
}

/******************************************************************************
*
* Name: nci_data_deactivate_rf_conn
*
* Description: Disable RF data connection. RF Connection ID is always 0 in p_conn_tbl.
*			  Connection actual credit is set to 0.
*
* Input: 	h_data - handle to NCI DATA context object
*
* Output: None
*
* Return: None
*
*******************************************************************************/
void nci_data_deactivate_rf_conn(nfc_handle_t h_data)
{
	struct nci_data_context *p_ctx = (struct nci_data_context *)h_data;
	struct nci_conn *p_conn = &p_ctx->p_conn_tbl[0];

	p_conn->b_active_conn = NCI_CONNECTION_DISABLED;
	p_conn->current_fc_credit = 0;
	p_conn->instance ++; /* Increment connection instance */
}

/******************************************************************************
*
* Name: nci_data_init_conns
*
* Description: Initiate NCI Connections context according to received
*			  max_logical_connections (within init response).
*
* Input: 	h_data - handle to NCI DATA context object
*		max_logical_connections - number of logical connections
*
* Output: None
*
* Return: None
*
*******************************************************************************/
void nci_data_init_conns(nfc_handle_t h_data, nfc_u8 max_logical_connections)
{
	struct nci_data_context *p_ctx = (struct nci_data_context *)h_data;

	OSA_ASSERT(max_logical_connections <= MAX_LOGICAL_CONNS);

	/* Initiate connections context */
	p_ctx->max_logical_connections = max_logical_connections;
}

/*******************************************************************************
	NCI Module constructor
*******************************************************************************/

/******************************************************************************
*
* Name: nci_data_create
*
* Description: NCI DATA main module initialization. Called by nci_create.
*			  Allocate memory and initialize parameters.
*			  Register notification callbacks.
*			  Register all connections to separate queues in OSA.
*
* Input: 	h_nci - handle to NCI object
*		h_osa - handle to OSA object
*		f_rx_ind_cb -callback function to be called when receiving data per connection
*		f_tx_done_cb -callback function to be called when packet transferring (to NFCC) is completed (when transport send function is done)
*		h_data_cb - handle for the NCI received Data and transmission complete indication callbacks
*
* Output: None
*
* Return: handle to created data main object
*
*******************************************************************************/
nfc_handle_t nci_data_create(nfc_handle_t h_nci, nfc_handle_t h_osa, data_cb_f f_rx_ind_cb, data_cb_f f_tx_done_cb, nfc_handle_t h_data_cb)
{
	nfc_u32 i;
	struct nci_data_context *p_ctx = (struct nci_data_context *)osa_mem_alloc(sizeof(struct nci_data_context));
	OSA_ASSERT(p_ctx);

	p_ctx->h_nci = h_nci;
	p_ctx->h_osa = h_osa;
	p_ctx->f_rx_ind_cb = f_rx_ind_cb;
	p_ctx->f_tx_done_cb = f_tx_done_cb;
	p_ctx->h_data_cb = h_data_cb;
	p_ctx->max_logical_connections = 0;

	/* register to relevent notifications */
	nci_register_ntf_cb(p_ctx->h_nci, E_NCI_NTF_CORE_CONN_CREDITS, static_conn_credit_ntf, p_ctx);
	nci_register_ntf_cb(p_ctx->h_nci, E_NCI_NTF_RF_ACTIVATE, static_activate_ntf, p_ctx);
	nci_register_ntf_cb(p_ctx->h_nci, E_NCI_NTF_RF_DEACTIVATE, static_deactivate_ntf, p_ctx);
	nci_register_ntf_cb(p_ctx->h_nci, E_NCI_NTF_CORE_CONN_CLOSE, static_conn_close_ntf, p_ctx);

	p_ctx->p_conn_tbl = (struct nci_conn *)osa_mem_alloc(MAX_LOGICAL_CONNS * sizeof(struct nci_conn));
	OSA_ASSERT(p_ctx->p_conn_tbl);

	for(i=0; i<MAX_LOGICAL_CONNS; i++)
	{
		osa_taskq_register(p_ctx->h_osa, (nfc_u8)(E_NCI_Q_DATA+i), nci_data_cb, p_ctx->h_nci, 0);
		p_ctx->p_conn_tbl[i].instance = 0;
		p_ctx->p_conn_tbl[i].b_active_conn = NCI_CONNECTION_DISABLED;
		p_ctx->p_conn_tbl[i].h_rx_fragments = NULL;
	}
	return p_ctx;
}

/*******************************************************************************
	NCI Module destructor
*******************************************************************************/
/******************************************************************************
*
* Name: nci_data_destroy
*
* Description: DATA module destruction.
*			  Close connections and frees object allocated memory.
*
* Input:	  h_data - handle to NCI DATA context object
*
* Output: None
*
* Return: None
*
*******************************************************************************/
void nci_data_destroy(nfc_handle_t h_data)
{
	struct nci_data_context *p_ctx = (struct nci_data_context *)h_data;
	nfc_u8 i;

	nci_unregister_ntf_cb(p_ctx->h_nci, E_NCI_NTF_CORE_CONN_CREDITS, static_conn_credit_ntf);
	nci_unregister_ntf_cb(p_ctx->h_nci, E_NCI_NTF_RF_ACTIVATE, static_activate_ntf);
	nci_unregister_ntf_cb(p_ctx->h_nci, E_NCI_NTF_RF_DEACTIVATE, static_deactivate_ntf);
	nci_unregister_ntf_cb(p_ctx->h_nci, E_NCI_NTF_CORE_CONN_CLOSE, static_conn_close_ntf);

	/* First close all open conntection to free their resources */
	for(i=0; i<p_ctx->max_logical_connections; i++)
	{
		nci_data_close_conn(h_data, i);
	}

	osa_mem_free(p_ctx->p_conn_tbl);
	osa_mem_free(p_ctx);
}


#endif //#if (NCI_VERSION_IN_USE==NCI_DRAFT_9)

/* EOF */











