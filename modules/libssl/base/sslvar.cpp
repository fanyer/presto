/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)
#include "modules/libssl/sslbase.h"

#include "modules/formats/url_dfr.h"

SSL_varvector32::SSL_varvector32(uint32 mlen,uint32 lensize)
{
	Init(mlen,lensize); 
}

void SSL_varvector32::Init(uint32 mlen,uint32 lensize)
{ 
	SetNeedDirectAccess(TRUE);
	SetDontAllocExtra(TRUE);
	spec.enable_tag = FALSE;
	spec.length_len = lensize;
	alloc_block_size=16;
	
	maxlength=mlen;
	if(maxlength > 0xffff)
		maxlength = 0xffff;
}

void SSL_varvector32::SetLegalMax(uint32 mlen)
{
	// Conditional, may not exceed maximum allowed vector size
	uint32 lensize = spec.length_len;
	if(lensize == 4 || mlen < (1U << (lensize * 8)))
		maxlength = mlen;
}

void SSL_varvector32::FixedLoadLength(BOOL fix)
{
	spec.enable_length =  !fix;
	SetFixedSize(fix);
}

void SSL_varvector32::ForceVectorLengthSize(uint32 lensize,uint32 mlen)
{
	spec.length_len = lensize;
	maxlength = mlen;
}


SSL_varvector32::SSL_varvector32()
{
	Init(0xffffffff - 4, 4); 
}

SSL_varvector32::SSL_varvector32(const SSL_varvector32 &old)
: SSL_Error_Status(), DataStream_GenericRecord_Small()
{
	Init(old.maxlength, 4);
	alloc_block_size = old.alloc_block_size;
	operator =(old);
}

SSL_varvector32::~SSL_varvector32()
{
	Resize(0);
}

OP_STATUS SSL_varvector32::PerformStreamActionL(DataStream::DatastreamStreamAction action, DataStream *src_target)
{
	OP_MEMORY_VAR OP_STATUS op_err= OpStatus::OK;
	TRAPD(op_err1, op_err =DataStream_GenericRecord_Small::PerformStreamActionL(action, src_target));
	if(OpStatus::IsError(op_err1))
	{
		RaiseAlert(op_err1);
		LEAVE(op_err1);
	}
	return op_err;
}

SSL_varvector32 &SSL_varvector32::operator =(const SSL_varvector32 &old)
{
	Set(old.GetDirect(), old.GetLength());

	return *this;
}

void SSL_varvector32::Set(DataStream *src)
{
	Resize(0);
	if(src)
	{
		SSL_TRAP_AND_RAISE_ERROR_THIS(src->WriteRecordPayloadL(this));
	}
}

BOOL SSL_varvector32::operator ==(const SSL_varvector32 &other) const
{
	if(other.GetLength() != GetLength())
		return FALSE;
	
	if(GetLength())
	{
		OP_ASSERT(other.GetDirect() && GetDirect());
		if(op_memcmp((void *) other.GetDirect(), (void *) GetDirect(),(size_t) GetLength()))
			return FALSE;
	} 
    
	return TRUE;
}

const byte *SSL_varvector32::SetL(const byte *source,uint32 len)
{
	ResetRecord();
	AddContentL(source, len);

	return source + len;
}

const byte *SSL_varvector32::Set(const byte *source,uint32 len)
{
	const byte * OP_MEMORY_VAR ret_source = NULL; 
	TRAPD(op_err, ret_source = SetL(source, len));
	if(OpStatus::IsError(op_err))
	{
		RaiseAlert(op_err);
		ret_source = source + len;
	}
	
	return ret_source;
}

void SSL_varvector32::Set(const char *source)
{
	Set((const byte *) source, op_strlen(source));
}

void SSL_varvector32::SetExternal(byte *exdata)
{
	SetExternalPayload(exdata);
}

void SSL_varvector32::Resize(uint32 nsize)
{ 
	SSL_TRAP_AND_RAISE_ERROR_THIS(ResizeL(nsize, TRUE, TRUE));
}

#ifdef SSL_FULL_VARVECTOR_SUPPORT
static byte SSL_varvector32_def_item;

byte &SSL_varvector32::operator [](uint32 item)
{
	if(item < GetLength())
		return GetDirect()[item];
	else
	{
		SSL_varvector32_def_item = 0;
		return SSL_varvector32_def_item;
	}
}

const byte &SSL_varvector32::operator [](uint32 item) const
{
	if(item < GetLength())
		return GetDirect()[item];
	else
	{
		SSL_varvector32_def_item=0;
		return SSL_varvector32_def_item;
	}
}
#endif

void SSL_varvector32::Blank(byte val)
{
	if(GetLength()>0 && GetDirect())
		op_memset(GetDirect(),val,(size_t) GetLength());
} 

void SSL_varvector32::Concat(SSL_varvector32 &first,SSL_varvector32 &second)
{
	SSL_TRAP_AND_RAISE_ERROR_THIS(ConcatL(first, second));
}

void SSL_varvector32::ConcatL(SSL_varvector32 &first,SSL_varvector32 &second)
{
	SetWritePos(0);
	first.SetReadPos(0);
	second.SetReadPos(0);
	AddContentL(&first);
	AddContentL(&second);
}

#if defined _DEBUG && defined YNP_WORK
void SSL_varvector32::Append(SSL_varvector32 &other)
{
	other.SetReadPos(0);
	SSL_TRAP_AND_RAISE_ERROR_THIS(AddContentL(&other));
}
#endif


const byte * SSL_varvector32::Append(const byte *source, uint32 len2)
{
	SSL_TRAP_AND_RAISE_ERROR_THIS(AddContentL(source, len2));

	return source + len2;
}

void SSL_varvector32::Append(const char *source)
{
	Append((const byte *) source,(source != NULL ? op_strlen(source) : 0));
}

#endif
