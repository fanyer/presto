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


// URL Data Storage

#ifndef URL_DS_H
#define URL_DS_H

class URL;
class Upload_Base;
class FileName_Store;
class URL_Manager;
class Authentication_Manager;
class Cache_Manager;
#ifdef CACHE_FAST_INDEX
	class CacheEntry;
	class DiskCacheEntry;
#endif
class URL_DynamicUIntAttributeDescriptor;
class URL_DynamicStringAttributeDescriptor;
class URL_DynamicURLAttributeDescriptor;
class URL_HTTP_ProtocolData;
class URL_ProtocolData;
class URL_FTP_ProtocolData;
class URL_Mailto_ProtocolData;
#ifdef _MIME_SUPPORT_
class URL_MIME_ProtocolData;
#endif
class URL_LoadHandler;

#include "modules/hardcore/mh/mh.h"
#include "modules/auth/auth_man.h"

#ifdef URL_ENABLE_ASSOCIATED_FILES
#include "modules/cache/AssociatedFile.h"
#endif

class URL_Dialogs;
class File;
class Context_Manager;

class URL_DataStorage : public MessageObject, public Link, public AuthenticationInterface
{
	friend class URL_Manager;
	friend class Authentication_Manager;
	friend class Cache_Manager;
	friend class Context_Manager;
	friend class Context_Manager_Disk;
	friend class CacheListWriter;
	friend class URL_LoadHandler;
	friend class CacheTester;
public:
	// Use in arrays of attributes, last element's attribute is 0
	// value can be either a value to be set, or a DataFile_Record tag
	struct URL_UintAttributeEntry
	{
		URL::URL_Uint32Attribute attribute;
		uint32			value;
	};

	// Use in arrays of attributes, last element's attribute is 0
	// value is a string
	struct URL_StringAttributeEntry
	{
		URL::URL_StringAttribute attribute;
		const char *value;
	};

	// Use in arrays of attributes, last element's attribute is 0
	// value is a DataFile_Record tag
	struct URL_StringAttributeRecEntry
	{
		URL::URL_StringAttribute attribute;
		uint32			value;
	};

	// Use in arrays of attributes, last element's attribute is 0
	// value is a DataFile_Record tag
	struct URL_UniStringAttributeRecEntry
	{
		URL::URL_UniStringAttribute attribute;
		uint32			value;
	};

#define URL_DYNATTR_ELEMENTS 3

	/** PayloadType must be have at least this API
	 *		class PayloadType
	 *		{
	 *			// Default Constructor
	 *			PayloadType();
	 * 
	 *			// A reset function
	 *			void Reset();
	 *			// Takes over the value of val, *WITHOUT* performing any allocations, val should be reset afterward 
	 *			void TakeOver(PayloadType &val);
	 *			// Return TRUE if this element contain a "non-zero" value;  that is, contain data
	 *			BOOL HasValue();
	 *		};
	 */
	template <class DescType, class PayloadType> struct URL_DynAttributeElement : public Link
	{
		struct element{
			const DescType *desc;
			PayloadType	value;
			element(): desc(NULL){};
			void TakeOver(element *src){
				if(src)
				{
					desc = src->desc;
					src->desc = NULL;
					value.TakeOver(src->value);
				}
				else
				{
					desc = NULL;
					value.Reset();
				}
			}
		} attributes[URL_DYNATTR_ELEMENTS];
		
		URL_DynAttributeElement(){};
		virtual ~URL_DynAttributeElement();

		element *FindElement(const DescType *desc);

		/** Returns TRUE if the element was set */
		BOOL InsertElement(const DescType *desc, PayloadType &val);

		/** Returns TRUE if the element contains values, last_element is the last element of the list, and can be deleted */
		BOOL DeleteElement(const DescType *desc, URL_DynAttributeElement *last_element);

		URL_DynAttributeElement *Suc(){return (URL_DynAttributeElement *) Link::Suc();}
	};

	template <class DescType, class PayloadType, class ValueType> class DynAttrHead : public AutoDeleteHead
	{
	public:
		PayloadType *First() const{return (PayloadType *) AutoDeleteHead::First();}
		PayloadType *Last() const{return (PayloadType *) AutoDeleteHead::Last();}

		typename PayloadType::element *FindElement(const DescType *desc, PayloadType **item_ret) const{
			if(item_ret)
				*item_ret = NULL;
			if(desc == NULL)
				return NULL;

			PayloadType *item = First();
			while(item)
			{
				typename PayloadType::element *elm = item->FindElement(desc);
				if(elm)
				{
					if(item_ret)
						*item_ret = item;
					return elm;
				}
				item = item->Suc();
			}
			return NULL;
		}
		OP_STATUS UpdateValue(const DescType *desc, ValueType &val) const{
			if(desc == NULL)
				return OpStatus::OK;

			PayloadType *item = First();
			typename PayloadType::element *elm=NULL;
			while(item)
			{
				elm = item->FindElement(desc);
				if(elm)
					break;
				item = item->Suc();
			}

			if(elm)
			{
				elm->value.TakeOver(val);
				return OpStatus::OK;
			}

			OP_ASSERT(!item);

			if(!val.HasValue()) // zero values not set
				return OpStatus::OK;

			item = Last();
			if(item && item->InsertElement(desc, val))
				return OpStatus::OK;

			item = OP_NEW(PayloadType, ());
			if(item == NULL)
				return OpStatus::ERR_NO_MEMORY;

			item->InsertElement(desc, val);
			item->Into((DynAttrHead *) this);

			return OpStatus::OK;
		}
		void RemoveValue(const DescType *desc)
		{
			PayloadType *item = NULL;
			FindElement(desc, &item); // Do not need return value;

			if(item && !item->DeleteElement(desc, Last()))
			{
				OP_DELETE(item); // No more attributes in the item; Kick it out
				item = NULL;
			}
		}
		
	};

	class UIntElement
	{
	public:
		uint32 value;
		UIntElement(uint32 val=0):value(val){}

		operator uint32(){return value;}

		UIntElement &operator =(uint32 val){value = val;return *this;}
		UIntElement &operator =(const UIntElement &val){value = val.value;return *this;}

		void Reset(){value=0;}
		void TakeOver(UIntElement &val){value = val.value; val.value = 0;}
		BOOL HasValue(){return value != 0;}
	};

	typedef URL_DynAttributeElement<URL_DynamicUIntAttributeDescriptor, UIntElement> URL_DynIntAttributeElement;
	typedef DynAttrHead<URL_DynamicUIntAttributeDescriptor, URL_DynIntAttributeElement, UIntElement>  URL_DynIntAttributeHead;

	class StringElement
	{
	public:
		OpString8 value;
		StringElement(){}

		operator OpString8 &(){return value;}

		void Reset(){value.Empty();}
		void TakeOver(StringElement &val){value.TakeOver(val.value);}
		void TakeOver(OpString8 &val){value.TakeOver(val);}
		BOOL HasValue(){return value.HasContent();}
	};

	typedef URL_DynAttributeElement<URL_DynamicStringAttributeDescriptor, StringElement> URL_DynStrAttributeElement;
	typedef DynAttrHead<URL_DynamicStringAttributeDescriptor, URL_DynStrAttributeElement, StringElement>  URL_DynStrAttributeHead;

	class URL_Element
	{
	public:
		OpString8 value_str;
		URL	value;
		URL_Element(){}

		void Reset(){value_str.Empty(); value = URL(); }
		void TakeOver(URL_Element &val){value_str.TakeOver(val.value_str);value = val.value;val.value = URL();}
		void TakeOver(OpString8 &val){value_str.TakeOver(val);value = URL();}
		void TakeOver(URL &val){value_str.Empty();value = val;}
		BOOL HasValue(){return value.IsValid() || value_str.HasContent();}
	};

	typedef URL_DynAttributeElement<URL_DynamicURLAttributeDescriptor, URL_Element> URL_DynURLAttributeElement;
	typedef DynAttrHead<URL_DynamicURLAttributeDescriptor, URL_DynURLAttributeElement, URL_Element>  URL_DynURLAttributeHead;

private:
	URL_Rep *url;
	class OpRequestImpl *op_request;
	
	MsgHndlList*	mh_list;

	URL_Dialogs *current_dialog;
 
	struct datainfo_st
	{
		UINT	security_status:4;
		UINT	header_loaded:1;
		/// TRUE if the content-type was NULL or broken.
		UINT	content_type_was_null:1;
		UINT	is_resuming:1;
		UINT	resume_supported:2;
		UINT	use_same_target:1;
		// Cache
		UINT	proxy_no_cache:1; // Loading
		UINT	cache_always_verify:1;
		UINT	cache_no_store:1;
#ifdef URL_ENABLE_ACTIVE_COMPRESS_CACHE
		UINT	compress_cache:1;
#endif
		UINT	forceCacheKeepOpenFile:1;
		// Third party requests
		UINT	disable_cookie:1;
		UINT	is_third_party:1;
		UINT	is_third_party_reach:1;
		UINT	use_generic_authentication_handling:1;
		UINT	was_inline:1;
		// Proxy
		UINT	determined_proxy:1;
		UINT	use_proxy:1;
#ifdef URL_ACTIVATE_URL_LOAD_RESERVATION_OBJECT
		UINT	overide_load_protection:1;
#endif
		UINT	untrusted_content:1;
		UINT autodetect_charset:4;
#ifdef TRUST_RATING
		UINT trust_rating:3;
#endif
		// Multimedia cache
		UINT multimedia:1;
#if defined(_EMBEDDED_BYTEMOBILE) || defined(WEB_TURBO_MODE)
		UINT	load_direct:1;
#endif // _EMBEDDED_BYTEMOBILE || WEB_TURBO_MODE
#ifdef _EMBEDDED_BYTEMOBILE
		UINT	predicted:1;
#endif //_EMBEDDED_BYTEMOBILE
		UINT	use_nettype:2; // NOTE: MUST match the size of OpSocketAddressNetType
		UINT	loaded_from_nettype:2; // NOTE: MUST match the size of OpSocketAddressNetType
#ifdef URL_NOTIFY_FILE_DOWNLOAD_SOCKET
		UINT	is_file_download:1;
#endif
		UINT	user_initiated:1;
		UINT	max_age_set:1;
		// TRUE if the size of the cache has been added here and not by Cache_Storage; this happens because 0-byte URLs
		// don't get a storage, so the only way to add the URL Object estimation is doing it manually in this class
		UINT	cache_size_added:1;
	} info;
	time_t local_time_loaded;

#ifdef URL_ENABLE_ASSOCIATED_FILES
	UINT32 assoc_files;
#endif

#ifdef URL_ACTIVATE_URL_LOAD_RESERVATION_OBJECT
	unsigned int	load_count;
#endif

	// Content type
	OpFileLength content_size;
	BOOL content_type_deterministic;
#ifdef WEB_TURBO_MODE
	UINT32 turbo_transferred_bytes;
	UINT32 turbo_orig_transferred_bytes;
#endif // WEB_TURBO_MODE
#ifdef USE_SPDY
	BOOL loaded_with_spdy;
#endif // USE_SPDY
	OpStringS8 content_type_string;
	unsigned short int mime_charset;
	OpStringS8 server_content_type_string;
#ifdef _EMBEDDED_BYTEMOBILE
	int predicted_depth;
#endif //_EMBEDDED_BYTEMOBILE
	OpStringS security_text;
	OpStringS internal_error_message;
	
	URL_LoadHandler		*loading;
	Cache_Storage		*old_storage;
	Cache_Storage		*storage;
	
	URL_HTTP_ProtocolData *http_data;
	union {
		URL_ProtocolData *prot_data;
		URL_FTP_ProtocolData *ftp_data;
		URL_Mailto_ProtocolData *mailto_data;
#ifdef _MIME_SUPPORT_
		URL_MIME_ProtocolData *mime_data;
#endif
	} protocol_data;

	URL_DynIntAttributeHead dynamic_int_attrs;
	URL_DynStrAttributeHead dynamic_string_attrs;
	URL_DynURLAttributeHead dynamic_url_attrs;

#ifdef DEBUG_LOAD_STATUS
	unsigned long started_tick;
#endif

	OpFileLength range_offset; ///< Position of the current range download, used for requests that require more than packet

	UINT32 timeout_poll_idle;  ///< Value of attribute URL::KTimeoutPollIdle

	UINT32 timeout_max_tcp_connection_established;

private:
	unsigned char*	CheckLoadBuf(unsigned long buf_size);
	void InternalInit();
	void InternalDestruct();
	
#ifdef _LOCALHOST_SUPPORT_
	void StartLoadingFile(BOOL guess_content_type, BOOL treat_zip_as_folder = TRUE);
#endif // _LOCALHOST_SUPPORT_

#ifdef DOM_GADGET_FILE_API_SUPPORT
	void StartLoadingMountPoint(BOOL guess_content_type);
#endif // DOM_GADGET_FILE_API_SUPPORT

#if defined(GADGET_SUPPORT) && defined(_LOCALHOST_SUPPORT_)
	void StartLoadingWidget(MessageHandler *msg_handler, BOOL guess_content_type);
#endif // GADGET_SUPPORT && _LOCALHOST_SUPPORT_

#if !defined NO_URL_OPERA  || defined(HAS_OPERABLANK)
	void StartLoadingOperaSpecialDocument(MessageHandler *msg_handler);
public:

private:
#endif // !NO_URL_OPERA
	void StartLoadingDataURL(const URL& referer_url);
	
#ifdef HAS_ATVEF_SUPPORT
	/**
	* This will fake image loading into a TV image.
	* You need to compile in url_lhtv.cpp if you define HAS_ATVEF_SUPPORT.
	* See the functions implementation in url_ds.cpp.
	*/
	void StartLoadingTvDocument();
#endif /* HAS_ATVEF_SUPPORT */
	
	CommState StartNameCompletion(const URL& referer_url);
	CommState StartLoading(MessageHandler* msg_handler, const URL& referer_url);
	CommState DetermineProxy(MessageHandler* msg_handler, const URL& referer_url);
	
	void UnsetListCallbacks();

	// Set a dynamic attribute - flag
	void AddDynamicAttributeFlagL(UINT32 mod_id, UINT32 tag_id);
	// Set a dynamic attribute - integer
	void AddDynamicAttributeIntL(UINT32 mod_id, UINT32 tag_id, UINT32 value);
	// Set a dynamic attribute - string
	void AddDynamicAttributeStringL(UINT32 mod_id, UINT32 tag_id, const OpStringC8& str);
	// Set a dynamic attribute - URL
	void AddDynamicAttributeURL_L(UINT32 mod_id, UINT32 tag_id, const OpStringC8& str);
	
public:


	URL_DataStorage(URL_Rep *);
	OP_STATUS Init();
#ifndef RAMCACHE_ONLY
#ifndef SEARCH_ENGINE_CACHE
	void InitL(DataFile_Record *rec, FileName_Store &filenames, OpFileFolder folder);
	#ifdef CACHE_FAST_INDEX
	void InitL(DiskCacheEntry *entry, FileName_Store &filenames, OpFileFolder folder);
	#endif
#else
	void InitL(DataFile_Record *rec, OpFileFolder folder);
	#ifdef CACHE_FAST_INDEX
	void InitL(DiskCacheEntry *entry, OpFileFolder folder);
	#endif
#endif
#endif
	virtual ~URL_DataStorage();
	
	// delete loadhandler
	void DeleteLoading();

#ifdef URL_ACTIVATE_URL_LOAD_RESERVATION_OBJECT
	/** Increment Loading and Used count of this URL to block (re)loading and unloading */
	void	IncLoading(){load_count ++;}

	/** Decrement Loading and Used count of this URL to unblock (re)loading and unloading */
	void	DecLoading(){if (load_count>0) load_count--;}
#endif
	void SetOpRequestImpl(class OpRequestImpl *request) { op_request = request; }
	class OpRequestImpl *GetOpRequestImpl() { return op_request; }
#ifdef _OPERA_DEBUG_DOC_
	void GetMemUsed(DebugUrlMemory &);
#endif
private:
	CommState	Load_Stage1(MessageHandler* mh, const URL& referer_url, BOOL user_initiated, BOOL thirdparty_determined, BOOL inline_loading);
	CommState	Load_Stage2(MessageHandler* msg_handler, const URL& referer_url);

	// Callbacks for start and end of a URL load

	/**
	* Convenience method for reporting that loading of a URL was started.
	* @see URL_Rep::OnLoadStarted for details.
	*/
	void		OnLoadStarted(MessageHandler* mh=NULL);

	/**
	* Convenience method for reporting that loading of a URL was finished.
	* @see URL_Rep::OnLoadFinished for details.
	*/
	void		OnLoadFinished(URLLoadResult result, MessageHandler* mh=NULL);

	/**
	* Call when loading fails to clean up MsgHndlList, and report
	* the situation to Scope.
	*/
	void		OnLoadFailed();

public:
	CommState	Load(MessageHandler* mh, const URL& referer_url, BOOL user_initiated, BOOL thirdparty_determined, BOOL inline_loading);
	CommState	Reload(MessageHandler* mh, const URL& referer_url, BOOL only_if_modified, BOOL proxy_no_cache,BOOL user_initiated, BOOL thirdparty_determined, EnteredByUser entered_by_user, BOOL inline_loading, BOOL* allow_if_modified = NULL);
	CommState   ResumeLoad(MessageHandler* msg_handler, const URL& referer_url);
	//BOOL		QuickLoad(); // Only for File, or already loaded
	BOOL		QuickLoad(BOOL guess_content_type); // Only for File, or already loaded
	
	void		StopLoading(MessageHandler* mh);
#ifdef DYNAMIC_PROXY_UPDATE
	void		StopAndRestartLoading();
#endif // DYNAMIC_PROXY_UPDATE
	void		RemoveMessageHandler(MessageHandler *old_mh);
	void		EndLoading(BOOL force);

	unsigned long SaveAsFile(const OpStringC &file_name);
	OP_STATUS		LoadToFile(const OpStringC &file_name);
	
#ifndef RAMCACHE_ONLY
	void WriteCacheDataL(DataFile_Record *rec);
# ifdef CACHE_FAST_INDEX
	void WriteCacheDataL(DiskCacheEntry *entry, BOOL embedded);
# endif
#endif

	BOOL Expired(BOOL inline_load, BOOL user_setting_only);
	BOOL Expired(BOOL inline_load, BOOL user_setting_only, INT32 maxstale, INT32 maxage);
	
	URL_DataDescriptor *GetDescriptor(MessageHandler *mh, BOOL get_raw_data = FALSE, BOOL get_decoded_data=TRUE, Window *window = NULL, 
		URLContentType override_content_type = URL_UNDETERMINED_CONTENT, unsigned short override_charset_id = 0, BOOL get_original_content=FALSE, unsigned short parent_charset_id = 0);
	//void UpdateDescriptor(URL_DataDescriptor *desc);
	
	virtual        void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
	
	void ReceiveData() {TRAPD(op_err, ReceiveDataL());};
	void ReceiveDataL();
	OP_STATUS CreateCache();
	DEPRECATED(BOOL CreateCacheL());
	OP_STATUS CreateStreamCache();
	Cache_Storage *CreateNewCache(OP_STATUS &op_err, BOOL force_to_file = FALSE);
	void CreateMultipartCacheL(ParameterList *content_type, const OpStringC8 &boundary_header);

#ifndef RAMCACHE_ONLY
	BOOL CachePersistent(void);
#endif

	void HandleFinished();

	/**
	 * Handle loading errors.
	 *
	 * Will properly shutdown the ongoing load according to error code.
	 *
	 * Special handling for application cache resources with fallback urls:
	 * In case the url has a fallback, the load will be finished as as
	 * successful load and the fallback resource will be served to the loader
	 * of this url. The loading should continue as no error has happened.
	 *
	 * Currently handled errors:
	 *
	 * ERR_COMM_CONNECTION_CLOSED
	 * ERR_AUTO_PROXY_CONFIG_FAILED
	 * ERR_AUTO_PROXY_CONFIG_FAILED
	 * ERR_COMM_RELOAD_DIRECT
	 * ERR_HTTP_100CONTINUE_ERROR
	 * ERR_OUT_OF_COVERAGE
	 * ERR_DISK_IS_FULL
	 *
	 *
	 * @param error The error causing the load to fail.
	 * @return TRUE if there was an application cache fallback, FALSE otherwise.
	 *
	 */
	BOOL HandleFailed(MH_PARAM_2 error);

	/**
	 * Start polling for data updates from/to the server.
	 *
	 * Only has effect if URL::KTimeoutPollIdle is set.
	 */
	void StartIdlePoll();

	OP_STATUS ExecuteRedirect();
	OP_STATUS ExecuteRedirect_Stage2(BOOL context_switch = FALSE);
	
	OP_STATUS WriteDocumentData(URL::URL_WriteDocumentConversion conversion, const OpStringC &data, unsigned int len= (unsigned int) KAll);
	OP_STATUS WriteDocumentData(URL::URL_WriteDocumentConversion conversion, const OpStringC8 &data, unsigned int len= (unsigned int) KAll);
	OP_STATUS WriteDocumentData(URL::URL_WriteDocumentConversion conversion, URL_DataDescriptor *data, unsigned int len= (unsigned int) KAll);
	OP_STATUS WriteDocumentData(const unsigned char *data, unsigned int len);
	void WriteDocumentDataFinished();
	void WriteDocumentDataSignalDataReady();
	
	void 			MIME_ForceContentTypeL(OpStringC8 mtype);
	
	OP_STATUS		FindContentType(URLContentType &ret_ct, 
						const char* mimetype, 
						const uni_char* ext, const uni_char *name, BOOL set_mime_type = FALSE);
	//void			AutoForceContentType() ;

	BOOL			DetermineThirdParty(const URL &referrer);

	// Special objects
	OP_STATUS CheckHTTPProtocolData();
	OP_STATUS CheckMailtoProtocolData();
	OP_STATUS CheckFTPProtocolData();
	URL_HTTP_ProtocolData *GetHTTPProtocolData();
#if defined SELFTEST || defined CACHE_ADVANCED_VIEW
	URL_HTTP_ProtocolData *GetHTTPProtocolDataNoAlloc() { return http_data; }
#endif
	URL_Mailto_ProtocolData *GetMailtoProtocolData();
	URL_HTTP_ProtocolData *GetHTTPProtocolDataL();
	URL_FTP_ProtocolData *GetFTPProtocolDataL();
	URL_Mailto_ProtocolData *GetMailtoProtocolDataL();

	MsgHndlList		*GetMessageHandlerList() { return mh_list; }
	MessageHandler*	GetFirstMessageHandler() { return mh_list->FirstMsgHandler(); }
	void			ChangeMessageHandler(MessageHandler* old_mh, MessageHandler* new_mh);
	void			BroadcastMessage(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2, int flags);
	void			SendMessages(MessageHandler *mh, BOOL set_failure, OpMessage msg, MH_PARAM_2 par2);
	//BOOL			BroadcastDelayedMessage(int msg, int par1, long par2, int flags, unsigned long delay) { return mh_list->BroadcastDelayedMessage(msg, par1, par2, flags, delay); }
	void			CleanMessageHandlers() { UnsetListCallbacks(); mh_list->Clean(); }
	
	void			UnsetCacheFinished();//{if(storage) storage->UnsetFinished();}
	void			MoveCacheToOld(BOOL conditionally=FALSE);
	void			ResetCache();
	void			UseOldCache() { storage =old_storage; old_storage = NULL;}
	//void			RemoveCache();// { delete storage; storage = NULL; }

	BOOL			GetWasInline(){ return info.was_inline; }
	BOOL			GetWasUserInitiated(){ return info.user_initiated; }
	/** Try to free the storage.
		@param system_time_sec if not 0, it represent the cached system time in seconds ( g_op_time_info->GetTimeUTC()/1000.0 ), to speed-up the computation
	*/
	BOOL			FreeUnusedResources(double cached_system_time_sec=0.0);

#ifdef _ENABLE_AUTHENTICATE
	// From AuthenticationInterface
	virtual void	Authenticate(AuthElm *auth_elm);
	virtual BOOL	CheckSameAuthorization(AuthElm *auth_elm, BOOL proxy);
	virtual void	FailAuthenticate(int mode);
	virtual authdata_st *GetAuthData();
#endif

#if defined(_EMBEDDED_BYTEMOBILE) || defined(WEB_TURBO_MODE)
	void			SetLoadDirect(BOOL val){info.load_direct = (BYTE) val;}
	BOOL			GetLoadDirect() const {return info.load_direct;}
#endif // _EMBEDDED_BYTEMOBILE || WEB_TURBO_MODE
#ifdef _EMBEDDED_BYTEMOBILE
	void			SetPredicted(BOOL val, int depth) { info.predicted = val; predicted_depth = depth; }
	BOOL			GetPredicted() {return info.predicted; }
#endif //_EMBEDDED_BYTEMOBILE

	//URL             GetHTTPContentLocation();

	BOOL			DumpSourceToDisk(BOOL force_file = FALSE);
	unsigned long	PrepareForViewing(BOOL get_raw_data, BOOL get_decoded_data, BOOL force_to_file=FALSE);
#ifdef	URL_ENABLE_HTTP_RANGE_SPEC
	/// Return the next range start for partial HTTP GET requests
	OpFileLength GetNextRangeStart();
#endif

	OP_STATUS		SetHTTP_Data(const char* data, BOOL new_data, BOOL with_headers) ;
#ifdef HAS_SET_HTTP_DATA
	void			SetHTTP_Data(Upload_Base* data, BOOL new_data);
	void			SetMailData(Upload_Base *data);
#endif
	
#ifdef _MIME_SUPPORT_
	BOOL			GetAttachment(int i, URL &url);
	BOOL			IsMHTML() const;
	URL				GetMHTMLRootPart();
#endif

#ifdef URL_ENABLE_ASSOCIATED_FILES
	AssociatedFile *CreateAssociatedFile(URL::AssociatedFileType type);
	AssociatedFile *OpenAssociatedFile(URL::AssociatedFileType type);

	OP_BOOLEAN GetFirstAssocFName(OpString &fname, URL::AssociatedFileType &type);
	OP_BOOLEAN GetNextAssocFName(OpString &fname, URL::AssociatedFileType &type);
#endif

	/** Copy the all_headers list into header_list_copy. The header list is empty unless
	* SetStoreAllHeaders is called with TRUE before the url is loaded. The headers are
	* copied into header_list_copy.
	*/
	void			CopyAllHeadersL(HeaderList& header_list_copy) const;

#ifdef _MIME_SUPPORT_
	URL_MIME_ProtocolData *CheckMIMEProtocolData(OP_STATUS &op_err);
#endif
//#ifdef STRICT_CACHE_LIMIT
    Cache_Storage * GetCacheStorage() { return storage; };
//#endif
	OpStringS8      *GetHTTPEncoding();
	
	uint32 GetAttribute(URL::URL_Uint32Attribute attr) const;
	const OpStringC8 GetAttribute(URL::URL_StringAttribute attr) const;
	const OpStringC GetAttribute(URL::URL_UniStringAttribute attr) const;
	void			GetAttributeL(URL::URL_StringAttribute attr, OpString8 &val) const;
	OP_STATUS		GetAttribute(URL::URL_StringAttribute attr, OpString8 &val) const;
	void			GetAttributeL(URL::URL_UniStringAttribute attr, OpString &val) const;
	OP_STATUS		GetAttribute(URL::URL_UniStringAttribute attr, OpString &val) const;
	const void *GetAttribute(URL::URL_VoidPAttribute  attr, const void *param) const;

	URL GetAttribute(URL::URL_URLAttribute  attr);

#ifdef DISK_CACHE_SUPPORT
	void GetAttributeL(const URL_DataStorage::URL_UintAttributeEntry *list, DataFile_Record *rec);
	void GetAttributeL(const URL_DataStorage::URL_StringAttributeRecEntry *list, DataFile_Record *rec);
	#ifdef CACHE_FAST_INDEX
		void GetAttributeL(const URL_DataStorage::URL_UintAttributeEntry *list, CacheEntry *entry);
		OP_STATUS GetAttributeL(const URL_DataStorage::URL_StringAttributeRecEntry *list, CacheEntry *entry);
	#endif
#endif

	void SetAttributeL(URL::URL_Uint32Attribute attr, uint32 value);
	void SetAttributeL(URL::URL_StringAttribute attr, const OpStringC8 &string);
	void SetAttributeL(URL::URL_UniStringAttribute attr, const OpStringC &string);
	void SetAttributeL(URL::URL_VoidPAttribute  attr, const void *param);

	/** Null terminated list, set indicated flags */
	OP_STATUS SetAttribute(const URL_DataStorage::URL_UintAttributeEntry *);
	OP_STATUS SetAttribute(const URL_DataStorage::URL_StringAttributeEntry *list);
#ifdef DISK_CACHE_SUPPORT
	void SetAttributeL(const URL_DataStorage::URL_UintAttributeEntry *list, DataFile_Record *rec);
	void SetAttributeL(const URL_DataStorage::URL_StringAttributeRecEntry *list, DataFile_Record *rec);
	#ifdef CACHE_FAST_INDEX
		void SetAttributeL(const URL_DataStorage::URL_UintAttributeEntry *list, CacheEntry *entry);
		void SetAttributeL(const URL_DataStorage::URL_StringAttributeRecEntry *list, CacheEntry *entry);
	#endif
#endif

	OP_STATUS SetAttribute(URL::URL_Uint32Attribute attr, uint32 value);
	OP_STATUS SetAttribute(URL::URL_StringAttribute attr, const OpStringC8 &string);
	OP_STATUS SetAttribute(URL::URL_UniStringAttribute attr, const OpStringC &string);
	OP_STATUS SetAttribute(URL::URL_VoidPAttribute  attr, const void *param);

	void SetAttributeL(URL::URL_URLAttribute  attr, const URL &param);
	OP_STATUS SetAttribute(URL::URL_URLAttribute  attr, const URL &param);

	//void			SetCachePolicy_AlwaysVerify(BOOL flag) ;
	//void			SetCachePolicy_NoStore(BOOL flag) ;

	OP_STATUS		SetSecurityStatus(uint32 stat, const uni_char *stat_text = NULL);
	
#ifdef __OEM_EXTENDED_CACHE_MANAGEMENT
	void SetIsOutOfCoverageFile(BOOL flag);
	BOOL GetIsOutOfCoverageFile() ;
	void SetNeverFlush(BOOL flag);
	BOOL GetNeverFlush() ;
#endif

	//void			SetUntrustedContent(BOOL val);
	
	void IterateMultipart();

	void StartTimeout();
	void StartTimeout(BOOL response_timeout);
private:
	CommState AskAboutURLWithUserName(const URL& referer_url);
	CommState AskAboutURLWithUserNameL(const URL& referer_url);

#ifdef GOGI_URL_FILTER
    OP_STATUS CheckURLAllowed(URL_Rep *url, BOOL& wait);
#endif // GOGI_URL_FILTER

	CommState InternalRedirect(const char *new_url, BOOL unique, URL_CONTEXT_ID new_ctx);
	OP_STATUS SetDynAttribute(URL::URL_Uint32Attribute attr, uint32 value);
	OP_STATUS SetDynAttribute(URL::URL_StringAttribute attr, const OpStringC8 &string);
	OP_STATUS GetDynAttribute(URL::URL_StringAttribute attr, OpString8 &val) const;
	URL GetDynAttribute(URL::URL_URLAttribute  attr);
	OP_STATUS SetDynAttribute(URL::URL_URLAttribute  attr, const URL &param);
	uint32 GetDynAttribute(URL::URL_Uint32Attribute attr) const;

	/**
	* Add a MessageHandler to the list of MessageHandlers, and notify listeners
	* that this happened.
	*
	* @param mh The new MessageHandler to add to mh_list.
	* @param always_new Parameter passed to MsgHndlList::Add.
	* @param first Parameter passed to MsgHndlList::Add.
	* @return OpStatus::OK, or OpStatus::ERR_NO_MEMORY.
	*/
	OP_STATUS AddMessageHandler(MessageHandler *mh, BOOL always_new = FALSE, BOOL first = FALSE);

protected:
	/// Return the URL_Rep associated with the URL_DataStorage
	URL_Rep *Rep() { return url; }
};

#endif // !URL_DS_H
