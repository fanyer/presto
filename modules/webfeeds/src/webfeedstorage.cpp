// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 2006-2012 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.

#include "core/pch.h"

#ifdef WEBFEEDS_SAVED_STORE_SUPPORT

#include "modules/webfeeds/src/webfeeds_api_impl.h"
#include "modules/webfeeds/src/webfeedstorage.h"
#include "modules/webfeeds/src/webfeed.h"

#include "modules/encodings/encoders/utf8-encoder.h"
#include "modules/formats/base64_decode.h"
#include "modules/stdlib/util/opdate.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/htmlify.h"
#include "modules/util/opfile/unistream.h"
#include "modules/url/url_api.h"
#include "modules/url/url_enum.h"
#include "modules/xmlutils/xmltoken.h"
#include "modules/xmlutils/xmlparser.h"
#include "modules/xmlutils/xmlnames.h"

# define STRN_MATCH(uni_str, nuni_str, len) (uni_strncmp(uni_str, nuni_str, len) == 0)


#define FEED_STORE_FILE	(UNI_L("webfeeds.store"))


//
// The WriteBuffer class
//

WriteBuffer::WriteBuffer() :
	m_buffer(NULL)
{
}

WriteBuffer::~WriteBuffer()
{
	OP_DELETEA(m_buffer);
}

void
WriteBuffer::InitL(const uni_char *file_name)
{
	m_max_len = 1024;
	m_used = 0;
	m_written = 0;
	m_buffer = OP_NEWA_L(char, m_max_len);

	LEAVE_IF_ERROR(m_file.Construct(file_name, OPFILE_TEMP_FOLDER));
	LEAVE_IF_ERROR(m_file.Open(OPFILE_WRITE | OPFILE_TEXT));
}

void
WriteBuffer::AppendOpString(OpStringC *str)
{
	int needed = str->UTF8(NULL, -1);
	if(needed <= 0)
		return;
	
	if (m_used + needed > m_max_len)
	{
		UTF16toUTF8Converter encoder;

		int len = UNICODE_SIZE(str->Length());
		int offset = 0;
		char *str_ptr = (char*)str->CStr();
		do
		{
			int read = 0;
			int rc = encoder.Convert(str_ptr + offset,
				len - offset, m_buffer + m_used,
				m_max_len - m_used, &read);

			m_used += rc;
			offset += read;

			UINT space_left = m_max_len - m_used;
			if (space_left < 10) // flush if buffer is full
				Flush();
			if (space_left == m_max_len - m_used)  // if still no more space, abort
				break;
		}
		while (len > offset);
	}
	else
	{
		// -1 because UTF8 counts the trailing 0 character
		m_used += str->UTF8(m_buffer + m_used, m_max_len - m_used) - 1;
	}
}

void
WriteBuffer::Append(const uni_char *buf)
{
	OpStringC16 str(buf);
	AppendOpString(&str);
}

void
WriteBuffer::Append(const char *buf)
{
	Append(buf, op_strlen(buf));
}

void
WriteBuffer::Append(const char* buf, int buf_len)
{
	// check if we have to flush what is already in the buffer
	if (m_used + buf_len > m_max_len)
		Flush();

	// check if we should flush the big content directly
	if (m_used + buf_len > m_max_len)
	{
		m_file.Write(buf, buf_len);
		m_written += buf_len;
		m_used = 0;
	}
	else
	{
		op_strncpy(m_buffer + m_used, buf, buf_len);
		m_used += buf_len;
	}
}

void
WriteBuffer::Append(UINT num, const char *format/*=NULL*/, int digits/*=1*/)
{
	int new_size = sizeof(num) * 3;
	if (digits > new_size)
		new_size = digits;

	if (m_used + new_size > m_max_len)
		Flush();

	if (format)
		m_used += op_sprintf(m_buffer + m_used, format, num);
	else
		m_used += op_sprintf(m_buffer + m_used, "%u", num);
}

void
WriteBuffer::AppendDate(double date)
{
	if (m_used + 4 + 3 + 3 + 1 + 2 + 3 + 3 + 4 + 7 > m_max_len)
		Flush();

	m_used += op_sprintf(m_buffer + m_used,
		"%d-%02d-%02dT%02d:%02d:%02dZ",
		OpDate::YearFromTime(date), OpDate::MonthFromTime(date) + 1,
		OpDate::DateFromTime(date), OpDate::HourFromTime(date),
		OpDate::MinFromTime(date), OpDate::SecFromTime(date));
}

void
WriteBuffer::AppendBinaryL(const unsigned char* buf, int buf_len)
{
	char *out_buf = NULL;
	int out_buf_len = 0;
	MIME_Encode_Error error = MIME_Encode_SetStr(out_buf, out_buf_len,
		(const char *)buf, buf_len, NULL, GEN_BASE64);

	if (error == MIME_FAILURE)
	{
		LEAVE(OpStatus::ERR_NO_MEMORY);
	}
	else if (error == MIME_NO_ERROR)
	{
		Append(out_buf, out_buf_len);
	}
}

void
WriteBuffer::AppendQuotedL(const uni_char* buf)
{
	uni_char *quoted = XMLify_string(buf);
	if (buf && *buf && !quoted)
	{
		LEAVE(OpStatus::ERR_NO_MEMORY);
	}

	Append(quoted);

	OP_DELETEA(quoted);
}

void
WriteBuffer::AppendQuotedL(const char* buf)
{
	if (!buf)
		return;
	
	char *quoted = XMLify_string(buf, op_strlen(buf), TRUE);
	if (*buf && !quoted)
		LEAVE(OpStatus::ERR_NO_MEMORY);

	Append(quoted);

	OP_DELETEA(quoted);
}

void
WriteBuffer::Flush()
{
	m_file.Write(m_buffer, m_used);
	m_written += m_used;
	m_used = 0;
}


//
// The WebFeedStorage class
//

WebFeedStorage::WebFeedStorage() : m_current_feed(NULL), m_current_stub(NULL),
	m_own_stub(FALSE),	m_current_entry(NULL), m_current_content(NULL), 
	m_api_impl(NULL), m_in_title(FALSE), m_is_feed_file(FALSE), m_first_free_id(1)
{
}

WebFeedStorage::~WebFeedStorage()
{
	if (m_own_stub)
		OP_DELETE(m_current_stub);

	if (m_current_feed)
		m_current_feed->DecRef();
	if (m_current_entry)
		m_current_entry->DecRef();
	if (m_current_content)
		m_current_content->DecRef();
}

OP_STATUS
WebFeedStorage::LocalLoad(const uni_char *file_name)
{
	OP_STATUS oom = OpStatus::OK;

	OpFile feed_file;
	OpStringC file_name_str(file_name);
	oom = feed_file.Construct(file_name_str.CStr(), OPFILE_WEBFEEDS_FOLDER);
	if (OpStatus::IsError(oom))
		return oom;

	UnicodeFileInputStream in_stream;
	RETURN_IF_ERROR(in_stream.Construct(&feed_file, URL_XML_CONTENT, TRUE));

	XMLParser *parser;
	URL dummy;
	RETURN_IF_ERROR(XMLParser::Make(parser, NULL, g_main_message_handler, this, dummy));
	OpStackAutoPtr<XMLParser> protected_parser(parser);

	// Set up XML configuration for parsing web feed storage:
	XMLParser::Configuration configuration;
	configuration.load_external_entities = XMLParser::LOADEXTERNALENTITIES_NO;
	configuration.max_tokens_per_call = 0;  // unlimited
#if defined(_DEBUG) && defined(XML_ERRORS)
	configuration.generate_error_report = TRUE;
#endif

	parser->SetConfiguration(configuration);

	int buf_len = 2048;

	BOOL more = in_stream.has_more_data();
	while (OpStatus::IsSuccess(oom) && more && !parser->IsFailed())
	{
		uni_char *buf = in_stream.get_block(buf_len);
		more = in_stream.has_more_data();

		OP_ASSERT(buf_len % sizeof(uni_char) == 0);
		buf_len /= sizeof(uni_char);

		OP_ASSERT(buf);
		if (!buf)
			return OpStatus::ERR;
		
		oom = parser->Parse(buf, buf_len, more);
		OP_ASSERT(!parser->IsFailed());
#if defined(_DEBUG) && defined XML_ERRORS
		if (parser->IsFailed())
		{
			XMLRange range;
			const char *error, *url, *fragment;
		
			range = parser->GetErrorPosition();
			parser->GetErrorDescription(error, url, fragment);
		}
#endif // defined(_DEBUG) && defined(XML_ERRORS)
	}

	return OpStatus::OK;
}

OP_STATUS
WebFeedStorage::LoadFeed(WebFeedsAPI_impl::WebFeedStub* stub, WebFeed *&feed, WebFeedsAPI_impl* api_impl)
{
	if (m_current_feed)
		m_current_feed->DecRef();

	OP_ASSERT(!feed);
	OP_ASSERT(!m_current_feed);

	m_api_impl = api_impl;
	m_is_feed_file = TRUE;

	if (m_current_feed)
		m_current_feed->IncRef();
	m_current_feed = NULL;

	if (m_own_stub)
		OP_DELETE(m_current_stub);
	m_current_stub = stub;
	m_own_stub = FALSE;

	OpString feed_file;
	feed_file.Reserve(14);
	uni_sprintf(feed_file.CStr(), UNI_L("%08x.feed"), stub->GetId());

	OP_STATUS ret_stat = LocalLoad(feed_file.CStr());

	feed = m_current_feed;
	if (m_current_feed)
		m_current_feed->DecRef();
	m_current_feed = NULL;

	return ret_stat;
}

void
WebFeedStorage::SaveFeedL(WebFeed *feed)
{
	if (!feed)
		return;

	WriteBuffer w_buf; ANCHOR(WriteBuffer, w_buf);
	w_buf.InitL(UNI_L("tmp.feed"));

	feed->GetStub()->SaveL(w_buf, FALSE);
	w_buf.Flush();
	w_buf.GetFile().Close();

	OpString file_name; ANCHOR(OpString, file_name);
	file_name.Reserve(14);
	uni_sprintf(file_name.CStr(), UNI_L("%08x.feed"), feed->GetId());

	OpFile old_file; ANCHOR(OpFile, old_file);
	LEAVE_IF_ERROR(old_file.Construct(file_name.CStr(), OPFILE_WEBFEEDS_FOLDER));
	LEAVE_IF_ERROR(old_file.SafeReplace(&w_buf.GetFile()));

	return;
}

OP_STATUS
WebFeedStorage::LoadStore(WebFeedsAPI_impl* api_impl)
{
	OP_ASSERT(!m_current_feed);
	
	if (m_current_feed)
		m_current_feed->DecRef();
	m_current_feed = NULL;

	if (m_own_stub)
		OP_DELETE(m_current_stub);
	m_current_stub = NULL;

	m_api_impl = api_impl;
	m_is_feed_file = FALSE;

	RETURN_IF_ERROR(LocalLoad(FEED_STORE_FILE));

	return OpStatus::OK;
}

void
WebFeedStorage::SaveStoreL(WebFeedsAPI_impl* api_impl)
{
	OP_ASSERT(api_impl);
	if (!api_impl)
		return;

	WriteBuffer w_buf; ANCHOR(WriteBuffer, w_buf);
	w_buf.InitL(UNI_L("tmp.storage"));

	api_impl->SaveL(w_buf);
	w_buf.Flush();
	w_buf.GetFile().Close();

	OpFile old_file; ANCHOR(OpFile, old_file);
	LEAVE_IF_ERROR(old_file.Construct(FEED_STORE_FILE, OPFILE_WEBFEEDS_FOLDER));
	LEAVE_IF_ERROR(old_file.SafeReplace(&w_buf.GetFile()));
}

/* virtual */ XMLTokenHandler::Result
WebFeedStorage::HandleToken(XMLToken &token)
{
	OP_STATUS status = OpStatus::OK;

	switch (token.GetType())
	{
		case XMLToken::TYPE_CDATA:
		case XMLToken::TYPE_Text:
			status = HandleTextToken(token);
			break;

		case XMLToken::TYPE_STag:
			status = HandleStartTagToken(token);
			break;

		case XMLToken::TYPE_ETag:
			status = HandleEndTagToken(token);
			break;

		case XMLToken::TYPE_EmptyElemTag:
			status = HandleStartTagToken(token);
			if (OpStatus::IsSuccess(status))
				status = HandleEndTagToken(token);
			break;

		default:
			break;
	}

	if (OpStatus::IsMemoryError(status))
		return XMLTokenHandler::RESULT_OOM;
	else if (OpStatus::IsError(status))
		return XMLTokenHandler::RESULT_ERROR;

	return XMLTokenHandler::RESULT_OK;
}


OP_STATUS
WebFeedStorage::HandleStartTagToken(const XMLToken& token)
{
	// Fetch information about the element.
	const XMLCompleteNameN &elemname = token.GetName();

	const uni_char *elm_name = elemname.GetLocalPart();
	UINT elm_name_len = elemname.GetLocalPartLength();

	if (STRN_MATCH(elm_name, "entry", elm_name_len))
	{
		OP_ASSERT(m_current_feed);
		OP_ASSERT(!m_current_entry);

		m_current_entry = OP_NEW(WebFeedEntry, (m_current_feed));

		if (!m_current_entry)
			return OpStatus::ERR_NO_MEMORY;

		if (OpStatus::IsError(m_current_entry->Init()))
		{
			m_current_entry->DecRef();
			return OpStatus::ERR_NO_MEMORY;
		}

		const XMLToken::Attribute *attrs = token.GetAttributes();
		for (UINT i = 0; i < token.GetAttributesCount(); i++)
		{
			const uni_char *attr_name = attrs[i].GetName().GetLocalPart();
			UINT attr_name_len = attrs[i].GetName().GetLocalPartLength();
			const uni_char *attr_value = attrs[i].GetValue();
			UINT attr_value_len = attrs[i].GetValueLength();

			if (STRN_MATCH(attr_name, "id", attr_name_len))
			{
				OpString attr_val_str;
				RETURN_IF_ERROR(attr_val_str.Set(attr_value, attr_value_len));

				m_current_entry->SetId((OpFeedEntry::EntryId)uni_atoi(attr_val_str.CStr()));
			}
			else if (STRN_MATCH(attr_name, "keep", attr_name_len))
			{
				if (STRN_MATCH(attr_value, "yes", attr_value_len))
					m_current_entry->SetKeep(TRUE);
			}
			else if (STRN_MATCH(attr_name, "read", attr_name_len))
			{
				if (STRN_MATCH(attr_value, "yes", attr_value_len))
					m_current_entry->SetReadStatus(OpFeedEntry::STATUS_READ);
			}
			else if (STRN_MATCH(attr_name, "guid", attr_name_len))
			{
				OpString attr_val_str;
				RETURN_IF_ERROR(attr_val_str.Set(attr_value, attr_value_len));

				m_current_entry->SetGuid(attr_val_str.CStr());
			}
		}
	}
	else if (STRN_MATCH(elm_name, "feed", elm_name_len))
	{
		OP_ASSERT(!m_current_feed);

		if (m_is_feed_file)
		{
			OP_STATUS status;
			if (m_current_feed)
				m_current_feed->DecRef();

			m_current_feed = OP_NEW(WebFeed, ());
			if (!m_current_feed)
				return OpStatus::ERR_NO_MEMORY;

			OP_ASSERT(m_current_stub);
			if (m_current_stub)
				status = m_current_feed->Init(m_current_stub);
			else
				return OpStatus::ERR;

			if (OpStatus::IsError(status))
			{
				m_current_feed->DecRef();
				m_current_feed = NULL;
				return OpStatus::ERR_NO_MEMORY;
			}
			else
				m_current_feed->IncRef();
		}
		else
		{
			OP_ASSERT(!m_current_stub);
			if (m_own_stub)
				OP_DELETE(m_current_stub);
			
			m_current_stub = OP_NEW(WebFeedsAPI_impl::WebFeedStub, ());
			if (!m_current_stub)
				return OpStatus::ERR_NO_MEMORY;

			RETURN_IF_ERROR(m_current_stub->Init());

			m_own_stub = TRUE;
		}

		const XMLToken::Attribute *attrs = token.GetAttributes();
		for (UINT i = 0; i < token.GetAttributesCount(); i++)
		{
			const uni_char *attr_name = attrs[i].GetName().GetLocalPart();
			UINT attr_name_len = attrs[i].GetName().GetLocalPartLength();
			OpString attr_value;
			RETURN_IF_ERROR(attr_value.Set(attrs[i].GetValue(), attrs[i].GetValueLength()));

			if (STRN_MATCH(attr_name, "id", attr_name_len))
			{
				OpFeed::FeedId id = (OpFeed::FeedId) uni_atoi(attr_value.CStr());
				m_current_stub->SetId(id);
				
				m_first_free_id = MAX(m_first_free_id, id+1);
			}
			else if (STRN_MATCH(attr_name, "next-free-entry-id", attr_name_len))
			{
				OpFeedEntry::EntryId next_id = (OpFeedEntry::EntryId) uni_atoi(attr_value.CStr());
				if (m_current_feed)
					m_current_feed->SetNextFreeEntryId(next_id);
			}
			else if (STRN_MATCH(attr_name, "update-interval", attr_name_len))
			{
				UINT update_interval = (UINT)uni_atoi(attr_value.CStr());
				m_current_stub->SetUpdateInterval(update_interval);
			}
			else if (STRN_MATCH(attr_name, "min-update-interval", attr_name_len))
			{
				UINT min_update_interval = (UINT) uni_atoi(attr_value.CStr());
				m_current_stub->SetMinUpdateInterval(min_update_interval);
			}
			else if (STRN_MATCH(attr_name, "total-entries", attr_name_len))
			{
				UINT total = (UINT) uni_atoi(attr_value.CStr());
				m_current_stub->IncrementTotalCount(total);
			}
			else if (STRN_MATCH(attr_name, "unread-entries", attr_name_len))
			{
				UINT count = (UINT) uni_atoi(attr_value.CStr());
				m_current_stub->IncrementUnreadCount(count);
			}
			else if (STRN_MATCH(attr_name, "icon", attr_name_len))
			{
				m_current_stub->SetInternalFeedIconFilename(attr_value.CStr());
			}
			else if (STRN_MATCH(attr_name, "prefetch", attr_name_len))
			{
				OP_ASSERT(m_current_feed);
				if (m_current_feed)
				{
					BOOL do_prefetch = uni_str_eq("true", attr_value.CStr());
					m_current_feed->SetPrefetchPrimaryWhenNewEntries(do_prefetch);
				}
			}
		}
	}
	else if (STRN_MATCH(elm_name, "setting", elm_name_len))
	{
		int name_idx = -1;
		int val_idx = -1;

		const XMLToken::Attribute *attrs = token.GetAttributes();
		for (UINT i = 0; i < token.GetAttributesCount(); i++)
		{
			const uni_char *attr_name = attrs[i].GetName().GetLocalPart();
			UINT attr_name_len = attrs[i].GetName().GetLocalPartLength();

			if (STRN_MATCH(attr_name, "name", attr_name_len))
			{
				name_idx = i;
			}
			else if (STRN_MATCH(attr_name, "value", attr_name_len))
			{
				val_idx = i;
			}
		}

		if (name_idx != -1 && val_idx != -1)
		{
			const uni_char *prop_name = attrs[name_idx].GetValue();
			UINT prop_name_len = attrs[name_idx].GetValueLength();

			OpString value_str;
			RETURN_IF_ERROR(value_str.Set(attrs[val_idx].GetValue(), attrs[val_idx].GetValueLength()));
			int val_num = uni_atoi(value_str.CStr());

			if (STRN_MATCH(prop_name, "max-size", prop_name_len))
			{
				OP_ASSERT(!m_current_feed);
				m_api_impl->SetMaxSize((UINT)val_num);

			}
			else if (STRN_MATCH(prop_name, "max-age", prop_name_len))
			{
				if (m_current_feed)
					m_current_feed->SetMaxAge((UINT)val_num);
				else
					m_api_impl->SetDefMaxAge((UINT)val_num);
			}
			else if (STRN_MATCH(prop_name, "max-entries", prop_name_len))
			{
				if (m_current_feed)
					m_current_feed->SetMaxEntries((UINT)val_num);
				else
					m_api_impl->SetDefMaxEntries((UINT)val_num);
			}
			else if (STRN_MATCH(prop_name, "show-images", prop_name_len))
			{
				if (value_str.Compare("yes") == 0)
				{
					if (m_current_feed)
						m_current_feed->SetShowImages(TRUE);
					else
						m_api_impl->SetDefShowImages(TRUE);
				}
				else
				{
					if (m_current_feed)
						m_current_feed->SetShowImages(FALSE);
					else
						m_api_impl->SetDefShowImages(FALSE);
				}
			}
			else if (STRN_MATCH(prop_name, "show-permalink", prop_name_len))
			{
				if (m_current_feed)
				{
					if (value_str.Compare("yes") == 0)
						m_current_feed->SetShowPermalink(TRUE);
					else
						m_current_feed->SetShowPermalink(FALSE);
				}
			}

		}
	}
	else if (STRN_MATCH(elm_name, "url", elm_name_len))
	{
		int url_idx = -1;
		int type_idx = -1;

		const XMLToken::Attribute *attrs = token.GetAttributes();
		for (UINT i = 0; i < token.GetAttributesCount(); i++)
		{
			const uni_char *attr_name = attrs[i].GetName().GetLocalPart();
			UINT attr_name_len = attrs[i].GetName().GetLocalPartLength();

			if (STRN_MATCH(attr_name, "value", attr_name_len))
				url_idx = i;
			else if (STRN_MATCH(attr_name, "type", attr_name_len))
				type_idx = i;
		}

		if (url_idx != -1)
		{
			OpString url_str;
			RETURN_IF_ERROR(url_str.Set(attrs[url_idx].GetValue(), attrs[url_idx].GetValueLength()));

			if (m_current_entry)
			{
				WebFeedLinkElement* link_elem = NULL;
				m_current_entry->AddAlternateLink(link_elem);
				link_elem->SetURI(url_str);
			}
			else if (type_idx != -1
				&& STRN_MATCH(attrs[type_idx].GetValue(), "icon", attrs[type_idx].GetValueLength()))
			{
				if (m_current_feed)
					m_current_feed->SetIcon(url_str.CStr());
			}
			else
			{
				URL url = g_url_api->GetURL(url_str.CStr());
				m_current_stub->SetURL(url);
			}
		}
	}
	else if (STRN_MATCH(elm_name, "date", elm_name_len))
	{
		int value_idx = -1;
		int type_idx = -1;

		const XMLToken::Attribute *attrs = token.GetAttributes();
		for (UINT i = 0; i < token.GetAttributesCount(); i++)
		{
			const uni_char *attr_name = attrs[i].GetName().GetLocalPart();
			UINT attr_name_len = attrs[i].GetName().GetLocalPartLength();

			if (STRN_MATCH(attr_name, "value", attr_name_len))
				value_idx = i;
			else if (STRN_MATCH(attr_name, "type", attr_name_len))
				type_idx = i;
		}

		if (value_idx != -1)
		{
			OpString value_str;
			RETURN_IF_ERROR(value_str.Set(attrs[value_idx].GetValue(), attrs[value_idx].GetValueLength()));

			double date = OpDate::ParseRFC3339Date(value_str.CStr());
			if (type_idx != -1
				&& STRN_MATCH(attrs[type_idx].GetValue(), "published", attrs[type_idx].GetValueLength()))
			{
				if (m_current_entry)
					m_current_entry->SetPublicationDate(date);
			}
			else
				m_current_stub->SetUpdateTime(date);
		}
	}
	else if (STRN_MATCH(elm_name, "content", elm_name_len))
	{
		OP_ASSERT(m_current_entry || (m_current_feed && m_in_title));

		int type_idx = -1;
		const XMLToken::Attribute *attrs = token.GetAttributes();
		for (UINT i = 0; i < token.GetAttributesCount(); i++)
		{
			if (STRN_MATCH(attrs[i].GetName().GetLocalPart(), "type", attrs[i].GetName().GetLocalPartLength()))
				type_idx = i;
		}

		if (!m_in_title && m_current_prop.IsEmpty())
		{
			RETURN_IF_ERROR(AddNewContent());
		}

		if (type_idx != -1)
		{
			OpString type;
			RETURN_IF_ERROR(type.Set(attrs[type_idx].GetValue(), attrs[type_idx].GetValueLength()));
			RETURN_IF_ERROR(m_current_content->SetType(type));
		}
	}
	else if (STRN_MATCH(elm_name, "title", elm_name_len))
	{
		m_in_title = TRUE;
		RETURN_IF_ERROR(AddNewContent());
	}
	else if (STRN_MATCH(elm_name, "prop", elm_name_len))
	{
		int name_idx = -1;

		const XMLToken::Attribute *attrs = token.GetAttributes();
		for (UINT i = 0; i < token.GetAttributesCount(); i++)
		{
			if (STRN_MATCH(attrs[i].GetName().GetLocalPart(), "name", attrs[i].GetName().GetLocalPartLength()))
				name_idx = i;
		}

		if (name_idx != -1)
		{
			RETURN_IF_ERROR(m_current_prop.Set(attrs[name_idx].GetValue(), attrs[name_idx].GetValueLength()));
			RETURN_IF_ERROR(AddNewContent());
		}
	}
	else if (STRN_MATCH(elm_name, "store", elm_name_len))
	{
		m_first_free_id = 1;

		const XMLToken::Attribute *attrs = token.GetAttributes();
		for (UINT i = 0; i < token.GetAttributesCount(); i++)
		{
			const uni_char *attr_name = attrs[i].GetName().GetLocalPart();
			UINT attr_name_len = attrs[i].GetName().GetLocalPartLength();

			OpString attr_value;
			RETURN_IF_ERROR(attr_value.Set(attrs[i].GetValue(), attrs[i].GetValueLength()));

			if (STRN_MATCH(attr_name, "default-update-interval", attr_name_len))
			{
				m_def_update_interval = (OpFeed::FeedId) uni_atoi(attr_value.CStr());
				m_api_impl->SetDefUpdateInterval(m_def_update_interval);
			}
			else if (STRN_MATCH(attr_name, "space-factor", attr_name_len))
			{
				UINT space_factor = uni_atoi(attr_value.CStr());
				m_api_impl->SetMaxSizeSpaceFactor(space_factor / 100.0);
			}
		}
	}

	return OpStatus::OK;
}


OP_STATUS
WebFeedStorage::HandleEndTagToken(const XMLToken& token)
{
	// Fetch information about the element.
	const XMLCompleteNameN &elemname = token.GetName();

	const uni_char *elm_name = elemname.GetLocalPart();
	UINT elm_name_len = elemname.GetLocalPartLength();

	if (STRN_MATCH(elm_name, "entry", elm_name_len))
	{
		OP_ASSERT(m_current_feed);
		OP_ASSERT(m_current_entry);
		OP_ASSERT(m_is_feed_file);
		
		if (!m_is_feed_file)
			return OpStatus::OK;

		if (m_current_feed && m_current_entry)
			RETURN_IF_LEAVE(m_current_feed->AddEntryL(m_current_entry, m_current_entry->GetId()));
		else
		{
			OP_ASSERT(FALSE);
		}

		if (m_current_entry)
			m_current_entry->DecRef();
		m_current_entry = NULL;

		if (m_current_content)
			m_current_content->DecRef();
		m_current_content = NULL;
	}
	else if (STRN_MATCH(elm_name, "feed", elm_name_len))
	{
		if (!m_is_feed_file)
		{
			// all saved feeds in store are subscribed
			m_current_stub->SetSubscribed(TRUE);

			m_api_impl->AddFeed(m_current_stub);

			m_current_feed = NULL;
			m_own_stub = FALSE;
		}
		m_current_stub = NULL;
	}
	else if (STRN_MATCH(elm_name, "content", elm_name_len))
	{
		OP_ASSERT(m_current_content && m_current_feed);
		if (m_current_content && m_current_feed)
		{
			if (m_in_title)
			{
				if (m_current_entry)
					m_current_entry->SetTitle(m_current_content);
				else if (m_current_feed)
					m_current_feed->SetParsedTitle(m_current_content);

				m_in_title = FALSE;
			}
			else if (!m_current_prop.IsEmpty())
			{
				if (m_current_entry)
				{
					RETURN_IF_ERROR(m_current_entry->AddProperty(m_current_prop.CStr(), m_current_content));
				}

				m_current_prop.Empty();
			}
			else if (m_current_entry)
				m_current_entry->SetContent(m_current_content);

			m_current_content->DecRef();
			m_current_content = NULL;
		}
	}
	else if (STRN_MATCH(elm_name, "title", elm_name_len))
	{
		if (m_current_content)
		{
			if (m_current_entry)
				m_current_entry->SetTitle(m_current_content);
			else if (m_current_feed)
				m_current_feed->SetParsedTitle(m_current_content);
			else if (m_current_stub && !m_current_content->IsBinary())
				m_current_stub->SetTitle(m_current_content->Data());

			m_current_content->DecRef();
			m_current_content = NULL;
		}

		m_in_title = FALSE;
	}
	else if (STRN_MATCH(elm_name, "prop", elm_name_len))
	{
		if (m_current_entry && !m_current_prop.IsEmpty())
		{
			RETURN_IF_ERROR(m_current_entry->AddProperty(m_current_prop.CStr(), m_current_content));
		}

		m_current_prop.Empty();
		if (m_current_content)
			m_current_content->DecRef();
		m_current_content = NULL;
	}
	else if (STRN_MATCH(elm_name, "store", elm_name_len))
	{
		m_api_impl->SetNextFreeFeedId(m_first_free_id);
	}

	return OpStatus::OK;
}


OP_STATUS
WebFeedStorage::HandleTextToken(const XMLToken& token)
{
	if (m_current_content)
	{
		if (token.GetType() == XMLToken::TYPE_CDATA)
		{
			//TODO: make this work for binary stuff as well
			const uni_char* allocated_value = token.GetLiteralAllocatedValue();
			if (!allocated_value)
				return OpStatus::ERR_NO_MEMORY;

			m_current_content->SetValue((unsigned char*)allocated_value, uni_strlen(allocated_value) * sizeof(uni_char));
		}
		else
		{
			const uni_char* allocated_value = token.GetLiteralAllocatedValue();
			if (!allocated_value)
				return OpStatus::ERR_NO_MEMORY;

			m_current_content->SetValue((unsigned char*)allocated_value, uni_strlen(allocated_value) * sizeof(uni_char));
		}
	}

	return OpStatus::OK;
}

OP_STATUS
WebFeedStorage::AddNewContent()
{
	if (m_current_content)
	{
		OP_ASSERT(FALSE);
		m_current_content->DecRef();
	}

	m_current_content = OP_NEW(WebFeedContentElement, ());
	if (!m_current_content)
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

#endif // WEBFEEDS_SAVED_STORE_SUPPORT
