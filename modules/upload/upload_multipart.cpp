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

Upload_Multipart::Upload_Multipart()
{
	current_item = NULL;
	started = FALSE;
}

Upload_Multipart::~Upload_Multipart()
{
	current_item = NULL;
}

void Upload_Multipart::InitL(const OpStringC8 &content_type, const char **header_names)
{
	Upload_Handler::InitL(ENCODING_NONE, header_names);

	SetContentTypeL(content_type.HasContent() ? content_type.CStr() : "multipart/mixed");
}

void Upload_Multipart::AddElement(Upload_Base *item)
{
	if(item)
	{
		if(item->InList())
			item->Out();
		item->Into(this);
	}
}

void Upload_Multipart::AddElements(Upload_Multipart &item)
{
	Append(&item);
}

OpFileLength Upload_Multipart::CalculateLength()
{
	return PayloadLength() + Upload_Base::CalculateLength();
}

uint32 Upload_Multipart::PrepareL(Boundary_List &boundaries, Upload_Transfer_Mode transfer_restrictions)
{
	uint32 min_len = 0, len = 0;

	OpStackAutoPtr<Header_Boundary_Parameter> boundary_param (OP_NEW_L(Header_Boundary_Parameter, ()));

	boundary_param->InitL("boundary", &boundary);

	AccessHeaders().AddParameterL("Content-Type", boundary_param.release());

	boundary.GenerateL(&boundaries); // preliminary generate to get size
	if(boundary.InList())
		boundary.Out();
	boundary.Into(&boundaries);

	Upload_Base *item = First();
	while(item)
	{
		len = item->PrepareL(boundaries, transfer_restrictions);
		if(len > min_len)
			min_len = len;

		item = item->Suc();
	}

	SetMinimumBufferSize(min_len + boundary.Length(Boundary_Internal) * 3);

	return Upload_Handler::PrepareL(boundaries, UPLOAD_BINARY_NO_CONVERSION);
}

void Upload_Multipart::ResetL()
{
	Upload_Handler::ResetL();
	ResetContent();
}

void Upload_Multipart::Release()
{
	Upload_Handler::Release();
	Upload_Base *item = First();
	while(item)
	{
		item->Release();
		item = item->Suc();
	}
}

Upload_Base *Upload_Multipart::FirstChild()
{
	return First();
}

uint32 Upload_Multipart::GetNextContentBlock(unsigned char *buf, uint32 max_len, BOOL &more)
{
	uint32 remaining_len = max_len;
	more = TRUE;

	if (!started)
	{
		Boundary_Kind boundary_kind = current_item ? Boundary_First : Boundary_First_Last;

		if (remaining_len < boundary.Length(boundary_kind))
			return 0;
		buf = boundary.WriteBoundary(buf, remaining_len, boundary_kind);

		started = TRUE;
	}

	while (current_item)
	{
		BOOL done = FALSE;
		Upload_Base *next = current_item->Suc();
		Boundary_Kind boundary_kind = next ? Boundary_Internal : Boundary_Last;

		// Make sure we can add the boundary when we're "done"
		if (remaining_len <= boundary.Length(boundary_kind))
			break;
		remaining_len -= boundary.Length(boundary_kind);

		buf = current_item->OutputContent(buf, remaining_len, done);

		remaining_len += boundary.Length(boundary_kind);

		if (!done)
			break;

		current_item = next;

		buf = boundary.WriteBoundary(buf, remaining_len, boundary_kind);
	}

	more = current_item ? TRUE : FALSE;

	return max_len - remaining_len;
}

void Upload_Multipart::ResetContent()
{
	started = FALSE;
	current_item = First();

	Upload_Base *item = First();
	while(item)
	{
		item->ResetL();
		item = item->Suc();
	}
}

OpFileLength Upload_Multipart::PayloadLength()
{
	OpFileLength len = 0;
	BOOL first = TRUE;

	for (Upload_Base *item = First(); item; item = item->Suc())
	{
		len += item->CalculateLength() + boundary.Length(first ? Boundary_First : Boundary_Internal);
		first = FALSE;
	}

	len += boundary.Length(first ? Boundary_First_Last : Boundary_Last);

	return len;
}

BOOL Upload_Multipart::BodyNeedScan()
{
	return FALSE;
}

uint32 Upload_Multipart::FileCount()
{
	uint32 count = 0;

	Upload_Base *item = First();
	while(item)
	{
		count += item->FileCount();

		item = item->Suc();
	}

	return count;
}
      
uint32 Upload_Multipart::UploadedFileCount()
{
	uint32 count = 0;

	Upload_Base *item = First();
	while(item && item != current_item)
	{
		count += item->UploadedFileCount();

		item = item->Suc();
	}

	return count;
}

#ifdef UPLOAD_NEED_MULTIPART_FLAG
BOOL Upload_Multipart::IsMultiPart()
{
	return TRUE;
}
#endif

#endif // HTTP_data
