/*
 * nci_buff.h
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

  Header file for definition of NCI abstrcuted data buffer

*******************************************************************************/

#ifndef __NFC_NCI_BUFF_H
#define __NFC_NCI_BUFF_H

#include "nfc_types.h"


struct nci_buff;

struct nci_buff * nci_buff_alloc(nfc_u32 length);
nfc_u8 *nci_buff_data(struct nci_buff *p_buff);
nfc_u32 nci_buff_length(struct nci_buff *p_buff);
nfc_u8 *nci_buff_add_prefix(struct nci_buff *p_buff, nfc_u32 length);
nfc_u8 *nci_buff_cut_prefix(struct nci_buff *p_buff, nfc_u32 length);
nfc_u8 *nci_buff_add_suffix(struct nci_buff *p_buff, nfc_u32 length);
nfc_u8 *nci_buff_cut_suffix(struct nci_buff *p_buff, nfc_u32 length);
struct nci_buff *nci_buff_append(struct nci_buff *p_this, struct nci_buff *p_appended_buff);
nfc_u32	nci_buff_copy(nfc_u8 *p_dest, struct nci_buff *p_src_buff, nfc_u32 dest_len);
void	nci_buff_free(struct nci_buff *p_buff);
nfc_u8 nci_buff_get_fragments(struct nci_buff *p_buff, nfc_u8 max_frag_num, nfc_u8 *p_frag[], nfc_u32 frag_len[]);
void nci_buff_dump(struct nci_buff *p_buff, const nfc_s8 *p_str);

#endif /* __NFC_NCI_BUFF_H */
