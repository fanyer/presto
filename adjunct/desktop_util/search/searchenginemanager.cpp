/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef DESKTOP_UTIL_SEARCH_ENGINES

#include "modules/prefsfile/prefsfile.h"
#include "adjunct/desktop_util/search/searchenginemanager.h"
#include "adjunct/desktop_util/search/search_net.h"
#include "adjunct/desktop_util/search/search_protection.h"
#include "adjunct/desktop_util/search/tldupdater.h"
#include "adjunct/desktop_util/search/search_field_history.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/desktop_util/prefs/PrefsUtils.h"
#include "adjunct/desktop_util/resources/ResourceSetup.h"
#include "adjunct/quick/dialogs/PreferencesDialog.h"
#include "modules/locale/locale-enum.h"
#include "modules/hardcore/unicode/unicode.h"
#include "modules/util/opfile/opfile.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/util/filefun.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/dochand/win.h"
#include "modules/dom/domutils.h"
#include "modules/idna/idna.h"

#ifdef ENABLE_USAGE_REPORT
#include "adjunct/quick/usagereport/UsageReport.h"
#endif

/** Name of file to read search engines from. */
#define SEARCHDEFINES UNI_L("search.ini")

/** Template for search.ini sections. */
#define STD_SECTION "Search Engine %d"

// --------------------------------------------------------------------------------
//
// class SearchEngineManager
//
// --------------------------------------------------------------------------------

// Should be changed to use the TreeModel instead of the extra UserSearches TreeModel
// Currently it is a treemodel only to force calls to some methods used by
// OpPersonalbar - 
//
// And because of the hack, for example, removed entries aren't removed from pbar.
//
// The api also needs some cleanup as some member fields are accessed directly 
// (from core), and there is this whole mess of GetSearch[Whatever]From[Id/UserId/Index]...
//
//

SearchEngineManager* SearchEngineManager::GetInstance()
{
	static SearchEngineManager s_search_engine_manager;
	return &s_search_engine_manager;
}


/***********************************************************************************
 *
 * SearchEngineManager Constructor
 *
 * Sets version to 9 - the version where the new dual file system started
 * Sets m_next_user_id to SEARCH_USER_ID_START which is the lowest possible 
 *   user id (1000000)
 * 
 ***********************************************************************************/
SearchEngineManager::SearchEngineManager()
  : m_version(9)  
  ,m_user_search_engines_file(NULL)
  ,m_next_user_id(SEARCH_USER_ID_START)		
  ,m_usersearches(0)
  ,m_tld_update(NULL)
{}

SearchEngineManager::~SearchEngineManager()
{
#ifdef SEARCH_PROVIDER_QUERY_SUPPORT
	if (g_opera && g_windowCommanderManager->GetSearchProviderListener() == this)
		g_windowCommanderManager->SetSearchProviderListener(NULL);
#endif //SEARCH_PROVIDER_QUERY_SUPPORT
}

#ifdef SEARCH_PROVIDER_QUERY_SUPPORT
//from OpSearchProviderListener
OpSearchProviderListener::SearchProviderInfo*
SearchEngineManager::OnRequestSearchProviderInfo(OpSearchProviderListener::RequestReason reason)
{
	SearchTemplate* st = GetDefaultErrorPageSearch();
	return st ? &st->m_spii : NULL;
}
#endif

/***********************************************************************************
 *
 * ConstructL
 *
 * Creates UserSearches model and calls LoadSearchesL to read the search.ini
 * package and user file and build the internal list of search engines and the
 * UserSearches model used in the UI treeviews.
 *
 ***********************************************************************************/
void SearchEngineManager::ConstructL()
{
#ifdef SEARCH_PROVIDER_QUERY_SUPPORT
	g_windowCommanderManager->SetSearchProviderListener(this);
#endif //SEARCH_PROVIDER_QUERY_SUPPORT
	m_usersearches = OP_NEW_L(UserSearches, ());

	// LoadSearchesL will be called by OpBootManager when it gets
	// name of user's region. This may be delayed for a few seconds
	// if Opera needs to contact the autoupdate server to get IP-based
	// country code (DSK-351304).
}

void SearchEngineManager::CleanUp()
{
	if (m_usersearches)
		m_usersearches->Clear();

	m_search_engines_list.DeleteAll();

    OP_DELETE(m_user_search_engines_file);
	m_user_search_engines_file = NULL;
	OP_DELETE(m_usersearches);
	m_usersearches = NULL;
	OP_DELETE(m_tld_update);
	m_tld_update = NULL;
}

class SearchIniChecker : public ResourceSetup::PrefsFileChecker
{
public:
	SearchIniChecker() : m_skip_defaults(MAYBE) {}

	virtual OP_STATUS CheckFile(OpFileFolder folder, const OpStringC& filename, BOOL& is_valid);

private:
	SearchProtection::SignatureChecker m_signature_checker;
	BOOL3 m_skip_defaults;
};

OP_STATUS SearchIniChecker::CheckFile(OpFileFolder folder, const OpStringC& filename, BOOL& is_valid)
{
	if (m_skip_defaults == YES && folder == OPFILE_INI_FOLDER)
	{
		is_valid = FALSE;
		return OpStatus::OK;
	}
	OpFile file;
	RETURN_IF_ERROR(file.Construct(filename, folder));
	RETURN_IF_ERROR(file.Exists(is_valid));
	if (is_valid)
	{
		OP_BOOLEAN sig_valid = m_signature_checker.VerifyFile(filename, folder);
		if (sig_valid == OpBoolean::IS_FALSE && m_skip_defaults == MAYBE)
		{
			// skip [resources]/defaults/search.ini if Opera has hardcoded searches for current region and/or language (DSK-335696, DSK-363175)
			m_skip_defaults = SearchProtection::HardcodedSearchEngines::HasLocalizedSearches() ? YES : NO;
		}
		if (sig_valid != OpBoolean::IS_TRUE)
		{
			is_valid = FALSE;
		}
		RETURN_IF_MEMORY_ERROR(sig_valid);
	}
	return OpStatus::OK;
}


/***********************************************************************************
 *
 * LoadSearchesL
 *
 * Reads package search.ini and user search.ini file 
 * Merges these into m_search_engine_list, and builds the UserSearches model
 *  to be used in the UI.
 *
 *
 ***********************************************************************************/
void SearchEngineManager::LoadSearchesL()
{
	if( m_usersearches )
	{
		m_usersearches->Clear();
	}

	m_search_engines_list.DeleteAll();

	m_fallback_default_search_guid.Empty();
	m_fallback_speeddial_search_guid.Empty();

	OP_DELETE(m_user_search_engines_file);
	m_user_search_engines_file = NULL;

	{
		OpStackAutoPtr<PrefsFile> additional(OP_NEW_L(PrefsFile, (PREFS_STD)));
		additional->ConstructL();
		m_user_search_engines_file = additional.release();
	}

	// There are 2 search.ini files that need to be read in and merged together.
	// First read in the package search.ini... This is a direct read in.
	// Secondly read in the user search.ini... This will only add items to the list
	//                                         that are not already in the list and
	//										   remove any that have been specifically
	//                                         marked to be removed (i.e. any of the
	//                                         orginal package search.ini searches)

	// Figure out which of the search.ini's to use from the package.
	ANCHORD(OpFile, searchdeffile);
	BOOL exists = FALSE;

	{
		ANCHORD(SearchIniChecker, searchdef_checker);
		OP_STATUS status = ResourceSetup::GetDefaultPrefsFile(SEARCHDEFINES, searchdeffile, &searchdef_checker);
		LEAVE_IF_FATAL(status);
		exists = OpStatus::IsSuccess(status);
	}

	// Now we need the name of the user search.ini, which is only in one location
	// This will work by accident since on Mac OPFILE_HOME_FOLDER and
	// OPFILE_USERPREFS_FOLDER are in the same location but I don't think that
	// this is right - adamm 27-02-2007
	ANCHORD(OpFile, searchuserfile);
	LEAVE_IF_ERROR(searchuserfile.Construct(SEARCHDEFINES, OPFILE_HOME_FOLDER));
	m_user_search_engines_file->SetFileL(&searchuserfile);

	// At this point we should have searchdeffile pointing at one of the package
	// search.ini's and searchuserfile pointing at the user search.ini

	if (exists)
	{
		PrefsFile package_search_engines(PREFS_STD);
		ANCHOR(PrefsFile, package_search_engines);

		// Set the package search.ini file into the prefsfile
		package_search_engines.ConstructL();
		package_search_engines.SetFileL(&searchdeffile);

		// Add all the searches from the package seach.ini
		AddSearchesL(&package_search_engines);

		// Hardcoded searches will not be used in this session, but since we have valid package
		// searches SearchProtection might want to use them to initialize IDs of preferred hardcoded
		// searches.
		SearchTemplate* default_search = GetDefaultSearch();
		SearchTemplate* speeddial_search = GetDefaultSpeedDialSearch();
		LEAVE_IF_FATAL(SearchProtection::InitIdsOfHardcodedSearches(default_search, speeddial_search));
	}
	else
	{
		// There were no package searches or they were all tampered - fall back to hardcoded searches
		AddHardcodedSearchesL();
	}

	// We may need genuine package searches to replace tampered default searches, so create
	// copies before merging profile search.ini.
	OpStackAutoPtr<SearchTemplate> copy_of_default_search_ptr(NULL);
	OpStackAutoPtr<SearchTemplate> copy_of_speeddial_search_ptr(NULL);
	{
		SearchTemplate* default_search = GetDefaultSearch();
		if (default_search)
		{
			SearchTemplate* copy = NULL;
			LEAVE_IF_FATAL(default_search->CreatePackageCopy(copy));
			copy_of_default_search_ptr.reset(copy);
		}
		SearchTemplate* speeddial_search = GetDefaultSpeedDialSearch();
		if (speeddial_search && speeddial_search != default_search)
		{
			SearchTemplate* copy = NULL;
			LEAVE_IF_FATAL(speeddial_search->CreatePackageCopy(copy));
			copy_of_speeddial_search_ptr.reset(copy);
		}
	}

	// Now we need to merge in the searches from the user search.ini
	// Load up the user searches
	m_user_search_engines_file->LoadAllL();

	// Add all the searches from the user search.ini
	AddSearchesL(m_user_search_engines_file, FALSE);

	LEAVE_IF_ERROR(CheckProtectedSearches(copy_of_default_search_ptr, copy_of_speeddial_search_ptr));

	// Add to UserSearches model 
	// ---- Build the UserSearches model from the m_search_engines_list,
	// ---- adding all entries that have a key

	SearchTemplate  *search;
	BOOL old_format = FALSE;

	for (INT32 i = 0; i < m_search_engines_list.GetCount(); i++)
	{
		search = m_search_engines_list.Get(i);

		// Check if there are any Non-ID (i.e. old format) searches if so we want to write this file back out
		if (search->GetNonID())
		{
			old_format = TRUE;
		}

//		printf("ID: %d Non-ID: %d Search: %s\n", search->GetUserID(), search->GetNonID(), search->m_url.UTF8AllocL());

		// Add the search to the user search if it has a key, (i.e. this will be displayed in the UI)
		if (!search->GetKey().IsEmpty() && m_usersearches)
		{
			m_usersearches->AddUserSearch(search);
		}
	}

	// printf("Next ID: %d\n", m_next_user_id);

	// If there was an old format search then just write out again!
	// Old format change will only occur between version 9.2x and 9.50
	if (old_format)
	{
		Write();
	}

#ifdef SUPPORT_SYNC_SEARCHES
	// BroadcastSearchEnginesChanged(); // For Link
#endif

	// This broadcasts currently to OpPersonalbar
	BroadcastTreeChanged();

}


/***********************************************************************************
 *
 * AddSearchesL
 *
 * @param PrefsFile prefsfile - File to add searches from
 * @param BOOL from_package   - TRUE if this is the package search.ini file.
 *                              Default is TRUE
 *
 * Reads prefsfile and adds all the searches to the internal list of search engines,
 * m_search_engines_list. 
 * 
 * If the file is not the package file, and an entry with this URL is already
 * in the internal list, the entry already there gets its fields overwritten with
 * the read entrys fields - so that user changes are merged into the list, including
 * deletes of package file entries.
 *
 * If the search is not already in the list, it is added.
 *
 ***********************************************************************************/

void SearchEngineManager::AddSearchesL(PrefsFile* prefsfile,
									   BOOL from_package)
{
	BOOL  add_search, deleted;
	INT32 index;

	// Load up the prefs file
	prefsfile->LoadAllL();

	// Hold the version of the package search.ini so it can be written into
	// the user file 9 is the default since that is the version that this new
	// package/user dual search.ini system started - adamm05-03-2007
	if (from_package)
		m_version = prefsfile->ReadIntL("Version", "File Version", 9);

	char currsection[64]; /* ARRAY OK 2004-02-02 rikard */
	OpString tmp_default;
	int k = 1;

	// Default Search	 - The default search used in the address field and search field
	// Speed Dial Search - The search used in Speed Dial

	tmp_default.SetL(prefsfile->ReadStringL("Options", "Default Search"));
	if(tmp_default.HasContent())
	{
		if(from_package)
		{
			m_package_default_search_guid.SetL(tmp_default);
			m_default_search_guid.SetL(tmp_default);
		}
		else
		{
			m_default_search_guid.SetL(tmp_default);
		}
	}
	tmp_default.SetL(prefsfile->ReadStringL("Options", "Speed Dial Search"));
	if(from_package)
	{
		m_package_speeddial_search_guid.SetL(tmp_default);
		m_default_speeddial_search_guid.SetL(tmp_default);
	}
	else
	{
		// only set it if it's present in the file
		if(prefsfile->IsKey("Options", "Speed Dial Search"))
		{
			m_default_speeddial_search_guid.SetL(tmp_default);
		}
	}
#ifdef SEARCH_PROVIDER_QUERY_SUPPORT
	tmp_default.SetL(prefsfile->ReadStringL("Options", "Error Search"));
	if(tmp_default.HasContent())
	{
		if(from_package)
		{
			m_package_error_search_guid.SetL(tmp_default);
			m_default_error_search_guid.SetL(tmp_default);
		}
		else
		{
			m_default_error_search_guid.SetL(tmp_default);
		}
	}
#endif // SEARCH_PROVIDER_QUERY_SUPPORT
	// Loop the sections (i.e. [Search Engine xx]) in the loaded up search.ini
	// and add them
	while (TRUE)
	{
		// Build the section name to load
		op_snprintf(currsection, 64, STD_SECTION, k);
		currsection[63] = 0;

		if (!prefsfile->IsSection(currsection))
			break;

		SearchTemplate *customnewone = OP_NEW_L(SearchTemplate, ());
		OpStackAutoPtr<SearchTemplate> customnewone_ap(customnewone);

		deleted = customnewone->ReadL(currsection, prefsfile, from_package ? SearchTemplate::PACKAGE : SearchTemplate::CUSTOM);
		add_search = !deleted;
		index = -1;

		// We need to determine what happens now if this is a search from
		// the user search.ini
		if (!from_package)
		{

			// Does this search exist in the list of loaded package searches already?
			// This search is only based on the ID or url
			SearchTemplate *match = FindSearchL(customnewone, &index, TRUE);

			// If the item is a "special" deleted item then we need to remove the item.
			if (match)
			{
				// Matches are never added
				add_search = FALSE;

				// Exact UI matches are considered to be in the user search.ini
				// because they existed in the old single file format. This should
				// not stay in this file, therefore UI matched searches are not
				// merged over. Any searches marked as deleted must be new so must
				// be merged
				if (deleted || !match->IsMatch(customnewone, TRUE))
				{
					// Copy the fields from customnewone to match,
					// replacing the one in the list already with the one read here.
					LEAVE_IF_ERROR(match->Copy(customnewone, FALSE));
					match->SetIsCustomOrModified();
				}
			}
		}

		// Add the search if it wasn't found in the list (from the package) already)
		if (add_search)
		{
			if (customnewone->GetUniqueGUID().HasContent())
				OP_ASSERT(!GetByUniqueGUID(customnewone->GetUniqueGUID()));

			// Add the search to the internal list
			if (OpStatus::IsSuccess(m_search_engines_list.Add(customnewone)))
			{
				customnewone_ap.release();
			}

			// Set the deleted index to the last item if it was just added
			index = m_search_engines_list.GetCount(CompleteVector) - 1;
		}
		
		// Deleted searches need to be hidden
		if (deleted)
		{
			m_search_engines_list.SetVisibility(m_search_engines_list.GetIndex(index, VisibleVector, CompleteVector), FALSE);
		}

		k++;
	}

	prefsfile->Flush();
}


OP_STATUS SearchEngineManager::AddSearch(SearchEngineItem* search_engine, OpStringC char_set)
{
	if (!search_engine)
		return OpStatus::ERR_NULL_POINTER;

	OpAutoPtr<SearchEngineItem> auto_delete(search_engine);
	if (!search_engine->IsValid())
		return OpStatus::OK; // nothing to do ...

	OpString key;
	RETURN_IF_ERROR(search_engine->GetKey(key));
	OpString name;
	RETURN_IF_ERROR(search_engine->GetName(name));
	OpString url;
	RETURN_IF_ERROR(search_engine->GetURL(url));
	OpString query;
	RETURN_IF_ERROR(search_engine->GetQuery(query));
	BOOL is_post = search_engine->IsPost();

	if (!key.HasContent() || !name.HasContent() || !url.HasContent())
		return OpStatus::OK; // nothing to do ...

	OpAutoPtr<SearchTemplate> item(OP_NEW(SearchTemplate, ()));
	RETURN_OOM_IF_NULL(item.get());
	RETURN_IF_ERROR(item->SetURL(url));
	RETURN_IF_ERROR(item->SetName(name));
	RETURN_IF_ERROR(item->SetKey(key));
	RETURN_IF_ERROR(item->SetQuery(query));
	RETURN_IF_ERROR(item->SetEncoding(char_set));
	item->SetSearchType(SEARCH_TYPE_GOOGLE);
	item->SetIsPost(is_post);

	RETURN_IF_ERROR(g_searchEngineManager->AddItem(item.get()));
	SearchTemplate* itemp = item.release();

	OpString guid;
	if (search_engine->IsDefaultEngine() &&
		OpStatus::IsSuccess(itemp->GetUniqueGUID(guid)))
		OpStatus::Ignore(SetDefaultSearch(guid));

	if (search_engine->IsSpeeddialEngine() &&
		(guid.HasContent() || OpStatus::IsSuccess(itemp->GetUniqueGUID(guid))))
		OpStatus::Ignore(SetDefaultSpeedDialSearch(guid));

	RETURN_IF_ERROR(Write());

	// Update search fields if they were changed
	if (search_engine->IsDefaultEngine() || search_engine->IsSpeeddialEngine())
		g_application->SettingsChanged(SETTINGS_SEARCH);

	return OpStatus::OK;
}

OP_STATUS SearchEngineManager::EditSearch(SearchEngineItem* search_engine, OpStringC char_set, SearchTemplate* item_to_change, BOOL default_search_changed, BOOL speeddial_search_changed)
{
	if (!search_engine)
		return OpStatus::ERR_NULL_POINTER;

	OpAutoPtr<SearchEngineItem> auto_delete(search_engine);
	if (!search_engine->IsValid())
		return OpStatus::OK; // nothing to do ...

	OpString engine_name;
	RETURN_IF_ERROR(item_to_change->GetEngineName(engine_name, FALSE));

	OpString key;
	RETURN_IF_ERROR(search_engine->GetKey(key));
	OpString name;
	RETURN_IF_ERROR(search_engine->GetName(name));
	OpString url;
	RETURN_IF_ERROR(search_engine->GetURL(url));
	OpString query;
	RETURN_IF_ERROR(search_engine->GetQuery(query));
	BOOL is_post = search_engine->IsPost();

	if (!key.HasContent() || !name.HasContent() || !url.HasContent())
		return OpStatus::OK; // nothing to do ...

	UINT flag = 0;
	if (key.Compare(item_to_change->GetKey()))
		flag |= SearchEngineManager::FLAG_KEY;
	if (name.Compare(engine_name))
		flag |= SearchEngineManager::FLAG_NAME;
	if (url.Compare(item_to_change->GetUrl()))
		flag |= SearchEngineManager::FLAG_URL;
	if (query.Compare(item_to_change->GetQuery()))
		flag |= SearchEngineManager::FLAG_QUERY;
	if (is_post != item_to_change->GetIsPost())
		flag |= SearchEngineManager::FLAG_ISPOST;

	if (flag)
	{
		// data has changed, we need to save it back
		INT32 pos = FindItem(item_to_change);
		if (pos != -1)
		{
			RETURN_IF_ERROR(item_to_change->SetKey(key));
			RETURN_IF_ERROR(item_to_change->SetName(name));
			RETURN_IF_ERROR(item_to_change->SetURL(url));
			RETURN_IF_ERROR(item_to_change->SetQuery(query));
			item_to_change->SetIsPost(is_post);
			ChangeItem(item_to_change, pos, flag);
		}
	}

	if (default_search_changed)
	{
		// After editing, the engine is default -> need to change if it wasn't before
		OpString guid;
		if (search_engine->IsDefaultEngine())
		{
			if (OpStatus::IsSuccess(item_to_change->GetUniqueGUID(guid)))
				OpStatus::Ignore(SetDefaultSearch(guid));
		}
		else // was default before, isn't anymore. Reset to default.
		{
			// fallback to google if you uncheck the default checkbox
			OpStatus::Ignore(SetDefaultSearch(guid));
		}
	}

	if (speeddial_search_changed)
	{
		// After editing, the engine is sd default -> need to change if it wasn't before
		OpString guid;
		if (search_engine->IsSpeeddialEngine())
		{
			if (OpStatus::IsSuccess(item_to_change->GetUniqueGUID(guid)))
				OpStatus::Ignore(SetDefaultSpeedDialSearch(guid));
		}
		else // was default before, isn't anymore. Reset to default.
		{
			// Default is to show nothing (removes the search from speed dial)
			OpStatus::Ignore(SetDefaultSpeedDialSearch(guid));
		}
	}

	OP_STATUS status = OpStatus::OK;
	if (flag != 0 || default_search_changed || speeddial_search_changed)
	{
		status = Write();
		g_application->SettingsChanged(SETTINGS_SEARCH);
	}
	return status;
}


/***********************************************************************************
 *
 * AddHardcodedSearchesL
 *
 * Loads default and speed dial search from hardcoded searches.
 *
 ***********************************************************************************/

void SearchEngineManager::AddHardcodedSearchesL()
{
	ANCHORD(SearchProtection::HardcodedSearchEngines, hardcoded_engines);
	LEAVE_IF_ERROR(hardcoded_engines.Init());

	const SearchProtection::SearchEngine* hardcoded_default =
		hardcoded_engines.GetSearchEngine(SearchProtection::DEFAULT_SEARCH);
	if (hardcoded_default)
	{
		SearchTemplate* search;
		LEAVE_IF_ERROR(hardcoded_default->CreateSearchTemplate(search));
		OP_ASSERT(search->GetUniqueGUID().HasContent());
		OP_ASSERT(GetByUniqueGUID(search->GetUniqueGUID()) == NULL);
		OP_STATUS status = m_search_engines_list.Add(search);
		if (OpStatus::IsError(status))
		{
			OP_DELETE(search);
			LEAVE(status);
		}
		m_package_default_search_guid.SetL(search->GetUniqueGUID());
		m_default_search_guid.SetL(search->GetUniqueGUID());
	}

	const SearchProtection::SearchEngine* hardcoded_speeddial =
		hardcoded_engines.GetSearchEngine(SearchProtection::SPEED_DIAL_SEARCH);
	if (hardcoded_speeddial)
	{
		if (hardcoded_speeddial->GetUniqueIdL() == m_package_default_search_guid)
		{
			m_package_speeddial_search_guid.SetL(m_package_default_search_guid);
			m_default_speeddial_search_guid.SetL(m_package_default_search_guid);
		}
		else
		{
			SearchTemplate* search;
			LEAVE_IF_ERROR(hardcoded_speeddial->CreateSearchTemplate(search));
			OP_ASSERT(search->GetUniqueGUID().HasContent());
			OP_ASSERT(GetByUniqueGUID(search->GetUniqueGUID()) == NULL);
			OP_STATUS status = m_search_engines_list.Add(search);
			if (OpStatus::IsError(status))
			{
				OP_DELETE(search);
				LEAVE(status);
			}
			m_package_speeddial_search_guid.SetL(search->GetUniqueGUID());
			m_default_speeddial_search_guid.SetL(search->GetUniqueGUID());
		}
	}
}


/***********************************************************************************
 *
 * FindSearchL
 *
 * @param SearchTemplate original_search
 * @param INT32 *index - returns index of item which matches original_search
 * @param BOOL from_package_only - If TRUE, skip searches that are not from package
 *
 ***********************************************************************************/

SearchTemplate *SearchEngineManager::FindSearchL(const SearchTemplate *original_search,
												 INT32 *index,
												 BOOL from_package_only)
{
	SearchTemplate *search = 0;
	INT32			count  = m_search_engines_list.GetCount();

	// Default index
	if (index)
	{
		*index = -1;
	}

	for (INT32 i = 0; i < count; i++)
	{
		search = m_search_engines_list.Get(i);

		// OBS OBS TODO: This skips modified package searches as well as custom added searches -> IS THAT THE INTENTION??
		// Skip over user seaches if this a from package only search
		if (from_package_only && !search->IsFromPackage())
			continue;

		// Check for a match
		if (original_search->IsMatch(search))
		{
			// Return the index
			if (index)
				*index = i;

			return search;
		}
	}
	return NULL;
}


/***********************************************************************************
 *
 * InsertFallbackSearch
 *
 * @param search fallback search
 * @param position position of fallback search
 *
 ***********************************************************************************/

OP_STATUS SearchEngineManager::InsertFallbackSearch(SearchTemplate* search, int position)
{
	OP_ASSERT(search->IsCopyFromPackage());
	// check for key conflicts
	OpStringC key = search->GetKey();
	for (size_t i = 0; i < GetSearchEnginesCount(); ++i)
	{
		SearchTemplate* st = GetSearchEngine(i);
		if (st->GetKey() == key)
		{
			// we want fallback search to keep its key, so generate new key
			// for the other search (DSK-339031)
			OpString new_key;
			RETURN_IF_ERROR(FindFreeKey(key, st->GetName(), new_key));
			RETURN_IF_ERROR(st->SetKey(new_key));
		}
	}
	return m_search_engines_list.Insert(position, search);
}

/***********************************************************************************
 *
 * TryYandexSearchProtectionException
 *
 * @param search the search that was detected as being hijacked.
 *
 ***********************************************************************************/

bool SearchEngineManager::TryYandexSearchProtectionException(SearchTemplate *search)
{
	OperaVersion search_protection_tied_to_machine_ver;
	RETURN_VALUE_IF_ERROR(search_protection_tied_to_machine_ver.Set(UNI_L("12.13.00")), false);
	if (search && search->GetUrl().Find(UNI_L("yandex")) != KNotFound && g_run_type->m_previous_version < search_protection_tied_to_machine_ver )
	{
		INT32 pos = FindItem(search);
		if (pos != -1)
		{
			if (search->GetUrl().Find(UNI_L("yandex.com.tr")) != KNotFound)
				search->SetURL(UNI_L("http://yandex.com.tr/yandsearch?clid=1669559&text=%s"));
			else if (search->GetUrl().Find(UNI_L("yandex.com")) != KNotFound)
				search->SetURL(UNI_L("http://www.yandex.com/yandsearch?clid=2008212&text=%s"));
			else
				search->SetURL(UNI_L("http://www.yandex.ru/yandsearch?clid=9582&text=%s"));
			ChangeItem(search, pos, SearchEngineManager::FLAG_URL);
			if (OpStatus::IsSuccess(Write()))
				return true;
		}
	}
	return false;
}

/***********************************************************************************
 *
 * CheckProtectedSearches
 *
 * @param copy_of_package_default_search copy of default search from package search.ini
 * @param copy_of_package_speeddial_search copy of speed dial search from package search.ini
 *
 ***********************************************************************************/

OP_STATUS SearchEngineManager::CheckProtectedSearches(OpStackAutoPtr<SearchTemplate>& copy_of_package_default_search,
													  OpStackAutoPtr<SearchTemplate>& copy_of_package_speeddial_search)
{
	SearchTemplate* search = GetDefaultSearch();
	if (SearchProtection::CheckSearch(SearchProtection::DEFAULT_SEARCH, search) == OpBoolean::IS_FALSE)
	{
		// first choice is default search from package, but it can only be used if not modified
		// by user's search.ini
		SearchTemplate* fallback = GetByUniqueGUID(m_package_default_search_guid);

		bool is_fallback_yandex = false;
		if (fallback && !fallback->IsCustomOrModified())
			is_fallback_yandex = fallback->GetUrl().Find(UNI_L("yandex")) != KNotFound;
		else if (copy_of_package_default_search.get())
			is_fallback_yandex = copy_of_package_default_search.get()->GetUrl().Find(UNI_L("yandex")) != KNotFound;

		if (is_fallback_yandex || !TryYandexSearchProtectionException(search))
		{
			if (!fallback || fallback->IsCustomOrModified())
			{
				if (copy_of_package_default_search.get())
				{
					// insert at the beginning of the list (DSK-339031)
					RETURN_IF_ERROR(InsertFallbackSearch(copy_of_package_default_search.get(), 0));
					fallback = copy_of_package_default_search.release();
				}
				else
				{
					fallback = NULL;
				}
			}
			if (fallback && fallback != search)
			{
				RETURN_IF_ERROR(m_fallback_default_search_guid.Set(fallback->GetUniqueGUID()));
				if (search)	// move tampered search to the end of the list (DSK-339031)
				{
					RETURN_IF_ERROR(m_search_engines_list.RemoveByItem(search));
					RETURN_IF_ERROR(m_search_engines_list.Add(search));
				}
			}
		}
	}

	// check/restore procedure for speed dial search is similar to that of default search, but
	// there are more choices for fallback search
	search = GetDefaultSpeedDialSearch();
	if (SearchProtection::CheckSearch(SearchProtection::SPEED_DIAL_SEARCH, search) == OpBoolean::IS_FALSE)
	{
		SearchTemplate* fallback = GetByUniqueGUID(m_package_speeddial_search_guid);

		bool is_fallback_yandex = false;
		if (fallback && !fallback->IsCustomOrModified())
			is_fallback_yandex = fallback->GetUrl().Find(UNI_L("yandex")) != KNotFound;
		else if (copy_of_package_speeddial_search.get())
			is_fallback_yandex = copy_of_package_speeddial_search.get()->GetUrl().Find(UNI_L("yandex")) != KNotFound;
		else if (copy_of_package_default_search.get())
			is_fallback_yandex = copy_of_package_default_search.get()->GetUrl().Find(UNI_L("yandex")) != KNotFound;
		else if (GetDefaultSearch())
			is_fallback_yandex = GetDefaultSearch()->GetUrl().Find(UNI_L("yandex")) != KNotFound;

		if (is_fallback_yandex || !TryYandexSearchProtectionException(search))
		{
			if (!fallback || fallback->IsCustomOrModified())
			{
				if (copy_of_package_speeddial_search.get())
				{
					RETURN_IF_ERROR(InsertFallbackSearch(copy_of_package_speeddial_search.get(), 1));
					fallback = copy_of_package_speeddial_search.release();
				}
				else if (copy_of_package_default_search.get())
				{
					RETURN_IF_ERROR(InsertFallbackSearch(copy_of_package_default_search.get(), 1));
					fallback = copy_of_package_default_search.release();
				}
				else
				{
					fallback = GetDefaultSearch();
				}
			}
			if (fallback && fallback != search)
			{
				RETURN_IF_ERROR(m_fallback_speeddial_search_guid.Set(fallback->GetUniqueGUID()));
				if (search)
				{
					RETURN_IF_ERROR(m_search_engines_list.RemoveByItem(search));
					RETURN_IF_ERROR(m_search_engines_list.Add(search));
				}
			}
		}
	}
	return OpStatus::OK;
}


/***********************************************************************************
 *
 * GetItemByPosition
 *
 * Get Item by position in m_search_engines_list
 *
 ***********************************************************************************/

OpTreeModelItem* SearchEngineManager::GetItemByPosition(INT32 position)
{
	return m_search_engines_list.Get(position);
}


/***********************************************************************************
 *
 * GetColumnData
 *
 *
 ***********************************************************************************/

OP_STATUS SearchEngineManager::GetColumnData(ColumnData* column_data)
{
	switch(column_data->column)
	{
	case 0:
		{
			// name
			return g_languageManager->GetString(Str::SI_IDSTR_SEARCH_HEADER_ENGINE, column_data->text);
		}
	case 1:
		{
			// size
			return g_languageManager->GetString(Str::SI_IDSTR_SEARCH_HEADER_KEYWORD, column_data->text);
		}
	}
	return OpStatus::OK;
}

////////////////////////////////////////////////////////////////////////////////

/***********************************************************************************
 * Write
 *
 * Write user search.ini file to disk. 
 *
 * search->Write skips entries that are from the package and haven't been changed
 * by the user, so this will only write such entries.
 *
 ***********************************************************************************/
OP_STATUS SearchEngineManager::Write()
{
	if (!HasLoadedConfig())
	{
		OP_ASSERT(!"Write() called before LoadSearchesL(), data loss will occur if we continue with Write()");
		return OpStatus::ERR;
	}

	char currsection[64]; /* ARRAY OK 2004-02-02 rikard */
	INT32 section_num = 1, i;

	RETURN_IF_LEAVE(m_user_search_engines_file->DeleteAllSectionsL());

	// We need the version so that any old ones will NEVER overwrite this new one
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(m_user_search_engines_file, "Version", "File Version", m_version));

	// only save it in the user file if it has changed
	if(m_default_search_guid.HasContent() && m_package_default_search_guid.Compare(m_default_search_guid))
	{
		RETURN_IF_ERROR(PrefsUtils::WritePrefsString(m_user_search_engines_file, "Options", "Default Search", m_default_search_guid));
	}
	if(m_package_speeddial_search_guid.Compare(m_default_speeddial_search_guid))
	{
		RETURN_IF_ERROR(PrefsUtils::WritePrefsString(m_user_search_engines_file, "Options", "Speed Dial Search", m_default_speeddial_search_guid));
	}
#ifdef SEARCH_PROVIDER_QUERY_SUPPORT
	if(m_default_error_search_guid.HasContent() && m_package_error_search_guid.Compare(m_default_error_search_guid))
	{
		RETURN_IF_ERROR(PrefsUtils::WritePrefsString(m_user_search_engines_file, "Options", "Error Search", m_default_error_search_guid));
	}
#endif // SEARCH_PROVIDER_QUERY_SUPPORT
	for (i = 0; i < m_search_engines_list.GetCount(CompleteVector); i++)
	{
		SearchTemplate* search = m_search_engines_list.Get(i, CompleteVector);

		if (search)
		{
			op_snprintf(currsection, 64, STD_SECTION, section_num);
			currsection[63] = 0;

			BOOL result = TRUE;
			RETURN_IF_LEAVE(search->WriteL(currsection, m_user_search_engines_file, result));

			// Only increment the section if a section was actually written,
			// which is only user searches. This keeps the searches in the
			// user search.ini in order
			if (result)
				section_num++;
		}
		else
		{
			return OpStatus::ERR;
		}
	}

	RETURN_IF_LEAVE(m_user_search_engines_file->CommitL());

	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS SearchEngineManager::SetDefaultSearch(const OpStringC& search)
{
	// trying to modify searches before LoadSearchesL() ?
	OP_ASSERT(HasLoadedConfig());

	RETURN_IF_ERROR(m_default_search_guid.Set(search.CStr()));

	// disable override for default search
	m_fallback_default_search_guid.Empty();

	SearchTemplate* st = GetDefaultSearch();
	OpStatus::Ignore(SearchProtection::ProtectSearch(SearchProtection::DEFAULT_SEARCH, st));

	// fallback searches are normally not stored in disk files, so if user
	// explicitly selects fallback search it must be added to user's search.ini
	if (st && st->IsCopyFromPackage())
	{
		st->SetIsCustomOrModified();
#ifdef SUPPORT_SYNC_SEARCHES
		BroadcastSearchEngineAdded(st);
#endif
	}

	return OpStatus::OK;
}

OP_STATUS SearchEngineManager::SetDefaultSpeedDialSearch(const OpStringC& search)
{
	// trying to modify searches before LoadSearchesL() ?
	OP_ASSERT(HasLoadedConfig());

	if (search.HasContent())
	{
		RETURN_IF_ERROR(m_default_speeddial_search_guid.Set(search.CStr()));
	}
	else
	{
		m_default_speeddial_search_guid.Empty();
	}

	// disable override for speed dial search
	m_fallback_speeddial_search_guid.Empty();

	SearchTemplate* st = GetDefaultSpeedDialSearch();
	OpStatus::Ignore(SearchProtection::ProtectSearch(SearchProtection::SPEED_DIAL_SEARCH, st));

	// fallback searches are normally not stored in disk files, so if user
	// explicitly selects fallback search it must be added to user's search.ini
	if (st && st->IsCopyFromPackage())
	{
		st->SetIsCustomOrModified();
#ifdef SUPPORT_SYNC_SEARCHES
		BroadcastSearchEngineAdded(st);
#endif
	}

	return OpStatus::OK;
}


/***********************************************************************************
 *
 * GetDefaultSearch
 *
 *
 ***********************************************************************************/

SearchTemplate* SearchEngineManager::GetDefaultSearch()
{
	if (m_fallback_default_search_guid.HasContent())
	{
		SearchTemplate* search = GetByUniqueGUID(m_fallback_default_search_guid);
		OP_ASSERT(search);
		return search;
	}

	// new method where the default is stored in the search.ini
	if (m_default_search_guid.HasContent())
	{
		SearchTemplate* search = GetByUniqueGUID(m_default_search_guid);
		if (search)
			return search;
	}

	if (!GetSearchEnginesCount())
		return NULL;

	// fallback to 1st in list
	SearchTemplate* search = GetSearchEngine(0);

	// And write it to prefs
	if(OpStatus::IsSuccess(m_default_search_guid.Set(search->GetUniqueGUID())))
	{
		Write();
	}

	return search;
}


/***********************************************************************************
 *
 * GetDefaultSpeedDialSearch
 *
 *
 ***********************************************************************************/

SearchTemplate* SearchEngineManager::GetDefaultSpeedDialSearch()
{
	if (m_fallback_speeddial_search_guid.HasContent())
	{
		SearchTemplate* search = GetByUniqueGUID(m_fallback_speeddial_search_guid);
		OP_ASSERT(search);
		return search;
	}

	// new method where the default is stored in the search.ini
	if(m_default_speeddial_search_guid.HasContent())
	{
		SearchTemplate* search = GetByUniqueGUID(m_default_speeddial_search_guid);
		if (search)
			return search;
	}

	// We don't handle old IDs anymore as the setting is gone from prefs
	return NULL;
}

#ifdef SEARCH_PROVIDER_QUERY_SUPPORT
/***********************************************************************************
 *
 * GetDefaultErrorPageSearch
 *
 *
 ***********************************************************************************/

SearchTemplate* SearchEngineManager::GetDefaultErrorPageSearch()
{
	if(m_default_error_search_guid.HasContent())
	{
		if (GetByUniqueGUID(m_default_error_search_guid))
		{
			return GetByUniqueGUID(m_default_error_search_guid);
		}
	}
	// We don't handle old IDs anymore as the setting is gone from prefs
	return NULL;
}
#endif // SEARCH_PROVIDER_QUERY_SUPPORT

//////////////////////////////////////////////////////////////////////////////////////////


/***********************************************************************************
 *
 * AddItem
 *
 * Adds SearchTemplate search to the internal search engines list
 * and to the UserSearches
 *
 ***********************************************************************************/
OP_STATUS SearchEngineManager::AddItem(SearchTemplate* search)
{
	// trying to modify searches before LoadSearchesL() ?
	OP_ASSERT(HasLoadedConfig());

	if (!search->GetUniqueGUID().HasContent())
	{
		OpString guid;
		if (OpStatus::IsSuccess(StringUtils::GenerateClientID(guid)))
		{
			search->SetUniqueGUID(guid.CStr());
			OP_ASSERT(search->GetUniqueGUID().HasContent());
		}
	}

	// Should never add item that's already in the list
	SearchTemplate* temp = g_searchEngineManager->GetByUniqueGUID(search->GetUniqueGUID());
	OP_ASSERT(!temp);

	OP_STATUS status = m_search_engines_list.Add(search);

	if (OpStatus::IsSuccess(status))
	{
		m_usersearches->AddUserSearch(search);
		
#ifdef SUPPORT_SYNC_SEARCHES
		BroadcastSearchEngineAdded(search);
#endif
	}

	return status;
}


/***********************************************************************************
 *
 * RemoveItem
 *
 * @param search - item to 'remove' 
 *
 * The item 'search' is removed from the UserSearches model (visible in the treeviews),
 * and set to invisible in the list of all search engines.
 * 
 * m_from_package is set to FALSE, ensuring that the entry is written to the
 * user search.ini on Write, both if it's a user added search, and if it is a default
 * search.
 *
 ***********************************************************************************/

OP_STATUS SearchEngineManager::RemoveItem(SearchTemplate *search)
{
	BOOL default_search_changed = FALSE;

	// On removing of this item we may need to reset the default search type or default speed dial search type.
	SearchTemplate* search_default = GetDefaultSearch();
	INT32 index = search->GetIndex() == 0 ? 1 : 0;

	if (search_default && search->GetUniqueGUID().Compare(search_default->GetUniqueGUID()) == 0)
	{
		// Get the first user search and set that as the new default
		SearchTemplate *user_search = static_cast<SearchTemplate *>(m_usersearches->GetItemByPosition(index));

		if (user_search)
		{
			OpString guid;

			RETURN_IF_ERROR(user_search->GetUniqueGUID(guid));

			RETURN_IF_ERROR(SetDefaultSearch(guid));

			default_search_changed = TRUE;
		}
	}

	SearchTemplate* sd_default = GetDefaultSpeedDialSearch();
	if (sd_default && search->GetUniqueGUID().Compare(sd_default->GetUniqueGUID()) == 0)
	{
		// Get the first user search and set that as the new default
		SearchTemplate *user_search = static_cast<SearchTemplate *>(m_usersearches->GetItemByPosition(index));

		if (user_search)
		{
			OpString guid;

			RETURN_IF_ERROR(user_search->GetUniqueGUID(guid));

			RETURN_IF_ERROR(SetDefaultSpeedDialSearch(guid));

			default_search_changed = TRUE;
		}
	}

	if (default_search_changed)
	{
		g_application->SettingsChanged(SETTINGS_SEARCH);
	}
	// Remove the search
	m_search_engines_list.SetVisibility(m_search_engines_list.Find(search, CompleteVector), FALSE);

	m_usersearches->RemoveUserSearch(search);

#ifdef SUPPORT_SYNC_SEARCHES
	if (search->IsCopyFromPackage())
	{
		// nothing to do - fallback search that was not modified was never synced
	}
	else if (search->IsFromPackageOrModified())
	{
		// If this is a search from the package and it hasn't been changed previously, send an add
		if (search->IsFromPackage())
		{
			BroadcastSearchEngineAdded(search);
		}
		// If this is a search from the package and it has been changed previously, send modified
		else
		{
			BroadcastSearchEngineChanged(search, SearchEngineManager::FLAG_DELETED);
		}
	}
	else // Else removal of custom search engine, send a removed notification
	{
		BroadcastSearchEngineRemoved(search);
	}
#endif

	search->OnRemoving();

	if (!search->IsCopyFromPackage()) // if it is a copy it will just disappear
	{
		search->SetIsCustomOrModified();
	}

	if (g_search_field_history->RemoveEntries(search->GetUniqueGUID()))
	{
		g_search_field_history->Write();
	}

	return OpStatus::OK;
}


/***********************************************************************************
 *
 * ChangeItem
 *
 * Sets item to be a user search since it changed. Broadcasts change to listeners.
 *
 ***********************************************************************************/
void SearchEngineManager::ChangeItem(SearchTemplate *search, INT32 pos, UINT32 changed_flag)
{
	// If this is a first change of fallback search
	if (search->IsCopyFromPackage())
	{
		search->SetIsCustomOrModified();
#ifdef SUPPORT_SYNC_SEARCHES
		BroadcastSearchEngineAdded(search);
#endif
		// when modified fallback default search becomes regular default search
		const OpStringC& guid = search->GetUniqueGUID();
		if (guid == m_fallback_default_search_guid)
		{
			m_fallback_default_search_guid.Empty();
			m_default_search_guid.Set(guid);
		}
		if (guid == m_fallback_speeddial_search_guid)
		{
			m_fallback_speeddial_search_guid.Empty();
			m_default_speeddial_search_guid.Set(guid);
		}
	}
	// If this is a first change on a default search engine
	else if (search->IsFromPackage())
	{
		search->SetIsCustomOrModified();
#ifdef SUPPORT_SYNC_SEARCHES
		BroadcastSearchEngineAdded(search);
#endif
	}
	else // not first change on default search, or this is a change on a custom search
	{
		OP_ASSERT(search->IsCustomOrModified());
#ifdef SUPPORT_SYNC_SEARCHES
		BroadcastSearchEngineChanged(search, changed_flag);
#endif
	}

	BroadcastItemChanged(pos);

	search->Change();

	SearchTemplate* default_search = GetDefaultSearch();
	if (default_search == search)
	{
		OpStatus::Ignore(SearchProtection::ProtectSearch(SearchProtection::DEFAULT_SEARCH, search));
	}
	SearchTemplate* speeddial_search = GetDefaultSpeedDialSearch();
	if (speeddial_search == search)
	{
		OpStatus::Ignore(SearchProtection::ProtectSearch(SearchProtection::SPEED_DIAL_SEARCH, search));
	}
}

/**********************************************************************************/

BOOL SearchEngineManager::IsDetectedIntranetHost(const uni_char *text)
{
	OpStringC url_list = g_pcui->GetStringPref(PrefsCollectionUI::IntranetHosts);
	const uni_char *tmp = url_list;
	while (tmp)
	{
		const uni_char *start = tmp;
		tmp = uni_strpbrk(tmp, UNI_L(";, \t"));
		unsigned len = tmp ? tmp - start : uni_strlen(start);
		if (len > 0 && uni_strncmp(start, text, len) == 0 && text[len] == 0)
			return TRUE;
		if (tmp)
			tmp++; // Exclude the delimiter
	}
	return FALSE;
}

void SearchEngineManager::AddDetectedIntranetHost(const uni_char *text)
{
	OpString url_list;
	if (OpStatus::IsSuccess(url_list.Set(g_pcui->GetStringPref(PrefsCollectionUI::IntranetHosts)) &&
		url_list.Find(text) == KNotFound))
	{
		if (url_list.Length())
			url_list.Append(UNI_L(","));
		url_list.Append(text);
		OP_STATUS status;
		TRAP(status, g_pcui->WriteStringL(PrefsCollectionUI::IntranetHosts, url_list));
	}
}

/***********************************************************************************
 * IsIntranet
 ***********************************************************************************/

BOOL SearchEngineManager::IsIntranet(const uni_char *text)
{
	if (IsUrl(text) || !IsSingleWordSearch(text) || uni_strlen(text) == 0)
		return FALSE;

	if (uni_isdigit(text[0]) || text[0] == '?')
		return FALSE;

	return TRUE;
}

/***********************************************************************************
 * IsUrl
 ***********************************************************************************/
BOOL SearchEngineManager::IsUrl(const uni_char *text)
{
	OpString resolved;
	TRAPD(status,g_url_api->ResolveUrlNameL(text, resolved, TRUE));
	if (OpStatus::IsError(status))
		return FALSE;
	URL url = g_url_api->GetURL(resolved);
	if (DOM_Utils::IsOperaIllegalURL(url))
		return FALSE;

	int length = uni_strlen(text);
	int dot_index = -1;
	int num_space = 0, first_space_index = -1;
	int num_dots = 0;
	int first_slash_index = -1;
	BOOL has_colon = FALSE;
	for( int i = 0; i < length; i++ )
	{
		if(text[i] == '/' || text[i] == '\\')
		{
			if (first_slash_index == -1)
				first_slash_index = i;
		}
		else if (text[i] == ':' && num_space == 0)
		{
			has_colon = TRUE;
		}
		else if( text[i] == ' ' && i != 0 && i != length - 1 && !has_colon)
		{
			num_space++;
			if (first_space_index == -1)
				first_space_index = i;
		}
		else if (IDNA::IsIDNAFullStop(text[i]))
		{
			num_dots++;
			dot_index = i;
		}
	}
	if (num_space)
	{
		// Check if the part up to the first space looks like a url.
		// If that is the case we will treat it like a url.
		OpString tmp;
		if (OpStatus::IsSuccess(tmp.Set(text, first_space_index)))
			return IsUrl(tmp.CStr());
		return FALSE;
	}
	if (has_colon)
		return TRUE;
	if (first_slash_index != -1)
	{
		// If a slash is preceded by only digits (no dots) it is technically a valid url but we won't accept it here.
		if (first_slash_index == 0 || uni_strspn(text, UNI_L("0123456789")) != static_cast<size_t>(first_slash_index))
			return TRUE;
	}
	if (dot_index != -1)
	{
		if (dot_index <= length - 3)
			return TRUE;
		// Or if there is only one character after the dot, check if it's a digit (to detect f.ex 127.0.0.1)
		else if (dot_index < length - 1 && num_dots == 3 && uni_isdigit(text[length-1]))
			return TRUE;
	}
	// Check if there are some special cases stored in prefs, like previosly typed intranets
	return IsDetectedIntranetHost(text);
}

/***********************************************************************************
 * IsSingleWordSearch
 ***********************************************************************************/
BOOL SearchEngineManager::IsSingleWordSearch(const uni_char *text)
{
	int length = uni_strlen(text);
	for( int i = 0; i < length; i++ )
	{
		if( text[i] == ' ')
			return FALSE;
	}
	return !IsUrl(text);
}


BOOL SearchEngineManager::CheckAndDoSearch(SearchSetting& settings, BOOL do_search)
{
	if (settings.m_keyword.IsEmpty())
		return FALSE;

	OpString key, rest;
	if (!key.Reserve(32) || !rest.Reserve(2))
		return FALSE;

	SplitKeyValue(settings.m_keyword, key, rest);

	if (key.HasContent())
	{
		// Can not search with empty argument
		if (rest.IsEmpty())
			return FALSE;

		SearchTemplate* search_template = g_searchEngineManager->GetSearchEngineByKey(key);
		if (search_template)
		{
			if (do_search)
			{
				OpString keyword;
				if (OpStatus::IsError(keyword.Set(settings.m_keyword)))
					return FALSE;

				// Modify settings with the new values derived from keyword
				settings.m_search_template = search_template;
				if (OpStatus::IsError(settings.m_keyword.Set(rest)))
					return FALSE;

				DoSearch(settings);

				if (OpStatus::IsError(settings.m_keyword.Set(keyword)))
					return FALSE;

#ifdef ENABLE_USAGE_REPORT
				if(g_usage_report_manager && g_usage_report_manager->GetSearchReport())
				{
					g_usage_report_manager->GetSearchReport()->AddSearch(SearchReport::SearchUnknown, search_template->GetUniqueGUID());
				}
#endif
			}
			return TRUE;
		}
	}

	// The rest of the function ends up in a recursive call which can only
	// be called once. Should not be a problem but stay safe.

	static BOOL busy = FALSE;
	if (busy)
		return FALSE;

	busy = TRUE;

	// Search with default search engine if we
	// 1) have no searchkey
	// 2) have one or more search terms
	// 3) it doesn't look like a url
	// 4) Search terms do not match a nickname (bug #259338)
	// [espen 2006-10-20]

	BOOL search_with_default = TRUE;

	if (IsUrl(settings.m_keyword.CStr()))
		search_with_default = FALSE;
	else if (g_hotlist_manager->HasNickname(settings.m_keyword.CStr(), 0))
		search_with_default = FALSE;


	BOOL success = FALSE;

	if (search_with_default && rest.HasContent())
	{
		// Search with the default search engine
		SearchTemplate* search_template = GetDefaultSearch();
		if (search_template && search_template->GetKey().HasContent())
		{
			// Modify settings with the new values derived from template and rest part of keyword
			settings.m_search_template = search_template;
			settings.m_keyword.Empty();
			if (OpStatus::IsSuccess(settings.m_keyword.AppendFormat(UNI_L("%s %s"), search_template->GetKey().CStr(), rest.CStr())))
			{
				success = CheckAndDoSearch(settings, do_search);
#ifdef ENABLE_USAGE_REPORT
				if (do_search && g_usage_report_manager && g_usage_report_manager->GetSearchReport())
				{
					g_usage_report_manager->GetSearchReport()->SetWasDefaultSearch(TRUE);
				}
#endif // ENABLE_USAGE_REPORT
			}
		}
	}

	busy = FALSE;

	return success;
}



/***********************************************************************************
 * DoHistorySearch
 *
 *
 *
 ***********************************************************************************/
BOOL SearchEngineManager::DoHistorySearch(const OpStringC& search_terms)
{
	OpInputAction action(OpInputAction::ACTION_MANAGE);
	action.SetActionDataString(UNI_L("history"));
	action.SetActionDataStringParameter(search_terms.CStr());
	g_input_manager->InvokeAction(&action);

	return TRUE;
}


/***********************************************************************************
 *
 * DoInlineFind
 *
 ***********************************************************************************/

BOOL SearchEngineManager::DoInlineFind(const OpStringC& search_terms)
{
	g_input_manager->InvokeAction(OpInputAction::ACTION_FIND_INLINE, 0, search_terms.CStr());
	return TRUE;
}


/***********************************************************************************
 *
 * MakeSearchWithString
 *
 * @param int index     - index of search engine to use
 * @param OpString% buf - 
 *
 * Looks up the description of the search based on its index in the search engine
 * list and gets the string that represents it.
 *
 ***********************************************************************************/

void SearchEngineManager::MakeSearchWithString(int index, OpString& buf)
{
	if ((index < 0) || (index >= (int) GetSearchEnginesCount()))
	{
		index = 0;
	}

	SearchTemplate* search_template = GetSearchEngine(index);

	MakeSearchWithString(search_template, buf);
}

void SearchEngineManager::MakeSearchWithString(SearchTemplate* search, OpString& buf)
{
	if(!search)
	{
		return;
	}

	OpString format_str;
	g_languageManager->GetString(search->GetFormatId(), format_str);

	OpString engine_name;
	RETURN_VOID_IF_ERROR(search->GetEngineName(engine_name));

	if (format_str.IsEmpty())
	{
		buf.TakeOver(engine_name);
	}
	else
	{
		buf.Empty();
		buf.AppendFormat(format_str.CStr(), engine_name.CStr());
	}
}


/***********************************************************************************
 *
 * AddSearchStringToList
 *
 * This function is meant to add a search entry at the bottom of the auto complete
 * list we get when preparing the autocomplete dropdown list.
 *
 * The list is assumed to contain three strings per item.
 *
 * This is ONLY used for OpEdit, the Address drop down is a OpAddressDropDown
 * and uses different functionallity which was added for peregrine.
 *
 ***********************************************************************************/

void SearchEngineManager::AddSearchStringToList(const uni_char* search_key,
												uni_char**& list,
												int& num_items)
{
	// If the string doesn't start with www and there is a default search engine :
	BOOL add_search =
		uni_stristr(search_key, UNI_L("www")) != search_key &&
		GetDefaultSearch();

	if (add_search)
	{
		// Make an array that contains one more item :
		uni_char** tmp = OP_NEWA(uni_char*, (num_items+1) * 3);
		if (!tmp)
			return;

		// Copy the current array into the new one :
		int i;
		for (i=0; i<num_items*3; i++)
		{
			tmp[i] = list[i];
		}

		// Replace the current one with the new one :
		OP_DELETEA(list);
		list = tmp;

		// Update the number of items :
	    num_items += 1;

		OpString key;
		OpString value;
		OpString input;

		input.Set(search_key);

		// Split up the string into key value :
		SplitKeyValue(input, key, value);

		OpString search_label;
		OpString search_string;

		// Lookup the search :
		FindSearchByKey(key, value, search_label, search_string);

		// Copy and insert the search string :
		uni_char* tmpstr = OP_NEWA(uni_char, search_string.Length()+1);
		if (tmpstr)
			uni_strcpy(tmpstr, search_string.CStr());
		list[i] = tmpstr;

		// Copy and insert the search label :
		tmpstr = OP_NEWA(uni_char, search_label.Length()+1);
		if (tmpstr)
		{
			if (search_label.CStr())
				uni_strcpy(tmpstr, search_label.CStr());
			else
				*tmpstr = 0;
		}

		list[i+1] = tmpstr;

		// Copy and insert the search string - again (God knows why) :
		tmpstr = OP_NEWA(uni_char, search_string.Length()+1);
		if (tmpstr)
			uni_strcpy(tmpstr, search_string.CStr());
		list[i+2] = tmpstr;
	}
}


/***********************************************************************************
 *
 * SplitKeyValue
 *
 * Splits a address field entry into its separate key and "the rest" (i.e. should
 * be the url) parts
 *
 ***********************************************************************************/
void SearchEngineManager::SplitKeyValue(const OpStringC& input,
										OpString & key,
										OpString & value)
{
	// Clear the strings :
	key.Empty();
	value.Empty();

	// Make a copy of the input and strip leading spaces :
	OpString local_copy;
	local_copy.Set(input.CStr());
	local_copy.Strip(TRUE, FALSE);

	// Find the position of the space if there is one :
	int space_pos = local_copy.FindFirstOf(' ');
	BOOL key_found = FALSE;

	if (space_pos != KNotFound)
	{
		// Check whether it is a real key :

		OpString possible_key;
		possible_key.Set(local_copy.CStr(), space_pos);

		SearchTemplate* search = g_searchEngineManager->GetSearchEngineByKey(possible_key);
		if (search)
		{
			// Split the string on the space
			key.Set(possible_key.CStr());
			value.Set(local_copy.SubString(space_pos));
			key_found = TRUE;
		}
	}

	if(!key_found)
	{
		// Leave key empty
		value.Set(local_copy.CStr());
	}

	// Strip spaces :
	value.Strip();
}


/***********************************************************************************
 *
 * IsSearchString
 *
 ***********************************************************************************/

BOOL SearchEngineManager::IsSearchString(const OpStringC& input)
{
	OpString key;
	OpString value;
	SplitKeyValue(input, key, value);
	SearchTemplate* search_template = GetSearchEngineByKey(key);

	return search_template ? TRUE : FALSE;
}


/***********************************************************************************
 *
 * FindSearchByKey
 *
 * Takes what is the verified key and value (i.e) everything except the key) and
 * returns a description for the search (i.e. search_label = "Google") and
 * an internal search url string (i.e. search_string = "g hello world").
 * This is needed to ensure that the default search key is added to the internal
 * search url if no valid key was entered.
 *
 ***********************************************************************************/

BOOL SearchEngineManager::FindSearchByKey(const OpStringC & key,
										  const OpStringC & value,
										  OpString & search_label,
										  OpString & search_string)
{
	BOOL return_val = FALSE;

	SearchTemplate* search = GetSearchEngineByKey(key);
	if (search)
	{
		search_string.Set(key.CStr());
		search_string.Append(" ");
		MakeSearchWithString(search, search_label);
	}
	else
	{
		// Make the search string and label from the default search engine since the user
		// didn't type (a valid) key :
		SearchTemplate *default_search = GetDefaultSearch();
		if (default_search)
			search_string.Set(default_search->GetKey()); // Default search is always the special key
	
		search_string.Append(" ");

		// The key wasn't a key after all - put it back in :
		if(key.HasContent())
		{
			search_string.Append(key.CStr());
			search_string.Append(" ");
		}

		MakeSearchWithString(default_search, search_label);
		return_val = TRUE;
	}

	search_string.Append(value.CStr());
	return return_val;
}

/*****************************************************************************
 *
 * KeyInUse
 *
 * @param key - the key to check if is already in use in searchmodel
 * @param exclude_search - exclude this search from the search
 *
 * @return TRUE if the key is used by an item in the search model,
 *           excluding item exclude_search
 *
 *****************************************************************************/

BOOL SearchEngineManager::KeyInUse(const OpStringC& key, SearchTemplate* exclude_search)
{
	for(unsigned int i = 0; i < g_searchEngineManager->GetSearchEnginesCount(); i++)
	{
		SearchTemplate* search = g_searchEngineManager->GetSearchEngine(i);

		if (search != exclude_search && !search->GetKey().IsEmpty() && search->GetKey().CompareI(key) == 0)
		{
			return TRUE;
		}
	}
	return FALSE;
}


namespace
{
/**
 * Gets all ascii letters from input string that are not in output
 * string and appends them to output string.
 *
 * @param out output string
 * @param in intput string
 *
 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY
 *
 */
inline OP_STATUS add_ascii_letters(OpString& out, OpStringC in)
{
	if (in.HasContent())
	{
		for (const uni_char* p = in.CStr(); *p; ++p)
		{
			if (*p < 128 && op_isalpha(*p))
			{
				uni_char c = op_tolower(*p);
				if (out.FindFirstOf(c) == KNotFound)
					RETURN_IF_ERROR(out.Append(&c, 1));
			}
		}
	}
	return OpStatus::OK;
}
} // namespace

/*****************************************************************************
 *
 * FindFreeKey
 *
 * @param ref_key optional reference key used to generate new key
 * @param ref_name optional reference name used to generate new key
 * @param new_key gets new key on success
 *
 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY
 *
 *****************************************************************************/
OP_STATUS SearchEngineManager::FindFreeKey(OpStringC ref_key, OpStringC ref_name, OpString& new_key)
{
	OpString in_candidates; // ascii letters from ref_key and ref_name - higher priority than other letters
	RETURN_IF_ERROR(add_ascii_letters(in_candidates, ref_key));
	RETURN_IF_ERROR(add_ascii_letters(in_candidates, ref_name));

	OpString candidates;
	RETURN_IF_ERROR(candidates.Set(in_candidates));
	const int max_candidates = 'z' - 'a' + 1;
	RETURN_OOM_IF_NULL(candidates.Reserve(max_candidates));
	for (int i = 0; i <= max_candidates; ++i)
	{
		uni_char c = i + 'a';
		if (in_candidates.FindFirstOf(c) == KNotFound)
			RETURN_IF_ERROR(candidates.Append(&c, 1));
	}
	candidates[max_candidates] = 0;

	// letter+number will be used if all candidates are already taken
	uni_char key_with_number = candidates[0];
	unsigned int free_number = 1;

	for(size_t i = 0; i < GetSearchEnginesCount(); i++)
	{
		OpStringC key = GetSearchEngine(i)->GetKey();
		if (key.HasContent())
		{
			if (key[1] == 0)
			{
				int index = candidates.FindFirstOf(key[0]);
				if (index != KNotFound)
				{
					candidates.Delete(index, 1);
				}
			}
			else if (key[0] == key_with_number)
			{
				const uni_char* p = key.CStr() + 1;
				while (op_isdigit(*p)) ++p;
				if (*p == 0 && p > key.CStr() + 1)
				{
					unsigned int nr = static_cast<unsigned int>(uni_atoi(key.CStr() + 1));
					if (nr >= free_number)
						free_number = nr + 1;
				}
			}
		}
	}

	if (candidates.HasContent())
	{
		return new_key.Set(candidates, 1);
	}
	else
	{
		new_key.Empty();
		return new_key.AppendFormat("%c%u", key_with_number, free_number);
	}
}


/***************************************************************************
 *
 * GetSearchEngineByKey
 *
 *
 * Note: Goes backwards though the list so user searches, are prefered before package, 
 * a cheap fix which should work most of the time :) 
 * This is only a problem for upgrades from the old system to the 
 * new system anyway, otherwise you can't have duplicate keys
 *
 * // Used by OpAddressDropDown
 *
 ***************************************************************************/

SearchTemplate* SearchEngineManager::GetSearchEngineByKey(const OpStringC& key)
{
	if (key.HasContent())
	{
		SearchTemplate *default_search = GetDefaultSearch();
			

		// Get the default seach engine if the special key is used
		if (default_search && !key.Compare(default_search->GetKey()) ||
			!key.Compare(DEFAULT_SEARCH_KEY))
		{
			return g_searchEngineManager->GetDefaultSearch();
		}
		else
		{
			for (int i = GetSearchEnginesCount() - 1; i >= 0; i--)
			{
				SearchTemplate* search_template = GetSearchEngine(i);
				
				if (search_template->GetKey().CompareI(key) == 0)
				{
					return search_template;
				}
			}
		}
	}

	return NULL;
}



/***********************************************************************************
 *
 * PrepareSearch
 *
 *
 ***********************************************************************************/

BOOL SearchEngineManager::PrepareSearch(URL& url_object,
										const uni_char* keyword,
										BOOL resolve_keyword,
										int id,
										SearchReqIssuer search_req_issuer,
										const OpStringC& key,
										URL_CONTEXT_ID context_id)
{
	SearchTemplate* search_template = SearchFromID(id);

	if (!search_template)
		return FALSE;

	return search_template->MakeSearchURL(url_object, 
										  keyword, 
										  resolve_keyword,
										  search_req_issuer,
										  key,
										  context_id);
}


BOOL SearchEngineManager::DoSearch(SearchSetting& settings)
{
	if (settings.m_keyword.IsEmpty() || !settings.m_search_template)
		return FALSE;

	// Update search engine icon on first search in a session
	if (!settings.m_search_template->GetSearchUsed())
	{
		// FIXME: Temporary fix for bug #DSK-267730 (Too many favicon requests sent to redir.opera.com)
		// g_favicon_manager->LoadSearchIcon(settings.m_search_template->GetUniqueGUID());
		settings.m_search_template->SetSearchUsed(TRUE);
	}
	URL_CONTEXT_ID context = 0;
	if ((settings.m_target_window && settings.m_target_window->PrivacyMode()) ||
		(g_application->GetActiveBrowserDesktopWindow() && g_application->GetActiveBrowserDesktopWindow()->PrivacyMode()))
		context = g_windowManager->GetPrivacyModeContextId();

	URL url;
	PrepareSearch(url, settings.m_keyword.CStr(), FALSE, settings.m_search_template->GetID(), settings.m_search_issuer, UNI_L(""), context);
	if (url.IsValid())
	{
		// We want to use the current tab if it is empty and the user has not specified destination
		DocumentDesktopWindow* active_document_desktop_window = g_application->GetActiveDocumentDesktopWindow();
		if (active_document_desktop_window && !active_document_desktop_window->HasDocument())
		{
			if (settings.m_new_window == MAYBE)
				settings.m_new_window = NO;
			if (settings.m_new_page == MAYBE)
				settings.m_new_page = NO;
			if (settings.m_in_background == MAYBE)
				settings.m_in_background = NO;
		}

		settings.m_url = url;
		return g_application->OpenURL(settings);
	}

	if (settings.m_search_template->GetSearchType() == SEARCH_TYPE_HISTORY)
		return DoHistorySearch(settings.m_keyword);

	if (settings.m_search_template->GetSearchType() == SEARCH_TYPE_INPAGE)
		return DoInlineFind(settings.m_keyword);

    return FALSE;
 }


/***********************************************************************************
 *
 * GetGUIDByOldID
 *
 * Converts old ID to GUID based on mapping in translations module
 * If 0 is returned, generating a new GUID should be safe.
 *
 ***********************************************************************************/
const char* SearchEngineManager::GetGUIDByOldId(int id)
{
	for (int i = 0; i < num_search_id_mappings; i++) // rid of magic no 
	{
		if (g_search_id_mapping[i].old_id == id)
		{
			return g_search_id_mapping[i].new_id;
		}
	}
	return 0;
}


/***********************************************************************************
 *
 * SearchTypeToID
 *
 * (Used: OpBrowserView::HandleHotclickActions)
 *
 ***********************************************************************************/

int SearchEngineManager::SearchTypeToID(SearchType type)
{
	unsigned int i;

	for (i = 0; i < GetSearchEnginesCount(); i++)
	{
		SearchTemplate *search = GetSearchEngine(i);
		if(search->GetSearchType() == type)
		{
			return search->GetID();
		}
	}
	return 0;
}

int SearchEngineManager::SearchTypeToIndex(SearchType type)
{
	int index = 0;
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
	for (unsigned int i = 0; i < g_searchEngineManager->GetSearchEnginesCount(); i++)
	{
		if (g_searchEngineManager->GetSearchEngine(i)->GetSearchType() == type)
		{
			index = i;
			break;
		}
	}
#endif // DESKTOP_UTIL_SEARCH_ENGINES
	return index;
}


/***********************************************************************************
 *
 * SearchIndexToID
 *
 * (Used in SearchDropDown and SearchEdit)
 *
 ***********************************************************************************/
int SearchEngineManager::SearchIndexToID(int index)
{
	if (index >= 0 && index < (int) GetSearchEnginesCount())
	{
		SearchTemplate *search = GetSearchEngine(index);
		if(search)
		{
			return search->GetID();
		}
	}
	return 0;
}


/***********************************************************************************
 *
 * SearchIDToIndex
 *
 * (Used in Application.cpp (actions))
 *
 ***********************************************************************************/
int SearchEngineManager::SearchIDToIndex(int id)
{
	unsigned int i;

	for (i = 0; i < GetSearchEnginesCount(); i++)
	{
		SearchTemplate* search = GetSearchEngine(i);

		if(search)
		{
			if (id == search->GetID())
			{
				return i;
			}
		}
	}
	return 0;
}

/***********************************************************************************
 *
 * IDToSearchType
 *
 *
 ***********************************************************************************/

SearchType SearchEngineManager::IDToSearchType(int id)
{
	unsigned int i;

	for (i = 0; i < GetSearchEnginesCount(); i++)
	{
		SearchTemplate* search = GetSearchEngine(i);

		if (search)
		{
			if (id == search->GetID())
			{
				return search->GetSearchType();
			}
		}
	}
	return (SearchType)0;
}


/***********************************************************************************
 *
 * SearchFromID
 *
 *
 ***********************************************************************************/

SearchTemplate* SearchEngineManager::SearchFromID(int id)
{
	unsigned int i;

	for (i = 0; i < GetSearchEnginesCount(); i++)
	{
		SearchTemplate* search = GetSearchEngine(i);

		if (search)
		{
			if (id == search->GetID())
			{
				return search;
			}
		}
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////////////////////

// Sorry about this one, needed as long as the indexes in the search list
// is sent around in quick, should go away ...
int SearchEngineManager::GetSearchIndex(SearchTemplate* search)
{
	for (unsigned int i = 0; i < GetSearchEnginesCount(); i++)
	{
		if (search == GetSearchEngine(i))
			return i;

	}
	return -1;
}


/*************************************************************************
 *
 * GetByUniqueGUID
 *
 *
 *************************************************************************/

SearchTemplate* SearchEngineManager::GetByUniqueGUID(const OpStringC& guid)
{
	for (UINT32 i = 0; i < GetSearchEnginesCount(); i++)
	{
		SearchTemplate* item = GetSearchEngine(i);

		if (item && item->GetUniqueGUID().Compare(guid) == 0)
		{
			return item;
		}
	}
	
	return 0;
}

/*************************************************************************
 *
 * UpdateGoogleTLD
 *
 *************************************************************************/

OP_STATUS SearchEngineManager::UpdateGoogleTLD()
{
	BOOL has_google_tld = FALSE;
	
	// There is no need to update anything if there is no TLD searchs in what is currently loaded
	for (UINT32 i = 0; i < GetSearchEnginesCount(); i++)
	{
		SearchTemplate* item = GetSearchEngine(i);

		// Check if this is a good TLD search
		if (item && item->GetUseTLD())
		{
			has_google_tld = TRUE;
			break;
		}
	}
	
	// Just return ok it there is no need to update the setting
	if (!has_google_tld)
		return OpStatus::OK;

	// Now make the object if it doesn't exist
	if (!m_tld_update)
	{
		// Start the attempted download of the tld file
		m_tld_update = OP_NEW(TLDUpdater, ());
		if (!m_tld_update)
			return OpStatus::ERR_NO_MEMORY;
	}

	return m_tld_update->StartDownload();
}

// --------------------------------------------------------------------------------
//
// class UserSearches
//
// --------------------------------------------------------------------------------

/***********************************************************************************
 *
 * GetColumnData
 *
 *
 ***********************************************************************************/

OP_STATUS UserSearches::GetColumnData(ColumnData* column_data)
{

	switch(column_data->column)
	{
	    case 1:
		{
			// name
			return g_languageManager->GetString(Str::SI_IDSTR_SEARCH_HEADER_ENGINE, column_data->text);
		}
	    case 2:
		{
			// size
			return g_languageManager->GetString(Str::SI_IDSTR_SEARCH_HEADER_KEYWORD, column_data->text);
		}
	}
	return OpStatus::OK;
}


/***********************************************************************************
 *
 * AddUserSearch
 *
 * Ownership will NOT be transferred to UserSearches
 *
 ***********************************************************************************/
OP_STATUS UserSearches::AddUserSearch(SearchTemplate* searchtemplate)
{
	OP_ASSERT(searchtemplate->GetUniqueGUID().HasContent());

	/*INT32 index = */ AddLast(searchtemplate);

	return OpStatus::OK;
}


/***********************************************************************************
 *
 * RemoveUserSearch
 *
 *
 ***********************************************************************************/

OP_STATUS UserSearches::RemoveUserSearch(SearchTemplate* searchtemplate)
{
	if (searchtemplate)
		searchtemplate->Remove();

	return OpStatus::OK;
}



#endif // DESKTOP_UTIL_SEARCH_ENGINES
