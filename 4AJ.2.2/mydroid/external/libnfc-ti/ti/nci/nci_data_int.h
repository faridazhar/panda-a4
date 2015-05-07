/*
 * nci_data_int.h
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

  This header files contains the definitions for the NFC Controller Interface.

*******************************************************************************/

#ifndef __NCI_DATA_INT_H
#define __NCI_DATA_INT_H

#include "nfc_types.h"
#include "nci_buff.h"

#if (NCI_VERSION_IN_USE==NCI_DRAFT_9)


/*********************************************************************************/
/* NCI Global Routines                                                           */
/*********************************************************************************/

/*** nci data handlers ***/
/*******************************************************************************
	(TX) Prepare data for transmission (including segmentation if needed) and
	insert packet(s) to connection queue.
	triggered when user calls nci_send_data API.
*******************************************************************************/
nci_status_e nci_data_prepare_2_send(nfc_handle_t h_data, nfc_u8 conn_id, nfc_u8 *p_user_payload, nfc_u32 user_payload_len);

/*******************************************************************************
	(TX) Send Data Handler (per Connection) - Send data packet to transport
	driver (within NCI single context)
*******************************************************************************/
void nci_data_send(nfc_handle_t h_data, nfc_handle_t h_nci_data);

/*******************************************************************************
	(RX) Receive Data Handler (per Connection) including fragment reassembly.
	Called within NCI single context.
*******************************************************************************/
void nci_data_receive (nfc_handle_t h_data, nfc_u8 conn_id, struct nci_buff *p_buff, nfc_u8 b_last_fragment);

/*** nci (logical) data connections handlers ***/
/*******************************************************************************
	Open Data Connection (upon conn create response)
	NFCEE Connection is activated upon openning, RF conn is not activated
*******************************************************************************/
void nci_data_open_conn(nfc_handle_t h_data, nfc_u8 conn_id, nfc_u8 initial_num_of_credits, nfc_u8 maximum_packet_payload_size);

/*******************************************************************************
	Close Data Connection (upon CLOSE_CONN RSP or NTF and upon module destruction)
*******************************************************************************/
void nci_data_close_conn(nfc_handle_t h_data, nfc_u8 conn_id);

/*******************************************************************************
	Enable RF Data Connection (upon ACTIVATE NTF)
*******************************************************************************/
void nci_data_activate_rf_conn(nfc_handle_t h_data);

/*******************************************************************************
	Disable RF Data Connection (upon DE-ACTIVATE NTF)
*******************************************************************************/
void nci_data_deactivate_rf_conn(nfc_handle_t h_data);

/*** nci data module costructor/destrcutor and init handler ***/
/*******************************************************************************
	NCI Module constructor
*******************************************************************************/
nfc_handle_t nci_data_create(nfc_handle_t h_nci, nfc_handle_t h_osa, data_cb_f f_rx_ind_cb, data_cb_f f_tx_done_cb, nfc_handle_t h_data_cb);
/*******************************************************************************
	NCI Module destructor
*******************************************************************************/
void nci_data_destroy(nfc_handle_t h_data);
/*******************************************************************************
	Setting the maximum connections as declared within init response
*******************************************************************************/
void nci_data_init_conns(nfc_handle_t h_data, nfc_u8 max_logical_connections);

#else

#error Incorrect NCI version, expected NCI_DRAFT_9

#endif //#if (NCI_VERSION_IN_USE==NCI_DRAFT_9)

#endif /* __NCI_DATA_INT_H */
























