/*
 * nci_trans.c
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

  This source file contains the NCI transport layer (supporting ncidev i/f)

*******************************************************************************/
#include "nci_api.h"

#if (NCI_VERSION_IN_USE==NCI_DRAFT_9)

#include "nfc_types.h"
#include "nci_dev.h"
#include "nfc_os_adapt.h"
#include "nci_int.h"


struct ncidev			*g_h_ncidev = NULL;

#ifdef NCI_DBG

nfc_s8 *dbg_core_strs[] =
{
	"NCI_OPCODE_CORE_RESET",
	"NCI_OPCODE_CORE_INIT",
	"NCI_OPCODE_CORE_SET_CONFIG",
	"NCI_OPCODE_CORE_GET_CONFIG",
	"NCI_OPCODE_CORE_CONN_CREATE",
	"NCI_OPCODE_CORE_CONN_ACCEPT",
	"NCI_OPCODE_CORE_CONN_CLOSE",
	"NCI_OPCODE_CORE_CONN_CREDITS",
	"NCI_OPCODE_CORE_RF_FIELD_INFO",
	"NCI_OPCODE_CORE_GENERIC_ERROR",
	"NCI_OPCODE_CORE_INTERFACE_ERROR",
};

nfc_s8 *dbg_rf_mgmt_strs[] =
{
	"NCI_OPCODE_RF_DISCOVER_MAP",
	"NCI_OPCODE_RF_SET_LISTEN_MODE_ROUTING",
	"NCI_OPCODE_RF_GET_LISTEN_MODE_ROUTING",
	"NCI_OPCODE_RF_DISCOVER",
	"NCI_OPCODE_RF_DISCOVER_SEL",
	"NCI_OPCODE_RF_ACTIVATE",
	"NCI_OPCODE_RF_DEACTIVATE",
	"NCI_OPCODE_RF_T3T_POLLING",
};

nfc_s8 *dbg_nfcee_mgmt_strs[] =
{
	"NCI_OPCODE_NFCEE_DISCOVER",
	"NCI_OPCODE_NFCEE_MODE_SET",
};

nfc_s8 *dbg_ce_strs[] =
{
	"NCI_OPCODE_CE_ACTION",
};

nfc_s8 *dbg_test_strs[] =
{
	"NCI_OPCODE_TEST_RF_CONTROL",
};

nfc_s8 **dbg_strs[] =
{
	dbg_core_strs,
	dbg_rf_mgmt_strs,
	dbg_nfcee_mgmt_strs,
	dbg_ce_strs,
	dbg_test_strs,
};

typedef struct dbg_tlv
{
	nfc_u8 type;
	nfc_s8 *str;
}dbg_tlv_t;

dbg_tlv_t dbg_tlv_data[] =
{
	{NCI_CFG_TYPE_TOTAL_DURATION,"NCI_CFG_TYPE_TOTAL_DURATION"},
	{NCI_CFG_TYPE_CON_DEVICES_LIMIT,"NCI_CFG_TYPE_CON_DEVICES_LIMIT"},

	{NCI_CFG_TYPE_PA_BAIL_OUT,"NCI_CFG_TYPE_PA_BAIL_OUT"},

	{NCI_CFG_TYPE_PB_AFI,"NCI_CFG_TYPE_PB_AFI"},
	{NCI_CFG_TYPE_PB_BAIL_OUT,"NCI_CFG_TYPE_PB_BAIL_OUT"},

	{NCI_CFG_TYPE_PF_BIT_RATE			,"NCI_CFG_TYPE_PF_BIT_RATE"},

	{NCI_CFG_TYPE_PB_H_INFO			,"NCI_CFG_TYPE_PB_H_INFO"},

	{NCI_CFG_TYPE_BITR_NFC_DEP			,"NCI_CFG_TYPE_BITR_NFC_DEP"},
	{NCI_CFG_TYPE_ATR_REQ_GEN_BYTES			,"NCI_CFG_TYPE_ATR_REQ_GEN_BYTES"},
	{NCI_CFG_TYPE_ATR_REQ_CONFIG			,"NCI_CFG_TYPE_ATR_REQ_CONFIG"},

	{NCI_CFG_TYPE_LA_PROTOCOL_TYPE		,"NCI_CFG_TYPE_LA_PROTOCOL_TYPE"},
	{NCI_CFG_TYPE_LA_NFCID1				,"NCI_CFG_TYPE_LA_NFCID1"},

	{NCI_CFG_TYPE_LB_NFCID0				,"NCI_CFG_TYPE_LB_NFCID0"},
	{NCI_CFG_TYPE_LB_APPLICATION_DATA	,"NCI_CFG_TYPE_LB_APPLICATION_DATA"},
	{NCI_CFG_TYPE_LB_SFGI					,"NCI_CFG_TYPE_LB_SFGI"},
	{NCI_CFG_TYPE_LB_ADC_FO					,"NCI_CFG_TYPE_LB_ADC"},

	{NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_1	,"NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_1"},
	{NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_2	,"NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_2"},
	{NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_3	,"NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_3"},
	{NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_4	,"NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_4"},
	{NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_5	,"NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_5"},
	{NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_6	,"NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_6"},
	{NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_7	,"NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_7"},
	{NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_8	,"NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_8"},
	{NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_9	,"NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_9"},
	{NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_10	,"NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_10"},
	{NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_11	,"NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_11"},
	{NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_12,"NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_12"},
	{NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_13	,"NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_13"},
	{NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_14,"NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_14"},
	{NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_15	,"NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_15"},
	{NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_16	,"NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_16"},
	{NCI_CFG_TYPE_LF_T3T_PMM				,"NCI_CFG_TYPE_LF_T3T_PMM"},
	{NCI_CFG_TYPE_LF_T3T_MAX				,"NCI_CFG_TYPE_LF_T3T_MAX"},
	{NCI_CFG_TYPE_LF_PROTOCOL_TYPE1		,"NCI_CFG_TYPE_LF_PROTOCOL_TYPE1"},
	{NCI_CFG_TYPE_LF_T3T_FLAGS2			,"NCI_CFG_TYPE_LF_T3T_FLAGS2"},
	{NCI_CFG_TYPE_LF_CON_BITR_F			,"NCI_CFG_TYPE_LF_CON_BITR_F"},


	{NCI_CFG_TYPE_FWI					,"NCI_CFG_TYPE_FWI"},
	{NCI_CFG_TYPE_LA_HIST_BY		,"NCI_CFG_TYPE_LA_HIST_BY"},
	{NCI_CFG_TYPE_LB_PROTOCOL_TYPE		,"NCI_CFG_TYPE_LB_PROTOCOL_TYPE"},
	{NCI_CFG_TYPE_LB_H_INFO_RESP					,"NCI_CFG_TYPE_LB_H_INFO_RESP"},


	{NCI_CFG_TYPE_WT						,"NCI_CFG_TYPE_WT"},
	{NCI_CFG_TYPE_ATR_RES_GEN_BYTES			,"NCI_CFG_TYPE_ATR_RES_GEN_BYTES"},
	{NCI_CFG_TYPE_ATR_RES_CONFIG			,"NCI_CFG_TYPE_ATR_RES_CONFIG"},

	{NCI_CFG_TYPE_RF_FIELD_INFO			,"NCI_CFG_TYPE_RF_FIELD_INFO"},
	{NCI_CFG_TYPE_NFCDEP_OP		,"NCI_CFG_TYPE_NFCDEP_OP"},
};

nfc_s8 dbg_tlv_data_num_recs = sizeof(dbg_tlv_data) / sizeof(dbg_tlv_t);

char* get_tlv_str(nfc_u8 type)
{
	nfc_s8 i = 0;

	for(i = 0; i < dbg_tlv_data_num_recs; i++)
	{
		if(dbg_tlv_data[i].type == type)
			return dbg_tlv_data[i].str;
	}

	OSA_ASSERT(0);

	return NULL;
}


#endif /* NCI_DBG */


/******************************************************************************
*
* Name: ncidev_register
*
* Description: Register ncidev handle to list of ncidev handles
*
* Input: 	p_ncidev - handle to ncidev object
*
* Output: None
*
* Return: Success - NFC_RES_OK
*		  Failure - NFC_RES_ERROR - ncidev object already exist
*
*******************************************************************************/
nfc_status ncidev_register(struct ncidev *p_ncidev)
{
	struct ncidev *p_list_dev = g_h_ncidev;

	while(p_list_dev)
	{
		if(p_list_dev->dev_id == p_ncidev->dev_id)
		{
			osa_report(ERROR, ("nci device is already registered (%d)", p_ncidev->dev_id));
			return NFC_RES_ERROR;
		}
		p_list_dev = p_list_dev->p_next;
	}
	p_ncidev->p_next = g_h_ncidev;
	g_h_ncidev = p_ncidev;
	return NFC_RES_OK;
}

/******************************************************************************
*
* Name: ncidev_unregister
*
* Description: Un-Register ncidev handle from list of ncidev handles
*
* Input: 	p_ncidev - handle to ncidev object
*
* Output: None
*
* Return: None
*
*******************************************************************************/
void ncidev_unregister(struct ncidev *p_ncidev)
{
	struct ncidev *p_list_dev = g_h_ncidev;
	struct ncidev *p_list_prev = g_h_ncidev;

	while(p_list_dev)
	{
		if(p_list_dev->dev_id == p_ncidev->dev_id)
		{
			if(p_list_dev == g_h_ncidev)
				g_h_ncidev = p_list_dev->p_next;
			else
				p_list_prev->p_next = p_list_dev->p_next;
			osa_report(INFORMATION, ("nci device un-registered (%d)", p_ncidev->dev_id));
			return;
		}
		p_list_prev = p_list_dev;
		p_list_dev = p_list_dev->p_next;
	}

	return;
}

/******************************************************************************
*
* Name: nci_trans_get_ncidev
*
* Description: Find ncidev handle in list of ncidev handles according to dev id
*
* Input: 	dev_id - ncidev object dev id
*
* Output: None
*
* Return: Success - ncidev handle
*		  Failure - NULL - ncidev object not found
*
*******************************************************************************/
nfc_handle_t nci_trans_get_ncidev(nfc_u32 dev_id)
{
	struct ncidev *p_list_dev = g_h_ncidev;

	while(p_list_dev)
	{
		if(p_list_dev->dev_id == dev_id)
			break;

		p_list_dev = p_list_dev->p_next;
	}
	return (nfc_handle_t)p_list_dev;
};

/******************************************************************************
*
* Name: ncidev_receive
*
* Description: Receive packet from RX channel and queue it on NCI RX queue
*
* Input: 	p_ncidev - handle to ncidev object
*		p_buff - handle to buffer containing data received
*
* Output: None
*
* Return: NFC_RES_COMPLETE
*
*******************************************************************************/
nfc_status ncidev_receive(struct ncidev *p_ncidev, struct nci_buff *p_buff)
{
	osa_taskq_schedule(p_ncidev->h_osa, E_NCI_Q_RX, p_buff);
	return NFC_RES_COMPLETE;
}

/******************************************************************************
*
* Name: ncidev_log
*
* Description: Print NCI message information for debugging
*
* Input: 	p_ncidev - ncidev object
*		p_buff - buffer containing message
*		p_prefix - printed message prefix which notes direction - Rx \ Tx
*
* Output: None
*
* Return: None
*
*******************************************************************************/
void ncidev_log(struct ncidev *p_ncidev, struct nci_buff *p_buff, char *p_prefix)
{
#ifdef NCI_DBG
	nfc_u8 *p_hdr = (nfc_u8*)nci_buff_data(p_buff);
	nfc_u32 mt = NCI_MT_GET(p_hdr), header_len , data_len, conn_id, opcode;
	nfc_u8 pbf = NCI_PBF_GET(p_hdr);
	nfc_s8 *p_dbg = "Invalid", *p_type = "Invalid";
	OSA_UNUSED_PARAMETER(p_ncidev);

	switch (mt)
	{
		case NCI_MT_DATA:
		{
			header_len = NCI_DATA_HEADER_SIZE;
			conn_id = NCI_CONNID_GET(p_hdr);
			data_len = NCI_DATA_LEN_GET(p_hdr);
			osa_report(INFORMATION, ("%s NCI Hdr: MT=Data, PBF=%d, ConnId=%d", p_prefix, pbf, conn_id));
			osa_dump_buffer("   NCI Payload (DATA)",  &p_hdr[header_len], data_len);
		}
		break;
		default: /* Control */
		{
			nfc_s8 *p_suffix = "Invalid";

			header_len = NCI_CTRL_HEADER_SIZE;
			opcode = NCI_OPCODE_GET(p_hdr);
			data_len = NCI_CTRL_LEN_GET(p_hdr);
			if(mt == NCI_MT_COMMAND)
			{
				p_type = "Cmd";
				p_suffix = "_CMD";
				p_dbg = dbg_strs[opcode>>8][(opcode&0x3f)];
			}
			else if(mt == NCI_MT_NOTIFICATION)
			{
				p_type = "Ntf";
				p_suffix = "_NTF";
				p_dbg = dbg_strs[opcode>>8][(opcode&0x3f)];
			}
			else if(mt == NCI_MT_RESPONSE)
			{
				p_type = "Rsp";
				p_suffix = "_RSP";
				p_dbg = dbg_strs[opcode>>8][(opcode&0x3f)];
			}
			osa_report(INFORMATION, ("%s NCI Hdr: MT=%s, PBF=%d, Opcode=%s%s [%02x %02x %02x]", p_prefix, p_type, pbf, p_dbg, p_suffix,
				p_hdr[0], p_hdr[1], p_hdr[2]));

			if(mt == NCI_MT_COMMAND && opcode == NCI_OPCODE_CORE_SET_CONFIG_CMD)
			{
				nfc_u8 i, tlv_num = p_hdr[header_len++];
				osa_report(INFORMATION, ("%s Set Config: number of TLVs = %d", p_prefix, tlv_num));
				for(i=0; i<tlv_num; i++)
				{
					osa_report(INFORMATION, ("%s             TLV Type = %s, Len = %d", p_prefix, get_tlv_str(p_hdr[header_len]), p_hdr[header_len+1]));
				    osa_dump_buffer("           TLV Payload: ",  &p_hdr[header_len+2], p_hdr[header_len+1]);
					header_len += (2 + p_hdr[header_len+1]);
				}
				return;
			}
			osa_dump_buffer("   NCI Payload (CTRL)",  &p_hdr[header_len], data_len);
		}
		break;
	}
#endif /* NCI_DBG */
}

/******************************************************************************
*
* Name: nci_trans_open
*
* Description: Open connection to transport driver
*
* Input: 	h_nci - handle to NCI object
*
* Output: None
*
* Return: Result from trans_open() - NFC_RES_OK \ NFC_RES_MEM_ERROR
*
*******************************************************************************/
nfc_status nci_trans_open(nfc_handle_t h_nci)
{
	struct nci_context *p_nci = (struct nci_context*)h_nci;
	struct ncidev *p_ncidev = p_nci->h_ncidev;

	return p_ncidev->open(p_ncidev->h_trans);
}

/******************************************************************************
*
* Name: nci_trans_close
*
* Description: Close connection to transport driver
*
* Input: 	h_nci - handle to NCI object
*
* Output: None
*
* Return: Result from trans_close() - NFC_RES_OK
*
*******************************************************************************/
nfc_status nci_trans_close(nfc_handle_t h_nci)
{
	struct nci_context *p_nci = (struct nci_context*)h_nci;
	struct ncidev *p_ncidev = p_nci->h_ncidev;

	return p_ncidev->close(p_ncidev->h_trans);
}

/******************************************************************************
*
* Name: nci_trans_send
*
* Description: Send data packet to transport driver
*
* Input: 	h_nci - handle to NCI object
*		p_buff - data packet
*
* Output: None
*
* Return: Result from trans_send() - NFC_RES_OK \ NFC_RES_ERROR
*
*******************************************************************************/
nfc_status nci_trans_send(nfc_handle_t h_nci, struct nci_buff *p_buff)
{
	struct nci_context *p_nci = (struct nci_context*)h_nci;
	struct ncidev *p_ncidev = p_nci->h_ncidev;

	return p_ncidev->send(p_ncidev->h_trans, p_buff);
}


#endif //#if (NCI_VERSION_IN_USE==NCI_DRAFT_9)

/* EOF */











