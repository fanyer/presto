/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/m2/src/composer/messageencoder.h"
#include "adjunct/desktop_util/datastructures/StreamBuffer.h"


/***********************************************************************************
 **
 **
 ** MessageEncoder::AddHeaderValue
 ***********************************************************************************/
OP_STATUS MessageEncoder::AddHeaderValue(Header::HeaderType type, const OpStringC& value, const OpStringC8& charset)
{
	// these headers are added by Upload::PrepareUploadL
	if (value.IsEmpty() || type == Header::CONTENTTYPE || type == Header::MIMEVERSION || type == Header::CONTENTTRANSFERENCODING)
		return OpStatus::OK;

	OpAutoPtr<Header> header (OP_NEW(Header, (0, type)));
	if (!header.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(header->SetCharset(charset));
	RETURN_IF_ERROR(header->SetValue(value));

	header.release()->Into(&m_header_list);

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 ** MessageEncoder::AddHeaderValue
 ***********************************************************************************/
OP_STATUS MessageEncoder::AddHeaderValue(const OpStringC8& name, const OpStringC& value, const OpStringC8& charset)
{
	if (name.IsEmpty() || value.IsEmpty())
		return OpStatus::OK;

	OpAutoPtr<Header> header (OP_NEW(Header, (0, Header::UNKNOWN)));
	if (!header.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(header->SetName(name));
	RETURN_IF_ERROR(header->SetCharset(charset));
	RETURN_IF_ERROR(header->SetValue(value));
	

	header.release()->Into(&m_header_list);

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 ** MessageEncoder::SetHTMLBody
 ***********************************************************************************/
OP_STATUS MessageEncoder::SetHTMLBody(OpAutoPtr<Upload_Base> html_body)
{
	m_html_body = html_body;

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** MessageEncoder::SetPlainTextBody
 ***********************************************************************************/
OP_STATUS MessageEncoder::SetPlainTextBody(const OpStringC& plain_text, const OpStringC8& charset)
{
	// this should rather be in Upload_OpString16, but for now wrap here
	OpString flowed_body;
	if (m_wrapper.get() && !plain_text.IsEmpty())
		RETURN_IF_ERROR(m_wrapper->WrapText(flowed_body, plain_text, TRUE));
	else
		RETURN_IF_ERROR(flowed_body.Set(plain_text));

	OpAutoPtr<Upload_OpString16> plain_text_body (OP_NEW(Upload_OpString16, ()));
	if (!plain_text_body.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_LEAVE(plain_text_body->InitL(flowed_body, "text/plain", charset));
	RETURN_IF_LEAVE(plain_text_body->AccessHeaders().AddParameterL("Content-Type", "format", "flowed"));
	RETURN_IF_LEAVE(plain_text_body->AccessHeaders().AddParameterL("Content-Type", "delsp", "yes"));

	m_plain_text_body = plain_text_body.release();

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** MessageEncoder::AddAttachment
 ***********************************************************************************/
OP_STATUS MessageEncoder::AddAttachment(const OpStringC& attachment_path, const OpStringC& suggested_filename, const OpStringC8& charset, const OpStringC8& mime_type)
{
	OpFile file;
	RETURN_IF_ERROR(file.Construct(attachment_path.CStr()));

	OpAutoPtr<Upload_URL> upload_url (OP_NEW(Upload_URL, ()));
	if (!upload_url.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_LEAVE(upload_url->InitL(file.GetFullPath(), suggested_filename, "attachment", 0, charset));

	if (!mime_type.IsEmpty())
	{
		upload_url->ClearAndSetContentTypeL(mime_type.CStr());
	}
	
	// work around for outlook and non-ascii attachment filenames (DSK-130346)
	BOOL is_ascii = TRUE;
	int i = suggested_filename.Length();
	do
	{
		if (suggested_filename.CStr()[--i] > 127)
		{
			is_ascii = FALSE;
		}
	}
	while (is_ascii && i >= 0);

	if (!is_ascii)
		upload_url->SetForceQPContentTypeName(TRUE);

	RETURN_IF_ERROR(InitializeAttachments());
	m_attachments->AddElement(upload_url.release());

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** MessageEncoder::InitializeAttachments
 ***********************************************************************************/
OP_STATUS MessageEncoder::InitializeAttachments()
{
	if (m_attachments.get())
		return OpStatus::OK;

	m_attachments = OP_NEW(Upload_Multipart, ());
	if (!m_attachments.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_LEAVE(m_attachments->InitL());

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** MessageEncoder::Encode
 ***********************************************************************************/
OP_STATUS MessageEncoder::Encode(OpString8& encoded_message, const OpStringC8& charset, BOOL encode_attachments)
{
	OpAutoPtr<Upload_Base> main_part;

	RETURN_IF_ERROR(CreateMainPart(main_part, encode_attachments));
	RETURN_IF_ERROR(AddHeadersTo(*main_part));

	return EncodeMainPartToMessage(*main_part, encoded_message, charset);
}


/***********************************************************************************
 **
 **
 ** MessageEncoder::CreateMainPart
 ***********************************************************************************/
OP_STATUS MessageEncoder::CreateMainPart(OpAutoPtr<Upload_Base>& main_part, BOOL encode_attachments)
{
	OpAutoPtr<Upload_Base> text_part;
	RETURN_IF_ERROR(CreateTextPart(text_part));

	if (encode_attachments && m_attachments.get())
	{
		RETURN_IF_ERROR(CreateMultipartWithAttachments(text_part, main_part));
	}
	else
	{
		main_part = text_part;
	}

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** MessageEncoder::CreateTextPart
 ***********************************************************************************/
OP_STATUS MessageEncoder::CreateTextPart(OpAutoPtr<Upload_Base>& text_part)
{
	if (!m_plain_text_body.get() && !m_html_body.get())
		return OpStatus::ERR;

	if (m_plain_text_body.get() && m_html_body.get())
	{
		RETURN_IF_ERROR(CreateMultipartAlternative(text_part));
	}
	else if (m_html_body.get())
	{
		text_part = m_html_body;
	}
	else
	{
		text_part = m_plain_text_body;
	}
	
	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** MessageEncoder::CreateMultipartAlternative
 ***********************************************************************************/
OP_STATUS MessageEncoder::CreateMultipartAlternative(OpAutoPtr<Upload_Base>& text_part)
{
	OpAutoPtr<Upload_Multipart> multipart (OP_NEW(Upload_Multipart, ()));
	if (!multipart.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_LEAVE(multipart->InitL("multipart/alternative"));

	multipart->AddElement(m_plain_text_body.release());
	multipart->AddElement(m_html_body.release());

	text_part = multipart.release();

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** MessageEncoder::CreateMultipartWithAttachments
 ***********************************************************************************/
OP_STATUS MessageEncoder::CreateMultipartWithAttachments(OpAutoPtr<Upload_Base> text_part, OpAutoPtr<Upload_Base>& target)
{
	OpAutoPtr<Upload_Multipart> multipart (OP_NEW(Upload_Multipart, ()));
	if (!multipart.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_LEAVE(multipart->InitL("multipart/mixed"));

	multipart->AddElement(text_part.release());
	multipart->AddElements(*m_attachments);

	target = multipart.release();

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** MessageEncoder::AddHeadersTo
 ***********************************************************************************/
OP_STATUS MessageEncoder::AddHeadersTo(Upload_Base& main_part)
{
	Header* header = static_cast<Header*>(m_header_list.First());
	while (header)
	{
		OpString8 header_name;
		RETURN_IF_ERROR(header->GetName(header_name));
		OpString8 header_value;
		RETURN_IF_ERROR(header->GetValue(header_value, header->HeaderSupportsQuotePair()));

		OpAutoPtr<Header_Item> mime_header (OP_NEW(Header_Item, ()));
		if (!mime_header.get())
			return OpStatus::ERR_NO_MEMORY;

		RETURN_IF_LEAVE(mime_header->InitL(header_name, header_value));
		main_part.AccessHeaders().InsertHeader(mime_header.release());

		header = static_cast<Header*>(header->Suc());
	}

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 ** MessageEncoder::EncodeMainPartToMessage
 ***********************************************************************************/
OP_STATUS MessageEncoder::EncodeMainPartToMessage(Upload_Base& main_part, OpString8& encoded_message, const OpStringC8& charset)
{
	RETURN_IF_LEAVE(
					main_part.PrepareUploadL(m_allow_8bit? UPLOAD_8BIT_TRANSFER: UPLOAD_7BIT_TRANSFER);
					main_part.ResetL());

	StreamBuffer<char> main_buffer;
	RETURN_IF_ERROR(main_buffer.Reserve(main_part.CalculateLength()));

	OpString8      part_buffer_holder;
	unsigned PartBufferSize = 2048;
	char*          part_buffer    = part_buffer_holder.Reserve(PartBufferSize);
	if (!part_buffer)
		return OpStatus::ERR_NO_MEMORY;

	BOOL done = FALSE;
	while (!done)
	{
		unsigned remaining = PartBufferSize;

		main_part.OutputContent(reinterpret_cast<unsigned char*>(part_buffer), remaining, done);

		if (remaining == PartBufferSize && PartBufferSize == 2048) // No progress, output failed
		{
			PartBufferSize += PartBufferSize;	// try again with double the size (workaround for DSK-250881)
			part_buffer    = part_buffer_holder.Reserve(PartBufferSize);
		}
		else if (remaining == PartBufferSize) // No progress, 2 attempts failed 
			return OpStatus::ERR;
		else
			RETURN_IF_ERROR(main_buffer.Append(part_buffer, PartBufferSize - remaining));
	}

	encoded_message.TakeOver(main_buffer.Release());

	return OpStatus::OK;
}

#endif // M2_SUPPORT
