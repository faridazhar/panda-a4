/*
 * nfc_os_adapt.c
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

  Implementation of the nfc OS adaptation Layer (using LibNfc phOsalNfc)

*******************************************************************************/
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>

#include "nfc_types.h"
#include "nfc_os_adapt.h"
#include "nci_utils.h"
#include "phOsalNfc.h"
#include "phDal4Nfc_messageQueueLib.h"
#include "phOsalNfc_Timer.h"


#ifndef bool
#define bool int
#define  true ((bool)1)
#define  false ((bool)0)
#endif

#ifndef null
#define null ((void*)0)
#endif

#ifndef BOOL
#define BOOL   bool
#endif
#ifndef TRUE
#define TRUE   true
#endif
#ifndef FALSE
#define FALSE  false
#endif


struct nfc_taskq
{
	osa_taskq_cb		f_cb;
	nfc_handle_t			h_cb;
	nfc_u8				b_enabled;
	nfc_handle_t			h_q;
};

struct osa_context
{
	struct nfc_taskq	taskq[E_NCI_Q_MAX];
	nfc_handle_t			queue_cs;			/* global NFC Critical Sections */
	nfc_u32				active_queue_ctr;	/* global enabled NFC queues counter */
};


/* Timer API */
struct nfc_timer
{
	osa_timer_cb		f_cb;
	nfc_handle_t			h_cb;
	uint32_t			h_timer;
	nfc_handle_t			h_osa;
	nfc_u32				id;
};


typedef struct phOsalNfc_scheduleMsg_t
{
	nfc_handle_t			h_osa;
	nfc_u8				que_id;
	nfc_handle_t			p_param;
	void				*defer_msg;
} phOsalNfc_scheduleMsg;



/*******************************************************************************
	Global Routines
*******************************************************************************/
nfc_handle_t osa_create()
{
	nfc_u32 i;
	struct osa_context *p_osa = (struct osa_context *)osa_mem_alloc(sizeof(struct osa_context));
	OSA_ASSERT(p_osa);

	for(i=0; i<E_NCI_Q_MAX; i++)
	{
		p_osa->taskq[i].f_cb = NULL;
		p_osa->taskq[i].h_cb = NULL;
		p_osa->taskq[i].h_q = NULL;
		p_osa->taskq[i].b_enabled = 0;
	}

	p_osa->active_queue_ctr = 0;

	return p_osa;
}

void osa_destroy(nfc_handle_t h_osa)
{
	osa_mem_free(h_osa);
}

void *osa_mem_alloc(nfc_u32 length)
{
	return phOsalNfc_GetMemory(length);
}

void osa_mem_free(void *p_buff)
{
	phOsalNfc_FreeMemory(p_buff);
}

void osa_mem_set(void *p_buff, nfc_u8 value, nfc_u32 length)
{
	memset(p_buff, value, length);
}

void osa_mem_zero(void *p_buff, nfc_u32 length)
{
	memset(p_buff, 0, length);
}

void osa_mem_copy(void *p_dst, void *p_src, nfc_u32 length)
{
	memcpy(p_dst, p_src, length);
}

nfc_status osa_taskq_register(nfc_handle_t h_osa, nfc_u8 que_id, osa_taskq_cb f_cb, nfc_handle_t h_cb, nfc_u8 b_initially_enabled)
{
	struct osa_context *p_osa = (struct osa_context *)h_osa;

	p_osa->taskq[que_id].f_cb = f_cb;
	p_osa->taskq[que_id].h_cb = h_cb;
	p_osa->taskq[que_id].h_q = que_create();
	p_osa->taskq[que_id].b_enabled = b_initially_enabled?true:false;

	return NFC_RES_OK;
}

nfc_status osa_taskq_unregister(nfc_handle_t h_osa, nfc_u8 que_id)
{
	struct osa_context *p_osa = (struct osa_context *)h_osa;

	que_destroy(p_osa->taskq[que_id].h_q);
	p_osa->taskq[que_id].h_q = NULL;
	p_osa->taskq[que_id].b_enabled = false;

	return NFC_RES_OK;
}
static void static_taskq_process(struct osa_context *p_osa) // Empty all enbaled queues and process the callbacks
{
	struct nfc_taskq *p_taskq;
	nfc_handle_t p_param;
	nfc_u32 processing_ctr = 0;

	// Keep processing until all active (enabled) queues are emptied
	while(p_osa->active_queue_ctr)
	{
		processing_ctr = 0;

		// loop through all nci queues
		for(p_taskq = p_osa->taskq; p_osa->active_queue_ctr && (p_taskq != &p_osa->taskq[E_NCI_Q_MAX]); p_taskq++)
		{
			// loop through all queue elements
			do
			{
				/* Now, dequeueing element from the queue shall be protected by queue critical section */

				if(p_taskq->b_enabled)
				{
					p_param = que_dequeue(p_taskq->h_q);

#ifdef TASKQ_DEBUG
					osa_report(INFORMATION,  ("taskq_process %d", p_taskq-g_nfc_taskq));
#endif // TASKQ_DEBUG

					if(p_param)
					{
						if(p_taskq->f_cb)
							p_taskq->f_cb(p_taskq->h_cb, p_param);

						processing_ctr ++;
					}
				}
				else
				{
					p_param = NULL;
				}
			} while (p_param);
		}

		p_osa->active_queue_ctr -= processing_ctr;
	}
}

extern int nDeferedCallMessageQueueId;

static void static_osa_taskq_schedule_DeferredCall(void *params)
{
	phOsalNfc_scheduleMsg *schedule_msg;
	struct osa_context *p_osa;
	struct nfc_taskq *p_taskq;
	nfc_status status;

	if(params == NULL)
		return;

	schedule_msg = (phOsalNfc_scheduleMsg *)params;

	p_osa = (struct osa_context *)schedule_msg->h_osa;
	p_taskq = &p_osa->taskq[schedule_msg->que_id];

	status = que_enqueue(p_taskq->h_q, schedule_msg->p_param);
	OSA_ASSERT(status == NFC_RES_OK);

	if(p_taskq->b_enabled)
	{
		p_osa->active_queue_ctr ++;
		static_taskq_process(p_osa);
	}

	phOsalNfc_FreeMemory(schedule_msg->defer_msg);
	phOsalNfc_FreeMemory(schedule_msg);
}


nfc_status osa_taskq_schedule(nfc_handle_t h_osa, nfc_u8 que_id, nfc_handle_t p_param)
{
	struct osa_context *p_osa = (struct osa_context *)h_osa;
	struct nfc_taskq *p_taskq = &p_osa->taskq[que_id];
	phOsalNfc_scheduleMsg *schedule_msg;
	phLibNfc_DeferredCall_t *defer_msg;
	phDal4Nfc_Message_Wrapper_t wrapper;

#ifdef TASKQ_DEBUG
	osa_report(INFORMATION,  ("taskq_schedule %d", que_id));
#endif

	OSA_ASSERT(que_id < E_NCI_Q_MAX && p_taskq->h_q != NULL);

	schedule_msg = phOsalNfc_GetMemory(sizeof(phOsalNfc_scheduleMsg));
	if(schedule_msg == NULL)
	{
		phOsalNfc_RaiseException(phOsalNfc_e_NoMemory, 0);
	}

	defer_msg = phOsalNfc_GetMemory(sizeof(phLibNfc_DeferredCall_t));
	if(defer_msg == NULL)
	{
		phOsalNfc_FreeMemory(schedule_msg);
		phOsalNfc_RaiseException(phOsalNfc_e_NoMemory, 0);
	}

	schedule_msg->h_osa = h_osa;
	schedule_msg->que_id = que_id;
	schedule_msg->p_param = p_param;
	schedule_msg->defer_msg = defer_msg;

	defer_msg->pCallback = static_osa_taskq_schedule_DeferredCall;
	defer_msg->pParameter = schedule_msg;

	wrapper.mtype = 1;
	wrapper.msg.eMsgType = PH_LIBNFC_DEFERREDCALL_MSG;
	wrapper.msg.pMsgData = defer_msg;
	wrapper.msg.Size = sizeof(phLibNfc_DeferredCall_t);

	phDal4Nfc_msgsnd(nDeferedCallMessageQueueId, (void *)&wrapper,
		sizeof(phLibNfc_Message_t), 0);

	return NFC_RES_OK;
}


void osa_taskq_enable(nfc_handle_t h_osa, nfc_u8 que_id)
{
	struct osa_context *p_osa = (struct osa_context *)h_osa;
	struct nfc_taskq *p_taskq = &p_osa->taskq[que_id];

#ifdef TASKQ_DEBUG
	osa_report(INFORMATION,  ("taskq_enable %d", que_id));
#endif

	OSA_ASSERT(que_id < E_NCI_Q_MAX && p_taskq->h_q != NULL);

	if(p_taskq->b_enabled == false)
	{
		p_osa->active_queue_ctr += que_size(p_taskq->h_q);
		p_taskq->b_enabled = true;
	}
}

void osa_taskq_disable(nfc_handle_t h_osa, nfc_u8 que_id)
{
	struct osa_context *p_osa = (struct osa_context *)h_osa;
	struct nfc_taskq *p_taskq = &p_osa->taskq[que_id];

#ifdef TASKQ_DEBUG
	osa_report(INFORMATION,  ("taskq_disable %d", que_id));
#endif // TASKQ_DEBUG

	if(p_taskq->b_enabled == true)
	{
		p_osa->active_queue_ctr -= que_size(p_taskq->h_q);
		p_taskq->b_enabled = false;
	}
}

/* Timer */
static void os_timer_cb(uint32_t TimerId, void *pContext)
{
	struct nfc_timer *p_timer = (struct nfc_timer*)pContext;

	osa_report(INFORMATION, ("OSA timer %d Callback (timer=%08x osa=%08x)", (int) p_timer->id, (unsigned) p_timer, (unsigned) p_timer->h_osa));

	/* Stop periodic timer */
	phOsalNfc_Timer_Stop(p_timer->h_timer);
	phOsalNfc_Timer_Delete(p_timer->h_timer);

	p_timer->f_cb(p_timer->h_cb);

	osa_mem_free(p_timer);
}

nfc_handle_t osa_timer_start(nfc_handle_t h_osa, nfc_u32 expiry_time, osa_timer_cb f_cb, nfc_handle_t h_cb)
{
	struct nfc_timer *p_timer = (struct nfc_timer*)osa_mem_alloc(sizeof(struct nfc_timer));
	static nfc_u32 ids = 0;
	OSA_ASSERT(p_timer);
	p_timer->f_cb = f_cb;
	p_timer->h_cb = h_cb;
	p_timer->h_osa = h_osa;
	p_timer->id = ids++;

	p_timer->h_timer = phOsalNfc_Timer_Create();
	phOsalNfc_Timer_Start(p_timer->h_timer, expiry_time, os_timer_cb, p_timer);

	osa_report(INFORMATION, ("OSA timer %d Start (timer=%08x osa=%08x h_timer=%08x)", (int) p_timer->id, (unsigned) p_timer, (unsigned) p_timer->h_osa, (unsigned) p_timer->h_timer));

	return p_timer;
}

void osa_timer_stop(void *h_timer)
{
	struct nfc_timer *p_timer = (struct nfc_timer*)h_timer;

	osa_report(INFORMATION, ("OSA timer %d Stop (timer=%08x osa=%08x h_timer=%08x)", (int) p_timer->id, (unsigned) p_timer, (unsigned) p_timer->h_osa, (unsigned) p_timer->h_timer));;

	phOsalNfc_Timer_Stop(p_timer->h_timer);
	phOsalNfc_Timer_Delete(p_timer->h_timer);

	osa_mem_free(h_timer);
}


nfc_u32 osa_time_sec()
{
	// not used
	return 0;
}

nfc_u32 osa_time_msec()
{
	// not used
	return 0;
}


/* Endianess API */
nfc_u16 osa_btoh16(const nfc_u8 *u16)
{
	return (nfc_u16)( ((nfc_u16) *u16 << 8) | ((nfc_u16) *(u16+1)) );
}

void osa_htob16(const nfc_u16 val, nfc_u8 *u16)
{
   u16[0] = (nfc_u8)(val>>8);
   u16[1] = (nfc_u8)val;
}

nfc_u32 osa_btoh32(const nfc_u8 *u32)
{
	return (nfc_u32)( ((nfc_u32) *u32 << 24)   | \
				  ((nfc_u32) *(u32+1) << 16) | \
				  ((nfc_u32) *(u32+2) << 8)  | \
				  ((nfc_u32) *(u32+3)) );
}

void osa_htob32(const nfc_u32 val, nfc_u8 *u32)
{
   u32[0] = (nfc_u8) (val>>24);
   u32[1] = (nfc_u8) (val>>16);
   u32[2] = (nfc_u8) (val>>8);
   u32[3] = (nfc_u8) val;
}

void osa_assert(const char *expression, const nfc_s8 *file, nfc_u16 line)
{
	osa_report(ERROR,  ("osa_assert %s", expression));
}

void osa_exit(const nfc_bool failure)
{
	exit(failure ? EXIT_FAILURE : EXIT_SUCCESS);
}

#define OSA_DATA_PATH           "/data/data/"
#define OSA_PROPS_PATH          "/props/"
#define OSA_PROP_INDEX_FILE     "/index.dat"
#define OSA_PROP_PROP_FILE1     "/prop1.dat"
#define OSA_PROP_PROP_FILE2     "/prop2.dat"

#define MAX_PROCESS_NAME (30)

#define OSA_PROP_MAX_FILENAME_SIZE (    \
	(sizeof(OSA_DATA_PATH) - 1) +       \
	MAX_PROCESS_NAME +                  \
	(sizeof(OSA_PROPS_PATH) - 1) +      \
	MAX_KEY_LEN +                       \
	(sizeof(OSA_PROP_INDEX_FILE) - 1))

/* Calculate CRC and return it */
static nfc_u16 osa_calculate_crc(const nfc_u8* const data, const nfc_u16 len)
{
	int i,j;
	nfc_u16 crc = 0x6363;

	for(i=0; i<len; i++) {
		nfc_u8 byte = data[i];
		for(j=0; j<8; j++) {
			nfc_bool bit = (crc ^ byte) & 1;
			byte >>= 1;
			crc >>= 1;
			if(bit) crc ^= 0x8408;
		}
	}
	return crc;
}

/* Index file info */
typedef struct {
	nfc_u8  curr_prop_file;
	nfc_u16 prop_file_len;
	nfc_u16 prop_file_crc;
} OSA_SETPROP_INDEX;

typedef struct {
	int    index_file;
	int    prop_file[2];
	int    prop_dir;
	OSA_SETPROP_INDEX index;
} OSA_SETPROP_FILE_CONTEXT;

typedef struct {
	/* Current Key Value Files */
	OSA_SETPROP_FILE_CONTEXT files;

	/* Current Key Value Caching */
	nfc_s8  curr_key[MAX_KEY_LEN];
	nfc_u8  curr_buff[MAX_PROP_LEN];
	nfc_u16 curr_buff_len;
} OSA_SETPROP_CONTEXT;

static OSA_SETPROP_CONTEXT osa_setprop_ctx={{0,{0,},0,{0,0,0}},{0,},{0,},0};

static nfc_status osa_get_process_name(nfc_s8 *pname)
{
	static nfc_s8 process_name[MAX_PROCESS_NAME+1]={'\0',};
	nfc_status ret_code = NFC_RES_OK;

	/* Get Process Name */
	if (process_name[0] == '\0')
	{
		pid_t pid = getpid();
		int proc_fd=-1;
		int fstat;

		sprintf((char*)process_name, "/proc/%d/cmdline", (int) pid);
		proc_fd = open((char*)process_name, O_RDONLY);
		if (proc_fd == -1)
		{
			osa_report(ERROR,  ("osa_get_process_name: failed open of %s, stat=0x%X",
				(char*)process_name, errno));
			ret_code = NFC_RES_FILE_ERROR;
			goto Cleanup;
		}

		fstat = read(proc_fd, process_name, MAX_PROCESS_NAME);
		if (fstat == -1)
		{
			osa_report(ERROR,  ("osa_get_process_name: failed reading from %s, stat=0x%X",
				(char*)process_name, errno));
			ret_code = NFC_RES_FILE_ERROR;
			close(proc_fd);
			goto Cleanup;
		}

		osa_report(INFORMATION,  ("osa_get_process_name: name is %s", process_name));

	}

	strcpy((char*)pname, (char*)process_name);

Cleanup:
	return ret_code;
}

static nfc_status osa_update_prop_key(const nfc_s8* const key)
{
	OSA_SETPROP_CONTEXT *ctx = &osa_setprop_ctx;
	OSA_SETPROP_FILE_CONTEXT files={-1,{-1,-1},-1,{0,0,0}};
	nfc_s8 filename[OSA_PROP_MAX_FILENAME_SIZE+1];

	int fstat;
	nfc_u32 pathLen=0;
	nfc_status ret_code = NFC_RES_OK;

	if (key == NULL || key[0] == '\0')
	{
		osa_report(ERROR,  ("osa_update_prop_key: illegal empty key"));
		ret_code = NFC_RES_PARAM_ERROR;
		goto Cleanup;
	}

	if (strlen((char*)key) > MAX_KEY_LEN)
	{
		osa_report(ERROR,  ("osa_update_prop_key: illegal key length, more than %d, key=%s",
			MAX_KEY_LEN, (char*) key));
		ret_code = NFC_RES_PARAM_ERROR;
		goto Cleanup;
	}

	if (strcmp((char*)key, (char*)ctx->curr_key) == 0)
	{
		/* Key was not changed - finish */
		goto Cleanup;
	}

	/* Create process data directories if necessary */
	strcpy((char*) filename, OSA_DATA_PATH);
	ret_code = osa_get_process_name(&filename[strlen((char*)filename)]);
	if (ret_code != NFC_RES_OK)
	{
		/* Failed getting process name */
		osa_report(ERROR,  ("osa_update_prop_key: failed getting process name, stat=0x%X",
			ret_code));

		strcat((char*)filename, "com.android.nfc");
		osa_report(ERROR,  ("osa_update_prop_key: fall back to %s",
			(char*) filename));

		ret_code = NFC_RES_OK;
	}

	if (ctx->curr_key[0] == '\0')
	{
		/* If first time - Create process and props directories if necessary */
		fstat = mkdir((char*)filename, S_IRWXU|S_IRGRP|S_IROTH);
		if (fstat != 0)
		{
			if (errno != EEXIST)
			{
				osa_report(ERROR,  ("osa_update_prop_key: failed creation of %s, stat=0x%X",
					filename, errno));
				ret_code = NFC_RES_FILE_ERROR;
				goto Cleanup;
			}
		}

		strcat((char*)filename, OSA_PROPS_PATH);
		fstat = mkdir((char*)filename, S_IRWXU|S_IRGRP|S_IROTH);
		if (fstat != 0)
		{
			if (errno != EEXIST)
			{
				osa_report(ERROR,  ("osa_update_prop_key: failed creation of %s, stat=0x%X",
					filename, errno));
				ret_code = NFC_RES_FILE_ERROR;
				goto Cleanup;
			}
		}

		/* Sync into file system */
		sync();
	}
	else
	{
		strcat((char*)filename, OSA_PROPS_PATH);
	}

	/* Create Key Directory if necessary */
	strcat((char*)filename, (char*) key);
	fstat = mkdir((char*)filename, S_IRWXU|S_IRGRP|S_IROTH);
	if (fstat != 0)
	{
		if (errno != EEXIST)
		{
			osa_report(ERROR,  ("osa_update_prop_key: failed creation of %s, stat=0x%X",
				(char*) filename, errno));
			ret_code = NFC_RES_FILE_ERROR;
			goto Cleanup;
		}
	}

	files.prop_dir = open((char*)filename, O_DIRECTORY);
	if (files.prop_dir == -1)
	{
		osa_report(ERROR,  ("osa_update_prop_key: failed open of %s, stat=0x%X",
			(char*) filename, errno));
		ret_code = NFC_RES_FILE_ERROR;
		goto Cleanup;
	}

	pathLen = strlen((char*) filename);

	/* Open Index file */
	filename[pathLen]='\0';
	strcat((char*) filename, OSA_PROP_INDEX_FILE);
	files.index_file = open(
		(char*) filename,
		O_RDWR|O_CREAT,
		S_IRWXU|S_IRGRP|S_IROTH);

	if (files.index_file == -1)
	{
		osa_report(ERROR,  ("osa_update_prop_key: failed open of %s, stat=0x%X",
			(char*)filename, errno));
		ret_code = NFC_RES_FILE_ERROR;
		goto Cleanup;
	}

	/* Open Prop file 1 */
	filename[pathLen]='\0';
	strcat((char*) filename, OSA_PROP_PROP_FILE1);
	files.prop_file[0] = open(
		(char*) filename,
		O_RDWR|O_CREAT,
		S_IRWXU|S_IRGRP|S_IROTH);

	if (files.prop_file[0] == -1)
	{
		osa_report(ERROR,  ("osa_update_prop_key: failed open of %s, stat=0x%X",
			(char*) filename, errno));
		ret_code = NFC_RES_FILE_ERROR;
		goto Cleanup;
	}

	/* Open Prop file 2 */
	filename[pathLen]='\0';
	strcat((char*) filename, OSA_PROP_PROP_FILE2);
	files.prop_file[1] = open(
		(char*) filename,
		O_RDWR|O_CREAT,
		S_IRWXU|S_IRGRP|S_IROTH);

	if (files.prop_file[1] == -1)
	{
		osa_report(ERROR,  ("osa_update_prop_key: failed open of %s, stat=0x%X",
			(char*) filename, errno));
		ret_code = NFC_RES_FILE_ERROR;
		goto Cleanup;
	}

	/* Read current prop file info from Index */
	fstat = read(files.index_file, &files.index, sizeof(files.index));
	if (fstat == -1)
	{
		osa_report(ERROR,  ("osa_update_prop_key: failed reading index file, stat=0x%X", errno));
		ret_code = NFC_RES_FILE_ERROR;
		goto Cleanup;
	}
	else if (fstat == 0)
	{
		/* Index file was just created - so no need to update cache */
	}
	else
	{
		/* Index file has the current file - update cache from it */
		fstat = read(files.prop_file[files.index.curr_prop_file], ctx->curr_buff, MAX_PROP_LEN);
		if (fstat == -1)
		{
			osa_report(ERROR,  ("osa_update_prop_key: failed reading prop file[%d], stat=0x%X",
				(int) files.index.curr_prop_file, errno));
			ret_code = NFC_RES_FILE_ERROR;
			goto Cleanup;
		}

		/* Verify that information is valid */
		if (fstat == files.index.prop_file_len &&
			osa_calculate_crc(ctx->curr_buff, fstat) == files.index.prop_file_crc)
		{
			/* File is valid - use file information */
		}
		else
		{
			/* File info is not valid - empty buffer */
			osa_report(ERROR,  ("osa_update_prop_key: CRC failed on prop file[%d], empty buffer",
				(int) files.index.curr_prop_file));

			fstat = 0;
		}
	}

	/* All is successful - update rest of the context */

	/* 1 - curr_buff was updated in the successful read above */

	/* 2 - Update curr_buff_len */
	ctx->curr_buff_len = fstat;

	/* 3 - Update files */
	/* If we had a key before - close previous files */
	if (ctx->curr_key[0] != '\0')
	{
		/* If key changed close current files */
		close(ctx->files.prop_dir);
		close(ctx->files.index_file);
		close(ctx->files.prop_file[0]);
		close(ctx->files.prop_file[1]);
	}
	ctx->files = files;

	/* 3 - Update curr_key */
	strcpy((char*) ctx->curr_key, (char*) key);

Cleanup:
	if (ret_code != NFC_RES_OK)
	{
		if (files.prop_dir     != -1) close(files.prop_dir);
		if (files.index_file   != -1) close(files.index_file);
		if (files.prop_file[0] != -1) close(files.prop_file[0]);
		if (files.prop_file[1] != -1) close(files.prop_file[1]);
	}

	return ret_code;
}

static nfc_status osa_update_prop_file(nfc_u8* buff, nfc_u16 buff_len, OSA_SETPROP_INDEX next_prop_index)
{
	OSA_SETPROP_CONTEXT *ctx = &osa_setprop_ctx;
	nfc_status ret_code = NFC_RES_OK;
	int fstat;

	if (buff_len > 0)
	{
		fstat = lseek(ctx->files.prop_file[next_prop_index.curr_prop_file], SEEK_SET, 0);
		if (fstat == -1)
		{
			osa_report(ERROR,  ("osa_update_prop_file: failed lseek next prop file[%d], stat=0x%X",
				(int) next_prop_index.curr_prop_file, errno));
			ret_code = NFC_RES_FILE_ERROR;
			goto Cleanup;
		}

		fstat = write(ctx->files.prop_file[next_prop_index.curr_prop_file], buff, buff_len);
		if (fstat == -1)
		{
			osa_report(ERROR,  ("osa_update_prop_file: failed write to next prop file[%d], stat=0x%X",
				(int) next_prop_index.curr_prop_file, errno));
			ret_code = NFC_RES_FILE_ERROR;
			goto Cleanup;
		}
	}

	fstat = ftruncate(ctx->files.prop_file[next_prop_index.curr_prop_file], buff_len);
	if (fstat == -1)
	{
		osa_report(ERROR,  ("osa_update_prop_file: failed ftruncate to next prop file[%d], stat=0x%X",
			(int) next_prop_index.curr_prop_file, errno));
		ret_code = NFC_RES_FILE_ERROR;
		goto Cleanup;
	}

	fstat = lseek(ctx->files.index_file, SEEK_SET, 0);
	if (fstat == -1)
	{
		osa_report(ERROR,  ("osa_update_prop_file: failed lseek index_file, stat=0x%X",
			errno));
		ret_code = NFC_RES_FILE_ERROR;
		goto Cleanup;
	}

	fstat = write(ctx->files.index_file, &next_prop_index, sizeof(next_prop_index));
	if (fstat == -1)
	{
		osa_report(ERROR,  ("osa_update_prop_file: failed write to index_file, stat=0x%X",
			errno));
		ret_code = NFC_RES_FILE_ERROR;
		goto Cleanup;
	}

	fstat = fsync(ctx->files.prop_file[next_prop_index.curr_prop_file]);
	if (fstat == -1)
	{
		osa_report(ERROR,  ("osa_update_prop_file: failed fsync to next prop file[%d], stat=0x%X",
			(int) next_prop_index.curr_prop_file, errno));
		ret_code = NFC_RES_FILE_ERROR;
		goto Cleanup;
	}

	fstat = fsync(ctx->files.index_file);
	if (fstat == -1)
	{
		osa_report(ERROR,  ("osa_update_prop_file: failed fsync to index_file, stat=0x%X",
			errno));
		ret_code = NFC_RES_FILE_ERROR;
		goto Cleanup;
	}

	fstat = fsync(ctx->files.prop_dir);
	if (fstat == -1)
	{
		osa_report(ERROR,  ("osa_update_prop_file: failed fsync to prop_dir, stat=0x%X",
			errno));
		ret_code = NFC_RES_FILE_ERROR;
		goto Cleanup;
	}

Cleanup:
	return ret_code;
}

nfc_status osa_setprop(const nfc_s8* const key, nfc_u8* buff, nfc_u16 buff_len)
{
	OSA_SETPROP_CONTEXT *ctx = &osa_setprop_ctx;
	OSA_SETPROP_INDEX next_prop_index;
	nfc_status ret_code = NFC_RES_OK;

	if (buff_len > 0 && buff == NULL)
	{
		osa_report(ERROR,  ("osa_setprop: illegal empty buff"));
		ret_code = NFC_RES_PARAM_ERROR;
		goto Cleanup;
	}

	ret_code = osa_update_prop_key(key);
	if (ret_code != NFC_RES_OK)
	{
		goto Cleanup;
	}

	next_prop_index.curr_prop_file = 1 - ctx->files.index.curr_prop_file;
	next_prop_index.prop_file_len = buff_len;
	next_prop_index.prop_file_crc = osa_calculate_crc(buff, buff_len);

	ret_code = osa_update_prop_file(buff, buff_len, next_prop_index);
	if (ret_code != NFC_RES_OK)
	{
		goto Cleanup;
	}

	/* Update Cache */
	if (buff_len > 0)
	{
		memcpy(ctx->curr_buff, buff, buff_len);
	}
	ctx->curr_buff_len = buff_len;
	ctx->files.index = next_prop_index;

Cleanup:
	return ret_code;
}

nfc_status osa_getprop(const nfc_s8* const key, nfc_u8* buff, nfc_u16* buff_len)
{
	OSA_SETPROP_CONTEXT *ctx = &osa_setprop_ctx;

	nfc_status ret_code = NFC_RES_OK;

	if (buff_len == NULL)
	{
		osa_report(ERROR,  ("osa_getprop: illegal empty buff_len"));
		ret_code = NFC_RES_PARAM_ERROR;
		goto Cleanup;
	}

	if (*buff_len > 0 && buff == NULL)
	{
		osa_report(ERROR,  ("osa_getprop: illegal empty buff"));
		ret_code = NFC_RES_PARAM_ERROR;
		goto Cleanup;
	}

	ret_code = osa_update_prop_key(key);
	if (ret_code != NFC_RES_OK)
	{
		goto Cleanup;
	}

	if (*buff_len < ctx->curr_buff_len)
	{
		osa_report(ERROR,  ("osa_getprop: return buff truncated from %d to %d",
			(int) ctx->curr_buff_len,
			(int) *buff_len));
		ret_code = NFC_RES_TRUNCATED;
	}
	else
	{
		*buff_len = ctx->curr_buff_len;
	}

	memcpy(buff, ctx->curr_buff, *buff_len);

Cleanup:
	return ret_code;
}

/* EOF */
