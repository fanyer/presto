/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef LOADINLINEELM_H
#define LOADINLINEELM_H

#include "modules/logdoc/helistelm.h"
#include "modules/url/url2.h"
#include "modules/util/simset.h"
#include "modules/doc/externalinlinelistener.h"
#include "modules/url/url_loading.h"

#ifdef CORS_SUPPORT
class OpCrossOrigin_Request;
class LoadInlineElm_SecurityCheckCallback;
#endif // CORS_SUPPORT

/**
 * When we handle an external resource that has to be loaded seperatly
 * we create a LoadInlineElm to keep track of the external resource.
 */
class LoadInlineElm : public Link
{
private:

	URL			url;
	URL_InUse	url_in_use;
	URL_InUse	redirected_url_in_use;
	IAmLoadingThisURL url_reserved;
	IAmLoadingThisURL url_reserved_for_data_async_msg;

	HEList		helm_list;

	List<ExternalInlineListener> *externallisteners;
	UrlImageContentProvider *provider;
	FramesDocument* doc;

	union
	{
		struct
		{
			//unsigned long	inline_type:16;
			unsigned long	loading:1;
			unsigned long	loaded:1;
			unsigned long	is_visible:1;
#ifdef SUPPORT_VISUAL_ADBLOCK
			unsigned long	is_contentblock_rendered:1;	// set if the adblocker should render this specially
#endif // SUPPORT_VISUAL_ADBLOCK
			unsigned long	is_processing:1;
			unsigned long	need_reset_image:1;
			unsigned long	sent_handle_inline_data_async_msg:1;
			unsigned long	need_security_update:1;
			unsigned long	access_forbidden:1; ///< If the inline security check failed.
#ifdef CORS_SUPPORT
			unsigned long	is_cors_request:1; ///< If the inline was loaded using a CORS request.
#endif // CORS_SUPPORT
			unsigned long	is_current_doc:1;
			unsigned long	delay_load:1;
#ifdef SPECULATIVE_PARSER
			unsigned long	is_speculative_load:1; ///< Load initiated by speculative parser.
#endif // SPECULATIVE_PARSER
		} info;
		unsigned long	info_init;
	};

	OpFileLength last_loaded_size;
	OpFileLength last_total_size;

#ifdef CORS_SUPPORT
	OpCrossOrigin_Request *cross_origin_request;
	/**< For the duration of a cross-origin load, CORS
	     needs to keep track of the request so as to
	     be able to perform the final resource sharing
	     check. Owned by this element. */

	LoadInlineElm_SecurityCheckCallback *security_callback;
#endif // CORS_SUPPORT

	friend class HEListElm;
	void CheckDelayLoadState();

public:

	/**
	 * Create a LoadInlineElm. Normally followed by calling
	 * FramesDocument::AddInline.
	 *
	 * @param[in] curl The url of the external resource.
	 *
	 * @param[in] inline_type For what purpose we load it. For
	 * instance IMAGE_INLINE or SCRIPT_INLINE. This is unused except
	 * for applets and should be removed completely as soon as we've
	 * found a better solution for applets.
	 */
				LoadInlineElm(FramesDocument* doc, const URL& curl);
	virtual 	~LoadInlineElm();

	/**
	 * Checks if a resource is needed.
	 *
	 * @returns TRUE if there are noone registered to be interested in
	 * this resource.
	 */
	BOOL		IsUnused() { return helm_list.Empty() && !externallisteners; }

	/**
	 * Check if this LoadInlineElm is loading the url indicated by the URL_ID.
	 *
	 * @param[in] id id of the URL to check.
	 */
	BOOL		IsLoadingUrl(URL_ID id);

	/**
	 * @returns The url of the external resource.
	 */
	URL*		GetUrl() { return &url; }

	/**
	 * @returns The currently redirected url of the external resource.
	 */
	URL*		GetRedirectedUrl() { return redirected_url_in_use.GetURL().IsEmpty() ? &url : &redirected_url_in_use.GetURL(); }

	/**
	 * Inform the LoadInlineElm that the url was redirected
	 * to url with id target_id. For more information check the url.
	 */
	void        UrlMoved(FramesDocument *document, URL_ID target_id);

	/**
	 * Resets the effects of a previous redirection for this element. Used by
	 * FramesDocument::LocalLoadInline() if we are starting a new load of a
	 * url that was previously redirected.
	 */
	void		ResetRedirection();

	/**
	 * @returns Whether we are currently loading this resource.
	 */
	BOOL		GetLoading() const { return info.loading; }

	/**
	 * @returns If we have finished loading this resource.
	 */
	BOOL		GetLoaded() const { return info.loaded; }

	/**
	 * @returns TRUE if this resource affects the document's
	 *          progress display and delays the load event,
	 *          FALSE otherwise.
	 */
	BOOL		GetDelayLoad() const { return info.delay_load; }

#ifdef SUPPORT_VISUAL_ADBLOCK
	BOOL		GetContentBlockRendered() { return info.is_contentblock_rendered; }
	void		SetContentBlockRendered(BOOL rendered) { info.is_contentblock_rendered = rendered; }
#endif // SUPPORT_VISUAL_ADBLOCK

	/**
	 * Update flags so that we know that we either is loading it or
	 * not loading it right now.
	 *
	 * @param[in] val TRUE if we are now loading it, FALSE if we are
	 * not (anymore?) loading it.
	 */
	void		SetLoading(BOOL val);

	/**
	 * Update flags so that we know if we've loaded it as much as we can.
	 *
	 * @param[in] val TRUE if we're finished with it, FALSE if have
	 * not yet loaded it.
	 */
	void		SetLoaded(BOOL val) { info.loaded = val; }

	/**
	 * Set if we're currently interested in this resource. If we say
	 * that we are not, it might be freed from caches (which is a good
	 * thing).
	 *
	 * @param[in] used TRUE if we need the resource to be locked.
	 */
	void        SetUsed(BOOL used);

	/**
	 * Set a flag saying that we should call
	 *
	 *   UrlImageContentProvider::ResetImageFromUrl()
	 *
	 * when we receive a positive loading message (non-failure and not
	 * MSG_NOT_MODIFIED) for this URL.
	 */
	void		SetNeedResetImage() { info.need_reset_image = 1; }
	void		UnsetNeedResetImage() { info.need_reset_image = 0; }

	/**
	 * Returns the last StorageId of the resource kept in the image cache.
	 */
	UINT32		GetImageStorageId();

	/**
	 * Our owner document became, or stopped being, the current doc; that is,
	 * its IsCurrentDoc() function's return value just changed.
	 */
	void		UpdateDocumentIsCurrent();

	/**
	 * Returns TRUE if this inline needs to update and report its security
	 * before use.
	 */
	BOOL		GetNeedSecurityUpdate() { return info.need_security_update; }

	/**
	 * Changes the flag that says if we need to update the security.
	 *
	 * @param val The new value.
	 */
	void		SetNeedSecurityUpdate(BOOL val) { info.need_security_update = val; }

	/**
	 * Calls
	 *
	 *   UrlImageContentProvider::ResetImageFromUrl()
	 *
	 * if SetNeedResetImage() has been called.  Also resets the flag.
	 */
	void		ResetImageIfNeeded();

	/**
	 * Check if element has already registered a load of this resource.
	 *
	 * @param[in] element The element.
	 *
	 * @param[in] inline_type The inline type.
	 */
	BOOL		IsElementAdded(HTML_Element* element, InlineResourceType inline_type);

	/**
	 * Register an HTML_Element as interested in this resource.
	 *
	 * @param[in] element The element.
	 *
	 * @param[in] inline_type The inline type.
	 *
	 * @param[in] doc The document that had the element.
	 *
	 * @param[in] img_url The resource. Since this already exists in
	 * LoadInlineElm::url this parameter should most likely just be
	 * removed.
	 *
	 * @returns The created HEListElm or NULL if the operation failed
	 * (Out of memory).
	 *
	 * @todo Why do we have an url here and another url in the
	 * LoadInlineElm constructor? And the same question about
	 * inline_type.
	 */
	HEListElm*	AddHEListElm(HTML_Element* element, InlineResourceType inline_type, FramesDocument* doc, URL* img_url, BOOL update_url);

    /**
	 * Remove element . Example of use, remove element from
	 * loadelements list of elements.  Use when element changes and
	 * needs to reflow its href.
	 *
	 * @param[in] element The element.
	 *
	 * @param[in] inline_type The inline type.
	 */
    BOOL        RemoveHEListElm(HTML_Element* element, InlineResourceType inline_type);

	/**
	 * Check if someone is trying to load this resource as an
	 * |inline_type|.
	 *
	 * @param[in] inline_type The type to check, for instance
	 * IMAGE_INLINE.
	 *
	 * @returns TRUE if there are someone trying to load this as an
	 * |inline_type|, FALSE otherwise.
	 */
	BOOL		HasInlineType(InlineResourceType inline_type);

	/**
	 * Returns the first HEListElm registered in this
	 * LoadInlineElm. The caller can then iterate with the
	 * HEListElm::Suc() method
	 *
	 * @returns The first HEListElm or NULL if there are none. Even if
	 * it returns NULL there can be people interested in it (for
	 * instance applets or people using externallisteners).
	 */
	HEListElm*	GetFirstHEListElm() const { return helm_list.First(); }

	/**
	 * Returns if this inline is only being loaded as image and
	 * background images.
	 *
	 * @returns FALSE if this inline is being loaded as something
	 * other than images and background images (defined by HEListElm::IsImageInline()).
	 */
	BOOL		HasOnlyImageInlines();

	/**
	 * Returns if this inline is only being loaded as inline_type.
	 *
	 * @param[in] inline_type The type to check, for instance
	 * IMAGE_INLINE.
	 *
	 * @returns TRUE if this inline is being loaded only as inline_type,
	 *          FALSE if it's being loaded as some other type of inline, or
	 *          not loading at all.
	 */
	BOOL		HasOnlyInlineType(InlineResourceType inline_type);

	/**
	 * Adds an HEListElm without any checks.
	 *
	 * @param[in] helm The element to add.
	 */
	void		Add(HEListElm* helm);

	/**
	 * Removes an HEListElm without any checks.
	 *
	 * @param[in] helm The element to add.
	 */
	void		Remove(HEListElm* helm);

	/**
	 * @Returns the amount of bytes that the url says it has loaded,
	 * and updates what GetLastLoadedSize() will return.
	 */
	OpFileLength GetLoadedSize();

	/**
	 * @Returns the amount of bytes that the url says it will have to
	 * load, and updates what GetLastTotalSize() will return.
	 */
	OpFileLength GetTotalSize();

	/**
	 * @returns what the last call to GetLoadedSize() returned.
	 */
	OpFileLength GetLastLoadedSize() { return last_loaded_size; }

	/**
	 * @returns what the last call to GetTotalSize() returned.
	 */
	OpFileLength GetLastTotalSize() { return last_total_size; }

	/**
	 * Adds an external listener for the load. It will be informed of
	 * important events.
	 *
	 * @param[in] listener The listener to get called.
	 *
	 * @see ExternalInlineListener.
	 */
	OP_STATUS AddExternalListener(ExternalInlineListener *listener);

	/**
	 * Removes a previously added listener.
	 *
	 * @param[in] listener The listener to be removed.
	 *
	 * @returns TRUE if the listener was found in the list of listeners
	 */
	BOOL RemoveExternalListener(ExternalInlineListener *listener);

	/**
	 * @returns the first or the registered listeners or NULL if there
	 * was none. Use ExternalInlineListener::Suc() to reach the
	 * others.
	 */
	ExternalInlineListener *GetFirstExternalListener() { return externallisteners ? externallisteners->First() : NULL; }

	/**
	 * Register the UrlImageContentProvider with the LoadInlineElm.
	 *
	 * @param[in] provider The provider to register. NULL to
	 * deregister it.
	 */
	void		SetUrlImageContentProvider(UrlImageContentProvider *provider);

	/**
	 * While we are receiving/handling data for an LoadInlineElm we
	 * mark it with the "IsProcessing" flag so that we don't
	 * accidently does something stupid to it, like delete it.
	 *
	 * @param[in] If we are currently handling data for the
	 * LoadInlineElm.
	 */
	void		SetIsProcessing(BOOL value) { info.is_processing = !!value; }

	/**
	 * @returns If we are currently handling data for the
	 * LoadInlineElm.
	 */
	BOOL		GetIsProcessing() { return info.is_processing; }

	/**
	 * Returns if loading this inline is forbidden due to security policies.
	 */
	BOOL		GetAccessForbidden() { return info.access_forbidden; }

	/**
	 * Checks if there has been a cross-network load or another restriction
	 * that prevents this inline from being used and sets the proper flags.
	 */
	void		CheckAccessForbidden();

	/**
	 * Returns TRUE if a URL in this inline's redirect chain has the
	 * given ID.  It need not be the first or the last URL in the
	 * redirect chain.
	 */
	BOOL		IsAffectedBy(URL_ID id);

	/**
	 * Returns TRUE if there is already a MSG_HANDLE_INLINE_DATA_ASYNC
	 * message in the message queue for this inline.
	 */
	BOOL		HasSentHandleInlineDataAsyncMsg() { return info.sent_handle_inline_data_async_msg; }

	/**
	 * Changes the flag that says if we have a MSG_HANDLE_INLINE_DATA_ASYNC
	 * message in the message queue.
	 *
	 * @param[in] val The new value.
	 */
	void		SetSentHandleInlineDataAsyncMsg(BOOL val);

#ifdef SPECULATIVE_PARSER
	/**
	 * Returns TRUE if this is a load initiated by the speculative parser.
	 */
	BOOL		IsSpeculativeLoad() { return info.is_speculative_load; }

	/**
	 * Sets if this is a speculative load initiated by the speculative parser.
	 */
	void		SetIsSpeculativeLoad(BOOL val) { info.is_speculative_load = !!val; }

#endif // SPECULATIVE_PARSER

#ifdef CORS_SUPPORT
	/**
	 * Returns TRUE if this inline was loaded by performing a CORS-enabled load.
	 */
	BOOL		IsCrossOriginRequest() { return info.is_cors_request; }

	/**
	 * If this inline is performing a CORS-enabled load,
	 * return the cross-origin request for this element.
	 * It will be non-NULL only while checking with CORS
	 * if the inline is allowed.
	 *
	 */
	OpCrossOrigin_Request*		GetCrossOriginRequest() { return cross_origin_request; }

	/**
	 * If a resource should be attempted loaded via a CORS-enabled
	 * fetch, set up the inline accordingly. This is performed with
	 * respect to an element, it carrying an attribute controlling if
	 * the CORS request should be credentialled or not.
	 *
	 * @param origin_url Origin from where the image is loaded.
	 * @param inline_url The target resource to access.
	 * @param html_elm The element from which crossorigin state is
	 *                 determined (usually the element for which the
	 *                 inline load is attempted).
	 * @return OpStatus::OK on success.
	 *         OpStatus::ERR_NO_MEMORY on OOM.
	 *
	 */
	OP_STATUS		SetCrossOriginRequest(const URL &origin_url, URL *inline_url, HTML_Element *html_elm);

	/**
	 * Upon receiving the response from a CORS-allowed inline
	 * load, check if it passes the CORS resource sharing check.
	 * If it does, mark the attached HEListElms as being
	 * "CORS allowed" and set 'allowed' to TRUE.
	 *
	 * The completion of this check also marks the end of
	 * the CORS-involved part of the loading and the underlying
	 * CORS request will be released as a result. Hence the
	 * access check can and must only be performed once.
	 *
	 * @param [out]allowed Result of CORS access check.
	 * @return OpStatus::OK on successfully performed check.
	 *         OpStatus::ERR_NO_MEMORY on OOM.
	 *
	 */
	OP_STATUS 		CheckCrossOriginAccess(BOOL &allowed);

	/**
	 * Called from the security check callback when it has finished.
	 */
	void 			OnSecurityCheckResult(BOOL allowed);

#endif // CORS_SUPPORT
};


#endif // LOADINLINEELM_H
