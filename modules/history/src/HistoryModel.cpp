/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * psmaas - Patricia Aas
 */
#include "core/pch.h"

#ifdef HISTORY_SUPPORT

#include "modules/debug/debug.h"
#include "modules/unicode/unicode.h"
#include "modules/history/src/HistoryModel.h"
#include "modules/history/OpHistoryPage.h"
#include "modules/history/OpHistoryTimeFolder.h"
#include "modules/history/OpHistorySiteFolder.h"
#include "modules/history/OpHistoryPrefixFolder.h"
#include "modules/history/merge_sort.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/util/winutils.h"
#include "modules/url/url2.h"

#if defined(URL_USE_HISTORY_VLINKS) && defined(DISK_CACHE_SUPPORT)
#include "modules/cache/cache_int.h"
#include "modules/url/url_man.h"
#include "modules/url/url_rep.h"
#include "modules/formats/url_dfr.h"
#endif // URL_USE_HISTORY_VLINKS && DISK_CACHE_SUPPORT

#ifdef VISITED_PAGES_SEARCH
#include "modules/search_engine/VisitedSearch.h"
#endif //VISITED_PAGES_SEARCH

/*___________________________________________________________________________*/
/*                         HISTORY MODEL                                     */
/*___________________________________________________________________________*/

/***********************************************************************************
 ** Creates a history model with m_max set to m (positive integer)
 **
 **
 ** HistoryModel - Constructor
 **
 ** @param number of items to be stored
 ***********************************************************************************/
HistoryModel::HistoryModel(INT32 m,
						   OpVector<HistoryPage> *    modelVector,
						   OpVector<HistoryPage> *    bookmarkVector,
						   OpVector<HistoryPage> *    operaPageVector,
						   OpVector<HistoryPage> *    nickPageVector,
						   HistoryTimeFolder     **   timeFolderArray,
						   HistoryPrefixFolder   **   protocol_array,
						   OpSplayTree<HistoryItem, HistoryPage> * strippedTree,
						   OpSimpleSplayTree<PrefixMatchNode> * prefix_tree,
						   OpTimer               *    midnight_timer,
						   HistoryTimePeriod     *    history_time_periods,
						   HistoryPrefix         *    history_prefixes,
						   HistoryInternalPage   *    history_internal_pages,
						   int                        size_history_time_periods,
						   int                        size_history_prefixes,
						   int                        size_history_internal_pages)
	: m_listener(0)
	  , m_CurrentIndex(0)
	  , m_max(m)
	  , m_model_alive(TRUE)
	  , m_modelVector(modelVector)
	  , m_bookmarkVector(bookmarkVector)
	  , m_operaPageVector(operaPageVector)
	  , m_nickPageVector(nickPageVector)
	  , m_timeFolderArray(timeFolderArray)
	  , m_protocol_array(protocol_array)
	  , m_strippedTree(strippedTree)
	  , m_prefix_tree(prefix_tree)
	  , m_midnight_timer_on(FALSE)
	  , m_midnight_timer(midnight_timer)
	  , m_history_time_periods(history_time_periods)
	  , m_history_prefixes(history_prefixes)
	  , m_history_internal_pages(history_internal_pages)
	  , m_size_history_time_periods(size_history_time_periods)
	  , m_size_history_prefixes(size_history_prefixes)
	  , m_size_history_internal_pages(size_history_internal_pages)
{
}

/***********************************************************************************
 ** Create will attempt to allocate all the memory that the history model needs and
 ** initialize those datastructures. If this is successful Create will call the
 ** HistoryModel constructor with pointers to those data structures. If not
 ** successful - an out of memory has occured and Create will clean up any memory
 ** allocated and return 0.
 **
 ** @param num - the maximum number of history items
 **
 ** @return a pointer to the history model
 ***********************************************************************************/
HistoryModel*  HistoryModel::Create(INT32 num)
{
	// The main search tree :
	OpSplayTree<HistoryItem, HistoryPage> * stripped_tree = 0;

	// The prefix search tree and the protocol array containing all the prefix folders:
	OpSimpleSplayTree<PrefixMatchNode> * prefix_tree = 0;
	HistoryPrefixFolder ** protocol_array = 0;

	// The timer :
	OpTimer * midnight_timer = NULL;

	// The time folders :
	HistoryTimeFolder    ** timeFolderArray = 0;

	// The model vectors :
	OpVector<HistoryPage>*  modelVector     = 0;
	OpVector<HistoryPage>*  bookmarkVector  = 0;
	OpVector<HistoryPage>*  operaPageVector = 0;
	OpVector<HistoryPage>*  nickPageVector  = 0;

	// The tables :
	HistoryTimePeriod   * history_time_periods   = 0;
	HistoryPrefix       * history_prefixes       = 0;
	HistoryInternalPage * history_internal_pages = 0;
	int size_history_time_periods   = 0;
	int size_history_prefixes       = 0;
	int size_history_internal_pages = 0;

	do
	{
		if(OpStatus::IsMemoryError(MakeHistoryTimePeriods(history_time_periods, size_history_time_periods)))
			break;

		if(OpStatus::IsMemoryError(MakeHistoryPrefixes(history_prefixes, size_history_prefixes)))
			break;

		if(OpStatus::IsMemoryError(MakeHistoryInternalPages(history_internal_pages, size_history_internal_pages)))
			break;

		if(OpStatus::IsMemoryError(initPrefixes(&prefix_tree, &protocol_array, history_prefixes, size_history_prefixes)))
			break;

		if(OpStatus::IsMemoryError(initFolderArray(&timeFolderArray, history_time_periods)))
			break;

		modelVector     = OP_NEW(OpVector<HistoryPage>, ());
		bookmarkVector  = OP_NEW(OpVector<HistoryPage>, ());
		operaPageVector = OP_NEW(OpVector<HistoryPage>, ());
		nickPageVector  = OP_NEW(OpVector<HistoryPage>, ());
		midnight_timer  = OP_NEW(OpTimer, ());
		stripped_tree   = OP_NEW((OpSplayTree<HistoryItem, HistoryPage>), (size_history_prefixes));

		if(!modelVector     ||
		   !bookmarkVector  ||
		   !operaPageVector ||
		   !nickPageVector  ||
		   !midnight_timer  ||
		   !stripped_tree)
			break;

		HistoryModel * instance = OP_NEW(HistoryModel, (num,
														modelVector,
														bookmarkVector,
														operaPageVector,
														nickPageVector,
														timeFolderArray,
														protocol_array,
														stripped_tree,
														prefix_tree,
														midnight_timer,
														history_time_periods,
														history_prefixes,
														history_internal_pages,
														size_history_time_periods,
														size_history_prefixes,
														size_history_internal_pages));

		OP_ASSERT(instance);

		if(instance)
			instance->SetMidnightTimeout();

		return instance;
	}
	while(FALSE);

	// A memory error occured
	// delete everything allocated :

	CleanTables(history_time_periods,
				history_prefixes,
				history_internal_pages);

	CleanUp(prefix_tree,
			protocol_array,
			size_history_prefixes);

	CleanUpMore(midnight_timer,
				modelVector,
				bookmarkVector,
				operaPageVector,
				nickPageVector,
				timeFolderArray,
				stripped_tree);

	OP_ASSERT(FALSE);
	return 0;
}


/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS HistoryModel::InitOperaPages()
{
	OP_STATUS status;
	OpString  title;

	for(int i = 0; i < m_size_history_internal_pages; i++)
	{
		status = g_languageManager->GetString(m_history_internal_pages[i].title, title);

		if(!OpStatus::IsSuccess(status))
		{
			OP_ASSERT(FALSE);
			return status;
		}

		status = AddOperaItem(m_history_internal_pages[i].url, title.CStr());

		if(OpStatus::IsMemoryError(status))
		{
			OP_ASSERT(FALSE);
			return status;
		}
	}

	return OpStatus::OK;
}

/***********************************************************************************
 ** Calls DeleteAll
 **
 ** HistoryModel::~HistoryModel
 ***********************************************************************************/
HistoryModel::~HistoryModel()
{
	m_model_alive = FALSE;

    //Remove all from UI - they will now be destroyed
	BroadcastModelDeleted();

	//Save to disk:
#ifdef HISTORY_SAVE
	Save(TRUE);
#endif //HISTORY_SAVE

#ifdef HISTORY_DEBUG
	LogStats();
#endif //HISTORY_DEBUG

    //Deleting all the items :
    while(m_modelVector->GetCount())
    {
		HistoryPage * item = m_modelVector->Remove(0);
		item->SetIsRemoved();
		DeleteItem(item);
    }

    //Deleting all the bookmarks :
    while(m_bookmarkVector->GetCount())
    {
		HistoryPage * item = m_bookmarkVector->Remove(0);
		RemoveBookmark(item);
    }

	//Deleting all the internal pages :
    while(m_operaPageVector->GetCount())
    {
		HistoryPage * item = m_operaPageVector->Remove(0);
		RemoveOperaPage(item);
    }

	//Deleting all the nicks :
	while(m_nickPageVector->GetCount())
    {
		HistoryPage * item = m_nickPageVector->Remove(0);
		RemoveNick(item);
    }

	CleanUp(m_prefix_tree,
			m_protocol_array,
			m_size_history_prefixes);

	CleanUpMore(m_midnight_timer,
				m_modelVector,
				m_bookmarkVector,
				m_operaPageVector,
				m_nickPageVector,
				m_timeFolderArray,
				m_strippedTree);

	CleanTables(m_history_time_periods,
				m_history_prefixes,
				m_history_internal_pages);

#ifdef HISTORY_DEBUG
	LogStatsWhenDeleted();
#endif //HISTORY_DEBUG
}

/***********************************************************************************
 ** CleanUp
 **
 ** A local function to clean up if any allocation fails in initPrefixes
 ** (see below)
 **
 ***********************************************************************************/
void HistoryModel::CleanUp(OpSimpleSplayTree<PrefixMatchNode> *& loc_prefix_tree,
						   HistoryPrefixFolder           **& protocol_array,
						   int                               size_history_prefixes)
{
	//Delete all allocated :

	if(loc_prefix_tree)
		OP_DELETE(loc_prefix_tree);

	if(protocol_array)
	{
		for(int i = 0; i < size_history_prefixes; i++)
		{
			OP_DELETE(protocol_array[i]);
		}

 		OP_DELETEA(protocol_array);
	}

	loc_prefix_tree   = 0;
	protocol_array    = 0;
}

/***********************************************************************************
 ** CleanUpMore
 **
 **
 **
 **
 ***********************************************************************************/
void HistoryModel::CleanUpMore(OpTimer *& midnight_timer,
							   OpVector<HistoryPage>    *& modelVector,
							   OpVector<HistoryPage>    *& bookmarkVector,
							   OpVector<HistoryPage>    *& operaPageVector,
							   OpVector<HistoryPage>    *& nickPageVector,
							   HistoryTimeFolder       **& timeFolderArray,
							   OpSplayTree<HistoryItem, HistoryPage> *& stripped_tree)
{
	//The timers:
	OP_DELETE(midnight_timer);

	//And finally the vectors
	OP_DELETE(modelVector);
	OP_DELETE(bookmarkVector);
	OP_DELETE(operaPageVector);
	OP_DELETE(nickPageVector);

	OP_DELETE(stripped_tree);

	CleanTimeFolders(timeFolderArray);

	midnight_timer  = 0;
	modelVector     = 0;
	bookmarkVector  = 0;
	operaPageVector = 0;
	nickPageVector  = 0;
	stripped_tree   = 0;
	timeFolderArray = 0;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void HistoryModel::CleanTimeFolders(HistoryTimeFolder **& timeFolderArray)
{
	if(timeFolderArray)
	{
		//Time folders:
		for(INT i = 0; i < NUM_TIME_PERIODS; i++)
		{
			OP_DELETE(timeFolderArray[i]);
		}

		OP_DELETEA(timeFolderArray);

		timeFolderArray = 0;
	}
}


/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void HistoryModel::CleanKey(HistoryKey * key)
{
	if(key && !key->InUse())
	{
		OP_DELETE(key);
	}
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void HistoryModel::CleanTables(HistoryTimePeriod   *& history_time_periods,
							   HistoryPrefix       *& history_prefixes,
							   HistoryInternalPage *& history_internal_pages)
{
	OP_DELETEA(history_time_periods);
	OP_DELETEA(history_prefixes);
	OP_DELETEA(history_internal_pages);

	history_time_periods   = 0;
	history_prefixes       = 0;
	history_internal_pages = 0;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
// If this is the stripped tree you need an array at each node to take care of
//   duplicate stipped urls with different prefixes: http://www.opera.com and
//   ftp://ftp.opera.com - whose stripped versions are only "opera.com"

// If this is a prefix tree than all urls are distinct and only one item can be
//   present in a node in the tree.

OP_STATUS HistoryModel::initPrefixes(OpSimpleSplayTree<PrefixMatchNode> ** prefix_tree,
									 HistoryPrefixFolder *** protocol_array,
									 HistoryPrefix * history_prefixes,
									 int size_history_prefixes)
{
	// Set up the temporary pointers. Will not set the parameters before
	// all memory has successfully been allocated and initialized.
	OpSimpleSplayTree<PrefixMatchNode> * loc_prefix_tree    = 0;
	HistoryPrefixFolder               ** loc_protocol_array = 0;

	// Temporary array to set parent folder for the prefix folders.
	// Contains the index into loc_protocol_array of the corresponding
	// toplevel folder.
	INT top_level_protocols[NUM_PROTOCOLS];

	// Allocate the datastructures we need :
	RETURN_IF_ERROR(AllocatePrefixMemory(loc_prefix_tree,
										 loc_protocol_array,
										 size_history_prefixes));

	// -----------------------------------------------------------
	// Process the prefixes :
	INT i = 0;
	OP_STATUS status = OpStatus::OK;

	for( ; i < size_history_prefixes; i++)
	{
		status = initPrefix(loc_prefix_tree,
							loc_protocol_array,
							top_level_protocols,
							i,
							history_prefixes);

		if(!OpStatus::IsSuccess(status))
			break;
	}
	// -----------------------------------------------------------

	// -----------------------------------------------------------
	//If Out of memory - delete everything allocated
	OP_ASSERT(i == size_history_prefixes);

	if(i < size_history_prefixes)
	{
        //Delete all structures :
		CleanUp(loc_prefix_tree,
				loc_protocol_array,
				size_history_prefixes);

		OP_ASSERT(FALSE);

		return OpStatus::ERR_NO_MEMORY;
	}
	// -----------------------------------------------------------

	// If not return pointers to the memory successfully initialized :
	*prefix_tree    = loc_prefix_tree;
	*protocol_array = loc_protocol_array;

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS HistoryModel::initPrefix(OpSimpleSplayTree<PrefixMatchNode> *& prefix_tree,
								   HistoryPrefixFolder **& protocol_array,
								   INT * top_level_protocols,
								   INT index,
								   HistoryPrefix * history_prefixes)

{
	PrefixMatchNode ** insert_point = 0;
	PrefixMatchNode *  prefix_node  = 0;
	PrefixMatchNode *  suffix_node  = 0;
	HistoryPrefixFolder * folder = 0;

	//____________________________________________________________
	// Get the matchnode keys:

	HistoryKey * prefix_key = 0;
	HistoryKey * suffix_key = 0;

	OpString prefix_str;
	RETURN_IF_ERROR(prefix_str.Set(history_prefixes[index].prefix));

	OpString suffix_str;
	RETURN_IF_ERROR(suffix_str.Set(history_prefixes[index].suffix));

	prefix_key = (HistoryKey *) prefix_tree->GetKey(prefix_str.CStr());
	suffix_key = (HistoryKey *) prefix_tree->GetKey(suffix_str.CStr());

	if(!prefix_key)
		prefix_key = HistoryKey::Create(prefix_str);

	if(!suffix_key)
		suffix_key = HistoryKey::Create(suffix_str);

	OP_ASSERT(prefix_key);
	OP_ASSERT(suffix_key);

	// -----------------------------------------------------------
	if(!prefix_key || !suffix_key)
	{
		CleanKey(prefix_key);
		CleanKey(suffix_key);

		return OpStatus::ERR_NO_MEMORY;
	}
	// -----------------------------------------------------------

	//____________________________________________________________
	// The folder uses the prefix_key :

	HistoryKey * key = prefix_key;

	//____________________________________________________________
	// Create the folder itself:

	folder = HistoryPrefixFolder::Create(history_prefixes[index].prefix,
										 index,
										 key,
										 prefix_key,
										 suffix_key);

	// -----------------------------------------------------------
	if(!folder)
	{
		CleanKey(prefix_key);
		CleanKey(suffix_key);

		return OpStatus::ERR_NO_MEMORY;
	}
	// -----------------------------------------------------------

	// Insert the folder :
	protocol_array[index] = folder;

	// Get the nodes :
	prefix_node = &(folder->GetPrefixNode());
	suffix_node = &(folder->GetSuffixNode());

	// Get a pointer to the spot where the prefix node will be inserted.
	insert_point = prefix_tree->InsertInternalNode(prefix_node->GetKey());

	// -----------------------------------------------------------
	if(!insert_point)
		return OpStatus::ERR_NO_MEMORY;
	// -----------------------------------------------------------

	//Insert the prefix node in the tree :
	*insert_point = prefix_node;

	// Set the parent folder :
	// If folder has a suffix and has a protocol - it must be a subfolder to another
	// toplevel prefix - as in: http://www. is contained in http://
	if(history_prefixes[index].suffix_len != 0 && history_prefixes[index].protocol != NONE_PROTOCOL)
	{
		folder->SetContained(TRUE);

		HistoryProtocol       folder_protocol = history_prefixes[index].protocol;
		INT                   parent_index    = top_level_protocols[folder_protocol];
		HistoryPrefixFolder * parent          = protocol_array[parent_index];

		folder->SetPrefixFolder(parent);
	}

	// If this is a toplevel prefix insert it in the protocol array so that it's children can find it :
	if(history_prefixes[index].suffix_len == 0 && history_prefixes[index].protocol != NONE_PROTOCOL)
	{
		top_level_protocols[history_prefixes[index].protocol] = index;
	}

	// Insert the suffix node into the tree if there is a suffix :
	if(history_prefixes[index].suffix_len)
	{
		insert_point = prefix_tree->InsertLeafNode(suffix_node->GetKey(),
												   history_prefixes[index].protocol);

		// -----------------------------------------------------------
		if(!insert_point)
			return OpStatus::ERR_NO_MEMORY;
		// -----------------------------------------------------------

		//Insert the suffix node in the tree :
		*insert_point = suffix_node;
	}

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS HistoryModel::AllocatePrefixMemory(OpSimpleSplayTree<PrefixMatchNode> *& prefix_tree,
											 HistoryPrefixFolder **& protocol_array,
											 int size_history_prefixes)
{
	prefix_tree    = OP_NEW(OpSimpleSplayTree<PrefixMatchNode>, (NUM_PROTOCOLS));
	protocol_array = OP_NEWA(HistoryPrefixFolder*, size_history_prefixes);

	OP_ASSERT(prefix_tree);
	OP_ASSERT(protocol_array);

	if(!prefix_tree   ||
	   !protocol_array)
	{
        //Delete all allocated :
		CleanUp(prefix_tree,
				protocol_array,
				size_history_prefixes);

		return OpStatus::ERR_NO_MEMORY;
	}

 	for(INT j = 0; j < NUM_PROTOCOLS; j++)
 		protocol_array[j] = 0;

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS HistoryModel::initFolderArray(HistoryTimeFolder*** time_folder_array,
										HistoryTimePeriod * history_time_periods)
{
	HistoryTimeFolder** timeFolderArray = 0;
	timeFolderArray = OP_NEWA(HistoryTimeFolder*, NUM_TIME_PERIODS);

	if(!timeFolderArray)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	INT i = 0;

	//Make a clean array :
	for(i = 0; i < NUM_TIME_PERIODS; i++)
		timeFolderArray[i] = 0;

/*
  If the strings are not present - the Create method will assume out of memory
  and the core history model will not be created - there will be no attempt to
  recover from this - the strings should be there.
*/

	OpString title;
	OP_STATUS status = OpStatus::OK;

	for(i = 0; i < NUM_TIME_PERIODS; i++)
	{
		status = g_languageManager->GetString(history_time_periods[i].title, title);

		if(!OpStatus::IsSuccess(status))
		{
			OP_ASSERT(FALSE);
			break; //Error - will be caught by test after loop
		}

		OP_ASSERT(title.CStr());

		HistoryKey * key = HistoryKey::Create(title);

		// -----------------------------------------------------------
		if(!key)
			break; //Out of memory - will be caught by test after loop
		// -----------------------------------------------------------

		timeFolderArray[i] = HistoryTimeFolder::Create(title.CStr(),
																(TimePeriod) i,
																key);

		if(!timeFolderArray[i])
		{
			OP_ASSERT(FALSE);
			status = OpStatus::ERR_NO_MEMORY;
			break; //Out of memory - will be caught by test after loop
		}
	}

	if(i < NUM_TIME_PERIODS)
	{
		CleanTimeFolders(timeFolderArray);
		return status;
	}

	*time_folder_array = timeFolderArray;
	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
HistoryPage*  HistoryModel::GetItemAtPosition(UINT32 index)
{
	if(index < m_modelVector->GetCount())
		return m_modelVector->Get(index);
	return 0;
}


/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL HistoryModel::IsAcceptable(const OpStringC& url_name)
{
	if(url_name.IsEmpty())
	{
		return FALSE;
	}

	UINT32 len = url_name.Length();

	if(len > MAX_URL_LEN)
	{
		//Urls should not be added to history if they
		//exceed MAX_URL_LEN (currently 4096)
		return FALSE;
	}

	UINT32 prefix_len = 0;
	OP_STATUS status = OpStatus::OK;
	INT32 prefix_num = GetPrefix(url_name, prefix_len, status);
	RETURN_VALUE_IF_ERROR(status, FALSE);

	return prefix_num != 0 && len > prefix_len;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS HistoryModel::MakeUnescaped(OpString& address)
{
	if (address.HasContent())
	{
		// address is stored as an escaped string. We want to show it
		// as an unescaped string to make it as readable as possible
		URL address_url = g_url_api->GetURL(address.CStr());
		return address_url.GetAttribute(URL::KUniName_With_Fragment_Username, address);
	}

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS HistoryModel::AddHistoryItem(const OpStringC& url_name,
									   const OpStringC& title,
									   HistoryItemType type,
									   time_t acc,
									   BOOL save_session,
									   time_t average_interval,
									   HistoryPage ** out_item,
									   BOOL prefer_exisiting_title)
{
	//title can be a zero length string : <title></title>
	OP_ASSERT(url_name.HasContent() && title.HasContent());

    if( url_name.IsEmpty() || title.IsEmpty())
		return OpStatus::ERR;

	if(url_name.Length() > MAX_URL_LEN)
	{
		//Urls should not be added to history if they
		//exceed MAX_URL_LEN (currently 4096)
		return OpStatus::ERR;
	}

	if(title.Length() > MAX_URL_LEN)
	{
		//Titles should not be added to history if they
		//exceed MAX_URL_LEN (currently 4096)
		return OpStatus::ERR;
	}

    if (m_max || type != HISTORY_ITEM_TYPE)
    {
		if (out_item)
			*out_item = 0;

        //http://www.something.com/... -> something.com/...

		OpString stripped;
		OpString address;
		RETURN_IF_ERROR(address.Set(url_name));

		INT32 prefix_num = 0;

		if(type == NICK_ITEM_TYPE)
		{
			RETURN_IF_ERROR(stripped.Set(address.CStr()));
		}
		else
		{
			OP_STATUS strip_status = OpStatus::OK;
			prefix_num = StripPrefix(address, stripped, strip_status);

			OP_ASSERT(prefix_num);

			if(prefix_num == 0) // Invalid entry
				return OpStatus::ERR;

			// Stripped should contain stripped version of url
			OP_ASSERT(OpStatus::IsSuccess(strip_status));
			RETURN_IF_ERROR(strip_status);
		}

		HistoryPage *  item           = 0;
		HistoryPage ** item_normal    = 0;

		// Inserting stripped for prefix into stripped tree
		LeafKeyGuard<HistoryItem, HistoryPage> guard(m_strippedTree, prefix_num);
		guard.findKey(stripped);

		// If the nick already exists abort
		if(type == NICK_ITEM_TYPE && guard.getKey())
			return OpStatus::ERR;

		// If the key does not exist make it
		if(!guard.getKey())
			RETURN_IF_ERROR(guard.createKey(stripped));

		item_normal = guard.insertKey();

		if(!item_normal)
			return OpStatus::ERR_NO_MEMORY;

		BOOL new_item = FALSE;

		if(!(*item_normal))
		{
			//No such item in the list/tree - create the item and insert into tree:

			item = HistoryPage::Create(acc, average_interval, title, guard.getKey()); //Insert into tree

			if(item)
				guard.release();
			else
				return OpStatus::ERR_NO_MEMORY;

			*item_normal = item;

			new_item = TRUE;
		}
		else
		{
			//Item was in the list/tree:

			item = *item_normal;

			if(type == HISTORY_ITEM_TYPE)
			{
				if (!prefer_exisiting_title)
					item->SetTitle(title); //Title may have changed

				if(item->IsInHistory())
				{
					//Unlink from list so that it can be inserted at the front.
					// This will not remove from trees.
					MoveItem(item);
				}
			}
		}

		//Insert folder :
		HistorySiteFolder * server = 0;

		if(type == HISTORY_ITEM_TYPE  ||
		   type == BOOKMARK_ITEM_TYPE)
		{
			server = AddServer(url_name);
		}

		if(type == HISTORY_ITEM_TYPE)
		{
			item->SetInHistory();

			if(!new_item)
			{
				item->UpdateAccessed(acc);
			}
		}
		else if(type == BOOKMARK_ITEM_TYPE)
		{
			if(!item->IsBookmarked()) //insert bookmark
			{
				RETURN_IF_ERROR(m_bookmarkVector->Add(item));
				item->SetBookmarked();
			}
		}
		else if(type == OPERA_PAGE_TYPE)
		{
			if(!item->IsOperaPage()) //insert internal page
			{
				RETURN_IF_ERROR(m_operaPageVector->Add(item));
				item->SetIsOperaPage();
			}
		}
		else if(type == NICK_ITEM_TYPE)
		{
			if(!item->IsNick())
			{
				RETURN_IF_ERROR(m_nickPageVector->Add(item));
				item->SetIsNick();
			}
		}

		if(type == HISTORY_ITEM_TYPE  ||
		   type == BOOKMARK_ITEM_TYPE ||
		   type == OPERA_PAGE_TYPE)
		{
			//Associate prefixfolder with item
			PrefixMatchNode * prefix_node = 0;
			m_prefix_tree->FindPrefixInternalNode(prefix_node, url_name.CStr());

			//Item should have prefix folder
			OP_ASSERT(prefix_node);

			if(prefix_node)
			{
				item->SetPrefixFolder(m_protocol_array[prefix_node->GetIndex()]);
			}
		}

		if(type == HISTORY_ITEM_TYPE || type == BOOKMARK_ITEM_TYPE)
		{
			RETURN_IF_ERROR(AddItem(item, server, save_session, type));
		}

		if (out_item)
			*out_item = item;

		if(type == HISTORY_ITEM_TYPE)
		{
			if (save_session)
			{
#if defined(HISTORY_SAVE)
				return Save();
#endif // HISTORY_SAVE
			}
		}
    }
    return OpStatus::OK;
}


/***********************************************************************************
 ** Adds an item to the list checking for duplicates.
 ** In that case the duplicate will be updated and otherwise a new item is inserted
 ** at the head of the list. If max capacity has been reached the last item in the list
 ** is removed. If save_session is TRUE g_application->SaveCurrentSession() is called.
 ** If the new item could not be allocated ERR_NO_MEMORY is returned.
 **
 ** HistoryModel::Add
 ** @param url_name
 ** @param title
 ** @param acc
 ** @param save_session
 ** @param average_interval
 **
 ** @return OP_STATUS (OK or ERR_NO_MEMORY)
 ***********************************************************************************/
OP_STATUS HistoryModel::AddPage(const OpStringC& url_name,
								const OpStringC& title,
								time_t acc,
								BOOL save_session,
								time_t average_interval)
{
    return AddHistoryItem(url_name,
						  title,
						  HISTORY_ITEM_TYPE,
						  acc,
						  save_session,
						  average_interval);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS HistoryModel::AddBookmark(const OpStringC& url_name,
									const OpStringC& title,
									HistoryPage *& item)
{
    return AddHistoryItem(url_name,
						  title,
						  BOOKMARK_ITEM_TYPE,
						  0,       // acc
						  FALSE,   // save_session
						  -1,      // average_interval
						  &item);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS HistoryModel::AddBookmarkNick(const OpStringC & nick,
										HistoryPage & bookmark_item)
{
	if(!bookmark_item.IsBookmarked())
		return OpStatus::ERR;

	if(nick.IsEmpty())
		return OpStatus::ERR;

	// If the bookmark has an old nick - remove it :
	// -----------------------
	HistoryPage * old_nick = bookmark_item.GetAssociatedItem();

	if(old_nick)
	{
		// TODO no real reason to remove the existing nick if
		//      the old and the new are the same
		RemoveNick(old_nick);
	}

	// Use the title of the bookmark for the title of the nick :
	// -----------------------
	OpString title;
	bookmark_item.GetTitle(title);

	// Add the nick :
	// -----------------------
	HistoryPage * nick_item = 0;
	RETURN_IF_ERROR(AddHistoryItem(nick,
								   title,
								   NICK_ITEM_TYPE,
								   0,       // acc
								   FALSE,   // save_session
								   -1,      // average_interval
								   &nick_item));

	if(!nick_item)
		return OpStatus::ERR;

	// Associate the nick with the bookmark :
	// -----------------------
	OP_STATUS status = nick_item->SetAssociatedItem(&bookmark_item);

	if(!OpStatus::IsSuccess(status))
	{
		RemoveNick(nick_item);
		return status;
	}

	// Associate the bookmark with the nick :
	// -----------------------
	status = bookmark_item.SetAssociatedItem(nick_item);

	if(!OpStatus::IsSuccess(status))
	{
		RemoveNick(nick_item);
		return status;
	}

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS HistoryModel::GetBookmarkByNick(const OpStringC & nick,
										  HistoryPage *& bookmark_item)
{
	HistoryPage * nick_item = 0;
	RETURN_IF_ERROR(GetItem(nick, nick_item, TRUE));

	bookmark_item = nick_item->GetAssociatedItem();

	if(bookmark_item)
		return OpStatus::OK;

	return OpStatus::ERR;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS HistoryModel::AddOperaItem(const OpStringC& url_name,
									 const OpStringC& title)
{
    return AddHistoryItem(url_name,
						  title,
						  OPERA_PAGE_TYPE);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void HistoryModel::Delete(HistoryItem* item, BOOL force)
{
	if(!force && item->IsPinned())
		return;

    if(item->GetType() == OpTypedObject::FOLDER_TYPE)
    {
		DeleteFolder((HistoryFolder*) item);
	}
    else if(item->GetType() == OpTypedObject::HISTORY_ELEMENT_TYPE)
    {
		DeleteItem((HistoryPage*) item);
    }
}

/***********************************************************************************
 **
 ** DeleteItem and RemoveFolder are mutually recursive. If an item to be removed
 ** is a folder than all of its subitems will have to be deleted, some of these
 ** may again be folders and so on...
 **
 ***********************************************************************************/
void HistoryModel::DeleteItem(HistoryPage* page,
							  BOOL deleteParent,
							  BOOL literal_key)
{
	HistorySiteFolder* parent = page->GetSiteFolder();

	if(page->IsInHistory())
	{
		BroadcastItemRemoving(page);

#ifdef VISITED_PAGES_SEARCH
		if(m_model_alive)
		{
			// Remove the index for the page aswell:
			OpString address;
			page->GetAddress(address);
			OpString8 tmp;
			if (OpStatus::IsSuccess(tmp.Set(address.CStr())))
				OpStatus::Ignore(g_visited_search->InvalidateUrl(tmp.CStr()));
		}
#endif //VISITED_PAGES_SEARCH
	}

	BOOL removed = RemoveItem(page, literal_key);

	// Item should have been removed
	OP_ASSERT(removed              ||
			  page->IsBookmarked() ||
			  page->IsOperaPage()  ||
			  page->IsNick());

	// This may be a recovery if item was not a bookmark or an internal page
	// - but still was not removed from the trees : better to not leave a
	// dangling pointer than to leak a bit
	if(removed)
	{
		BroadcastItemDeleting(page);
		OP_DELETE(page);
	}
	else
	{
#ifdef HISTORY_DEBUG
		OpString address;
		page->GetAddress(address);
		OpString8 tmp;
		tmp.Set(address.CStr());
		printf("DeleteItem : Aborted\n");
		printf("Address    : %s\n", tmp.CStr());
		printf("Bookmarked : %d\n", page->IsBookmarked());
		printf("OperaPage  : %d\n", page->IsOperaPage());
		printf("Nick       : %d\n", page->IsNick());
#endif // HISTORY_DEBUG
	}

	if(parent && deleteParent && parent->IsEmpty())
		Delete(parent);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void HistoryModel::DeleteFolder(HistoryFolder* folder)
{
	EmptyFolder(folder);

	BroadcastItemRemoving(folder);

	BOOL removed = FALSE;

	if(folder->IsDeletable())
	{
		removed = RemoveFolder(folder);

		// Folder should have been removed
		OP_ASSERT(removed);
	}

	HistorySiteFolder* site = 0;

	//This might be the site folders last subfolder
	if(folder->GetFolderType() == SITE_SUB_FOLDER)
		site = folder->GetSiteFolder();

	//Should not leave a dangeling pointer in tree -
	// better off with a memory leak -> this is a recovery
	if(removed && folder->IsDeletable())
	{
		BroadcastItemDeleting(folder);
		OP_DELETE(folder);
	}
	else
	{
#ifdef HISTORY_DEBUG
		OpString title;
		folder->GetTitle(title);
		OpString8 tmp;
		tmp.Set(title.CStr());
		printf("DeleteFolder : Aborted\n");
		printf("Title        : %s\n", tmp.CStr());
		printf("Deletable    : %d\n", folder->IsDeletable());
#endif // HISTORY_DEBUG
	}

	if(site && site->IsEmpty())
		Delete(site);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void HistoryModel::EmptyFolder(HistoryFolder* folder)
{
	HistoryFolderType type = folder->GetFolderType();

	switch(type)
	{
		case SITE_FOLDER     : EmptySiteFolder((HistorySiteFolder*) folder);       break;
		case SITE_SUB_FOLDER : EmptySiteSubFolder((HistorySiteSubFolder*) folder); break;
		case TIME_FOLDER     : EmptyTimeFolder((HistoryTimeFolder*) folder);       break;
		case PREFIX_FOLDER   : EmptyPrefixFolder((HistoryPrefixFolder*) folder);   break;
		default: OP_ASSERT(FALSE);
	}
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void HistoryModel::EmptySiteFolder(HistorySiteFolder* folder)
{
    OpVector<HistoryPage> folder_items;

	m_strippedTree->GetLeafNodesOfSubtree(folder->GetKeyString(), folder_items, OpTypedObject::HISTORY_ELEMENT_TYPE);

    // Either there are items in the folder or it is empty
	OP_ASSERT(folder_items.GetCount() || folder->IsEmpty());

	// You can't have items and be empty at the same time
	OP_ASSERT(!(folder_items.GetCount() && folder->IsEmpty()));

	while(folder_items.GetCount())
	{
		HistoryPage * item = folder_items.Remove(0);
		DeleteItem(item, FALSE);
	}

	// Now it should be empty
	OP_ASSERT(folder->IsEmpty());
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void HistoryModel::EmptySiteSubFolder(HistorySiteSubFolder* folder)
{
	TimePeriod period = folder->GetPeriod();
	HistorySiteFolder*            site   = folder->GetSiteFolder();

	UINT32 start_index = BinaryTimeSearchPeriod(period);
	HistoryPage* item = 0;

    OpVector<HistoryPage> folder_items;

	for(; start_index < m_modelVector->GetCount(); start_index++)
	{
		item = m_modelVector->Get(start_index);

		if(GetTimePeriod(item->GetAccessed()) != period)
			break;

		if(item->GetSiteFolder() != site)
			continue;

		RETURN_VOID_IF_ERROR(folder_items.Add(item));
	}

	while(folder_items.GetCount())
	{
		HistoryPage* item = folder_items.Remove(0);
		DeleteItem(item, FALSE);
	}
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
TimePeriod HistoryModel::GetTimePeriod(time_t accessed){

	time_t now     = g_timecache->CurrentTime();
	time_t elapsed = now - accessed;

	time_t today_sec      = SecondsSinceMidnight();
	time_t yesterday_sec  = today_sec     + m_history_time_periods[YESTERDAY].seconds;
	time_t last_week_sec  = yesterday_sec + m_history_time_periods[WEEK].seconds;
	time_t last_month_sec = last_week_sec + m_history_time_periods[MONTH].seconds;

	if (elapsed < today_sec)  //Since midnight
		return TODAY;

	else if (elapsed < yesterday_sec) //Yesterday
		return YESTERDAY;

	else if (elapsed < last_week_sec) //The week before yesterday
		return WEEK;

	else if (elapsed < last_month_sec)  //Last 4 weeks before last week
		return MONTH;

	else //Older
		return OLDER;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
time_t HistoryModel::SecondsSinceMidnight()
{
	time_t now     = g_timecache->CurrentTime();
	struct tm* time_struct = op_localtime(&now);
	if (!time_struct)
		return 0;

	time_t sec = time_struct->tm_sec + (time_struct->tm_min*60) + (time_struct->tm_hour*3600);
	return sec;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL HistoryModel::IsLocalFileURL(const OpStringC & url, OpString & stripped)
{
	OP_STATUS status = OpStatus::OK;
	int prefix_num = StripPrefix(url, stripped, status);

	RETURN_VALUE_IF_ERROR(status, FALSE);

	if(prefix_num == 0)
		return FALSE;

	HistoryProtocol protocol = m_history_prefixes[prefix_num].protocol;

	if(protocol == FILE_PROTOCOL)
	{
		if(stripped[0] == '/')
			return TRUE;

		// Add a slash to the beginning if there isn't one there already
		if(OpStatus::IsSuccess(stripped.Insert(0, "/")))
			return TRUE;
	}

	stripped.Empty();
	return FALSE;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void HistoryModel::EmptyTimeFolder(HistoryTimeFolder* folder)
{
	TimePeriod period = folder->GetPeriod();
	UINT32 start_index = BinaryTimeSearchPeriod(period);
	HistoryPage* item = 0;

    OpVector<HistoryPage> folder_items;

	for(; start_index < m_modelVector->GetCount(); start_index++)
	{
		item = m_modelVector->Get(start_index);

		if(GetTimePeriod(item->GetAccessed()) != period)
			break;

		RETURN_VOID_IF_ERROR(folder_items.Add(item));
	}

	while(folder_items.GetCount())
	{
		HistoryPage* item = folder_items.Remove(0);
		DeleteItem(item, TRUE);
	}
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void HistoryModel::EmptyPrefixFolder(HistoryPrefixFolder* folder)
{
    OpVector<HistoryPage> folder_items;

    OpVector<PrefixMatchNode> pvector;

	//Find all prefixes matching - protocol
	m_prefix_tree->FindExactInternalNodes(pvector, folder->GetKeyString(), OpTypedObject::FOLDER_TYPE);

	for(UINT32 i = 0; i < pvector.GetCount(); i++)
	{
		PrefixMatchNode * prefix_node = pvector.Get(i);
		INT32 prefix_num = prefix_node->GetIndex();
		m_strippedTree->FindAllLeafNodes(folder_items, OpTypedObject::HISTORY_ELEMENT_TYPE, prefix_num);
	}

	while(folder_items.GetCount())
	{
		HistoryPage* item = folder_items.Remove(0);

		if(item)
			DeleteItem(item, TRUE);
	}
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
INT32 HistoryModel::BinaryTimeSearchPeriod(TimePeriod period){

	time_t search_time = 0;
	time_t now         = g_timecache->CurrentTime();

	time_t today_sec      = SecondsSinceMidnight();
	time_t yesterday_sec  = today_sec     + m_history_time_periods[YESTERDAY].seconds;
	time_t last_week_sec  = yesterday_sec + m_history_time_periods[WEEK].seconds;
	time_t last_month_sec = last_week_sec + m_history_time_periods[MONTH].seconds;

	switch(period)
	{
		case TODAY:      search_time = now;                  break;
		case YESTERDAY:  search_time = now - today_sec;      break;
		case WEEK:       search_time = now - yesterday_sec;  break;
		case MONTH:      search_time = now - last_week_sec;  break;
		case OLDER:      search_time = now - last_month_sec; break;
		default: OP_ASSERT(FALSE);
	}

	return BinaryTimeSearch(search_time);
}

/***********************************************************************************
 **
 **
 **
 ** Remember that the vector is sorted in reverse - so the highest access times are
 ** at the top.
 ***********************************************************************************/
INT32 HistoryModel::BinaryTimeSearch(time_t search_time)
{
	if(m_modelVector->GetCount() == 0)
		return 0;

	// Just in case we are adding them in order
	// Which we are when parsing the global.dat file
	if(m_modelVector->Get(0)->GetAccessed() < search_time)
		return 0;

	INT start = 0;
	INT end   = m_modelVector->GetCount() - 1;

	while(start < end)
	{
		INT middle = (start + end) / 2;

		HistoryPage * middle_page = m_modelVector->Get(middle);

		if(middle_page->GetAccessed() > search_time)
		{
			start = middle + 1;
		}
		else
		{
			end = middle;
		}
	}

	return start;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS HistoryModel::DeleteAllItems()//	*** DG-241198 used by Application.cpp
{
	BroadcastModelChanging();

    //Deleting all the items

	while(m_modelVector->GetCount())
    {
		HistoryPage * item = m_modelVector->Remove(0);
		DeleteItem(item);
    }

    //Removing all the items
    m_modelVector->Clear();

#ifdef VISITED_PAGES_SEARCH
	//Remove all indexed data:
	RETURN_IF_ERROR(g_visited_search->Clear());
#endif //VISITED_PAGES_SEARCH

    //All thats left are the time folders and prefixfolders
    //They are generic and contain no personal data

	BroadcastModelChanged();

	return OpStatus::OK;
}


/***********************************************************************************
 ** Returns address of first item in list. Sets m_CurrentIndex to this item. If list is
 ** empty returns 0
 **
 ** HistoryModel::GetFirst
 **
 ** @return address of first item in list or 0 if list is empty
 ***********************************************************************************/
OP_STATUS HistoryModel::GetFirst(OpString & address)
{
    m_CurrentIndex = 0;

    if (m_modelVector->GetCount() == 0)
		return OpStatus::ERR;

    HistoryPage * item = m_modelVector->Get(m_CurrentIndex);

    return item ? item->GetAddress(address) : OpStatus::ERR;
}

/***********************************************************************************
 ** Returns address of item following m_CurrentIndex in list. Sets m_CurrentIndex to this
 ** item. If no such item exists returns 0.
 **
 ** HistoryModel::GetNext
 **
 ** @return address of next item in list or 0 if index is outside the list
 ***********************************************************************************/
OP_STATUS HistoryModel::GetNext(OpString & address)
{
    m_CurrentIndex++;

    if (m_CurrentIndex >= m_modelVector->GetCount())
		return OpStatus::ERR;

    HistoryPage * item =  m_modelVector->Get(m_CurrentIndex);

    return item ? item->GetAddress(address) : OpStatus::ERR;
}

/***********************************************************************************
 ** Returns title of item m_CurrentIndex
 **
 ** HistoryModel::GetTitle
 **
 ** @return title of m_CurrentIndex or 0 if index is outside the list
 ***********************************************************************************/
OP_STATUS HistoryModel::GetTitle(OpString & title)
{
    if (m_CurrentIndex >= m_modelVector->GetCount())
		return OpStatus::ERR;

    HistoryPage * item =  m_modelVector->Get(m_CurrentIndex);
    return item ? item->GetTitle(title) : OpStatus::ERR;
}

/***********************************************************************************
 ** Returns accessed of item m_CurrentIndex
 **
 ** HistoryModel::GetAccessed
 **
 ** @return accessed of m_CurrentIndex or 0 if index is outside the list
 ***********************************************************************************/
time_t HistoryModel::GetAccessed()
{
    if (m_CurrentIndex >= m_modelVector->GetCount())
		return 0;

    HistoryPage * item =  m_modelVector->Get(m_CurrentIndex);
    return item ? item->GetAccessed() : 0;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void HistoryModel::SetMidnightTimeout ()
{
	if( !m_midnight_timer_on )
	{
		UINT32 timeout_ms = static_cast<UINT32>(1000 * (m_history_time_periods[YESTERDAY].seconds - SecondsSinceMidnight()));

		m_midnight_timer->SetTimerListener( this );
		m_midnight_timer->Start(timeout_ms);
		m_midnight_timer_on = TRUE;
	}
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void HistoryModel::OnTimeOut(OpTimer* timer)
{
	if( timer == m_midnight_timer && m_midnight_timer_on)
	{
		m_midnight_timer_on = FALSE;

		// Broadcast to listeners to prepare to rebuild. This will force them to
		// regroup. This is the midnight call out.
		BroadcastModelChanging();
		BroadcastModelChanged();

		SetMidnightTimeout();
	}
	else
	{
		DelayedSave::OnTimeOut(timer);
	}
}

/***********************************************************************************
 ** Writes the list in reverse order to file
 **
 ** HistoryModel::Write
 ** @param ofp
 **
 ** @return OP_STATUS
 ***********************************************************************************/
OP_STATUS HistoryModel::Write(OpFile *ofp)
{
    OpString buf;

    // Try to allocate some initial space,
	// Should the buffer be bigger ?
	if (buf.Reserve(128) == NULL)
		return OpStatus::ERR_NO_MEMORY;

    //	Write OLDEST FIRST
	INT32 counter = m_modelVector->GetCount()-1;

    HistoryPage *p = 0;

    while (counter >= 0)
    {
		p = m_modelVector->Get(counter);

		RETURN_IF_ERROR(buf.Set(""));

		// Title :
		RETURN_IF_ERROR(buf.Append(p->GetTitle()));
		RETURN_IF_ERROR(buf.Append("\n"));

		// Url :
		RETURN_IF_ERROR(buf.Append(p->GetPrefix()));
		RETURN_IF_ERROR(buf.Append(p->GetStrippedAddress()));
		RETURN_IF_ERROR(buf.Append("\n"));

		// Last visited :
		RETURN_IF_ERROR(buf.AppendFormat(UNI_L("%ld\n"), p->GetAccessed()));

		// Average interval between visits :
		RETURN_IF_ERROR(buf.AppendFormat(UNI_L("%ld"), p->GetAverageInterval()));

		// Write to file :
		RETURN_IF_ERROR(ofp->WriteUTF8Line(buf));

		counter--;
    }

    return OpStatus::OK;
}

/***********************************************************************************
 ** Items are read from file and added to the list using Add. The items are validated
 ** before the insertion.  If not valid reading will abort, all items will be deleted and 
 ** return OpStatus::ERR if not valid.
 **
 ** HistoryModel::Read
 ** @param ifp
 **
 ** @return OP_STATUS (OK, ERR or ERR_NO_MEMORY)
 **
 ** Note: MAX_URL_LEN is 4096 and ReadUTF8Line reads only 4095 bytes plus '/0'
 ***********************************************************************************/
OP_STATUS HistoryModel::Read(OpFile *ifp)
{
    time_t timeAcc     = 0;
    long   avgInterval = -1;

    OpString acpTitle;
    OpString acpAddress;

    if (acpTitle.Reserve(MAX_URL_LEN) == NULL)
		return OpStatus::ERR_NO_MEMORY;

    if (acpAddress.Reserve(MAX_URL_LEN) == NULL)
		return OpStatus::ERR_NO_MEMORY;

    while (!ifp->Eof())
    {
		OpString number;

		// Title :
		RETURN_IF_ERROR(ifp->ReadUTF8Line(acpTitle));
		if (ifp->Eof()) break;

		// Url :
		RETURN_IF_ERROR(ifp->ReadUTF8Line(acpAddress));
		if (ifp->Eof()) break;

		// Last visited :
		RETURN_IF_ERROR(ifp->ReadUTF8Line(number));
		timeAcc = uni_atoi(number.CStr());
		if (ifp->Eof()) break;

		// Average interval between visits :
		RETURN_IF_ERROR(ifp->ReadUTF8Line(number));
		avgInterval = uni_atoi(number.CStr());

		//	Validate item before adding it to the list.
		//  TODO - do real validation
		if( op_localtime( &timeAcc) == NULL)
		{
			DeleteAllItems();
			return OpStatus::ERR;
		}

		// Add the item :
		RETURN_IF_ERROR(AddPage( acpAddress.Strip().CStr(),
								 acpTitle.Strip().CStr(),
								 timeAcc,
								 FALSE,
								 avgInterval));
    }
    return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS HistoryModel::OpenFile(OpFile *&fp,
								 const OpStringC &name,
								 OpFileFolder folder,
								 OpFileOpenMode mode)
{
	fp = NULL;

	OpFile *f = OP_NEW(OpFile, ());

	if (!f)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS op_err = f->Construct(name.CStr(), folder);
	if(OpStatus::IsError(op_err))
	{
		OP_DELETE(f);
		return op_err;
	}

	op_err = f->Open(mode);
	if (OpStatus::IsMemoryError(op_err)) // Only check for OOM, IsOpen is checked later on.
	{
		OP_DELETE(f);
		return op_err;
	}

	fp = f;
	return OpStatus::OK;
}

#if defined(URL_USE_HISTORY_VLINKS) && defined(DISK_CACHE_SUPPORT)
/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS HistoryModel::OpenVlinkFile(OpFile *& fp,
									  OpFileFolder folder)
{
	OpString name;
	RETURN_IF_ERROR(name.Set("vlink4.dat"));
	OpString name_old;
	RETURN_IF_ERROR(name_old.Set("vlink4.old"));

	fp = NULL;

	RETURN_IF_ERROR(OpenFile(fp, name, folder, OPFILE_READ));

	if(!fp->IsOpen())
	{
		OP_DELETE(fp);
		fp = NULL;

		RETURN_IF_ERROR(OpenFile(fp, name_old, folder, OPFILE_READ));
		if(!fp->IsOpen())
		{
			OP_DELETE(fp);
			fp = NULL;
			return OpStatus::ERR;
		}
	}
	return OpStatus::OK;
}
#endif // URL_USE_HISTORY_VLINKS && DISK_CACHE_SUPPORT

#if defined(URL_USE_HISTORY_VLINKS) && defined(DISK_CACHE_SUPPORT)
/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS HistoryModel::ReadVlink()
{
	// Set up the names and folder :
	OpFileFolder folder = OPFILE_HOME_FOLDER;

	// Open and initialize the cache file :
	OpFile *f = NULL;
	RETURN_IF_ERROR(OpenVlinkFile(f, folder));
	DataFile dcachefile(f);
	RETURN_IF_ERROR(dcachefile.Init());

	// Prepare the stream of urls in the file
	DataFile_Record * rec = 0;
	OpAutoPtr<DataFile_Record> url_rec(0);
	RETURN_IF_ERROR(dcachefile.GetNextRecord(rec));
	url_rec.reset(rec);

	// Read the urls out of the file:
	while(url_rec.get() != NULL)
	{
		url_rec->SetRecordSpec(dcachefile.GetRecordSpec());
		RETURN_IF_ERROR(url_rec->IndexRecords());

		// Parse the record into a URL_rep
		URL_Rep *url_rep_ptr;
		OP_STATUS status;
#ifdef SEARCH_ENGINE_CACHE
		TRAP(status, URL_Rep::CreateL(&url_rep_ptr, url_rec.get(), folder, 0));
#else
		FileName_Store dummy(1);
		RETURN_IF_ERROR(dummy.Construct());
		TRAP(status, URL_Rep::CreateL(&url_rep_ptr, url_rec.get(), dummy, folder, 0));
#endif // SEARCH_ENGINE_CACHE
		RETURN_IF_ERROR(status);

		// Create the corresponding URL object
		URL url(url_rep_ptr, (char *) NULL);
		url_rep_ptr->DecRef();

		// Get the correctly formatted url as a string
		OpString url_string;
		RETURN_IF_ERROR(url.GetAttribute(URL::KUniName_With_Fragment_Username, url_string));

		// Get the accessedtime
		time_t url_time = 0;
		url.GetAttribute(URL::KVLocalTimeVisited, &url_time, TRUE);

		// Add the item :
		RETURN_IF_ERROR(AddHistoryItem(url_string.Strip().CStr(),
									   url_string.Strip().CStr(),
									   HISTORY_ITEM_TYPE,
									   url_time,
									   TRUE, -1, NULL,
									   TRUE /* if the item exists prefer its current title */));

		// Get the next one
		RETURN_IF_ERROR(dcachefile.GetNextRecord(rec));
		url_rec.reset(rec);
	}

	// Close the file:
	dcachefile.Close();

	return OpStatus::OK;
}
#endif // URL_USE_HISTORY_VLINKS && DISK_CACHE_SUPPORT

/***********************************************************************************
 ** Resizes the list by deleting the excess from the end of the list.
 **
 ** HistoryModel::Size
 ** @param m Positive integer - new size
 ***********************************************************************************/
void HistoryModel::Size(INT32 m)
{
    if(m < 0)
		m = 0;

    m_max = m;

    if(m_max < m_modelVector->GetCount())
    {
		BroadcastModelChanging();

		while(m_modelVector->GetCount() > m_max)
		{
			HistoryPage * last_item = m_modelVector->Get(m_modelVector->GetCount()-1);
			DeleteItem(last_item);
		}

		BroadcastModelChanged();
    }
}

/***********************************************************************************
 ** LowerString
 **
 ** @param
 **
 ** @return
 ***********************************************************************************/
void LowerString(uni_char * address, char last)
{
    for( uni_char *p = address; *p && *p != last; p++)
	{
		if ( *p < 128  )
		{
			if( *p >= 'A' && *p <= 'Z' )
				*p+=0x20;
		}
		else
		{
			*p = Unicode::ToLower( *p );
		}
	}
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS HistoryModel::GetItems(const OpStringC& in_MatchString,
								 OpVector<HistoryPage>& out_vector,
								 int max_number,
								 SortType sorted)
{
	switch(sorted)
	{
		case BY_POPULARITY  :
			RETURN_IF_ERROR(GetItemsByPopularity(in_MatchString, out_vector, max_number));
			break;

		case ALPHABETICALLY :
			RETURN_IF_ERROR(GetItemsAlphabetically(in_MatchString, out_vector, max_number));
			break;

		case BY_TIME :
			RETURN_IF_ERROR(GetItemsByTime(in_MatchString, out_vector, max_number));
			break;

		default:
			OP_ASSERT(FALSE); //No sort method given
			return OpStatus::ERR;
	}

	return OpStatus::OK;
}

/***********************************************************************************
 ** Searches the list for addresses starting with in_MatchString, ignoring the prefixes
 ** listed in prefixes. Creates an array of the correct size (number of addresses * 3)
 ** and sets items[i*3] to the address, items[i*3 + 1] to accessed and items[i*3 + 2]
 ** to the collated address. The array is sorted based on the collated address and
 ** returned by setting the out_Items pointer. The size is set in out_ItemCount.
 **
 ** HistoryModel::GetItems
 ** @param in_MatchString
 ** @param out_Items
 ** @param out_ItemCount
 **
 ** @return OP_STATUS (OK or ERR_NO_MEMORY)
 ***********************************************************************************/
OP_STATUS  HistoryModel::GetItems(const OpStringC& 	in_MatchString,
								  uni_char *** 		out_Items,
								  int* 				out_ItemCount,
								  SortType 			sorted)
{

	OpVector<HistoryPage> out_vector;

	RETURN_IF_ERROR(GetItems(in_MatchString, out_vector, HISTORY_DEFAULT_MAX_GET_ITEMS, sorted));

	RETURN_IF_ERROR(CreateItemsArray(out_vector, out_Items, out_ItemCount));

	return OpStatus::OK;
}

/***********************************************************************************
 ** Searches the list for addresses starting with in_MatchString, ignoring the prefixes
 ** listed in prefixes. The result is stored in out_vector and sorted based on the address.
 **
 ** HistoryModel::GetItemsAlphabetically
 ** @param in_MatchString
 ** @param out_vector containing the results
 **
 ** @return OP_STATUS (OK or ERR_NO_MEMORY)
 ***********************************************************************************/
OP_STATUS HistoryModel::GetItemsAlphabetically(const OpStringC& in_MatchString,
											   OpVector<HistoryPage>& out_vector,
											   int max_number)
{
	RETURN_IF_ERROR(GetItemsVector(in_MatchString, out_vector));

	return OpStatus::OK;
}

/***********************************************************************************
 ** Searches the list for addresses starting with in_MatchString, ignoring the prefixes
 ** listed in prefixes. The array is sorted based on the popularity of the items.
 **
 ** HistoryModel::GetItemsByPopularity
 ** @param in_MatchString
 ** @param out_vector containing the results
 **
 ** @return OP_STATUS (OK or ERR_NO_MEMORY)
 **
 ** written by arjanl
 ***********************************************************************************/
OP_STATUS HistoryModel::GetItemsByPopularity(const OpStringC& in_MatchString,
											 OpVector<HistoryPage>& out_vector,
											 int max_number)
{
	// Find items matching in_MatchString
	RETURN_IF_ERROR(GetItemsVector(in_MatchString, out_vector));

	// Sort items by popularity

	// 0. Split the items into two groups - the ones with popularity and those without
	int split = 0; // The point where the vector will be split
	SplitItems(out_vector, split);

	// 1. Mergesort items [0..split) by popularity
	RETURN_IF_ERROR(MergeSorting<HistoryPage>::MergeSort(out_vector,
														 0,
														 split,
														 PopularityGT));

	if(max_number > 0)
	{
		// 2. Cut off unnecessary items, will adjust split if necessary
		CutOffItems(out_vector, split, (unsigned int) max_number);
	}

	// 3. Mergesort items [split..GetCount()) by access time
	RETURN_IF_ERROR(MergeSorting<HistoryPage>::MergeSort(out_vector,
														 split,
														 out_vector.GetCount(),
														 AccessTimeGT));

	return OpStatus::OK;
}

/***********************************************************************************
 ** Split the items into two groups - the ones with popularity and those without
 **
 **
 **
 **
 ***********************************************************************************/
void HistoryModel::SplitItems(OpVector<HistoryPage>& out_vector, int& split)
{
	// Put items with average_interval -1 (visited only once)
	// at the end, these shouldn't be sorted
	int i = 0;
	split = out_vector.GetCount();

	while(i != split)
	{
		// Loop invariant: (Forall x : 0 <= x < i : average_interval[x] != -1) &&
		//                 (Forall x : split <= x < out_vector.GetCount() : average_interval[x] == -1)

		HistoryPage* current_item = out_vector.Get(i);

		if(current_item->GetAverageInterval() == -1)
		{
			MergeSorting<HistoryPage>::Swap(out_vector, i, split - 1);
			split--;
		}
		else
			i++;
	}
}

/***********************************************************************************
 ** Cut off unnecessary items, will adjust split if necessary
 **
 **
 **
 **
 ***********************************************************************************/
void HistoryModel::CutOffItems(OpVector<HistoryPage>& out_vector, int& split, unsigned int max_items)
{
	if (out_vector.GetCount() > max_items)
		out_vector.Remove(max_items, out_vector.GetCount() - max_items);

	int max = (int) max_items;
	split = (split < max) ? split : max;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS HistoryModel::GetItemsByTime(const OpStringC& in_MatchString,
									   OpVector<HistoryPage>& out_vector,
									   int max_number)
{
	// Find items matching in_MatchString
	RETURN_IF_ERROR(GetItemsVector(in_MatchString, out_vector));


	// 0. Split the items into two groups - the ones with popularity and those without
	int split = 0; // The point where the vector will be split
	SplitItems(out_vector, split);

	// 1. Mergesort items [0..split) by the adjusted time
	RETURN_IF_ERROR(MergeSorting<HistoryPage>::MergeSort(out_vector,
														 0,
														 split,
														 AdjustedTimeGT));

	if(max_number > 0)
	{
		// 2. Cut off unnecessary items, will adjust split if necessary
		CutOffItems(out_vector, split, (unsigned int) max_number);
	}

	// 3. Mergesort items [split..GetCount()) by by the adjusted time
	RETURN_IF_ERROR(MergeSorting<HistoryPage>::MergeSort(out_vector,
														 split,
														 out_vector.GetCount(),
														 AdjustedTimeGT));

	return OpStatus::OK;
}


/***********************************************************************************
 ** GetServerName
 **
 ** @param
 **
 ** @return
 ***********************************************************************************/
OP_STATUS HistoryModel::GetServerName(const OpStringC& url_name, OpString & stripped, bool key)
{
	OP_STATUS status = OpStatus::OK;
	StripPrefix(url_name, stripped, status);
	RETURN_IF_ERROR(status);

	// Stripped should contain stripped version of url
	OP_ASSERT(stripped.HasContent());
	if(stripped.IsEmpty())
		return OpStatus::ERR;

	int index = stripped.FindFirstOf('/');

	if(index == 0)
	{
		// Ignore the slash if it is the first character
		index = stripped.FindFirstOf(UNI_L("/"), 1);
	}

	if(index != KNotFound)
		stripped.Delete(index);

	if(key)
		stripped.Append(UNI_L("/"));

	return OpStatus::OK;
}



/***********************************************************************************
 ** GetServerAddress
 **
 ** @param
 **
 ** @return
 ***********************************************************************************/
OP_STATUS HistoryModel::GetServerAddress(const OpStringC& address, OpString & stripped)
{
	RETURN_IF_ERROR(stripped.Set(address));

   	//Stripped should contain address
	OP_ASSERT(stripped.CStr());

	if(!stripped.CStr())
		return OpStatus::ERR_NO_MEMORY;

	PrefixMatchNode * prefix_node = 0;

	//Find longest matching prefix
	m_prefix_tree->FindPrefixInternalNode(prefix_node, address.CStr());

	INT32 prefix_num = prefix_node->GetIndex();
	INT32 prefixLen  = m_history_prefixes[prefix_num].prefix_len;

	uni_char * c = stripped.CStr() + prefixLen;

	while(*c != '\0' && *c != '/') c++;

	if(*c == '/')
		c++;

	*c = '\0';

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 **
 **
 ** TODO : This should really return an OP_STATUS and take the prefix_num as parameter
 ***********************************************************************************/
INT32 HistoryModel::StripPrefix(const OpStringC& address, OpString& stripped, OP_STATUS& status)
{
	UINT32 prefix_len = 0;
	INT32 prefix_num = GetPrefix(address, prefix_len, status);
	RETURN_VALUE_IF_ERROR(status, 0);

	status = stripped.Set(address.CStr() + prefix_len);
	return prefix_num;	
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
INT32 HistoryModel::GetPrefix(const OpStringC& address, UINT32& prefix_len, OP_STATUS& status)
{
	PrefixMatchNode * prefix_node = 0;
	status = OpStatus::ERR;
	
	
	//Find longest matching prefix
	m_prefix_tree->FindPrefixInternalNode(prefix_node, address.CStr());
	
	if(prefix_node)
	{
		INT32 prefix_num = prefix_node->GetIndex();
		prefix_len  = m_history_prefixes[prefix_num].prefix_len;
		status = OpStatus::OK;
		return prefix_num;
	}
	return 0;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
HistorySiteFolder * HistoryModel::AddServer(const OpStringC& url_name)
{
	HistoryItem ** current = 0;

	// Get the name of the folder :
	OpString server_name;
	RETURN_VALUE_IF_ERROR(GetServerName(url_name, server_name, false), NULL);

	OpString server_key;
	RETURN_VALUE_IF_ERROR(GetServerName(url_name, server_key, true), NULL);

	// Try to get an existing key from the tree :
	InternalKeyGuard<HistoryItem, HistoryPage> key_guard(m_strippedTree);
	key_guard.findKey(server_key);

	// Create your own if it's not there :
	if(!key_guard.getKey())
		RETURN_VALUE_IF_ERROR(key_guard.createKey(server_key), 0);

	// Get our insertion position :
	current = key_guard.insertKey();

	// We need our insertion position :
	if(!current)
		return 0;

	if (!(*current))
	{
		//If there wasn't a folder there - make one :
		HistorySiteFolder * server = HistorySiteFolder::Create(server_name.CStr(), key_guard.getKey());

		if(server)
			key_guard.release();

		*current = server; //If out of memory - server will be 0, this achieves the right effect
	}

	return static_cast<HistorySiteFolder*>(*current);
}


/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS HistoryModel::GetHistoryItems(OpVector<HistoryPage> &vector)
{
	OpVector<HistoryPage> items_vector;
	RETURN_IF_ERROR(m_strippedTree->FindAllLeafNodes(items_vector));

	// Only keep history items :
	for(UINT32 i = 0; i < items_vector.GetCount(); i++)
	{
		HistoryPage * page_item = items_vector.Get(i);

		if(page_item->IsInHistory())
			RETURN_IF_ERROR(vector.Add(page_item));
	}

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS HistoryModel::GetBookmarkItems(OpVector<HistoryPage> &vector)
{
	OpVector<HistoryPage> items_vector;
	RETURN_IF_ERROR(m_strippedTree->FindAllLeafNodes(items_vector));

	// Only keep bookmark items :
	for(UINT32 i = 0; i < items_vector.GetCount(); i++)
	{
		HistoryPage * page_item = items_vector.Get(i);

		if(page_item->IsBookmarked())
			RETURN_IF_ERROR(vector.Add(page_item));

	}

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS HistoryModel::GetInternalPages(OpVector<HistoryPage> &vector)
{
	return vector.DuplicateOf(*m_operaPageVector);
}


/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL HistoryModel::RemoveFolder(HistoryFolder* folder)
{
	BOOL folder_removed = TRUE; //If it is pinned you can consider it removed

	if(!folder->IsPinned() && folder->GetFolderType() == SITE_FOLDER)
	{
		folder_removed = m_strippedTree->RemoveInternalNode(folder->GetKeyString());

        // Folder should have been removed
		OP_ASSERT(folder_removed);
	}

	return folder_removed;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL HistoryModel::RemoveItem(HistoryPage* item,
							  BOOL literal_key)
{
	BOOL stripped_removed  = FALSE;

	if(!item->IsBookmarked() &&
	   !item->IsOperaPage()  &&
	   !item->IsNick())
	{
		stripped_removed  = m_strippedTree->RemoveLeafNode(item->GetStrippedAddress(), item->GetPrefixNum());
	}
	else
	{
#ifdef HISTORY_DEBUG
		OpString8 tmp;
		tmp.Set(item->GetStrippedAddress());
		printf("RemoveItem : Aborted\n");
		printf("Address    : %s\n", tmp.CStr());
		printf("Bookmarked : %d\n", item->IsBookmarked());
		printf("OperaPage  : %d\n", item->IsOperaPage());
		printf("Nick       : %d\n", item->IsNick());
#endif // HISTORY_DEBUG
	}

	if(item->IsInHistory())
	{
		if(!item->IsRemoved())
			m_modelVector->RemoveByItem(item);
		item->ClearInHistory();
	}

	return stripped_removed;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS HistoryModel::RemoveBookmark(const OpStringC& url_name)
{
	HistoryPage * bookmark_item = 0;

	OP_STATUS status = GetItem(url_name, bookmark_item);

	if(bookmark_item)
		if(!RemoveBookmark(bookmark_item))
			status = OpStatus::ERR;

	return status;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS HistoryModel::GetItem(const OpStringC & address,
								HistoryPage *& history_item)
{
	return GetItem(address, history_item, FALSE);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS HistoryModel::GetItem(const OpStringC & address,
								HistoryPage *& history_item,
								BOOL literal_key)
{
	if(address.IsEmpty())
 		return OpStatus::ERR;

	INT32 prefix_num = 0;
	UINT32 prefix_len = 0;

	if(!literal_key)
	{
		OP_STATUS status = OpStatus::OK;
		prefix_num = GetPrefix(address, prefix_len, status);
		RETURN_IF_ERROR(status);

		if(prefix_num == 0)
			return OpStatus::ERR;
	}

	history_item = m_strippedTree->FindLeafNode(address.CStr() + prefix_len, prefix_num);

	return history_item ? OpStatus::OK : OpStatus::ERR;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL HistoryModel::IsVisited(const OpStringC& url_name)
{
	HistoryPage * history_item = 0;

	if(OpStatus::IsSuccess(GetItem(url_name, history_item)) && history_item->IsInHistory())
		return history_item->IsVisited();

	return FALSE;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void HistoryModel::SetIsTyped(const OpStringC & url)
{
	HistoryPage * history_item = 0;

	if(OpStatus::IsSuccess(GetItem(url, history_item)) && history_item->IsInHistory())
	{
		history_item->SetIsTyped();
	}
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void HistoryModel::ClearIsTyped(const OpStringC & url)
{
	HistoryPage * history_item = 0;

	if(OpStatus::IsSuccess(GetItem(url, history_item)) && history_item->IsInHistory())
	{
		history_item->ClearIsTyped();
	}
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL HistoryModel::RemoveBookmark(HistoryPage * bookmark_item)
{
	if(bookmark_item && bookmark_item->GetType() == OpTypedObject::HISTORY_ELEMENT_TYPE)
	{
		if(bookmark_item->IsBookmarked())
		{
			bookmark_item->ClearBookmarked();

			// Remove the nick too :
			HistoryPage * nick_item = bookmark_item->GetAssociatedItem();
			RemoveNick(nick_item);

			m_bookmarkVector->RemoveByItem(bookmark_item);

			if(!bookmark_item->IsInHistory() &&
			   !bookmark_item->IsOperaPage() &&
			   !bookmark_item->IsNick())
			{
				DeleteItem(bookmark_item);
			}
			else
			{
#ifdef HISTORY_DEBUG
				printf("RemoveBookmark : Aborted\n");
				printf("Bookmarked : %d\n", bookmark_item->IsBookmarked());
				printf("OperaPage  : %d\n", bookmark_item->IsOperaPage());
				printf("Nick       : %d\n", bookmark_item->IsNick());
#endif // HISTORY_DEBUG
			}

			return TRUE;
		}
	}

	return FALSE;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL HistoryModel::RemoveNick(HistoryPage * nick_item)
{
	if(nick_item && nick_item->GetType() == OpTypedObject::HISTORY_ELEMENT_TYPE)
	{
		if(nick_item->IsNick())
		{
			nick_item->ClearNick();
			nick_item->ClearAssociatedItem();

			m_nickPageVector->RemoveByItem(nick_item);

			if(!nick_item->IsInHistory() &&
			   !nick_item->IsOperaPage() &&
			   !nick_item->IsBookmarked())
			{
				DeleteItem(nick_item, TRUE, TRUE);
			}
			else
			{
#ifdef HISTORY_DEBUG
				printf("RemoveNick : Aborted\n");
				printf("Bookmarked : %d\n", nick_item->IsBookmarked());
				printf("OperaPage  : %d\n", nick_item->IsOperaPage());
				printf("Nick       : %d\n", nick_item->IsNick());
#endif // HISTORY_DEBUG
			}

			return TRUE;
		}
	}

	return FALSE;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL HistoryModel::RemoveOperaPage(HistoryPage * opera_item)
{
	if(opera_item && opera_item->GetType() == OpTypedObject::HISTORY_ELEMENT_TYPE)
	{
		if(opera_item->IsOperaPage())
		{
			opera_item->ClearIsOperaPage();

			m_operaPageVector->RemoveByItem(opera_item);

			if(!opera_item->IsInHistory()  &&
			   !opera_item->IsBookmarked() &&
			   !opera_item->IsNick())
			{
				DeleteItem(opera_item);
			}
			else
			{
#ifdef HISTORY_DEBUG
				printf("RemoveOperaPage : Aborted\n");
				printf("Bookmarked : %d\n", opera_item->IsBookmarked());
				printf("OperaPage  : %d\n", opera_item->IsOperaPage());
				printf("Nick       : %d\n", opera_item->IsNick());
#endif //HISTORY_DEBUG
			}

			return TRUE;
		}
	}

	return FALSE;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void HistoryModel::MoveItem(HistoryPage* item)
{
	BroadcastItemMoving(item);
	m_modelVector->RemoveByItem(item);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS HistoryModel::AddItem(HistoryPage* item,
								HistorySiteFolder* site,
								BOOL save_session,
								HistoryItemType type)
{
	//Associate sitefolder with item
	item->SetSiteFolder(site);

	if(type == HISTORY_ITEM_TYPE)
	{
		HistoryPage* page = item;

		if(page->IsInHistory())
		{
			//If history is full - delete oldest item
			if(m_modelVector->GetCount() == m_max)
			{
				HistoryPage * last_item = m_modelVector->Get(m_max-1);
				DeleteItem(last_item);
			}

			RETURN_IF_ERROR(AddItemSorted(page));
			BroadcastItemAdded(page, save_session);
		}
	}

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS HistoryModel::AddItemSorted(HistoryPage* page)
{
	INT32 pos = BinaryTimeSearch(page->GetAccessed());
	return m_modelVector->Insert(pos, page);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS HistoryModel::FindInPrefixTrees(const OpStringC& in_MatchString,
										  OpVector<HistoryItem> &pvector,
										  OpVector<HistoryPage> &vector)
{
	BOOL found_in_prefix = TRUE;
	UINT32 i;

	//Find all prefixes matching - protocol

	//Make the string lowercase:
	OpString protocol_string;
	RETURN_IF_ERROR(protocol_string.Set(in_MatchString));
	protocol_string.MakeLower();

	//Search for the prefix:
	OpVector<PrefixMatchNode> prefix_vector;

	m_prefix_tree->FindExactInternalNodes(prefix_vector,
										  protocol_string.CStr(),
										  OpTypedObject::FOLDER_TYPE);

	if(prefix_vector.GetCount() == 0)
	{
		m_prefix_tree->FindExactLeafNodes(prefix_vector,
										  protocol_string.CStr(),
										  OpTypedObject::FOLDER_TYPE);
		found_in_prefix = FALSE;
	}

	for(i = 0; i < prefix_vector.GetCount(); i++)
	{
		PrefixMatchNode * node = prefix_vector.Get(i);

		OP_ASSERT(node);

		HistoryPrefixFolder *folder = m_protocol_array[node->GetIndex()];

		OP_ASSERT(folder);

		if(folder)
			RETURN_IF_ERROR(pvector.Add(folder));
	}

	for(i = 0; i < pvector.GetCount(); i++)
	{
		HistoryPrefixFolder *folder = (HistoryPrefixFolder*) pvector.Get(i);

		INT32 prefix_num = folder->GetIndex();
		INT32 addressLen = in_MatchString.Length();
		INT32 prefixLen  = m_history_prefixes[prefix_num].prefix_len;
		INT32 suffixLen  = m_history_prefixes[prefix_num].suffix_len;

		uni_char * stripped = 0;

		if(addressLen >= prefixLen && found_in_prefix)
		{
			// Finds these : http://www.
			INT32 len = addressLen-prefixLen;
			stripped  = OP_NEWA(uni_char, len+1);

			if(!stripped)
				break;

			uni_strcpy(stripped, &in_MatchString.CStr()[prefixLen]);
		}
		else if(addressLen >= suffixLen && !found_in_prefix)
		{
			// Finds these: www.
			INT32 len = addressLen-suffixLen;
			stripped  = OP_NEWA(uni_char, len+1);

			if(!stripped)
				break;

			uni_strcpy(stripped, &in_MatchString.CStr()[suffixLen]);
		}

		if(stripped && uni_strlen(stripped))
		{
			// Prefix is an actual prefix -> look for the rest of the string
			LowerString(stripped, '/');
			m_strippedTree->FindLeafNodes(vector,
										  stripped,
										  OpTypedObject::HISTORY_ELEMENT_TYPE,
										  prefix_num);
		}
		else if(m_history_prefixes[prefix_num].show_all_matches)
		{
			// Prefix is only a possible prefix -> find all
			m_strippedTree->FindAllLeafNodes(vector,
											 OpTypedObject::HISTORY_ELEMENT_TYPE,
											 prefix_num);
		}

		OP_DELETEA(stripped);
	}

	if(i < pvector.GetCount())
	{
		//Out of Memory - nothing to be deallocated
		return OpStatus::ERR_NO_MEMORY;
	}

    // If no items were found - there were no matching prefixes after all - this may be a stripped url
    // Should be searched for in the stripped tree.
	if(vector.GetCount() == 0)
		pvector.Clear();

	return OpStatus::OK;
}


/***********************************************************************************
 ** Searches the list for addresses starting with in_MatchString, ignoring the prefixes
 ** listed in prefixes and stores the results in out_vector. The results are sorted
 ** by concatenated address.
 **
 ** HistoryModel::GetItemsVector
 ** @param in_MatchString
 ** @param out_vector
 **
 ** @return OP_STATUS (OK or ERR_NO_MEMORY)
 ***********************************************************************************/
OP_STATUS HistoryModel::GetItemsVector(const OpStringC& in_MatchString,
									   OpVector<HistoryPage>& out_vector)
{
	OpString search_string;
	RETURN_IF_ERROR(search_string.Set(in_MatchString));
	search_string.Strip(TRUE, FALSE); //Only strip leading blanks

	if(search_string.IsEmpty())
		return OpStatus::OK;

	OpVector<HistoryItem> pvector;

	//TODO : React if status is ERR_NO_MEMORY
	OP_STATUS status = FindInPrefixTrees(search_string.CStr(), pvector, out_vector);

	if(OpStatus::IsMemoryError(status))
		return status;

	if(pvector.GetCount() == 0)//this is not a prefix search -> search the stripped tree
	{		                   //Find all stripped urls starting with search_string
		LowerString(search_string.CStr(), '/');
		m_strippedTree->FindLeafNodes(out_vector, search_string.CStr(),
									  OpTypedObject::HISTORY_ELEMENT_TYPE);
	}

	return OpStatus::OK;
}

/***********************************************************************************
 ** Copies in_vector into a new array of the correct size (number of addresses * 3)
 ** and sets items[i*3] to the address, items[i*3 + 1] to accessed and items[i*3 + 2]
 ** to the collated address. The array is returned by setting the out_Items pointer.
 ** The size is set in out_ItemCount.
 **
 ** HistoryModel::CreateItemsArray
 ** @param in_vector
 ** @param out_Items
 ** @param out_ItemCount
 **
 ** @return OP_STATUS (OK or ERR_NO_MEMORY)
 ***********************************************************************************/
OP_STATUS HistoryModel::CreateItemsArray(OpVector<HistoryPage>& in_vector,
										 uni_char *** out_Items,
										 int* out_ItemCount)
{
	//Make array that is to be returned
	uni_char ** items = OP_NEWA(uni_char*, in_vector.GetCount() * 3);

	if (items == NULL)
		return OpStatus::ERR_NO_MEMORY;

	UINT32 i;

    //Insert address and accessed for all items found
	for(i = 0; i < in_vector.GetCount(); i++)
	{

		items[i*3]     = 0;
		items[i*3 + 1] = 0;
		items[i*3 + 2] = 0;

		HistoryPage * item = in_vector.Get(i);

		/* ________________ 0. URL address _____________________ */

		OpString address;
		item->GetAddress(address);
		items[i*3] = uni_stripdup(address.CStr());

		if (!items[i*3])
			break; //Out of memory - will be caught by test after loop

		/* ________________ 1. Title ___________________________ */

		OpString title;
		item->GetTitle(title);

		items[i*3 + 1] = uni_stripdup(title.CStr());

		if (!items[i*3 + 1])
			break; //Out of memory - will be caught by test after loop

		/* ________________ 2. Not used anymore ________________ */
		items[i*3 + 2] = 0;
	}

	if(i < in_vector.GetCount())
	{
		//Out of Memory - Delete all allocated

		for(UINT32 j = 0; j <= i; j++)
		{
			OP_DELETEA(items[j*3]);
			OP_DELETEA(items[j*3+1]);
			OP_DELETEA(items[j*3+2]);
		}

		OP_DELETEA(items);

		return OpStatus::ERR_NO_MEMORY;
	}

	*out_Items		= items;
	*out_ItemCount	= in_vector.GetCount();

	return OpStatus::OK;
}

/***********************************************************************************
 ** static sorting function
 **
 **
 **
 **
 ***********************************************************************************/
BOOL HistoryModel::PopularityGT(const HistoryPage& item1, const HistoryPage& item2)
{
	return item1.GetAverageInterval() < item2.GetAverageInterval();
}

/***********************************************************************************
 ** static sorting function
 **
 **
 **
 **
 ***********************************************************************************/
BOOL HistoryModel::AccessTimeGT(const HistoryPage& item1, const HistoryPage& item2)
{
	return item1.GetAccessed() > item2.GetAccessed();
}


/***********************************************************************************
 ** static sorting function
 **
 **
 **
 **
 ***********************************************************************************/
BOOL HistoryModel::AdjustedTimeGT(const HistoryPage& item1, const HistoryPage& item2)
{
	if(item1.IsNick() != item2.IsNick())
		return item1.IsNick();

	if(item1.IsBookmarked() != item2.IsBookmarked())
		return item1.IsBookmarked();

	if(item1.IsDomain() != item2.IsDomain())
		return item1.IsDomain();

	if(item1.IsTyped() != item2.IsTyped())
		return item1.IsTyped();

	return item1.GetAccessed() > item2.GetAccessed();
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
#ifdef HISTORY_DEBUG
int HistoryModel::GetTotalPagesSize()
{
	int total_size = 0;

	for(unsigned int i = 0; i < m_modelVector->GetCount(); i++)
		total_size += m_modelVector->Get(i)->GetSize();

	return total_size;
}
#endif //HISTORY_DEBUG

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
#ifdef HISTORY_DEBUG
int HistoryModel::GetTotalKeySize()
{
	int total_size = 0;

	for(unsigned int i = 0; i < HistoryKey::GetKeys().GetCount(); i++)
		total_size += HistoryKey::GetKeys().Get(i)->GetSize();

	return total_size;
}
#endif //HISTORY_DEBUG

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
#ifdef HISTORY_DEBUG
int HistoryModel::GetTotalSiteFolderSize()
{
	int total_size = 0;

	for(unsigned int i = 0; i < HistorySiteFolder::GetSites().GetCount(); i++)
		total_size += HistorySiteFolder::GetSites().Get(i)->GetSize();

	return total_size;
}
#endif //HISTORY_DEBUG

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
#ifdef HISTORY_DEBUG
void HistoryModel::LogStats()
{
	int num_pages            = HistoryPage::m_number_of_pages;
	int num_keys             = HistoryKey::m_number_of_keys;
	int num_folders          = HistoryFolder::m_number_of_folders;
	int num_site_folders     = HistorySiteFolder::m_number_of_site_folders;
	int num_site_sub_folders = NUM_TIME_PERIODS * num_site_folders;

	int stripped_tree_num_arrays = m_strippedTree->GetNumberOfLeafNodeArrays();
	int stripped_tree_num_nodes  = m_strippedTree->GetNumberOfNodes();
	int stripped_tree_node_size  = m_strippedTree->GetNodeSize();
	int stripped_tree_size       = m_strippedTree->GetTreeSize();

	int prefix_tree_num_arrays = m_prefix_tree->GetNumberOfLeafNodeArrays();
	int prefix_tree_num_nodes  = m_prefix_tree->GetNumberOfNodes();
	int prefix_tree_node_size  = m_prefix_tree->GetNodeSize();
	int prefix_tree_size       = m_prefix_tree->GetTreeSize();

	int address_num       = HistoryPage::s_number_of_addresses;
	int address_total_len = HistoryPage::s_total_address_length;

	int title_num       = HistoryPage::s_number_of_titles;
	int title_total_len = HistoryPage::s_total_title_length;

	int total_size_site_folders   = GetTotalSiteFolderSize();
	int total_size_pages          = GetTotalPagesSize();
	int total_size_keys           = GetTotalKeySize();
	int total_size_pages_keys_and_site_folders = total_size_pages + total_size_keys + total_size_site_folders;

	printf("--------------------------------------------------------------\n");
	printf("Num pages                   : %d\n", num_pages);
	printf("Num keys                    : %d\n", num_keys);
	printf("Num folders                 : %d\n", num_folders);
	printf("Num site folders            : %d\n", num_site_folders);
	printf("Num site sub folders        : %d\n", num_site_sub_folders);
	printf("--------------------------------------------------------------\n");

	printf("m_strippedTree # arrays     : %d\n", stripped_tree_num_arrays);
	printf("m_strippedTree # nodes      : %d\n", stripped_tree_num_nodes);
	printf("m_strippedTree sizeof(node) : %d\n", stripped_tree_node_size);
	printf("Sizeof(m_strippedTree)      : %d Kb\n", stripped_tree_size / 1024);
	printf("Average node size           : %d bytes\n", stripped_tree_size / stripped_tree_num_nodes);
	printf("Average nodes per page      : %d\n", stripped_tree_num_nodes / num_pages);
	printf("--------------------------------------------------------------\n");

	printf("m_prefix_tree # arrays      : %d\n", prefix_tree_num_arrays);
	printf("m_prefix_tree # nodes       : %d\n", prefix_tree_num_nodes);
	printf("m_prefix_tree sizeof(node)  : %d\n", prefix_tree_node_size);
	printf("Sizeof(m_prefix_tree)       : %d Kb\n", prefix_tree_size / 1024);
	printf("Average node size           : %d bytes\n", prefix_tree_size / prefix_tree_num_nodes);
	printf("--------------------------------------------------------------\n");

	printf("Num stripped addresses      : %d\n", address_num);
	printf("Tot stripped addresses len  : %d\n", address_total_len);
	printf("Avg stripped addresses len  : %d\n", address_total_len / address_num);
	printf("--------------------------------------------------------------\n");

	printf("Num titles                  : %d\n", title_num);
	printf("Tot title len               : %d\n", title_total_len);
	printf("Avg title len               : %d\n", title_total_len ? title_total_len / title_num : 0);
	printf("--------------------------------------------------------------\n");

	printf("Total page size             : %d Kb\n", total_size_pages / 1024);
	printf("Avg page size               : %d\n", total_size_pages / m_modelVector->GetCount());
	printf("--------------------------------------------------------------\n");

	printf("Total key size              : %d Kb\n", total_size_keys / 1024);
	printf("Avg key size pr page        : %d\n", total_size_keys / m_modelVector->GetCount());
	printf("--------------------------------------------------------------\n");

	printf("Total site folder size      : %d Kb\n", total_size_site_folders / 1024);
	printf("Avg site folder size pr page: %d\n", total_size_site_folders / m_modelVector->GetCount());
	printf("--------------------------------------------------------------\n");

	printf("Total page size (w/key & site)     : %d Kb\n", total_size_pages_keys_and_site_folders / 1024);
	printf("Avg page size (w/key & site)       : %d\n", total_size_pages_keys_and_site_folders / m_modelVector->GetCount());
	printf("--------------------------------------------------------------\n");
}
#endif //HISTORY_DEBUG

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
#ifdef HISTORY_DEBUG
void HistoryModel::LogStatsWhenDeleted()
{
	int pages        = HistoryPage::m_number_of_pages;
	int folders      = HistoryFolder::m_number_of_folders;
	int site_folders = HistorySiteFolder::m_number_of_site_folders;
	int keys         = HistoryKey::m_number_of_keys;
	int keys_left    = HistoryKey::GetKeys().GetCount();

	printf("--------------------------------------------------------------\n");
	printf("Num pages        : %d\n", pages);
	printf("Num folders      : %d\n", folders);
	printf("Num site folders : %d\n", site_folders);
	printf("Num keys         : %d\n", keys);
	printf("Num keys         : %d\n", keys_left);
	printf("--------------------------------------------------------------\n");

	unsigned int i = 0;

	for(i = 0; i < HistoryKey::GetKeys().GetCount(); i++)
	{
		HistoryKey * key = HistoryKey::GetKeys().Get(i);
		OpString8 tmp;
		tmp.Set(key->GetKey());
		printf("Key : %d \t\t %s\n", key->NumberOfReferences(), tmp.CStr());
	}

	for(i = 0; i < HistorySiteFolder::GetSites().GetCount(); i++)
	{
		HistorySiteFolder * site = HistorySiteFolder::GetSites().Get(i);
		OpString title;
		site->GetTitle(title);
		OpString8 tmp;
		tmp.Set(title.CStr());
		printf("Empty : %d \t\t %s\n", site->IsEmpty(), tmp.CStr());
	}
}
#endif //HISTORY_DEBUG

#endif // HISTORY_SUPPORT
