// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 2005-2011 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.

#include "core/pch.h"

#ifdef WEBFEEDS_BACKEND_SUPPORT

#include "modules/webfeeds/src/webfeedentry.h"
#include "modules/webfeeds/src/webfeedparser.h"
#include "modules/webfeeds/src/webfeedutil.h"
#include "modules/webfeeds/src/webfeedstorage.h"

#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/util/str.h"
#include "modules/url/url2.h"

#ifdef DOC_PREFETCH_API
# include "modules/doc/prefetch.h"
#endif // DOC_PREFETCH_API

// ***************************************************************************
//
//	WebFeedPropElm
//
// ***************************************************************************

WebFeedPropElm::~WebFeedPropElm()
{
	if (m_content)
		m_content->DecRef();
	OP_DELETEA(m_prop);
}

OP_STATUS
WebFeedPropElm::Init(const uni_char* property_name, WebFeedContentElement* content)
{
	OP_ASSERT(content && property_name);
	m_content = content;
	m_content->IncRef();

	m_prop = UniSetNewStr(property_name);
	if (!m_prop)
		return OpStatus::ERR_NO_MEMORY;
	return OpStatus::OK;
}

void
WebFeedPropElm::SetContent(WebFeedContentElement* new_content)
{
	if (new_content == m_content)
		return;

	if (m_content)
		m_content->DecRef();
	m_content = new_content;
	if (m_content)
		m_content->IncRef();
}

// ***************************************************************************
//
//	WebFeedPersonElement
//
// ***************************************************************************

OP_STATUS WebFeedPersonElement::SetName(const OpStringC& name)
{
	return WebFeedUtil::StripTags(name.CStr(), m_name);
}

// ***************************************************************************
//
//	WebFeedContentElement
//
// ***************************************************************************


WebFeedContentElement::~WebFeedContentElement()
{
	OP_ASSERT(m_reference_count == 0);

	if(m_value)
		OP_DELETEA(m_value);
	m_value = NULL;
	m_value_length = 0;
}

OpFeedContent* WebFeedContentElement::IncRef()
{
	m_reference_count++;
	return this;
}

void WebFeedContentElement::DecRef()
{
	OP_ASSERT(m_reference_count > 0);

	if (--m_reference_count == 0)
		OP_DELETE(this);
}

ES_Object* WebFeedContentElement::GetDOMObject(DOM_Environment* environment)
{
	ES_Object* dom_content = NULL;
	if (m_dom_objects.GetData(environment, &dom_content) == OpStatus::OK)
		return dom_content;

	return NULL;
}

void WebFeedContentElement::SetDOMObject(ES_Object *dom_obj, DOM_Environment* environment)
{
	ES_Object* dummy;
	// Have to remove entry first, because if it already exists adding it again will result in error, and there is no update method
	// Ignore error from removing something which doesn't exist
	m_dom_objects.Remove(environment, &dummy);
	m_dom_objects.Add(environment, dom_obj);
}

const uni_char* WebFeedContentElement::Data() const
{
	return (uni_char*)m_value;
}

void WebFeedContentElement::Data(const unsigned char*& buffer, UINT& buffer_length) const
{
	buffer = m_value;
	buffer_length = m_value_length;
}

const uni_char* WebFeedContentElement::MIME_Type() const
{
	return m_type.CStr();
}

const uni_char* WebFeedContentElement::GetBaseURI() const
{
	if (m_base_uri.HasContent())
		return m_base_uri.CStr();
	return NULL;
}


BOOL WebFeedContentElement::IsPlainText() const
{
	return m_type.IsEmpty() || m_type.CompareI("text/plain") == 0;
}

BOOL WebFeedContentElement::IsMarkup() const
{
	return m_type.CompareI("text/html") == 0 || IsXML();
}

BOOL WebFeedContentElement::IsXML() const
{
	if (!HasType())
		return FALSE;

	const uni_char* type = m_type.CStr();
	UINT type_len = m_type.Length();
	return (uni_strni_eq_lower(type, UNI_L("application/"), 12) &&  // starts with "application/"
			uni_stri_eq(type + (type_len - 3), "XML"));             // ends with "xml"
}

BOOL WebFeedContentElement::IsBinary() const
{
	return HasType() && !IsMarkup() && !IsPlainText();
}

#ifdef WEBFEEDS_SAVED_STORE_SUPPORT
UINT WebFeedContentElement::GetApproximateSaveSize()
{
	UINT size = 32;

	if (HasValue())
		if (IsBinary())
			size += (UINT)(m_value_length * 0.75);
		else  // content is saved in UTF-8.  With mostly western characters
		      // it will probably use less space than this, with others probably more
			size += (UINT)(m_value_length * 0.75);

	return size;
}

void WebFeedContentElement::SaveL(WriteBuffer &buf, BOOL force_tags)
{
	BOOL is_plain_text = IsPlainText();
	BOOL in_cdata = FALSE;

	if (!is_plain_text || force_tags)
	{
		if (!is_plain_text)
		{
			buf.Append("<content type=\"");
			buf.AppendOpString(&m_type);
			buf.Append("\"><![CDATA[");
			in_cdata = TRUE;
		}
		else
			buf.Append("<content>");
	}

	if (HasValue())
	{
		if (!IsBinary())
			if (in_cdata)
				buf.Append(Data());
			else
				buf.AppendQuotedL(Data());
		else
			buf.AppendBinaryL(m_value, m_value_length);
	}

	if (!is_plain_text || force_tags)
	{
		if (in_cdata)
			buf.Append("]]>");

		buf.Append("</content>");
	}
}
#endif // WEBFEEDS_SAVED_STORE_SUPPORT

void WebFeedContentElement::SetValue(unsigned char* data, UINT data_length)
{
	if (m_value)
		OP_DELETEA(m_value);
	m_value = data;
	m_value_length = data_length;
}

void WebFeedContentElement::SetValue(uni_char* data, UINT data_length)
{
	if (m_value)
		OP_DELETEA(m_value);
	m_value = (unsigned char*)data;
	m_value_length = data_length * sizeof(uni_char);
}

OP_STATUS WebFeedContentElement::SetValue(const OpStringC& value)
{
	// copy the buffer:
	uni_char* cstr = UniSetNewStr(value.CStr());
	if (!cstr)
		return OpStatus::ERR_NO_MEMORY;

	if (m_value)
		OP_DELETEA(m_value);
	m_value = (unsigned char*)cstr;
	m_value_length = uni_strlen(cstr) * sizeof(uni_char);

	return OpStatus::OK;
}

#ifdef OLD_FEED_DISPLAY
#ifdef WEBFEEDS_DISPLAY_SUPPORT
void WebFeedContentElement::WriteAsStrippedHTMLL(URL& out_url) const
{
	if (!HasValue())
		return;

	if (!IsMarkup())  // no markup to strip
	{
		WriteAsHTMLL(out_url);
		return;
	}

	OpString stripped; ANCHOR(OpString, stripped);
	LEAVE_IF_ERROR(WebFeedUtil::StripTags((uni_char*)m_value, stripped));
	LEAVE_IF_ERROR(out_url.WriteDocumentData(URL::KNormal, stripped));
}

void WebFeedContentElement::WriteAsHTMLL(URL& out_url) const
{
	if (HasValue())
	{
		if (IsPlainText())
			LEAVE_IF_ERROR(out_url.WriteDocumentData(URL::KHTMLify, (uni_char*)m_value, m_value_length / sizeof(uni_char)));
		else if (IsMarkup())
		{
			OpString stripped_html; ANCHOR(OpString, stripped_html);
			WebFeedUtil::StripTags((const uni_char*)m_value, stripped_html, UNI_L("style"), TRUE);
			LEAVE_IF_ERROR(out_url.WriteDocumentData(URL::KNormal, stripped_html, stripped_html.Length()));
		}
		else
			OP_ASSERT(!"What to do, what to do, with this strange content type");
	}
}
#endif // WEBFEEDS_DISPLAY_SUPPORT
#endif // OLD_FEED_DISPLAY

// ***************************************************************************
//
//	WebFeedEntry
//
// ***************************************************************************

WebFeedEntry::WebFeedEntry(WebFeed *parent_feed /* = NULL */) :
	m_reference_count(1),  // the one doing new has a reference.
	m_title(NULL), m_author(NULL), m_content(NULL), m_feed(parent_feed), m_id(0),
	m_keep_item(FALSE), m_mark_for_removal(FALSE), m_status(STATUS_UNREAD),
	m_pubdate(0.0), m_properties(NULL)
{
}

OP_STATUS WebFeedEntry::Init()
{
	OP_ASSERT(m_title == NULL && m_author == NULL && m_content == NULL);

	m_title = OP_NEW(WebFeedContentElement, ());
	m_author = OP_NEW(WebFeedPersonElement, ());
	m_content = OP_NEW(WebFeedContentElement, ());

	if (!m_title || !m_author || !m_content)
	{
		if (m_title)
			m_title->DecRef();
		OP_DELETE(m_author);
		if (m_content)
			m_content->DecRef();
		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}


OpFeedEntry* WebFeedEntry::IncRef()
{
	m_feed->IncRef();
	m_reference_count++;

	return this;
}

void WebFeedEntry::DecRef()
{
	// Entries should usually have a m_feed, but may not if they were never
	// inserted into one.  Still need to be able to delete them.
	WebFeed* feed = m_feed;  // because this pointer might be deleted further down

	if (--m_reference_count == 0)
	{
		if (feed)
		{
			// this only happens if the store is already destroyed
			OP_ASSERT(g_webfeeds_api == NULL);
			feed->RemoveEntry(this);   // will delete this
		}
		else  // happens if the entry was never inserted in a feed, just delete it
			OP_DELETE(this);
	}
	else if (feed && m_reference_count == 1 && m_mark_for_removal)
		feed->RemoveEntry(this);   // will delete this

	if (feed)
		feed->DecRef();   // one less reference to the feed/entry system -
						  // this might delete the entry
}

ES_Object* WebFeedEntry::GetDOMObject(DOM_Environment* environment)
{
	ES_Object* dom_obj;
	if (m_dom_objects.GetData(environment, &dom_obj) == OpStatus::OK)
		return dom_obj;

	return NULL;
}

void WebFeedEntry::SetDOMObject(ES_Object *dom_obj, DOM_Environment* environment)
{
	ES_Object* dummy;
	// Have to remove entry first, because if it already exists adding it again will result in error, and there is no update method
	// Ignore error from removing something which doesn't exist
	m_dom_objects.Remove(environment, &dummy);
	m_dom_objects.Add(environment, dom_obj);
}

void WebFeedEntry::MarkForRemoval()
{
	OP_ASSERT(m_reference_count >= 1);

	m_mark_for_removal = TRUE;
	m_feed->RemoveEntryFromCount(this);
	if (m_reference_count == 1)
		m_feed->RemoveEntry(this);
}

WebFeedEntry::~WebFeedEntry()
{
	OP_ASSERT(m_reference_count == 1 || (m_feed == NULL && m_reference_count == 0));  // the feed object should hold the last reference if it exists
	OP_ASSERT(m_title && m_content);

	if (m_title)
		m_title->DecRef();
	if (m_content)
		m_content->DecRef();

	OP_DELETE(m_author);
	OP_DELETE(m_properties);

#ifdef _DEBUG
	m_title = (WebFeedContentElement*)0xdeadbeef;
	m_author = (WebFeedPersonElement*)0xdeadbeef;
	m_content = (WebFeedContentElement*)0xdeadbeef;

	m_feed = (WebFeed*)0xdeadbeef;
#endif
}

OpFeedEntry::EntryId WebFeedEntry::GetId() { return m_id; }
void WebFeedEntry::SetId(EntryId id) { m_id = id; }

#ifdef OLD_FEED_DISPLAY
OP_STATUS WebFeedEntry::GetEntryIdURL(OpString& res)
{
	RETURN_IF_ERROR(GetFeed()->GetFeedIdURL(res));
	RETURN_IF_ERROR(res.AppendFormat(UNI_L("-%08x"), GetId()));

	return OpStatus::OK;
}
#endif // OLD_FEED_DISPLAY

const uni_char* WebFeedEntry::GetGuid()
{
	if (!m_guid.IsEmpty())
		return m_guid.CStr();
	else
		return NULL;
}
OP_STATUS WebFeedEntry::SetGuid(const uni_char* guid) { return m_guid.Set(guid); }

OpFeedEntry::ReadStatus WebFeedEntry::GetReadStatus() { return m_status; }
void WebFeedEntry::SetReadStatus(ReadStatus status) {
	if (m_status == status)
		return;

	if (m_feed)
	{
		if (m_status == STATUS_UNREAD)
			m_feed->MarkOneEntryNotUnread();
		else if (status == STATUS_UNREAD)
			m_feed->MarkOneEntryUnread();
	}

	m_status = status;
}

OpFeedEntry* WebFeedEntry::GetPrevious(BOOL unread_only)
{
	WebFeedEntry* prev = this;
	while ((prev = (WebFeedEntry*)prev->Pred()))
		if (!prev->MarkedForRemoval()
			&& (!unread_only || prev->GetReadStatus() != OpFeedEntry::STATUS_UNREAD))
		{
			return prev;  // found an acceptable prev
		}

	return NULL;
}

OpFeedEntry* WebFeedEntry::GetNext(BOOL unread_only)
{
	WebFeedEntry* next = this;
	while ((next = (WebFeedEntry*)next->Suc()))
		if (!next->MarkedForRemoval()
			&& (!unread_only || next->GetReadStatus() != OpFeedEntry::STATUS_UNREAD))
		{
			return next;  // found an acceptable next
		}

	return NULL;
}

OpFeed*	WebFeedEntry::GetFeed() { return (OpFeed*) m_feed; }

BOOL WebFeedEntry::GetKeep() { return m_keep_item; }
void WebFeedEntry::SetKeep(BOOL keep) { m_keep_item = keep; }

OpFeedContent* WebFeedEntry::GetTitle() { return m_title; }
void WebFeedEntry::SetTitle(WebFeedContentElement* title)
{
	if (title == m_title)
		return;

	if (m_title)
		m_title->DecRef();

	m_title = title;
	if(m_title)
		m_title->IncRef();
}

const uni_char* WebFeedEntry::GetAuthor() { return m_author->Name().CStr(); }
const uni_char* WebFeedEntry::GetAuthorEmail() { return m_author->Email().CStr(); }
WebFeedPersonElement* WebFeedEntry::AuthorElement() { return m_author; }

OpFeedContent* WebFeedEntry::GetContent() { return m_content; }
void WebFeedEntry::SetContent(WebFeedContentElement* content)
{
	if (content == m_content)
		return;

	if (m_content)
		m_content->DecRef();

	m_content = content;
	if (m_content)
		m_content->IncRef();
}

double WebFeedEntry::GetPublicationDate() { return m_pubdate; }
void WebFeedEntry::SetPublicationDate(double date) { m_pubdate = date; }

#ifdef WEBFEEDS_SAVED_STORE_SUPPORT
UINT WebFeedEntry::GetApproximateSaveSize()
{
	UINT size = 0;
	const UINT fixed_content_size = 128;

	size += fixed_content_size;
	size += m_title->GetApproximateSaveSize();
	size += m_content->GetApproximateSaveSize();

	if (m_properties)
		for (WebFeedPropElm *prop = (WebFeedPropElm *)m_properties->First(); prop; prop = (WebFeedPropElm *)prop->Suc())
			size += 32 + prop->GetContent()->GetApproximateSaveSize();

	return size;
}


void WebFeedEntry::SaveL(WriteBuffer &buf)
{
	buf.Append("\n<entry id=\"");
	buf.Append(m_id);
	buf.Append("\"");

	if (m_status == STATUS_READ)
		buf.Append(" read=\"yes\"");

	if (m_keep_item)
		buf.Append(" keep=\"yes\"");

	if (GetGuid())
	{
		buf.Append(" guid=\"");
		buf.AppendQuotedL(GetGuid());
		buf.Append("\"");
	}
	buf.Append(">\n");

	if (m_title)
	{
		buf.Append("<title>");
		m_title->SaveL(buf, FALSE);
		buf.Append("</title>\n");
	}

	if (m_pubdate != 0.0)
	{
		buf.Append("<date type=\"published\" value=\"");
		buf.AppendDate(m_pubdate);
		buf.Append("\"/>\n");
	}

	if (!GetPrimaryLink().IsEmpty())
	{
		OpString8 link;
		GetPrimaryLink().GetAttributeL(URL::KName_With_Fragment_Username_Password_Escaped_NOT_FOR_UI, link);

		buf.Append("<url value=\"");
		buf.AppendQuotedL(link.CStr());
		buf.Append("\"/>");
	}

	if (m_properties)
	{
		WebFeedPropElm *prop = (WebFeedPropElm *)m_properties->First();
		while (prop)
		{
			buf.Append("<prop name=\"");
			buf.Append(prop->GetName());
			buf.Append("\">");

			prop->GetContent()->SaveL(buf, FALSE);

			buf.Append("</prop>\n");

			prop = (WebFeedPropElm *)prop->Suc();
		}
	}

	if (m_content)
		m_content->SaveL(buf, TRUE);

	buf.Append("\n</entry>\n");
}
#endif // WEBFEEDS_SAVED_STORE_SUPPORT

WebFeedPropElm*
WebFeedEntry::GetPropEntry(const uni_char* prop)
{
	if (!m_properties)
		return NULL;

	WebFeedPropElm *tmp = (WebFeedPropElm *)m_properties->First();
	while (tmp && !tmp->IsEqual(prop))
		tmp = (WebFeedPropElm *)tmp->Suc();

	return tmp;
}

const uni_char*
WebFeedEntry::GetProperty(const uni_char* property)
{
	OpFeedContent *content = GetPropertyContent(property);
	if (content)
		return content->Data();

	return NULL;
}

OpFeedContent*
WebFeedEntry::GetPropertyContent(const uni_char* property)
{
	WebFeedPropElm *entry = GetPropEntry(property);
	if (entry)
		return entry->GetContent();

	return NULL;
}

OP_STATUS
WebFeedEntry::AddProperty(const uni_char* property, WebFeedContentElement* prop)
{
	if (!m_properties)
	{
		m_properties = OP_NEW(AutoDeleteHead, ());
		if (!m_properties)
			return OpStatus::ERR_NO_MEMORY;
	}

	// Check if property exists
	WebFeedPropElm *old_prop = GetPropEntry(property);
	if (old_prop)
	{
		old_prop->SetContent(prop);
		return OpStatus::OK;
	}

	WebFeedPropElm *new_entry = OP_NEW(WebFeedPropElm, ());
	if (!new_entry || OpStatus::IsMemoryError(new_entry->Init(property, prop)))
	{
		OP_DELETE(new_entry);
		return OpStatus::ERR_NO_MEMORY;
	}

	new_entry->Into(m_properties);

	return OpStatus::OK;
}

#ifdef OLD_FEED_DISPLAY
#ifdef WEBFEEDS_DISPLAY_SUPPORT
inline void AddHTMLL(URL& out_url, const uni_char* data)
{
	LEAVE_IF_ERROR(out_url.WriteDocumentData(URL::KNormal, data));
}

void WebFeedEntry::WriteNavigationHeaderL(URL& out_url)
{
	OpFeedEntry* next = GetNext(FALSE);
	OpFeedEntry* prev = GetPrevious(FALSE);
	OpString localized_string; ANCHOR(OpString, localized_string);

	// Write next/previous entry links, like this:  << | all entries | >>
	AddHTMLL(out_url, UNI_L("<ol class=\"navigation\">\n"));
	if (prev)
	{
#if LANGUAGE_DATABASE_VERSION >= 842
		int length = g_languageManager->GetStringL(Str::S_WEBFEEDS_PREVIOUS_ENTRY, localized_string);
		if (!length)
#endif
			localized_string.SetL("");

		out_url.WriteDocumentDataUniSprintf(UNI_L("<li><a rel=\"prev\" href=\"opera:feed-id/%08x-%08x\" title=\"%s\">&laquo;&laquo;</a></li>\n"), m_feed->GetId(), prev->GetId(), localized_string.CStr());
	}
	else
		AddHTMLL(out_url, UNI_L("<li>&laquo;&laquo;</li>"));

#if LANGUAGE_DATABASE_VERSION >= 842
	int length = g_languageManager->GetStringL(Str::S_WEBFEEDS_ALL_ENTRIES, localized_string);
	if (!length)
#endif
		localized_string.SetL("");
	OpString feed_id_url;  ANCHOR(OpString, feed_id_url);
	LEAVE_IF_ERROR(m_feed->GetFeedIdURL(feed_id_url));
	out_url.WriteDocumentDataUniSprintf(UNI_L("<li><a href=\"%s\">%s</a></li>\n"), feed_id_url.CStr(), localized_string.CStr());

	if (next)
	{
#if LANGUAGE_DATABASE_VERSION >= 842
		int length = g_languageManager->GetStringL(Str::S_WEBFEEDS_NEXT_ENTRY, localized_string);
		if (!length)
#endif
			localized_string.SetL("");
		out_url.WriteDocumentDataUniSprintf(UNI_L("<li><a rel=\"next\" href=\"opera:feed-id/%08x-%08x\" title=\"%s\">&raquo;&raquo;</a></li>\n"), m_feed->GetId(), next->GetId(), localized_string.CStr());
	}
	else
		AddHTMLL(out_url, UNI_L("<li>&raquo;&raquo;</li>"));

	AddHTMLL(out_url, UNI_L("</ol>\n"));
}

OP_STATUS WebFeedEntry::WriteEntry(URL& out_url, BOOL complete_document, BOOL mark_read)
{
#ifdef OLD_FEED_DISPLAY
	TRAPD(status, WriteEntryL(out_url, complete_document, mark_read));
	return status;
#else
	return OpStatus::ERR_NOT_SUPPORTED;
#endif
}

void WebFeedEntry::WriteEntryL(URL& out_url, BOOL complete_document, BOOL mark_read)
{
	if (complete_document)
	{
		// Get address of style file:
		OpString styleurl; ANCHOR(OpString, styleurl);
		g_pcfiles->GetFileURLL(PrefsCollectionFiles::StyleWebFeedsDisplay, &styleurl);

		// write head
		OpString title; ANCHOR(OpString, title);
		if (m_title->Data())
			LEAVE_IF_ERROR(WebFeedUtil::StripTags(m_title->Data(), title));
		else
			title.SetL(UNI_L("WebFeed"));

		OpString next_url; ANCHOR(OpString, next_url);
		WebFeedEntry* next = (WebFeedEntry*)GetNext(FALSE);
		if (next)
		{
			LEAVE_IF_ERROR(next->GetEntryIdURL(next_url));
			OP_ASSERT(!next_url.IsEmpty());
		}

		OpString prev_url; ANCHOR(OpString, prev_url);
		WebFeedEntry* prev = (WebFeedEntry*)GetPrevious(FALSE);
		if (prev)
		{
			LEAVE_IF_ERROR(prev->GetEntryIdURL(prev_url));
			OP_ASSERT(!prev_url.IsEmpty());
		}

		OpString direction;  ANCHOR(OpString, direction);
		switch (g_languageManager->GetWritingDirection)
		{
		case OpLanguageManager::LTR:
		default:
			direction.SetL("ltr");
			break;

		case OpLanguageManager::RTL:
			direction.SetL("rtl");
			break;
		}

		if (m_content->IsXML())
		{
			AddHTMLL(out_url, UNI_L("\xFEFF"));  // Byte order mark
			AddHTMLL(out_url, UNI_L("<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.1//EN\" \"http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd\">\n"));
			out_url.WriteDocumentDataUniSprintf(UNI_L("<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"%s\" dir=\"%s\">\n"), g_languageManager->GetLanguage().CStr(), direction.CStr());
			out_url.WriteDocumentDataUniSprintf(UNI_L("<head>\n<title>%s</title>\n"), title.CStr());
			out_url.WriteDocumentDataUniSprintf(UNI_L("<link rel=\"stylesheet\" href=\"%s\" type=\"text/css\" media=\"screen,projection,tv,handheld,speech\"/>\n"), styleurl.CStr());

			if (!next_url.IsEmpty())
				out_url.WriteDocumentDataUniSprintf(UNI_L("<link rel=\"next\" href=\"%s\"/>\n"), next_url.CStr());

			if (!prev_url.IsEmpty())
				out_url.WriteDocumentDataUniSprintf(UNI_L("<link rel=\"prev\" href=\"%s\"/>\n"), prev_url.CStr());

			if (m_content->HasBaseURI())
				out_url.WriteDocumentDataUniSprintf(UNI_L("<base href=\"%s\"/>\n"), m_content->GetBaseURI());
		}
		else
		{
			AddHTMLL(out_url, UNI_L("\xFEFF"));  // Byte order mark
			AddHTMLL(out_url, UNI_L("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">\n"));
			out_url.WriteDocumentDataUniSprintf(UNI_L("<html lang=\"%s\" dir=\"%s\">\n"), g_languageManager->GetLanguage().CStr(), direction.CStr());
			out_url.WriteDocumentDataUniSprintf(UNI_L("<head>\n<title>%s</title>\n"), title.CStr());
			out_url.WriteDocumentDataUniSprintf(UNI_L("<link rel=\"stylesheet\" href=\"%s\" type=\"text/css\" media=\"screen,projection,tv,handheld,speech\">\n"), styleurl.CStr());

			if (!next_url.IsEmpty())
				out_url.WriteDocumentDataUniSprintf(UNI_L("<link rel=\"next\" href=\"%s\">\n"), next_url.CStr());

			if (!prev_url.IsEmpty())
				out_url.WriteDocumentDataUniSprintf(UNI_L("<link rel=\"prev\" href=\"%s\">\n"), prev_url.CStr());

			if (m_content->HasBaseURI())
				out_url.WriteDocumentDataUniSprintf(UNI_L("<base href=\"%s\">\n"), m_content->GetBaseURI());
		}

		AddHTMLL(out_url, UNI_L("</head>\n<body>\n"));
	}

	// Write title (if any):
	URL external_url = GetPrimaryLink(); ANCHOR(URL, external_url);
	AddHTMLL(out_url, UNI_L("<h1>\n"));
	if (!external_url.IsEmpty())
		out_url.WriteDocumentDataUniSprintf(UNI_L("<a href=\"%s\">"), external_url.GetUniName(FALSE, PASSWORD_SHOW));
	if (m_title->HasValue())
		m_title->WriteAsStrippedHTMLL(out_url);
	if (!external_url.IsEmpty())
		AddHTMLL(out_url, UNI_L("\n</a>"));
	AddHTMLL(out_url, UNI_L("\n</h1>\n"));

	// Write next/previous entry links:
	WriteNavigationHeaderL(out_url);

	// Write date
	if (m_pubdate != 0.0)
	{
		uni_char* pubdate = WebFeedUtil::TimeToString(m_pubdate);
		LEAVE_IF_NULL(pubdate);

		out_url.WriteDocumentDataUniSprintf(UNI_L("<ins>%s</ins>"), pubdate);
		OP_DELETEA(pubdate);
	}

	// Write content:
	AddHTMLL(out_url, UNI_L("<div dir=\"ltr\">\n"));
	if (m_content->HasValue())
		m_content->WriteAsHTMLL(out_url);
	AddHTMLL(out_url, UNI_L("\n</div>\n"));

	if (complete_document)
	{
		// Finish up page:
		AddHTMLL(out_url, UNI_L("</body>\n</html>\n"));
		out_url.SetAttribute(URL::KForceContentType, URL_HTML_CONTENT);
		out_url.SetAttribute(URL::KCachePolicy_MustRevalidate, TRUE);
		out_url.SetAttribute(URL::KSuppressScriptAndEmbeds, TRUE);

		out_url.SetIsGeneratedByOpera();
		out_url.WriteDocumentDataFinished();
		out_url.ForceStatus(URL_LOADED);
	}

	if (mark_read)
		SetReadStatus(STATUS_READ);
}
#endif // WEBFEEDS_DISPLAY_SUPPORT
#endif // OLD_FEED_DISPLAY

OpFeedLinkElement* WebFeedEntry::GetLink(UINT32 index) const
{
	return m_links.Get(index);
}

OP_STATUS WebFeedEntry::AddNewLink(WebFeedLinkElement::LinkRel relationship, WebFeedLinkElement*& new_link)
{
	OpStackAutoPtr<WebFeedLinkElement> link(OP_NEW(WebFeedLinkElement, (relationship)));
	if (!link.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(m_links.Add(link.get()));

	new_link = link.release();
	return OpStatus::OK;
}

OP_STATUS WebFeedEntry::SetAlternateLink(WebFeedLinkElement*& alternate_link)
{
	for (unsigned i=0; i < m_links.GetCount(); i++)
	{
		if (m_links.Get(i)->Relationship() == OpFeedLinkElement::Alternate)
		{
			OpStackAutoPtr<WebFeedLinkElement> link(OP_NEW(WebFeedLinkElement, (OpFeedLinkElement::Alternate)));
			if (!link.get())
				return OpStatus::ERR_NO_MEMORY;

			delete m_links.Get(i);
			RETURN_IF_ERROR(m_links.Replace(i, link.get()));

			alternate_link = link.release();
			return OpStatus::OK;
		}
	}

	RETURN_IF_ERROR(AddAlternateLink(alternate_link));

	return OpStatus::OK;
}

URL WebFeedEntry::GetPrimaryLink()
{
	for (unsigned i=0; i < m_links.GetCount(); i++)
	{
		if (m_links.Get(i)->Relationship() == OpFeedLinkElement::Alternate)
			return m_links.Get(i)->URI();
	}

	return URL();
}

unsigned WebFeedEntry::LinkCount() const
{
	return m_links.GetCount();
}

OP_STATUS WebFeedEntry::PrefetchPrimaryLink()
{
	URL linked_article = GetPrimaryLink();

    if (!linked_article.IsEmpty())
    {
        // TODO: should probably introduce some mechanism to not fetch links for all entries at once
        PreFetcher* fetcher = OP_NEW(PreFetcher, (linked_article, NULL));
        if (!fetcher || OpStatus::IsMemoryError(fetcher->Fetch(m_feed->GetURL())))
        {
            OP_DELETE(fetcher);
            return OpStatus::ERR_NO_MEMORY;
        }
    }

	return OpStatus::OK;
}
#endif  // WEBFEEDS_BACKEND_SUPPORT
