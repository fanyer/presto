/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#if !defined SEARCH_NET_H && defined DESKTOP_UTIL_SEARCH_ENGINES
#define SEARCH_NET_H

#include "adjunct/desktop_util/treemodel/optreemodel.h"
#include "adjunct/desktop_util/treemodel/optreemodelitem.h"
#include "modules/util/adt/OpFilteredVector.h"
#include "modules/util/simset.h"
#include "modules/url/url2.h"

#include "adjunct/desktop_util/search/search_types.h"
#include "adjunct/desktop_util/search/searchenginemanager.h"

// Used to ID the google search for the hack so we don't get paid when "g search" is used from the address bar
#define GOGGLE_SEARCH_GUID				UNI_L("7A8CADE6677811DDBA4B5E9D55D89593")
#define GOOGLE_SEARCH_PAY_SOURCEID		UNI_L("sourceid=opera")
#define GOOGLE_SEARCH_NO_PAY_SOURCEID	UNI_L("sourceid=opera_g")

#ifdef SEARCH_PROVIDER_QUERY_SUPPORT
#include "modules/windowcommander/OpWindowCommanderManager.h"
#endif //SEARCH_PROVIDER_QUERY_SUPPORT

class PrefsFile;
class UserSearches;
class SearchEngineManager;

/***********************************************************************************
 ** SearchTemplate
 ** --------------
 **
 **
 **
 **
 ***********************************************************************************/

class SearchTemplate : public TreeModelItem<SearchTemplate, UserSearches>, 
					   public OpFilteredVectorItem<SearchTemplate>
{

	friend class SearchEngineManager;

public:

	enum SearchStore
	{
		PACKAGE,           // from the search.ini included in the package (ie. not the user prefs search.ini) 
		PACKAGE_MODIFIED,  // from package search.ini, but modified by user prefs search.ini
		PACKAGE_COPY,      // copy of a PACKAGE search with different GUID, can be used as a fallback for tampered search
		CUSTOM             // from user prefs search.ini
	};

	enum SearchSource
	{
		ADDRESS_BAR,//searchning from address bar
		SEARCH_BAR, //searchning from search bar
		SEARCH_404	//searchning from document desktop window on 404 error
	};

//  -------------------------
//  Initialization and destruction :
//  -------------------------

	/**
	 * Constructor
	 *
	 * @param filtered - whether the item should be initially hidden
	 */
    SearchTemplate(BOOL filtered = FALSE);


	/**
	 * Destructor
	 */
	virtual ~SearchTemplate() {}


	/**
	 * Reads an entry and sets up the item accordingly
	 *
	 * @param section       -
	 * @param searchengines -
	 * @param from_package  -
	 *
	 * @return
	 */
	BOOL ReadL(const OpStringC8& section,
			   PrefsFile* searchengines,
			   SearchStore store = PACKAGE);


	/**
	 * Writes an entry based on its state
	 *
	 * @param section       -
	 * @param searchengines -
	 * @param result        - TRUE if the section was written
	 *
	 * @return TRUE if search was written
	 */
	void WriteL(const OpStringC8& section,
				PrefsFile* searchengines,
				BOOL& result);


//  -------------------------
//  Interface :
//  -------------------------


	/**
	 * Makes the actual search url based on the user input
	 *
	 * @param url_object      - the url object to be used
	 * @param keyword         - the user input
	 * @param resolve_keyword - whether the address should be resolved
	 * @param search_req_issuer - identifies search issuer
	 * @param key			  - key that was used to init the search
	 *
	 * @return
	 */
	BOOL MakeSearchURL(URL& url_object,
					   const uni_char* keyword,
					   BOOL resolve_keyword,
					   SearchEngineManager::SearchReqIssuer search_req_issuer = SearchEngineManager::SEARCH_REQ_ISSUER_ADDRESSBAR,
					   const OpStringC& key = UNI_L(""),
					   URL_CONTEXT_ID context_id = 0);


	/**
	 * Test whether this item is a match for the item search
	 *
	 * @param search   - the item to compare to
	 * @param ui_match - whether only the url should be tested
	 *
	 * @return TRUE if the items match and FALSE otherwise
	 */
	BOOL IsMatch(const SearchTemplate* search,
				 BOOL ui_match = FALSE) const;


	/**
	 * Get the name of the search engine
	 *
	 * @param name      - string that will be set to the name
	 * @param strip_amp - whether ampersands should be removed
	 */
	OP_STATUS GetEngineName(OpString& name,
					   BOOL strip_amp = TRUE);


	/**
	 * Make this item be a copy of the input item
	 *
	 * @param search       - the item to be copied
	 * @param change_store - whether the m_store field should be copied
	 *
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY
	 */
	OP_STATUS Copy(const SearchTemplate* search,
			  BOOL change_store = TRUE);

	/**
	 * If this item was read from package search.ini then create exact
	 * copy with different GUID and store set to PACKAGE_COPY.
	 *
	 * @param new_search - gets copy of this item
	 * @return OpStatus::OK, or OpStatus::ERR_NO_MEMORY, or OpStatus::ERR
	 */
	OP_STATUS CreatePackageCopy(SearchTemplate*& new_search) const;

	/**
	 * Gets the personal bar position
	 *
	 * @return the personal bar posistion
	 */
	INT32 GetPersonalbarPos() const {return m_pbar_pos;}
	BOOL GetOnPersonalbar() const { return m_pbar_pos >= 0;}


	/**
	 * Gets the url of the icon for this search
	 *
	 * @return the icon url if set - if not returns the url
	 */
	const OpStringC& GetIconAddress() const { return m_icon_url.HasContent() ? m_icon_url : m_url; }

	/**
	 * Gets an absolute URL to the icon if set
	 *
	 * @return the icon url if set - if not returns empty string
	 */
	const OpStringC& GetAbsoluteIconAddress() const { return m_icon_url; }

	/**
	 * @return the skin icon for the search engine - will be used when there is no favicon
	 */
	const char* GetSkinIcon() const { return m_type == SEARCH_TYPE_HISTORY ? "History" : "Search Web"; }

	/**
	 * @return the search type for the search engine
	 */
	SearchType GetSearchType() { return m_type; }
#ifdef SUPPORT_SYNC_SEARCHES
	const uni_char* GetTypeString();
#endif

	BOOL IsTranslation() { return m_type >= SEARCH_TYPE_TRANSLATE_FIRST && m_type <= SEARCH_TYPE_TRANSLATE_LAST; }
	BOOL IsDeletable() { return m_type != SEARCH_TYPE_INPAGE && m_type != SEARCH_TYPE_HISTORY; }

	// == OpTreeModelItem ======================

	virtual OP_STATUS	GetItemData(ItemData* item_data);
	virtual INT32		GetID() { return m_ui_id; }
	virtual Type		GetType() { return SEARCHTEMPLATE_TYPE; };


	// ========= Getters and Setters ==================


	BOOL				GetNonID() { return m_no_id; }
	
	void				SetSearchUsed(BOOL search_used) { m_search_used = search_used; }
	BOOL				GetSearchUsed() { return m_search_used; }

	const OpStringC& GetKey() const { return m_key; }
	const OpStringC& GetQuery() const { return m_query; }
	const OpStringC& GetEncoding() const { return m_encoding; }
	const OpStringC& GetUniqueGUID() const { return m_guid; }
	const OpStringC& GetName() const { return m_stringname; }
	BOOL  GetSeparatorAfter() const { return m_seperator_after; }
	BOOL  GetIsPost() const { return m_is_post; }
	BOOL  GetUseTLD() const { return m_use_tld; }
	Str::LocaleString GetFormatId() const;
	OP_STATUS GetUniqueGUID(OpString& guid) const { return guid.Set(m_guid.CStr()); }
	OP_STATUS GetUrl(OpString& url) const;
	const OpStringC& GetUrl() {
		OpStatus::Ignore(GetUrl(m_tmp_url_with_tracking_code));
		return m_tmp_url_with_tracking_code;
	}

	BOOL IsFromPackage() const { return m_store == PACKAGE || m_store == PACKAGE_COPY; }
	BOOL IsFromPackageOrModified() const { return IsFromPackage() || m_store == PACKAGE_MODIFIED; }
	BOOL IsCopyFromPackage() const { return m_store == PACKAGE_COPY; }
	BOOL IsCustomOrModified() const { return m_store == CUSTOM || m_store == PACKAGE_MODIFIED; }
	BOOL IsCustom() const { return m_store == CUSTOM; }
	void SetIsCustomOrModified() {
		if (m_store == PACKAGE_COPY)
			m_store = CUSTOM;
		else if (m_store == PACKAGE)
			m_store = PACKAGE_MODIFIED;
		OP_ASSERT(m_store == CUSTOM || m_store == PACKAGE_MODIFIED);
		m_use_tld = FALSE;
	}

	Image GetIcon();

	// Set functions ---------------------------------------------------------
	// private:

	/**
	 * Set the personal bar position
	 *
	 * @param new_pos - the new postition
	 */
	void SetPersonalbarPos(INT32 new_pos) {m_pbar_pos = new_pos;}
	OP_STATUS SetName(const OpStringC& name) { return m_stringname.Set(name.CStr()); }
	OP_STATUS SetURL(const OpStringC& url) { 
		OP_STATUS status = url.Compare(m_tmp_url_with_tracking_code) ? m_url.Set(url.CStr()) : OpStatus::OK; 
		return status;
	}
	OP_STATUS SetKey(const OpStringC& key) { return m_key.Set(key.CStr()); }
	OP_STATUS SetQuery(const OpStringC& query) { return m_query.Set(query.CStr()); }
	OP_STATUS SetUniqueGUID(const OpStringC& guid) 
	{ 
		OP_ASSERT(guid.HasContent());
		if (m_guid.HasContent()) 
		{
			OP_ASSERT(FALSE);
			return OpStatus::ERR;
		}
		return m_guid.Set(guid.CStr()); 
	}

	OP_STATUS SetEncoding(const OpStringC& encoding) { return m_encoding.Set(encoding);}
	void SetIsPost(BOOL is_post) { m_is_post = is_post; }
	void SetUseTLD(BOOL use_tld) { m_use_tld = use_tld; }
	void SetSeparatorAfter(BOOL separator_after) { m_seperator_after = separator_after; }
	void SetSearchType(SearchType type) { m_type = type; }

#ifdef SEARCH_PROVIDER_QUERY_SUPPORT
	OpSearchProviderListener::SearchProviderInfo* GetSearchProviderInfo();
#endif //SEARCH_PROVIDER_QUERY_SUPPORT
//  -------------------------
//  Public fields :  (Used in modules/widgets/OpEdit.cpp) -> TODO: make private
//  -------------------------
	OpString m_url;
	OpString m_query;

	enum SearchSuggestProtocol
	{
		SUGGEST_NONE,
		SUGGEST_JSON
	};

	BOOL IsSuggestEnabled() const { return m_suggest_protocol != SUGGEST_NONE; }

	SearchSuggestProtocol GetSuggestProtocol() const { return m_suggest_protocol; }

	/**
	 * Based on the input search_term this method will generate a valid search
	 * url to be used for the suggest search. 
	 *
	 * @param search_term The input word to search for
	 * @param search_url The generated url to use for the query
	 * @param source of the search (for now: address bar or search bar)
	 * @return OK on suggest, ERR_NOT_SUPPORTED if no suggest is available, otherwise error code
	 */
	OP_STATUS MakeSuggestQuery(const OpStringC& search_term, OpString& search_url, SearchSource client) const;

private:

//  -------------------------
//  Private functions :
//  -------------------------

	/**
	 *
	 *
	 * @param outstring -  Must be at least MAX_URL_LEN+1 long.  Will be zero-terminated.
	 * @param instring  -
	 */
	static void UrlifySearchString(uni_char* outstring,
								const uni_char* instring);


	/**
	 * URLs containing URL-escaped parts contain '%' signs that sprintf
	 * will interpret as input if we don't escape them first.
	 *
	 * @param searchurl -
	 */
	void SprintEscape(uni_char *searchurl);


	/**
	 * URLs containing URL-escaped parts contain '%' signs that sprintf
	 * will interpret as input if we don't escape them first.
	 *
	 * @param searchurl -
	 */
	void SprintUnEscape(uni_char *searchurl);


//  -------------------------
//  Private fields :
//  -------------------------

	OpString m_key;
    OpString m_stringname;
	OpString m_guid;

	// This need to be present in chartables.bin
	OpString m_encoding;

	// The only special type today is SEARCH_TYPE_INPAGE, so defaulting
	// to SEARCH_TYPE_GOOGLE for the moment
	SearchType m_type;

	int m_format_id;

	BOOL m_seperator_after:1;
	BOOL m_is_post:1;
	BOOL m_use_tld:1;			// Set to use google TLD

	INT32				m_pbar_pos;		///< used when rearranging order
	INT32				m_ui_id;	// ID used to idenify the search in the UI (the one returned by GetID() (OpTypedObject)

	int					m_name;
	OpString			m_icon_url; // Only set for package searches

	BOOL				m_no_id;		// Special flag that identifies a loaded search as one that is of the old
										// format which doesn't have an ID

	BOOL				m_search_used;	// Set once the search has been used for the first time this session

	SearchSuggestProtocol m_suggest_protocol;	// the protocol to be used for suggest
	OpString			m_suggest_url;			// the url to use for suggestions
	OpString			m_default_tracking_code;   // string replaces the string {TrackingCode} in the URL field if it is present. This string identifies searches invoked from search or address fields.
	OpString			m_speeddial_tracking_code; // string replaces the string {TrackingCode} in the URL field if it is present. This string identifies searches invoked from speeddial field.
	OpString			m_tmp_url_with_tracking_code;

protected:

	SearchStore m_store;

#ifdef SEARCH_PROVIDER_QUERY_SUPPORT
	class SearchProviderInfoImpl : public OpSearchProviderListener::SearchProviderInfo
	{
		SearchTemplate* m_st;
		OpString m_name;
		OpString m_skin_icon;

	public:
		SearchProviderInfoImpl(SearchTemplate* st) : m_st(st){ OP_ASSERT(st); }
		//from OpSearchProviderListener::SearchProviderInfo
		virtual const uni_char* GetName()
		{
			if (OpStatus::IsSuccess(m_st->GetEngineName(m_name, TRUE)) && !m_name.IsEmpty())
				return m_name.CStr();
			return NULL;
		}

		virtual const uni_char* GetUrl(){ return m_st->GetUrl().CStr(); }
		virtual const uni_char* GetQuery(){ return m_st->GetQuery().CStr(); }
		virtual const uni_char* GetIconUrl()
		{
			if (!m_st->GetAbsoluteIconAddress().IsEmpty())
				return m_st->GetAbsoluteIconAddress().CStr();
			if (OpStatus::IsSuccess(m_skin_icon.Set(m_st->GetSkinIcon())))
				return m_skin_icon.CStr();
			return NULL;
		}
		virtual const uni_char* GetEncoding(){ return m_st->GetEncoding().CStr(); }
		virtual BOOL IsPost(){ return m_st->GetIsPost(); }
		virtual void Dispose(){}
		virtual int GetNumberOfSearchHits() {return 0; };

		virtual const uni_char* GetGUID() { return m_st->GetUniqueGUID().CStr(); }
		virtual BOOL ProvidesSuggestions() { return m_st->IsSuggestEnabled(); }
	};
	SearchProviderInfoImpl m_spii;
#endif
};

#endif
