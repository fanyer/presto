/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef SEARCH_ENGINES

#include "modules/encodings/encoders/outputconverter.h"
#include "modules/forms/form.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefsfile/prefsentry.h"
#include "modules/prefsfile/prefssection.h"
#include "modules/formats/uri_escape.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/str.h"
#include "modules/encodings/utility/opstring-encodings.h"

#include "modules/searchmanager/searchmanager.h"


SearchElement::SearchElement()
: m_id(0),
  m_name_number(Str::NOT_A_STRING),
  m_description_number(Str::NOT_A_STRING),
  m_is_post(FALSE),
  m_charset_id(0),
  m_type(SEARCH_TYPE_UNDEFINED),
  m_version(0),
  m_is_deleted(FALSE),
#ifdef USERDEFINED_SEARCHES
  m_belongs_in_userfile(FALSE),
  m_is_changed_this_session(FALSE),
#endif // USERDEFINED_SEARCHES
  m_is_separator(FALSE)
#ifdef SEARCH_PROVIDER_QUERY_SUPPORT
  ,m_spii(this)
#endif
{
}

SearchElement::~SearchElement()
{
	if (m_charset_id)
		g_charsetManager->DecrementCharsetIDReference(m_charset_id);
}
#ifdef SEARCH_PROVIDER_QUERY_SUPPORT
OpSearchProviderListener::SearchProviderInfo* SearchElement::GetSearchProviderInfo()
{
	return &m_spii;
}
#endif //SEARCH_PROVIDER_QUERY_SUPPOR

#ifdef USERDEFINED_SEARCHES
OP_STATUS SearchElement::ConstructSeparator()
{
	Clear();
	m_is_separator = TRUE;
	SetBelongsInUserfile();
	SetIsChangedThisSession(TRUE);
	return OpStatus::OK;
}

OP_STATUS SearchElement::Construct(const OpStringC& name, const OpStringC& url, const OpStringC& key, SearchType type,
                                   const OpStringC& description, BOOL is_post, const OpStringC& post_query,
                                   const OpStringC& charset, const OpStringC& icon_url, const OpStringC& opensearch_xml_url)
{
	OP_STATUS ret;
	if ((ret=m_name_str.Set(name))!=OpStatus::OK ||
	    (ret=m_description_str.Set(description))!=OpStatus::OK ||
	    (ret=m_url.Set(url))!=OpStatus::OK ||
	    (ret=m_key.Set(key))!=OpStatus::OK ||
	    (ret=m_post_query.Set(post_query))!=OpStatus::OK ||
	    (ret=m_icon_url.Set(icon_url))!=OpStatus::OK ||
	    (ret=m_opensearch_xml_url.Set(opensearch_xml_url))!=OpStatus::OK)
	{
		return ret;
	}

	m_id = 0;
	m_section_name.Empty();
	m_name_number = Str::NOT_A_STRING;
	m_description_number = Str::NOT_A_STRING;
	m_is_post = is_post;
	m_charset_id = CharsetToId(charset);
	if (m_charset_id)
		g_charsetManager->IncrementCharsetIDReference(m_charset_id);

	m_type = type;
	m_is_deleted = FALSE;
	m_version = 0;
	m_is_separator = FALSE;
	SetBelongsInUserfile();
	SetIsChangedThisSession(TRUE);
	return OpStatus::OK;
}

OP_STATUS SearchElement::Construct(Str::LocaleString name, const OpStringC& url, const OpStringC& key, SearchType type,
                                   Str::LocaleString description, BOOL is_post, const OpStringC& post_query,
                                   const OpStringC& charset, const OpStringC& icon_url, const OpStringC& opensearch_xml_url)
{
	OP_STATUS ret;
	if ((ret=m_url.Set(url))!=OpStatus::OK ||
	    (ret=m_key.Set(key))!=OpStatus::OK ||
	    (ret=m_post_query.Set(post_query))!=OpStatus::OK ||
	    (ret=m_icon_url.Set(icon_url))!=OpStatus::OK ||
	    (ret=m_opensearch_xml_url.Set(opensearch_xml_url))!=OpStatus::OK)
	{
		return ret;
	}

	m_id = 0;
	m_section_name.Empty();
	m_name_str.Empty();
	m_description_str.Empty();
	m_name_number = name;
	m_description_number = description;
	m_is_post = is_post;
	m_charset_id = CharsetToId(charset);
	if (m_charset_id)
		g_charsetManager->IncrementCharsetIDReference(m_charset_id);

	m_type = type;
	m_is_deleted = FALSE;
	m_version = 0;
	m_is_separator = FALSE;
	SetBelongsInUserfile();
	SetIsChangedThisSession(TRUE);
	return OpStatus::OK;
}
#endif // USERDEFINED_SEARCHES

#ifdef USERDEFINED_SEARCHES
OP_STATUS SearchElement::ConstructL(PrefsFile* prefs_file, const OpStringC& prefs_section, BOOL is_userfile)
#else
OP_STATUS SearchElement::ConstructL(PrefsFile* prefs_file, const OpStringC& prefs_section)
#endif // USERDEFINED_SEARCHES
{
	if (!prefs_file || prefs_section.IsEmpty())
		return OpStatus::ERR_NULL_POINTER;

	Clear();

	m_section_name.SetL(prefs_section);

	m_id = prefs_file->ReadIntL(m_section_name, UNI_L("ID"), 0);
	m_version = prefs_file->ReadIntL(m_section_name, UNI_L("Version"), 0);
	m_is_deleted = prefs_file->ReadBoolL(m_section_name, UNI_L("Deleted"));
	m_is_separator = prefs_file->ReadBoolL(m_section_name, UNI_L("Is Separator"));
	if (m_is_separator)
	{
#ifdef USERDEFINED_SEARCHES
		SetBelongsInUserfile(is_userfile);
		SetIsChangedThisSession(FALSE);
#endif // USERDEFINED_SEARCHES
		return OpStatus::OK;
	}

	prefs_file->ReadStringL(m_section_name, UNI_L("GUID"), m_link_guid);
	m_name_number = Str::LocaleString(prefs_file->ReadIntL(m_section_name, UNI_L("Name ID"), Str::NOT_A_STRING));
	prefs_file->ReadStringL(m_section_name, UNI_L("Name"), m_name_str);
	m_description_number = Str::LocaleString(prefs_file->ReadIntL(m_section_name, UNI_L("Description ID"), Str::NOT_A_STRING));
	prefs_file->ReadStringL(m_section_name, UNI_L("Description"), m_description_str);
	prefs_file->ReadStringL(m_section_name, UNI_L("URL"), m_url);
	prefs_file->ReadStringL(m_section_name, UNI_L("Key"), m_key);
	m_is_post = prefs_file->ReadBoolL(m_section_name, UNI_L("Is Post"));
	prefs_file->ReadStringL(m_section_name, UNI_L("Post Query"), m_post_query);
	m_type = static_cast<SearchType>(prefs_file->ReadIntL(m_section_name, UNI_L("Type"), SEARCH_TYPE_UNDEFINED));
	prefs_file->ReadStringL(m_section_name, UNI_L("Icon URL"), m_icon_url);
	prefs_file->ReadStringL(m_section_name, UNI_L("OpenSearch URL"), m_opensearch_xml_url);

	OpString charset_str;
	prefs_file->ReadStringL(m_section_name, UNI_L("Encoding"), charset_str, UNI_L("utf-8"));
	m_charset_id = CharsetToId(charset_str);
	if (m_charset_id)
		g_charsetManager->IncrementCharsetIDReference(m_charset_id);

#ifdef USERDEFINED_SEARCHES
	SetBelongsInUserfile(is_userfile);
	SetIsChangedThisSession(FALSE);
#endif // USERDEFINED_SEARCHES

	return OpStatus::OK;
}

OP_STATUS SearchElement::GetName(OpString& name, BOOL strip_amp)
{
	OP_STATUS ret;
	if (m_name_str.IsEmpty() && m_name_number!=Str::NOT_A_STRING)
	{
		if ((ret=g_languageManager->GetString(m_name_number, m_name_str)) != OpStatus::OK)
			return ret;
	}

	if ((ret=name.Set(m_name_str)) != OpStatus::OK)
		return ret;

	return strip_amp ? RemoveChars(name, UNI_L("&")) : OpStatus::OK;
}

OP_STATUS SearchElement::GetDescription(OpString& description, BOOL strip_name_amp)
{
	OP_STATUS ret;
	if (m_description_str.IsEmpty() && m_description_number!=Str::NOT_A_STRING)
	{
		if ((ret=g_languageManager->GetString(m_description_number, m_description_str)) != OpStatus::OK)
			return ret;
	}

	if ((ret=description.Set(m_description_str)) != OpStatus::OK)
		return ret;

	int substitute_pos = description.Find(UNI_L("%s"));
	if (substitute_pos == KNotFound)
		return OpStatus::OK; //Nothing to substitute

	OpString name;
	if ((ret=GetName(name, strip_name_amp)) != OpStatus::OK)
		return ret;

	description.Delete(substitute_pos, 2); //Remove %s
	return name.HasContent() ? description.Insert(substitute_pos, name) : OpStatus::OK;
}

#ifdef USERDEFINED_SEARCHES
void SearchElement::SetName(Str::LocaleString name)
{
	if (m_name_number == name)
		return;

	SetBelongsInUserfile();

	m_name_number = name;
	m_name_str.Empty();
}

OP_STATUS SearchElement::SetName(const OpStringC& name)
{
	if (m_name_number==Str::NOT_A_STRING && m_name_str.Compare(name)==0)
		return OpStatus::OK;

	SetBelongsInUserfile();

	m_name_number = Str::NOT_A_STRING;
	return m_name_str.Set(name);
}

void SearchElement::SetDescription(Str::LocaleString description)
{
	if (m_description_number == description)
		return;

	SetBelongsInUserfile();

	m_description_number = description;
	m_description_str.Empty();
}

OP_STATUS SearchElement::SetDescription(const OpStringC& description)
{
	if (m_description_number==Str::NOT_A_STRING && m_description_str.Compare(description)==0)
		return OpStatus::OK;

	SetBelongsInUserfile();

	m_description_number = Str::NOT_A_STRING;
	return m_description_str.Set(description);
}

OP_STATUS SearchElement::SetURL(const OpStringC& url)
{
	if (m_url.Compare(url) == 0)
		return OpStatus::OK;

	SetBelongsInUserfile();

	return m_url.Set(url);
}

OP_STATUS SearchElement::SetKey(const OpStringC& key)
{
	if (m_key.Compare(key) == 0)
		return OpStatus::OK;

	SetBelongsInUserfile();

	return m_key.Set(key);
}

void SearchElement::SetIsPost(BOOL is_post)
{
	if (m_is_post==is_post)
		return;

	SetBelongsInUserfile();

	m_is_post=is_post;
}

OP_STATUS SearchElement::SetPostQuery(const OpStringC& post_query)
{
	if (m_post_query.Compare(post_query) == 0)
		return OpStatus::OK;

	SetBelongsInUserfile();

	return m_post_query.Set(post_query);
}

void SearchElement::SetCharsetId(unsigned short id)
{
	if (id==m_charset_id)
		return;

	SetBelongsInUserfile();

	if (m_charset_id)
		g_charsetManager->DecrementCharsetIDReference(m_charset_id);

	m_charset_id=id;

	if (id)
		g_charsetManager->IncrementCharsetIDReference(id);
}

void SearchElement::SetCharset(const OpStringC& charset)
{
	SetCharsetId(CharsetToId(charset));
}

void SearchElement::SetType(SearchType type)
{
	if (m_type==type)
		return;

	SetBelongsInUserfile();

	m_type=type;
}

OP_STATUS SearchElement::SetIconURL(const OpStringC& icon_url)
{
	if (m_icon_url.Compare(icon_url) == 0)
		return OpStatus::OK;

	SetBelongsInUserfile();

	return m_icon_url.Set(icon_url);
}

OP_STATUS SearchElement::SetOpenSearchXMLURL(const OpStringC& xml_url)
{
	if (m_opensearch_xml_url.Compare(xml_url) == 0)
		return OpStatus::OK;

	SetBelongsInUserfile();

	return m_opensearch_xml_url.Set(xml_url);
}

void SearchElement::SetBelongsInUserfile()
{
	if (!m_belongs_in_userfile)
	{
		m_belongs_in_userfile = TRUE;
		m_is_changed_this_session = TRUE;
	}
}

void SearchElement::SetIsDeleted(BOOL is_deleted)
{
	if (m_is_deleted==is_deleted)
		return;

	SetBelongsInUserfile();

	m_is_deleted = is_deleted;
}
#endif // USERDEFINED_SEARCHES

OP_STATUS SearchElement::MakeSearchString(OpString& url, OpString& post_query, const OpStringC& keyword)
{
	OP_STATUS ret;

	//Generate search_string
	OpString search_string;
	if (keyword.HasContent() &&
	    (ret=EscapeSearchString(search_string, keyword))!= OpStatus::OK)
	{
			return ret;
	}

	//Generate hits per page count
	OpString hits_per_page;
	int hits = g_pccore->GetIntegerPref(PrefsCollectionCore::PreferredNumberOfHits);
	if (hits == 0)
		hits = 10;

	if ((ret=hits_per_page.AppendFormat(UNI_L("%i"), hits))!=OpStatus::OK)
		return ret;

	//Clear post query if N/A
	if (!m_is_post)
	{
		post_query.Empty();
	}

	//Copy url and post query into the returned variables
	if ((ret=url.Set(m_url))!=OpStatus::OK ||
	    (m_is_post && (ret=post_query.Set(m_post_query))!=OpStatus::OK))
	{
		return ret;
	}

	//Substitute %s with search string and %i with hits per page for url (make loops if all, not only first, %s/%i is to be replaced)
	int substitute_pos = url.Find(UNI_L("%s"));
	if (substitute_pos != KNotFound)
	{
		url.Delete(substitute_pos, 2); //Remove %s
		if (search_string.HasContent() && (ret=url.Insert(substitute_pos, search_string))!=OpStatus::OK)
			return ret;
	}
	
	substitute_pos = url.Find(UNI_L("%i"));
	if (substitute_pos != KNotFound)
	{
		url.Delete(substitute_pos, 2); //Remove %s
		if (hits_per_page.HasContent() && (ret=url.Insert(substitute_pos, hits_per_page))!=OpStatus::OK)
			return ret;
	}

	if (m_is_post)
	{
		//Substitute %s with search string and %i with hits per page for post query (make loops if all, not only first, %s/%i is to be replaced)
		substitute_pos = post_query.Find(UNI_L("%s"));
		if (substitute_pos != KNotFound)
		{
			post_query.Delete(substitute_pos, 2); //Remove %s
			if (search_string.HasContent() && (ret=post_query.Insert(substitute_pos, search_string))!=OpStatus::OK)
				return ret;
		}
		
		substitute_pos = post_query.Find(UNI_L("%i"));
		if (substitute_pos != KNotFound)
		{
			post_query.Delete(substitute_pos, 2); //Remove %s
			if (hits_per_page.HasContent() && (ret=post_query.Insert(substitute_pos, hits_per_page))!=OpStatus::OK)
				return ret;
		}
	}

	return OpStatus::OK;
}

OP_STATUS SearchElement::MakeSearchURL(URL& url, const OpStringC& keyword)
{
	OP_STATUS ret;
	OpString search_url_string;
	OpString search_post_query_string;
	if ((ret=MakeSearchString(search_url_string, search_post_query_string, keyword)) != OpStatus::OK)
		return ret;

	OpStringC empty;
	url = g_url_api->GetURL(search_url_string, empty, m_is_post);

	if(m_is_post)
	{
		url.SetHTTP_Method(HTTP_METHOD_POST);

		OpString8 post_query8;
		if ((ret=post_query8.Set(search_post_query_string.CStr())) != OpStatus::OK)
			return ret;

		url.SetHTTP_Data(post_query8.CStr(), TRUE);
		url.SetHTTP_ContentType(FormUrlEncodedString);
	}

	return OpStatus::OK;
}

unsigned short SearchElement::CharsetToId(const OpStringC& charset)
{
	if (charset.IsEmpty())
		return 0;

	OpString8 charset8;
	if (charset8.Set(charset.CStr()) != OpStatus::OK)
		return 0;

	const char* canonical_charset_name = g_charsetManager->GetCanonicalCharsetName(charset8.CStr());
	OP_MEMORY_VAR unsigned short id = 0;
	TRAPD(ret, id = g_charsetManager->GetCharsetIDL(canonical_charset_name));
	if (ret != OpStatus::OK)
		return 0;

	return id;
}

OP_STATUS SearchElement::EscapeSearchString(OpString& escaped, const OpStringC& search) const
{
	escaped.Set(UNI_L(""), 0); //Clear (without deallocating)

	OP_MEMORY_VAR OP_STATUS ret;
	if (search.IsEmpty()) //Nothing to do
		return OpStatus::OK;

	OpString special_chars_encoded;
	BOOL has_special_characters = search.FindFirstOf(OpStringC16(UNI_L("&;%+#"))) != KNotFound;
	if (has_special_characters)
	{
		const uni_char* src = search.CStr();
		int dest_len = uni_strlen(src)*130/100+10;
		if (!special_chars_encoded.Reserve(dest_len+1))
			return OpStatus::ERR_NO_MEMORY;

		//Escape special characters before converting to search encoding
		uni_char* dest = special_chars_encoded.CStr();
		int src_offset = 0;
		int dest_offset = 0;
		uni_char src_char;
		while ((src_char=*(src+src_offset++)) != 0)
		{
			if ((dest_offset+2) > dest_len)
			{
				dest_len = dest_len*130/100+10;
				*(dest+dest_offset) = 0;
				if (!special_chars_encoded.Reserve(dest_len+1))
					return OpStatus::ERR_NO_MEMORY;
				dest = special_chars_encoded.CStr();
			}
	
			if ((Unicode::IsCntrl(src_char) || Unicode::IsSpace(src_char)) &&
						(dest_offset>0 && *(dest+dest_offset)!='+')) //Ignore space/ctrl at start of search, and compact all others to one character
			{
				*(dest+dest_offset++) = '+';
			}
			else
			{
				dest_offset += UriEscape::EscapeIfNeeded(dest+dest_offset, src_char, UriEscape::SearchUnsafe);
			}
		}
		while (dest_offset>0 && *(dest+dest_offset-1)=='+') dest_offset--; //Strip space/ctrl at end of search
		*(dest+dest_offset) = 0; //Zero-terminate
	}

	//Convert to search encoding
	OutputConverter *converter;
	if ((ret=OutputConverter::CreateCharConverter(g_charsetManager && m_charset_id ? g_charsetManager->GetCharsetFromID(m_charset_id) : "iso-8859-1", &converter, TRUE)) != OpStatus::OK)
		return ret;
	if (!converter)
		return OpStatus::ERR_NO_MEMORY;

	OpString8 down_converted8;
	TRAP(ret, SetToEncodingL(&down_converted8, converter, has_special_characters ? special_chars_encoded.CStr() : search.CStr()));
	OP_DELETE(converter);
	if(ret != OpStatus::OK)
		return ret;

	//Convert back to 16-bit, escaping all non-ASCII characters
	const char* src = down_converted8.CStr();
	int dest_len = UriEscape::GetEscapedLength(src, UriEscape::Range_80_ff);
	if (!escaped.Reserve(dest_len+1))
		return OpStatus::ERR_NO_MEMORY;
	UriEscape::Escape(escaped.CStr(), src, UriEscape::Range_80_ff);

	return OpStatus::OK;
}

void SearchElement::Clear()
{
	m_section_name.Empty();
	m_id = 0;
	m_link_guid.Empty();
	m_name_number = Str::NOT_A_STRING;
	m_name_str.Empty();
	m_description_number = Str::NOT_A_STRING;
	m_description_str.Empty();
	m_url.Empty();
	m_key.Empty();
	m_is_post = FALSE;
	m_post_query.Empty();
	if (m_charset_id)
		g_charsetManager->DecrementCharsetIDReference(m_charset_id);
	m_charset_id = 0;
	m_type = SEARCH_TYPE_UNDEFINED;
	m_icon_url.Empty();
	m_opensearch_xml_url.Empty();

	m_version = 0;
#ifdef USERDEFINED_SEARCHES
	m_belongs_in_userfile = FALSE;
	m_is_changed_this_session = FALSE;
#endif // USERDEFINED_SEARCHES
	m_is_deleted = FALSE;
	m_is_separator = FALSE;
}

#ifdef USERDEFINED_SEARCHES
OP_STATUS SearchElement::WriteL(PrefsFile* prefs_file)
{
	if (!m_belongs_in_userfile)
		return OpStatus::OK; //Don't try to write to read-only file

	if (!prefs_file)
		return OpStatus::ERR_NULL_POINTER;

	OP_ASSERT(m_section_name.HasContent());
	if (m_section_name.IsEmpty())
		return OpStatus::ERR;

	OpString charset_str;
	charset_str.SetL(g_charsetManager ? g_charsetManager->GetCharsetFromID(m_charset_id) : "utf-8");

	prefs_file->WriteIntL(m_section_name, UNI_L("ID"), m_id);
	prefs_file->WriteIntL(m_section_name, UNI_L("Deleted"), m_is_deleted);
	prefs_file->WriteIntL(m_section_name, UNI_L("Version"), m_version);
	if (m_is_separator)
	{
		prefs_file->WriteIntL(m_section_name, UNI_L("Is Separator"), TRUE);
		SetIsChangedThisSession(FALSE);
		return OpStatus::OK;
	}

	prefs_file->WriteStringL(m_section_name, UNI_L("GUID"), m_link_guid.CStr());
	prefs_file->WriteIntL(m_section_name, UNI_L("Name ID"), m_name_number);
	prefs_file->WriteStringL(m_section_name, UNI_L("Name"), m_name_str.CStr());
	prefs_file->WriteIntL(m_section_name, UNI_L("Description ID"), m_description_number);
	prefs_file->WriteStringL(m_section_name, UNI_L("Description"), m_description_str.CStr());
	prefs_file->WriteStringL(m_section_name, UNI_L("URL"), m_url.CStr());
	prefs_file->WriteStringL(m_section_name, UNI_L("Key"), m_key.CStr());
	prefs_file->WriteIntL(m_section_name, UNI_L("Is Post"), m_is_post);
	prefs_file->WriteStringL(m_section_name, UNI_L("Post Query"), m_post_query.CStr());
	prefs_file->WriteStringL(m_section_name, UNI_L("Encoding"), charset_str.CStr());
	prefs_file->WriteIntL(m_section_name, UNI_L("Type"), m_type);
	prefs_file->WriteStringL(m_section_name, UNI_L("Icon URL"), m_icon_url.CStr());
	prefs_file->WriteStringL(m_section_name, UNI_L("OpenSearch URL"), m_opensearch_xml_url.CStr());

	SetIsChangedThisSession(FALSE);
	return OpStatus::OK;
}
#endif // USERDEFINED_SEARCHES

#ifdef SEARCH_PROVIDER_QUERY_SUPPORT
int SearchElement::SearchProviderInfoImpl::GetNumberOfSearchHits()
{
	return g_pccore->GetIntegerPref(PrefsCollectionCore::PreferredNumberOfHits);
}
#endif //SEARCH_PROVIDER_QUERY_SUPPORT

/******************************************************************************************************/

SearchManager::SearchManager()
:
#ifdef OPENSEARCH_SUPPORT
  MessageObject(),
#endif // OPENSEARCH_SUPPORT
  m_next_available_id(FIRST_USERDEFINED_SEARCH_ID),
  m_version(0),
  m_fixed_file(NULL)
#ifdef USERDEFINED_SEARCHES
 ,m_user_file(NULL),
  m_ordering_changed(FALSE),
  m_defaults_changed(FALSE),
  m_userfile_changed_this_session(FALSE)
#endif // USERDEFINED_SEARCHES
#ifdef SEARCH_PROVIDER_QUERY_SUPPORT
  , m_spl_impl(this)
#endif //SEARCH_PROVIDER_QUERY_SUPPORT
{
	int i;
	for (i=0; i<DEFAULT_SEARCH_PRESET_COUNT; i++)
		m_default_search_id[i] = 0;

#ifdef OPENSEARCH_SUPPORT
	g_main_message_handler->SetCallBack(this, MSG_SEARCHMANAGER_DELETE_OPENSEARCH, 0);
#endif // OPENSEARCH_SUPPORT

#ifdef SEARCH_PROVIDER_QUERY_SUPPORT
	if (g_windowCommanderManager)
		g_windowCommanderManager->SetSearchProviderListener(&m_spl_impl);
#endif //SEARCH_PROVIDER_QUERY_SUPPORT

}

SearchManager::~SearchManager()
{
#ifdef USERDEFINED_SEARCHES
#ifdef SELFTEST
	OpString8 dummy;
	TRAPD(err, WriteL(dummy));
#else
	TRAPD(err, WriteL());
#endif // SELFTEST
	OP_DELETE(m_user_file);
#endif // USERDEFINED_SEARCHES
	OP_DELETE(m_fixed_file);

	m_search_engines.DeleteAll();

#ifdef OPENSEARCH_SUPPORT
	g_main_message_handler->UnsetCallBacks(this);
	m_opensearch_parsers.DeleteAll();
#endif // OPENSEARCH_SUPPORT

#ifdef SEARCH_PROVIDER_QUERY_SUPPORT
	if (g_windowCommanderManager && g_windowCommanderManager->GetSearchProviderListener() == &m_spl_impl)
		g_windowCommanderManager->SetSearchProviderListener(NULL);
#endif //SEARCH_PROVIDER_QUERY_SUPPORT
}

#ifdef USERDEFINED_SEARCHES
OP_STATUS SearchManager::LoadL(OpFileFolder fixed_folder, OpFileFolder user_folder)
#else
OP_STATUS SearchManager::LoadL(OpFileFolder fixed_folder)
#endif // USERDEFINED_SEARCHES
{
	OP_STATUS ret;
	OpFile* search_ini_file = OP_NEW_L(OpFile, ());
	OpStackAutoPtr<OpFile> search_ini_file_autoptr(search_ini_file);
	if ((ret=search_ini_file->Construct(UNI_L("search.ini"), fixed_folder))!=OpStatus::OK)
		return ret;

#ifdef OPFILE_HAVE_GENERIC_LANGUAGE_FOLDER
	BOOL localized_file_exists = TRUE;
	if (fixed_folder==OPFILE_LANGUAGE_FOLDER &&
	    (OpStatus::IsError(search_ini_file->Exists(localized_file_exists)) || !localized_file_exists))
	{
		//If the file is not in the localized folder, use the normal INI folder instead
		OpFile* normal_search_ini_file = OP_NEW_L(OpFile, ());
		search_ini_file_autoptr.reset(normal_search_ini_file);
		if ((ret=normal_search_ini_file->Construct(UNI_L("search.ini"), OPFILE_GENERIC_LANGUAGE_FOLDER))!=OpStatus::OK)
			return ret;
	}
#endif // OPFILE_HAVE_GENERIC_LANGUAGE_FOLDER

#ifdef USERDEFINED_SEARCHES
	OpFile* user_search_ini_file = OP_NEW_L(OpFile, ());
	OpStackAutoPtr<OpFile> user_search_ini_file_autoptr(user_search_ini_file);
	if ((ret=user_search_ini_file->Construct(UNI_L("search.ini"), user_folder))!=OpStatus::OK)
		return ret;
#endif // USERDEFINED_SEARCHES

#ifdef USERDEFINED_SEARCHES
	return LoadL(search_ini_file_autoptr.release(), user_search_ini_file_autoptr.release());
#else
	return LoadL(search_ini_file_autoptr.release());
#endif // USERDEFINED_SEARCHES
}

#ifdef USERDEFINED_SEARCHES
OP_STATUS SearchManager::LoadL(OpFileDescriptor* fixed_file, OpFileDescriptor* user_file)
#else
OP_STATUS SearchManager::LoadL(OpFileDescriptor* fixed_file)
#endif // USERDEFINED_SEARCHES
{
	m_fixed_file = fixed_file;
#ifdef USERDEFINED_SEARCHES
	m_user_file = user_file;
#endif // USERDEFINED_SEARCHES

	m_search_engines.DeleteAll(); //Just to be sure..

	if (!fixed_file
#ifdef USERDEFINED_SEARCHES
		|| !user_file
#endif // USERDEFINED_SEARCHES
	   )
	{
		return OpStatus::ERR_NULL_POINTER;
	}

	PrefsFile search_ini_prefs(PREFS_INI);
	ANCHOR(PrefsFile, search_ini_prefs);
	search_ini_prefs.ConstructL();
	search_ini_prefs.SetFileL(m_fixed_file);
	search_ini_prefs.LoadAllL();

#ifdef USERDEFINED_SEARCHES
	PrefsFile user_search_ini_prefs(PREFS_INI);
	ANCHOR(PrefsFile, user_search_ini_prefs);
	user_search_ini_prefs.ConstructL();
	user_search_ini_prefs.SetFileL(m_user_file);
	user_search_ini_prefs.LoadAllL();

	BOOL ignore_userfile = FALSE;
	int user_version = user_search_ini_prefs.ReadIntL("Version", "File Version", 0);
	if (user_version < 10)
	{
		user_search_ini_prefs.Flush(); //Ignore old data
		ignore_userfile = TRUE;
	}

	m_ordering_changed = !ignore_userfile && user_search_ini_prefs.IsSection("Ordering");
	m_defaults_changed = !ignore_userfile && user_search_ini_prefs.IsSection("Defaults");

	BOOL is_special_file = search_ini_prefs.ReadBoolL("Version", "Branded");
	BOOL use_userfile = FALSE;
	BOOL copy_to_userfile = FALSE;
#endif // USERDEFINED_SEARCHES

	m_version = search_ini_prefs.ReadIntL("Version", "File Version", 10); //10 was the first version with Core support for Search Manager

	PrefsSection* ordering_section =
#ifdef USERDEFINED_SEARCHES
		m_ordering_changed ? user_search_ini_prefs.ReadSectionL(UNI_L("Ordering")) :
#endif // USERDEFINED_SEARCHES
		search_ini_prefs.ReadSectionL(UNI_L("Ordering"));

	OpStackAutoPtr<PrefsSection> ordering_section_autoptr(ordering_section);

	OP_STATUS ret;
	if (ordering_section)
	{
		const uni_char* search_engine_section_name;
		const PrefsEntry* search_engine = ordering_section->Entries();
		while (search_engine)
		{
			if ((search_engine_section_name=search_engine->Key()) != NULL)
			{
#ifdef USERDEFINED_SEARCHES
				copy_to_userfile = FALSE;
				use_userfile = !ignore_userfile && user_search_ini_prefs.IsSection(search_engine_section_name);
				if (use_userfile && is_special_file)
				{
					int search_version = search_ini_prefs.ReadIntL(search_engine_section_name, UNI_L("Version"), 0);
					int user_search_version = user_search_ini_prefs.ReadIntL(search_engine_section_name, UNI_L("Version"), 0);
					if (search_version > user_search_version)
					{
						use_userfile = FALSE;
						copy_to_userfile = TRUE;
					}
				}
#endif // USERDEFINED_SEARCHES

				if (
#ifdef USERDEFINED_SEARCHES
					use_userfile || 
#endif // USERDEFINED_SEARCHES
					search_ini_prefs.IsSection(search_engine_section_name))
				{
					SearchElement* new_element = OP_NEW_L(SearchElement, ());
					OpStackAutoPtr<SearchElement> new_element_autoptr(new_element);
#ifdef USERDEFINED_SEARCHES
					if ((ret=new_element->ConstructL(use_userfile ? &user_search_ini_prefs : &search_ini_prefs, search_engine_section_name, use_userfile)) != OpStatus::OK)
#else
					if ((ret=new_element->ConstructL(&search_ini_prefs, search_engine_section_name)) != OpStatus::OK)
#endif // USERDEFINED_SEARCHES
						return ret;

#ifdef USERDEFINED_SEARCHES
					if (copy_to_userfile)
						new_element->SetBelongsInUserfile();
#endif // USERDEFINED_SEARCHES

					if (m_search_engines.Add(new_element) == OpStatus::OK)
					{
						new_element_autoptr.release();

						if (new_element->GetId() >= m_next_available_id)
							m_next_available_id = new_element->GetId()+1;
					}
				}
			}
			search_engine = search_engine->Suc();
		}
	}

	//Read Default searches
	int i;
	for (i=0; i<DEFAULT_SEARCH_PRESET_COUNT; i++)
		m_default_search_id[i] = 0;

	PrefsSection* defaults_section = 
#ifdef USERDEFINED_SEARCHES
		m_defaults_changed ? user_search_ini_prefs.ReadSectionL(UNI_L("Defaults")) :
#endif // USERDEFINED_SEARCHES
		search_ini_prefs.ReadSectionL(UNI_L("Defaults"));

	OpStackAutoPtr<PrefsSection> defaults_section_autoptr(defaults_section);

	if (defaults_section)
	{
		const uni_char* defaults_value;
		SearchElement* defaults_search;
		OpString defaults_key;
		ANCHOR(OpString, defaults_key);

		for (i=0; i<DEFAULT_SEARCH_PRESET_COUNT; i++)
		{
			defaults_key.SetL(UNI_L(""));
			if ((ret=defaults_key.AppendFormat(UNI_L("Type %i"), i+1)) != OpStatus::OK)
				return ret;

			if ((defaults_value = defaults_section->Get(defaults_key.CStr())) != NULL)
			{
				defaults_search = GetSearchBySectionName(defaults_value, TRUE);
				if (defaults_search)
					m_default_search_id[i] = defaults_search->GetId();
			}
		}
	}

	return OpStatus::OK;
}

#ifdef OPENSEARCH_SUPPORT
OP_STATUS SearchManager::AddOpenSearch(const OpStringC& opensearch_xml_url_string)
{
	OpStringC empty;
	URL opensearch_xml_url = g_url_api->GetURL(opensearch_xml_url_string, empty, FALSE);
	return AddOpenSearch(opensearch_xml_url);
}

OP_STATUS SearchManager::AddOpenSearch(URL& opensearch_xml_url)
{
	OP_STATUS ret;
	OpString opensearch_xml_url_string;
	if ((ret=opensearch_xml_url.GetAttribute(URL::KUniName_With_Fragment, opensearch_xml_url_string)) != OpStatus::OK)
		return ret;

	SearchElement* existing_search = GetSearchByOpenSearchXMLURL(opensearch_xml_url_string, TRUE);
	if (existing_search != NULL)
	{
		if (existing_search->IsDeleted())
			existing_search->SetIsDeleted(FALSE);

		return OpStatus::OK;
	}

	OpenSearchParser* opensearch_parser = OP_NEW(OpenSearchParser, ());
	if (!opensearch_parser)
		return OpStatus::ERR_NO_MEMORY;

	if ((ret=m_opensearch_parsers.Add(opensearch_parser)) != OpStatus::OK)
	{
		OP_DELETE(opensearch_parser);
		return ret;
	}

	return opensearch_parser->Construct(opensearch_xml_url, this);
}

SearchElement* SearchManager::GetSearchByOpenSearchXMLURL(const OpStringC& xml_url, BOOL include_deleted)
{
	SearchElement* search;
	UINT32 count = m_search_engines.GetCount();
	UINT32 i;
	for (i=0; i<count; i++)
	{
		if ((search=m_search_engines.Get(i)) != NULL)
		{
			if (search->m_opensearch_xml_url.Compare(xml_url)==0 && (include_deleted || !search->m_is_deleted))
				return search;
		}
	}
	return NULL;
}
#endif // OPENSEARCH_SUPPORT

#ifdef USERDEFINED_SEARCHES
OP_STATUS SearchManager::AssignIdAndAddSearch(SearchElement* search)
{
	if (!search)
		return OpStatus::ERR_NULL_POINTER;

	OP_ASSERT(search->GetId() == 0);
	search->m_id = m_next_available_id++;
	search->m_section_name.Empty();
	search->m_section_name.AppendFormat(search->IsSeparator() ? UNI_L("---------- %i") : UNI_L("Userdefined %i"), search->GetId()-FIRST_USERDEFINED_SEARCH_ID+1);

	OP_STATUS ret;
	if ((ret=m_search_engines.Add(search)) != OpStatus::OK)
		return ret;

	m_ordering_changed = TRUE; //To write out new Ordering section, for the new search to be picked up
	m_userfile_changed_this_session = TRUE;

	return OpStatus::OK;
}

OP_STATUS SearchManager::WriteL(
#ifdef SELFTEST
                                OpString8& written_file_content
#endif // SELFTEST
                               )
{
	if (!m_user_file)
		return OpStatus::ERR_FILE_NOT_FOUND;

	//Do we actually need to write a new file? Only if something is changed this session!
	BOOL needs_to_save = m_userfile_changed_this_session;
	SearchElement* search;
	UINT32 search_count = m_search_engines.GetCount();
	UINT32 i;
	for (i=0; i<search_count && !needs_to_save; i++)
	{
		if ((search=m_search_engines.Get(i))!= NULL && search->IsChangedThisSession())
			needs_to_save = TRUE;
	}
	if (!needs_to_save)
		return OpStatus::OK;

	//Create prefsfile
	OP_STATUS ret;
	PrefsFile user_search_ini_prefs(PREFS_INI);
	ANCHOR(PrefsFile, user_search_ini_prefs);
	user_search_ini_prefs.ConstructL();
	user_search_ini_prefs.SetFileL(m_user_file);
	user_search_ini_prefs.DeleteAllSectionsL();

	//Write version info
	user_search_ini_prefs.WriteIntL("Version", "File Version", m_version);

	//Write ordering
	if (m_ordering_changed)
	{
		for (i=0; i<search_count; i++)
		{
			if ((search=m_search_engines.Get(i)) != NULL)
			{
				user_search_ini_prefs.WriteStringL(UNI_L("Ordering"), search->m_section_name, UNI_L(""));
			}
		}
	}

	//Write defaults
	if (m_defaults_changed)
	{
		OpString8 defaults_key;
		for (i=0; i<DEFAULT_SEARCH_PRESET_COUNT; i++)
		{
			if ((search=GetDefaultSearch(i, TRUE)) != NULL)
			{
				defaults_key.Empty();
				if ((ret=defaults_key.AppendFormat("Type %i", i+1)) != OpStatus::OK)
					return ret;

				user_search_ini_prefs.WriteStringL("Defaults", defaults_key, search->m_section_name);
			}
		}
	}

	//Write searches
	for (i=0; i<search_count; i++)
	{
		if ((search=m_search_engines.Get(i))!=NULL && search->BelongsInUserfile())
		{
			if ((ret=search->WriteL(&user_search_ini_prefs)) != OpStatus::OK)
				return ret;
		}
	}

	//Dump to disk!
	user_search_ini_prefs.CommitL(TRUE);

	//Everything is saved now. Reset state variables
	for (i=0; i<search_count; i++)
	{
		if ((search=m_search_engines.Get(i))!=NULL && search->IsChangedThisSession())
			search->SetIsChangedThisSession(FALSE);
	}
	m_userfile_changed_this_session = FALSE;

#ifdef SELFTEST
	written_file_content.SetL("");
	OpFileDescriptor* prefs_file_descriptor = user_search_ini_prefs.GetFile();
	if (prefs_file_descriptor)
	{
		OpFileLength file_length;
		if ((ret=prefs_file_descriptor->GetFileLength(file_length)) != OpStatus::OK)
			return ret;

		if (!written_file_content.ReserveL(file_length+1))
			return OpStatus::ERR_NO_MEMORY;

		OpFileLength bytes_read = 0;
		if ((ret=prefs_file_descriptor->Open(OPFILE_READ)) != OpStatus::OK ||
		    (ret=prefs_file_descriptor->Read(written_file_content.CStr(), file_length, &bytes_read)) != OpStatus::OK)
		{
			return ret;
		}

		*(written_file_content.CStr() + bytes_read) = 0; //Zero terminate
	}
#endif // SELFTEST

	return OpStatus::OK;
}

OP_STATUS SearchManager::MarkSearchDeleted(int id)
{
	SearchElement* search = GetSearchById(id);
	if (!search)
		return OpStatus::OK; //Nothing to do. Return ERR?

	search->SetIsDeleted(TRUE);
	return OpStatus::OK;
}

OP_STATUS SearchManager::MoveBefore(int id, int move_before_this_id)
{
	if (id == move_before_this_id)
		return OpStatus::ERR;

	SearchElement* search = GetSearchById(id);
	SearchElement* before_this_search = GetSearchById(move_before_this_id);
	if (!search || !before_this_search)
		return OpStatus::ERR_OUT_OF_RANGE;

	INT32 search_index = m_search_engines.Find(search);
	INT32 before_this_search_index = m_search_engines.Find(before_this_search);
	if (search_index==-1 || before_this_search_index==-1)
		return OpStatus::ERR_OUT_OF_RANGE;

	m_search_engines.Remove(search_index);
	if (search_index < before_this_search_index)
		before_this_search_index--;

	OP_STATUS ret;
	if ((ret=m_search_engines.Insert(before_this_search_index, search)) != OpStatus::OK) //If this fails, we're in trouble
		return ret;

	m_ordering_changed = TRUE;
	m_userfile_changed_this_session = TRUE;

	return OpStatus::OK;
}

OP_STATUS SearchManager::MoveAfter(int id, int move_after_this_id)
{
	if (id == move_after_this_id)
		return OpStatus::ERR;

	SearchElement* search = GetSearchById(id);
	SearchElement* after_this_search = GetSearchById(move_after_this_id);
	if (!search || !after_this_search)
		return OpStatus::ERR_OUT_OF_RANGE;

	INT32 search_index = m_search_engines.Find(search);
	INT32 after_this_search_index = m_search_engines.Find(after_this_search);
	if (search_index==-1 || after_this_search_index==-1)
		return OpStatus::ERR_OUT_OF_RANGE;

	m_search_engines.Remove(search_index);
	if (search_index < after_this_search_index)
		after_this_search_index--;

	OP_STATUS ret;
	if ((ret=m_search_engines.Insert(after_this_search_index+1, search)) != OpStatus::OK) //If this fails, we're in trouble
		return ret;

	m_ordering_changed = TRUE;
	m_userfile_changed_this_session = TRUE;

	return OpStatus::OK;
}
#endif // USERDEFINED_SEARCHES

SearchElement* SearchManager::GetSearchById(int id, BOOL include_deleted)
{
	SearchElement* search;
	UINT32 count = m_search_engines.GetCount();
	UINT32 i;
	for (i=0; i<count; i++)
	{
		if ((search=m_search_engines.Get(i)) != NULL)
		{
			if (search->m_id==id && (include_deleted || !search->m_is_deleted))
				return search;
		}
	}
	return NULL;
}

SearchElement* SearchManager::GetSearchByURL(const OpStringC& url, BOOL include_deleted)
{
	SearchElement* search;
	UINT32 count = m_search_engines.GetCount();
	UINT32 i;
	for (i=0; i<count; i++)
	{
		if ((search=m_search_engines.Get(i)) != NULL)
		{
			if (search->m_url.Compare(url)==0 && (include_deleted || !search->m_is_deleted))
				return search;
		}
	}
	return NULL;
}

SearchElement* SearchManager::GetSearchByKey(const OpStringC& key, BOOL include_deleted)
{
	SearchElement* search;
	UINT32 count = m_search_engines.GetCount();
	UINT32 i;
	for (i=0; i<count; i++)
	{
		if ((search=m_search_engines.Get(i)) != NULL)
		{
			if (search->m_key.Compare(key)==0 && (include_deleted || !search->m_is_deleted))
				return search;
		}
	}
	return NULL;
}

OP_STATUS SearchManager::GetSearchesByType(SearchType type, OpINT32Vector& result, BOOL include_deleted)
{
	result.Clear();

	OP_STATUS ret;
	SearchElement* search;
	UINT32 count = m_search_engines.GetCount();
	UINT32 i;
	for (i=0; i<count; i++)
	{
		if ((search=m_search_engines.Get(i)) != NULL)
		{
			if ((type==SEARCH_TYPE_UNDEFINED || search->m_type==type) && (include_deleted || !search->m_is_deleted))
			{
				if ((ret=result.Add(search->m_id)) != OpStatus::OK)
					return ret;
			}
		}
	}
	return OpStatus::OK;
}

#ifdef USERDEFINED_SEARCHES
void SearchManager::SetDefaultSearchId(unsigned int preset_index, int id)
{
	OP_ASSERT(preset_index<DEFAULT_SEARCH_PRESET_COUNT);
	if (preset_index>=DEFAULT_SEARCH_PRESET_COUNT || m_default_search_id[preset_index]==id)
		return;

	m_default_search_id[preset_index] = id;
	m_defaults_changed = TRUE;
	m_userfile_changed_this_session = TRUE;
}
#endif // USERDEFINED_SEARCHES

int SearchManager::GetSearchEnginesCount(BOOL include_deleted, BOOL include_separators) const
{
	int search_engine_count = 0;
	SearchElement* search;
	UINT32 tmp_count = m_search_engines.GetCount();
	UINT32 i;
	for (i=0; i<tmp_count; i++)
	{
		if ((search=m_search_engines.Get(i))!=NULL && (include_deleted || !search->m_is_deleted) && (include_separators || !search->m_is_separator))
		{
			search_engine_count++;
		}
	}
	return search_engine_count;
}

OP_STATUS SearchManager::GetSearchURL(OpString *key, OpString *search_query, OpString **url) /**OBSOLETE */
{
	if (!key || !search_query || !url)
		return OpStatus::ERR_NULL_POINTER;

	*url = OP_NEW(OpString, ());
	if (!*url)
		return OpStatus::ERR_NO_MEMORY;

	SearchElement* search_element = GetSearchByKey(*key);
	if (!search_element)
		return OpStatus::OK; //No search => empty url

	OpString dummy_postquery;	
	return search_element->MakeSearchString(**url, dummy_postquery, *search_query);
}

SearchElement* SearchManager::GetSearchBySectionName(const OpStringC& section_name, BOOL include_deleted)
{
	SearchElement* search;
	UINT32 count = m_search_engines.GetCount();
	UINT32 i;
	for (i=0; i<count; i++)
	{
		if ((search=m_search_engines.Get(i)) != NULL)
		{
			if (search->m_section_name.Compare(section_name)==0 && (include_deleted || !search->m_is_deleted))
				return search;
		}
	}
	return NULL;
}

#ifdef OPENSEARCH_SUPPORT
void SearchManager::HandleCallback(OpMessage msg, MH_PARAM_1, MH_PARAM_2)
{
	if (msg == MSG_SEARCHMANAGER_DELETE_OPENSEARCH)
	{
		OpenSearchParser* parser_object;
		UINT32 index = m_opensearch_parsers.GetCount();
		while (index > 0)
		{
			if ((parser_object=m_opensearch_parsers.Get(--index))!=NULL && parser_object->IsFinished())
			{
				m_opensearch_parsers.Delete(parser_object);
			}
		}
	}
}
#endif // OPENSEARCH_SUPPORT

#endif //SEARCH_ENGINES
