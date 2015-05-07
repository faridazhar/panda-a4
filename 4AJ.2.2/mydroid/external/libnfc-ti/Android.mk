LOCAL_PATH:= $(call my-dir)
ifeq ($(NFC_TI_DEVICE), true)
#
# libnfc
#

include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm

#set NCI stack version to be used
NCI_VER_1   := 1
#NCI_DRAFT_9 := 2

#phLibNfc
LOCAL_SRC_FILES:= \
	src/phLibNfc.c \
	src/phLibNfc_discovery.c \
	src/phLibNfc_initiator.c \
	src/phLibNfc_llcp.c \
	src/phLibNfc_Ioctl.c \
	src/phLibNfc_ndef_raw.c \
	src/phLibNfc_SE.c \
	src/phLibNfc_target.c

#phHalNfc
LOCAL_SRC_FILES += src/phHal4Nfc_ADD.c
LOCAL_SRC_FILES += src/phHal4Nfc.c
LOCAL_SRC_FILES += src/phHal4Nfc_Emulation.c
LOCAL_SRC_FILES += src/phHal4Nfc_P2P.c
LOCAL_SRC_FILES += src/phHal4Nfc_Reader.c

#phDnldNfc
#LOCAL_SRC_FILES += src/phDnldNfc.c

#phHciNfc
#LOCAL_SRC_FILES += src/phHciNfc_AdminMgmt.c
#LOCAL_SRC_FILES += src/phHciNfc.c
#LOCAL_SRC_FILES += src/phHciNfc_CE_A.c
#LOCAL_SRC_FILES += src/phHciNfc_CE_B.c
#LOCAL_SRC_FILES += src/phHciNfc_DevMgmt.c
#LOCAL_SRC_FILES += src/phHciNfc_Emulation.c
#LOCAL_SRC_FILES += src/phHciNfc_Felica.c
#LOCAL_SRC_FILES += src/phHciNfc_Generic.c
#LOCAL_SRC_FILES += src/phHciNfc_IDMgmt.c
#LOCAL_SRC_FILES += src/phHciNfc_ISO15693.c
#LOCAL_SRC_FILES += src/phHciNfc_Jewel.c
#LOCAL_SRC_FILES += src/phHciNfc_LinkMgmt.c
#LOCAL_SRC_FILES += src/phHciNfc_NfcIPMgmt.c
#LOCAL_SRC_FILES += src/phHciNfc_Pipe.c
#LOCAL_SRC_FILES += src/phHciNfc_PollingLoop.c
#LOCAL_SRC_FILES += src/phHciNfc_RFReaderA.c
#LOCAL_SRC_FILES += src/phHciNfc_RFReaderB.c
#LOCAL_SRC_FILES += src/phHciNfc_RFReader.c
#LOCAL_SRC_FILES += src/phHciNfc_Sequence.c
#LOCAL_SRC_FILES += src/phHciNfc_SWP.c
#LOCAL_SRC_FILES += src/phHciNfc_WI.c

#phLlcNfc
#LOCAL_SRC_FILES += src/phLlcNfc.c
#LOCAL_SRC_FILES += src/phLlcNfc_Frame.c
#LOCAL_SRC_FILES += src/phLlcNfc_Interface.c
#LOCAL_SRC_FILES += src/phLlcNfc_StateMachine.c
#LOCAL_SRC_FILES += src/phLlcNfc_Timer.c

#phFricNfc_Llcp
LOCAL_SRC_FILES += src/phFriNfc_Llcp.c
LOCAL_SRC_FILES += src/phFriNfc_LlcpUtils.c
LOCAL_SRC_FILES += src/phFriNfc_LlcpTransport.c
LOCAL_SRC_FILES += src/phFriNfc_LlcpTransport_Connectionless.c
LOCAL_SRC_FILES += src/phFriNfc_LlcpTransport_Connection.c
LOCAL_SRC_FILES += src/phFriNfc_LlcpMac.c
LOCAL_SRC_FILES += src/phFriNfc_LlcpMacNfcip.c

#phFriNfc_NdefMap
LOCAL_SRC_FILES += src/phFriNfc_FelicaMap.c
LOCAL_SRC_FILES += src/phFriNfc_MifareStdMap.c
LOCAL_SRC_FILES += src/phFriNfc_MifareULMap.c
LOCAL_SRC_FILES += src/phFriNfc_MapTools.c
LOCAL_SRC_FILES += src/phFriNfc_TopazMap.c
LOCAL_SRC_FILES += src/phFriNfc_TopazDynamicMap.c
LOCAL_SRC_FILES += src/phFriNfc_DesfireMap.c
LOCAL_SRC_FILES += src/phFriNfc_ISO15693Map.c
LOCAL_SRC_FILES += src/phFriNfc_NdefMap.c
LOCAL_SRC_FILES += src/phFriNfc_IntNdefMap.c

#phFriNfc_NdefReg
LOCAL_SRC_FILES += src/phFriNfc_NdefReg.c

#phFriNfc_SmtCrdFmt
LOCAL_SRC_FILES += src/phFriNfc_DesfireFormat.c
LOCAL_SRC_FILES += src/phFriNfc_MifULFormat.c
LOCAL_SRC_FILES += src/phFriNfc_MifStdFormat.c
LOCAL_SRC_FILES += src/phFriNfc_SmtCrdFmt.c
LOCAL_SRC_FILES += src/phFriNfc_ISO15693Format.c

#phFriNfc_OvrHal
LOCAL_SRC_FILES += src/phFriNfc_OvrHal.c

#phOsalNfc
LOCAL_SRC_FILES += Linux_x86/phOsalNfc_Timer.c
LOCAL_SRC_FILES += Linux_x86/phOsalNfc.c
LOCAL_SRC_FILES += Linux_x86/phOsalNfc_Utils.c

#phDal4Nfc
#LOCAL_SRC_FILES += Linux_x86/phDal4Nfc_uart.c
LOCAL_SRC_FILES += Linux_x86/phDal4Nfc.c
#LOCAL_SRC_FILES += Linux_x86/phDal4Nfc_i2c.c
LOCAL_SRC_FILES += Linux_x86/phDal4Nfc_messageQueueLib.c

#os_adapter
LOCAL_SRC_FILES += ti/os_adapter/os_dl_list.c
LOCAL_SRC_FILES += ti/os_adapter/os_queue.c
LOCAL_SRC_FILES += ti/os_adapter/nfc_os_adapt.c
LOCAL_SRC_FILES += ti/os_adapter/nci_buff.c

#trans_adapter
LOCAL_SRC_FILES += ti/trans_adapter/nfc_trans_adapt.c

ifeq ($(NCI_VER_1), 1)
#hci
LOCAL_SRC_FILES += ti/hci/hci.c
LOCAL_SRC_FILES += ti/hci/hci_core.c
LOCAL_SRC_FILES += ti/hci/hci_admin.c
LOCAL_SRC_FILES += ti/hci/hci_identity_mgmt.c
LOCAL_SRC_FILES += ti/hci/hci_loop_back.c
LOCAL_SRC_FILES += ti/hci/hci_connectivity.c
LOCAL_SRC_FILES += ti/hci/hci_gto_apdu.c
endif

#nci draft 9
LOCAL_SRC_FILES += ti/nci/nci_api.c
LOCAL_SRC_FILES += ti/nci/nci_core.c
LOCAL_SRC_FILES += ti/nci/nci_data.c
LOCAL_SRC_FILES += ti/nci/nci_cmd_builder.c
LOCAL_SRC_FILES += ti/nci/nci_rsp_parser.c
LOCAL_SRC_FILES += ti/nci/nci_ntf_parser.c
LOCAL_SRC_FILES += ti/nci/nci_trans.c

LOCAL_SRC_FILES += ti/nci/ncix.c
LOCAL_SRC_FILES += ti/nci/ncix_main.c
LOCAL_SRC_FILES += ti/nci/ncix_nfcee.c
LOCAL_SRC_FILES += ti/nci/ncix_rf_mgmt.c
LOCAL_SRC_FILES += ti/nci/ncix_sm.c

#nci version 1 (draft 18)
LOCAL_SRC_FILES += ti/nci/NCI_ver_1/nci_api.c
LOCAL_SRC_FILES += ti/nci/NCI_ver_1/nci_core.c
LOCAL_SRC_FILES += ti/nci/NCI_ver_1/nci_data.c
LOCAL_SRC_FILES += ti/nci/NCI_ver_1/nci_cmd_builder.c
LOCAL_SRC_FILES += ti/nci/NCI_ver_1/nci_rsp_parser.c
LOCAL_SRC_FILES += ti/nci/NCI_ver_1/nci_ntf_parser.c
LOCAL_SRC_FILES += ti/nci/NCI_ver_1/nci_trans.c
LOCAL_SRC_FILES += ti/nci/NCI_ver_1/nci_sm.c
LOCAL_SRC_FILES += ti/nci/NCI_ver_1/nci_rf_sm.c

LOCAL_SRC_FILES += ti/nci/NCI_ver_1/ncix.c
LOCAL_SRC_FILES += ti/nci/NCI_ver_1/ncix_main.c
LOCAL_SRC_FILES += ti/nci/NCI_ver_1/ncix_nfcee.c
LOCAL_SRC_FILES += ti/nci/NCI_ver_1/ncix_rf_mgmt.c

ifeq ($(NCI_VER_1), 1)
LOCAL_CFLAGS += -DNCI_VERSION_IN_USE=$(NCI_VER_1)
endif
ifeq ($(NCI_DRAFT_9), 2)
LOCAL_CFLAGS += -DNCI_VERSION_IN_USE=$(NCI_DRAFT_9)
endif

LOCAL_CFLAGS += -DDEBUG -DNCI_DATA_BIG_ENDIAN

#Define TI NFC device support
LOCAL_CFLAGS += -DHAL_NFC_TI_DEVICE

LOCAL_CFLAGS += -DNXP_MESSAGING -DANDROID -DNFC_TIMER_CONTEXT -fno-strict-aliasing

# Uncomment for Chipset command/responses
# Or use "setprop debug.nfc.LOW_LEVEL_TRACES" at run-time
# LOCAL_CFLAGS += -DLOW_LEVEL_TRACES

# Uncomment for DAL traces
# LOCAL_CFLAGS += -DDEBUG -DDAL_TRACE

# Uncomment for LLC traces
# LOCAL_CFLAGS += -DDEBUG -DLLC_TRACE

# Uncomment for LLCP traces
# LOCAL_CFLAGS += -DDEBUG -DLLCP_TRACE

# Uncomment for HCI traces
# LOCAL_CFLAGS += -DDEBUG -DHCI_TRACE

#includes
LOCAL_CFLAGS += -I$(LOCAL_PATH)/ti/inc
LOCAL_CFLAGS += -I$(LOCAL_PATH)/ti/nci
LOCAL_CFLAGS += -I$(LOCAL_PATH)/ti/nfcc_adapter
LOCAL_CFLAGS += -I$(LOCAL_PATH)/inc
LOCAL_CFLAGS += -I$(LOCAL_PATH)/Linux_x86
LOCAL_CFLAGS += -I$(LOCAL_PATH)/src

LOCAL_MODULE:= libnfc
LOCAL_MODULE_TAGS := optional
LOCAL_SHARED_LIBRARIES := libcutils libnfc_ndef libdl libhardware

include $(BUILD_SHARED_LIBRARY)

#
# libnfc_ndef
#

include $(CLEAR_VARS)

LOCAL_SRC_FILES += src/phFriNfc_NdefRecord.c

LOCAL_CFLAGS += -I$(LOCAL_PATH)/inc
LOCAL_CFLAGS += -I$(LOCAL_PATH)/src

LOCAL_MODULE:= libnfc_ndef
LOCAL_MODULE_TAGS := optional
LOCAL_SHARED_LIBRARIES := libcutils

include $(BUILD_SHARED_LIBRARY)
endif
