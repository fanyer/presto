/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef DESKTOP_UTIL_SEARCH_ENGINES

#include "adjunct/desktop_util/search/search_net.h"

#include "modules/url/url_tools.h"
#include "modules/display/vis_dev.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/logdoc/logdoc_util.h"
#include "modules/formats/uri_escape.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/prefsfile/prefsfile.h"
#include "modules/forms/form.h"
#include "modules/util/str.h"
#include "modules/util/opstring.h"

#include "adjunct/desktop_util/search/searchenginemanager.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/managers/FavIconManager.h"

#define GOOGLE_SEARCH_ID "7A8CADE6677811DDBA4B5E9D55D89593"

/**
 * "&json=t" is a temporary workaround to receive pure JSON format response from google.
 * so far they have adapted that only for client=opera, we need it for two other clients also.
 */
#define GoogleAdressbarSourceId UNI_L("opera-suggest-omnibox")
#define GoogleSearchbarSourceId UNI_L("opera-suggest-search")

// --------------------------------------------------------------------------------
//
// class SearchTemplate
//
// --------------------------------------------------------------------------------

/***********************************************************************************
 ** SearchTemplate Constructor
 **
 **
 ***********************************************************************************/
SearchTemplate::SearchTemplate(BOOL filtered) :
		OpFilteredVectorItem<SearchTemplate>(filtered),
		m_type(SEARCH_TYPE_GOOGLE),
		m_format_id(Str::NOT_A_STRING),
		m_seperator_after(FALSE),
		m_is_post(FALSE),
		m_use_tld(FALSE),
		m_pbar_pos(-1),
		m_ui_id(GetUniqueID() + SEARCH_TYPE_MAX), //  Note: this should be unique globally really, not like now
		m_name(Str::NOT_A_STRING),
		m_no_id(FALSE),
		m_search_used(FALSE),
		m_store(CUSTOM)
#ifdef SEARCH_PROVIDER_QUERY_SUPPORT
		,m_spii(this)
#endif
{
}

#ifdef SEARCH_PROVIDER_QUERY_SUPPORT
OpSearchProviderListener::SearchProviderInfo* SearchTemplate::GetSearchProviderInfo()
{
	return &m_spii;
}
#endif //SEARCH_PROVIDER_QUERY_SUPPORT

/***********************************************************************************
 ** GetEngineName
 **
 **
 ***********************************************************************************/
OP_STATUS SearchTemplate::GetEngineName(OpString& name, BOOL strip_amp)
{
	if(m_stringname.IsEmpty())
	{
		RETURN_IF_ERROR(g_languageManager->GetString(Str::LocaleString(m_name), name)); // FIXME: MAP
	}
	else
	{
		RETURN_IF_ERROR(name.Set(m_stringname)); // FIXME: MAP
	}

	if (strip_amp)
	{
		RemoveChars(name, UNI_L("&"));
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** GetItemData
 **
 **
 ***********************************************************************************/
OP_STATUS SearchTemplate::GetItemData(ItemData* item_data)
{
	if (item_data->query_type == COLUMN_QUERY)
	{
		switch(item_data->column_query_data.column)
		{
			case 0:
			{
				if (item_data->flags & FLAG_NO_PAINT)
					break;

				item_data->column_bitmap = GetIcon();

				if (item_data->column_bitmap.IsEmpty())
				{
					item_data->column_query_data.column_image = "Search Web";
				}
				break;
			}
			case 1:
			{
				RETURN_IF_ERROR(GetEngineName(*item_data->column_query_data.column_text));
				break;
			}
			case 2:
			{
				RETURN_IF_ERROR(item_data->column_query_data.column_text->Set(m_key));
				break;
			}
		}
	}
	return OpStatus::OK;
}

/*************************************************************************
 *
 * GetTypeString
 *
 *
 *
 ************************************************************************/
#ifdef SUPPORT_SYNC_SEARCHES
const uni_char* SearchTemplate::GetTypeString()
{
	switch(m_type)
	{
	case SEARCH_TYPE_GOOGLE   : return UNI_L("normal");
	case SEARCH_TYPE_INPAGE   : return UNI_L("find_in_page");
	case SEARCH_TYPE_HISTORY  : return UNI_L("history_search");
	}

	return UNI_L("normal");
}
#endif

OP_STATUS SearchTemplate::GetUrl(OpString& url) const
{
	RETURN_IF_ERROR(url.Set(m_url));
	if (m_default_tracking_code.HasContent())
	{
		RETURN_IF_ERROR(url.ReplaceAll(UNI_L("{TrackingCode}"), m_default_tracking_code.CStr()));
	}
	return OpStatus::OK;
}

/***********************************************************************************
 ** GetIcon
 **
 **
 ***********************************************************************************/
Image SearchTemplate::GetIcon()
{
	return g_favicon_manager->Get(GetUrl().CStr());
}


/***********************************************************************************
 ** GetEngineName
 **
 **
 ***********************************************************************************/
Str::LocaleString SearchTemplate::GetFormatId() const
{
	return Str::LocaleString(m_format_id);
}


/***********************************************************************************
 *
 * ReadL
 *
 * @param currsection -
 * @param searchengines - PrefsFile to read from
 * @param store - where is it read from (default is search.ini from PACKAGE)
 *
 * @return TRUE if Deleted is set on this searchTemplate in search.ini
 *
 ***********************************************************************************/
BOOL SearchTemplate::ReadL(const OpStringC8& currsection, PrefsFile* searchengines, SearchStore store)
{
	m_store = store;

	INT32 temp_id = searchengines->ReadIntL(currsection, "ID", -1);

	OpStringC empty_str(UNI_L(""));
	searchengines->ReadStringL(currsection, "UNIQUEID", m_guid, empty_str);

	// If the ID is -1 this means that the file loaded is
	// the old format which doesn't have IDs so we need to set a special
	// flag to make the merging work properly
	if (temp_id == -1 && !m_guid.HasContent())
	{
		m_no_id = TRUE;
		// Will get a generated guid below
	}

	// check if this is a default search, and if so, get the guid from the mapping
	if (!m_guid.HasContent() && !m_no_id)
	{
		m_guid.Set(g_searchEngineManager->GetGUIDByOldId(temp_id));
	}
	
	if (!m_guid.HasContent())// generate the GUID
	{
		OpString guid;
		if (OpStatus::IsSuccess(StringUtils::GenerateClientID(guid)))
		{
			SetUniqueGUID(guid.CStr());
			OP_ASSERT(GetUniqueGUID().HasContent());
		}
	}

	searchengines->ReadStringL(currsection, "Name", m_stringname, empty_str);

	m_format_id = Str::LocaleString(searchengines->ReadIntL(currsection, "Verbtext", Str::SI_IDSTR_SEARCH_FORMAT_WITH));

	searchengines->ReadStringL(currsection, "URL", m_url, empty_str);

	searchengines->ReadStringL(currsection, "Default Tracking Code", m_default_tracking_code, empty_str);

	searchengines->ReadStringL(currsection, "SpeedDial Tracking Code", m_speeddial_tracking_code, empty_str);

	searchengines->ReadStringL(currsection, "ICON", m_icon_url, empty_str);

	searchengines->ReadStringL(currsection, "Query", m_query, empty_str);

	searchengines->ReadStringL(currsection, "Key", m_key, empty_str);

	m_is_post = searchengines->ReadIntL(currsection, "Is post", 0) ? TRUE : FALSE;

	m_use_tld = searchengines->ReadIntL(currsection, "UseTLD", 0) ? TRUE : FALSE;

	m_seperator_after = searchengines->ReadIntL(currsection, "Has endseparator", 0);

	searchengines->ReadStringL(currsection, "Encoding", m_encoding, UNI_L("iso-8859-1"));

	m_type = (SearchType)searchengines->ReadIntL(currsection, "Search Type", SEARCH_TYPE_GOOGLE);

	m_pbar_pos = searchengines->ReadIntL(currsection, "Position", -1);

	m_name = Str::LocaleString(searchengines->ReadIntL(currsection, "Nameid", Str::NOT_A_STRING));

	// This string is used to makeover the final URL string using keys 'URL', and 'Default Tracking Code' and 'SpeedDial Tracking Code'
	m_tmp_url_with_tracking_code.Set(m_url); 

	// ** Suggest Protocol - The protocol used for search suggestions, if available. See below.
	// ** Suggest URL		 - The url used for search suggestions. See below.
	OpString tmp;

	m_suggest_protocol = SearchTemplate::SUGGEST_NONE;

	searchengines->ReadStringL(currsection, "Suggest Protocol", tmp, empty_str);
	if(!tmp.CompareI("JSON"))
	{
		m_suggest_protocol = SearchTemplate::SUGGEST_JSON;
	}
	if(m_suggest_protocol != SearchTemplate::SUGGEST_NONE)
	{
		searchengines->ReadStringL(currsection, "Suggest URL", m_suggest_url, empty_str);
		if(!m_suggest_url.HasContent())
		{
			m_suggest_protocol = SearchTemplate::SUGGEST_NONE;
		}
	}
	// Return if this is a deleted search or not
	return searchengines->ReadIntL(currsection, "Deleted", 0) ? TRUE : FALSE;
}


/***********************************************************************************
 ** WriteL
 **
 **
 ***********************************************************************************/
void SearchTemplate::WriteL(const OpStringC8& currsection, PrefsFile* searchengines, BOOL& result)
{
	// Only ever write out user search.ini information (i.e not from the package)
	// (or from package, but changed by user)
	if (IsFromPackage())
	{
		result = FALSE;
		return;
	}

	// If the search has been deleted and it was not from the package search.ini, don't write it to custom search.ini
	if (!IsFromPackageOrModified() && IsFiltered())
	{
		result = FALSE;
		return;
	}

	result = TRUE;

	//searchengines->WriteIntL(currsection, "ID", m_id);

	searchengines->WriteStringL(currsection, "UNIQUEID", m_guid);

	searchengines->WriteStringL(currsection, "Name", m_stringname);

	searchengines->WriteIntL(currsection, "Verbtext", m_format_id);

	searchengines->WriteStringL(currsection, "URL", m_url);

	// Write tracking codes only if URL still contains '{TrackingCode}' template.
	// Refer to the SearchTemplate::SetURL() which replaces SearchTemplate::m_url with an updated URL.
	if (m_url.FindI(UNI_L("{TrackingCode}")) != KNotFound)
	{
		searchengines->WriteStringL(currsection, "Default Tracking Code", m_default_tracking_code);
		searchengines->WriteStringL(currsection, "SpeedDial Tracking Code", m_speeddial_tracking_code);
	}
	else
	{
		// searchengines should be cleared before this function is called (SearchEngineManager does that)
		OP_ASSERT(!searchengines->IsKey(currsection, "Default Tracking Code"));
		OP_ASSERT(!searchengines->IsKey(currsection, "SpeedDial Tracking Code"));
	}
	
	// ** Suggest Protocol - The protocol used for search suggestions, if available. 
	// ** Suggest URL	   - The url used for search suggestions. 
	if(m_suggest_protocol != SearchTemplate::SUGGEST_NONE && m_suggest_url.HasContent())
	{
		const uni_char *suggest_type = NULL;

		switch(m_suggest_protocol)
		{
			case SearchTemplate::SUGGEST_JSON:
				suggest_type = UNI_L("JSON");
				break;
		}
		searchengines->WriteStringL(currsection, "Suggest Protocol", suggest_type);
		searchengines->WriteStringL(currsection, "Suggest URL", m_suggest_url);
	}
	// We do not write an ICON entry

	searchengines->WriteStringL(currsection, "Query", m_query);
	searchengines->WriteStringL(currsection, "Key", m_key);

	searchengines->WriteIntL(currsection, "Is post", m_is_post ? 1 : 0);
	searchengines->WriteIntL(currsection, "UseTLD", m_use_tld ? 1 : 0);
	searchengines->WriteIntL(currsection, "Has endseparator", m_seperator_after);

	searchengines->WriteStringL(currsection, "Encoding", m_encoding);

	searchengines->WriteIntL(currsection, "Search Type", m_type);
	searchengines->WriteIntL(currsection, "Position", m_pbar_pos);
	searchengines->WriteIntL(currsection, "Nameid", m_name);
	searchengines->WriteIntL(currsection, "Deleted", IsFiltered() ? 1 : 0);

}


/***********************************************************************************
 ** IsMatch
 **
 **
 ***********************************************************************************/
BOOL SearchTemplate::IsMatch(const SearchTemplate* search, BOOL ui_match /* default FALSE */) const
{
	// If this search is not marked a UI match then we ONLY test based on url
	if (!ui_match)
	{
		// If this is an old Non-ID search the test on url
		// otherwise use the ID
		if (m_no_id)
		{
			if (m_url.Compare(search->m_url.CStr()))
				return FALSE;
		}
		else
		{
			if (m_guid.Compare(search->GetUniqueGUID()) != 0)
				return FALSE;
		}
	}
	else
	{
		// Test everything that can be changed in the UI
		if (m_name != search->m_name)
			return FALSE;
		else if (m_is_post != search->m_is_post)
			return FALSE;
		else if (m_pbar_pos != search->m_pbar_pos)
			return FALSE;
		else if (m_stringname.Compare(search->m_stringname.CStr()))
			return FALSE;
		else if (m_key.Compare(search->m_key.CStr()))
			return FALSE;
		else if (m_url.Compare(search->m_url.CStr()))
			return FALSE;
		else if (m_query.Compare(search->m_query.CStr()))
			return FALSE;
	}

	return TRUE;
}


/***********************************************************************************
 *
 * Copy
 *
 * @param SearchTemplate search - Fields from this item is copied to this SearchTemplate
 * @param change_store - If TRUE, also copy param search's m_store field.
 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY
 *
 * Copies fields from SearchTemplate* search to SearchTemplate 'this'
 *
 ***********************************************************************************/
OP_STATUS SearchTemplate::Copy(const SearchTemplate* search, BOOL change_store)
{
	// Copy all the attribute over
	RETURN_IF_ERROR(m_stringname.Set(search->m_stringname.CStr()));
	m_format_id = search->m_format_id;
	RETURN_IF_ERROR(m_url.Set(search->m_url.CStr()));
	RETURN_IF_ERROR(m_query.Set(search->m_query.CStr()));
	RETURN_IF_ERROR(m_key.Set(search->m_key.CStr()));
	m_is_post = search->m_is_post;
	m_use_tld = search->m_use_tld;
	m_seperator_after = search->m_seperator_after;
	RETURN_IF_ERROR(m_encoding.Set(search->m_encoding.CStr()));
	m_type = search->m_type;
	m_pbar_pos = search->m_pbar_pos;
	m_name = search->m_name;
	m_suggest_protocol = search->m_suggest_protocol;
	RETURN_IF_ERROR(m_suggest_url.Set(search->m_suggest_url));
	if (m_url.FindI(UNI_L("{TrackingCode}")) != KNotFound)
	{
		RETURN_IF_ERROR(m_default_tracking_code.Set(search->m_default_tracking_code));
		RETURN_IF_ERROR(m_speeddial_tracking_code.Set(search->m_speeddial_tracking_code));
	}
	else
	{
		m_default_tracking_code.Empty();
		m_speeddial_tracking_code.Empty();
	}

	// Only change the store if asked
	if (change_store)
	{
		m_store = search->m_store;
	}

	return OpStatus::OK;
}


/***********************************************************************************
 *
 * CreatePackageCopy
 *
 * If this item was read from package search.ini then create exact
 * copy with different GUID and store set to PACKAGE_COPY.
 *
 * @param new_search - gets copy of this item
 * @return OpStatus::OK, or OpStatus::ERR_NO_MEMORY, or OpStatus::ERR
 *
 ***********************************************************************************/
 OP_STATUS SearchTemplate::CreatePackageCopy(SearchTemplate*& new_search) const
{
	if (!IsFromPackage())
	{
		return OpStatus::ERR;
	}
	OpAutoPtr<SearchTemplate> copy_ptr(OP_NEW(SearchTemplate, ()));
	RETURN_OOM_IF_NULL(copy_ptr.get());
	copy_ptr->m_store = PACKAGE_COPY;
	RETURN_IF_ERROR(copy_ptr->Copy(this, FALSE));
	RETURN_IF_ERROR(copy_ptr->m_icon_url.Set(m_icon_url));
	OP_ASSERT(copy_ptr->m_guid.IsEmpty());
	OpString guid;
	RETURN_IF_ERROR(StringUtils::GenerateClientID(guid));
	RETURN_IF_ERROR(copy_ptr->SetUniqueGUID(guid));
	new_search = copy_ptr.release();
	return OpStatus::OK;
}


OP_STATUS SearchTemplate::MakeSuggestQuery(const OpStringC& search_term, OpString& search_url, SearchSource source) const
{
	if(m_suggest_protocol == SUGGEST_NONE || !m_suggest_url.HasContent())
		return OpStatus::ERR_NOT_SUPPORTED;

	RETURN_IF_ERROR(search_url.Set(m_suggest_url));
	uni_char search_term_urlified[MAX_URL_LEN+1];
	UrlifySearchString(search_term_urlified, search_term.CStr());
	RETURN_IF_ERROR(search_url.ReplaceAll(UNI_L("{SearchTerm}"), search_term_urlified));

	if (GetUniqueGUID() == GOOGLE_SEARCH_ID)
	{
		switch (source)
		{
			case ADDRESS_BAR:
			{
				RETURN_IF_ERROR(search_url.ReplaceAll(UNI_L("{Client}"), GoogleAdressbarSourceId));
				break;
			}
			case SEARCH_BAR:
			case SEARCH_404:
			{
				RETURN_IF_ERROR(search_url.ReplaceAll(UNI_L("{Client}"), GoogleSearchbarSourceId));
				break;
			}
			default:
				OP_ASSERT(!"unknown");
		}
	}	

	return OpStatus::OK;
}

/***********************************************************************************
 ** UrlifySearchString
 **
 ** Used in MakeSearchURL function below
 ***********************************************************************************/
void SearchTemplate::UrlifySearchString(uni_char* outstring,
											 const uni_char* instring)
{
	uni_char* outstring_walk = outstring;

	for (int i = 0; (instring[i] != '\0') && (outstring_walk-outstring+2<MAX_URL_LEN) ; i++)
	{
		// 2 as in two potentional extra chars if encoding
		// *Always* escape & and ; and %  and + and #
		if ((instring[i] == '&') || (instring[i] == ';') || (instring[i] == '%') || (instring[i] == '+') || (instring[i]=='#') )
		{
			// (latin1-encoding and utf8 is same for & ; &)
			*outstring_walk++ = '%';
			*outstring_walk++ = UriEscape::EscapeFirst((char) instring[i]);
			*outstring_walk++ = UriEscape::EscapeLast((char) instring[i]);
		}
		// this ctrlandspacecompacting-part could be done before dosearch:
	    else if((instring[i] <=' ') // also ctrl-chars converted to space. I think this is ok
			|| instring[i]==160 /*NBSP*/ || (instring[i]>=8192 /*EMSPACE*/ && instring[i]<=8202 /*HAIRSPACE*/)
			|| instring[i]==8239 /*NARROWNBSP*/ || instring[i]==12288 /*IDEOGRSP*/) { // all the other spaces too is converted to space, except ogham
			*outstring_walk = '+';
			if((outstring!=outstring_walk) && *(outstring_walk-1) != '+') // this compacts multiple spaces to one
				outstring_walk++; //also strip starting spaces
		}
		else {
			*outstring_walk++ = instring[i];
		}
	}

	while((outstring!=outstring_walk) && *(outstring_walk-1) == '+') outstring_walk--; //also strip ending spaces

	*outstring_walk = '\0';
}


/***********************************************************************************
 ** SprintEscape
 **
 ** Used in MakeSearchURL function below
 ***********************************************************************************/
void SearchTemplate::SprintEscape(uni_char *searchurl)
{
	uni_char *tmp = searchurl;

	while(tmp && *(tmp))
	{
		tmp = uni_strstr(tmp, UNI_L("%"));
		if(tmp)
		{
			if (*(tmp+1))
			{
				// we need massive escaping here, not just for 's' and 'i'. This is a potentional
				// DDoS entry point.
				if (*(tmp+1) != 's' && *(tmp+1) != 'i')
				{
					*tmp = 0xFDD0; // U+FDD0 is reserved for process internal use
				}
			}
			tmp++;
		}
	}
}


/***********************************************************************************
 ** SprintUnEscape
 **
 ** Used in MakeSearchURL function below
 ***********************************************************************************/
void SearchTemplate::SprintUnEscape(uni_char *searchurl)
{
	uni_char *tmp = searchurl;

	while(tmp && *(tmp))
	{
		tmp = uni_strchr(tmp, 0xFDD0);
		if(tmp)
		{
			*(tmp) = '%';
		}
	}
}


/***********************************************************************************
 ** MakeSearchURL
 **
 **
 ***********************************************************************************/
BOOL SearchTemplate::MakeSearchURL(URL& url_object, 
								   const uni_char* keyword, 
								   BOOL resolve_keyword, 
								   SearchEngineManager::SearchReqIssuer search_req_issuer,
								   const OpStringC& key, 
								   URL_CONTEXT_ID context_id)
{
	uni_char keyword2[MAX_URL_LEN+1]; // searchword-part
	uni_char url[MAX_URL_LEN+200+1]; // final url (+200 for the string in search_engines)

	if (resolve_keyword)
	{
		// Change a "www.opera.com" to "http://www.opera.com"
		OpString keyword_before;
		OpString keyword_after;
		keyword_before.Set(keyword);
		ResolveUrlNameL(keyword_before, keyword_after);

		UrlifySearchString(keyword2, keyword_after.CStr()); // basic urlifying
	}
	else
	{
		UrlifySearchString(keyword2, keyword); // basic urlifying
	}

	unsigned int current_length = uni_strlen(keyword2);
	uni_char* searchencoding = m_encoding.CStr();

	EncodeFormsData(searchencoding ? make_singlebyte_in_tempbuffer(searchencoding, uni_strlen(searchencoding)) : "iso-8859-1",
									keyword2 /*uni_char variable*/, 0, current_length /*length now and later*/, MAX_URL_LEN /*max I can take*/);

	keyword2[current_length] = 0; // null-terminate yourself

	// MAX_URL_LEN = maximum number of characters to store

	if (!m_is_post && m_url.Find(UNI_L("%s")) == KNotFound) //if the string doesn't contain a %s we don't have anything to format the string with, prevents crashing
		return FALSE;

	uni_char* urlstring = m_url.CStr();
	uni_char* poststring = m_query.CStr();
	uni_char* teststring = urlstring;

	if(m_is_post)		// if the search uses post, check the query
	{
		teststring = poststring;
	}

	if(!teststring || !*teststring)
	{
		return FALSE;
	}

	int stringholders = 0;
	int intholders = 0;
	uni_char* tmp = teststring;
	uni_char* last_stringholder = NULL;

	while(tmp && *tmp)
	{
		tmp = uni_strstr(tmp, UNI_L("%s"));
		if (tmp)
		{
			last_stringholder = tmp;
			stringholders++;
			tmp++;
		}
	}

	tmp = urlstring;
	while(tmp && *tmp)
	{
		tmp = uni_strstr(tmp, UNI_L("%i"));
		if(tmp)
		{
			// Bug#96348: Make sure the intholder is always last
			if (tmp < last_stringholder)
				return FALSE;

			intholders++;
			tmp++;
		}
	}

	// escape away % followed by number before calling uni_snprintf

	SprintEscape(urlstring);

	if(stringholders!=1)
	{
		return FALSE;
	}

	// We need a new url so we don't screw the original which "urlstring" is pointing at
	OpString urlstring_copy;
	urlstring_copy.Set(urlstring);

	if (!urlstring_copy.HasContent())
		return FALSE;

	//Substitue the string '{TrackingCode}' in the URL with the string from the key 'SpeedDial Tracking Code' 
	// or 'Default Tracking Code' depending on the search request issuer types.
	if (KNotFound != urlstring_copy.FindI(OpStringC16(UNI_L("{TrackingCode}"))))
	{
		if (search_req_issuer == SearchEngineManager::SEARCH_REQ_ISSUER_SPEEDDIAL)
		{
			urlstring_copy.ReplaceAll(UNI_L("{TrackingCode}"), m_speeddial_tracking_code.CStr());
		}
		else
		{
			urlstring_copy.ReplaceAll(UNI_L("{TrackingCode}"), m_default_tracking_code.CStr());
		}
	}

	// Check if there was a google TLD
	if (m_use_tld)
	{
		OpStringC google_tld = g_pcui->GetStringPref(PrefsCollectionUI::GoogleTLDDefault);

		// Do a check to make sure the string looks ok has a .google. and isn't too long
		if (google_tld.Find(".google.") == KNotFound || google_tld.Length() > 30)
			return FALSE;

		// Change the www.google.com bit to the TLD version in the preference
		uni_char *second_slash = uni_strchr(urlstring_copy.CStr(), '/');
		if (!second_slash)
			return FALSE;

		second_slash = uni_strchr(second_slash + 1, '/');
		if (!second_slash)
			return FALSE;

		uni_char *third_slash = uni_strchr(second_slash + 1, '/');
		if (!third_slash)
			return FALSE;

		// Build the new string
		OpString new_url;

		*second_slash = '\0';
		new_url.Set(urlstring_copy.CStr());
		new_url.AppendFormat(UNI_L("/www%s%s"), google_tld.CStr(), third_slash);

		urlstring_copy.Set(new_url.CStr());
	}

#ifdef GOOGLE_G_SEARCH_NOT_PAID
	// Special google hack so that if the search is started with "g search" from the address bar we change the
	// sourceid so that we don't get paid. Currently not active
	if (!GetUniqueGUID().Compare(GOGGLE_SEARCH_GUID) && key.HasContent() && !key.Compare(GetKey().CStr()))
	{
		// Change the "sourceid=opera" to "source=opera_g"
		uni_char *p = uni_strstr(urlstring_copy.CStr(), GOOGLE_SEARCH_PAY_SOURCEID);
		if (p)
		{
			// Terminate the string at the point "sourceid=opera" starts
			*p = '\0';

			OpString new_url;

			new_url.AppendFormat(UNI_L("%s%s%s"), urlstring_copy.CStr(), GOOGLE_SEARCH_NO_PAY_SOURCEID, (p + uni_strlen(GOOGLE_SEARCH_PAY_SOURCEID)));

			urlstring_copy.Set(new_url.CStr());
		}
	}
#endif // GOOGLE_G_SEARCH_NOT_PAID

	uni_snprintf(url, MAX_URL_LEN+200, urlstring_copy.CStr() , keyword2, 0);

	url[MAX_URL_LEN + 200] = '\0';

	// replace $ with % again
	SprintUnEscape(url);

	URL dummy;
	OpString8 real_url;

	OpStatus::Ignore(real_url.SetUTF8FromUTF16(url));

	url_object = g_url_api->GetURL(dummy, real_url, m_is_post, context_id);

	if(m_is_post)
	{
		// escape away % followed by number before calling uni_snprintf
		SprintEscape(m_query.CStr());

		uni_snprintf(url, MAX_URL_LEN+200, m_query.CStr() , keyword2, 0);

		// replace $ with % again
		SprintUnEscape(url);

		url_object.SetHTTP_Method(HTTP_METHOD_POST);

		char* char_url = make_singlebyte_in_tempbuffer(url, uni_strlen(url));

		url_object.SetHTTP_Data(char_url, TRUE);
		url_object.SetHTTP_ContentType(FormUrlEncodedString);

	}
	url[MAX_URL_LEN + 200] = '\0';

	return TRUE;
}

#endif // DESKTOP_UTIL_SEARCH_ENGINES
