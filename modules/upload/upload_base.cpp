/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef HAS_SET_HTTP_DATA
#include "modules/upload/upload.h"

#include "modules/olddebug/tstdump.h"

Upload_Base::Upload_Base()
{
	headers_written = FALSE;
	only_body = FALSE;
}

Upload_Base::~Upload_Base()
{
	if(InList())
		Out();
}

void Upload_Base::InitL(const char **header_names)
{
	Headers.InitL(header_names);
}

OpFileLength Upload_Base::CalculateLength()
{
	return CalculateHeaderLength();
}

unsigned char *Upload_Base::OutputContent(unsigned char *target, uint32 &remaining_len, BOOL &done)
{
	return OutputHeaders(target, remaining_len, done);
}

uint32 Upload_Base::GetOutputData(unsigned char *target, uint32 buffer_len, BOOL &done)
{
	uint32 remaining_len = buffer_len;

	OutputContent(target, remaining_len, done);
	
	return buffer_len - remaining_len;
}

uint32 Upload_Base::CalculateHeaderLength()
{
	if(only_body)
		return 0;

	return Headers.CalculateLength() + STRINGLENGTH("\r\n");
}

unsigned char *Upload_Base::OutputHeaders(unsigned char *target, uint32 &remaining_len, BOOL &done)
{
	done = TRUE;
	if(only_body)
		headers_written = TRUE;

	if(!headers_written)
	{
		done = FALSE;
		uint32 header_len = Headers.CalculateLength() + STRINGLENGTH("\r\n");
		if(header_len <= remaining_len)
		{
			target = (unsigned char *) Headers.OutputHeaders((char *) target);
			*(target++)= '\r';
			*(target++)= '\n';
			remaining_len -= header_len;
			done = TRUE;
			headers_written = TRUE;
		}
	}

	return target;
}

uint32 Upload_Base::PrepareL(Boundary_List &boundaries, Upload_Transfer_Mode transfer_restriction)
{
	headers_written = only_body;
	return CalculateHeaderLength()+1;
}

uint32 Upload_Base::PrepareUploadL(Upload_Transfer_Mode transfer_restrictions)
{
	Boundary_List boundaries;
	ANCHOR(Boundary_List, boundaries);
	OP_STATUS op_err;
	OP_MEMORY_VAR BOOL finished =FALSE;
	OP_MEMORY_VAR uint32 len=0;

	if(transfer_restrictions != UPLOAD_BINARY_NO_CONVERSION)
		AccessHeaders().AddParameterL("MIME-Version","1.0");

	while(!finished)
	{
		boundaries.InitL();
		TRAP(op_err, len = PrepareL(boundaries, transfer_restrictions));

		if(op_err != Upload_Error_Status::ERR_FOUND_BOUNDARY_PREFIX)
		{
			if(Upload_Error_Status::IsError(op_err))
				LEAVE(op_err);
			finished = TRUE;
		}
		// else make a new try
	}

	boundaries.GenerateL();

	return len;
}

void Upload_Base::ResetL()
{
	headers_written = only_body;
}

void Upload_Base::Release()
{
	// Nothing to release;
}

Upload_Base *Upload_Base::FirstChild()
{
	return NULL;
}

void Upload_Base::ClearAndSetContentTypeL(const OpStringC8 &a_content_type)
{
	AccessHeaders().ClearAndAddParameterL("Content-Type", a_content_type);

	content_type.SetL(a_content_type);

	int len = content_type.FindFirstOf(" ;,\t");
	if(len != KNotFound)
		content_type.Delete(len); // remove parameters from the content type
}

void Upload_Base::SetContentTypeL(const OpStringC8 &a_content_type)
{
	Header_Item *item = AccessHeaders().FindHeader("Content-Type", TRUE);
	if(item && item->HasParameters())
		return; // already set.

	content_type.SetL(a_content_type);
	if(content_type.HasContent())
		AccessHeaders().AddParameterL("Content-Type", content_type);

	int len = content_type.FindFirstOf(" ;,\t");
	if(len != KNotFound)
		content_type.Delete(len); // remove parameters from the content type
}

void Upload_Base::SetCharsetL(const OpStringC8 &encoded_charset)
{
	charset.SetL(encoded_charset);
	if(encoded_charset.HasContent())
	{
		AccessHeaders().AddParameterL("Content-Type", "charset", encoded_charset);
	}

}

void Upload_Base::SetContentDispositionL(const OpStringC8 &disposition)
{
	if(disposition.HasContent())
		AccessHeaders().AddParameterL("Content-Disposition", disposition);
}

void Upload_Base::SetContentDispositionParameterL(const OpStringC8 &value)
{
	if(value.HasContent())
		AccessHeaders().AddParameterL("Content-Disposition", value);
}

void Upload_Base::SetContentDispositionParameterL(const OpStringC8 &name, const OpStringC8 &value, BOOL quote_value)
{
	if(name.HasContent())
		AccessHeaders().AddParameterL("Content-Disposition", name, value, quote_value);
}

uint32 Upload_Base::ExtractHeadersL(const unsigned char *src, uint32 len, BOOL use_all, Header_Protection_Policy policy, const KeywordIndex *excluded_headers, int excluded_list_len)
{
	return AccessHeaders().ExtractHeadersL(src, len, use_all, policy, excluded_headers, excluded_list_len);
}

uint32 Upload_Base::FileCount()
{
	return 0;
}
      
uint32 Upload_Base::UploadedFileCount()
{
	return 0;
}

#ifdef UPLOAD_NEED_MULTIPART_FLAG
BOOL Upload_Base::IsMultiPart()
{
	return FALSE;
}
#endif

#endif // HTTP_data
