/*
 * nci_ver_1\nci_ntf_parser.c
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

  This source file contains the NCI Notfication Parser Utility

*******************************************************************************/
#include "nci_api.h"

#if (NCI_VERSION_IN_USE==NCI_VER_1)

#include "nfc_types.h"
#include "nci_dev.h"
#include "nfc_os_adapt.h"

#include "nci_int.h"


/*******************************************************************************
	Notification Parser
*******************************************************************************/

/******************************************************************************
*
* Name: nci_parse_ntf
*
* Description: Parse received notification message
*			  Finds out actual notification code, data and length
*			  Put them in output parameters
*
* Input: opcode - NCI command opcode from header
*		p_data - Notification data buffer received
*		p_ntf_id - Notification ID code set to E_NCI_NTF_MAX
*		len -Notification data buffer received length
*
* Output: p_ntf_id - Actual notification ID code
*		  pu_ntf -buffer that holds actual notification message params
*
* Return: Actual notification message size (one of structures in nci_ntf_param_u)
*
*******************************************************************************/
nfc_u32 nci_parse_ntf(nfc_u16 opcode, nfc_u8 *p_data, nci_ntf_e *p_ntf_id, nci_ntf_param_u *pu_ntf, nfc_u32 len)
{
	nfc_u8 i;
	switch(opcode)
	{
		case NCI_OPCODE_CORE_RESET_NTF:
		{
			OSA_ASSERT(len == NCI_CORE_RESET_NTF_SIZE);
			NCI_GET_1B(p_data, pu_ntf->reset.reason_code);
			NCI_GET_1B(p_data, pu_ntf->reset.config_status);
			len = sizeof(pu_ntf->reset);
			*p_ntf_id = E_NCI_NTF_CORE_RESET;
		}
		break;
		case NCI_OPCODE_RF_FIELD_INFO_NTF:
		{
			OSA_ASSERT(len == NCI_CORE_RF_FIELD_INFO_NTF_SIZE);
			NCI_GET_1B(p_data, pu_ntf->rf_field_info.rf_field_status);
			len = sizeof(pu_ntf->rf_field_info);
			*p_ntf_id = E_NCI_NTF_RF_FIELD_INFO;
		}
		break;
		case NCI_OPCODE_CORE_CONN_CREDITS_NTF:
		{
			pu_ntf->conn_credit.num_of_entries = *p_data++;
			OSA_ASSERT((nfc_u8)len == NCI_CORE_CONN_CREDITS_NTF_MIN_SIZE+NCI_CORE_CONN_CREDITS_NTF_CONF_SIZE*pu_ntf->conn_credit.num_of_entries);
			for(i=0; i<pu_ntf->conn_credit.num_of_entries; i++)
			{
				NCI_GET_1B(p_data, pu_ntf->conn_credit.conn_entries[i].conn_id);
				pu_ntf->conn_credit.conn_entries[i].conn_id = NCI_CONNID_GET(&pu_ntf->conn_credit.conn_entries[i].conn_id);
				NCI_GET_1B(p_data, pu_ntf->conn_credit.conn_entries[i].credit);
			}
			len = sizeof(pu_ntf->conn_credit);
			*p_ntf_id = E_NCI_NTF_CORE_CONN_CREDITS;
		}
		break;

		case NCI_OPCODE_RF_DISCOVER_NTF:
		{
			NCI_GET_1B(p_data, pu_ntf->discover.rf_discover_id);
			NCI_GET_1B(p_data, pu_ntf->discover.rf_protocol);
			NCI_GET_1B(p_data, pu_ntf->discover.rf_tech_and_mode);
			NCI_GET_1B(p_data, pu_ntf->discover.length_rf_technology_specific_params);

			switch(pu_ntf->discover.rf_tech_and_mode)
			{
			case NFC_A_PASSIVE_POLL_MODE:
				{
					struct nci_specific_nfca_poll *poll_a = &pu_ntf->discover.rf_technology_specific_params.nfca_poll;

#ifdef NCI_BIG_ENDIAN
					NCI_GET_ARR(p_data, poll_a->sens_res, NCI_RF_ACTIVATE_NTF_A_PASSIVE_POLL_SENS_RES_SIZE);
#else
					NCI_GET_ARR_REV(p_data, poll_a->sens_res, NCI_RF_ACTIVATE_NTF_A_PASSIVE_POLL_SENS_RES_SIZE);
#endif

					NCI_GET_1B(p_data, poll_a->nfcid1_len);

#ifdef NCI_BIG_ENDIAN
					NCI_GET_ARR(p_data, poll_a->nfcid1, poll_a->nfcid1_len);
#else
					NCI_GET_ARR_REV(p_data, poll_a->nfcid1, poll_a->nfcid1_len);
#endif

					NCI_GET_1B(p_data, poll_a->sel_res_len);
					if(poll_a->sel_res_len != 0)
						NCI_GET_1B(p_data, poll_a->sel_res);
				}
				break;
			case NFC_B_PASSIVE_POLL_MODE:
				{
					struct nci_specific_nfcb_poll *poll_b = &pu_ntf->discover.rf_technology_specific_params.nfcb_poll;

					if(p_data, pu_ntf->discover.length_rf_technology_specific_params > 0)
					{
						NCI_GET_1B(p_data, poll_b->sensb_len);

#ifdef NCI_BIG_ENDIAN
						NCI_GET_ARR(p_data, poll_b->sensb_res , poll_b->sensb_len);
#else
						NCI_GET_ARR_REV(p_data, poll_b->sensb_res , poll_b->sensb_len);
#endif
					}
				}
				break;
			case NFC_F_PASSIVE_POLL_MODE:
				{
					struct nci_specific_nfcf_poll *poll_f = &pu_ntf->discover.rf_technology_specific_params.nfcf_poll;

					NCI_GET_1B(p_data, poll_f->bitrate);
					NCI_GET_1B(p_data, poll_f->sensf_len);
					if(poll_f->sensf_len > 0)

#ifdef NCI_BIG_ENDIAN
						NCI_GET_ARR(p_data, poll_f->sensf_res , poll_f->sensf_len);
#else
						NCI_GET_ARR_REV(p_data, poll_f->sensf_res , poll_f->sensf_len);
#endif
				}
				break;
			}

			NCI_GET_1B(p_data, pu_ntf->discover.notification_type);

			*p_ntf_id = E_NCI_NTF_RF_DISCOVER;
			break;
		}

		case NCI_OPCODE_RF_INTF_ACTIVATED_NTF:
		{
			nfc_u8 *p_data_base = p_data;

			NCI_GET_1B(p_data, pu_ntf->activate.rf_discovery_id);
			//If this contains a value of 0 (NFCEE Direct RF Interface) then all following parameters SHALL contain a value of 0 and SHALL be ignored.
			NCI_GET_1B(p_data, pu_ntf->activate.rf_interface_type);
			if(NCI_RF_INTERFACE_NFCEE_DIRECT == pu_ntf->activate.rf_interface_type)
			{
				len = sizeof(pu_ntf->activate);
				*p_ntf_id = E_NCI_NTF_RF_INTF_ACTIVATED;
				break;
			}
			NCI_GET_1B(p_data, pu_ntf->activate.selected_rf_protocol);
			NCI_GET_1B(p_data, pu_ntf->activate.activated_rf_technology_and_mode);
			NCI_GET_1B(p_data, pu_ntf->activate.max_data_packet_payload_size);
			NCI_GET_1B(p_data, pu_ntf->activate.initial_num_of_credits_rf_conn);
			NCI_GET_1B(p_data, pu_ntf->activate.length_rf_technology_specific_params);

			switch(pu_ntf->activate.activated_rf_technology_and_mode)
			{
				case NFC_A_PASSIVE_POLL_MODE:
				{
					struct nci_specific_nfca_poll *poll_a = &pu_ntf->activate.rf_technology_specific_params.nfca_poll;

#ifdef NCI_BIG_ENDIAN
					NCI_GET_ARR(p_data, poll_a->sens_res, NCI_RF_ACTIVATE_NTF_A_PASSIVE_POLL_SENS_RES_SIZE);
#else
					NCI_GET_ARR_REV(p_data, poll_a->sens_res, NCI_RF_ACTIVATE_NTF_A_PASSIVE_POLL_SENS_RES_SIZE);
#endif

					NCI_GET_1B(p_data, poll_a->nfcid1_len);

#ifdef NCI_BIG_ENDIAN
					NCI_GET_ARR(p_data, poll_a->nfcid1, poll_a->nfcid1_len);
#else
					NCI_GET_ARR_REV(p_data, poll_a->nfcid1, poll_a->nfcid1_len);
#endif

					NCI_GET_1B(p_data, poll_a->sel_res_len);
					if(poll_a->sel_res_len != 0)
						NCI_GET_1B(p_data, poll_a->sel_res);

					NCI_GET_1B(p_data, pu_ntf->activate.data_exchange_rf_technology_and_mode);
					NCI_GET_1B(p_data, pu_ntf->activate.data_exchange_transmit_bitrate);
					NCI_GET_1B(p_data, pu_ntf->activate.data_exchange_receive_bitrate);

					NCI_GET_1B(p_data, pu_ntf->activate.activation_params_len);
					if(pu_ntf->activate.rf_interface_type == NCI_RF_INTERFACE_ISO_DEP)
					{
						struct nci_activate_nfca_poll_iso_dep *iso_dep = &pu_ntf->activate.activation_params.nfca_poll_iso_dep;
						NCI_GET_1B(p_data, iso_dep->rats_res_length);

#ifdef NCI_BIG_ENDIAN
						NCI_GET_ARR(p_data, iso_dep->rats_res, iso_dep->rats_res_length);
#else
						/* TODO: Probably a firmware bug - currently we do not reverse the rats */
						//NCI_GET_ARR_REV(p_data, iso_dep->rats_res, iso_dep->rats_res_length);
						NCI_GET_ARR(p_data, iso_dep->rats_res, iso_dep->rats_res_length);
#endif
					}
					else if(pu_ntf->activate.rf_interface_type == NCI_RF_INTERFACE_NFC_DEP)
					{
						struct nci_activate_nfca_poll_nfc_dep *nfc_dep = &pu_ntf->activate.activation_params.nfca_poll_nfc_dep;
						NCI_GET_1B(p_data, nfc_dep->atr_res_length);

#ifdef NCI_BIG_ENDIAN
						NCI_GET_ARR(p_data, nfc_dep->atr_res, nfc_dep->atr_res_length);
#else
						NCI_GET_ARR_REV(p_data, nfc_dep->atr_res, nfc_dep->atr_res_length);
#endif
					}
					else if(pu_ntf->activate.rf_interface_type == NCI_RF_INTERFACE_FRAME)
					{
						if(pu_ntf->activate.activation_params_len > 0)
						{
							OSA_ASSERT(0);
						}
					}
				}
				break;
				case NFC_B_PASSIVE_POLL_MODE:
				{
					struct nci_specific_nfcb_poll *poll_b = &pu_ntf->activate.rf_technology_specific_params.nfcb_poll;

					if(p_data, pu_ntf->activate.length_rf_technology_specific_params > 0)
					{
						NCI_GET_1B(p_data, poll_b->sensb_len);
#ifdef NCI_BIG_ENDIAN
						NCI_GET_ARR(p_data, poll_b->sensb_res , poll_b->sensb_len);
#else
						NCI_GET_ARR_REV(p_data, poll_b->sensb_res , poll_b->sensb_len);
#endif
					}
					NCI_GET_1B(p_data, pu_ntf->activate.data_exchange_rf_technology_and_mode);
					NCI_GET_1B(p_data, pu_ntf->activate.data_exchange_transmit_bitrate);
					NCI_GET_1B(p_data, pu_ntf->activate.data_exchange_receive_bitrate);

					NCI_GET_1B(p_data, pu_ntf->activate.activation_params_len);

					if(pu_ntf->activate.activation_params_len > 0)
					{
						NCI_GET_1B(p_data, poll_b->attrib_resp_len);

#ifdef NCI_BIG_ENDIAN
						NCI_GET_ARR(p_data, poll_b->attrib_resp , poll_b->attrib_resp_len);
#else
						NCI_GET_ARR_REV(p_data, poll_b->attrib_resp , poll_b->attrib_resp_len);
#endif
					}
					else //TBD - remove else after FW bug fixed
						NCI_GET_1B(p_data, poll_b->attrib_resp_len);


				}
				break;
				case NFC_F_PASSIVE_POLL_MODE:
				{
					struct nci_specific_nfcf_poll *poll_f = &pu_ntf->activate.rf_technology_specific_params.nfcf_poll;

					NCI_GET_1B(p_data, poll_f->bitrate);
					NCI_GET_1B(p_data, poll_f->sensf_len);
					if(poll_f->sensf_len > 0)
#ifdef NCI_BIG_ENDIAN
						NCI_GET_ARR(p_data, poll_f->sensf_res , poll_f->sensf_len);
#else
						NCI_GET_ARR_REV(p_data, poll_f->sensf_res , poll_f->sensf_len);
#endif

					NCI_GET_1B(p_data, pu_ntf->activate.data_exchange_rf_technology_and_mode);
					NCI_GET_1B(p_data, pu_ntf->activate.data_exchange_transmit_bitrate);
					NCI_GET_1B(p_data, pu_ntf->activate.data_exchange_receive_bitrate);

					NCI_GET_1B(p_data, pu_ntf->activate.activation_params_len);

					if(pu_ntf->activate.rf_interface_type == NCI_RF_INTERFACE_NFC_DEP)
					{
						if(pu_ntf->activate.activation_params_len > 0)
						{
							struct nci_activate_nfcf_poll_nfc_dep *nfc_dep = &pu_ntf->activate.activation_params.nfcf_poll_nfc_dep;
							NCI_GET_1B(p_data, nfc_dep->atr_res_length);

#ifdef NCI_BIG_ENDIAN
							NCI_GET_ARR(p_data, nfc_dep->atr_res, nfc_dep->atr_res_length);
#else
							NCI_GET_ARR_REV(p_data, nfc_dep->atr_res, nfc_dep->atr_res_length);
#endif
						}
					}
				}
				break;
				case NFC_15693_PASSIVE_POLL_MODE:
				{
					if(pu_ntf->activate.length_rf_technology_specific_params > 0)
					{
						NCI_GET_1B(p_data, pu_ntf->activate.rf_technology_specific_params.nfcv_poll.dsfid);
#ifdef NCI_BIG_ENDIAN
						NCI_GET_ARR(p_data, pu_ntf->activate.rf_technology_specific_params.nfcv_poll.uid, pu_ntf->activate.length_rf_technology_specific_params - 1);
#else
						NCI_GET_ARR_REV(p_data, pu_ntf->activate.rf_technology_specific_params.nfcv_poll.uid, pu_ntf->activate.length_rf_technology_specific_params - 1);
#endif
					}

					NCI_GET_1B(p_data, pu_ntf->activate.data_exchange_rf_technology_and_mode);
					NCI_GET_1B(p_data, pu_ntf->activate.data_exchange_transmit_bitrate);
					NCI_GET_1B(p_data, pu_ntf->activate.data_exchange_receive_bitrate);

					NCI_GET_1B(p_data, pu_ntf->activate.activation_params_len);

					OSA_ASSERT(pu_ntf->activate.activation_params_len == 0);
				}
				break;

				case NFC_A_PASSIVE_LISTEN_MODE:
				{
					NCI_GET_1B(p_data, pu_ntf->activate.data_exchange_rf_technology_and_mode);
					NCI_GET_1B(p_data, pu_ntf->activate.data_exchange_transmit_bitrate);
					NCI_GET_1B(p_data, pu_ntf->activate.data_exchange_receive_bitrate);

					NCI_GET_1B(p_data, pu_ntf->activate.activation_params_len);
					if(pu_ntf->activate.rf_interface_type == NCI_RF_INTERFACE_ISO_DEP)
					{
						struct nci_activate_nfca_listen_iso_dep *iso_dep = &pu_ntf->activate.activation_params.nfca_listen_iso_dep;
						NCI_GET_1B(p_data, iso_dep->rats_param);
					}
					else if(pu_ntf->activate.rf_interface_type == NCI_RF_INTERFACE_NFC_DEP)
					{
						struct nci_activate_nfca_listen_nfc_dep *nfc_dep = &pu_ntf->activate.activation_params.nfca_listen_nfc_dep;
						NCI_GET_1B(p_data, nfc_dep->atr_req_length);

#ifdef NCI_BIG_ENDIAN
						NCI_GET_ARR(p_data, nfc_dep->atr_req, nfc_dep->atr_req_length);
#else
						NCI_GET_ARR_REV(p_data, nfc_dep->atr_req, nfc_dep->atr_req_length);
#endif
					}

				}
				break;
				case NFC_B_PASSIVE_LISTEN_MODE:
				{
					struct nci_specific_nfcb_listen *listen_b = &pu_ntf->activate.rf_technology_specific_params.nfcb_listen;

					if(pu_ntf->activate.length_rf_technology_specific_params > 0)
					{
						OSA_ASSERT(0);
					}

					NCI_GET_1B(p_data, pu_ntf->activate.data_exchange_rf_technology_and_mode);
					NCI_GET_1B(p_data, pu_ntf->activate.data_exchange_transmit_bitrate);
					NCI_GET_1B(p_data, pu_ntf->activate.data_exchange_receive_bitrate);
					NCI_GET_1B(p_data, pu_ntf->activate.activation_params_len);

					if(pu_ntf->activate.activation_params_len > 0)
					{
						NCI_GET_1B(p_data, listen_b->attrib_cmd_len);

#ifdef NCI_BIG_ENDIAN
						NCI_GET_ARR(p_data, listen_b->attrib_cmd , listen_b->attrib_cmd_len);
#else
						NCI_GET_ARR_REV(p_data, listen_b->attrib_cmd , listen_b->attrib_cmd_len);
#endif
					}
					else //TBD - remove else after FW bug fixed
						NCI_GET_1B(p_data, listen_b->attrib_cmd_len);
				}
				break;
				case NFC_F_PASSIVE_LISTEN_MODE:
				{
					struct nci_specific_nfcf_listen *listen_f = &pu_ntf->activate.rf_technology_specific_params.nfcf_listen;

					if(pu_ntf->activate.length_rf_technology_specific_params > 0)
					{
						NCI_GET_1B(p_data, listen_f->local_nfcid2_len);

#ifdef NCI_BIG_ENDIAN
						NCI_GET_ARR(p_data, listen_f->local_nfcid2, listen_f->local_nfcid2_len);
#else
						NCI_GET_ARR_REV(p_data, listen_f->local_nfcid2, listen_f->local_nfcid2_len);
#endif

					}

					NCI_GET_1B(p_data, pu_ntf->activate.data_exchange_rf_technology_and_mode);
					NCI_GET_1B(p_data, pu_ntf->activate.data_exchange_transmit_bitrate);
					NCI_GET_1B(p_data, pu_ntf->activate.data_exchange_receive_bitrate);

					NCI_GET_1B(p_data, pu_ntf->activate.activation_params_len);

					if(pu_ntf->activate.rf_interface_type == NCI_RF_INTERFACE_NFC_DEP)
					{
						struct nci_activate_nfcf_listen_nfc_dep *nfc_dep = &pu_ntf->activate.activation_params.nfcf_listen_nfc_dep;
						NCI_GET_1B(p_data, nfc_dep->atr_req_length);
#ifdef NCI_BIG_ENDIAN
						NCI_GET_ARR(p_data, nfc_dep->atr_req, nfc_dep->atr_req_length);
#else
						NCI_GET_ARR_REV(p_data, nfc_dep->atr_req, nfc_dep->atr_req_length);
#endif
					}
				}
				break;

			}
			OSA_ASSERT(len == (nfc_u32)(p_data - p_data_base));
			len = sizeof(pu_ntf->activate);
			*p_ntf_id = E_NCI_NTF_RF_INTF_ACTIVATED;
		}
		break;
		case NCI_OPCODE_RF_DEACTIVATE_NTF:
		{
			OSA_ASSERT(len == NCI_RF_DEACTIVATE_NTF_SIZE);
			NCI_GET_1B(p_data, pu_ntf->deactivate.type);
			NCI_GET_1B(p_data, pu_ntf->deactivate.reason);
			len = sizeof(pu_ntf->deactivate);
			*p_ntf_id = E_NCI_NTF_RF_DEACTIVATE;
		}
		break;
		case NCI_OPCODE_NFCEE_DISCOVER_NTF:
		{
			NCI_GET_1B(p_data, pu_ntf->nfcee_discover.nfcee_id);
			NCI_GET_1B(p_data, pu_ntf->nfcee_discover.nfcee_status);
			NCI_GET_1B(p_data, pu_ntf->nfcee_discover.num_nfcee_interface_information_entries);
			if (pu_ntf->nfcee_discover.num_nfcee_interface_information_entries > NCI_MAX_NFCEE_PROTOCOLS)
				pu_ntf->nfcee_discover.num_nfcee_interface_information_entries = NCI_MAX_NFCEE_PROTOCOLS;
			for (i=0; i<pu_ntf->nfcee_discover.num_nfcee_interface_information_entries; i++)
				NCI_GET_1B(p_data, pu_ntf->nfcee_discover.nfcee_protocols[i]);
			NCI_GET_1B(p_data, pu_ntf->nfcee_discover.num_nfcee_information_TLVs);
			if(pu_ntf->nfcee_discover.num_nfcee_information_TLVs > 0)
			{
				OSA_ASSERT(0);
			}
			/*TBD - TLVs parsing is currently not required - will be added later if we need it*/
			len = sizeof(pu_ntf->nfcee_discover);
 			*p_ntf_id = E_NCI_NTF_NFCEE_DISCOVER;
	   }
		break;

		case NCI_OPCODE_RF_NFCEE_ACTION_NTF:
		{
			NCI_GET_1B(p_data, pu_ntf->rf_nfcee_action.id);
			NCI_GET_1B(p_data, pu_ntf->rf_nfcee_action.trigger);
			NCI_GET_1B(p_data, pu_ntf->rf_nfcee_action.supporting_data_length);
			if(pu_ntf->rf_nfcee_action.supporting_data_length > 0)

#ifndef NCI_BIG_ENDIAN
				NCI_GET_ARR(p_data, pu_ntf->rf_nfcee_action.supporting_data, pu_ntf->rf_nfcee_action.supporting_data_length);
#else
				NCI_GET_ARR_REV(p_data, pu_ntf->rf_nfcee_action.supporting_data, pu_ntf->rf_nfcee_action.supporting_data_length);
#endif

			len = sizeof(pu_ntf->rf_nfcee_action);
			*p_ntf_id = E_NCI_NTF_RF_NFCEE_ACTION;
		}
		break;

		case NCI_OPCODE_RF_NFCEE_DISCOVERY_REQ_NTF:
		{
			nfc_u8 i = 0;

			NCI_GET_1B(p_data, pu_ntf->rf_nfcee_discovery_req.num_tlvs);

			for(i = 0; i < pu_ntf->rf_nfcee_discovery_req.num_tlvs; i++)
			{
				NCI_GET_1B(p_data, pu_ntf->rf_nfcee_discovery_req.tlvs_arr[i].discovery_request_type);
				NCI_GET_1B(p_data, pu_ntf->rf_nfcee_discovery_req.tlvs_arr[i].length);
				NCI_GET_1B(p_data, pu_ntf->rf_nfcee_discovery_req.tlvs_arr[i].nfceeID);
				NCI_GET_1B(p_data, pu_ntf->rf_nfcee_discovery_req.tlvs_arr[i].rf_tech_and_mode);
				NCI_GET_1B(p_data, pu_ntf->rf_nfcee_discovery_req.tlvs_arr[i].rf_protocol);
			}

			len = sizeof(pu_ntf->rf_nfcee_discovery_req);
			*p_ntf_id = E_NCI_NTF_RF_NFCEE_DISCOVERY_REQ;
		}
		break;

		case NCI_OPCODE_CORE_INTERFACE_ERROR_NTF:
		{
			NCI_GET_1B(p_data, pu_ntf->core_iface_error.status);
			NCI_GET_1B(p_data, pu_ntf->core_iface_error.connID);

			len = sizeof(pu_ntf->core_iface_error);
			*p_ntf_id = E_NCI_NTF_CORE_INTERFACE_ERROR;
		}
		break;

		case NCI_OPCODE_CORE_GENERIC_ERROR_NTF:
		{
			NCI_GET_1B(p_data, pu_ntf->core_generic_error.status);

			len = sizeof(pu_ntf->core_iface_error);
			*p_ntf_id = E_NCI_NTF_CORE_GENERIC_ERROR;
		}
		break;

		default:
			/* Not Supported notification arrived*/
			OSA_ASSERT(0);
	}
	return len;
}

#endif //#if (NCI_VERSION_IN_USE==NCI_VER_1)

/* EOF */











