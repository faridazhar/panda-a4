/*
 * nfc_trans_adapt.h
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

 Definition for The NCI Transport Adapter API

*******************************************************************************/

#ifndef __NFC_HAL_TRANS_H
#define __NFC_HAL_TRANS_H

#include "nfc_types.h"


nfc_handle_t transa_create(nfc_handle_t h_osa);
void transa_destroy(nfc_handle_t h_trans);

#endif /* __NFC_HAL_TRANS_H */
