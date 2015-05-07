/*
 * nci_dev.h
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

  Definition of the NCI Device I/F

*******************************************************************************/

#ifndef __NFC_NCI_DEV_H
#define __NFC_NCI_DEV_H

#include "nfc_types.h"
#include "nci_buff.h"

struct ncidev
{
	struct ncidev	*p_next;
	nfc_handle_t		h_trans;
	nfc_handle_t		h_osa;
	nfc_u32			dev_id;
	nfc_status		(*open)(nfc_handle_t h_trans);
	nfc_status		(*close)(nfc_handle_t h_trans);
	nfc_status		(*send)(nfc_handle_t h_trans, struct nci_buff*);
};

nfc_status ncidev_register(struct ncidev *p_ncidev);
void ncidev_unregister(struct ncidev *p_ncidev);
nfc_status ncidev_receive(struct ncidev *p_ncidev, struct nci_buff*);
void ncidev_log(struct ncidev *p_ncidev, struct nci_buff *p_buff, char *p_prefix);

#endif /* __NFC_NCI_DEV_H */
