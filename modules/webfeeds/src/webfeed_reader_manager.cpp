// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.

#include "core/pch.h"

#ifdef WEBFEEDS_EXTERNAL_READERS

#include "modules/webfeeds/src/webfeed_reader_manager.h"
#include "modules/formats/uri_escape.h"
#include "modules/util/opstrlst.h"
#include "modules/util/opfile/opfile.h"

OP_STATUS WebFeedReaderManager::Init()
{
	OpFile file; // Read only

	RETURN_IF_ERROR(file.Construct(UNI_L("feedreaders.ini"), OPFILE_INI_FOLDER));

	PrefsFile prefsfile(PREFS_INI);

	RETURN_IF_LEAVE(
		prefsfile.ConstructL();
		prefsfile.SetFileL(&file);
	);

	prefsfile.SetReadOnly();

	RETURN_IF_ERROR(Init(&prefsfile, TRUE));

	file.Close();

#if defined WEBFEEDS_ADD_REMOVE_EXTERNAL_READERS
	RETURN_IF_ERROR(file.Construct(UNI_L("feedreaders.ini"), OPFILE_WEBFEEDS_FOLDER));

	RETURN_IF_LEAVE(
		m_prefsfile.ConstructL();
		m_prefsfile.SetFileL(&file);
	);

	RETURN_IF_ERROR(Init(&m_prefsfile, FALSE));
#endif

	return OpStatus::OK;
}

OP_STATUS WebFeedReaderManager::Init(PrefsFile* prefs_file, BOOL preinstalled)
{
	if (!prefs_file)
		return OpStatus::ERR;

	RETURN_IF_LEAVE(prefs_file->LoadAllL());

	// Read sections, each section is a webfeed reader
	OpString_list section_list;
	RETURN_IF_LEAVE(prefs_file->ReadAllSectionsL(section_list));

	for (unsigned long i = 0; i < section_list.Count(); i++)
	{
		OpAutoPtr<WebfeedReader> reader (OP_NEW(WebfeedReader, ()));
		if (!reader.get())
			return OpStatus::ERR_NO_MEMORY;

		// Get name, ID and URL for reader
		RETURN_IF_ERROR(reader->name.Set(section_list[i]));

		RETURN_IF_LEAVE(reader->id = prefs_file->ReadIntL(reader->name, UNI_L("ID"), 0));
		RETURN_IF_LEAVE(prefs_file->ReadStringL(reader->name, UNI_L("URL"), reader->subscribe_url));

		if (preinstalled)
			reader->source = READER_PREINSTALLED;
		else
		{
			reader->id = GetFreeId();
			reader->source = READER_CUSTOM;
		}

		// Only add valid readers to list
		if (reader->id > 0 && reader->name.HasContent() && reader->subscribe_url.HasContent())
		{
			RETURN_IF_ERROR(m_readers.Add(reader.get()));
			reader.release();
		}
	}

	return OpStatus::OK;
}

unsigned WebFeedReaderManager::GetCount() const
{
	return m_readers.GetCount();
}

unsigned WebFeedReaderManager::GetIdByIndex(unsigned index) const
{
	return m_readers.Get(index)->id;
}

OP_STATUS WebFeedReaderManager::GetName(unsigned id, OpString& name) const
{
	WebfeedReader* reader = GetReaderById(id);
	if (!reader)
		return OpStatus::ERR;

	return name.Set(reader->name);
}


OP_STATUS WebFeedReaderManager::GetSource(unsigned id, ReaderSource& source) const
{
	WebfeedReader* reader = GetReaderById(id);
	if (!reader)
		return OpStatus::ERR;

	source = reader->source;

	return OpStatus::OK;
}


OP_STATUS WebFeedReaderManager::GetTargetURL(unsigned id, const URL& feed_url, URL& target_url) const
{
	OpString feed_url_string;

	RETURN_IF_ERROR(feed_url.GetAttribute(URL::KUniName_Escaped, feed_url_string));
	if (feed_url_string.IsEmpty())
		return OpStatus::ERR;

	WebfeedReader* reader = GetReaderById(id);
	if (!reader)
		return OpStatus::ERR;

	// encode feed URL
	OpString encoded_holder;
	const uni_char* unencoded = feed_url_string.CStr();

	uni_char* encoded = encoded_holder.Reserve(feed_url_string.Length() * 3);
	if (!encoded)
		return OpStatus::ERR_NO_MEMORY;

	while (*unencoded)
	{
		switch (*unencoded)
		{
			case ':':
			case '/':
			case '?':
			case '&':
			case '=':
				*encoded++ = '%';
				*encoded++ = UriEscape::EscapeFirst(static_cast<char>(*unencoded));
				*encoded++ = UriEscape::EscapeLast(static_cast<char>(*unencoded));
				break;
			default:
				*encoded++ = *unencoded;
		}
		unencoded++;
	}
	*encoded = 0;

	// Create target URL
	OpString target_url_str;
	target_url_str.Empty();
	RETURN_IF_ERROR(target_url_str.AppendFormat(reader->subscribe_url.CStr(), encoded_holder.CStr()));

	target_url = g_url_api->GetURL(target_url_str.CStr());

	return OpStatus::OK;
}


WebFeedReaderManager::WebfeedReader* WebFeedReaderManager::GetReaderById(unsigned id) const
{
	WebfeedReader* reader = 0;

	for (unsigned i = 0; i < m_readers.GetCount() && !reader; i++)
	{
		if (m_readers.Get(i)->id == id)
			reader = m_readers.Get(i);
	}

	return reader;
}

unsigned WebFeedReaderManager::GetFreeId()
{
	unsigned candidate = m_next_free_custom_id;

	if (candidate == 0)
	{
		candidate = m_custom_webfeed_id_origin;

		unsigned count = m_readers.GetCount();
		for (unsigned i=0; i<count; i++)
		{
			unsigned val = GetIdByIndex(i);
			if (candidate <= val)
				candidate = val+1;
		}
	}

	m_next_free_custom_id = candidate + 1;

	return candidate;
}





#ifdef WEBFEEDS_ADD_REMOVE_EXTERNAL_READERS
OP_STATUS WebFeedReaderManager::AddReader(unsigned id, const uni_char *feed_url, const uni_char *feed_name)
{
	OpAutoPtr<WebfeedReader> ap_reader (OP_NEW(WebfeedReader, ()));
	if (!ap_reader.get())
		return OpStatus::ERR_NO_MEMORY;

	ap_reader->id = id;
	ap_reader->source = READER_CUSTOM;
	RETURN_IF_ERROR(ap_reader->name.Set(feed_name));
	RETURN_IF_ERROR(ap_reader->subscribe_url.Set(feed_url));

	// Only add valid readers to list
	if (ap_reader->id > 0 && ap_reader->name.HasContent() && ap_reader->subscribe_url.HasContent())
	{
		RETURN_IF_ERROR(m_readers.Add(ap_reader.get()));
		WebfeedReader *reader = ap_reader.release();
#ifdef PREFSFILE_WRITE
		RETURN_IF_LEAVE(RETURN_IF_ERROR(m_prefsfile.WriteStringL(reader->name, UNI_L("URL"), reader->subscribe_url));
		                RETURN_IF_ERROR(m_prefsfile.WriteIntL(reader->name, UNI_L("ID"), reader->id));
		                m_prefsfile.CommitL(FALSE, FALSE)
		);
#endif // PREFSFILE_WRITE
	}

	return OpStatus::OK;
}

BOOL WebFeedReaderManager::HasReader(const uni_char *feed_url, unsigned *id) const
{
	if (id)
		*id = UINT_MAX;

	for (UINT32 idx = 0; idx < m_readers.GetCount(); idx++)
	{
		WebfeedReader* reader = m_readers.Get(idx);
		if (reader->subscribe_url.CompareI(feed_url) == 0)
		{
			if (id)
				*id = reader->id;
			return TRUE;
		}
	}

	return FALSE;
}

OP_STATUS WebFeedReaderManager::DeleteReader(unsigned id)
{
	WebfeedReader* reader = GetReaderById(id);

	if (!reader)
		return OpStatus::OK;

	if (reader->source != READER_CUSTOM)
		return OpStatus::ERR;

	RETURN_IF_ERROR(m_readers.RemoveByItem(reader));

#ifdef PREFSFILE_WRITE
	RETURN_IF_LEAVE(m_prefsfile.DeleteSectionL(reader->name.CStr());
	                m_prefsfile.CommitL(FALSE, FALSE)
	);
#endif // PREFSFILE_WRITE

	OP_DELETE(reader);

	return OpStatus::OK;
}

#endif // WEBFEEDS_ADD_REMOVE_EXTERNAL_READERS
#endif // WEBFEEDS_EXTERNAL_READERS
