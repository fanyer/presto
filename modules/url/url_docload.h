/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */
#ifndef _URL_DOCLOADER_H_
#define _URL_DOCLOADER_H_

#include "modules/url/url2.h"
#include "modules/url/url_loading.h"
#include "modules/cache/url_dd.h"
#include "modules/hardcore/mh/mh.h"
#include "modules/util/smartptr.h"
#include "modules/util/twoway.h"

/** URL_DocumentHandler is a handle base class used by URL_DocumentLoader to 
 *	signal URL loading events to the objects that will process the received data.
 *	The actual handlers are derived either directly from URL_DocumentHandler 
 *	or URL_DocumentLoader.
 *
 *	Actual loading requests are initiated with URL_DocumentLoader, with a 
 *	pointe to an object derived from URL_DocumentHandler, if the handler
 *	is different from the URL_DocumentLoader derived class.
 *
 *	URL_DocumentLoader manages all loading, and the messages from the URLs,
 *	and then triggers the appropriate callback to the handling object.
 *	
 *  Implementations need only implement a callback if they plan to act on it,
 *	otherwise they can leave the operation to the default callback implementation.
 *
 *	Destruction of this object will result in immediate 
 *
 *	Current callbacks:
 *		OnURLLoadingStarted		Called to inform handler that loading has started.
 *		OnURLRedirected			Indicates that the currently loading URL has been redirected
 *								to the other URL.
 *		OnURLHeaderLoaded		Indicates that information about the content has been received,
 *								such as the content type. Any registered datadescriptor is deleted
 *								before this call.
 *		OnURLDataLoaded			Called every time there is new data (but only once per actual 
 *								call to DataDescriptor date retrieval). If there is a datadescriptor
 *								registered calls will continue after actual loading is completed, 
 *								until either the implementation returns FALSE, or destroys the descriptor.
 *								If the implementation do not want to use this system, and to continue 
 *								processing data after the loading is finished it must register 
 *								an independent callback for MSG_URL_DATA_LOADED (not recommended 
 *								with this system).
 *		OnURLRestartSuggested	Indicates that loading of the content might have to be restarted,
 *								usually becase an incorrect charset guessing has been detected.
 *								Restart is optional, but must be indicated in the return value.
 *		OnURLNewBodyPart		A new multipart is ready, or about to be loaded, and the implementation 
 *								should wrap up what it is doing, and wait for the next header loaded 
 *								event (dataloaded events may occur before that, as part of read events
 *								on an unfinished descriptor; stored descriptors will be destroyed 
 *								when the header loaded message arrives).
 *								This event may also occur for normal loads, when HTTP needs to refetch a body
 *		OnURLLoadingFailed		A failure notification has been received, the string enum is the received error code
 *		OnURLLoadingStopped		Loading of the URL was stopped from/via URL_DocumentLoader, no further events will
 *								be signaled for the URL.
 *
 *	Most of the post-header loaded callbacks provide the ability to store a datadescriptor in the loader. 
 *	The descriptor must be requested by the implementation, then assigned to the provided autopointer.
 *	The loader assumes ownership of this descriptor, and will destroy it unless the implementation 
 *	releases it from the autopointer's control.
 *	If registered, a descriptor will be destroyed before the header loaded event is called.
 */
class URL_DocumentHandler :  public TwoWayPointer_Target
{
private:
	/* Used by default dataloaded function to indicate to the default new body part function that it is used, allowing loading to progress */
	BOOL using_default_data_loaded;
private:
	// Does not exist
	URL_DocumentHandler(const URL_DocumentHandler *);
public:
	/** Constructor */
	URL_DocumentHandler() : using_default_data_loaded(FALSE){};

	/** Destructor; will stop loading any associated URL */
	virtual ~URL_DocumentHandler(){};

	/** Indicates that loading of the URL has started successfully
	 *	This allows the document handler to take specific 
	 *	actions that have to be performed immediatately after
	 *	loading has been initiated.
	 *
	 *	@param	url		The URL for which this callback is done
	 *	@return	BOOL	TRUE if loading should continue, FALSE if terminate loading
	 */
	virtual BOOL OnURLLoadingStarted(URL &url);

	/**	Indicates that the currently loading URL has been redirected
	 *	to the other URL. May be called multiple times in a sequence,
	 *	if the redirected_to URL is also redirected.
	 *
	 *	@param	url		The URL for which this callback is done
	 *	@param	redirected_to	The next URL in the redirect chain
	 *	@return	BOOL	TRUE if loading should continue, FALSE if terminate loading
	 */
	virtual BOOL OnURLRedirected(URL &url, URL &redirected_to);

	/**	Indicates that information about the content has been received,
	 *	such as the content type. Any registered datadescriptor is deleted
	 *	before this call.
	 *
	 *	@param	url		The URL for which this callback is done
	 *	@return	BOOL	TRUE if loading should continue, FALSE if terminate loading
	 */
	virtual BOOL OnURLHeaderLoaded(URL &url);

	/**	Called every time there is new data (but only once per actual 
	 *	call to DataDescriptor date retrieval). If there is a datadescriptor
	 *	registered calls will continue after actual loading is completed, 
	 *	until either the implementation returns FALSE, or destroys the descriptor.
	 *	If the implementation do not want to use this system, and to continue 
	 *	processing data after the loading is finished it must register 
	 *	an independent callback for MSG_URL_DATA_LOADED (not recommended 
	 *	with this system).
	 *
	 *	@param	url			The URL for which this callback is done
	 *	@param	finished	TRUE if the URL is finished; if stored_desc is NULL on 
	 *						return this is the last call (if the return value is TRUE)
	 *	@param	stored_desc	Reference to an autopointer where a datadescriptor can be stored.
	 *						Unless/until released, this autopointer takes ownership of the
	 *						descriptor and will destroy it.
	 *	@return	BOOL		TRUE if loading should continue, FALSE if terminate loading
	 */
	virtual BOOL OnURLDataLoaded(URL &url, BOOL finished, OpAutoPtr<URL_DataDescriptor> &stored_desc);

	/**	Indicates that loading of the content might have to be restarted,
	 *	usually becase an incorrect charset guessing has been detected.
	 *	Restart is optional, but must be indicated in the return parameter.
	 *
	 *	@param	url					The URL for which this callback is done
	 *	@param	restart_processing	Returned parameter, default FALSE. Set to TRUE by 
	 *								implementation if a restart of loading is to be done;
	 *								next event will be header loaded (although some 
	 *								dataloaded may occur).
	 *	@param	stored_desc			Reference to an autopointer where a datadescriptor can be stored.
	 *								Unless/until released, this autopointer takes ownership of the
	 *								descriptor and will destroy it.
	 *	@return	BOOL				TRUE if loading should continue, FALSE if terminate loading
	 */
	virtual BOOL OnURLRestartSuggested(URL &url, BOOL &restart_processing, OpAutoPtr<URL_DataDescriptor> &stored_desc);

	/**	A new multipart is ready, or about to be loaded, and the implementation 
	 *	should wrap up what it is doing, and wait for the next header loaded 
	 *	event (dataloaded events may occur before that, as part of read events
	 *	on an unfinished descriptor; stored descriptors will be destroyed 
	 *	when the header loaded message arrives).
	 *	This event may also occur for normal loads, when HTTP needs to refetch a body
	 *
	 *	@param	url			The URL for which this callback is done
	 *	@param	stored_desc	Reference to an autopointer where a datadescriptor can be stored.
	 *						Unless/until released, this autopointer takes ownership of the
	 *						descriptor and will destroy it.
	 *	@return	BOOL		TRUE if loading should continue, FALSE if terminate loading
	 */
	virtual BOOL OnURLNewBodyPart(URL &url, OpAutoPtr<URL_DataDescriptor> &stored_desc);

	/**	A failure notification has been received, the string enum is the received error code
	 *
	 *	@param	url			The URL for which this callback is done
	 *	@param	stored_desc	Reference to an autopointer where a datadescriptor can be stored.
	 *						Unless/until released, this autopointer takes ownership of the
	 *						descriptor and will destroy it.
	 *	@param	error		Error string code; may be Str:NOT_A_STRING if the error code is not provided.
	 */
	virtual void OnURLLoadingFailed(URL &url, Str::LocaleString error, OpAutoPtr<URL_DataDescriptor> &stored_desc);
	/**	Loading of the URL was stopped from/via URL_DocumentLoader, no further events will
	 *	be signaled for the URL.
	 *
	 *	@param	url			The URL for which this callback is done
	 *	@param	stored_desc	Reference to an autopointer where a datadescriptor can be stored.
	 *						Unless/until released, this autopointer takes ownership of the
	 *						descriptor and will destroy it.
	 */
	virtual void OnURLLoadingStopped(URL &url, OpAutoPtr<URL_DataDescriptor> &stored_desc);
};


/** URL_DocumentLoader is used to manage loading of one or more URLs, with 
 *	all events indicated to the implementation, or optionally one or more specified 
 *	handlers for the URL.
 *
 *	The implementation is also informed each time a URL has completed loaded, and 
 *	when all URLs currently loading has finished loading.
 *
 *	URL_DocumentLoader has the same callbacks as URL_DocumentHandler (which is a base class
 *
 *	URL_DocumentLoader is derived from MessageObject, and implements HandleCallback. 
 *	All classes inheriting URL_DocumentLoader and that also implements HandleCallback
 *	MUST call URL_DocumentLoader::HandleCallback for any message it does not handle
 *
 *	The fallback message handler is g_main_message_handler, a different message handler SHOULD be
 *	configured with the URL_DocumentLoader::Construct function. If the fallback is used, 
 *	an assert will triggered first, and any OOM with registering callbacks will be ignored.
 *	This MAY lead to never ending loading bugs.
 *
 *	Note: Parallel loading a URL in multiple DocumentLoaders with the same message handler can lead to
 *	difficulties, as StopLoading events can then stop loading for the other loaders as well, without
 *	any indication to them.
 *
 *	The LoadDocument() function is provided to start loading URLs based on a URL object, 
 *	or optionally string versions of relative URLs in combination with a base URL; Context
 *	specific base URLs MUST be non-empty.
 *
 *	Note that the same URL can be loaded multiple times with different handlers, and that 
 *	the callbacks will be called for each handler, and that the datadescriptors are stored separately
 *
 *	In addition to the URL_DocumentHandler callbacks, URL_DocumentLoader have the following callbacks
 *
 *		OnURLLoadingFinished	The specified URL has finished loading. This call does not indicate
 *								whether or not the load was a success; that is indicated to the handler.
 *		OnAllDocumentsFinished	All currently loading URLs have been loaded, regardless of success.
 */
class URL_DocumentLoader : public MessageObject, public URL_DocumentHandler, private TwoWayAction_Target
{
// private: 
// FIXME: ADS compiler does not allow encapsulated class HandlingURL to access private encapsulated class LoadingURLRef
//        For now it is just made public to make it compile. steink

public: 
	// The reference for URLs in the same loader. Prevents stoploading unless the last handler is deleted.
	class LoadingURLRef : public URLLink, public OpReferenceCounter
	{
	private:
		/** The message handler to use when loading or posting documents; if the message handler is deleted the owner will cancel all loading */
		TwoWayPointer<MessageHandler> document_mh;
	public:
		LoadingURLRef(URL &url, MessageHandler *mh): URLLink(url), document_mh(mh) { }
		virtual ~LoadingURLRef(){
			if(url.IsValid() && url.GetAttribute(URL::KLoadStatus, URL::KFollowRedirect) == URL_LOADING)
				url.StopLoading(document_mh);
			if(InList())
				Out();
		}

	};

private:

	/** The actual management of the loading; one each for a URL/handler pair 
	 *	Posts a message to the owner when finished
	 */
	class HandlingURL : public ListElement<HandlingURL>,  public MessageObject, public TwoWayAction_Target
	{
	public:
		/** The loading URL */
		URL url;
		/** Reference counter for the URL to prevent premature stop loading */
		OpSmartPointerWithDelete<LoadingURLRef> url_handler;
		/** The loading URL, cache locker, unset when loading ends */
		URL_InUse loader; 
		/** The loading URL, reload blockerm, set after header loader */
		IAmLoadingThisURL  loading_url;
		/** The stored datadescriptor, allocated by handler, owned by this object */
		OpAutoPtr<URL_DataDescriptor> stored_desc; 
		/** The message handler to use when loading or posting documents; if the message handler is deleted the owner will cancel all loading */
		TwoWayPointer<MessageHandler> document_mh;
		/** Pointer to the handler; if the handler is deleted all loading in this manager will stop */
		TwoWayPointer_WithAction<URL_DocumentHandler> handler;

		/** Header loaded received? */
		BOOL header_loaded;

		/** ID of owner */
		MH_PARAM_1 owner_id;

		/** Are we finished? */
		BOOL finished;

		/** We have sent a polling message? */
		BOOL sent_poll_message;
		/** We received a dataloaded message since the last polling message was sent */
		BOOL got_data_loaded_message;
	
	public:
		/** Constructor */
		HandlingURL(LoadingURLRef *a_url, MessageHandler *mh, URL_DocumentHandler *hndlr, MH_PARAM_1 ownr_id);
		/** Destructor */
		virtual ~HandlingURL();

		/** Handling messages */
		virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

		/** Start loading the document */
		OP_STATUS LoadDocument(const URL& referer_url, const URL_LoadPolicy &loadpolicy);

		/** Action when the handler is destryed; stop loading */
		void TwoWayPointerActionL(TwoWayPointer_Target *action_source, uint32 action_val);
	};

	/** KTimeoutMaximum to set for url(s) */
	uint32 timeout_max;

	/** KTimeoutPollIdle to set for url(s) */
	uint32 timeout_idle;

	/** KTimeoutMinimumBeforeIdleCheck to set for url(s) */
	uint32 timeout_minidle;

	/** KTimeoutStartsAtRequestSend to set for url(s) */
	BOOL timeout_startreq;

	/** KTimeoutStartsAtConnectionStart to set for url(s) */
	BOOL timeout_startconn;

	/** List of loading URLs */
	AutoDeleteList<HandlingURL> loading_urls;

	/** List of referenced loading URLs;not auto delete because loading_urls will delete all in this list */
	List<LoadingURLRef> loading_urls_refs;

	/** The message handler to use when loading or posting documents; if the message handler is deleted all loading will be cancelled */
	TwoWayPointer_WithAction<MessageHandler> document_mh;

	/** Action when the message handler is deleted */
	void TwoWayPointerActionL(TwoWayPointer_Target *action_source, uint32 action_val);

private:
	// Does not exist
	URL_DocumentLoader(URL_DocumentLoader *mh);
public:

	/** Constructor */
	URL_DocumentLoader();

	/** Specify a message handler; deletion of this message handler will terminate loading immediately */
	OP_STATUS Construct(MessageHandler *mh);

	/** Destructor; will stop loading of all documents (of course ;) ) */
	virtual ~URL_DocumentLoader();

	/** Start loading the specified document 
	 *
	 *	Errors are reported either immediately (if error codes are returned), 
	 *	in which case OnURLLoadingFinished will NOT be called, or later as part of 
	 *	normal error handling.
	 *
	 *	@param	url		The URL of the document to be loaded
	 *	@param	referer_url		The referrer URL to be used to load this URL (as well as to determine thirdparty cookie policies, etc.)
	 *	@param	loadpolicy		The cache policy and other loading policies.
	 *	@param	handler			If non-NULL this points to a handler implementation. If NULL, this object is the handler.
	 *
	 *	@return	OP_STATUS		If non-successful the loading did not start.
	 */
	OP_STATUS LoadDocument(URL &url, const URL& referer_url, const URL_LoadPolicy &loadpolicy, URL_DocumentHandler *handler= NULL, BOOL ocsp_crl_url = FALSE);

	/** Start loading the specified document 
	 *
	 *	Errors are reported either immediately (if error codes are returned), 
	 *	in which case OnURLLoadingFinished will NOT be called, or later as part of 
	 *	normal error handling.
	 *
	 *	@param	url				The URL of the document to be loaded. This can be an absolute URL or a relative URL.
	 *	@param	base_url		The base URL to the url string is to be resolved relative to; must be non-empty if non-zero CONTEXT_IDs are used
	 *	@param	referer_url		The referrer URL to be used to load this URL (as well as to determine thirdparty cookie policies, etc.)
	 *	@param	loadpolicy		The cache policy and other loading policies.
	 *	@param	handler			If non-NULL this points to a handler implementation. If NULL, this object is the handler.
	 *
	 *	@return	OP_STATUS		If non-successful the loading did not start.
	 */
	OP_STATUS LoadDocument(const OpStringC8 &url, URL &base_url, const URL& referer_url, const URL_LoadPolicy &loadpolicy,
							URL &loading_url, BOOL unique=FALSE, URL_DocumentHandler *handler= NULL);

	/** Start loading the specified document 
	 *
	 *	Errors are reported either immediately (if error codes are returned), 
	 *	in which case OnURLLoadingFinished will NOT be called, or later as part of 
	 *	normal error handling.
	 *
	 *	@param	url				The URL of the document to be loaded. This can be an absolute URL or a relative URL.
	 *	@param	base_url		The base URL to the url string is to be resolved relative to; must be non-empty if non-zero CONTEXT_IDs are used
	 *	@param	referer_url		The referrer URL to be used to load this URL (as well as to determine thirdparty cookie policies, etc.)
	 *	@param	loadpolicy		The cache policy and other loading policies.
	 *	@param	handler			If non-NULL this points to a handler implementation. If NULL, this object is the handler.
	 *
	 *	@return	OP_STATUS		If non-successful the loading did not start.
	 */
	OP_STATUS LoadDocument(const OpStringC  &url, URL &base_url, const URL& referer_url, const URL_LoadPolicy &loadpolicy, URL &loading_url, 
							BOOL unique=FALSE, URL_DocumentHandler *handler= NULL);

	/** Stop loading the specific URL; OnURLLoadingFinished is called */
	void StopLoading(URL &url);

	/** Stop loading all currently loading URLs; OnURLLoadingFinished is called for each */
	void StopLoadingAll();

	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	/**	The specified URL has finished loading. This call does not indicate whether 
	 *	or not the load was a success; that is indicated to the handler.
	 *
	 *	@param	url		The URL that has been finished
	 */
	virtual void OnURLLoadingFinished(URL &url);

	/** All currently loading URLs have been loaded, regardless of success. */
	virtual void OnAllDocumentsFinished();

	/** Return the Message handler being used */
	MessageHandler *GetMessageHandler() const{return document_mh;}

	/** TRUE if there are URLs being loaded */
	BOOL ActiveLoadingURLs() const{return !loading_urls.Empty();};

	/** Set the timeout for the loading */
	void SetTimeOut(uint32 val){timeout_max = val;}

	/** Get the timeout for the loading */
	uint32 GetTimeOut() const {return timeout_max;}
	
	/** Set the idle timeout for the loading */
	void SetTimeOutIdle(uint32 val){timeout_idle = val;}

	/** Get the idle timeout for the loading */
	uint32 GetTimeOutIdle() const {return timeout_idle;}

	/** Set the delay before idle timeout for the loading */
	void SetTimeOutIdleDelay(uint32 val){timeout_minidle = val;}

	/** Get the delay before idle timeout for the loading */
	uint32 GetTimeOutIdleDelay() const {return timeout_minidle;}

	/** Set the flag for starting timeout when sending request for the loading */
	void SetTimeOutStartRequest(BOOL val){timeout_startreq = val;}

	/** Get the flag for starting timeout when sending request  for the loading */
	BOOL GetTimeOutStartRequest() const {return timeout_startreq;}
	
	/** Set the flag for starting timeout when starting connection */
	void SetTimeOutStartConnection(BOOL val){timeout_startconn = val;}

	/** Get the flag for starting timeout when starting connection */
	BOOL GetTimeOutStartConnection() const {return timeout_startconn;}

};

#endif // _URL_DOCLOADER_H_
