/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
** Yngve Pettersen
*/
#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)
#include "modules/libssl/sslbase.h"

SSL_LoadAndWritable_list::SSL_LoadAndWritable_list(DataStream_FlexibleSequence &payld, uint32 len_size, uint32 max_len)
: payload(payld), max_length(max_len)
{
	/* If this triggers: Adjust your len_size or max_len to a legal value */
	OP_ASSERT( max_len < ((uint32) 1<< (len_size*8))); //"SSL_LoadAndWritable_list::SSL_LoadAndWritable_list: Max_len for the structure is too large! Adjust the definition");

	spec.enable_tag = FALSE;
	spec.enable_length = TRUE;
	spec.length_len = len_size;
}

SSL_LoadAndWritable_list::~SSL_LoadAndWritable_list()
{
}

#if 0
void SSL_LoadAndWritable_list::Resize(uint32 nlength)
{
	SSL_TRAP_AND_RAISE_ERROR_THIS(payload.ResizeL(nlength));
}

BOOL SSL_LoadAndWritable_list::Contain(DataStream &src)
{
	OP_MEMORY_VAR BOOL ret = FALSE;
	SSL_TRAP_AND_RAISE_ERROR_THIS(ret = payload.ContainL(src));

	return ret;
}

BOOL SSL_LoadAndWritable_list::Contain(DataStream &source, uint32& index)
{
	OP_MEMORY_VAR BOOL ret = FALSE;
	SSL_TRAP_AND_RAISE_ERROR_THIS(ret = payload.ContainL(source, index));

	return ret;
}

void SSL_LoadAndWritable_list::Copy(SSL_LoadAndWritable_list &source)
{
	SSL_TRAP_AND_RAISE_ERROR_THIS(payload.CopyL(source.payload));
}

void SSL_LoadAndWritable_list::Transfer(uint32 start, SSL_LoadAndWritable_list &source,uint32 start1, uint32 len)
{
	SSL_TRAP_AND_RAISE_ERROR_THIS(payload.TransferL(start, source.payload, start1, len));
}
#endif

void SSL_LoadAndWritable_list::CopyCommon(SSL_LoadAndWritable_list &ordered_list, SSL_LoadAndWritable_list &master_list, BOOL reverse)
{
	SSL_TRAP_AND_RAISE_ERROR_THIS(payload.CopyCommonL(ordered_list.payload, master_list.payload, reverse));
}
#endif
