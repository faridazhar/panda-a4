/*
 * nfc_os_adapt.h
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

  This header files contains the OS adapatation definitions used by NFC Driver

*******************************************************************************/

#ifndef __NFC_OS_ADAPT_H
#define __NFC_OS_ADAPT_H

#include "nfc_types.h"
#include "nci_api.h"

#ifdef ANDROID
#include <utils/Log.h>
#endif


#define HCI_CHANNEL_NFC			12

/* NAL thread Queues */
#define NCI_MAX_DATA_CONNS	NCI_MAX_CONNS

enum
{
	E_NCI_Q_RESERVED = 0,	/* Q ID 0 is reserved (for timer handling) */
	E_NCI_Q_RX,
	E_NCI_Q_CMD,
	E_NCI_Q_OPERATION,
	E_NCI_Q_DATA,	/* DATA queues per maximum supported connetions */
	E_NCI_Q_DATA_LAST = E_NCI_Q_DATA + NCI_MAX_DATA_CONNS - 1,
	E_HCI_Q_OPERATION,

	E_NCI_Q_MAX,
};

/* Framework creation and destruction */
nfc_handle_t osa_create();
void osa_destroy(nfc_handle_t);

/* Memory API */
void		*osa_mem_alloc(nfc_u32 length);
void		osa_mem_free(void *p_buff);
void		osa_mem_set(void *p_buff, nfc_u8 value, nfc_u32 length);
void		osa_mem_zero(void *p_buff, nfc_u32 length);
void		osa_mem_copy(void *p_dst, void *p_src, nfc_u32 length);

/* Timer API */
typedef void (*osa_timer_cb)(nfc_handle_t h_cb);
nfc_handle_t	osa_timer_start(nfc_handle_t h_osa, nfc_u32 expiry_time, osa_timer_cb f_cb, nfc_handle_t h_cb);
void		osa_timer_stop(void *h_timer);
nfc_u32		osa_time_sec();
nfc_u32		osa_time_msec();

/* Task Handling API */

/* osa_taskq_cb - definition of user callback that will process the incoming data in NFC task Q context
   h_cb - is the handle, registered within osa_taskq_register
   p_param - is the pointer to user defined message, sent within osa_task_schedule
 */
typedef void (*osa_taskq_cb)(nfc_handle_t h_cb, nfc_handle_t p_param);
nfc_status	osa_taskq_register(nfc_handle_t h_osa, nfc_u8 que_id, osa_taskq_cb f_cb, nfc_handle_t h_cb, nfc_u8 b_initially_enabled);
nfc_status osa_taskq_unregister(nfc_handle_t h_osa, nfc_u8 que_id);
nfc_status	osa_taskq_schedule(nfc_handle_t h_osa, nfc_u8 que_id, nfc_handle_t p_param);
void		osa_taskq_enable(nfc_handle_t h_osa, nfc_u8 que_id);
void		osa_taskq_disable(nfc_handle_t h_osa, nfc_u8 que_id);

/* Persistent Properties API */
#define MAX_KEY_LEN  (32)
#define MAX_PROP_LEN (0xFFFF)
nfc_status osa_setprop(const nfc_s8* const key, nfc_u8* buff, nfc_u16 buff_len);
nfc_status osa_getprop(const nfc_s8* const key, nfc_u8* buff, nfc_u16* buff_len);

/* Endianess API */
nfc_u16 osa_btoh16(const nfc_u8 *u16);
void	osa_htob16(const nfc_u16 val, nfc_u8 *u16);
nfc_u32 osa_btoh32(const nfc_u8 *u32);
void	osa_htob32(const nfc_u32 val, nfc_u8 *u32);
nfc_u16 osa_ltoh16(const nfc_u8 *u16);
void	osa_htol16(const nfc_u16 val, nfc_u8 *u16);
nfc_u32 osa_ltoh32(const nfc_u8 *u32);
void	osa_htol32(const nfc_u32 val, nfc_u8 *u32);


/* Report API */
#ifdef ANDROID
#define osa_report(severity,msg)                        ALOGI msg
#define osa_dump_buffer(prefix, buffer, len)
#else
char *format_msg(const char *format, ...);
void report_int(const char*		fileName,
				nfc_u32			line,
				nfc_u32			severity,
				const char* 	msg);
void dump_int (const char 		*file,
				nfc_u32			line,
				const char		*pTitle,
				const nfc_u8	*pBuf,
				const nfc_u32	len);

enum
{
	E_REPORT_INFORMATION	= 10,
	E_REPORT_ERROR			= 8,
	E_REPORT_FATAL			= 6,
};

#define osa_report(severity, msg)						report_int(__FILE__, __LINE__, E_REPORT_##severity, format_msg msg)
#define osa_dump_buffer(prefix, buffer, len)			dump_int(__FILE__, __LINE__, prefix, buffer, len)
#endif /* ANDROID */

void osa_assert(const char *expression, const nfc_s8 *file, nfc_u16 line);

void osa_exit(const nfc_bool failure);

#define	OSA_ASSERT(exp)									(((exp)!= 0)?(void)0:osa_assert(#exp,(const nfc_s8*)__FILE__,(nfc_u16)__LINE__))
#define	OSA_UNUSED_PARAMETER(par)						((par) = (par))

#define OSA_MIN(a,b) (((a) < (b)) ? (a) : (b))
#define OSA_MAX(a,b) (((a) > (b)) ? (a) : (b))

#endif /* __NFC_OS_ADAPT_H */
