/*
 * nfc_types.h
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

  This header files contains the basic types used by NCI Driver.

*******************************************************************************/

#ifndef __NFC_TYPES_H
#define __NFC_TYPES_H

typedef void *			nfc_handle_t;

typedef unsigned char	nfc_u8;
typedef unsigned short	nfc_u16;
typedef unsigned long	nfc_u32;
typedef signed char		nfc_s8;
typedef signed short	nfc_s16;
typedef signed long		nfc_s32;
typedef nfc_u8			nfc_bool;

#define NFC_FALSE (0)
#define NFC_TRUE (1)

#ifndef NULL
#define NULL	0
#endif

/* Status Enums Definition */
typedef enum
{
	NFC_RES_COMPLETE, 	/* synchronous operation (completed successfully) */
	NFC_RES_OK = NFC_RES_COMPLETE,
	NFC_RES_PENDING, 	/* a-synchronous operation (completion event will follow) */
	NFC_RES_ERROR,
	NFC_RES_PARAM_ERROR,/* failure in param passing */
	NFC_RES_MEM_ERROR,	/* failure in memory allocation */
	NFC_RES_FILE_ERROR,	/* failure in file access */
	NFC_RES_STATE_ERROR,	/* invalid state */
	NFC_RES_TRUNCATED,  /* return buffer truncated */
	NFC_RES_UNKNOWN_OPCODE  /* unknown opcode */
} nfc_status;


#endif /* __NFC_TYPES_H */
