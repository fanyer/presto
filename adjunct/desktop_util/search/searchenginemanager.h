// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
//
// Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//

#ifndef SEARCHMANAGER_H
#define SEARCHMANAGER_H

#ifdef DESKTOP_UTIL_SEARCH_ENGINES

// Use mapping in translations to map from IDs to GUIDs for searches still using the ID
// Anything without a mapping gets a new generated guid.

#include "modules/util/adt/OpFilteredVector.h"
#include "adjunct/desktop_util/treemodel/optreemodel.h"
#include "adjunct/desktop_util/treemodel/optreemodelitem.h"

#include "adjunct/desktop_util/search/search_types.h"

#ifdef SEARCH_PROVIDER_QUERY_SUPPORT
#include "modules/windowcommander/OpWindowCommanderManager.h"
#endif //SEARCH_PROVIDER_QUERY_SUPPORT

#include "adjunct/quick/data_types/OpenURLSetting.h"

#define SUPPORTS_ADD_SEARCH_TO_HISTORY_LIST

class PrefsFile;
class SearchTemplate;
class SearchEngineItem;
class Window;
class UserSearches;
class DesktopWindow;
class TLDUpdater;

#define SEARCH_USER_ID_START    1000000		// ID at which user defined searches start from

#define DEFAULT_SEARCH_KEY    UNI_L("?")	// Default search key

#define g_searchEngineManager (SearchEngineManager::GetInstance())

static const struct search_id_map_s
{
	int old_id;
	const char* new_id;
} g_search_id_mapping[] =
{
	#include "data/translations/search-map.inc"
};

static const int num_search_id_mappings = sizeof(g_search_id_mapping) / sizeof(search_id_map_s);

/*********************************************************************************
 * 
 * SearchEngineManager holds a list m_search_engine_list of all search engines,
 * the ones from the package, and the user added ones, both visible and deleted 
 * ('hidden') ones, and a TreeModel UserSearches holding the searches
 * visible in the UI.
 *
 **********************************************************************************/


/***********************************************************************************
 ** SearchEngineManager
 ** -------------------
 **
 ** Option entries:
 **
 ** Default Search	 - The default search used in the address field and search field
 ** Speed Dial Search - The search used in Speed Dial
 **
 ** Search Engine Entry :
 **
 ** Name             - the name of the search engine
 ** URL              - the search url
 ** Default Tracking Code - string replaces the string {TrackingCode} in the URL field if it is present. This string identifies searches invoked from search or address fields.
 ** SpeedDial Tracking Code - string replaces the string {TrackingCode} in the URL field if it is present. This string identifies searches invoked from speeddial field.
 ** Query            - should be set to the query if "Is post" is 1
 ** Key              - the key for the search engine (e.g. "g" for google)
 ** Is post          - if the url should be a post (0 == NO and 1 == YES)
 ** Has endseparator - dropdown and menu : add seperator after
 ** Encoding         - the encoding of the url
 ** Search Type      - the search type (see search_types.h)
 ** Verbtext         - string id for format of the search label
 ** Position         - personal bar position
 ** Nameid           - language title id
 ** Suggest Protocol - The protocol used for search suggestions, if available. See below.
 ** Suggest URL		 - The url used for search suggestions. See below.
 **
 ** Suggest protocols:
 **
 ** BING - The Bing protocol format. Uses JSON. 
 ** WIKIPEDIA - The Wikipedia protocol format. Uses JSON.
 ** YANDEX - The Yandex protocol format. Uses JSON. 
 **
 ** Suggest URL keywords:
 **
 ** {SearchTerm} - The search term to search for. 
 **
 ***********************************************************************************/
// Is an OpTreeModel for OpPersonalbar
class SearchEngineManager : public OpTreeModel
#ifdef SEARCH_PROVIDER_QUERY_SUPPORT
	, public OpSearchProviderListener
#endif
{

public:


	enum 
	{
		FLAG_NONE                   =   0x0,
		FLAG_GUID                   =   0x00001,
		FLAG_KEY                    =	0x00002,		// Set if nick has changed
		FLAG_NAME                   =	0x00004,		// Set if name has changed
		FLAG_URL                    =	0x00008,		// Set if url has changed
		FLAG_ICON                   =   0x00010,
		FLAG_PERSONALBAR            =   0x00020,
		FLAG_ENCODING               =   0x00040,
		FLAG_QUERY                  =   0x00080,
		FLAG_ISPOST                 =   0x00100,
		FLAG_DELETED                =   0x00200,

		FLAG_MOVED_FROM             =   0x10000,
		FLAG_MOVED_TO               =   0x20000,
		FLAG_UNKNOWN				=	0xFFFFF,	// Set if change unknown (sets all of the flags)
	};

	enum SearchReqIssuer
	{
		SEARCH_REQ_ISSUER_ADDRESSBAR,
		SEARCH_REQ_ISSUER_SPEEDDIAL,
		SEARCH_REQ_ISSUER_OTHERS,
	};

	class SearchSetting : public OpenURLSetting
	{
	public:
		SearchSetting() 
			: OpenURLSetting()
			, m_search_template(0)
			, m_search_issuer(SearchEngineManager::SEARCH_REQ_ISSUER_ADDRESSBAR)
			{
				m_new_window    = MAYBE;
				m_new_page      = MAYBE;
				m_in_background = MAYBE;
			}

		SearchSetting(const OpenURLSetting& src)
			: OpenURLSetting(src)
			, m_search_template(0)
			, m_search_issuer(SearchEngineManager::SEARCH_REQ_ISSUER_ADDRESSBAR)
			{
				m_new_window    = MAYBE;
				m_new_page      = MAYBE;
				m_in_background = MAYBE;
			}

		OpString m_keyword;
		OpString m_key;
		SearchTemplate* m_search_template;
		SearchReqIssuer m_search_issuer;
	};

	static SearchEngineManager* GetInstance();

	void CleanUp();

#ifdef SUPPORT_SYNC_SEARCHES
	/**
	 * Listener class for listening to notifications about adds, removals or changes
	 * to items in the set corresponding to search.ini in the user profile.
	 * This set consists of changed or removed default searches, and user added searches.
	 * 
	 * This is not a listener meant for UI, as it notifies about changes to the internal
	 * structure of items, not about what should be shown in the UI.
	 */
	class Listener
	{
	public:
		virtual	void OnSearchEngineItemAdded(SearchTemplate* item) = 0;
		virtual	void OnSearchEngineItemChanged(SearchTemplate* item, UINT32 flag) = 0;
		virtual	void OnSearchEngineItemRemoved(SearchTemplate* item) = 0;

		// Listeners should rebuild search engines from scratch
		virtual void OnSearchEnginesChanged() = 0;
		virtual ~Listener() {}
	};
#endif

#ifdef SUPPORT_SYNC_SEARCHES
	OP_STATUS AddSearchEngineListener(Listener* listener) {  if (m_search_engine_listeners.Find(listener) < 0) return m_search_engine_listeners.Add(listener); else return OpStatus::ERR; }
	OP_STATUS RemoveSearchEngineListener(Listener* listener) { return m_search_engine_listeners.RemoveByItem(listener);}
#endif

	/**
	 * Will load the searches.
	 */
	void ConstructL();


	/**
	 * Reads in the search.ini and merges the user and package search.ini
	 * if that is neccessary.
	 */
	void LoadSearchesL();

	/**
	 * Writes out to the user search.ini
	 *
	 * @return OpStatus::OK if successful
	 */
	OP_STATUS Write();


	/**
	 *
	 * Get UserSearches model to be displayed in the UI
	 * (contains non-deleted and merged entries)
	 *
	 */
	UserSearches* GetSearchModel() { return m_usersearches; }



#ifdef SUPPORT_SYNC_SEARCHES
    /***
	 * Broadcast Add, Change and Remove of search engine item
	 *
	 * This broadcasts changes to the set of items in the user profile
	 * search.ini.
	 *
	 * When a default search engine is changed or removed for the first time,
	 * a BroadcastAdded notification is sent.
	 * 
	 * When a default search engine is changed or removed not for the first
	 * time, a BroadcastChanged notification is sent.
	 *
	 * For custom search engines, the notifications will correspond
	 * to 'normal' adds, changes and removals of items.
	 *
	 */
	void BroadcastSearchEngineAdded(SearchTemplate* search)
	{
		for ( UINT32 i = 0; i < m_search_engine_listeners.GetCount(); i++ )
		{
			Listener* listener = m_search_engine_listeners.Get(i);
			listener->OnSearchEngineItemAdded(search);
		}
	}

	void BroadcastSearchEngineChanged(SearchTemplate* search, UINT32 changed_flag)
	{
		for ( UINT32 i = 0; i < m_search_engine_listeners.GetCount(); i++ )
		{
			Listener* listener = m_search_engine_listeners.Get(i);
			listener->OnSearchEngineItemChanged(search, changed_flag);
		}
	}

	void BroadcastSearchEngineRemoved(SearchTemplate* search)
	{
		for ( UINT32 i = 0; i < m_search_engine_listeners.GetCount(); i++ )
		{
			Listener* listener = m_search_engine_listeners.Get(i);
			listener->OnSearchEngineItemRemoved(search);
		}
	}

	void BroadcastSearchEnginesChanged()
	{
		for ( UINT32 i = 0; i < m_search_engine_listeners.GetCount(); i++ )
		{
			Listener* listener = m_search_engine_listeners.Get(i);
			listener->OnSearchEnginesChanged();
		}
	}
#endif // SUPPORT_SYNC_SEARCHES

	/**
	 * Return TRUE if the text is single word and POSSIBLY a intranet page but not known to be one.
	 * So f.ex "wiki" would return TRUE, but "wiki/" or "wiki.com" would return FALSE.
	 */
	BOOL IsIntranet(const uni_char *text);

	/**
	 * Returns TRUE if the text looks like a url.
	 */
	BOOL IsUrl(const uni_char *text);

	/** Return TRUE if it's a hostname for a intranet. This checks with the list of
	  * stored intranet host names in prefs, previosly added with AddSpecialIntranetHost. */
	BOOL IsDetectedIntranetHost(const uni_char *text);

	/** Add a host name to the list of intranet hosts in prefs. All host names in this list
	 *  should be treaded like urls when f.ex typing them in the addressfield. */
	void AddDetectedIntranetHost(const uni_char *text);

	/**
	 * Returns TRUE if the text doesn't look like a url or search, and is a single word.
	 */
	BOOL IsSingleWordSearch(const uni_char *text);

	/**
	 * Attempt to start a search using the content of 'm_keyword'. The format
	 * can be "<key> <value>" where <key> is a known search engine key. The default
	 * search engine will be used if a valid key can not be extracted.
	 *
	 * Note that the content of 'settings' can be modified
	 *
	 * The 'm_keyword' is tested in such a way that a regular url or a nickname
	 * will not start a search.
	 *
	 * @param settings The 'm_keyword' member must be set
	 *
	 * @return TRUE if the search was performed, otherwise FALSE
	 */
	BOOL CheckAndDoSearch(SearchSetting& settings) { return CheckAndDoSearch(settings, TRUE); }

	/**
	 * Check if a search can be performed. Behavior is the same as for @ref CheckAndDoSearch
	 *
	 * Note that the content of 'settings' can be modified
	 *
	 * @param settings The 'm_keyword' member must be set
	 *
	 * @return TRUE when a search can be made, otherwise FALSE
	 */
	BOOL CanSearch(SearchSetting& settings) { return CheckAndDoSearch(settings, FALSE); }

	/**
	 * Search for keyword with specifed search engine, or if it fails, attempt a history
	 * or inline search. Note that the content of 'settings' can be modified
	 *
	 * @param settings Search setting. The 'm_keyword' and 'm_search_template' members
	 *        must always be set
	 *
	 * @return TRUE on success (search started) otherwise FALSE
	 */
	BOOL DoSearch(SearchSetting& settings);

	/**
	 * Open the history page and set the quickfind field to the string in search_terms
	 *
	 * @param search_terms to search for
	 */
	BOOL DoHistorySearch(const OpStringC& search_terms);

	/**
	 * Do inline find in the current page
	 *
	 * @param search_terms to search for
	 */
	BOOL DoInlineFind(const OpStringC& search_terms);

	/**
	 * This function is meant to add a search entry at the bottom of the list we
	 * get when preparing the autocomplete dropdown list. The list is assumed to
	 * contain three strings per item.
	 *
	 * This is ONLY used for OpEdit, the Address dropdown is an OpAddressDropDown
	 * and uses different functionality which was added for peregrine.
	 *
	 * @param search_key - the user typed string
	 * @param list       - an array representing the dropdown strings
	 * @param num_items  - the number of items in the list.
	 */
	void AddSearchStringToList(const uni_char* search_key,
									  uni_char**& list,
									  int& num_items);


	/**
	 * Find the SearchTemplate based on the id and make a search url
	 * based on that SearchTemplate and place that in url_object.
	 *
	 * @param url_object      - that should contain the finished url
	 * @param keyword         - that should be searched for
	 * @param resolve_keyword - whether the key word should be resolved
	 *                          (i.e. "www.opera.com" to "http://www.opera.com")
	 * @param id              - of the SearchTemplate requested
	 * @param search_req_issuer - identifies search issuer
	 * @param key			  - key used to init the search
	 *
	 *
	 * @return TRUE if successful - FALSE otherwise
	 */
	BOOL PrepareSearch(URL& url_object,
							  const uni_char* keyword,
							  BOOL resolve_keyword,
							  int id,
							  SearchReqIssuer search_req_issuer = SEARCH_REQ_ISSUER_ADDRESSBAR,
							  const OpStringC& key = UNI_L(""),
							  URL_CONTEXT_ID context_id = 0);

	/**
	 * Looks up the description of the search based on its index in the search
	 * engine list and gets the string that represents it.
	 *
	 * @param index - the index of the desired search
	 * @param buf   - the string will be set to the search's description
	 */
	void MakeSearchWithString(int index,
							  OpString& buf);
	void MakeSearchWithString(SearchTemplate* search, 
							  OpString& buf);


	/**
	 * Find the search string and label for a search key. If the key
	 * is empty the search string and label is made for the default
	 * search engine.
	 *
	 * The search_label will be set to the description for the
	 * search (i.e. search_label = "Google").
	 *
	 * The search_string will be set to the internal search url string
	 * (i.e. search_string = "g hello world").
	 *
	 * @param key           - the search key (eg. "g") can be empty
	 * @param value         - the search words
	 * @param search_label  - string that will be set to the name of the searchengine
	 * @param search_string - string that will be set to the internal search url
	 *
	 * @return TRUE if the search found is the default search
	 */
	BOOL FindSearchByKey(const OpStringC & key,
						 const OpStringC & value,
						 OpString & search_label,
						 OpString & search_string);
	

	/**
	 * Takes a string that may contain one or more spaces
	 * and splits it such that the first word (if there is one)
	 * is placed in key and the rest in value. If there is no space
	 * input is copied into value and key is left empty.
	 *
	 * @param input - the string to be split
	 * @param key   - the word before the space
	 * @param value - the words after the space
	 */
	static void SplitKeyValue(const OpStringC& input,
							  OpString & key,
							  OpString & value);

	
	/**
	 * @return TRUE if string is a search string i.e. "g hello world"
	 */
	BOOL IsSearchString(const OpStringC& input);

//  -------------------------
//  Conversion methods :
//  -------------------------

	const char* GetGUIDByOldId(int id);

	/**
	 * Gets the index of the item with the given id
	 *
	 * @param id - the requested id
	 *
	 * @return the index of the item with this id - default is 0
	 */
	int SearchIDToIndex(int id);


	/**
	 * Gets the id of the item with the given type
	 *
	 * @param type - the requested type
	 *
	 * @return the id of the item with this type, if the type is not found, this returns 0
	 */

	// NOTE: Only one item for each search type??
	int SearchTypeToID(SearchType type);
	int SearchTypeToIndex(SearchType type);

	/**
	 * Gets the id of the item at the given index
	 *
	 * @param index - the requested index
	 *
	 * @return the id of the item at the given index - default is 0
	 */
	int SearchIndexToID(int index);

	/**
	 * Gets the search type of the item with the given id
	 *
	 * @param id - the requested id
	 *
	 * @return the search type of the item with the given id -
	 *         default is 0 (SEARCH_TYPE_GOOGLE)
	 */
	SearchType IDToSearchType(int id);



	/**
	 * Gets the search with the given id
	 *
	 * @param id - the requested id
	 *
	 * @return the item with this id if it exists
	 */
	SearchTemplate* SearchFromID(int id);

	int GetSearchIndex(SearchTemplate* search);

//  -------------------------
//  Managing the search engine list :
//  -------------------------

	/**
	 * Adds a new search template to the search engine manager.
	 * @param search_engine The search engine to be added. Ownership is taken
	 *        over by this function.
	 * @param char_set The character set of the new search engine.
	 * @sa EditSearchEngineDialog
	 */
	OP_STATUS AddSearch(SearchEngineItem* search_engine, OpStringC char_set);

	/**
	 * Edits an existing search template in the search engine manager
	 * @param search_engine The item holding all the changes to be made.
	 *        Ownership is taken over by this function.
	 * @param char_set The character set of the changed item.
	 * @param item_to_change The item that the above changes will be applied to.
	 * @param default_search_changed Indicates if the default search engine has
	 *        changed in search_engine.
	 * @param speeddial_search_changed Indicates if the default SpeedDial search
	 *        engine has changed in search_engine.
	 * @sa EditSearchEngineDialog
	 */
	OP_STATUS EditSearch(SearchEngineItem * search_engine, OpStringC char_set, SearchTemplate * item_to_change, BOOL default_search_changed = FALSE, BOOL speeddial_search_changed = FALSE);

	/**
	 * Retrieve number of search engines.
	 *
	 * @return the number of search engines
	 */
	inline unsigned int GetSearchEnginesCount()	{ return m_search_engines_list.GetCount(); }


	/**
	 * Retrieves the search template at a specific index
	 *
	 * @param i - the index of the desired item
	 *
	 * @return the item if it exists
	 */
	SearchTemplate* GetSearchEngine(int i) { return m_search_engines_list.Get(i); };

	/**
	 * Retrieves the search template with a specific user id
	 *
	 * @param i - user id the desired item
	 *
	 * @return the item if it exists
	 */
	SearchTemplate* GetDefaultSearch();
	SearchTemplate* GetDefaultSpeedDialSearch();
#ifdef SEARCH_PROVIDER_QUERY_SUPPORT
	SearchTemplate* GetDefaultErrorPageSearch();
#endif // SEARCH_PROVIDER_QUERY_SUPPORT
	OP_STATUS SetDefaultSearch(const OpStringC& search);
	OP_STATUS SetDefaultSpeedDialSearch(const OpStringC& search);
	
#ifdef SEARCH_PROVIDER_QUERY_SUPPORT
	//from OpSearchProviderListener
	virtual SearchProviderInfo* OnRequestSearchProviderInfo(RequestReason reason);
#endif
	/**
	 * Retrieves the search template with a specific key
	 *
	 * @param key for the search engine
	 *
	 * @return the item if it exists
	 */
	SearchTemplate* GetSearchEngineByKey(const OpStringC& key);


	/**
	 * Returns TRUE if key is already in use
	 *
	 * @param key for the search engine
	 * @param exclude_search item to exclude from the search
	 *
	 * (Note: There can  currently more than one item with the same key,'
	 *  so just doing GetSearchEngineByKey doesn't work here
	 *  because the item returned might be exclude_search, but there
	 *  might still be another item with this key too)
	 *
	 * @return TRUE if in use
	 */

	BOOL KeyInUse(const OpStringC& key, 
				  SearchTemplate* exclude_search);


	/**
	 * Creates search key that is not in use.
	 *
	 * @param ref_key optional reference key used to generate new key
	 * @param ref_name optional reference name used to generate new key
	 * @param new_key gets new key on success
	 *
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY
	 */
	OP_STATUS FindFreeKey(OpStringC ref_key, OpStringC ref_name, OpString& new_key);


	/**
	 * Adds a search item
	 *
	 * @param search - item to be added
	 *
	 * @return OpStatus::OK if successful
	 */
	OP_STATUS AddItem(SearchTemplate* search);


	/**
	 * Removes a search item
	 *
	 * @param search - item to be removed
	 *
	 * @return OpStatus::OK if successful
	 */
	OP_STATUS RemoveItem(SearchTemplate* search);


	/**
	 * Gets the index of an item
	 *
	 * @param search - the item to be search for
	 *
	 * @return the index of the item
	 */
	INT32 FindItem(SearchTemplate* search) {return m_search_engines_list.Find(search);}


	/**
	 * Force this search to become a user search
	 *
	 * @param search - item to be changed
	 * @param pos    - the index of the item in the search engine list
	 * @param flag   - indicates which fields on the item has changed
	 */
	void ChangeItem(SearchTemplate* search, INT32 pos, UINT32 flag);

	SearchTemplate* GetByUniqueGUID(const OpStringC& guid);

	// Updates the google top level domain (TLD) i.e get's google.ru, google.no, etc
	OP_STATUS UpdateGoogleTLD();

	// == OpTreeModel ======================

	virtual INT32				GetColumnCount() { return 2; }
	virtual OP_STATUS			GetColumnData(ColumnData* column_data);
	virtual OpTreeModelItem*	GetItemByPosition(INT32 position);
	virtual INT32				GetItemParent(INT32 position) {return -1;}
	virtual INT32				GetItemCount() {return m_search_engines_list.GetCount();}

	/**
	 * Returns true if LoadSearchesL has been called at least once.
	 */
	bool HasLoadedConfig() const { return m_user_search_engines_file != NULL; }

private:
	/**
	 * Checks if the url the user typed in is a search-command if so, calls DoSearch [all os]
	 * if the url is not a search command the default search engine will be called if
	 * the url does not seem to be a url (i.e. it looks like "hello world"  or similar)
	 *
	 * @param settings Search setting. The 'm_keyword' and 'm_search_template' members
	 *        must always be set
	 * @param do_search If FALSE, it will only calculate a return value without actually
	 *        do the search.
	 *
	 * @return TRUE if the search was performed, otherwise FALSE
	 */
	BOOL CheckAndDoSearch(SearchSetting& settings, BOOL do_search);

	/**
	 * Copies the searches from a prefs file to the class lists
	 *
	 * @param prefsfile    -
	 * @param from_package -
	 */
	void AddSearchesL(PrefsFile* prefsfile,
					  BOOL from_package = TRUE);

	/**
	 * Loads default and speed dial search from hardcoded searches.
	 */
	void AddHardcodedSearchesL();

	/**
	 * Searches the current m_user_search_engines_list for a search
	 *
	 * @param original_search   -
	 * @param index             -
	 * @param from_package_only -
	 *
	 * @return
	 */
	SearchTemplate * FindSearchL(const SearchTemplate *original_search,
								 INT32 *index,
								 BOOL from_package_only = TRUE);

	/**
	 * Inserts fallback search into the list of search engines and resolves keyword
	 * conflicts caused by adding fallback search.
	 *
	 * @param search fallback search
	 * @param position position of fallback search
	 *
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY
	 */
	OP_STATUS InsertFallbackSearch(SearchTemplate* search, int position);

	/**
	 * Temporary Exception from search protection for Yandex.
	 * TODO: remove when enough people have transitioned to the new search protection.
	 * If the default search has been hijacked, and happens to be set to Yandex, we let it be a Yandex search
	 * but we make sure it corresponds to our deal with yandex.
	 * Without this, it would be replaced by google, which our current deals don't permit.
	 *
	 * @param search the search that was detected as being hijacked.
	 *
	 * @return true if the exception was applied. When false is returned, the normal operation mode for
	 *         hijacked searches should be used instead.
	 */
	bool TryYandexSearchProtectionException(SearchTemplate *search);

	/**
	 * Checks protected searches. If search is found to be tampered it is replaced with
	 * package search (if not modified) or copy of package search made before merging
	 * changes from user's search.ini.
	 *
	 * @param copy_of_package_default_search copy of default search from package search.ini
	 * @param copy_of_package_speeddial_search copy of speed dial search from package search.ini
	 *
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY
	 */
	OP_STATUS CheckProtectedSearches(OpStackAutoPtr<SearchTemplate>& copy_of_pacakge_default_search,
									 OpStackAutoPtr<SearchTemplate>& copy_of_package_speeddial_search);

	// Initialization and destruction

	SearchEngineManager();
	~SearchEngineManager();

private:
	// Version of the package search.ini :
	int m_version;

	// Complete set of searchengines :
	OpFilteredVector<SearchTemplate> m_search_engines_list;

	// Used to read both the package search.ini and user search.ini,
	// but holds and writes out just the package search.ini
	PrefsFile* m_user_search_engines_file;

	INT32 m_next_user_id; // Holds the highest user search id currently assigned

	UserSearches* m_usersearches;
	
	TLDUpdater* m_tld_update;		// Google top level domain (TLD) update object

#ifdef SUPPORT_SYNC_SEARCHES
	OpVector<Listener> m_search_engine_listeners;
#endif

	OpString	m_default_search_guid;				// GUID if the default search
	OpString	m_default_speeddial_search_guid;	// GUID if the default Speed Dial search

	OpString	m_package_default_search_guid;		// GUID if the default search
	OpString	m_package_speeddial_search_guid;	// GUID if the default Speed Dial search

	OpString	m_fallback_default_search_guid;     // GUID of search to use instead of tampered default search
	OpString	m_fallback_speeddial_search_guid;   // GUID of search to use instead of tampered Speed Dial search

#ifdef SEARCH_PROVIDER_QUERY_SUPPORT
	OpString	m_default_error_search_guid;				// GUID if the default error search
	OpString	m_package_error_search_guid;		// GUID if the default error search
#endif // SEARCH_PROVIDER_QUERY_SUPPORT
};


/***********************************************************************************
 **
 ** UserSearches
 ** ------------
 **
 **
 ***********************************************************************************/

class UserSearches : public TreeModel<SearchTemplate>
{
public:

	friend class SearchEngineManager;

	UserSearches() {}
	~UserSearches() {}

	// == OpTreeModel ======================

	virtual INT32				GetColumnCount() {return 3;}
	virtual INT32				GetItemParent(INT32 position) {return -1;}
	virtual OP_STATUS			GetColumnData(ColumnData* column_data);

private:

	OP_STATUS					AddUserSearch(SearchTemplate*);
	OP_STATUS					RemoveUserSearch(SearchTemplate*);

	void Clear() { RemoveAll(); }
};

#endif // DESKTOP_UTIL_SEARCH_ENGINES

#endif // SEARCHMANAGER_H
