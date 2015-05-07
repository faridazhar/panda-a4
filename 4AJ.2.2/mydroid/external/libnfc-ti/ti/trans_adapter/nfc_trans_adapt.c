/*
 * nfc_trans_adapt.c
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

  Implementation of the NCI Trnasport adapter (Platfrom specific) over NFC driver.


*******************************************************************************/
#include "nfc_types.h"
#include "nci_dev.h"
#include "nfc_os_adapt.h"
#include "nfc_trans_adapt.h"

#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <poll.h>
#include <string.h>


#define NFC_DEVICE_NAME      "/dev/nfc"
#define NFC_MAX_BUF_SIZE     300
#define NFC_POLL_TIMEOUT     10000


#define NCI_TRANS_DEBUG
#define LOG_TAG              "NFC_TRANS_ADAPT"

static struct trans_context
{
	struct ncidev		ncidev;
	int			        nfc_fd;
	int			        close_fd[2];
	pthread_t		    rx_tid;
	int                 rx_thread_running;
};


static void* rx_thread(void *arg)
{
	struct trans_context *p_trans = (struct trans_context*)arg;
	struct nci_buff *p_nci_buffer = 0;
	struct pollfd fdarray[2];
	int rc;
	ssize_t length;

	/* setup the rx waiting on the NFC device */
	fdarray[0].fd = p_trans->nfc_fd;
	fdarray[0].events = POLLIN;

	/* setup the rx waiting on the 'close' pipe */
	fdarray[1].fd = p_trans->close_fd[0];
	fdarray[1].events = POLLIN;

	while (p_trans->rx_thread_running)
	{
		/* wait for rx or timeout */
		rc = poll(fdarray, 2, NFC_POLL_TIMEOUT);
		if (rc <= 0)    /* timeout or error */
		{
			osa_report(ERROR, ("rx_thread: poll returned %d, errno %s", rc, strerror(errno)));
			continue;
		}

		/* check if we've rx data to read from the NFC device */
		if ((p_trans->rx_thread_running) &&
			(fdarray[0].revents & POLLIN))
		{
			/* if needed, allocate a new nci_buff for rx packet */
			if (!p_nci_buffer)
			{
				p_nci_buffer = nci_buff_alloc(NFC_MAX_BUF_SIZE);
				if (!p_nci_buffer)
				{
					osa_report(ERROR, ("rx_thread: failed to alloc nci_buff"));
					continue;
				}
			}

			/* read a COMPLETE packet from the NFC device */
			length = read(p_trans->nfc_fd, nci_buff_data(p_nci_buffer), NFC_MAX_BUF_SIZE);

			if (length > 0)
			{
				/* trim the suffix */
				nci_buff_cut_suffix(p_nci_buffer, (NFC_MAX_BUF_SIZE-length));

#ifdef NCI_TRANS_DEBUG
				osa_report(INFORMATION,  ("<--RX length = %d",  nci_buff_length(p_nci_buffer)));
				ncidev_log(&p_trans->ncidev, p_nci_buffer, "<--RX ");
#endif /* NCI_TRANS_DEBUG */

				/* forward the packet to the upper layer of ncidev */
				ncidev_receive(&p_trans->ncidev, p_nci_buffer);

				/* zero the nci_buff, since it was passed to upper layer */
				p_nci_buffer = 0;
			}
			else
			{
				osa_report(ERROR, ("rx_thread: read returned %d, errno %s", length, strerror(errno)));
			}
		}
	}

	/* cleanup any remaining nci_buff */
	if (p_nci_buffer)
	{
		nci_buff_free(p_nci_buffer);
	}

	return ((void*)0);
}


nfc_status trans_send(nfc_handle_t h_trans, struct nci_buff *p_buff)
{
	nfc_status rc = NFC_RES_OK;
	struct trans_context *p_trans = (struct trans_context*)h_trans;

#ifdef NCI_TRANS_DEBUG
	osa_report(INFORMATION,  ("TX--> length = %d", nci_buff_length(p_buff)));
	ncidev_log(&p_trans->ncidev, p_buff, "TX--> ");
#endif /* NCI_TRANS_DEBUG */

	if (write(p_trans->nfc_fd, nci_buff_data(p_buff), nci_buff_length(p_buff)) < 0)
	{
		osa_report(ERROR, ("trans_send: failed to write to NFC device, errno %s", strerror(errno)));
		rc = NFC_RES_ERROR;
	}

	return rc;
}


nfc_status trans_open(nfc_handle_t h_trans)
{
	struct trans_context *p_trans = (struct trans_context*)h_trans;
	int rc;

	/* open the NFC device driver file */
	p_trans->nfc_fd = open(NFC_DEVICE_NAME, O_RDWR);
	if (p_trans->nfc_fd < 0)
	{
		osa_report(ERROR, ("trans_open: failed to open NFC device, errno %s", strerror(errno)));

		return NFC_RES_ERROR;
	}

	/* create the 'close' pipe */
	rc = pipe(p_trans->close_fd);
	if (rc != 0)
	{
		osa_report(ERROR, ("trans_open: failed to create close pipe, errno %s", strerror(errno)));

		close(p_trans->nfc_fd);
		return NFC_RES_ERROR;
	}

	p_trans->rx_thread_running = 1;

	/* create rx thread */
	rc = pthread_create(&p_trans->rx_tid,
						NULL,
						rx_thread,
						(void*)p_trans);
	if (rc != 0)
	{
		osa_report(ERROR, ("trans_open: failed to create RX thread"));

		p_trans->rx_thread_running = 0;
		close(p_trans->close_fd[0]);
		close(p_trans->close_fd[1]);
		close(p_trans->nfc_fd);
		return NFC_RES_ERROR;
	}

	return NFC_RES_OK;
}


nfc_status	trans_close(nfc_handle_t h_trans)
{
	struct trans_context *p_trans = (struct trans_context*)h_trans;

	/* terminate the rx thread */
	p_trans->rx_thread_running = 0;

	/* wakeup rx thread via closing the 'close' pipe */
	close(p_trans->close_fd[0]);
	close(p_trans->close_fd[1]);

	/* wait fot the rx thread to exit */
	pthread_join(p_trans->rx_tid, NULL);

	/* now close the NFC device driver file */
	if (close(p_trans->nfc_fd) < 0)
	{
		osa_report(ERROR, ("trans_close: failed to close NFC device, errno %s", strerror(errno)));
	}

	return NFC_RES_OK;
}


nfc_handle_t transa_create(nfc_handle_t h_osa)
{
	nfc_status rc;
	/* Initiate dev-id = 0 context */
	struct trans_context *p_trans = (struct trans_context*)osa_mem_alloc(sizeof(struct trans_context));
	osa_mem_zero((void*)p_trans, sizeof(struct trans_context));

	p_trans->ncidev.h_trans = p_trans;
	p_trans->ncidev.h_osa = h_osa;
	p_trans->ncidev.dev_id = 0;
	p_trans->ncidev.open = trans_open;
	p_trans->ncidev.close = trans_close;
	p_trans->ncidev.send = trans_send;
	rc = ncidev_register(&p_trans->ncidev);
	OSA_ASSERT(rc == NFC_RES_OK);

	return (nfc_handle_t)p_trans;

}


void transa_destroy(nfc_handle_t h_trans)
{
	struct trans_context *p_trans = (struct trans_context*)h_trans;

	ncidev_unregister(&p_trans->ncidev);
	osa_mem_free(p_trans);
}


/* EOF */
