/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef HAS_SET_HTTP_DATA
#include "modules/upload/upload.h"

#include "modules/olddebug/tstdump.h"

#include "modules/util/cleanse.h"
#include "modules/encodings/utility/opstring-encodings.h"

Upload_BinaryBuffer::Upload_BinaryBuffer()
{
	binary_buffer = NULL;
	binary_len = 0;
	binary_pos = 0;
	own_buffer = FALSE;
}

Upload_BinaryBuffer::~Upload_BinaryBuffer()
{
	if(binary_buffer && own_buffer)
	{
		OPERA_cleanse_heap(binary_buffer, binary_len);
		OP_DELETEA(binary_buffer);
	}

	binary_buffer = NULL;
	binary_len = 0;
	binary_pos = 0;
	own_buffer = FALSE;
}

void Upload_BinaryBuffer::InitL(unsigned char *buf, uint32 buf_len, Upload_Buffer_Ownership action, const OpStringC8 &content_type, const OpStringC8 &encoded_charset, MIME_Encoding enc, const char **header_names)
{
	if(binary_buffer && own_buffer)
	{
		OPERA_cleanse_heap(binary_buffer, binary_len);
		OP_DELETEA(binary_buffer);
	}

	Upload_Handler::InitL(enc, header_names);
	SetContentTypeL(content_type);
	SetCharsetL(encoded_charset);
 
	binary_buffer = NULL;
	binary_len = 0;
	binary_pos = 0;
	own_buffer = TRUE;

	switch(action)
	{
	case UPLOAD_REFERENCE_BUFFER:
		own_buffer = FALSE;
		// fall-through
	case UPLOAD_TAKE_OVER_BUFFER:
		binary_buffer = buf;
		binary_len = buf_len;
		binary_pos = 0;
		break;
		
	case UPLOAD_COPY_EXTRACT_HEADERS:
		if(buf && buf_len)
		{
			uint32 header_len = ExtractHeadersL(buf, buf_len, FALSE,(content_type.HasContent() ? HEADER_REMOVE_HTTP_CONTENT_TYPE :  HEADER_REMOVE_HTTP));

			// Check if we consumed all in buf
			if (header_len > buf_len) {
				OP_ASSERT(!"Should not end up here");			   
				header_len = buf_len;
			}

			if(header_len)
			{
				buf_len -= header_len;
				buf += header_len;
			}
		}
		// fall-through
	case UPLOAD_COPY_BUFFER:
		if(buf && buf_len)
		{
			binary_buffer = OP_NEWA_L(unsigned char, buf_len);
			
			op_memcpy(binary_buffer, buf, buf_len);
			
			binary_len = buf_len;
		}
		break;
	}

	if(binary_len > UPLOAD_LONG_BINARY_BUFFER_SIZE)
		SetMinimumBufferSize(UPLOAD_MIN_BUFFER_SIZE_WHEN_LONG);
}

void Upload_BinaryBuffer::ResetL()
{
	Upload_Handler::ResetL();
	binary_pos = 0;
}

void Upload_BinaryBuffer::Release()
{
	if(!own_buffer)
	{
		binary_buffer = NULL;

		binary_len = 0;
		binary_pos = 0;
	}
}

uint32 Upload_BinaryBuffer::GetNextContentBlock(unsigned char *buf, uint32 max_len, BOOL &more)
{
	more = FALSE;
	if(binary_buffer == NULL || binary_len == 0 || binary_pos== binary_len)
		return 0;

	uint32 len = max_len;

	if(binary_pos + len > binary_len)
		len = binary_len - binary_pos;

	op_memcpy(buf, binary_buffer + binary_pos, len);
	binary_pos += len;

	if(binary_pos < binary_len)
		more = TRUE;

	return len;
}

void Upload_BinaryBuffer::ResetContent()
{
	binary_pos = 0;
}

OpFileLength Upload_BinaryBuffer::PayloadLength()
{
	return binary_len;
}

Upload_OpString8::Upload_OpString8()
{
}

Upload_OpString8::~Upload_OpString8()
{
}

void Upload_OpString8::InitL(const OpStringC8 &value, const OpStringC8 &content_type, const OpStringC8 &encoded_charset, MIME_Encoding enc, const char **header_names)
{
	Upload_BinaryBuffer::InitL((unsigned char *) value.CStr(), value.Length(), UPLOAD_COPY_BUFFER, content_type, encoded_charset, enc, header_names);
}


#ifdef UPLOAD2_OPSTRING16_SUPPORT
Upload_OpString16::Upload_OpString16()
{
}

Upload_OpString16::~Upload_OpString16()
{
}

void Upload_OpString16::InitL(const OpStringC16 &value, const OpStringC8 &content_type, const OpStringC8 &convert_to_charset, MIME_Encoding enc, const char **header_names)
{
	if(value.IsEmpty() || convert_to_charset.IsEmpty() || convert_to_charset.CompareI("utf-16") == 0)
		Upload_BinaryBuffer::InitL((unsigned char *) value.CStr(), UNICODE_SIZE(value.Length()), UPLOAD_COPY_BUFFER, content_type, convert_to_charset, enc, header_names);

	OpString8 buffer;
	ANCHOR(OpString8, buffer);

	SetToEncodingL(&buffer, convert_to_charset, value);

	Upload_BinaryBuffer::InitL((unsigned char *) buffer.CStr(), buffer.Length(), UPLOAD_COPY_BUFFER, content_type, convert_to_charset, enc, header_names);
}
#endif

#ifdef UPLOAD2_EXTERNAL_BUFFER_SUPPORT
Upload_ExternalBuffer::Upload_ExternalBuffer()
{
	release_desc = NULL;
}

Upload_ExternalBuffer::~Upload_ExternalBuffer()
{
	Release();
}

void Upload_ExternalBuffer::InitL(unsigned char *buf, uint32 buf_len, Upload_ExternalBuffer_ReleaseDescriptor *release, const OpStringC8 &content_type, const OpStringC8 &charset, MIME_Encoding enc, const char **header_names)
{
	release_desc = NULL;
	Upload_BinaryBuffer::InitL(buf, buf_len, UPLOAD_REFERENCE_BUFFER, content_type,charset, enc, header_names);

	release_desc = release;
}

void Upload_ExternalBuffer::Release()
{
	Upload_BinaryBuffer::Release();
	if(release_desc)
		release_desc->Release();
}

Upload_ExternalMultiBuffer::Upload_ExternalMultiBuffer()
{
	release_desc = NULL;
	buffer_list = NULL;
	buffer_list_parts = 0;
	buffer_item = 0;
	buffer_pos = 0;
}

Upload_ExternalMultiBuffer::~Upload_ExternalMultiBuffer()
{
	Release();
}

void Upload_ExternalMultiBuffer::InitL(IOVector_t *buf_list, uint32 parts, 
									   Upload_ExternalBuffer_ReleaseDescriptor *release,
									   OpStringC8 content_type, OpStringC8 charset, 
									   MIME_Encoding enc, const char **header_names)
{
	release_desc = NULL;
	Upload_Handler::InitL(enc, header_names);
	SetContentTypeL(content_type);
	SetCharsetL(charset);

	buffer_list = buf_list;
	buffer_list_parts = parts;
	release_desc = release;
}

void Upload_ExternalMultiBuffer::ResetL()
{
	ResetContent();

	Upload_Handler::ResetL();
}

void Upload_ExternalMultiBuffer::Release()
{
	buffer_list = NULL;
	buffer_list_parts = 0;
	Upload_Handler::Release();
	if(release_desc)
		release_desc->Release();
}

uint32 Upload_ExternalMultiBuffer::GetNextContentBlock(unsigned char *buf, uint32 max_len, BOOL &more)
{
	more = FALSE;
	if(buffer_list == NULL || buffer_list_parts == 0 || buffer_item >= buffer_list_parts)
		return 0;

	uint32 len = 0;

	while(len < max_len && buffer_item < buffer_list_parts)
	{
		uint32 blen = max_len - len;

		if(buffer_list[buffer_item].Base == NULL)
		{
			buffer_item++;
			buffer_pos = 0;
			continue;
		}

		if(blen > buffer_pos + buffer_list[buffer_item].Length)
			blen = buffer_list[buffer_item].Length - buffer_pos;

		op_memcpy(buf, ((unsigned char *) buffer_list[buffer_item].Base) + buffer_pos, blen);

		buf += blen;
		buffer_pos += blen;
		len += blen;

		if(buffer_pos >= buffer_list[buffer_item].Length)
		{
			buffer_item ++;
			buffer_pos = 0;
		}
	}

	if(buffer_item < buffer_list_parts)
		more = TRUE;

	return len;
}

void Upload_ExternalMultiBuffer::ResetContent()
{
	buffer_item = 0;
	buffer_pos = 0;
}

OpFileLength Upload_ExternalMultiBuffer::PayloadLength()
{
	if(buffer_list == NULL)
		return 0;

	uint32 i;
	uint32 len = 0;

	for(i = 0; i< buffer_list_parts; i++)
		len += buffer_list[i].Length;

	return len;
}
#endif // UPLOAD2_EXTERNAL_MULTI_BUFFER_SUPPORT

#endif // HTTP_DATA

