/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Luca Venturi
**
** Some classes used to manage opera:cache, listing the URLs in the cache
*/


#ifndef CACHE_LIST_H
#define CACHE_LIST_H

#ifndef NO_URL_OPERA
class CacheListStats;

#ifdef CACHE_ADVANCED_VIEW
	#include "modules/pi/OpLocale.h"

	/// Class notified of a list of cache entries
	class CacheEntriesListener
	{
	public:
		// Virtual destructor
		virtual ~CacheEntriesListener() { }
		
		/**
		  Called before the first entry
		  @param document URL that could be used to write something (if allowed)
		*/
		virtual OP_STATUS ListStart(URL document)=0;
		/** Called for each entry
			@param url_rep URL to list
			@param url_shown number of URLs shown so far
			@param link_url TRUE to link the URL (disabled for some views)
		*/
		virtual OP_STATUS ListEntry(URL_Rep *url_rep, int url_shown, BOOL link_url)=0;
		/** Called after the last entry
		    @param list_stats statistics related to URL usage
			@param url_shown URL shown
			@param url_total Total number of URL processed
			@param num_no_ds Number of URLs without Data Storage
			@param num_no_ds Number of URLs without Cache Storage
			@param num_200 Number of URLs with HTTP Response Code 200 (HTTP OK)
			@param num_0 Number of URLs with HTTP Response Code 0...
			@param num_404 Number of URLs with HTTP Response Code 404 (HTTP NOT Found)
			@param num_301_307 Number of URLs with HTTP Response Code 301 or 307 (HTTP Moved Permanently / HTTP Temporary redirect)
		*/
		virtual OP_STATUS ListEnd(CacheListStats &list_stats, int url_shown, int url_total, int num_no_ds, int num_no_cs, int num_no_http, int num_200, int num_0, int num_404, int num_301_307, int num_204)=0;
	};
	
	class FilterRules;

	/// Implements the List View
	class CacheListView: public CacheEntriesListener
	{
	public:
		CacheListView(FilterRules *filter_rules) { filter=filter_rules; loc=NULL; }
		virtual ~CacheListView() { OP_DELETE(loc); }
		// Second phase constructor
		OP_STATUS Construct(const OpString &drstr_address, const OpString &drstr_size);
		OP_STATUS ListStart(URL document);
		OP_STATUS ListEntry(URL_Rep *url_rep, int url_shown, BOOL link_url);
		OP_STATUS ListEnd(CacheListStats &list_stats, int url_shown, int url_total, int num_no_ds, int num_no_cs, int num_no_http, int num_200, int num_0, int num_404, int num_301_307, int num_204);
		
	protected:
		// Buffer used to construct the HTML
		TempBuffer dom_list;
		// Temporary string used for the legth
		OpString8 temp_len_str;
		// Temporary string used for the description
		OpString temp_desc;
		// URL used to show the result ("opera:cache")
		URL url;
		// Rules used to filter the URLs
		FilterRules *filter;
		// Translation of "Address"
		OpString address;
		// Translation of "Size"
		OpString size;
		// Locale used to format the dates
		OpLocale *loc;
		
		// Write the scripts (if required)
		virtual OP_STATUS WriteScripts() { return OpStatus::OK; }
		// Write the scripts (if required)
		virtual OP_STATUS WriteTableHeader();
		// Write additional data for the entry
		virtual OP_STATUS WriteAdditionalData(URL_Rep *url_rep, int url_shown, const uni_char *name, const uni_char *desc) { return OpStatus::OK; }
	};

	/// Implements the Preview View
	class CachePreviewView: public CacheListView
	{
	public:
		CachePreviewView(FilterRules *filter_rules): CacheListView(filter_rules) { }
		OP_STATUS Construct(const OpString &drstr_address, const OpString &drstr_size, const OpString &drstr_preview);
		
	protected:
		OP_STATUS WriteScripts();
		OP_STATUS WriteTableHeader();
		OP_STATUS WriteAdditionalData(URL_Rep *url_rep, int url_shown, const uni_char *name, const uni_char *desc);
		
		// Translation of "Preview"
		OpString preview;
	};
	
	/// Save each file in a predefined path
	class CacheSaveView: public CacheEntriesListener
	{
	public:
		OP_STATUS ListStart(URL document);
		OP_STATUS ListEntry(URL_Rep *url_rep, int url_shown, BOOL link_url);
		OP_STATUS ListEnd(CacheListStats &list_stats, int url_shown, int url_total, int num_no_ds, int num_no_cs, int num_no_http, int num_200, int num_0, int num_404, int num_301_307, int num_204)  { return OpStatus::OK; }
		/**
			Set the directory path
			@param directory Path of the export destination (without trailing slash)
		*/
		OP_STATUS SetDirectory(OpString *directory) { OP_ASSERT(directory); if(!directory) return OpStatus::ERR_NULL_POINTER; RETURN_IF_ERROR(path.Set(directory->CStr())); return path.Append(PATHSEP); }
		
	protected:
		OpString path; // Path where to save the files
		UINT32 time;   // Time when the saving started... to create unique names
	};
#endif

class Context_Manager;
class CacheListWriter;

/// Types of URL to show
enum URLs_To_Show
{
	// Shows the URLs normally considered in the cache (also temporary ones), of more than 0 bytes
	SHOW_NORMAL_URLS,
	// Shows only pending URLs (loading and with size of 0, but with a storage)
	SHOW_PENDING_URLS,
	// Shows only unique URLs (also of 0 bytes, but with a storage)
	SHOW_UNIQUE_URLS,
	// Shows all the URLs known to the cache
	SHOW_ALL_URLS,
	// Shows all the URLs known to the cache, except local files
	SHOW_ALL_URLS_NO_FILES,
	// Shows all the URLs known to the cache, that are HTTPS
	SHOW_ALL_URLS_HTTPS,
	// Shows all the URLs in use (depending on show_num, the behavior changes: -1: in use, 0: not in use, 1: used count==1, 2: used count>1 )
	SHOW_ALL_USED_URLS,
	// Shows all the URLs referenced (depending on show_num, the behavior changes: -1: referenced, 0: not referenced, 1: reference count==1, 2: reference count>1 )
	SHOW_ALL_REF_URLS,
	// Shows all the URLs with the given cache type (as specified by show_num)
	SHOW_ALL_TYPE_URLS
};

/// Statistics useful for opera:cache
class CacheListStats
{
private:
	friend class CacheListWriter;

	CacheListStats() { num = used_1 = used_2_and_more = ref_1 = ref_2_and_more = unique = pending = https = type_mem = type_mem_persistent = type_disk = type_temp = type_user = type_stream = type_mhtml = type_nothing = type_0 = persistent = 0; }

	/// Number of URLs with a used count of 1
	UINT32 used_1;
	/// Number of URLs with a used count of 2 or more
	UINT32 used_2_and_more;
	/// Number of URLs with a reference count of 1
	UINT32 ref_1;
	/// Number of URLs with a reference count of 2 or more
	UINT32 ref_2_and_more;
	/// Number of URL unique
	UINT32 unique;
	/// Number of HTTPS URLs
	UINT32 https;
	/// Number of URL pending (typically still loading, but also of zero bytes)
	UINT32 pending;
	/// Number of URLs marked as URL_CACHE_MEMORY
	UINT32 type_mem;
	/// Number of URLs marked as URL_CACHE_MEMORY and persistent (not a nice combination...)
	UINT32 type_mem_persistent;
	/// Number of URLs marked as URL_CACHE_DISK
	UINT32 type_disk;
	/// Number of URLs marked as URL_CACHE_TEMP
	UINT32 type_temp;
	/// Number of URLs marked as URL_CACHE_USERFILE
	UINT32 type_user;
	/// Number of URLs marked as URL_CACHE_STREAM
	UINT32 type_stream;
	/// Number of URLs marked as URL_CACHE_MHTML
	UINT32 type_mhtml;
	/// Number of URLs marked as URL_CACHE_NO
	UINT32 type_nothing;
	/// Number of URLs with type 0... this is not nice, but it can happen...
	UINT32 type_0;
	/// Number of URLs that claim to have a persistent storage
	UINT32 persistent;
	/// Total number of URLs
	UINT32 num;
};

/// Class used to generate the "advanced view" content of opera:cache
/// For chained cache, this will limit the view to the first level, but as deeper level could require asynchronous operations,
/// this it's probably the best thing to do.
// One idea would be to give access to one level at a time, assuming that it is important from a debugging point of view.
// In this way, no asynchronous operation are required but one that will provide the complete result
class CacheListWriter
{
private:
	friend class CacheListView;

	/// Context_Manager under analysis
	Context_Manager *ctx_man;
public:
	/// Default constructor
	CacheListWriter(Context_Manager *ctx = NULL);

	void SetContextManager(Context_Manager *ctx) { ctx_man=ctx; }

#ifdef CACHE_GENERATE_ARRAY
	/// Generate a list of the URLs in the cache
	OP_STATUS		GenerateCacheArray(uni_char** &filename, uni_char** &location, int* &size, int &num_items);
#endif
#if !defined(NO_URL_OPERA) 
	/// Write the information presented by opera:cache
	OP_STATUS		GenerateCacheList(URL& url);
#endif
	/// Generate statistics about the cache
	OP_STATUS WriteStatistics(CacheListStats &list_stats, URL& url);


private:
	/// Function used to understand if a URL qualifies for being shown.
	/// @param url_rep URL to check
	/// @param is_temp if !=NULL, it will be set to TRUE if the URL is a temporary one (we had to make them visible because of our QAs).
	///        this variable is not set for pending URLs
	/// @param urls_to_show Types of URLs to show (@see URLs_To_Show)
	/// @return Returns TRUE if the URL can be shown in the list
	BOOL IsUrlToShowInCacheList(URL_Rep *url_rep, BOOL *is_temp, URLs_To_Show urls_to_show);

	/// Update the cache statistics with the data of the supplied URL_Rep
	static void UpdateCacheListStats(CacheListStats &list_stats, URL_Rep *url_rep);
	/// Append the cache statistics to a string
	static void WriteCacheListStats(OpString &stats, CacheListStats &list_stats, BOOL show_links);

#ifdef CACHE_ADVANCED_VIEW
private:
#ifdef _DEBUG
	CacheSaveView save_view;	///< Debug implementation of export in opera:cache
#endif
	OpString random_key_priv;	///< Random key used to authenticate the privileged operations (list and preview)
	CacheEntriesListener *export_listener; ///< Listener used to export the files of the cache

public:

	/// Generate the "advanced view" summary of the cache contents
	OP_STATUS WriteAdvancedViewSummary(URL& url, OpStringHashTable<DomainSummary> *domains);
	/// Generate the "advanced view" detail page, with the preview of the images
	/// PROPAGATE: NEVER
	OP_STATUS WriteAdvancedViewDetail(URL& url, BOOL &detail_mode, const OpString &drstr_address, const OpString &drstr_size, const OpString &drstr_filename, const OpString &drstr_preview);
	/// Sort the domains
	/// PROPAGATE: NEVER
	void SortDomains(OpVector<DomainSummary> *domains);
	// Set the export listener (it enables the export with filters in opera:cache)
	/// PROPAGATE: NEVER
	void SetExportListener(CacheEntriesListener *listener) { export_listener=listener; }
	// Get the export listener
	/// PROPAGATE: NEVER
	CacheEntriesListener * GetExportListener(CacheEntriesListener *listener) { return export_listener; }
	/// Compute a validation code used to authorize the action; for now, it is just a random string action-dependent
	/// PROPAGATE: NEVER
	OP_STATUS ComputeValidation(const OpString8 &action, OpString &validation);
	/// Check the validation code to authorize the action
	/// PROPAGATE: NEVER
	OP_STATUS CheckValidation(const OpString8 &action, const OpString &validation, BOOL &ok);
#endif // CACHE_ADVANCED_VIEW
};

#endif // #ifndef NO_URL_OPERA

#endif //CACHE_LIST_H
