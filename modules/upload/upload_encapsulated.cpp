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

#ifdef UPLOAD2_ENCAPSULATED_SUPPORT
Upload_EncapsulatedElement::Upload_EncapsulatedElement()
{
	only_body = FALSE;
	element = NULL;
	separate_body = FALSE;
	external_object = FALSE;
	element_prepared = FALSE;
	extra_header_written = FALSE;
}

Upload_EncapsulatedElement::~Upload_EncapsulatedElement()
{
	if(!external_object)
		OP_DELETE(element);
	element = NULL;
}

#ifdef UPLOAD_ENCAP_DEFAULT_INIT
void Upload_EncapsulatedElement::InitL(Upload_Base *elm, OpStringC8 content_type, MIME_Encoding enc, const char **header_names)
{
	InitL(elm, TRUE, TRUE, FALSE, TRUE, content_type, enc, header_names);
}
#endif

void Upload_EncapsulatedElement::InitL(Upload_Base *elm, BOOL take_over, BOOL sepbody, BOOL prepbody, BOOL force_content_type,
									   OpStringC8 content_type, MIME_Encoding enc, const char **header_names)
{
	SetElement(elm, take_over, sepbody, prepbody);

	Upload_Handler::InitL(enc, header_names);
	SetContentTypeL(!force_content_type || content_type.HasContent() ? content_type.CStr() : "application/mime");
}

void Upload_EncapsulatedElement::SetElement(Upload_Base *elm, BOOL take_over, BOOL sepbody, BOOL prep_body)
{
	if(!external_object)
		OP_DELETE(element);
	element = elm;

	separate_body = sepbody;
	external_object = !take_over;
	element_prepared = prep_body;
}

Upload_Base *Upload_EncapsulatedElement::GetElement() const
{
	return element;
}

OpFileLength Upload_EncapsulatedElement::CalculateLength()
{
	return Upload_Handler::CalculateLength() + (element ? element->CalculateLength() : 0);
}

uint32 Upload_EncapsulatedElement::PrepareL(Boundary_List &boundaries, Upload_Transfer_Mode transfer_restrictions)
{
	int min_len = 0;
	if(element && !element_prepared)
		min_len = element->PrepareL(boundaries, UPLOAD_BINARY_NO_CONVERSION);

	SetMinimumBufferSize(min_len);

	return Upload_Handler::PrepareL(boundaries, transfer_restrictions) + (separate_body ? 0 : min_len);
}

void Upload_EncapsulatedElement::ResetL()
{
	extra_header_written = FALSE;
	if(element) 
		element->ResetL();

	Upload_Handler::ResetL();
}

void Upload_EncapsulatedElement::Release()
{
	if(element) 
		element->Release();

	Upload_Handler::Release();
}

uint32 Upload_EncapsulatedElement::GetNextContentBlock(unsigned char *buf, uint32 max_len, BOOL &more)
{
	more = FALSE;

	if(!element)
		return 0;

	uint32 remaining_len = max_len;
	BOOL done = TRUE;

	element->OutputContent(buf, remaining_len, done);

	more = !done;

	return max_len - remaining_len;
}

void Upload_EncapsulatedElement::ResetContent()
{
	if(element) 
		element->ResetL();

	Upload_Handler::ResetContent();
}

OpFileLength Upload_EncapsulatedElement::PayloadLength()
{
	return (element ? (separate_body ? element->CalculateLength() : element->PayloadLength()) : 0);
}

uint32 Upload_EncapsulatedElement::CalculateHeaderLength()
{
	return Upload_Handler::CalculateHeaderLength() + (separate_body || !element ? 0 : element->CalculateHeaderLength() - STRINGLENGTH("\r\n"));
}

unsigned char *Upload_EncapsulatedElement::OutputHeaders(unsigned char *a_target, uint32 &remaining_len, BOOL &done)
{
	unsigned char * OP_MEMORY_VAR target = a_target;
	done = FALSE;

	target = Upload_Handler::OutputHeaders(target, remaining_len, done);

	if(done && !separate_body && element && !extra_header_written)
	{
		target -= STRINGLENGTH("\r\n"); // The extra CRLF is always present and must be removed
		remaining_len += STRINGLENGTH("\r\n");
		done = FALSE;
		target = element->OutputHeaders(target, remaining_len, done);
		extra_header_written = TRUE;

		if(!done)
		{
			TRAPD(op_err, ResetL());
			OpStatus::Ignore(op_err);
		}
	}

	return target;
}

#ifdef UPLOAD_EXTERNAL_BODY_SUPPORT
void Upload_External_Body::InitL(URL &url, const OpStringC8 &content_id,  const char **header_names)
{
	OpStackAutoPtr<Upload_OpString8> dummy_element (OP_NEW_L(Upload_OpString8, ()));

	const char *c_typ = url.GetAttribute(URL::KMIME_Type).CStr();
	if(c_typ == NULL || *c_typ == '\0')
		c_typ = "application/octet-stream";

	dummy_element->InitL("THIS IS NOT THE REAL BODY!\r\n", url.GetAttribute(URL::KMIME_Type), NULL, ENCODING_BINARY);
	
	dummy_element->AccessHeaders().AddParameterL("Content-ID", content_id);

	Upload_EncapsulatedElement::InitL(dummy_element.release(), "message/external-body", ENCODING_7BIT, header_names);

	AccessHeaders().AddParameterL("Content-Type", "access-type", "URL");

	AccessHeaders().SetSeparatorL("Content-Type", SEPARATOR_SEMICOLON_NEWLINE);

	OpString8 encoded_URL;
	ANCHOR(OpString8, encoded_URL);
	OpString8 url_name;
	ANCHOR(OpString8, url_name);
	url.GetAttribute(URL::KName_Username_Escaped, url_name);

	uint32 len = url_name.Length();

	encoded_URL.ReserveL(len + (len <= 40 ? 0 : (len+39/40) * (40 + STRINGLENGTH("\r\n         "))));
	encoded_URL.AppendL(url_name);
	len -= len%40;
	while(len >= 40)
	{
		encoded_URL.InsertL(len, "\r\n         ");
		len -= 40;
	}

	AccessHeaders().AddParameterL("Content-Type", "URL", encoded_URL, TRUE);
}
#endif

#endif

#endif // HTTP_data

