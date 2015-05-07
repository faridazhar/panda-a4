/*
 * nci_ver_1\nci_cmd_builder.c
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

  This souce file contains the NCI Command Generation utility

*******************************************************************************/
#include "nci_api.h"

#if (NCI_VERSION_IN_USE==NCI_VER_1)

#include "nfc_types.h"
#include "nci_dev.h"
#include "nfc_os_adapt.h"
#include "nci_int.h"


/*******************************************************************************
	Command Builder
*******************************************************************************/

/******************************************************************************
 *
* Name: nci_generate_cmd
*
* Description: Build NCI command from opcode and parameters.
*			  Allocate memory for the command (memory shall be released by task handling the command).
*			  Add NCI header too.
*			  Result is stored in pp_nci_buff.
*
* Input:	opcode -NCI command opcode
*		pu_cmd -NCI command parameters
*		h_nci -handle to NCI object
*
* Output: pp_nci_buff - Structure containing built NCI command
*
* Return: Success - NCI_STATUS_OK
*		  Failure - NCI_STATUS_FAILED
*
*******************************************************************************/
nci_status_e nci_generate_cmd(nfc_u16 opcode, nci_cmd_param_u *pu_cmd, struct nci_buff **pp_nci_buff, nfc_handle_t h_nci)
{
	struct nci_buff	*p_buff = NULL;
	nfc_u8		*p_data;
	nfc_u32		len, i;
	nci_status_e status = NCI_STATUS_OK;

	switch(opcode)
	{
		case NCI_OPCODE_CORE_RESET_CMD:
			p_buff = nci_buff_alloc(NCI_CTRL_HEADER_SIZE+NCI_CORE_RESET_CMD_SIZE);
			p_data = nci_buff_data(p_buff) + NCI_CTRL_HEADER_SIZE;
			NCI_SET_1B(p_data, pu_cmd->core_reset.keep_config);
		break;
		case NCI_OPCODE_CORE_INIT_CMD:
			p_buff = nci_buff_alloc(NCI_CTRL_HEADER_SIZE+NCI_CORE_INIT_CMD_SIZE);
			p_data = nci_buff_data(p_buff) + NCI_CTRL_HEADER_SIZE;
		break;
		case NCI_OPCODE_NFCEE_DISCOVER_CMD:
			p_buff = nci_buff_alloc(NCI_CTRL_HEADER_SIZE+NCI_NFCEE_DISCOVER_CMD_SIZE);
			p_data = nci_buff_data(p_buff) + NCI_CTRL_HEADER_SIZE;
			NCI_SET_1B(p_data, pu_cmd->nfcee_discover.action);
		break;
		case NCI_OPCODE_NFCEE_MODE_SET_CMD:
			p_buff = nci_buff_alloc(NCI_CTRL_HEADER_SIZE+NCI_NFCEE_MODE_SET_CMD_SIZE);
			p_data = nci_buff_data(p_buff) + NCI_CTRL_HEADER_SIZE;
			NCI_SET_1B(p_data, pu_cmd->nfcee_mode_set.nfcee_id);
			NCI_SET_1B(p_data, pu_cmd->nfcee_mode_set.nfcee_mode);
		break;
		case NCI_OPCODE_CORE_SET_CONFIG_CMD:
		{
			nfc_u8 *p_data_base;
			struct nci_context *p_nci = (struct nci_context*)h_nci;
			nfc_u8 max_pkt_size;
			nfc_u8 payload_total_length = NCI_CORE_SET_CONFIG_CMD_MIN_SIZE; /* num_of_tlvs field (1 byte) */

			/*
			  * Calculate total payload length
			  */
			for(i=0; i<pu_cmd->core_set_config.num_of_tlvs; i++)
				payload_total_length += (NCI_TLV_HEADER_LEN + pu_cmd->core_set_config.p_tlv[i].length); /* Type (1 Byte) + Length (1 byte) + value's length */

			/*
			  * Verify that total payload length <= max_control_packet_size
			  */
			max_pkt_size = (p_nci->max_control_packet_size != 0) ? p_nci->max_control_packet_size : NCI_MAX_CONTROL_PACKET_SIZE;
			if(payload_total_length > max_pkt_size)
			{
				/*
				  * ERROR - total payload length > max_control_packet_size so return ERROR
				  */
				return NCI_STATUS_FAILED;
			}

			p_buff = nci_buff_alloc(NCI_CTRL_HEADER_SIZE+payload_total_length);
			p_data = nci_buff_data(p_buff) + NCI_CTRL_HEADER_SIZE;
			p_data_base = p_data;
			NCI_SET_1B(p_data, pu_cmd->core_set_config.num_of_tlvs);
			for(i=0; i<pu_cmd->core_set_config.num_of_tlvs; i++)
			{
				NCI_SET_1B(p_data, pu_cmd->core_set_config.p_tlv[i].type);
				NCI_SET_1B(p_data, pu_cmd->core_set_config.p_tlv[i].length);
#ifdef NCI_BIG_ENDIAN
				NCI_SET_ARR(p_data, pu_cmd->core_set_config.p_tlv[i].p_value, pu_cmd->core_set_config.p_tlv[i].length);
#else
				NCI_SET_ARR_REV(p_data, pu_cmd->core_set_config.p_tlv[i].p_value, pu_cmd->core_set_config.p_tlv[i].length);
#endif
			}
			OSA_ASSERT(p_data - p_data_base == payload_total_length);
		}
		break;
		case NCI_OPCODE_CORE_GET_CONFIG_CMD:
		{
			nfc_u8 *p_data_base;

			p_buff = nci_buff_alloc(NCI_CTRL_HEADER_SIZE+pu_cmd->core_get_config.num_of_params+NCI_CORE_GET_CONFIG_CMD_MIN_SIZE);
			p_data = nci_buff_data(p_buff) + NCI_CTRL_HEADER_SIZE;
			p_data_base = p_data;
			NCI_SET_1B(p_data, pu_cmd->core_get_config.num_of_params);

			osa_report(INFORMATION, ("#########>> CORE_GET_CONFIG_CMD: num_of_params=%d",pu_cmd->core_get_config.num_of_params));

			for(i=0; i<pu_cmd->core_get_config.num_of_params; i++)
			{
				NCI_SET_1B(p_data, pu_cmd->core_get_config.param_type[i]);
				osa_report(INFORMATION, ("#########>> CORE_GET_CONFIG_CMD: index %d: param type =0x%02x.",i,pu_cmd->core_get_config.param_type[i]));
			}
			OSA_ASSERT(p_data - p_data_base == (pu_cmd->core_get_config.num_of_params+NCI_CORE_GET_CONFIG_CMD_MIN_SIZE));
		}
		break;
		case NCI_OPCODE_CORE_CONN_CREATE_CMD:
			{
				nfc_u8 payload_total_length = 0;

				for(i=0; i<pu_cmd->conn_create.num_of_tlvs; i++)
					payload_total_length += (NCI_TLV_HEADER_LEN + pu_cmd->conn_create.p_tlv[i].length);

				payload_total_length += sizeof(pu_cmd->conn_create.destination_type);
				payload_total_length += sizeof(pu_cmd->conn_create.num_of_tlvs);

				p_buff = nci_buff_alloc(NCI_CTRL_HEADER_SIZE + payload_total_length);
				p_data = nci_buff_data(p_buff) + NCI_CTRL_HEADER_SIZE;

				NCI_SET_1B(p_data, pu_cmd->conn_create.destination_type);
				NCI_SET_1B(p_data, pu_cmd->conn_create.num_of_tlvs);

				for(i=0; i<pu_cmd->conn_create.num_of_tlvs; i++)
				{
					NCI_SET_1B(p_data, pu_cmd->conn_create.p_tlv[i].type);
					NCI_SET_1B(p_data, pu_cmd->conn_create.p_tlv[i].length);
#ifdef NCI_BIG_ENDIAN
					NCI_SET_ARR(p_data, pu_cmd->conn_create.p_tlv[i].p_value, pu_cmd->conn_create.p_tlv[i].length);
#else
					NCI_SET_ARR_REV(p_data, pu_cmd->conn_create.p_tlv[i].p_value, pu_cmd->conn_create.p_tlv[i].length);
#endif
				}
			}
		break;
		case NCI_OPCODE_CORE_CONN_CLOSE_CMD:
			p_buff = nci_buff_alloc(NCI_CTRL_HEADER_SIZE+NCI_CORE_CONN_CLOSE_CMD_SIZE);
			p_data = nci_buff_data(p_buff) + NCI_CTRL_HEADER_SIZE;
			NCI_SET_1B(p_data, pu_cmd->conn_close.conn_id);
		break;
		case NCI_OPCODE_RF_DISCOVER_SEL_CMD:
			p_buff = nci_buff_alloc(NCI_CTRL_HEADER_SIZE+NCI_RF_DISCOVER_SEL_CMD_SIZE);
			p_data = nci_buff_data(p_buff) + NCI_CTRL_HEADER_SIZE;
			NCI_SET_1B(p_data, pu_cmd->rf_discover_select.rf_discovery_id);
			NCI_SET_1B(p_data, pu_cmd->rf_discover_select.rf_protocol);
			NCI_SET_1B(p_data, pu_cmd->rf_discover_select.rf_interface);
		break;
		case NCI_OPCODE_RF_DISCOVER_CMD:
			p_buff = nci_buff_alloc(NCI_CTRL_HEADER_SIZE+pu_cmd->discover.num_of_conf*NCI_RF_DISCOVER_CMD_CONF_SIZE+NCI_RF_DISCOVER_CMD_MIN_SIZE);
			p_data = nci_buff_data(p_buff) + NCI_CTRL_HEADER_SIZE;
			NCI_SET_1B(p_data, pu_cmd->discover.num_of_conf);
			for(i=0; i<pu_cmd->discover.num_of_conf; i++)
			{
				NCI_SET_1B(p_data, pu_cmd->discover.disc_conf[i].type);
				NCI_SET_1B(p_data, pu_cmd->discover.disc_conf[i].frequency);
			}
		break;
		case NCI_OPCODE_RF_DISCOVER_MAP_CMD:
			p_buff = nci_buff_alloc(NCI_CTRL_HEADER_SIZE+pu_cmd->discover_map.num_of_conf*NCI_RF_DISCOVER_MAP_CMD_CONF_SIZE+NCI_RF_DISCOVER_MAP_CMD_MIN_SIZE);
			p_data = nci_buff_data(p_buff) + NCI_CTRL_HEADER_SIZE;
			NCI_SET_1B(p_data, pu_cmd->discover_map.num_of_conf);
			for(i=0; i<pu_cmd->discover_map.num_of_conf; i++)
			{
				NCI_SET_1B(p_data, pu_cmd->discover_map.disc_map_conf[i].rf_protocol);
				NCI_SET_1B(p_data, pu_cmd->discover_map.disc_map_conf[i].mode);
				NCI_SET_1B(p_data, pu_cmd->discover_map.disc_map_conf[i].rf_interface_type);
			}
		break;

		case NCI_OPCODE_RF_DEACTIVATE_CMD:
			p_buff = nci_buff_alloc(NCI_CTRL_HEADER_SIZE+NCI_RF_DEACTIVATE_CMD_SIZE);
			p_data = nci_buff_data(p_buff) + NCI_CTRL_HEADER_SIZE;
			NCI_SET_1B(p_data, pu_cmd->deactivate.type);
		break;

		case NCI_OPCODE_RF_SET_LISTEN_MODE_ROUTING_CMD:
			{
				nfc_u8 *p_data_base;
				struct nci_context *p_nci = (struct nci_context*)h_nci;
				nfc_u8 max_pkt_size;
				nfc_u8 payload_total_length = 0;

				/* This Control Message SHALL be at least one Routing Entry*/
				OSA_ASSERT(pu_cmd->rf_set_listen_mode_routing.num_of_routing_entries > 0);

				/*
				  * Calculate total payload length
				  */
				payload_total_length = sizeof(nfc_u8) + sizeof(nfc_u8); /*more_to_follow size + num_of_tlvs size*/
				for(i=0; i<pu_cmd->rf_set_listen_mode_routing.num_of_routing_entries; i++)
					payload_total_length += (NCI_TLV_HEADER_LEN + pu_cmd->rf_set_listen_mode_routing.p_routing_entries[i].length); /* Type (1 Byte) + Length (1 byte) + value's length */

				/*
				  * Verify that total payload length <= max_control_packet_size
				  */
				max_pkt_size = (p_nci->max_control_packet_size != 0) ? p_nci->max_control_packet_size : NCI_MAX_CONTROL_PACKET_SIZE;
				if(payload_total_length > max_pkt_size)
				{
					/*
					  * ERROR - total payload length > max_control_packet_size so return ERROR
					  */
					return NCI_STATUS_FAILED;
				}

				p_buff = nci_buff_alloc(NCI_CTRL_HEADER_SIZE+payload_total_length);
				p_data = nci_buff_data(p_buff) + NCI_CTRL_HEADER_SIZE;
				p_data_base = p_data;
				NCI_SET_1B(p_data, pu_cmd->rf_set_listen_mode_routing.more_to_follow);
				NCI_SET_1B(p_data, pu_cmd->rf_set_listen_mode_routing.num_of_routing_entries);
				for(i=0; i<pu_cmd->rf_set_listen_mode_routing.num_of_routing_entries; i++)
				{
					NCI_SET_1B(p_data, pu_cmd->rf_set_listen_mode_routing.p_routing_entries[i].type);
					NCI_SET_1B(p_data, pu_cmd->rf_set_listen_mode_routing.p_routing_entries[i].length);
					//TBD - check if required NCI_SET_ARR_REV for AID
					NCI_SET_ARR(p_data, pu_cmd->rf_set_listen_mode_routing.p_routing_entries[i].p_value, pu_cmd->rf_set_listen_mode_routing.p_routing_entries[i].length);
				}
				OSA_ASSERT(p_data - p_data_base == payload_total_length);
			}
		break;

		default:
			/* Not Supported */
			OSA_ASSERT(0);
		break;

	}

	if ((status == NCI_STATUS_OK) && (p_buff != NULL))
	{
		/* Prepare NCI header */
		p_data = nci_buff_data(p_buff);
		len = nci_buff_length(p_buff) - NCI_CTRL_HEADER_SIZE;
		NCI_MT_SET(p_data, NCI_MT_COMMAND);
		NCI_PBF_SET(p_data, 0);
		NCI_OPCODE_SET(p_data, (nfc_u16)(opcode));
		NCI_CTRL_LEN_SET(p_data, (nfc_u8)len);
	}
	else
	{
		nci_buff_free (p_buff);
		if (status == NCI_STATUS_OK) status = NCI_STATUS_FAILED; // Amir - should be removed once the check of each parameter will be implemented
	}

	*pp_nci_buff = p_buff;

	return status;
}

#endif //#if (NCI_VERSION_IN_USE==NCI_VER_1)

/* EOF */











