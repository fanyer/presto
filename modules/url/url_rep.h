/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/


#ifndef _URL_REP_H_
#define _URL_REP_H_

class URL_Rep;
class URL_RelRep;
class URL_ProtocolData;
class URL_HTTP_ProtocolData;
class URL_FTP_ProtocolData;
class URL_MIME_ProtocolData;
class URL_Manager;
class Cache_Manager;
class CacheLimit;
class URL_LoadHandler;
//class FramesDocument;
class MemoryManager;
class DocumentManager;
class HTTP_Request;
class DataFile_Record;
class URL_DataDescriptor;
class Cache_Storage;
class URL_DataStorage;
class Cookie;
class HTTP_Link_Relations;
class FileName_Store;
//class ServerName;
class AuthElm;
class AsyncExporterListener;
struct DebugUrlMemory;
class AuthenticationInterface;
struct authdata_st;
class MsgHndlList;
#ifdef WEBSOCKETS_SUPPORT
class OpWebSocket;
#endif //WEBSOCKETS_SUPPORT

#include "modules/hardcore/mem/mem_man.h"
#include "modules/util/simset.h"

#include "modules/url/url_name.h"
#include "modules/url/protocols/http_met.h"

#include "modules/url/url_brel.h"

#ifdef URL_ENABLE_ASSOCIATED_FILES
#include "modules/cache/AssociatedFile.h"
#endif

#ifdef CACHE_FAST_INDEX
	class DiskCacheEntry;
#endif
	
#ifdef APPLICATION_CACHE_SUPPORT
class ApplicationCache;
#endif // APPLICATION_CACHE_SUPPORT


class URL_Rep : public HashedLink
{
friend class URL;
friend class URL_Manager;
friend class URL_Store;
friend class NewsConnection;
friend class URL_DataStorage;
friend class Cache_Storage;
friend class CacheLimit;
friend class Cache_Manager;
friend class Context_Manager;
friend class Context_Manager_Base;
friend class UrlModule;
#ifdef SELFTEST
friend class ST_urlenum;
#endif

#ifdef SELFTEST
	friend class CacheTester;
#endif

private:
#if defined URL_USE_UNIQUE_ID
	URL_ID		url_id;
#endif
	URL_Name			name;
	struct rep_info_st {
		UINT	content_type:6; // Update debug assert/LEAVE in URL_Manager::InitL when changing this size
		UINT	status:3;
		UINT	unique:1;
		UINT	reload:1;
		UINT	override_redirect_disabled:1; // TRUE if Cookies, authentication, redirect is disabled
		UINT	is_forms_request:1;
		UINT	have_form_password:1;
		UINT	have_authentication:1;
		UINT	visited_checked:1;	// set to TRUE when visited has been checked once to avoid checking multiple times, cleared when visited is set.
		UINT	bypass_proxy:1;
#ifdef WEB_TURBO_MODE
		UINT	uses_turbo:1;
#endif // WEB_TURBO_MODE
	}info;
	time_t				last_visited;
	unsigned int		reference_count;
	int used;

	URL_DataStorage*	storage;
	RelRep_Store relative_names;

	AuthenticationInterface	*auth_interface;
#ifdef WEBSOCKETS_SUPPORT
	OpWebSocket *websocket;
#endif //WEBSOCKETS_SUPPORT

#ifdef	CACHE_STATS
		// Number of times that the file has been accessed from the disk
		UINT32 access_disk;
		// Number of times that the file has been accessed from another source (e.g. memory)
		UINT32 access_other;
#endif
#if defined(MHTML_ARCHIVE_REDIRECT_SUPPORT) && defined(URL_SEPARATE_MIME_URL_NAMESPACE)
	AutoDeleteHead cleanup_items;
#endif

protected:
	URL_Rep();
	void PreDestruct();
private:

	OP_STATUS Construct(URL_Name_Components_st &url);
#ifdef DISK_CACHE_SUPPORT
#ifndef SEARCH_ENGINE_CACHE
	void ConstructL(URL_Name_Components_st &url, DataFile_Record *, FileName_Store &filenames, OpFileFolder folder);
	#ifdef CACHE_FAST_INDEX
		void ConstructL(URL_Name_Components_st &url, DiskCacheEntry *, FileName_Store &filenames, OpFileFolder folder);
	#endif
#else
	void ConstructL(URL_Name_Components_st &url, DataFile_Record *, OpFileFolder folder);
	#ifdef CACHE_FAST_INDEX
		void ConstructL(URL_Name_Components_st &url, DiskCacheEntry *, OpFileFolder folder);
	#endif
#endif
#endif
public:
	/**
	 * Creates and initializes an URL_Rep object.
	 *
	 * @param url_rep  Set to the created object if successful and to
	 *                 NULL otherwise.
	 *                 DON'T use this as a way to check for errors, check the
	 *                 return value instead!
	 * @param url
	 *
	 * @return OP_STATUS - Always check this.
	 */
	static void CreateL(URL_Rep **url_rep, URL_Name_Components_st &url, URL_CONTEXT_ID context_id)
	{ LEAVE_IF_ERROR(Create(url_rep, url, context_id));	}
#ifdef DISK_CACHE_SUPPORT
	static void CreateL(URL_Rep **url_rep, DataFile_Record *rec,
#ifndef SEARCH_ENGINE_CACHE
			 FileName_Store &filenames,
#endif
			 OpFileFolder folder
					, URL_CONTEXT_ID context_id
					);
#ifdef CACHE_FAST_INDEX
			static void CreateL(URL_Rep **url_rep, DiskCacheEntry *entry,
	#ifndef SEARCH_ENGINE_CACHE
				 FileName_Store &filenames,
	#endif
				 OpFileFolder folder
						, URL_CONTEXT_ID context_id
						);
#endif
#endif
	static OP_STATUS Create(URL_Rep **url_rep, URL_Name_Components_st &url, URL_CONTEXT_ID context_id);

	virtual			~URL_Rep();

#ifndef RAMCACHE_ONLY
	void				WriteCacheDataL(DataFile_Record *rec);
	#ifdef CACHE_FAST_INDEX
	void				WriteCacheDataL(DiskCacheEntry *entry);
	#endif
#endif

#ifdef _OPERA_DEBUG_DOC_
	int					GetMemUsed();
#endif

	BOOL				HasBeenVisited();

#ifdef APPLICATION_CACHE_SUPPORT
	/**
	 * Checks if the url was loaded from an application cache,
	 * and if it has a fallback. Should only be called when 
	 * loading fails, to redirect the url to the fallback
	 * given by the application cache fallback.
	 *
	 * If it had an fallback url, this function sets the fallback url
	 * attribute and broadcasts the URL_LOADED message.
	 * 
	 * @param 	had_fallback(out) True if this url has a fallback. 
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY
	 */
	OP_STATUS CheckAndSetApplicationCacheFallback(BOOL &had_fallback);
#endif // APPLICATION_CACHE_SUPPORT
	
private:
	URL_RelRep *		GetRelativeId(const OpStringC8 &rel);
	void				RemoveRelativeId(const OpStringC8 &rel);
	OP_STATUS			CreateStorage();
	/** ONLY used by the constructor */
	BOOL				CheckStorage(OP_STATUS &op_err);
	OP_STATUS			ExtractExtension(OpString &source, OpString &target) const;
	/// Returns an estimation of the bytes occupied by the URL object;
	/// NOTE: The lenth of the URL string can be significant for data://
	OpFileLength GetURLObjectSize() { return GetAttribute(URL::KName).Length() + GetAttribute(URL::KQuery).Length() + CACHE_URL_OBJECT_SIZE_ESTIMATION; }

#ifdef SCOPE_RESOURCE_MANAGER
	/**
	 * Helper method for extracting the DocumentManager and Window from a message handler.
	 * If the message handler is NULL then it will try to get the first message handler in the URL_DataStorage object.
	 */
	void				GetLoadOwner(MessageHandler *mh, DocumentManager *&docman, Window *&window);
#endif // SCOPE_RESOURCE_MANAGER
public:
	void				SetAuthInterface(AuthenticationInterface *ai) { auth_interface = ai; }

#ifdef WEBSOCKETS_SUPPORT
	void				SetWebSocket(OpWebSocket *ws) { websocket = ws; }
	OpWebSocket*		GetWebSocket() const { return websocket; }
#endif //WEBSOCKETS_SUPPORT

	BOOL				CheckStorage();
#ifdef STRICT_CACHE_LIMIT
    CommState           Reload(MessageHandler* mh);
#endif
	CommState			Reload(MessageHandler* mh, const URL& referer_url, BOOL only_if_modified, BOOL proxy_no_cache, BOOL user_initiated, BOOL thirdparty_determined, EnteredByUser entered_by_user, BOOL inline_loading, BOOL* allow_if_modified);
	CommState			Load(MessageHandler* mh, const URL& referer_url, BOOL user_initiated, BOOL thirdparty_determined, BOOL inline_loading);
	CommState			ResumeLoad(MessageHandler* mh, const URL& referer_url);
	BOOL				QuickLoad(BOOL guess_content_type); // Only for File, or already loaded

	// Callbacks for start and end of a URL load

#ifdef SCOPE_RESOURCE_MANAGER
	/**
	 * Returns TRUE if the current URL type must report events (ie. On* calls), FALSE otherwise.
	 */
	BOOL				IsEventRequired() const;
#endif // SCOPE_RESOURCE_MANAGER

	/**
	 * Must be called when loading of a URL has started.
	 * This will trigger callbacks to systems that need to know about this event.
	 *
	 * This is currently used by the resource manager in scope, @see OpScopeResourceListener
	 */
	void				OnLoadStarted(MessageHandler* mh=NULL);

	/**
	 * Must be called when a URL has been redirected to another URL.
	 * This will trigger callbacks to systems that need to know about this event.
	 *
	 * This is currently used by the resource manager in scope, @see OpScopeResourceListener
	 */
	void				OnLoadRedirect(URL_Rep *redirect, MessageHandler* mh=NULL);

	/**
	 * Must be called when loading of a URL has finished.
	 * This will trigger callbacks to systems that need to know about this event.
	 *
	 * This is currently used by the resource manager in scope, @see OpScopeResourceListener
	 */
	void				OnLoadFinished(URLLoadResult result, MessageHandler* mh=NULL);

	URL_DataDescriptor* GetDescriptor(MessageHandler *mh, URL::URL_Redirect follow_ref, BOOL get_raw_data = FALSE, BOOL get_decoded_data=TRUE,
				Window *window = NULL, URLContentType override_content_type = URL_UNDETERMINED_CONTENT, unsigned short override_charset_id = 0, BOOL get_original_content=FALSE, unsigned short parent_charset_id = 0);


	BOOL				Expired(BOOL inline_load, BOOL user_setting_only);
	BOOL				Expired(BOOL inline_load, BOOL user_setting_only, INT32 maxstale, INT32 maxage);

	#if CACHE_SMALL_FILES_SIZE>0
		BOOL IsEmbedded();
	#endif

	URL_ID				GetID() const {
#if defined URL_USE_UNIQUE_ID
							return url_id;
#else
							return ((URL_ID) (UINTPTR) this);
#endif
						}  // This is also done in URL::Id()
	virtual URL_CONTEXT_ID GetContextId() const	{return 0;}

	/** Returns a pointer to a string which contains the full URL with
	  * the option to remove username and password components from the
	  * string or to sensor them.
	  * @param password_hide defines the state of the username and password in the returned string
	  */
	const char *Name(Password_Status password_hide) const; // content of buffer destroyed on next call
	const uni_char *UniName(Password_Status password_hide) const; // content of buffer destroyed on next call

	unsigned long 	SaveAsFile(const OpStringC &file_name);
	OP_STATUS				LoadToFile(const OpStringC &file_name);
	
	/** Save the document as the named file. Does not remove encoding. The cache object is unchanged (it uses AccesReadOnly()).
	 *  This is the method of choice for Multimedia Cache, but it enable only complete export (so if something is missing, an error is retrieved)
	 *
	 *	@param	file_name	Location to save the document
	 *	@return unsigned long	error code,0 if successful
	 */
	OP_STATUS ExportAsFile(const OpStringC &file_name) ;

	/** Checks if ExportAsFile() is allowed (it means than the file is completely available, 
	    and it is particularly important for the Multimedia cache)

		@return TRUE if ExportAsFile() can be called
	*/
	BOOL IsExportAllowed();

#ifdef PI_ASYNC_FILE_OP
	/** Asynchronously save the document as the named file. Does not decode the content. The cache object is unchanged (it uses AccesReadOnly()).
	 *  This is the method of choice for Multimedia Cache, but it enable only complete export (so if something is missing, an error is retrieved).
	 *  Note that in case of Unsupported operation, URL::SaveAsFile() will be attempted, if requested.
	 *
	 *	@param file_name File where to save the document
	 *	@param listener Listener that will be notified of the progress (it cannot be NULL)
	 *	@param param optional user parameter that will be passed to the listener
	 *	@param delete_if_error TRUE to delete the target file in case of error
	 *  @param safe_fall_back If TRUE, in case of problems, URL::SaveAsFile() will be attempted.
	 *                        This should really be TRUE, or containers and embedded files will fail... and encoded files... and more...
	 *	@return In case of OpStatus::ERR_NOT_SUPPORTED, URL::SaveAsFile() can be attempted.
	 */
	OP_STATUS ExportAsFileAsync(const OpStringC &file_name, AsyncExporterListener *listener, UINT32 param=0, BOOL delete_if_error=TRUE, BOOL safe_fall_back=TRUE);
#endif // PI_ASYNC_FILE_OP

	void WriteDocumentDataFinished();
	void WriteDocumentDataSignalDataReady();

	// len is number of uni_char elements
	OP_STATUS			WriteDocumentData(URL::URL_WriteDocumentConversion conversion, const OpStringC &data, unsigned int len= (unsigned int) KAll);
	OP_STATUS			WriteDocumentData(URL::URL_WriteDocumentConversion conversion, const OpStringC8 &data, unsigned int len= (unsigned int) KAll);
	OP_STATUS			WriteDocumentData(URL::URL_WriteDocumentConversion conversion, URL_DataDescriptor *data, unsigned int len= (unsigned int) KAll);

#ifdef URL_ENABLE_ASSOCIATED_FILES
	AssociatedFile *CreateAssociatedFile(URL::AssociatedFileType type);
	AssociatedFile *OpenAssociatedFile(URL::AssociatedFileType type);
#endif

	BOOL				GetIsFollowed() { return last_visited != 0; }
	void				SetIsFollowed(BOOL is_followed);

	unsigned long 		HandleError(unsigned long error);

	URL_DataStorage *	GetDataStorage() { return storage; }

	void				SetLocalTimeVisited(time_t lta);
	//time_t				GetLocalTimeVisited() { return last_visited; }
	void				Access(BOOL use_inline);
	BOOL				GetHasCheckedVisited() { return info.visited_checked; };
	void				SetHasCheckedVisited(BOOL checked) { info.visited_checked = checked; };

	OP_STATUS			GetSuggestedFileName(OpString &target) const;
	OP_STATUS			GetSuggestedFileNameExtension(OpString &target) const;

	void 	IncUsed(int i){used += i;};
	void	DecUsed(int i);
#ifdef URL_ACTIVATE_URL_LOAD_RESERVATION_OBJECT
	/** Increment Loading and Used count of this URL to block (re)loading and unloading
	 *
	 *	@return		OP_STATUS	Will fail in case of OOM, and error is returned
	 */
	OP_STATUS			IncLoading();

	/** Decrement Loading and Used count of this URL to unblock (re)loading and unloading */
	void			DecLoading();
#endif

	int		GetUsedCount() { return used; };
	void 	IncRef(){reference_count ++;};
	unsigned int		DecRef(){return (reference_count ? --reference_count : 0);};
	unsigned int		GetRefCount() { return reference_count; };

	virtual	const char* LinkId();

	BOOL				IsFollowed();

	void				StopLoading(MessageHandler* mh);
#ifdef DYNAMIC_PROXY_UPDATE
	void				StopAndRestartLoading();
#endif // DYNAMIC_PROXY_UPDATE
	void				RemoveMessageHandler(MessageHandler *old_mh);

	BOOL				DumpSourceToDisk(BOOL force_file = FALSE) ;
	unsigned long		PrepareForViewing(URL::URL_Redirect follow_ref, BOOL get_raw_char_data, BOOL get_decoded_data, BOOL force_to_file=FALSE);

	BOOL				FreeUnusedResources(double cached_system_time_sec=0.0) ;

	void				Unload();

	MessageHandler*		GetFirstMessageHandler() ;
	void				ChangeMessageHandler(MessageHandler* old_mh, MessageHandler* new_mh);

#ifdef _EMBEDDED_BYTEMOBILE
	void				SetPredicted(BOOL val, int depth);
#endif // _EMBEDDED_BYTEMOBILE
#if defined(_EMBEDDED_BYTEMOBILE) || defined(WEB_TURBO_MODE)
	void				CheckBypassFilter();
#endif // _EMBEDDED_BYTEMOBILE || WEB_TURBO_MODE

	OP_STATUS			SetHTTP_Data(const char* data, BOOL new_data, BOOL with_headers) ;
#ifdef HAS_SET_HTTP_DATA
	void				SetHTTP_Data(Upload_Base* data, BOOL new_data);
#endif

#ifdef HAS_SET_HTTP_DATA
	void				SetMailData(Upload_Base *data);
#endif
	BOOL				PrefetchDNS();
#ifdef _ENABLE_AUTHENTICATE
	void				Authenticate(AuthElm *auth_elm);
	BOOL				CheckSameAuthorization(AuthElm *auth_elm, BOOL proxy);
	void				FailAuthenticate(int mode);
	authdata_st*		GetAuthData();
	MsgHndlList*		GetMessageHandlerList();
#endif

	BOOL				DetermineThirdParty(URL &referrer);

#ifdef _MIME_SUPPORT_
	BOOL			GetAttachment(int i, URL &url);
	BOOL			IsMHTML() const;
	URL				GetMHTMLRootPart();
#endif
	void			CopyAllHeadersL(HeaderList& header_list_copy) const;

#ifdef _OPERA_DEBUG_DOC_
	void				GetMemUsed(DebugUrlMemory &debug);
#endif

	OpStringS8      *GetHTTPEncoding();

	uint32 GetAttribute(URL::URL_Uint32Attribute attr, URL::URL_Redirect follow_ref = URL::KNoRedirect) const;
	const OpStringC8 GetAttribute(URL::URL_StringAttribute attr, URL::URL_Redirect follow_ref = URL::KNoRedirect, URL_RelRep* rel_rep=NULL) const;
	const OpStringC GetAttribute(URL::URL_UniStringAttribute attr, URL::URL_Redirect follow_ref = URL::KNoRedirect, URL_RelRep* rel_rep=NULL) const;
	OP_STATUS		GetAttribute(URL::URL_StringAttribute attr, OpString8 &val, URL::URL_Redirect follow_ref = URL::KNoRedirect, URL_RelRep* rel_rep=NULL) const;
	OP_STATUS		GetAttribute(URL::URL_StringAttribute attr, OpString &val, URL::URL_Redirect follow_ref = URL::KNoRedirect, URL_RelRep* rel_rep=NULL) const;
	OP_STATUS		GetAttribute(URL::URL_UniStringAttribute attr, OpString &val, URL::URL_Redirect follow_ref = URL::KNoRedirect, URL_RelRep* rel_rep=NULL) const;
	void			GetAttributeL(URL::URL_StringAttribute attr, OpString8 &val, URL::URL_Redirect follow_ref = URL::KNoRedirect, URL_RelRep* rel_rep=NULL) const;
	void			GetAttributeL(URL::URL_UniStringAttribute attr, OpString &val, URL::URL_Redirect follow_ref = URL::KNoRedirect, URL_RelRep* rel_rep=NULL) const;
	const void *GetAttribute(URL::URL_VoidPAttribute  attr, const void *param, URL::URL_Redirect follow_ref = URL::KNoRedirect) const;

	OP_STATUS		GetAttribute(URL::URL_NameStringAttribute attr, OpString8 &val, URL::URL_Redirect follow_ref = URL::KNoRedirect, URL_RelRep* rel_rep=NULL) const;
	OP_STATUS		GetAttribute(URL::URL_UniNameStringAttribute attr, unsigned short charsetID, OpString &val, URL::URL_Redirect follow_ref = URL::KNoRedirect, URL_RelRep* rel_rep=NULL) const;
	const OpStringC8 GetAttribute(URL::URL_NameStringAttribute attr, URL::URL_Redirect follow_ref = URL::KNoRedirect, URL_RelRep* rel_rep=NULL) const;
	const OpStringC GetAttribute(URL::URL_UniNameStringAttribute attr, URL::URL_Redirect follow_ref = URL::KNoRedirect, URL_RelRep* rel_rep=NULL) const;

	URL GetAttribute(URL::URL_URLAttribute  attr, URL::URL_Redirect follow_ref = URL::KNoRedirect);

	//URL GetAttribute(URL::URL_URLAttribute  attr, BOOL follow_ref){return GetAttribute(attr, (follow_ref ? URL::KFollowRedirect : URL::KNoRedirect));};
	uint32 GetAttribute(URL::URL_Uint32Attribute attr, BOOL follow_ref) const{return GetAttribute(attr, (follow_ref ? URL::KFollowRedirect : URL::KNoRedirect));};
/*
	const OpStringC8 GetAttribute(URL::URL_StringAttribute attr, BOOL follow_ref, URL_RelRep* rel_rep=NULL) const;
	const OpStringC GetAttribute(URL::URL_UniStringAttribute attr, BOOL follow_ref, URL_RelRep* rel_rep=NULL) const;
	void			GetAttributeL(URL::URL_StringAttribute attr, OpString8 &val, BOOL follow_ref, URL_RelRep* rel_rep=NULL) const;
	void			GetAttributeL(URL::URL_UniStringAttribute attr, OpString &val, BOOL follow_ref, URL_RelRep* rel_rep=NULL) const;
	const void *GetAttribute(URL::URL_VoidPAttribute  attr, const void *param, BOOL follow_ref ) const;*/

	OP_STATUS SetAttribute(URL::URL_Uint32Attribute attr, uint32 value);
	OP_STATUS SetAttribute(URL::URL_StringAttribute attr, const OpStringC8 &string);
	OP_STATUS SetAttribute(URL::URL_UniStringAttribute attr, const OpStringC &string);
	OP_STATUS SetAttribute(URL::URL_UniNameStringAttribute attr, const OpStringC &string) { return name.SetAttribute(attr, string); }
	OP_STATUS SetAttribute(URL::URL_VoidPAttribute  attr, const void *param);
	OP_STATUS SetAttribute(URL::URL_URLAttribute  attr, const URL &param);

	void SetAttributeL(URL::URL_Uint32Attribute attr, uint32 value);
	void SetAttributeL(URL::URL_StringAttribute attr, const OpStringC8 &string);
	void SetAttributeL(URL::URL_UniStringAttribute attr, const OpStringC &string);
	void SetAttributeL(URL::URL_VoidPAttribute  attr, const void *param);
	void SetAttributeL(URL::URL_URLAttribute  attr, const URL &param);


	void IterateMultipart();

#if defined(MHTML_ARCHIVE_REDIRECT_SUPPORT) && defined(URL_SEPARATE_MIME_URL_NAMESPACE)
	void AddCleanupItem(Link *cleanup_item) { cleanup_item->Into(&cleanup_items); }
#endif

	BOOL NameEquals(const URL_Rep *rep) const { return name == rep->name; }
};



void GeneralValidateSuggestedFileNameFromMIMEL(OpString & suggested, const OpStringC &mime_type);

inline const char *URL_Rep::Name(Password_Status password_hide) const{return name.Name(URL::KName_Escaped); }
inline const uni_char *URL_Rep::UniName(Password_Status password_hide) const {return name.UniName(URL::KUniName_Escaped, 0); }

#include "modules/url/url_tags.h"

#endif // _URL_REP_H
