/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifdef SEARCH_ENGINES

#ifndef SEARCH_MANAGER_H
#define SEARCH_MANAGER_H

#include "modules/encodings/utility/charsetnames.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/prefsfile/prefsfile.h"
#include "modules/util/adt/opvector.h"
#include "modules/util/opstring.h"
#include "modules/locale/locale-enum.h"

#ifdef OPENSEARCH_SUPPORT
#include "modules/searchmanager/src/searchmanager_opensearch.h"
#endif // OPENSEARCH_SUPPORT

#ifdef SEARCH_PROVIDER_QUERY_SUPPORT
#include "modules/windowcommander/OpWindowCommanderManager.h"
#endif //SEARCH_PROVIDER_QUERY_SUPPORT

#define DEFAULT_SEARCH_PRESET_COUNT 2 ///< The size of the array containing default searches. Right now, 'default search' uses one and speeddial uses one
#define FIRST_USERDEFINED_SEARCH_ID 1000000


#ifdef DESKTOP_UTIL_SEARCH_ENGINES
#include "adjunct/desktop_util/search/search_types.h"
#else
enum SearchType
{
	SEARCH_TYPE_UNDEFINED = 0,
	SEARCH_TYPE_SEARCH = 1,
	SEARCH_TYPE_TRANSLATION = 2,
	SEARCH_TYPE_DICTIONARY = 3,
	SEARCH_TYPE_ENCYCLOPEDIA = 4,
	SEARCH_TYPE_CURRENCY = 5
};
#endif // DESKTOP_UTIL_SEARCH_ENGINES

/**
 * The searchelement class contains functionality for handling each individual search,
 * from reading from file or constructing a new search, to setting and getting search info,
 * and deciding if it needs to be written and actually write itself (if user-defined searches
 * are enabled). You will normally find a given SearchElement by querying the SearchManager,
 * and then make your changes on the SearchElement object itself.
 */ 
class SearchElement {
friend class SearchManager; //We are one big happy family
public:
	SearchElement();
	virtual ~SearchElement();

#ifdef USERDEFINED_SEARCHES
	/** Constructs a separator object. If #IsSeparator returns TRUE, most Get-/Set-functions are meaningless.
      *
      * @return OpStatus::OK
      */
	OP_STATUS ConstructSeparator(); //Construct a dummy search element, to be used as a separator


	/** Constructs a search element object with name and description based on OpStrings.
      *
      * @param name name of search
      * @param url URL to be used when searching. '\%s' will be replaced by search query, '\%i' will be replaced by hits per page (from PrefsCollectionCore::PreferredNumberOfHits). An examle url could be "http://address/search=%s&hits_per_page=%i"
      * @param key search key (or short string) for the search. Typical use is the key "g" for Google, which then lets you type "g foobar" to search for foobar from the address bar
      * @param type search type. Used for grouping searches. Groups can be fetched by calling SearchManager#GetSearchesByType
      * @param description more verbose description than name. '\%s' will be replaced by name when calling #GetDescription
      * @param is_post set to TRUE if search needs to be sent using POST
      * @param post_query search-string to use if search needs to be sent using POST. '\%s'/'\%i' will substitute as described for url parameter
      * @param charset charset to use for search-string. Searches with non-ASCII characters will be converted to given charset before characters with the high bit set (and some others) are escaped
      * @param icon_url favicon-URL
      * @param opensearch_xml_url URL for the XML file if this search element is defined by OpenSearch
      * @return OpStatus::OK, ERR or ERR_NO_MEMORY
      */
	OP_STATUS Construct(const OpStringC& name, const OpStringC& url, const OpStringC& key=UNI_L(""), SearchType type=SEARCH_TYPE_SEARCH,
	                    const OpStringC& description=UNI_L(""), BOOL is_post=FALSE, const OpStringC& post_query=UNI_L(""),
	                    const OpStringC& charset=UNI_L("utf-8"), const OpStringC& icon_url=UNI_L(""), const OpStringC& opensearch_xml_url=UNI_L(""));


	/** Constructs a search element object with name and description based on LocaleString. Except for this, identical to #Construct(const OpStringC&, const OpStringC&, const OpStringC&, SearchType, const OpStringC&, BOOL, const OpStringC&, const OpStringC&, const OpStringC&, const OpStringC&)
      */
	OP_STATUS Construct(Str::LocaleString name, const OpStringC& url, const OpStringC& key=UNI_L(""), SearchType type=SEARCH_TYPE_SEARCH,
	                    Str::LocaleString description=Str::NOT_A_STRING, BOOL is_post=FALSE, const OpStringC& post_query=UNI_L(""),
	                    const OpStringC& charset=UNI_L("utf-8"), const OpStringC& icon_url=UNI_L(""), const OpStringC& opensearch_xml_url=UNI_L(""));


	/** Constructs a search element object from a PrefsFile section in either the fixed search.ini or the user-defined search.ini.
      *
      * @param prefs_file PrefsFile to read search element info from
      * @param prefs_section PrefsFile section to read search element info from
      * @param is_userfile set to FALSE if prefs_file is the fixed search.ini, or to TRUE if it is the user-defined search.ini
      * @return OpStatus::OK or ERR_NULL_POINTER. May Leave
      */
	OP_STATUS ConstructL(PrefsFile* prefs_file, const OpStringC& prefs_section, BOOL is_userfile);
#else
	/** Constructs a search element object from a PrefsFile section.
      *
      * @param prefs_file PrefsFile to read search element info from
      * @param prefs_section PrefsFile section to read search element info from
      * @return OpStatus::OK or ERR_NULL_POINTER. May Leave
      */
	OP_STATUS ConstructL(PrefsFile* prefs_file, const OpStringC& prefs_section);
#endif // USERDEFINED_SEARCHES


	/** Returns the search element unique ID (this number will never change, but it may not be globally unique)
      *
      * @return Search element unique ID
      */
	int GetId() const {return m_id;}


	/** Gets the globally unique ID used by Link (currently not in use)
      *
      * @return Search element globally unique ID set by Link
      */
	OP_STATUS GetLinkGUID(OpString& guid) const {return guid.Set(m_link_guid);}


	/** Gets the name.
      *
      * @param name returns the name
      * @param strip_name_amp set to TRUE if '&'s should be stripped from the name
      * @return OpStatus::OK or ERR_OUT_OF_MEMORY
      */
	OP_STATUS GetName(OpString& name, BOOL strip_amp=TRUE);


	/** Gets the description.
      *
      * @param description returns the description. '\%s' will be replaced by name
      * @param strip_name_amp set to TRUE if '&'s should be stripped from the name. Ignored if description does not contain '\%s'
      * @return OpStatus::OK or ERR_OUT_OF_MEMORY
      */
	OP_STATUS GetDescription(OpString& description, BOOL strip_name_amp=TRUE);


	OP_STATUS GetURL(OpString& url) const {return url.Set(m_url);}
	OP_STATUS GetKey(OpString& key) const {return key.Set(m_key);}
	BOOL IsPost() const {return m_is_post;}
	OP_STATUS GetPostQuery(OpString& post_query) const {return post_query.Set(m_post_query);}

	/** Gets the charset id. These ids are static for the whole Opera session, and is an easier/faster way of comparing charsets
      *
      * @return Charset id
      */
	unsigned short GetCharsetId() const {return m_charset_id;}


	OP_STATUS GetCharset(OpString& charset) const {return g_charsetManager ? charset.Set(g_charsetManager->GetCharsetFromID(m_charset_id)) : OpStatus::ERR_NULL_POINTER;}
	SearchType GetType() const {return m_type;}
	OP_STATUS GetIconURL(OpString& icon_url) const {return icon_url.Set(m_icon_url);}
	OP_STATUS GetOpenSearchXMLURL(OpString& xml_url) const {return xml_url.Set(m_opensearch_xml_url);}
	BOOL IsSeparator() const {return m_is_separator;}
	BOOL IsDeleted() const {return m_is_deleted;}
#ifdef USERDEFINED_SEARCHES
	/** Gets the search element version. This is only used in branded search.ini. Searches with version set, will be copied to the user-defined search.ini,
      * unless it already contains a search with the same id and a higher or equal version.
      *
      * @return version
      */
	unsigned short GetVersion() const {return m_version;}


	/**
      * @return TRUE if this search element is either loaded from the user-defined search.ini, or modified so it will be saved to the user-defined search.ini file
      */
	BOOL BelongsInUserfile() const {return m_belongs_in_userfile;}


	/**
      * @return TRUE if this search element is modified this session (and thus will have to be saved)
      */
	BOOL IsChangedThisSession() const {return m_is_changed_this_session;}


	/** Sets the name based on a LocaleString. Name is usually shown in drop-down lists, context-menues, in search-field background etc.
      * Feel free to use a string containing '&' if you want to use the name in context-menues with hotkey underlining.
      *
      * @param name LocaleString to use for looking up name string
      */
	void SetName(Str::LocaleString name);


	/** Sets the name. Name is usually shown in drop-down lists, context-menues, in search-field background etc.
      * Feel free to use a string containing '&' if you want to use the name in context-menues with hotkey underlining.
      *
      * @param name name to set
      * @return OpStatus::OK or ERR_NO_MEMORY
      */
	OP_STATUS SetName(const OpStringC& name);


	/** Sets the description based on a LocaleString. Description is usually shown in where something more verbose than name is needed
      * (Like when you type "g test", and the autocomplete drop-down shows "Search with Google"). A '\%s' will be replaced by name
      *
      * @param description description to set
      */
	void SetDescription(Str::LocaleString description);


	/** Sets the description. Description is usually shown in where something more verbose than name is needed
      * (Like when you type "g test", and the autocomplete drop-down shows "Search with Google"). A '\%s' will be replaced by name
      *
      * @param description description to set
      * @return OpStatus::OK or ERR_NO_MEMORY
      */
	OP_STATUS SetDescription(const OpStringC& description);


	OP_STATUS SetURL(const OpStringC& url);
	OP_STATUS SetKey(const OpStringC& key);
	void SetIsPost(BOOL is_post);
	OP_STATUS SetPostQuery(const OpStringC& post_query);


	/** Sets the charset id. These ids are static for the whole Opera session, and is an easier/faster way of comparing charsets
      * Unless you have gotten the ID from a call to #GetCharsetId for this or another search element, you probably want to use
      * #SetCharset(const OpStringC&) instead
      *
      * @param id charset id to set
      */
	void SetCharsetId(unsigned short id);


	void SetCharset(const OpStringC& charset);
	void SetType(SearchType type);
	OP_STATUS SetIconURL(const OpStringC& icon_url);
	OP_STATUS SetOpenSearchXMLURL(const OpStringC& xml_url);


	/** Marks that this search element should be stored in the user-defined search.ini file.
      * This will usually be done automatically for you, but if you think you know better, be my guest
      */
	void SetBelongsInUserfile();


	/** Marks that this search element is changed this session, and if the search element belongs in the user-defined search.ini file,
      * this file should be written sooner or later. Marking searches changed will usually be done automatically for you, but if you
      * think you know better, be my guest
      */
	void SetIsChangedThisSession() {m_is_changed_this_session = TRUE;}


	/** Marks this search element as deleted or undeleted. It will not be removed, as quite a few functions have a parameter to also consider deleted
      * searches within their functionality, and for the possibility of undeleting searches marked as deleted.
      *
      * @param is_deleted TRUE if this search element is to be marked as deleted, FALSE to undelete it
      */
	void SetIsDeleted(BOOL is_deleted);
#endif // USERDEFINED_SEARCHES

	/** Gets the search string to use for a given keyword.
      *
      * @param url returns the search url (with parameter keyword embedded, converted to charset and escaped)
      * @param post_query returns the post query (if applicable. Otherwise, an empty string is returned)
      * @param keyword the keyword user wants to search for
      * @return OpStatus::OK, ERR or ERR_OUT_OF_MEMORY
      */
	OP_STATUS MakeSearchString(OpString& url, OpString& post_query, const OpStringC& keyword);
	DEPRECATED(inline OP_STATUS MakeSearchString(OpString& url, OpString& post_query, const OpStringC& keyword, BOOL not_in_use));


	/** Gets the search URL to use for a given keyword. If search element is to use POST, this will be set up for you
      *
      * @param url returns the search url ready to use
      * @param keyword the keyword user wants to search for
      * @return OpStatus::OK, ERR or ERR_OUT_OF_MEMORY
      */
	OP_STATUS MakeSearchURL(URL& url, const OpStringC& keyword);
	DEPRECATED(inline OP_STATUS MakeSearchURL(URL& url, const OpStringC& keyword, BOOL not_in_use));

#ifdef SEARCH_PROVIDER_QUERY_SUPPORT
	/**
	 * Returns a OpSearchProviderListener::SearchProviderInfo object
	 * that represents the data in this object used by OpSearchProviderListener
	 */
	OpSearchProviderListener::SearchProviderInfo* GetSearchProviderInfo();
#endif //SEARCH_PROVIDER_QUERY_SUPPORT

public:
	static unsigned short CharsetToId(const OpStringC& charset);
	OP_STATUS EscapeSearchString(OpString& escaped, const OpStringC& search) const;

private:
	void Clear();
#ifdef USERDEFINED_SEARCHES
	OP_STATUS WriteL(PrefsFile* prefs_file);
	void SetId(int id) {m_id = id;}
	OP_STATUS SetLinkGUID(const OpStringC& guid) {return m_link_guid.Set(guid);}
	void SetVersion(unsigned short version) {m_version = version;}
	void SetBelongsInUserfile(BOOL belongs_in_userfile) {m_belongs_in_userfile = belongs_in_userfile;}
	void SetIsChangedThisSession(BOOL is_changed_this_session) {m_is_changed_this_session = is_changed_this_session;}
#endif // USERDEFINED_SEARCHES

private:
	OpString m_section_name;
	int m_id; //Unique ID (0=not set, 1-999.999=Opera defined, 1.000.000->=User defined)
	OpString m_link_guid; //128-bit GUID which will be used in Link. Currently not used
	Str::LocaleString m_name_number; //(Reference to the language file)
	OpString m_name_str; //Name (may be looked up, if m_name_number != Str::NOT_A_STRING)
	Str::LocaleString m_description_number; //(Reference to the language file)
	OpString m_description_str; //Description (may be looked up, if m_description_number != Str::NOT_A_STRING)
	OpString m_url; //Search URL. %s is a placeholder for search string, %i is a placeholder for hits per page
	OpString m_key;
	BOOL m_is_post;
	OpString m_post_query;
	unsigned short m_charset_id;
	SearchType m_type;
	OpString m_icon_url;
	OpString m_opensearch_xml_url;

	unsigned short m_version;
	BOOL m_is_deleted;
#ifdef USERDEFINED_SEARCHES
	BOOL m_belongs_in_userfile;
	BOOL m_is_changed_this_session;
#endif //USERDEFINED_SEARCHES
	BOOL m_is_separator;
	
#ifdef SEARCH_PROVIDER_QUERY_SUPPORT
	class SearchProviderInfoImpl : public OpSearchProviderListener::SearchProviderInfo
	{
		SearchElement* m_se;
		OpString m_name;
		OpString m_encoding;
	public:
		SearchProviderInfoImpl(SearchElement* se) : m_se(se){ OP_ASSERT(se); }
		//from OpSearchProviderListener::SearchProviderInfo
		virtual const uni_char* GetName()
		{
			if (OpStatus::IsSuccess(m_se->GetName(m_name, TRUE)) && !m_name.IsEmpty())
				return m_name.CStr();
			return NULL;
		}
		virtual const uni_char* GetUrl(){ return m_se->m_url.CStr(); }
		virtual const uni_char* GetQuery(){ return m_se->m_post_query.CStr(); }
		virtual const uni_char* GetIconUrl(){ return m_se->m_icon_url.CStr(); }
		virtual const uni_char* GetEncoding()
		{
			if (OpStatus::IsSuccess(m_se->GetCharset(m_encoding)))
				return m_encoding.CStr();
			return NULL;
		}
		virtual BOOL IsPost(){ return m_se->m_is_post; }
		virtual int GetNumberOfSearchHits();
		virtual void Dispose(){}
		// Can safely return 0 as long as ProvidesSuggestions() returns FALSE
		virtual const uni_char* GetGUID(){ return 0; }
		virtual BOOL ProvidesSuggestions(){ return FALSE; }
	};
	SearchProviderInfoImpl m_spii;
	friend class SearchProviderInfoImpl;
#endif
};

/**
 * The searchmanager class contains functionality for reading the ini-files containing
 * information about searchengines and add and remove searchengines manually. It also
 * contains methods for querying for searchengines by different items, changing their
 * order, and managing the default search engines (which currently is an array of two
 * searches. Index 0 is the 'browser default search engine', Index 1 is the 'speeddial
 * default search engine')
 */ 
class SearchManager
#ifdef OPENSEARCH_SUPPORT
 : private MessageObject
#endif // OPENSEARCH_SUPPORT
 {
public:
	SearchManager();
	virtual ~SearchManager();

#ifdef USERDEFINED_SEARCHES
	/** Loads search.ini from the given folders. Call this to initialize SearchManager
      *
      * @param fixed_folder the folder for the opera provided search.ini file
      * @param user_folder the folder where the user-defined/-modified searches will be read/written
      * @return OpStatus::OK, ERR, ERR_FILE_NOT_FOUND or ERR_OUT_OF_MEMORY. May leave
      */
	OP_STATUS LoadL(OpFileFolder fixed_folder=OPFILE_INI_FOLDER, OpFileFolder user_folder=OPFILE_USERPREFS_FOLDER);

	/** Loads search.ini from the given files. Call this to initialize SearchManager
      *
      * @param fixed_file a filedescriptor pointing to the opera provided search.ini file. SearchManager takes ownership of this descriptor!
      * @param user_folder a filedescriptor pointing to where the user-defined/-modified searches will be read/written. SearchManager takes ownership of this descriptor!
      * @return OpStatus::OK, ERR, ERR_FILE_NOT_FOUND or ERR_OUT_OF_MEMORY. May leave
      */
	OP_STATUS LoadL(OpFileDescriptor* fixed_file, OpFileDescriptor* user_file); //Takes ownership of fixed_file and user_file!
#else
	/** Loads search.ini from the given folder. Call this to initialize SearchManager
      *
      * @param fixed_folder the folder for the opera provided (read-only) search.ini file
      * @return OpStatus::OK, ERR, ERR_FILE_NOT_FOUND or ERR_OUT_OF_MEMORY. May leave
      */
	OP_STATUS LoadL(OpFileFolder fixed_folder=OPFILE_INI_FOLDER);


	/** Loads search.ini from the given file. Call this to initialize SearchManager
      *
      * @param fixed_file a filedescriptor pointing to the opera provided (read-only) search.ini file. SearchManager takes ownership of this descriptor!
      * @return OpStatus::OK, ERR, ERR_FILE_NOT_FOUND or ERR_OUT_OF_MEMORY. May leave
      */
	OP_STATUS LoadL(OpFileDescriptor* fixed_file); //Takes ownership of fixed_file!
#endif // USERDEFINED_SEARCHES

#ifdef OPENSEARCH_SUPPORT
	OP_STATUS AddOpenSearch(const OpStringC& opensearch_xml_url_string);
	OP_STATUS AddOpenSearch(URL& opensearch_xml_url);
	
	BOOL HasOpenSearch(const OpStringC& opensearch_xml_url, BOOL include_deleted=FALSE) {return GetSearchByOpenSearchXMLURL(opensearch_xml_url, include_deleted)!=NULL;}

	SearchElement* GetSearchByOpenSearchXMLURL(const OpStringC& xml_url, BOOL include_deleted=FALSE);
#endif // OPENSEARCH_SUPPORT

#ifdef USERDEFINED_SEARCHES
	/** Assigns id and adds the search element to the list of searches managed by the search manager. The search element must be initialized by
      * SearchElement#ConstructSeparator, SearchElement#Construct or SearchElement#ConstructL before calling this function, and you need to call
      * this function to complete the creation of a search element. Search manager will take ownership of the search element object. If you want
      * the new search written to disk immediately, manually call #WriteL
      *
      * @param search search element to hand over to search manager
      * @return OpStatus::OK, ERR, ERR_NULL_POINTER or ERR_OUT_OF_MEMORY.
      */
	OP_STATUS AssignIdAndAddSearch(SearchElement* search);


	/** If anything is changed this session (since initialization of search manager, or since last call to #WriteL), all user-defined/-modified
      * searches are written to the file given in the call to #LoadL , and a new session is started. If nothing is changed, the function returns
      * without doing anything to the file.
      *
      * @return OpStatus::OK, ERR, ERR_FILE_NOT_FOUND or ERR_OUT_OF_MEMORY. May leave
      */
	OP_STATUS WriteL(
#ifdef SELFTEST
	                 OpString8& written_file_content
#endif // SELFTEST
	                );

	/** Marks the search with given unique id as deleted. It will not be removed, as quite a few functions have a parameter to also consider deleted
      * searches within their functionality.
      *
      * @param id the unique id for the search to be marked as deleted
      * @return OpStatus::OK, ERR, ERR_NULL_POINTER or ERR_OUT_OF_MEMORY.
      */
	OP_STATUS MarkSearchDeleted(int id);


	/** Changes the ordering of searches.
      *
      * @param id the unique id for the search to be moved
      * @param move_before_this_id the unique id for the search the first search should be moved in front of
      * @return OpStatus::OK or ERR
      */
	OP_STATUS MoveBefore(int id, int move_before_this_id);


	/** Changes the ordering of searches.
      *
      * @param id the unique id for the search to be moved
      * @param move_after_this_id the unique id for the search the first search should be moved after
      * @return OpStatus::OK or ERR
      */
	OP_STATUS MoveAfter(int id, int move_after_this_id);
#endif // USERDEFINED_SEARCHES

	/** Finds search element based on unique id. If multiple hits (there shouldn't be!), the one with highest ordering will be returned
      *
      * @param id the unique id for the search
      * @param include_deleted set to TRUE if also searches marked as deleted should be matched and returned
      * @return Matching search element, or NULL
      */
	SearchElement* GetSearchById(int id, BOOL include_deleted=FALSE);


	/** Finds search element based on url. If multiple hits, the one with highest ordering will be returned
      *
      * @param url the url for the search
      * @param include_deleted set to TRUE if also searches marked as deleted should be matched and returned
      * @return Matching search element, or NULL
      */
	SearchElement* GetSearchByURL(const OpStringC& url, BOOL include_deleted=FALSE);


	/** Finds search element based on key. If multiple hits, the one with highest ordering will be returned
      *
      * @param key the key for the search
      * @param include_deleted set to TRUE if also searches marked as deleted should be matched and returned
      * @return Matching search element, or NULL
      */
	SearchElement* GetSearchByKey(const OpStringC& key, BOOL include_deleted=FALSE);


	/** Finds search elements based on type. Result will be a vector of unique ids for matching searches
      *
      * @param type search type for the searches. Use SEARCH_TYPE_UNDEFINED to match all types
      * @param result Vector returning unique ids for all matches
      * @param include_deleted set to TRUE if also searches marked as deleted should be matched and returned
      * @return OpStatus::OK or ERR_NO_MEMORY
      */
	OP_STATUS GetSearchesByType(SearchType type, OpINT32Vector& result, BOOL include_deleted=FALSE); //Returns vector of all matching Search Ids


	/** Finds default search element for given index. Index will, for time being, be either 0 ("browser default search engine")
      * or 1 ("speeddial default search engine")
      *
      * @param preset_index specifies which default search index to use
      * @param include_deleted set to TRUE if searches marked as deleted should be returned
      * @return Default search element for given index, or NULL if not set (or marked as deleted, and include_deleted==FALSE)
      */
	SearchElement* GetDefaultSearch(unsigned int preset_index, BOOL include_deleted=FALSE) {return preset_index<DEFAULT_SEARCH_PRESET_COUNT ? GetSearchById(m_default_search_id[preset_index], include_deleted) : 0;}


	/** Finds default search element unique id for given index. Index will, for time being, be either 0 ("browser default search engine")
      * or 1 ("speeddial default search engine")
      *
      * @param preset_index specifies which default search index to use
      * @return Default search element unique id for given index. This function does NOT check is search element is marked as deleted!
      */
	int GetDefaultSearchId(unsigned int preset_index) const {return preset_index<DEFAULT_SEARCH_PRESET_COUNT ? m_default_search_id[preset_index] : 0;}


#ifdef USERDEFINED_SEARCHES
	/** Sets default search element unique id for given index. Index will, for time being, be either 0 ("browser default search engine")
      * or 1 ("speeddial default search engine")
      *
      * @param preset_index specifies which default search index to use
      * @param id search element unique id to set for given index
      */
	void SetDefaultSearchId(unsigned int preset_index, int id);
#endif // USERDEFINED_SEARCHES


	/** Finds number of search elements in the list managed by search manager
      *
      * @param include_deleted if TRUE, search elements marked as deleted will be included in the count
      * @param include_separators if TRUE, search elements that really are separators (SearchElement#IsSeparator() returns TRUE) will be included in the count
      */
	int GetSearchEnginesCount(BOOL include_deleted=FALSE, BOOL include_separators=FALSE) const;

public: /** Backwards compatibility */
    DEPRECATED(OP_STATUS GetSearchURL(OpString *key, OpString *search_query, OpString **url)); //Use SearchElement::MakeSearchString instead

private:
	SearchElement* GetSearchBySectionName(const OpStringC& section_name, BOOL include_deleted=FALSE);

#ifdef OPENSEARCH_SUPPORT
private:
	// Inherited interfaces from MessageObject:
	virtual void HandleCallback(OpMessage, MH_PARAM_1, MH_PARAM_2);
#endif // OPENSEARCH_SUPPORT

private:
	int m_next_available_id;
	int m_version; // Version of the package search.ini
	int m_default_search_id[DEFAULT_SEARCH_PRESET_COUNT];

	OpFileDescriptor* m_fixed_file;
#ifdef USERDEFINED_SEARCHES
	OpFileDescriptor* m_user_file;

	BOOL m_ordering_changed;
	BOOL m_defaults_changed;
	BOOL m_userfile_changed_this_session;
#endif

#ifdef SEARCH_PROVIDER_QUERY_SUPPORT
	class OpSearchProviderListenerImpl : public OpSearchProviderListener
	{
		SearchManager *m_sm;
	public:
		OpSearchProviderListenerImpl(SearchManager *sm) : m_sm(sm){ OP_ASSERT(sm); }
		virtual SearchProviderInfo* OnRequestSearchProviderInfo(RequestReason reason)
		{
			SearchElement* se = m_sm->GetDefaultSearch(0);
			return se ? se->GetSearchProviderInfo() : NULL;
		}
	};
	OpSearchProviderListenerImpl m_spl_impl;
#endif

	OpAutoVector<SearchElement> m_search_engines;

#ifdef OPENSEARCH_SUPPORT
	OpAutoVector<OpenSearchParser> m_opensearch_parsers;
#endif // OPENSEARCH_SUPPORT
};

// Deprecated functions
inline OP_STATUS SearchElement::MakeSearchString(OpString& url, OpString& post_query, const OpStringC& keyword, BOOL not_in_use)
{ return MakeSearchString(url, post_query, keyword); }

inline OP_STATUS SearchElement::MakeSearchURL(URL& url, const OpStringC& keyword, BOOL not_in_use)
{ return MakeSearchURL(url, keyword); }

#endif // SEARCH_MANAGER_H

#endif //SEARCH_ENGINES
