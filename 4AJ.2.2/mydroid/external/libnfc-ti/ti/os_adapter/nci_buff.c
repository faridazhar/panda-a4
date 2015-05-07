/*
 * nci_buff.c
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

  This source file contains the implementation of the NCI (data) buffer

*******************************************************************************/
/*******************************************************************************

  Implementation of the linux server porting

*******************************************************************************/
#include "nfc_types.h"
#include "nfc_os_adapt.h"
#include "nci_buff.h"


#define NCI_BUFF_DBG

#define PREFIX_SIZE 12
#define SUFFIX_SIZE 4

struct nci_buff
{
	nfc_u8				*p_buff;
	nfc_u32				buff_len;
	nfc_u8				*p_data;
	nfc_u32				data_len;
	struct nci_buff		*p_next;
};



/*******************************************************************************
	Global Routines
*******************************************************************************/
struct nci_buff * nci_buff_alloc(nfc_u32 length)
{
	struct nci_buff *p_this = (struct nci_buff*)osa_mem_alloc(sizeof(struct nci_buff));

	if(p_this)
	{
		p_this->buff_len = PREFIX_SIZE + length + SUFFIX_SIZE;
		p_this->p_buff = (nfc_u8*)osa_mem_alloc(p_this->buff_len);
		if(p_this->p_buff)
		{
			p_this->data_len = length;
			p_this->p_data = p_this->p_buff + PREFIX_SIZE;
			osa_mem_zero(p_this->p_data, length);
			p_this->p_next = NULL;
		}
		else
		{
			osa_mem_free(p_this);
			p_this = NULL;
		}
	}
	return p_this;
};

nfc_u8 *nci_buff_data(struct nci_buff *p_this)
{
	return p_this->p_data;
}

nfc_u32 nci_buff_length(struct nci_buff *p_this)
{
	return p_this->data_len;
}

nfc_u8 *nci_buff_add_prefix(struct nci_buff *p_this, nfc_u32 length)
{
	/* maintain buffer boundaries */
	if(p_this->p_buff > p_this->p_data - length)
	{
		return NULL;
	}
	p_this->p_data -= length;
	p_this->data_len += length;
	osa_mem_zero(p_this->p_data, length);
	return p_this->p_data;
}

nfc_u8 *nci_buff_cut_prefix(struct nci_buff *p_this, nfc_u32 length)
{
	/* maintain buffer boundaries */
	if(p_this->data_len < length)
	{
		return NULL;
	}
#ifdef NCI_BUFF_DBG
	osa_mem_set(p_this->p_data, 0xcd, length);
#endif /* NCI_BUFF_DBG */
	p_this->p_data += length;
	p_this->data_len -= length;
	return p_this->p_data;
}

nfc_u8 *nci_buff_add_suffix(struct nci_buff *p_this, nfc_u32 length)
{
	/* maintain buffer boundaries */
	if(p_this->p_data + p_this->data_len + length > p_this->p_buff + p_this->buff_len)
	{
		return NULL;
	}
	osa_mem_zero(p_this->p_data+p_this->data_len, length);
	p_this->data_len += length;
	return p_this->p_data;
}

nfc_u8 *nci_buff_cut_suffix(struct nci_buff *p_this, nfc_u32 length)
{
	/* maintain buffer boundaries */
	if(length > p_this->data_len)
	{
		return NULL;
	}
	p_this->data_len -= length;
#ifdef NCI_BUFF_DBG
	osa_mem_set(&p_this->p_data[p_this->data_len], 0xcd, length);
#endif /* NCI_BUFF_DBG */

	return p_this->p_data;
}

struct nci_buff *nci_buff_append(struct nci_buff *p_this, struct nci_buff *p_appended_buff)
{
	if(p_this == NULL)
	{
		p_this = p_appended_buff;
	}
	else
	{
		struct nci_buff *p_temp = p_this;
		while(p_temp->p_next)
			p_temp = p_temp->p_next;
		p_temp->p_next = p_appended_buff;
	}
	return p_this;
}

nfc_u32	nci_buff_copy(nfc_u8 *p_dest, struct nci_buff *p_src_buff, nfc_u32 dest_len)
{
	nfc_u32 total_len = 0;
	while(p_src_buff && (total_len + nci_buff_length(p_src_buff) <= dest_len))
	{
		osa_mem_copy(p_dest, nci_buff_data(p_src_buff), nci_buff_length(p_src_buff));
		p_dest += nci_buff_length(p_src_buff);
		p_src_buff = p_src_buff->p_next;
	}
	return total_len;
}


void	nci_buff_free(struct nci_buff *p_this)
{

	while(p_this)
	{
		struct nci_buff *p_next = p_this->p_next;
		osa_mem_free(p_this->p_buff);
		osa_mem_free(p_this);
		p_this = p_next;
	}
}



nfc_u8 nci_buff_get_fragments(struct nci_buff *p_this, nfc_u8 max_frag_num, nfc_u8 *p_frag[], nfc_u32 frag_len[])
{
	OSA_UNUSED_PARAMETER(max_frag_num);

	p_frag[0] = p_this->p_data;
	frag_len[0] = p_this->data_len;
	return 1;
}

void nci_buff_dump(struct nci_buff *p_this, const nfc_s8 *p_str)
{
	osa_dump_buffer(p_str,  p_this->p_data, p_this->data_len);
}




/* EOF */
