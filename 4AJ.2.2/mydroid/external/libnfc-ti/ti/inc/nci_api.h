/*
 * nci_api.h
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

#ifndef __NCI_API_H
#define __NCI_API_H

#define NCI_VER_1  (1)
#define NCI_DRAFT_9 (2)

#ifndef NCI_VERSION_IN_USE
#define NCI_VERSION_IN_USE NCI_VER_1
#endif

#if (NCI_VERSION_IN_USE==NCI_DRAFT_9)
#include "nfc_types.h"

#define NCI_MAX_CMD_SIZE					256
#define NCI_MAX_RSP_SIZE					256
#define NCI_MAX_EVT_SIZE					256
#define NCI_MAX_DATA_SIZE					256
#define NCI_CTRL_HEADER_SIZE				3
#define NCI_DATA_HEADER_SIZE				3

#define NCI_MAX_CONNS						16

#define NCI_MAX_NFCEE_PROTOCOLS             	5
#define NCI_MAX_NUM_NFCEE					(16)


#define NCI_MAX_CMD_PAYLOAD_SIZE			(NCI_MAX_CMD_SIZE-NCI_CTRL_HEADER_SIZE)
#define NCI_MAX_RSP_PAYLOAD_SIZE			(NCI_MAX_RSP_SIZE-NCI_CTRL_HEADER_SIZE)
#define NCI_MAX_EVT_PAYLOAD_SIZE			(NCI_MAX_EVT_SIZE-NCI_CTRL_HEADER_SIZE)
#define NCI_MAX_DATA_PAYLOAD_SIZE			(NCI_MAX_DATA_SIZE-NCI_DATA_HEADER_SIZE)

typedef enum
{
	NCI_STATUS_OK							= 0x00,
	NCI_STATUS_REJECTED						= 0x01,
	NCI_STATUS_MESSAGE_CORRUPTED			= 0x02,
	NCI_STATUS_BUFFER_FULL					= 0x03,
	NCI_STATUS_FAILED					= 0x04,
	NCI_STATUS_NOT_INITIALIZED				= 0x05,
	NCI_STATUS_SYNTAX_ERROR					= 0x06,
	NCI_STATUS_SEMANTIC_ERROR				= 0x07,
	NCI_STATUS_UNKNOWN_GID					= 0x08,
	NCI_STATUS_UNKNOWN_OID					= 0x09,
	NCI_STATUS_INVALID_PARAM				= 0x0a,
	NCI_STATUS_MESSAGE_SIZE_EXCEEDED		= 0x0b,

	/* Discovery Specific Status Codes */
	NCI_STATUS_DISCOVERY_ALREADY_STARTED	= 0xa0,

	/* Data Exchange Specific Status Codes */
	NCI_STATUS_RF_TRANSMISSION_ERROR			= 0xb0,
	NCI_STATUS_RF_PROTOCOL_ERROR				= 0xb1,
	NCI_STATUS_RF_TIMEOUT_ERROR				= 0xb2,
	NCI_STATUS_RF_LINK_LOSS_ERROR				= 0xb3,

	/* NFCEE Interface Specific Status Codes */
	NCI_STATUS_MAX_ACTIVE_NFCEE_INTERFACES_REACHED		= 0xc0,
	NCI_STATUS_NFCEE_INTERFACE_ACTIVATION_FAILED		= 0xc1,
	NCI_STATUS_NFCEE_TRANSMISSION_ERROR			= 0xc2,
	NCI_STATUS_NFCEE_PROTOCOL_ERROR				= 0xc3,
	NCI_STATUS_NFCEE_TIMEOUT_ERROR				= 0xc4,

	/* Proprietary Status Codes */
	NCI_STATUS_PROPRIETARY_0				= 0xe1,
	NCI_STATUS_FLUSHED,		/* called when data is flushed due to connection closure */

	NCI_STATUS_MAX
} nci_status_e;

enum
{
	NCI_GID_CORE		= 0x0,
	NCI_GID_RF_MGMT		= 0x1,
	NCI_GID_NFCEE_MGMT	= 0x2,
	NCI_GID_CE		= 0x3,
	NCI_GID_TEST		= 0x4,
	//GIDs 0x5-0xE are currently RFU
	NCI_GID_PROPRIETARY = 0xf
};


#define __OPCODE(gid,oid)		(nfc_u16)(((((nfc_u16)(gid))&0xf)<<8)|(((nfc_u16)(oid))&0x3f))

/********************************************************************************
 * NCI Command Opcodes
 ********************************************************************************/

/*NCI Core*/

#define	NCI_OPCODE_CORE_RESET_CMD						__OPCODE(NCI_GID_CORE		,0x00)
#define	NCI_OPCODE_CORE_RESET_RSP						(NCI_OPCODE_CORE_RESET_CMD)
#define	NCI_OPCODE_CORE_RESET_NTF						(NCI_OPCODE_CORE_RESET_CMD)
#define	NCI_OPCODE_CORE_INIT_CMD						__OPCODE(NCI_GID_CORE		,0x01)
#define	NCI_OPCODE_CORE_INIT_RSP						(NCI_OPCODE_CORE_INIT_CMD)
#define	NCI_OPCODE_CORE_SET_CONFIG_CMD					__OPCODE(NCI_GID_CORE		,0x02)
#define	NCI_OPCODE_CORE_SET_CONFIG_RSP					(NCI_OPCODE_CORE_SET_CONFIG_CMD)
#define	NCI_OPCODE_CORE_GET_CONFIG_CMD					__OPCODE(NCI_GID_CORE		,0x03)
#define	NCI_OPCODE_CORE_GET_CONFIG_RSP					(NCI_OPCODE_CORE_GET_CONFIG_CMD)
#define	NCI_OPCODE_CORE_CONN_CREATE_CMD					__OPCODE(NCI_GID_CORE		,0x04)
#define	NCI_OPCODE_CORE_CONN_CREATE_RSP					(NCI_OPCODE_CORE_CONN_CREATE_CMD)
#define	NCI_OPCODE_CORE_NFCC_CONN_CMD					__OPCODE(NCI_GID_CORE		,0x05)
#define	NCI_OPCODE_CORE_NFCC_CONN_RSP					(NCI_OPCODE_CORE_NFCC_CONN_CMD)
#define	NCI_OPCODE_CORE_NFCC_CONN_NTF					(NCI_OPCODE_CORE_NFCC_CONN_CMD)
#define	NCI_OPCODE_CORE_CONN_CLOSE_CMD					__OPCODE(NCI_GID_CORE		,0x06)
#define	NCI_OPCODE_CORE_CONN_CLOSE_RSP					(NCI_OPCODE_CORE_CONN_CLOSE_CMD)
#define	NCI_OPCODE_CORE_CONN_CLOSE_NTF					(NCI_OPCODE_CORE_CONN_CLOSE_CMD)
#define	NCI_OPCODE_CORE_CONN_CREDITS_NTF				__OPCODE(NCI_GID_CORE		,0x07)
#define	NCI_OPCODE_CORE_RF_FIELD_INFO_NTF				__OPCODE(NCI_GID_CORE		,0x08)
#define	NCI_OPCODE_CORE_GENERIC_ERROR_CMD				__OPCODE(NCI_GID_CORE		,0x09)
#define	NCI_OPCODE_CORE_GENERIC_ERROR_RSP				(NCI_OPCODE_CORE_GENERIC_ERROR_CMD)
#define	NCI_OPCODE_CORE_GENERIC_ERROR_NTF				(NCI_OPCODE_CORE_GENERIC_ERROR_CMD)
#define	NCI_OPCODE_CORE_INTERFACE_ERROR_CMD				__OPCODE(NCI_GID_CORE		,0x0a)
#define	NCI_OPCODE_CORE_INTERFACE_ERROR_RSP				(NCI_OPCODE_CORE_INTERFACE_ERROR_CMD)
#define	NCI_OPCODE_CORE_INTERFACE_ERROR_NTF				(NCI_OPCODE_CORE_INTERFACE_ERROR_CMD)

/* RF Management */

#define	NCI_OPCODE_RF_DISCOVER_MAP_CMD					__OPCODE(NCI_GID_RF_MGMT	,0x00)
#define	NCI_OPCODE_RF_DISCOVER_MAP_RSP					(NCI_OPCODE_RF_DISCOVER_MAP_CMD)
#define	NCI_OPCODE_RF_SET_LISTEN_MODE_ROUTING_CMD		__OPCODE(NCI_GID_RF_MGMT	,0x01)
#define	NCI_OPCODE_RF_SET_LISTEN_MODE_ROUTING_RSP		(NCI_OPCODE_RF_SET_LISTEN_MODE_ROUTING_CMD)
#define	NCI_OPCODE_RF_GET_LISTEN_MODE_ROUTING_CMD		__OPCODE(NCI_GID_RF_MGMT	,0x02)
#define	NCI_OPCODE_RF_GET_LISTEN_MODE_ROUTING_RSP		(NCI_OPCODE_RF_GET_LISTEN_MODE_ROUTING_CMD)
#define	NCI_OPCODE_RF_GET_LISTEN_MODE_ROUTING_NTF		(NCI_OPCODE_RF_GET_LISTEN_MODE_ROUTING_CMD)
#define	NCI_OPCODE_RF_DISCOVER_CMD						__OPCODE(NCI_GID_RF_MGMT	,0x03)
#define	NCI_OPCODE_RF_DISCOVER_RSP						(NCI_OPCODE_RF_DISCOVER_CMD)
#define	NCI_OPCODE_RF_DISCOVER_NTF						(NCI_OPCODE_RF_DISCOVER_CMD)
#define	NCI_OPCODE_RF_DISCOVER_SEL_CMD					__OPCODE(NCI_GID_RF_MGMT	,0x04)
#define	NCI_OPCODE_RF_DISCOVER_SEL_RSP					(NCI_OPCODE_RF_DISCOVER_SEL_CMD)
#define	NCI_OPCODE_RF_ACTIVATE_NTF						__OPCODE(NCI_GID_RF_MGMT	,0x05)
#define	NCI_OPCODE_RF_DEACTIVATE_CMD					__OPCODE(NCI_GID_RF_MGMT	,0x06)
#define	NCI_OPCODE_RF_DEACTIVATE_RSP					(NCI_OPCODE_RF_DEACTIVATE_CMD)
#define	NCI_OPCODE_RF_DEACTIVATE_NTF					(NCI_OPCODE_RF_DEACTIVATE_CMD)
#define	NCI_OPCODE_RF_T3T_POLLING_CMD					__OPCODE(NCI_GID_RF_MGMT	,0x07)
#define	NCI_OPCODE_RF_T3T_POLLING_RSP					(NCI_OPCODE_RF_T3T_POLLING_CMD)
#define	NCI_OPCODE_RF_T3T_POLLING_NTF					(NCI_OPCODE_RF_T3T_POLLING_CMD)

/* NFCEE Management */

#define NCI_OPCODE_NFCEE_DISCOVER_CMD					__OPCODE(NCI_GID_NFCEE_MGMT	,0x00)
#define NCI_OPCODE_NFCEE_DISCOVER_RSP					(NCI_OPCODE_NFCEE_DISCOVER_CMD)
#define NCI_OPCODE_NFCEE_DISCOVER_NTF					(NCI_OPCODE_NFCEE_DISCOVER_CMD)
#define NCI_OPCODE_NFCEE_MODE_SET_CMD					__OPCODE(NCI_GID_NFCEE_MGMT	,0x01)
#define NCI_OPCODE_NFCEE_MODE_SET_RSP					(NCI_OPCODE_NFCEE_MODE_SET_CMD)

/* Card Emulation */
#define NCI_OPCODE_CE_ACTION_NTF						__OPCODE(NCI_GID_CE			,0x00)

/* Testing */

#define NCI_OPCODE_TEST_RF_CONTROL_CMD					__OPCODE(NCI_GID_TEST		,0x00)
#define NCI_OPCODE_TEST_RF_CONTROL_RSP					(NCI_OPCODE_TEST_RF_CONTROL_CMD)
#define NCI_OPCODE_TEST_RF_CONTROL_NTF					(NCI_OPCODE_TEST_RF_CONTROL_CMD)



/********************************************************************************
 * NCI Operations Codes
 ********************************************************************************/
enum
{
	NCI_OPERATION_START	= 0,
	NCI_OPERATION_STOP,
	NCI_OPERATION_RF_ENABLE,
	NCI_OPERATION_RF_DISABLE,
	NCI_OPERATION_NFCEE_SWITCH_MODE,

	NCI_OPERATION_MAX
};


/********************************************************************************
 * NCI (Configuration) TLV Types
 ********************************************************************************/

/*Common Discovery Parameters*/
#define	NCI_CFG_TYPE_TOTAL_DURATION						(0x00)
#define	NCI_CFG_TYPE_CON_DEVICES_LIMIT					(0x01)

/*Poll mode - NFC-A Discovery Parameters*/
#define	NCI_CFG_TYPE_PA_BAIL_OUT						(0x08)

/*Poll mode - NFC-B Discovery Parameters*/
#define	NCI_CFG_TYPE_PB_AFI								(0x10)
#define	NCI_CFG_TYPE_PB_BAIL_OUT						(0x12)


/*Poll mode - NFC-F Discovery Parameters*/
#define	NCI_CFG_TYPE_PF_BIT_RATE						(0x18)

/*Poll mode - ISO-DEP Discovery Parameters*/
#define	NCI_CFG_TYPE_PB_H_INFO							(0x20)

/*Poll mode - NFC-DEP Discovery Parameters*/
#define	NCI_CFG_TYPE_BITR_NFC_DEP						(0x28)
#define	NCI_CFG_TYPE_ATR_REQ_GEN_BYTES					(0x29)
#define	NCI_CFG_TYPE_ATR_REQ_CONFIG						(0x2A)

/*Listen mode - NFC-A Discovery Parameters*/
#define	NCI_CFG_TYPE_LA_PROTOCOL_TYPE					(0x30)
#define	NCI_CFG_TYPE_LA_NFCID1							(0x31)


/*Listen mode - NFC-B Discovery Parameters*/
#define	NCI_CFG_TYPE_LB_NFCID0							(0x38)
#define	NCI_CFG_TYPE_LB_APPLICATION_DATA				(0x39)
#define	NCI_CFG_TYPE_LB_SFGI							(0x3A)
#define	NCI_CFG_TYPE_LB_ADC_FO							(0x3B)


/*Listen mode - NFC-F Discovery Parameters*/
#define	NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_1				(0x40)
#define	NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_2				(0x41)
#define	NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_3				(0x42)
#define	NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_4				(0x43)
#define	NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_5				(0x44)
#define	NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_6				(0x45)
#define	NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_7				(0x46)
#define	NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_8				(0x47)
#define	NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_9				(0x48)
#define	NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_10				(0x49)
#define	NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_11				(0x4A)
#define	NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_12				(0x4B)
#define	NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_13				(0x4C)
#define	NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_14				(0x4D)
#define	NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_15				(0x4E)
#define	NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_16				(0x4F)
#define	NCI_CFG_TYPE_LF_PROTOCOL_TYPE1					(0x50)
#define	NCI_CFG_TYPE_LF_T3T_PMM							(0x51)
#define	NCI_CFG_TYPE_LF_T3T_MAX							(0x52)
#define	NCI_CFG_TYPE_LF_T3T_FLAGS2						(0x53)
#define	NCI_CFG_TYPE_LF_CON_BITR_F						(0x54)

/*Listen mode - ISO-DEP Discovery Parameters*/
#define	NCI_CFG_TYPE_FWI								(0x58)
#define	NCI_CFG_TYPE_LA_HIST_BY							(0x59)
#define	NCI_CFG_TYPE_LB_PROTOCOL_TYPE					(0x5A)
#define	NCI_CFG_TYPE_LB_H_INFO_RESP						(0x5B)

/*Listen mode - NFC-DEP Discovery Parameters*/
#define	NCI_CFG_TYPE_WT									(0x60)
#define	NCI_CFG_TYPE_ATR_RES_GEN_BYTES					(0x61)
#define	NCI_CFG_TYPE_ATR_RES_CONFIG						(0x62)

/*Other Parameters*/
#define	NCI_CFG_TYPE_RF_FIELD_INFO						(0x80)
#define	NCI_CFG_TYPE_NFCDEP_OP							(0x81)




/********************************************************************************
 * NCI Common Values Definition
 ********************************************************************************/
/*** NCI_RF_PROTOCOL ***/
#define NCI_RF_PROTOCOL_UNKNOWN			0x00
#define NCI_RF_PROTOCOL_T1T				0x01
#define NCI_RF_PROTOCOL_T2T				0x02
#define NCI_RF_PROTOCOL_T3T				0x03
#define NCI_RF_PROTOCOL_ISO_DEP			0x04
#define NCI_RF_PROTOCOL_NFC_DEP			0x05
#define NCI_RF_PROTOCOL_NFC_RFU			0x06 /* In addition RFU: 0x06 - 0x7F, 0xff */
#define RF_PROTOCOL_PROP_BASE			0x80 /* 0x80-0xFE	Reserved for proprietary protocols */

/*** NCI_NFCEE_PROTOCOL ***/
#define NCI_NFCEE_PROTOCOL_RFU			0x00  /* In addition RFU: 0x04 - 0x7F, 0xff */
#define NCI_NFCEE_PROTOCOL_APDU			0x01
#define NCI_NFCEE_PROTOCOL_HCI_ACCESS	0x02
#define NCI_NFCEE_PROTOCOL_TAG3_CMD_SET 0x03
#define NCI_NFCEE_PROTOCOL_TRANSPARENT  0x04
#define NCI_NFCEE_PROTOCOL_PROP_BASE	0x80 /* 0x80-0xFE	Reserved for proprietary protocols */

/*** NCI_RF_INTERFACE ***/
#define NCI_RF_INTERFACE_RFU			0x00 /* In addition RFU: 0x04 - 0x7F, 0xff */
#define	NCI_RF_INTERFACE_FRAME			0x01
#define	NCI_RF_INTERFACE_ISO_DEP		0x02
#define	NCI_RF_INTERFACE_NFC_DEP		0x03
#define NCI_RF_INTERFACE_PROP_BASE		0x80 /* 0x80-0xFE	Reserved for proprietary protocols */


/********************************************************************************
 * NCI Message Payload Definition
 ********************************************************************************/

struct nci_rsp_gen
{
	nfc_u8 status;
};

/*** NCI_OPCODE_CORE_RESET_CMD ***/

/* no command parameters*/
#define NCI_VERSION_1_0		0x10

struct nci_rsp_reset
{
	nfc_u8 status;
	nfc_u8 version;
};

/*** NCI_OPCODE_CORE_RESET_NTF ***/
#define NCI_RESET_RC_UNKNOWN	0
#define NCI_RESET_RC_MEM_FULL	1
struct nci_ntf_reset
{
	nfc_u8 reason_code;
};

struct nci_ntf_generic_error
{
	nfc_u8 status;
};



#define NCI_FEATURE_DISC_FREQ_CONFIGURATION	0x01000000
#define NCI_FEATURE_DATA_FLOW_CTRL			0x02000000
#define NCI_FEATURE_CARD_EMULATION			0x00010000
#define NCI_FEATURE_LISTEN_MODE_ROUTING		0x00020000
#define NCI_FEATURE_NFCEE_DISC				0x00040000
#define NCI_FEATURE_BATTERY_OFF				0x00000001

#define NCI_RF_INTERFACE_BITMASK_FRAME		0x0100
#define NCI_RF_INTERFACE_BITMASK_ISO_DEP	0x0200
#define NCI_RF_INTERFACE_BITMASK_NFC_DEP	0x0400

#define NCI_NFCEE_INTERFACE_BITMASK_APDU	0x0100

struct nci_rsp_init
{
	nfc_u8	status;
	union
	{
		nfc_u8	array[4];
		nfc_u32 bit_mask;
	} nci_features;

	nfc_u8 num_supported_rf_interfaces;
	nfc_u8 supported_rf_interfaces[8];

	nfc_u8	max_logical_connections;
	nfc_u16	max_routing_table_size;
	nfc_u8	max_control_packet_payload_length;
	nfc_u16	rf_sending_buffer_size;
	nfc_u16	rf_receiving_buffer_size;

	union
	{
		nfc_u8	array[2];
		nfc_u16 bit_mask;
	} manufacturer_id;
};


/*** NCI_OPCODE_CORE_CONN_CREATE_CMD ***/
struct nci_cmd_conn_create
{
	nfc_u8 target_handle;
	nfc_u8 num_of_tlvs;
	struct nci_tlv	*p_tlv;			/* this will point to TLV Array */
	nfc_u8 total_length;
};

struct nci_rsp_conn_create
{
	nfc_u8 status;
	nfc_u8 maximum_packet_payload_size;
	nfc_u8 initial_num_of_credits;
	nfc_u8 conn_id;
};

/*** NCI_OPCODE_CORE_CONN_CLOSE_CMD ***/
struct nci_cmd_conn_close
{
	nfc_u8 conn_id;
};

/*** NCI_OPCODE_CORE_GET_CONFIG_CMD ***/
/*** NCI_OPCODE_CORE_SET_CONFIG_CMD ***/
#define NCI_MAX_TLV_IN_CMD 10
#define NCI_MAX_TLV_VALUE_LENGTH_IN_CMD 64 /* Bytes. Currently for testing only, there are values longer than this  */

#define NCI_CFG_TYPE_LA_PROTOCOL_TYPE_ISO_DEP	0x01
#define NCI_CFG_TYPE_LA_PROTOCOL_TYPE_NFC_DEP	0x02

#define NCI_CFG_TYPE_LB_PROTOCOL_TYPE_ISO_DEP	0x01

#define NCI_CFG_TYPE_LF_PROTOCOL_TYPE_NFC_DEP	0x02

struct nci_tlv
{
	nfc_u8	type;
	nfc_u8	length;
	nfc_u8	*p_value;	/* Pass pointer to config value (NCI core will build the structure) */
};

struct nci_rsp_tlv_core_get_config
{
	nfc_u8	type;
	nfc_u8	length;
	nfc_u8	value[NCI_MAX_TLV_VALUE_LENGTH_IN_CMD]; /* So we do not have to malloc/free*/
};

struct nci_cmd_core_set_config
{
	nfc_u8		num_of_tlvs;
	struct nci_tlv	*p_tlv;			/* this will point to TLV Array */
};

struct nci_cmd_core_get_config
{
	nfc_u8		num_of_params;
	nfc_u8		param_type[NCI_MAX_TLV_IN_CMD];	/* this will point to array with parameter types */
};


#define NCI_TLV_HEADER_LEN	2 /* type (1B) + length (1B) */

struct nci_rsp_core_set_config
{
	nfc_u8			status;
	nfc_u8			num_of_params;/* this will be 0 if status is not equal NCI_STATUS_INVALID_PARAM */
	nfc_u8			param_id[NCI_MAX_TLV_IN_CMD];  /* this will be  */
};

struct nci_rsp_core_get_config
{
	nfc_u8						status;
	nfc_u8						num_of_params;
	struct nci_rsp_tlv_core_get_config	params[NCI_MAX_TLV_IN_CMD];  /* this will be parameters' TLV */
};

/* Note: for Set and Get command use the dedicated nci_set_param and nci_get_param function calls */


/*** NCI_OPCODE_RF_DISCOVER_CMD ***/
#define NCI_DISCOVERY_TYPE_POLL_A_PASSIVE		0
#define	NCI_DISCOVERY_TYPE_POLL_B_PASSIVE		1
#define	NCI_DISCOVERY_TYPE_POLL_F_PASSIVE		2
#define	NCI_DISCOVERY_TYPE_POLL_A_ACTIVE		3

#define	NCI_DISCOVERY_TYPE_POLL_F_ACTIVE		5
#define	NCI_DISCOVERY_TYPE_WAKEUP_A_PASSIVE		6
#define	NCI_DISCOVERY_TYPE_WAKEUP_B_PASSIVE		7

#define	NCI_DISCOVERY_TYPE_WAKEUP_A_ACTIVE		9

/* 0x70 - 0x7F	Reserved for Proprietary Technologies in Poll Mode */
#define NCI_DISCOVERY_TYPE_POLL_15693			0x70

#define	NCI_DISCOVERY_TYPE_LISTEN_A_PASSIVE		0x80
#define	NCI_DISCOVERY_TYPE_LISTEN_B_PASSIVE		0x81
#define	NCI_DISCOVERY_TYPE_LISTEN_F_PASSIVE		0x82
#define	NCI_DISCOVERY_TYPE_LISTEN_A_ACTIVE		0x83

#define	NCI_DISCOVERY_TYPE_LISTEN_F_ACTIVE		0x85
/* 0xF0 - 0xFF	Reserved for Proprietary Technologies in Listen Mode.*/

struct disc_conf
{
	nfc_u8	type;
	nfc_u8	frequency;
};

struct nci_cmd_discover
{
	nfc_u8				num_of_conf;					// number of configurations
	struct disc_conf	disc_conf[6];					// maximum 6 configurations
};

/*** NCI_OPCODE_RF_DISCOVER_MAP_CMD ***/
#define NCI_DISC_MAP_MODE_POLL				0x01
#define NCI_DISC_MAP_MODE_LISTEN			0x02
#define NCI_DISC_MAP_MODE_BOTH				0x03

struct disc_map_conf
{
	nfc_u8	rf_protocol;				/* See NCI_RF_PROTOCOL */
	nfc_u8	mode;
	nfc_u8	rf_interface_type;			/* See NCI_RF_INTERFACE */
};

struct nci_cmd_discover_map
{
	nfc_u8					num_of_conf;				// number of configurations
	struct disc_map_conf	disc_map_conf[20];			// maximum 20 configurations
};


/* nci_rsp_gen_t */

/*** NCI_OPCODE_CORE_RF_FIELD_INFO_NTF ***/
struct nci_ntf_rf_field_info
{
	nfc_u8	rf_field_status;
};

/*** NCI_OPCODE_CORE_CONN_CREDITS_NTF ***/
struct conn_credit
{
	nfc_u8	conn_id;
	nfc_u8	credit;
};
struct nci_ntf_core_conn_credit
{
	nfc_u8				num_of_entries;
	struct conn_credit	conn_entries[NCI_MAX_CONNS];
} ;


/*** NCI_OPCODE_RF_IF_ACTIVATE_NTF ***/

struct nci_activate_nfca_poll_iso_dep
{
	nfc_u8	rats_res_length;
	nfc_u8	rats_res[20];		// (Byte 2 - Byte 6+k-1)
};

struct nci_activate_nfca_poll_nfc_dep
{
	nfc_u8	atr_res_length;
	nfc_u8	atr_res[50];		// (Byte 3 - Byte 17+n)
};

struct nci_activate_nfca_listen_iso_dep
{
	nfc_u8	rats_param;
};

struct nci_activate_nfca_listen_nfc_dep
{
	nfc_u8	atr_req_length;
	nfc_u8	atr_req[50];		// (Byte 3 - Byte 16+n)
};

struct nci_activate_nfcf_poll_nfc_dep
{
	nfc_u8	atr_res_length;
	nfc_u8	atr_res[50];		// (Byte 3 - Byte 17+n)
};

struct nci_activate_nfcf_listen_nfc_dep
{
	nfc_u8	atr_req_length;
	nfc_u8	atr_req[50];		// (Byte 3 - Byte 16+n)
};




/* following structure will be appended to nci_ntf_activate_t right after the relevant
	rf_technology_specific_params member */

struct nci_activate_nfca_poll
{
	nfc_u8	sens_res[2];
	nfc_u8	nfcid1_len;
	nfc_u8	nfcid1[10];
	nfc_u8	sel_res_len;
	nfc_u8	sel_res;
};


struct nci_activate_nfcb_poll
{
	nfc_u8	sensb_len; /* length of sensb_res can be 12 or 13 bytes */
	nfc_u8	sensb_res[13];

	nfc_u8	attrib_resp_len; /* ATTRIB Response Length */
	nfc_u8	attrib_resp[16];
};


struct nci_activate_nfcf_poll
{
	nfc_u8  bitrate;	  /* 1 - 212 kbps, 2 - 424 kbps, 0 and 3 to 255 - RFU */
	nfc_u8	sensf_len;	  /* Allowed values SHALL be 16 and 18. Other values are RFU */
	nfc_u8	sensf_res[18];/* Byte 2 – Byte 17 or 19  */
};

struct nci_activate_nfcf_listen
{
	nfc_u8  local_nfcid2_len; /* If NFCID2 is available, then Local NFCID2 Length SHALL be 8. If NFCID2 is not available, then Local NFCID2 Length SHALL be 0, and Local NFCID2 field is not present*/
	nfc_u8  local_nfcid2[8];
};

struct nci_activate_nfcb_listen
{
	nfc_u8  attrib_cmd_len;
	nfc_u8  attrib_cmd[50]; //Byte 2 – Byte 10+k  of ATTRIB Command as defined in  [DIGITAL]
};



/* selected_rf_protocol values */
/* See NCI_RF_PROTOCOL */

/* detected_rf_technology_and_mode values */
#define NFC_A_PASSIVE_POLL_MODE		0x00
#define	NFC_B_PASSIVE_POLL_MODE		0x01
#define	NFC_F_PASSIVE_POLL_MODE		0x02
#define	NFC_A_ACTIVE_POLL_MODE 		0x03
#define	NFC_F_ACTIVE_POLL_MODE		0x05
/* 0x70 - 0x7F	Reserved for Proprietary Technologies in Poll Mode */
#define	NFC_A_PASSIVE_LISTEN_MODE	0x80
#define	NFC_B_PASSIVE_LISTEN_MODE	0x81
#define	NFC_F_PASSIVE_LISTEN_MODE	0x82
/*0xF0 - 0xFF	Reserved for Proprietary Technologies in Listen Mode */


/* rf_interface_type values */
/* See NCI_RF_INTERFACE_... */

struct nci_ntf_activate
{
	nfc_u8	target_handle; /* TODO - what to send here? */
	nfc_u8	selected_rf_protocol;
	nfc_u8	detected_rf_technology_and_mode;
	nfc_u8	length_rf_technology_specific_params;
	union
	{
		struct nci_activate_nfca_poll				nfca_poll;
		struct nci_activate_nfcb_poll				nfcb_poll;
		struct nci_activate_nfcb_listen				nfcb_listen;
		struct nci_activate_nfcf_poll				nfcf_poll;
		struct nci_activate_nfcf_listen				nfcf_listen;
	} rf_technology_specific_params;

	nfc_u8	rf_interface_type;
	nfc_u8  activation_params_len;
	union
	{
		struct nci_activate_nfca_poll_iso_dep		nfca_poll_iso_dep;
		struct nci_activate_nfca_poll_nfc_dep		nfca_poll_nfc_dep;
		struct nci_activate_nfca_listen_iso_dep		nfca_listen_iso_dep;
		struct nci_activate_nfca_listen_nfc_dep		nfca_listen_nfc_dep;
		struct nci_activate_nfcf_poll_nfc_dep		nfcf_poll_nfc_dep;
		struct nci_activate_nfcf_listen_nfc_dep		nfcf_listen_nfc_dep;
		// TBD - add B
	} activation_params;
};



/*** NCI_OPCODE_RF_IF_DEACTIVATE_NTF ***/
#define	NCI_DEACTIVATE_TYPE_IDLE_MODE		0
#define	NCI_DEACTIVATE_TYPE_SLEEP_MODE		1
#define	NCI_DEACTIVATE_TYPE_SLEEP_AF_MODE	2
#define	NCI_DEACTIVATE_TYPE_RF_LINK_LOSS	3
#define	NCI_DEACTIVATE_TYPE_DISCOVERY_ERROR	4
struct nci_cmd_deactivate
{
	nfc_u8	type;
};

struct nci_ntf_deactivate
{
	nfc_u8	type;
};


/*** NCI_OPCODE_NFCEE_DISCOVER_CMD ***/
#define	NCI_NFCEE_DISCOVER_ACTION_DISABLE	    0
#define	NCI_NFCEE_DISCOVER_ACTION_ENABLE   		1
struct nci_cmd_nfcee_discover
{
	nfc_u8	action;
};

struct nci_rsp_nfcee_discover
{
	nfc_u8 status;
	nfc_u8 num_nfcee;   /* 0 - 255 Indicates the number of NFCEEs connected to the NFCEE */
};

/* detected_rf_technology_and_mode values */
#define NFCEE_STATUS_CONNECTED_AND_ACTIVE		0x00
#define	NFCEE_STATUS_CONNECTED_AND_INACTIVE		0x01
#define	NFCEE_STATUS_REMOVED            		0x02

struct nci_ntf_nfcee_discover
{
	nfc_u8 target_handle;
	nfc_u8 nfcee_status;
	nfc_u8 num_nfcee_interface_information_entries;  /* Number of supported NFCEE Protocols */
	nfc_u8 nfcee_protocols[NCI_MAX_NFCEE_PROTOCOLS];
	nfc_u8 num_nfcee_information_TLVs;
};


/*** NCI_OPCODE_NFCEE_MODE_SET_CMD ***/
#define	NCI_NFCEE_MODE_ACTIVATE   		        0x00
#define	NCI_NFCEE_MODE_DEACTIVATE	            0x01
struct nci_cmd_nfcee_mode_set
{
	nfc_u8 target_handle;
	nfc_u8 nfcee_mode;
};


/* Union for NCI Command Parameters  */
typedef union
{
	struct nci_cmd_core_set_config	core_set_config;
	struct nci_cmd_conn_create		conn_create;
	struct nci_cmd_conn_close		conn_close;
	struct nci_cmd_discover			discover;
	struct nci_cmd_discover_map		discover_map;
	struct nci_cmd_deactivate		deactivate;
	struct nci_cmd_nfcee_discover		nfcee_discover;
	struct nci_cmd_nfcee_mode_set	nfcee_mode_set;
	struct nci_cmd_core_get_config	core_get_config;
} nci_cmd_param_u;


/* Union for NCI Response Parameters  */
typedef union
{
	struct nci_rsp_gen			        	generic;
	struct nci_rsp_reset		        		reset;
	struct nci_rsp_init			        	init;
	struct nci_rsp_conn_create	        	conn_create;
	struct nci_rsp_core_set_config		core_set_config;
	struct nci_rsp_nfcee_discover	    	nfcee_discover;
	struct nci_rsp_core_get_config		core_get_config;
} nci_rsp_param_u;

/* Union for NCI Notifications Parameters  */
typedef union
{
	struct nci_ntf_reset				reset;
	struct nci_ntf_activate				activate;
	struct nci_ntf_deactivate			deactivate;
	struct nci_ntf_rf_field_info		rf_field_info;
	struct nci_ntf_core_conn_credit		conn_credit;
	struct nci_ntf_nfcee_discover		nfcee_discover;
	struct nci_ntf_generic_error		generic_error;
} nci_ntf_param_u;

/********************************************************************************
 * NCI Operation Payload Definition
 ********************************************************************************/
struct nci_op_rsp_gen
{
	nfc_u8 status;
};
/*** NCIX_OPCODE_START ***/

struct nci_op_start
{
	nfc_u32 rsvd;
};
struct nci_op_stop
{
	nfc_u32 rsvd;
};
struct nci_op_rsp_start
{
	nfc_u8							status; /* operation completion status (if !=NFC_RES_OK -> check
												command specific status) */
	struct nci_rsp_reset			reset;
	struct nci_rsp_init				init;
	struct nci_rsp_conn_create		conn_create;
	struct nci_rsp_nfcee_discover	nfcee_discover_rsp;
	nfc_u8							nfcee_discover_ntf_number;
  	struct nci_ntf_nfcee_discover	nfcee_discover_ntf[NCI_MAX_NUM_NFCEE];
};

/*** NCIX_OPCODE_DETECT ***/

/*******************************************************************************
   Protocols
*******************************************************************************/
/* TBD: change names to "NCI" defines */
#define NAL_PROTOCOL_READER_ISO_14443_4_A    0x0001  /* Reader ISO 14443 A level 4 */
#define NAL_PROTOCOL_READER_ISO_14443_4_B    0x0002  /* Reader ISO 14443 B level 4 */
#define NAL_PROTOCOL_READER_ISO_14443_3_A    0x0004  /* Reader ISO 14443 A level 3 */
#define NAL_PROTOCOL_READER_ISO_14443_3_B    0x0008  /* Reader ISO 14443 B level 3 */
#define NAL_PROTOCOL_READER_ISO_15693_3      0x0010  /* Reader ISO 15693 level 3 */
#define NAL_PROTOCOL_READER_ISO_15693_2      0x0020  /* Reader ISO 15693 level 2 */
#define NAL_PROTOCOL_READER_FELICA           0x0040  /* Reader Felica */
#define NAL_PROTOCOL_READER_P2P_INITIATOR    0x0080  /* Reader P2P Initiator */
#define NAL_PROTOCOL_READER_TYPE_1_CHIP      0x0100  /* Reader Type 1 */
#define NAL_PROTOCOL_READER_MIFARE_CLASSIC   0x0200  /* Reader Mifare Classic */
#define NAL_PROTOCOL_READER_BPRIME           0x0400  /* Reader B Prime */

#define NAL_PROTOCOL_CARD_ISO_14443_4_A      0x0001  /* Card ISO 14443 A level 4 */
#define NAL_PROTOCOL_CARD_ISO_14443_4_B      0x0002  /* Card ISO 14443 B level 4 */
#define NAL_PROTOCOL_CARD_ISO_14443_3_A      0x0004  /* Card ISO 14443 A level 3 */
#define NAL_PROTOCOL_CARD_ISO_14443_3_B      0x0008  /* Card ISO 14443 B level 3 */
#define NAL_PROTOCOL_CARD_ISO_15693_3        0x0010  /* Card ISO 15693 level 3 */
#define NAL_PROTOCOL_CARD_ISO_15693_2        0x0020  /* Card ISO 15693 level 2 */
#define NAL_PROTOCOL_CARD_FELICA             0x0040  /* Card Felica */
#define NAL_PROTOCOL_CARD_P2P_TARGET         0x0080  /* Card P2P Target */
#define NAL_PROTOCOL_CARD_TYPE_1_CHIP        0x0100  /* Card Type 1 */
#define NAL_PROTOCOL_CARD_MIFARE_CLASSIC     0x0200  /* Card Mifare Classic */
#define NAL_PROTOCOL_CARD_BPRIME             0x0400  /* Card B Prime */

struct nci_op_rf_enable
{
	nfc_u16	ce_bit_mask;
	nfc_u16	rw_bit_mask;
};
/*** NCIX_OPCODE_CANCEL_DETECTION ***/
struct nci_op_rf_disable
{
	nfc_u32 rsvd;
};

enum nfcee_switch_mode
{
	E_SWITCH_INVALID = 0x0a,
	E_SWITCH_RF_COMM,
	E_SWITCH_HOST_COMM,
	E_SWITCH_OFF
};

struct nci_op_nfcee
{
	nfc_u8					target_hndl;
	enum nfcee_switch_mode	new_mode;
};

/* Union for NCI Command Parameters  */
typedef union
{
	struct nci_op_start			start;		/* NCIX_OPCODE_START,				ncix_start_rsp */
	struct nci_op_stop			stop;		/* NCIX_OPCODE_STOP,				ncix_gen_rsp */
	struct nci_op_rf_enable		rf_enable;	/* NCIX_OPCODE_DETECT,				ncix_gen_rsp */
	struct nci_op_rf_disable	rf_disable;	/* NCIX_OPCODE_CANCEL_DETECTION,	ncix_gen_rsp */
	struct nci_op_nfcee			nfcee;		/* */
} op_param_u;




struct nci_op_rsp_nfcee
{
	nfc_u8							status; /* operation completion status (if !=NFC_RES_OK -> check
												command specific status) */
	nfc_u8							target_hndl;
	struct nci_rsp_conn_create		host_comm_params;

	enum nfcee_switch_mode			new_mode;
};

/* Union for NCI Response Parameters  */
typedef union
{
	struct  nci_op_rsp_gen		generic;
	struct  nci_op_rsp_start	start;
	struct  nci_op_rsp_nfcee	nfcee;

} op_rsp_param_u;


/*********************************************************************************/
/* NCI Global Routines                                                           */
/*********************************************************************************/

typedef void (*data_cb_f)(nfc_handle_t, nfc_u8 conn_id, nfc_u8 *p_nci_payload, nfc_u32 nci_payload_len, nci_status_e status);
typedef void (*ntf_cb_f)(nfc_handle_t, nfc_u16, nci_ntf_param_u*);
typedef void (*rsp_cb_f)(nfc_handle_t, nfc_u16, nci_rsp_param_u*);
typedef void (*op_rsp_cb_f)(nfc_handle_t h_cb, nfc_u16 opcode, op_rsp_param_u *pu_resp);


/**

 * Create NCI module - this will involve only SW init (context allocation, callbacks registration)
 * and registration of callback for incoming data. Returns handle to created NCI object
 *
 * @h_caller:		caller handle (to be used with data rx and tx callbacks)
 * @h_osa:			os context
 * @f_rx_ind_cb:	rx indication callback (for received data packets)
 * @f_tx_done_cb:	to be called when packet transferring (to NFCC) is completed (when transport send function is done)
 *

 **/

nfc_handle_t nci_create(nfc_handle_t h_caller,
					nfc_handle_t h_osa,
					data_cb_f f_rx_ind_cb,
					data_cb_f f_tx_done_cb);

/**

 * Destroy NCI module and free all its resources
 *
 * @h_nci:			handle to NCi object to destoy
 *

 **/
void nci_destroy(nfc_handle_t h_nci);

/**

 * Transport enabler - Connect to transport driver and open a data stream
 *
 * @h_nci:			handle to NCi object to open
 * @dev_id:			device id to be sent to transport driver
 *

 **/
nci_status_e nci_open(nfc_handle_t h_nci, nfc_u32 dev_id);

/**

 * Close Transport channel
 *
 * @h_nci:			handle to NCi object to close
 * @dev_id:			device id to be sent to transport driver
 *

 **/
nci_status_e nci_close(nfc_handle_t h_nci, nfc_u32 dev_id);


/**

 * Send Raw NCI data packet
 *
 * @h_nci:			handle to NCi object
 * @conn_id:		NCI connection id to send data
 * @p_nci_payload:	data packet (payload). Payload is being copied within this routine and can be freed by caller upon return.
 * @nci_payload_len:payload length.
 *

 **/
nci_status_e nci_send_data(nfc_handle_t h_nci, nfc_u8 conn_id, nfc_u8 *p_nci_payload, nfc_u32 nci_payload_len);

/**

 * Send single NCI command and register response handler
 *
 * @h_nci:			handle to NCi object
 * @opcode:			Command identifier
 * @pu_param:		A union representing NCI command payload (length is known by the opcode)
 *					pu_param is being copied inside this callback and can be freed by caller upon return.
 * @pu_resp:		Optional buffer for NCI response payload (NULL if response is ignored).
 * @rsp_len:		Maximum length of expected response (Assert will be invoked if response exceeds this).
 * @f_rsp_cb:		Optional callback function to be invoked upon response reception (or NULL).
 * @h_rsp_cb:		Optional parameter that is passed to f_rsp_cb, if invoked (caller object handle)
 *

 **/
nci_status_e nci_send_cmd(	nfc_handle_t h_nci,
							nfc_u16 opcode,
							nci_cmd_param_u *pu_param,
							nci_rsp_param_u *pu_resp,
							nfc_u8 rsp_len,
							rsp_cb_f f_rsp_cb,
							nfc_handle_t h_rsp_cb);

typedef enum
{
	E_NCI_NTF_CORE_RESET,
	E_NCI_NTF_CORE_CONN_ACCEPT,
	E_NCI_NTF_CORE_CONN_CLOSE,
	E_NCI_NTF_CORE_CONN_CREDITS,
	E_NCI_NTF_CORE_RF_FIELD_INFO,
	E_NCI_NTF_CORE_GENERIC_ERROR,
	E_NCI_NTF_CORE_INTERFACE_ERROR,
	E_NCI_NTF_RF_GET_LISTEN_MODE_ROUTING,
	E_NCI_NTF_RF_DISCOVER,
	E_NCI_NTF_RF_ACTIVATE,
	E_NCI_NTF_RF_DEACTIVATE,
	E_NCI_NTF_NFCEE_DISCOVER,
	E_NCI_NTF_CE_ACTION,
	E_NCI_NTF_TEST_RF_CONTROL,
	E_NCI_NTF_ALL,

	E_NCI_NTF_MAX
} nci_ntf_e;

/**

 * Register a callback to specific notification
 *
 * @h_nci:			handle to NCi object
 * @ntf_id:			notification id from nci_ntf_e enum
 * @tlv_num:		number of TLVs in the array
 * @f_rsp_cb:		callback function to be invoked upon notification reception
 * @h_rsp_cb:		Optional parameter that is passed to f_rsp_cb, if invoked (caller object handle)
 *

 **/

void nci_register_ntf_cb(nfc_handle_t h_nci,
						 nci_ntf_e ntf_id,
						 ntf_cb_f f_ntf_cb,
						 nfc_handle_t h_ntf_cb);


/**

 * Un-Register a callback to specific notification
 *
 * @h_nci:			handle to NCi object
 * @ntf_id:			notification id from nci_ntf_e enum
 * @tlv_num:		number of TLVs in the array
 * @f_rsp_cb:		callback function to unregister from ntf_id list
 *

 **/
void nci_unregister_ntf_cb(nfc_handle_t h_nci, nci_ntf_e ntf_id, ntf_cb_f f_ntf_cb);


/**

 * Start NCI operation (operation - a sequence of NCi commands and responses that can't be interrupted)
 *
 * @h_nci:			handle to NCi object
 * @opcode:			operation identifier
 * @pu_param:		A union representing NCI operation parametrs (an input to the operation)
 * @pu_resp:		Optional buffer for NCI operation response (NULL if response is ignored).
 * @rsp_len:		Maximum length of expected response (Assert will be invoked if response exceeds this).
 * @f_rsp_cb:		Optional callback function to be invoked upon operation completion (or NULL).
 * @h_rsp_cb:		Optional parameter that is passed to f_rsp_cb, if invoked (caller object handle)
 *

 **/
nci_status_e nci_start_operation(nfc_handle_t h_nci,
								 nfc_u16 opcode,
								 op_param_u *pu_param,
								 op_rsp_param_u *pu_resp,
								 nfc_u8 rsp_len,
								 op_rsp_cb_f f_rsp_cb,
								 nfc_handle_t h_rsp_cb);


#else //NCI_VERSION_IN_USE

#include "nfc_types.h"

#define NCI_MAX_CMD_SIZE					256
#define NCI_MAX_RSP_SIZE					256
#define NCI_MAX_EVT_SIZE					256
#define NCI_MAX_DATA_SIZE					256
#define NCI_CTRL_HEADER_SIZE				3
#define NCI_DATA_HEADER_SIZE				3

#define NCI_MAX_CONNS						16

#define NCI_MAX_NFCEE_PROTOCOLS             5
#define NCI_MAX_NUM_NFCEE					(16)
#define NCI_INVALID_NFCEE_ID				(0xff)
#define NCI_DH_NFCEE_ID						(0x00)


#define NCI_MAX_CMD_PAYLOAD_SIZE			(NCI_MAX_CMD_SIZE-NCI_CTRL_HEADER_SIZE)
#define NCI_MAX_RSP_PAYLOAD_SIZE			(NCI_MAX_RSP_SIZE-NCI_CTRL_HEADER_SIZE)
#define NCI_MAX_EVT_PAYLOAD_SIZE			(NCI_MAX_EVT_SIZE-NCI_CTRL_HEADER_SIZE)
#define NCI_MAX_DATA_PAYLOAD_SIZE			(NCI_MAX_DATA_SIZE-NCI_DATA_HEADER_SIZE)

typedef enum
{
	NCI_STATUS_OK							= 0x00,
	NCI_STATUS_REJECTED						= 0x01,
	NCI_STATUS_RF_FRAME_CORRUPTED			= 0x02,
	NCI_STATUS_FAILED						= 0x03,
	NCI_STATUS_NOT_INITIALIZED				= 0x04,
	NCI_STATUS_SYNTAX_ERROR					= 0x05,
	NCI_STATUS_SEMANTIC_ERROR				= 0x06,
	NCI_STATUS_UNKNOWN_GID					= 0x07,
	NCI_STATUS_UNKNOWN_OID					= 0x08,
	NCI_STATUS_INVALID_PARAM				= 0x09,
	NCI_STATUS_MESSAGE_SIZE_EXCEEDED		= 0x0a,

	/* Discovery Specific Status Codes */
	NCI_STATUS_DISCOVERY_ALREADY_STARTED			= 0xa0,
	NCI_STATUS_DISCOVERY_TARGET_ACTIVATION_FAILED	= 0xa1,

	/* Data Exchange Specific Status Codes */
	NCI_STATUS_RF_TRANSMISSION_ERROR			= 0xb0,
	NCI_STATUS_RF_PROTOCOL_ERROR				= 0xb1,
	NCI_STATUS_RF_TIMEOUT_ERROR				= 0xb2,
	NCI_STATUS_RF_LINK_LOSS_ERROR				= 0xb3,

	/* NFCEE Interface Specific Status Codes */
	NCI_STATUS_NFCEE_INTERFACE_ACTIVATION_FAILED	= 0xc0,
	NCI_STATUS_NFCEE_TRANSMISSION_ERROR				= 0xc1,
	NCI_STATUS_NFCEE_PROTOCOL_ERROR					= 0xc2,
	NCI_STATUS_NFCEE_TIMEOUT_ERROR					= 0xc3,

	/* Proprietary Status Codes */
	NCI_STATUS_PROPRIETARY_0				= 0xe1,
	NCI_STATUS_FLUSHED,		/* called when data is flushed due to connection closure */
	NCI_STATUS_MESSAGE_CORRUPTED,
	NCI_STATUS_MEM_FULL,
	NCI_STATUS_RM_SM_NOT_ALLOWED,

	NCI_STATUS_MAX
} nci_status_e;

enum
{
	NCI_GID_CORE		= 0x0,
	NCI_GID_RF_MGMT		= 0x1,
	NCI_GID_NFCEE_MGMT	= 0x2,
	NCI_GID_CE		= 0x3,
	NCI_GID_TEST		= 0x4,
	//GIDs 0x5-0xE are currently RFU
	NCI_GID_PROPRIETARY = 0xf
};


#define __OPCODE(gid,oid)		(nfc_u16)(((((nfc_u16)(gid))&0xf)<<8)|(((nfc_u16)(oid))&0x3f))

/********************************************************************************
 * NCI Command Opcodes
 ********************************************************************************/

/*NCI Core*/

#define	NCI_OPCODE_CORE_RESET_CMD						__OPCODE(NCI_GID_CORE		,0x00)
#define	NCI_OPCODE_CORE_RESET_RSP						(NCI_OPCODE_CORE_RESET_CMD)
#define	NCI_OPCODE_CORE_RESET_NTF						(NCI_OPCODE_CORE_RESET_CMD)
#define	NCI_OPCODE_CORE_INIT_CMD						__OPCODE(NCI_GID_CORE		,0x01)
#define	NCI_OPCODE_CORE_INIT_RSP						(NCI_OPCODE_CORE_INIT_CMD)
#define	NCI_OPCODE_CORE_SET_CONFIG_CMD					__OPCODE(NCI_GID_CORE		,0x02)
#define	NCI_OPCODE_CORE_SET_CONFIG_RSP					(NCI_OPCODE_CORE_SET_CONFIG_CMD)
#define	NCI_OPCODE_CORE_GET_CONFIG_CMD					__OPCODE(NCI_GID_CORE		,0x03)
#define	NCI_OPCODE_CORE_GET_CONFIG_RSP					(NCI_OPCODE_CORE_GET_CONFIG_CMD)
#define	NCI_OPCODE_CORE_CONN_CREATE_CMD					__OPCODE(NCI_GID_CORE		,0x04)
#define	NCI_OPCODE_CORE_CONN_CREATE_RSP						(NCI_OPCODE_CORE_CONN_CREATE_CMD)
#define	NCI_OPCODE_CORE_CONN_CLOSE_CMD					__OPCODE(NCI_GID_CORE		,0x05)
#define	NCI_OPCODE_CORE_CONN_CLOSE_RSP					(NCI_OPCODE_CORE_CONN_CLOSE_CMD)
#define	NCI_OPCODE_CORE_CONN_CREDITS_NTF				__OPCODE(NCI_GID_CORE		,0x06)
#define	NCI_OPCODE_CORE_GENERIC_ERROR_NTF				__OPCODE(NCI_GID_CORE		,0x07)
#define	NCI_OPCODE_CORE_INTERFACE_ERROR_NTF				__OPCODE(NCI_GID_CORE		,0x08)

/* RF Management */

#define	NCI_OPCODE_RF_DISCOVER_MAP_CMD					__OPCODE(NCI_GID_RF_MGMT	,0x00)
#define	NCI_OPCODE_RF_DISCOVER_MAP_RSP					(NCI_OPCODE_RF_DISCOVER_MAP_CMD)
#define	NCI_OPCODE_RF_SET_LISTEN_MODE_ROUTING_CMD		__OPCODE(NCI_GID_RF_MGMT	,0x01)
#define	NCI_OPCODE_RF_SET_LISTEN_MODE_ROUTING_RSP		(NCI_OPCODE_RF_SET_LISTEN_MODE_ROUTING_CMD)
#define	NCI_OPCODE_RF_GET_LISTEN_MODE_ROUTING_CMD		__OPCODE(NCI_GID_RF_MGMT	,0x02)
#define	NCI_OPCODE_RF_GET_LISTEN_MODE_ROUTING_RSP		(NCI_OPCODE_RF_GET_LISTEN_MODE_ROUTING_CMD)
#define	NCI_OPCODE_RF_GET_LISTEN_MODE_ROUTING_NTF		(NCI_OPCODE_RF_GET_LISTEN_MODE_ROUTING_CMD)
#define	NCI_OPCODE_RF_DISCOVER_CMD						__OPCODE(NCI_GID_RF_MGMT	,0x03)
#define	NCI_OPCODE_RF_DISCOVER_RSP						(NCI_OPCODE_RF_DISCOVER_CMD)
#define	NCI_OPCODE_RF_DISCOVER_NTF						(NCI_OPCODE_RF_DISCOVER_CMD)
#define	NCI_OPCODE_RF_DISCOVER_SEL_CMD					__OPCODE(NCI_GID_RF_MGMT	,0x04)
#define	NCI_OPCODE_RF_DISCOVER_SEL_RSP					(NCI_OPCODE_RF_DISCOVER_SEL_CMD)
#define	NCI_OPCODE_RF_INTF_ACTIVATED_NTF				__OPCODE(NCI_GID_RF_MGMT	,0x05)
#define	NCI_OPCODE_RF_DEACTIVATE_CMD					__OPCODE(NCI_GID_RF_MGMT	,0x06)
#define	NCI_OPCODE_RF_DEACTIVATE_RSP					(NCI_OPCODE_RF_DEACTIVATE_CMD)
#define	NCI_OPCODE_RF_DEACTIVATE_NTF					(NCI_OPCODE_RF_DEACTIVATE_CMD)
#define	NCI_OPCODE_RF_FIELD_INFO_NTF					__OPCODE(NCI_GID_RF_MGMT		,0x07)
#define	NCI_OPCODE_RF_T3T_POLLING_CMD					__OPCODE(NCI_GID_RF_MGMT	,0x08)
#define	NCI_OPCODE_RF_T3T_POLLING_RSP					(NCI_OPCODE_RF_T3T_POLLING_CMD)
#define	NCI_OPCODE_RF_T3T_POLLING_NTF					(NCI_OPCODE_RF_T3T_POLLING_CMD)
#define	NCI_OPCODE_RF_NFCEE_ACTION_NTF					__OPCODE(NCI_GID_RF_MGMT	,0x09)
#define	NCI_OPCODE_RF_NFCEE_DISCOVERY_REQ_NTF			__OPCODE(NCI_GID_RF_MGMT	,0x0a)
#define	NCI_OPCODE_RF_PARAMETER_UPDATE_CMD				__OPCODE(NCI_GID_RF_MGMT	,0x0b)
#define	NCI_OPCODE_RF_PARAMETER_UPDATE_RSP				(NCI_OPCODE_RF_PARAMETER_UPDATE_CMD)

/* NFCEE Management */

#define NCI_OPCODE_NFCEE_DISCOVER_CMD					__OPCODE(NCI_GID_NFCEE_MGMT	,0x00)
#define NCI_OPCODE_NFCEE_DISCOVER_RSP					(NCI_OPCODE_NFCEE_DISCOVER_CMD)
#define NCI_OPCODE_NFCEE_DISCOVER_NTF					(NCI_OPCODE_NFCEE_DISCOVER_CMD)
#define NCI_OPCODE_NFCEE_MODE_SET_CMD					__OPCODE(NCI_GID_NFCEE_MGMT	,0x01)
#define NCI_OPCODE_NFCEE_MODE_SET_RSP					(NCI_OPCODE_NFCEE_MODE_SET_CMD)

/* Card Emulation - Removed*/
//#define NCI_OPCODE_CE_ACTION_NTF						__OPCODE(NCI_GID_CE			,0x00)

/* Testing - Removed*/


/********************************************************************************
 * NCI Operations Codes
 ********************************************************************************/
enum
{
	NCI_OPERATION_START	= 0,
	NCI_OPERATION_STOP,
	NCI_OPERATION_RF_ENABLE,
	NCI_OPERATION_RF_DISABLE,
	NCI_OPERATION_NFCEE_SWITCH_MODE,
	NCI_OPERATION_RF_CONFIG,
	NCI_OPERATION_RF_CONFIG_LISTEN_MODE_ROUTING,
	NCI_OPERATION_SCRIPT,

	NCI_OPERATION_MAX
};


/********************************************************************************
 * NCI (Configuration) TLV Types
 ********************************************************************************/

/*Common Discovery Parameters*/
#define	NCI_CFG_TYPE_TOTAL_DURATION						(0x00)
#define	NCI_CFG_TYPE_CON_DEVICES_LIMIT					(0x01)

/*Poll mode - NFC-A Discovery Parameters*/
#define	NCI_CFG_TYPE_PA_BAIL_OUT						(0x08)

/*Poll mode - NFC-B Discovery Parameters*/
#define	NCI_CFG_TYPE_PB_AFI								(0x10)
#define	NCI_CFG_TYPE_PB_BAIL_OUT						(0x11)
#define	NCI_CFG_TYPE_PB_ATTRIB_PARAM					(0x12) /*Read only - can't be set by host*/


/*Poll mode - NFC-F Discovery Parameters*/
#define	NCI_CFG_TYPE_PF_BIT_RATE						(0x18)

/*Poll mode - ISO-DEP Discovery Parameters*/
#define	NCI_CFG_TYPE_PB_H_INFO							(0x20)
#define	NCI_CFG_TYPE_PI_BIT_RATE						(0x21)

/*Poll mode - NFC-DEP Discovery Parameters*/
#define	NCI_CFG_TYPE_PN_NFC_DEP_SPEED					(0x28)
#define	NCI_CFG_TYPE_PN_ATR_REQ_GEN_BYTES				(0x29)
#define	NCI_CFG_TYPE_PN_ATR_REQ_CONFIG					(0x2A)

/*Listen mode - NFC-A Discovery Parameters*/
#define	NCI_CFG_TYPE_LA_BIT_FRAME_SDD					(0x30)
#define	NCI_CFG_TYPE_LA_PLATFORM_CONFIG					(0x31)
#define	NCI_CFG_TYPE_LA_SEL_INFO						(0x32)
#define	NCI_CFG_TYPE_LA_NFCID1							(0x33)


/*Listen mode - NFC-B Discovery Parameters*/
#define	NCI_CFG_TYPE_LB_SENSB_INFO						(0x38)
#define	NCI_CFG_TYPE_LB_NFCID0							(0x39)
#define	NCI_CFG_TYPE_LB_APPLICATION_DATA				(0x3a)
#define	NCI_CFG_TYPE_LB_SFGI							(0x3b)
#define	NCI_CFG_TYPE_LB_ADC_FO							(0x3c)


/*Listen mode - NFC-F Discovery Parameters*/
#define	NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_1				(0x40)
#define	NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_2				(0x41)
#define	NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_3				(0x42)
#define	NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_4				(0x43)
#define	NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_5				(0x44)
#define	NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_6				(0x45)
#define	NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_7				(0x46)
#define	NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_8				(0x47)
#define	NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_9				(0x48)
#define	NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_10				(0x49)
#define	NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_11				(0x4A)
#define	NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_12				(0x4B)
#define	NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_13				(0x4C)
#define	NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_14				(0x4D)
#define	NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_15				(0x4E)
#define	NCI_CFG_TYPE_LF_T3T_IDENTIFIERS_16				(0x4F)
#define	NCI_CFG_TYPE_LF_PROTOCOL_TYPE					(0x50)
#define	NCI_CFG_TYPE_LF_T3T_PMM							(0x51)
#define	NCI_CFG_TYPE_LF_T3T_MAX							(0x52)
#define	NCI_CFG_TYPE_LF_T3T_FLAGS2						(0x53)
#define	NCI_CFG_TYPE_LF_CON_BITR_F						(0x54)

/*Listen mode - ISO-DEP Discovery Parameters*/
#define	NCI_CFG_TYPE_LI_FWI								(0x58)
#define	NCI_CFG_TYPE_LA_HIST_BY							(0x59)
#define	NCI_CFG_TYPE_LB_H_INFO_RESP						(0x5A)
#define	NCI_CFG_TYPE_LI_BIT_RATE						(0x5B)

/*Listen mode - NFC-DEP Discovery Parameters*/
#define	NCI_CFG_TYPE_LN_WT								(0x60)
#define	NCI_CFG_TYPE_LN_ATR_RES_GEN_BYTES				(0x61)
#define	NCI_CFG_TYPE_LN_ATR_RES_CONFIG						(0x62)

/*Other Parameters*/
#define	NCI_CFG_TYPE_RF_FIELD_INFO						(0x80)
#define	NCI_CFG_TYPE_RF_NFCEE_ACTION					(0x81)
#define	NCI_CFG_TYPE_NFCDEP_OP							(0x82)




/********************************************************************************
 * NCI Common Values Definition
 ********************************************************************************/
/*** NCI_RF_PROTOCOL ***/
#define NCI_RF_PROTOCOL_UNKNOWN			0x00
#define NCI_RF_PROTOCOL_T1T				0x01
#define NCI_RF_PROTOCOL_T2T				0x02
#define NCI_RF_PROTOCOL_T3T				0x03
#define NCI_RF_PROTOCOL_ISO_DEP			0x04
#define NCI_RF_PROTOCOL_NFC_DEP			0x05
#define NCI_RF_PROTOCOL_NFC_RFU			0x06 /* In addition RFU: 0x06 - 0x7F, 0xff */
#define RF_PROTOCOL_PROP_BASE			0x80 /* 0x80-0xFE	Reserved for proprietary protocols */
#define NCI_RF_PROTOCOL_ISO_15693		(RF_PROTOCOL_PROP_BASE+0)

#define MAX_SUPPORTED_PROTOCOLS_NUM (NCI_RF_PROTOCOL_NFC_DEP-NCI_RF_PROTOCOL_UNKNOWN+1)

/*** NCI_NFCEE_PROTOCOL ***/
#define NCI_NFCEE_PROTOCOL_APDU			0x00
#define NCI_NFCEE_PROTOCOL_HCI_ACCESS	0x01
#define NCI_NFCEE_PROTOCOL_TAG3_CMD_SET 0x02
#define NCI_NFCEE_PROTOCOL_TRANSPARENT  0x03
#define NCI_NFCEE_PROTOCOL_PROP_BASE	0x80 /*RFU: 0x04 - 0x7F, 0xff, 0x80-0xFE	 Reserved for proprietary protocols */

/*** NCI_RF_INTERFACE ***/
#define NCI_RF_INTERFACE_NFCEE_DIRECT	0x00 /* In addition RFU: 0x04 - 0x7F, 0xff */
#define	NCI_RF_INTERFACE_FRAME			0x01
#define	NCI_RF_INTERFACE_ISO_DEP		0x02
#define	NCI_RF_INTERFACE_NFC_DEP		0x03
#define NCI_RF_INTERFACE_PROP_BASE		0x80 /* 0x80-0xFE	Reserved for proprietary protocols */


/********************************************************************************
 * NCI Message Payload Definition
 ********************************************************************************/

struct nci_rsp_gen
{
	nfc_u8 status;
};

/*** NCI_OPCODE_CORE_RESET_CMD ***/

/* no command parameters*/
#define NCI_VERSION_1_0		0x10

struct nci_rsp_reset
{
	nfc_u8 status;
	nfc_u8 version;
	nfc_u8 config_status;
};

/*** NCI_OPCODE_CORE_RESET_NTF ***/
#define NCI_RESET_RC_UNKNOWN	0
#define NCI_RESET_RC_MEM_FULL	1
struct nci_ntf_reset
{
	nfc_u8 reason_code;
	nfc_u8 config_status;
};

#define NCI_NFCEE_ACTION_MAX_SUPPORT_DATA_LEN (256)

struct nci_ntf_rf_nfcee_action
{
	nfc_u8 id;
	nfc_u8 trigger;
	nfc_u8 supporting_data_length;
	nfc_u8 supporting_data[NCI_NFCEE_ACTION_MAX_SUPPORT_DATA_LEN];
};

//The reson for value 6 is that FW supports up to 3 personalities per UICC/SE, so totaly we can receive 1SE*3+1UICC*3=6 tlvs
#define NCI_NFCEE_DISCOVERY_REQUEST_MAX_NUM_TLVS (6)
#define NCI_NFCEE_DISCOVERY_REQUEST_TYPE_ADD (0x0)
#define NCI_NFCEE_DISCOVERY_REQUEST_TYPE_REMOVE (0x1)

struct nfcee_discovery_req_tlv
{
	nfc_u8 discovery_request_type;
	nfc_u8 length;
	nfc_u8 nfceeID;
	nfc_u8 rf_tech_and_mode;
	nfc_u8 rf_protocol;
};

struct nci_ntf_rf_nfcee_discovery_req
{
	nfc_u8 num_tlvs;
	struct nfcee_discovery_req_tlv tlvs_arr[NCI_NFCEE_DISCOVERY_REQUEST_MAX_NUM_TLVS];

};

struct nci_ntf_core_intf_error
{
	nfc_u8 status;
	nfc_u8 connID;
};

struct nci_ntf_core_generic_error
{
	nfc_u8 status;
};



#define NCI_FEATURE_DISC_FREQ_CONFIGURATION	0x01000000
#define NCI_FEATURE_DATA_FLOW_CTRL			0x02000000
#define NCI_FEATURE_CARD_EMULATION			0x00010000
#define NCI_FEATURE_NFCEE_DISC				0x00040000

#define NCI_RF_INTERFACE_BITMASK_FRAME		0x0100
#define NCI_RF_INTERFACE_BITMASK_ISO_DEP	0x0200
#define NCI_RF_INTERFACE_BITMASK_NFC_DEP	0x0400

#define NCI_NFCEE_INTERFACE_BITMASK_APDU	0x0100

#define NCI_FEATURE_LISTEN_MODE_ROUTING_TECH		0x00000200 /* Technology based Routing supported */
#define NCI_FEATURE_LISTEN_MODE_ROUTING_PROTOCOL	0x00000400 /* Protocol based routing supported */
#define NCI_FEATURE_LISTEN_MODE_ROUTING_AID			0x00000800 /* AID based routing supported */

#define NCI_FEATURE_BAT_OFF				0x00010000
#define NCI_FEATURE_SWITCHED_OFF		0x00020000 /* Bat. low*/

struct nci_rsp_init
{
	nfc_u8	status;
	union
	{
		nfc_u8	array[4];
		nfc_u32 bit_mask;
	} nfcc_features;

	nfc_u8 num_supported_rf_interfaces;
	nfc_u8 supported_rf_interfaces[8];

	nfc_u8	max_logical_connections;
	nfc_u16	max_routing_table_size;
	nfc_u8	max_control_packet_payload_length;
	nfc_u16	max_size_for_large_params;
	nfc_u8 initial_num_of_credits_rf_conn;
	nfc_u8 manufacturer_id;
	nfc_u8 manufacturer_specific_info[4];
};

#define NCI_DESTINATION_TYPE_NFCC_LOOPBACK (0x01)
#define NCI_DESTINATION_TYPE_REMOTE_EP (0x02)
#define NCI_DESTINATION_TYPE_NFCEE (0x03)
/*** NCI_OPCODE_CORE_CONN_CREATE_CMD ***/
struct nci_cmd_conn_create
{
	nfc_u8 destination_type;
	nfc_u8 num_of_tlvs;
	struct nci_tlv	*p_tlv;			/* this will point to TLV Array */
};

struct nci_rsp_conn_create
{
	nfc_u8 status;
	nfc_u8 maximum_packet_payload_size;
	nfc_u8 initial_num_of_credits;
	nfc_u8 conn_id;
};

/*** NCI_OPCODE_CORE_CONN_CLOSE_CMD ***/
struct nci_cmd_conn_close
{
	nfc_u8 conn_id;
};

/*** NCI_OPCODE_CORE_GET_CONFIG_CMD ***/
/*** NCI_OPCODE_CORE_SET_CONFIG_CMD ***/
#define NCI_MAX_TLV_IN_CMD 10
#define NCI_MAX_TLV_VALUE_LENGTH_IN_CMD 64 /* Bytes. Currently for testing only, there are values longer than this  */

#define NCI_CFG_TYPE_LA_PROTOCOL_TYPE_ISO_DEP	0x20
#define NCI_CFG_TYPE_LA_PROTOCOL_TYPE_NFC_DEP	0x40

#define NCI_CFG_TYPE_LB_PROTOCOL_TYPE_ISO_DEP	0x01

#define NCI_CFG_TYPE_LF_PROTOCOL_TYPE_NFC_DEP	0x02

struct nci_tlv
{
	nfc_u8	type;
	nfc_u8	length;
	nfc_u8	*p_value;	/* Pass pointer to config value (NCI core will build the structure) */
};

struct nci_rsp_tlv_core_get_config
{
	nfc_u8	type;
	nfc_u8	length;
	nfc_u8	value[NCI_MAX_TLV_VALUE_LENGTH_IN_CMD]; /* So we do not have to malloc/free*/
};

struct nci_cmd_core_reset
{
	nfc_u8		keep_config;
};

struct nci_cmd_core_set_config
{
	nfc_u8		num_of_tlvs;
	struct nci_tlv	*p_tlv;			/* this will point to TLV Array */
};

struct nci_cmd_core_get_config
{
	nfc_u8		num_of_params;
	nfc_u8		param_type[NCI_MAX_TLV_IN_CMD];	/* this will point to array with parameter types */
};


#define NCI_TLV_HEADER_LEN	2 /* type (1B) + length (1B) */

struct nci_rsp_core_set_config
{
	nfc_u8			status;
	nfc_u8			num_of_params;/* this will be 0 if status is not equal NCI_STATUS_INVALID_PARAM */
	nfc_u8			param_id[NCI_MAX_TLV_IN_CMD];  /* this will be  */
};

struct nci_rsp_core_get_config
{
	nfc_u8						status;
	nfc_u8						num_of_params;
	struct nci_rsp_tlv_core_get_config	params[NCI_MAX_TLV_IN_CMD];  /* this will be parameters' TLV */
};

/* Note: for Set and Get command use the dedicated nci_set_param and nci_get_param function calls */

struct disc_conf
{
	nfc_u8	type;
	nfc_u8	frequency;
};

struct nci_cmd_discover
{
	nfc_u8				num_of_conf;					// number of configurations
	struct disc_conf	disc_conf[6];					// maximum 6 configurations
};

struct nci_cmd_rf_discover_select
{
	nfc_u8		rf_discovery_id;
	nfc_u8		rf_protocol;
	nfc_u8		rf_interface;
};

/*** NCI_OPCODE_RF_DISCOVER_MAP_CMD ***/
#define NCI_DISC_MAP_MODE_POLL				0x01
#define NCI_DISC_MAP_MODE_LISTEN			0x02
#define NCI_DISC_MAP_MODE_BOTH				0x03

struct disc_map_conf
{
	nfc_u8	rf_protocol;				/* See NCI_RF_PROTOCOL */
	nfc_u8	mode;
	nfc_u8	rf_interface_type;			/* See NCI_RF_INTERFACE */
};

struct nci_cmd_discover_map
{
	nfc_u8					num_of_conf;				// number of configurations
	struct disc_map_conf	disc_map_conf[20];			// maximum 20 configurations
};


/* nci_rsp_gen_t */

/*** NCI_OPCODE_CORE_RF_FIELD_INFO_NTF ***/
struct nci_ntf_rf_field_info
{
	nfc_u8	rf_field_status;
};

/*** NCI_OPCODE_CORE_CONN_CREDITS_NTF ***/
struct conn_credit
{
	nfc_u8	conn_id;
	nfc_u8	credit;
};
struct nci_ntf_core_conn_credit
{
	nfc_u8				num_of_entries;
	struct conn_credit	conn_entries[NCI_MAX_CONNS];
} ;


/*** NCI_OPCODE_RF_IF_ACTIVATE_NTF ***/

struct nci_activate_nfca_poll_iso_dep
{
	nfc_u8	rats_res_length;
	nfc_u8	rats_res[20];		// (Byte 2 - Byte 6+k-1)
};

struct nci_activate_nfca_poll_nfc_dep
{
	nfc_u8	atr_res_length;
	nfc_u8	atr_res[50];		// (Byte 3 - Byte 17+n)
};

struct nci_activate_nfca_listen_iso_dep
{
	nfc_u8	rats_param;
};

struct nci_activate_nfca_listen_nfc_dep
{
	nfc_u8	atr_req_length;
	nfc_u8	atr_req[50];		// (Byte 3 - Byte 16+n)
};

struct nci_activate_nfcf_poll_nfc_dep
{
	nfc_u8	atr_res_length;
	nfc_u8	atr_res[50];		// (Byte 3 - Byte 17+n)
};

struct nci_activate_nfcf_listen_nfc_dep
{
	nfc_u8	atr_req_length;
	nfc_u8	atr_req[50];		// (Byte 3 - Byte 16+n)
};




/* following structure will be appended to nci_ntf_activate_t right after the relevant
	rf_technology_specific_params member */

struct nci_specific_nfca_poll
{
	nfc_u8	sens_res[2];
	nfc_u8	nfcid1_len;
	nfc_u8	nfcid1[10];
	nfc_u8	sel_res_len;
	nfc_u8	sel_res;
};


struct nci_specific_nfcb_poll
{
	nfc_u8	sensb_len; /* length of sensb_res can be 12 or 13 bytes */
	nfc_u8	sensb_res[13];

	nfc_u8	attrib_resp_len; /* ATTRIB Response Length */
	nfc_u8	attrib_resp[16];
};


struct nci_specific_nfcf_poll
{
	nfc_u8  bitrate;	  /* 1 - 212 kbps, 2 - 424 kbps, 0 and 3 to 255 - RFU */
	nfc_u8	sensf_len;	  /* Allowed values SHALL be 16 and 18. Other values are RFU */
	nfc_u8	sensf_res[18];/* Byte 2 – Byte 17 or 19  */
};

struct nci_specific_nfcf_listen
{
	nfc_u8  local_nfcid2_len; /* If NFCID2 is available, then Local NFCID2 Length SHALL be 8. If NFCID2 is not available, then Local NFCID2 Length SHALL be 0, and Local NFCID2 field is not present*/
	nfc_u8  local_nfcid2[8];
};

struct nci_specific_nfcb_listen
{
	nfc_u8  attrib_cmd_len;
	nfc_u8  attrib_cmd[50]; //Byte 2 – Byte 10+k  of ATTRIB Command as defined in  [DIGITAL]
};

struct nci_specific_nfcv_poll
{
	nfc_u8 dsfid;
	nfc_u8 uid[8];
};


/* selected_rf_protocol values */
/* See NCI_RF_PROTOCOL */

/* detected_rf_technology_and_mode values */
#define NFC_A_PASSIVE_POLL_MODE		0x00
#define	NFC_B_PASSIVE_POLL_MODE		0x01
#define	NFC_F_PASSIVE_POLL_MODE		0x02
#define	NFC_A_ACTIVE_POLL_MODE 		0x03
/*RFU - 0x04*/
#define	NFC_F_ACTIVE_POLL_MODE		0x05
/* 0x70 - 0x7F	Reserved for Proprietary Technologies in Poll Mode */
#define NFC_15693_PASSIVE_POLL_MODE 0x06

#define	NFC_A_PASSIVE_LISTEN_MODE	0x80
#define	NFC_B_PASSIVE_LISTEN_MODE	0x81
#define	NFC_F_PASSIVE_LISTEN_MODE	0x82
#define	NFC_A_ACTIVE_LISTEN_MODE	0x83
/*RFU - 0x84*/
#define	NFC_F_ACTIVE_LISTEN_MODE	0x85
#define NFC_15693_PASSIVE_LISTEN_MODE 0x86
/*0xF0 - 0xFF	Reserved for Proprietary Technologies in Listen Mode */
#define NCI_INVALID_TECH_AND_MODE	(0xff)

#define NUM_OF_TECH_AND_MODE_LISTEN_TECHS ((NFC_15693_PASSIVE_LISTEN_MODE - NFC_A_PASSIVE_LISTEN_MODE)+1)


/* rf_interface_type values */
/* See NCI_RF_INTERFACE_... */

union rf_technology_specific_params_t
{
	struct nci_specific_nfca_poll				nfca_poll;
	struct nci_specific_nfcb_poll				nfcb_poll;
	struct nci_specific_nfcb_listen				nfcb_listen;
	struct nci_specific_nfcf_poll				nfcf_poll;
	struct nci_specific_nfcf_listen				nfcf_listen;
	struct nci_specific_nfcv_poll				nfcv_poll;
};

struct nci_ntf_discover
{
	nfc_u8	rf_discover_id;
	nfc_u8	rf_protocol;
	nfc_u8	rf_tech_and_mode;
	nfc_u8	length_rf_technology_specific_params;
	union rf_technology_specific_params_t rf_technology_specific_params;
	nfc_u8	notification_type; //0 - Last Notification, 1 - Last Notification (due to NFCC reaching it’s resource limit), 2 - More Notification to follow
};

/*

Bit Rate value	Definition
0x00			NFC_BIT_RATE_106: 106 Kbit/s
0x01			NFC_BIT_RATE_212: 212 Kbit/s
0x02			NFC_BIT_RATE_424: 424 Kbit/s
0x03			NFC_BIT_RATE_848: 848 Kbit/s
0x04			NFC_BIT_RATE_1695: 1695 Kbit/s
0x05			NFC_BIT_RATE_3390: 3390 Kbit/s
0x06			NFC_BIT_RATE_6780: 6780 Kbit/s
0x07 – 0x7F	RFU
0x80-0xFE	For proprietary use
0xFF	RFU

*/
struct nci_ntf_activate
{
	nfc_u8	rf_discovery_id; /* TODO - what to send here? */
	nfc_u8	selected_rf_protocol;
	nfc_u8	activated_rf_technology_and_mode;
	nfc_u8	max_data_packet_payload_size;
	nfc_u8  initial_num_of_credits_rf_conn;
	nfc_u8	length_rf_technology_specific_params;
	union rf_technology_specific_params_t rf_technology_specific_params;
	nfc_u8	rf_interface_type;
	nfc_u8  data_exchange_rf_technology_and_mode;
	/*
	* Bit rate that will be used for future Data Exchange in the transmit direction. For a polling device this is the bit rate from poll to listen,
	* and for a listening device this is the bit rate from listen to poll.
	*/
	nfc_u8  data_exchange_transmit_bitrate;
	/*
	* Baud Rate that will be used for future Data Exchange in the receive direction. For a polling device this is the bit rate from listen to poll,
	* and for a listening device this is the bit rate from poll to listen
	*/
	nfc_u8  data_exchange_receive_bitrate;
	nfc_u8  activation_params_len;
	union
	{
		struct nci_activate_nfca_poll_iso_dep		nfca_poll_iso_dep;
		struct nci_activate_nfca_poll_nfc_dep		nfca_poll_nfc_dep;
		struct nci_activate_nfca_listen_iso_dep		nfca_listen_iso_dep;
		struct nci_activate_nfca_listen_nfc_dep		nfca_listen_nfc_dep;
		struct nci_activate_nfcf_poll_nfc_dep		nfcf_poll_nfc_dep;
		struct nci_activate_nfcf_listen_nfc_dep		nfcf_listen_nfc_dep;
		// TBD - add B
	} activation_params;
};



/*** NCI_OPCODE_RF_IF_DEACTIVATE_NTF ***/
#define	NCI_DEACTIVATE_TYPE_IDLE_MODE		0
#define	NCI_DEACTIVATE_TYPE_SLEEP_MODE		1
#define	NCI_DEACTIVATE_TYPE_SLEEP_AF_MODE	2
#define	NCI_DEACTIVATE_TYPE_DISCOVERY		3

#define	NCI_DEACTIVATE_REASON_DH_REQUEST	0
#define	NCI_DEACTIVATE_REASON_EP_REQUEST	1
#define	NCI_DEACTIVATE_REASON_RF_LINK_LOSS	2
#define	NCI_DEACTIVATE_REASON_NFC_B_BAD_AFI	3

struct nci_cmd_deactivate
{
	nfc_u8	type;
};

struct nci_ntf_deactivate
{
	nfc_u8	type;
	nfc_u8	reason;
};


/*** NCI_OPCODE_NFCEE_DISCOVER_CMD ***/
#define	NCI_NFCEE_DISCOVER_ACTION_DISABLE	    0
#define	NCI_NFCEE_DISCOVER_ACTION_ENABLE   		1
struct nci_cmd_nfcee_discover
{
	nfc_u8	action;
};

struct nci_rsp_nfcee_discover
{
	nfc_u8 status;
	nfc_u8 num_nfcee;   /* 0 - 255 Indicates the number of NFCEEs connected to the NFCEE */
};

/* detected_rf_technology_and_mode values */
#define NFCEE_STATUS_CONNECTED_AND_ACTIVE		0x00
#define	NFCEE_STATUS_CONNECTED_AND_INACTIVE		0x01
#define	NFCEE_STATUS_REMOVED            		0x02

struct nci_ntf_nfcee_discover
{
	nfc_u8 nfcee_id;
	nfc_u8 nfcee_status;
	nfc_u8 num_nfcee_interface_information_entries;  /* Number of supported NFCEE Protocols */
	nfc_u8 nfcee_protocols[NCI_MAX_NFCEE_PROTOCOLS];
	nfc_u8 num_nfcee_information_TLVs;
};


/*** NCI_OPCODE_NFCEE_MODE_SET_CMD ***/
#define	NCI_NFCEE_MODE_DEACTIVATE	            0x00
#define	NCI_NFCEE_MODE_ACTIVATE   		        0x01
struct nci_cmd_nfcee_mode_set
{
	nfc_u8 nfcee_id;
	nfc_u8 nfcee_mode;
};

#define NCI_ROUTING_MORE_LAST_MSG (0x00)
#define NCI_ROUTING_MORE_NOT_MSG  (0x01)

#define NCI_ROUTING_TYPE_TECH			(0x00)
#define NCI_ROUTING_TYPE_PROTOCOL		(0x01)
#define NCI_ROUTING_TYPE_AID		    (0x02)

#define NCI_ROUTING_POWER_STATE_SWITCHED_ON		(0x01)
#define NCI_ROUTING_POWER_STATE_SWITCHED_OFF	(0x02) /* i.e. bat low*/
#define NCI_ROUTING_POWER_STATE_BAT_OFF			(0x04)

#define NCI_NFC_RF_TECHNOLOGY_A		(0x00)
#define NCI_NFC_RF_TECHNOLOGY_B		(0x01)
#define NCI_NFC_RF_TECHNOLOGY_F		(0x02)
#define NCI_NFC_RF_TECHNOLOGY_15693	(0x03)
#define NCI_NFC_RF_NUM_TECHNOLOGIES (NCI_NFC_RF_TECHNOLOGY_15693 - NCI_NFC_RF_TECHNOLOGY_A + 1)

typedef struct
{
	nfc_u8 route_to; /*An NFCEE ID as defined in Table 82*/
	nfc_u8 power_state;
	nfc_u8 tech;
}listen_mode_routing_type_tech_value_t;

typedef struct
{
	nfc_u8 route_to; /*An NFCEE ID as defined in Table 82*/
	nfc_u8 power_state;
	nfc_u8 protocol;
}listen_mode_routing_type_protocol_value_t;

typedef struct
{
	nfc_u8 route_to; /* An NFCEE ID as defined in Table 82*/
	nfc_u8 power_state;
	nfc_u8 aid[16]; /* 5-16 Octets	Application Identifier*/
}listen_mode_routing_type_aid_value_t;

/**** NCI_OPCODE_RF_SET_LISTEN_MODE_ROUTING_CMD ***/
struct nci_cmd_rf_set_listen_mode_routing
{
	nfc_u8 more_to_follow;
	nfc_u8 num_of_routing_entries;
	struct nci_tlv	*p_routing_entries;
};

#define MAX_SUPPORTED_AIDS_NUM (32)
#define MAX_SUPPORTED_NUM_ROUTING_HOSTS (5)

typedef struct
{
	listen_mode_routing_type_aid_value_t aid_value;
	nfc_u8 aid_len;
}listen_mode_routing_type_aid_value_w_len_t;

typedef enum
{
	ROUTE_RF_TECHNOLOGY_INVALID = 0x00,
	ROUTE_RF_TECHNOLOGY_A		= 0x01,
	ROUTE_RF_TECHNOLOGY_B		= 0x02,
	ROUTE_RF_TECHNOLOGY_F		= 0x04,
	ROUTE_RF_TECHNOLOGY_15693	= 0x08,

}hosts_priority_tech_e;

typedef struct
{
	nfc_u8 techs;
	nfc_u8 nfcee_id;
}hosts_priority_with_tech_t;

/* hosts_priority_arr - is an array of hosts ordered by: 0 - highest priority, (MAX_SUPPORTED_NUM_ROUTING_HOSTS-1) - lowest*/
/* isSwitchOffModeSupported - means is Bat. low mode should be supported in terms of DH*/
/* isPowerOffModeSupported - means is Bat. off mode should be supported in terms of DH*/
typedef struct
{
	nfc_s8											aids_arr_size;
	listen_mode_routing_type_aid_value_w_len_t		aids_arr[MAX_SUPPORTED_AIDS_NUM];
	nfc_s8											hosts_priority_arr_size;
	hosts_priority_with_tech_t						hosts_priority_arr [MAX_SUPPORTED_NUM_ROUTING_HOSTS];
	nfc_u8											isSwitchOffModeSupported; //Bat. low mode
	nfc_u8											isPowerOffModeSupported; //Bat. off mode
}configure_listen_routing_t;


/* Union for NCI Command Parameters  */
typedef union
{
	struct nci_cmd_core_reset		core_reset;
	struct nci_cmd_core_set_config	core_set_config;
	struct nci_cmd_conn_create		conn_create;
	struct nci_cmd_conn_close		conn_close;
	struct nci_cmd_discover			discover;
	struct nci_cmd_discover_map		discover_map;
	struct nci_cmd_deactivate		deactivate;
	struct nci_cmd_nfcee_discover		nfcee_discover;
	struct nci_cmd_nfcee_mode_set	nfcee_mode_set;
	struct nci_cmd_core_get_config	core_get_config;
	struct nci_cmd_rf_discover_select rf_discover_select;
	struct nci_cmd_rf_set_listen_mode_routing rf_set_listen_mode_routing;

} nci_cmd_param_u;


/* Union for NCI Response Parameters  */
typedef union
{
	struct nci_rsp_gen			        	generic;
	struct nci_rsp_reset		        		reset;
	struct nci_rsp_init			        	init;
	struct nci_rsp_conn_create	        	conn_create;
	struct nci_rsp_core_set_config		core_set_config;
	struct nci_rsp_nfcee_discover	    	nfcee_discover;
	struct nci_rsp_core_get_config		core_get_config;
} nci_rsp_param_u;

/* Union for NCI Notifications Parameters  */
typedef union
{
	struct nci_ntf_reset				reset;
	struct nci_ntf_discover				discover;
	struct nci_ntf_activate				activate;
	struct nci_ntf_deactivate			deactivate;
	struct nci_ntf_rf_field_info		rf_field_info;
	struct nci_ntf_core_conn_credit		conn_credit;
	struct nci_ntf_nfcee_discover		nfcee_discover;
	struct nci_ntf_rf_nfcee_action		rf_nfcee_action;
	struct nci_ntf_rf_nfcee_discovery_req	rf_nfcee_discovery_req;
	struct nci_ntf_core_intf_error		core_iface_error;
	struct nci_ntf_core_generic_error   core_generic_error;
} nci_ntf_param_u;

/********************************************************************************
 * NCI Operation Payload Definition
 ********************************************************************************/
struct nci_op_rsp_gen
{
	nfc_u8 status;
};
/*** NCIX_OPCODE_START ***/

struct nci_op_start
{
	nfc_u32 rsvd;
};
struct nci_op_stop
{
	nfc_u8 mode;	/* 0 = NFC OFF, 1 = BAT LOW (switched off) */
};
struct nci_op_rsp_start
{
	nfc_u8							status; /* operation completion status (if !=NFC_RES_OK -> check
												command specific status) */
	struct nci_rsp_reset			reset;
	struct nci_rsp_init				init;
	struct nci_rsp_conn_create		conn_create;
	struct nci_rsp_nfcee_discover	nfcee_discover_rsp;
	nfc_u8							nfcee_discover_ntf_number;
  	struct nci_ntf_nfcee_discover	nfcee_discover_ntf[NCI_MAX_NUM_NFCEE];
};

/*** NCIX_OPCODE_DETECT ***/

/*******************************************************************************
   Protocols
*******************************************************************************/
/* TBD: change names to "NCI" defines */
#define NCI_PROTOCOL_READER_ISO_14443_4_A    0x0001  /* Reader ISO 14443 A level 4 */
#define NCI_PROTOCOL_READER_ISO_14443_4_B    0x0002  /* Reader ISO 14443 B level 4 */
#define NCI_PROTOCOL_READER_ISO_14443_3_A    0x0004  /* Reader ISO 14443 A level 3 */
#define NCI_PROTOCOL_READER_ISO_14443_3_B    0x0008  /* Reader ISO 14443 B level 3 */
#define NCI_PROTOCOL_READER_ISO_15693_3      0x0010  /* Reader ISO 15693 level 3 */
#define NCI_PROTOCOL_READER_ISO_15693_2      0x0020  /* Reader ISO 15693 level 2 */
#define NCI_PROTOCOL_READER_FELICA           0x0040  /* Reader Felica */
#define NCI_PROTOCOL_READER_P2P_INITIATOR_A  0x0080  /* Reader P2P Initiator ISO 14443 A */
#define NCI_PROTOCOL_READER_P2P_INITIATOR_F  0x0100  /* Reader P2P Initiator Felica */
#define NCI_PROTOCOL_READER_TYPE_1_CHIP      0x0200  /* Reader Type 1 */
#define NCI_PROTOCOL_READER_MIFARE_CLASSIC   0x0400  /* Reader Mifare Classic */
#define NCI_PROTOCOL_READER_BPRIME           0x0800  /* Reader B Prime */

#define NCI_PROTOCOL_CARD_ISO_14443_4_A      0x0001  /* Card ISO 14443 A level 4 */
#define NCI_PROTOCOL_CARD_ISO_14443_4_B      0x0002  /* Card ISO 14443 B level 4 */
#define NCI_PROTOCOL_CARD_ISO_14443_3_A      0x0004  /* Card ISO 14443 A level 3 */
#define NCI_PROTOCOL_CARD_ISO_14443_3_B      0x0008  /* Card ISO 14443 B level 3 */
#define NCI_PROTOCOL_CARD_ISO_15693_3        0x0010  /* Card ISO 15693 level 3 */
#define NCI_PROTOCOL_CARD_ISO_15693_2        0x0020  /* Card ISO 15693 level 2 */
#define NCI_PROTOCOL_CARD_FELICA             0x0040  /* Card Felica */
#define NCI_PROTOCOL_CARD_P2P_TARGET_A       0x0080  /* Card P2P Target ISO 14443 A */
#define NCI_PROTOCOL_CARD_P2P_TARGET_F       0x0100  /* Card P2P Target Felica */
#define NCI_PROTOCOL_CARD_TYPE_1_CHIP        0x0200  /* Card Type 1 */
#define NCI_PROTOCOL_CARD_MIFARE_CLASSIC     0x0400  /* Card Mifare Classic */
#define NCI_PROTOCOL_CARD_BPRIME             0x0800  /* Card B Prime */

struct nci_op_rf_enable
{
	nfc_u16	ce_bit_mask;
	nfc_u16	rw_bit_mask;
};
/*** NCIX_OPCODE_CANCEL_DETECTION ***/
struct nci_op_rf_disable
{
	nfc_u32 rsvd;
};

enum nfcee_switch_mode
{
	E_SWITCH_INVALID = 0x0a,
	E_SWITCH_RF_COMM,
	E_SWITCH_HOST_COMM,
	E_SWITCH_OFF
};

struct nci_op_nfcee
{
	nfc_u8					nfcee_id;
	enum nfcee_switch_mode	new_mode;
};

struct nci_op_core_set_config
{
	struct nci_cmd_core_set_config core_set_config;
	struct nci_cmd *p_cmd;
};

struct nci_op_configure_listen_routing
{
	configure_listen_routing_t listen_routing_config;
};

struct nci_op_script
{
	const nfc_s8* filename;
};

/* Union for NCI Command Parameters  */
typedef union
{
	struct nci_op_start			start;		/* NCIX_OPCODE_START,				ncix_start_rsp */
	struct nci_op_stop			stop;		/* NCIX_OPCODE_STOP,				ncix_gen_rsp */
	struct nci_op_rf_enable		rf_enable;	/* NCIX_OPCODE_DETECT,				ncix_gen_rsp */
	struct nci_op_rf_disable	rf_disable;	/* NCIX_OPCODE_CANCEL_DETECTION,	ncix_gen_rsp */
	struct nci_op_nfcee			nfcee;		/* */
	struct nci_op_core_set_config config;
	struct nci_op_configure_listen_routing listen_routing;
	struct nci_op_script		script;
} op_param_u;




struct nci_op_rsp_nfcee
{
	nfc_u8							status; /* operation completion status (if !=NFC_RES_OK -> check
												command specific status) */
	nfc_u8							nfcee_id;
	struct nci_rsp_conn_create		host_comm_params;

	enum nfcee_switch_mode			new_mode;
};

/* Union for NCI Response Parameters  */
typedef union
{
	struct  nci_op_rsp_gen		generic;
	struct  nci_op_rsp_start	start;
	struct  nci_op_rsp_nfcee	nfcee;

} op_rsp_param_u;


/*********************************************************************************/
/* NCI Global Routines                                                           */
/*********************************************************************************/

typedef void (*data_cb_f)(nfc_handle_t, nfc_u8 conn_id, nfc_u8 *p_nci_payload, nfc_u32 nci_payload_len, nci_status_e status);
typedef void (*ntf_cb_f)(nfc_handle_t, nfc_u16, nci_ntf_param_u*);
typedef void (*rsp_cb_f)(nfc_handle_t, nfc_u16, nci_rsp_param_u*);
typedef void (*op_rsp_cb_f)(nfc_handle_t h_cb, nfc_u16 opcode, op_rsp_param_u *pu_resp);
typedef void (*vendor_specific_cb_f)(nfc_handle_t usr_param, nfc_u16 opcode, nfc_u8* p_rsp_buff, nfc_u8 rsp_buff_len);
typedef void (*script_rsp_cb_f)(nfc_handle_t usr_param, nci_status_e status);

/**

 * Create NCI module - this will involve only SW init (context allocation, callbacks registration)
 * and registration of callback for incoming data. Returns handle to created NCI object
 *
 * @h_caller:		caller handle (to be used with data rx and tx callbacks)
 * @h_osa:			os context
 * @f_rx_ind_cb:	rx indication callback (for received data packets)
 * @f_tx_done_cb:	to be called when packet transferring (to NFCC) is completed (when transport send function is done)
 *

 **/

nfc_handle_t nci_create(nfc_handle_t h_caller,
					nfc_handle_t h_osa,
					data_cb_f f_rx_ind_cb,
					data_cb_f f_tx_done_cb);

/**

 * Destroy NCI module and free all its resources
 *
 * @h_nci:			handle to NCi object to destoy
 *

 **/
void nci_destroy(nfc_handle_t h_nci);

/**

 * Transport enabler - Connect to transport driver and open a data stream
 *
 * @h_nci:			handle to NCi object to open
 * @dev_id:			device id to be sent to transport driver
 *

 **/
nci_status_e nci_open(nfc_handle_t h_nci, nfc_u32 dev_id);

/**

 * Close Transport channel
 *
 * @h_nci:			handle to NCi object to close
 * @dev_id:			device id to be sent to transport driver
 *

 **/
nci_status_e nci_close(nfc_handle_t h_nci, nfc_u32 dev_id);


/**

 * Send Raw NCI data packet
 *
 * @h_nci:			handle to NCi object
 * @conn_id:		NCI connection id to send data
 * @p_nci_payload:	data packet (payload). Payload is being copied within this routine and can be freed by caller upon return.
 * @nci_payload_len:payload length.
 * @rx_timeout:		timeout in ms to wait for rx. 0 means no timeout is required. IMPORTANT: In P2P do not activate the timeout, since LLCP has internal mechanism for that.
 *

 **/
nci_status_e nci_send_data(nfc_handle_t h_nci, nfc_u8 conn_id, nfc_u8 *p_nci_payload, nfc_u32 nci_payload_len, nfc_u32 tx_timeout);

/**

 * Send single NCI command and register response handler
 *
 * @h_nci:			handle to NCi object
 * @opcode:			Command identifier
 * @pu_param:		A union representing NCI command payload (length is known by the opcode)
 *					pu_param is being copied inside this callback and can be freed by caller upon return.
 * @pu_resp:		Optional buffer for NCI response payload (NULL if response is ignored).
 * @rsp_len:		Maximum length of expected response (Assert will be invoked if response exceeds this).
 * @f_rsp_cb:		Optional callback function to be invoked upon response reception (or NULL).
 * @h_rsp_cb:		Optional parameter that is passed to f_rsp_cb, if invoked (caller object handle)
 *

 **/
nci_status_e nci_send_cmd(	nfc_handle_t h_nci,
							nfc_u16 opcode,
							nci_cmd_param_u *pu_param,
							nci_rsp_param_u *pu_resp,
							nfc_u8 rsp_len,
							rsp_cb_f f_rsp_cb,
							nfc_handle_t h_rsp_cb);

/**

* Send NCI commands script and register response handler. Return:
*					Success - NCI_STATUS_OK
*					Failure - NCI_STATUS_BUFFER_FULL \ NCI_STATUS_FAILED
*
* @h_nci:			handle to NCI object
* @pu_filename:		script file name
* @f_rsp_cb:		Optional callback function to be invoked upon completion of script (or NULL).
* @h_rsp_cb:		Optional parameter that is passed to f_rsp_cb, if invoked (caller object handle).
*

**/

void nci_send_script(nfc_handle_t h_nci,
	const nfc_s8* const pu_filename,
	script_rsp_cb_f f_rsp_cb, nfc_handle_t h_rsp_cb);

typedef enum
{
	E_NCI_NTF_CORE_RESET,
	E_NCI_NTF_CORE_CONN_CREDITS,
	E_NCI_NTF_CORE_GENERIC_ERROR,
	E_NCI_NTF_CORE_INTERFACE_ERROR,
	E_NCI_NTF_RF_GET_LISTEN_MODE_ROUTING,
	E_NCI_NTF_RF_DISCOVER,
	E_NCI_NTF_RF_INTF_ACTIVATED,
	E_NCI_NTF_RF_DEACTIVATE,
	E_NCI_NTF_RF_FIELD_INFO,
	E_NCI_NTF_RF_T3T_POLLING_NTF,
	E_NCI_NTF_RF_NFCEE_ACTION,
	E_NCI_NTF_RF_NFCEE_DISCOVERY_REQ,
	E_NCI_NTF_NFCEE_DISCOVER,
	E_NCI_NTF_ALL,

	E_NCI_NTF_MAX
} nci_ntf_e;

/**

 * Register a callback to specific notification
 *
 * @h_nci:			handle to NCi object
 * @ntf_id:			notification id from nci_ntf_e enum
 * @tlv_num:		number of TLVs in the array
 * @f_rsp_cb:		callback function to be invoked upon notification reception
 * @h_rsp_cb:		Optional parameter that is passed to f_rsp_cb, if invoked (caller object handle)
 *

 **/

void nci_register_ntf_cb(nfc_handle_t h_nci,
						 nci_ntf_e ntf_id,
						 ntf_cb_f f_ntf_cb,
						 nfc_handle_t h_ntf_cb);


/**

 * Un-Register a callback to specific notification
 *
 * @h_nci:			handle to NCi object
 * @ntf_id:			notification id from nci_ntf_e enum
 * @tlv_num:		number of TLVs in the array
 * @f_rsp_cb:		callback function to unregister from ntf_id list
 *

 **/
void nci_unregister_ntf_cb(nfc_handle_t h_nci, nci_ntf_e ntf_id, ntf_cb_f f_ntf_cb);


/**

 * Start NCI operation (operation - a sequence of NCi commands and responses that can't be interrupted)
 *
 * @h_nci:			handle to NCi object
 * @opcode:			operation identifier
 * @pu_param:		A union representing NCI operation parameters (an input to the operation)
 * @pu_resp:		Optional buffer for NCI operation response (NULL if response is ignored).
 * @rsp_len:		Maximum length of expected response (Assert will be invoked if response exceeds this).
 * @f_rsp_cb:		Optional callback function to be invoked upon operation completion (or NULL).
 * @h_rsp_cb:		Optional parameter that is passed to f_rsp_cb, if invoked (caller object handle)
 *

 **/
nci_status_e nci_start_operation(nfc_handle_t h_nci,
								 nfc_u16 opcode,
								 op_param_u *pu_param,
								 op_rsp_param_u *pu_resp,
								 nfc_u8 rsp_len,
								 op_rsp_cb_f f_rsp_cb,
								 nfc_handle_t h_rsp_cb);


nci_status_e nci_send_vendor_specific_cmd(	nfc_handle_t h_nci,
											nfc_u16 opcode,
											nfc_u8* p_payload,
											nfc_u8 payload_len,
											vendor_specific_cb_f vendor_specific_rsp_cb,
											nfc_handle_t vendor_specific_cb_param);

void nci_register_vendor_specific_ntf_cb(	nfc_handle_t h_nci,
											nfc_u16 opcode,
											vendor_specific_cb_f f_ntf_cb,
											nfc_handle_t vendor_specific_cb_param);

void nci_unregister_vendor_specific_ntf_cb(	nfc_handle_t h_nci,
											nfc_u16 opcode,
											vendor_specific_cb_f f_ntf_cb);

#endif //NCI_VERSION_IN_USE

#endif /* __NCI_API_H */























