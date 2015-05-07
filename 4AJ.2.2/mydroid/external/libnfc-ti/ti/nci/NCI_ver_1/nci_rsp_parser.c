/*
 * nci_ver_1\nci_rsp_parser.c
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

  This source file contains the NCI Response Parser Utility

*******************************************************************************/
#include "nci_api.h"

#if (NCI_VERSION_IN_USE==NCI_VER_1)

#include "nfc_types.h"
#include "nci_dev.h"
#include "nfc_os_adapt.h"
#include "nci_int.h"


/*******************************************************************************
	Response Parser
*******************************************************************************/

/******************************************************************************
*
* Name: nci_parse_rsp
*
* Description: Parse received response message
*			  Finds out actual response code, data and length
*			  Put them in output parameters
*
* Input: opcode - NCI command opcode from header
*		p_data - response data buffer received
*		len -response data buffer received length
*
* Output: pu_rsp -buffer that holds actual response message params
*
* Return: Actual response message size (one of structures in nci_rsp_param_u)
*
*******************************************************************************/
nfc_u32 nci_parse_rsp(nfc_u16 opcode, nfc_u8 *p_data, nci_rsp_param_u *pu_rsp, nfc_u32 len)
{
	NCI_GET_1B(p_data, pu_rsp->generic.status);

	switch(opcode)
	{
		case NCI_OPCODE_CORE_RESET_RSP:
		{
			OSA_ASSERT(len == NCI_CORE_RESET_RSP_SIZE);
			NCI_GET_1B(p_data, pu_rsp->reset.version);
			NCI_GET_1B(p_data, pu_rsp->reset.config_status);
			len = sizeof(pu_rsp->reset);
		}
		break;
		case NCI_OPCODE_CORE_INIT_RSP:
		{
			NCI_GET_ARR(p_data, pu_rsp->init.nfcc_features.array, NCI_CORE_INIT_RSP_FEATURES_SIZE);
			NCI_GET_1B(p_data, pu_rsp->init.num_supported_rf_interfaces);
			NCI_GET_ARR(p_data, pu_rsp->init.supported_rf_interfaces, pu_rsp->init.num_supported_rf_interfaces);
			NCI_GET_1B(p_data, pu_rsp->init.max_logical_connections);
			NCI_GET_2B(p_data, pu_rsp->init.max_routing_table_size);
			NCI_GET_1B(p_data, pu_rsp->init.max_control_packet_payload_length);
			NCI_GET_2B(p_data, pu_rsp->init.max_size_for_large_params);
			NCI_GET_1B(p_data, pu_rsp->init.manufacturer_id)
			NCI_GET_ARR(p_data, pu_rsp->init.manufacturer_specific_info, NCI_CORE_INIT_RSP_MANUFACTURER_SPECIFIC_SIZE);
			len = sizeof(pu_rsp->init);
		}
		break;
		case NCI_OPCODE_NFCEE_DISCOVER_RSP:
		{
			OSA_ASSERT(len == NCI_NFCEE_DISCOVER_RSP_SIZE);
			NCI_GET_1B(p_data, pu_rsp->nfcee_discover.num_nfcee);
			len = sizeof(pu_rsp->nfcee_discover);
		}
		break;
		case NCI_OPCODE_NFCEE_MODE_SET_RSP:
		{
			OSA_ASSERT(len == NCI_NFCEE_MODE_SET_RSP_SIZE);
			len = sizeof(pu_rsp->generic);
		}
		break;
		case NCI_OPCODE_CORE_CONN_CREATE_RSP:
		{
			OSA_ASSERT(len == NCI_CORE_CONN_CREATE_RSP_SIZE);
			NCI_GET_1B(p_data, pu_rsp->conn_create.maximum_packet_payload_size);
			NCI_GET_1B(p_data, pu_rsp->conn_create.initial_num_of_credits);
			NCI_GET_1B(p_data, pu_rsp->conn_create.conn_id);
			pu_rsp->conn_create.conn_id = NCI_CONNID_GET(&pu_rsp->conn_create.conn_id);
			len = sizeof(pu_rsp->conn_create);
		}
		break;
		case NCI_OPCODE_CORE_CONN_CLOSE_RSP:
		{
			OSA_ASSERT(len == NCI_CORE_CONN_CLOSE_RSP_SIZE);
			len = sizeof(pu_rsp->generic);
		}
		break;
		case NCI_OPCODE_CORE_SET_CONFIG_RSP:
		{
			NCI_GET_1B(p_data, pu_rsp->core_set_config.num_of_params);
			if(pu_rsp->generic.status == NCI_STATUS_INVALID_PARAM)
			{
				nfc_u8 i;
				for(i=0; i<pu_rsp->core_set_config.num_of_params; i++)
				{
					NCI_GET_1B(p_data, pu_rsp->core_set_config.param_id[i]);
				}
				OSA_ASSERT(len == (nfc_u8)(NCI_CORE_SET_CONFIG_RSP_MIN_SIZE+pu_rsp->core_set_config.num_of_params));
			}
			else
			{
				OSA_ASSERT(len == NCI_CORE_SET_CONFIG_RSP_MIN_SIZE);
			}
			len = sizeof(pu_rsp->core_set_config);
		}
		break;
		case NCI_OPCODE_CORE_GET_CONFIG_RSP:
		{
			nfc_u8 i;
			nfc_u8 total_length = NCI_CORE_GET_CONFIG_RSP_MIN_SIZE; /* status (1 byte) + Number of Parameters (1 byte) fields */
			NCI_GET_1B(p_data, pu_rsp->core_set_config.num_of_params);

			osa_report(INFORMATION, ("$$$$$$$$$$>> CORE_GET_CONFIG_RSP: status %d. num_of_params=%d.",pu_rsp->generic.status,pu_rsp->core_get_config.num_of_params));

			if( (pu_rsp->generic.status == NCI_STATUS_INVALID_PARAM) || /* Requesting parameters that are not available in NFCC */
				(pu_rsp->generic.status == NCI_STATUS_MESSAGE_SIZE_EXCEEDED)) /* NFCC reports that message size with all requested parameters would exceed the maximum Control Message size (> 255bytes) */
			{
				/* Do not do anything */
			}
			for(i=0; i<pu_rsp->core_get_config.num_of_params; i++)
			{
				NCI_GET_1B(p_data, pu_rsp->core_get_config.params[i].type);
				NCI_GET_1B(p_data, pu_rsp->core_get_config.params[i].length);
				total_length += (NCI_CORE_GET_CONFIG_RSP_MIN_SIZE + pu_rsp->core_get_config.params[i].length); /* type (1 byte) + length (1 byte) + value's length */
				NCI_GET_ARR(p_data, pu_rsp->core_get_config.params[i].value, pu_rsp->core_get_config.params[i].length);
				osa_report(INFORMATION, ("$$$$$$$$$$>> CORE_GET_CONFIG_RSP: index %d: type 0x%02x. length=%d. ",i,pu_rsp->core_get_config.params[i].type,pu_rsp->core_get_config.params[i].length));
				osa_dump_buffer("$$$$$$$$$$>> CORE_GET_CONFIG_RSP TLV Payload: ",  pu_rsp->core_get_config.params[i].value, pu_rsp->core_get_config.params[i].length);
			}
			OSA_ASSERT(len == total_length);

			len = sizeof(pu_rsp->core_get_config);
		}
		break;
		default:
			OSA_ASSERT(len == NCI_CORE_DEFAULT_RSP_SIZE);
			len = sizeof(pu_rsp->generic);
	}
	return (len);
}

#endif //#if (NCI_VERSION_IN_USE==NCI_VER_1)

/* EOF */











