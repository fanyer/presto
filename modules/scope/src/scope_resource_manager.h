/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef SCOPE_RESOURCE_MANAGER_H
#define SCOPE_RESOURCE_MANAGER_H

#ifdef SCOPE_RESOURCE_MANAGER

#include "modules/scope/src/scope_service.h"
#include "modules/scope/src/generated/g_scope_resource_manager_interface.h"
#include "modules/scope/src/scope_idtable.h"
#include "modules/url/url_dynattr.h"

class Header_Item;
class Header_List;
class Upload_EncapsulatedElement;

class OpScopeResourceManager
	: public OpScopeResourceManager_SI
{
public:
	OpScopeResourceManager();
	virtual ~OpScopeResourceManager();

	virtual OP_STATUS OnPostInitialization();

	OP_STATUS Construct(OpScopeIDTable<DocumentManager> *frame_ids,
						OpScopeIDTable<FramesDocument> *document_ids,
						OpScopeIDTable<unsigned> *resource_ids);

	// OpScopeService
	virtual OP_STATUS OnServiceEnabled();
	virtual OP_STATUS OnServiceDisabled();

	OP_STATUS OnUrlLoad(URL_Rep *url_rep, DocumentManager *docman, Window *window);
	OP_STATUS OnUrlFinished(URL_Rep *url_rep, URLLoadResult result);
	OP_STATUS OnUrlRedirect(URL_Rep *orig_url_rep, URL_Rep *new_url_rep);
	OP_STATUS OnUrlUnload(URL_Rep *rep);

	OP_STATUS OnComposeRequest(URL_Rep *rep, int request_id, Upload_EncapsulatedElement *upload, DocumentManager *docman, Window *window);
	OP_STATUS OnRequest(URL_Rep *rep, int request_id, Upload_EncapsulatedElement *upload, const char* buf, size_t len);
	OP_STATUS OnRequestFinished(URL_Rep *rep, int request_id, Upload_EncapsulatedElement *upload);
	OP_STATUS OnRequestRetry(URL_Rep *rep, int orig_request_id, int new_request_id);
	OP_STATUS OnResponse(URL_Rep *rep, int request_id, int response_code);
	OP_STATUS OnResponseHeader(URL_Rep *rep, int request_id, const HeaderList *headerList, const char* buf, size_t len);
	OP_STATUS OnResponseFinished(URL_Rep *rep, int request_id);
	BOOL ForceFullReload(URL_Rep *rep, DocumentManager *docman, Window *window);
	OP_STATUS OnXHRLoad(URL_Rep *url_rep, BOOL cached, DocumentManager *docman, Window *window);
	OP_STATUS OnDNSStarted(URL_Rep *url_rep, double prefetchtimespent);
	OP_STATUS OnDNSEnded(URL_Rep *url_rep, OpHostResolver::Error errorcode);

	// Request/Response functions
	virtual OP_STATUS DoGetResource(const GetResourceArg &in, ResourceData &out);
	virtual OP_STATUS DoSetReloadPolicy(const SetReloadPolicyArg &in);
	virtual OP_STATUS DoSetResponseMode(const SetResponseModeArg &in);
	virtual OP_STATUS DoSetRequestMode(const SetRequestModeArg &in);
	virtual OP_STATUS DoAddHeaderOverrides(const AddHeaderOverridesArg &in);
	virtual OP_STATUS DoRemoveHeaderOverrides(const RemoveHeaderOverridesArg &in);
	virtual OP_STATUS DoClearHeaderOverrides();
	virtual OP_STATUS DoCreateRequest(const CreateRequestArg &in, ResourceID &out);
	virtual OP_STATUS DoClearCache();
	virtual OP_STATUS DoGetResourceID(const GetResourceIDArg &in, ResourceID &out);

	/**
	 * Get the HTTP request's internal id.
	 *
	 * @return The request id if found, -1 if not.
	 */
	int GetHTTPRequestID(URL_Rep *url_rep);

private:

	static OP_STATUS Convert(OpHostResolver::Error status_in, DnsLookupFinished::Status &status_out);

	/**
	 * An Upload_EncapsulatedElement is a composite which contains
	 * an inner Upload_Base. The outer Upload_EncapsulatedElement
	 * has some HTTP headers, but Upload_Base may provide additional
	 * headers.
	 *
	 * We need to enumerate all headers for the upload as a whole, and
	 * override individual headers for the upload as a whole. This class
	 * makes this process easier by hiding the locations of each header.
	 */
	class HeaderManager
	{
	public:

		/**
		 * Create a new HeaderManager.
		 *
		 * @param upload The Upload_EncapsulatedElement. Can not be NULL.
		 */
		HeaderManager(Upload_EncapsulatedElement *upload);

		/**
		 * Get the first Header_Item for an upload, or NULL
		 * if there are no headers for this upload.
		 */
		Header_Item *First();

		/**
		 * Get the next Header_Item, or NULL if there is
		 * no next item.
		 */
		Header_Item *Next();

		/**
		 * Override a named header with the specified value. If no such
		 * header exists, one will be created.
		 *
		 * @param name The name of the header, e.g. "Content-Type".
		 * @param value The value of the headers, e.g. "text/plain".
		 * @return OpStatus::OK, or OpStatus::ERR_NO_MEMORY.
		 */
		OP_STATUS Override(const OpString8 &name, const OpString8 &value);

		/**
		 * Remove all headers from the upload.
		 */
		void RemoveAll();

	private:

		// The 'outer' element.
		Upload_EncapsulatedElement *upload;

		// The 'inner' element, ensapsulated by 'upload'.
		Upload_Base *element;

		// Current state of iteration.
		Upload_Base *current_base;
		Header_Item *current_item;

	}; // HeaderManager

	class ContentReader
	{
	public:
		virtual OP_STATUS Read(ByteBuffer &buf, OpString8 &charset, const uni_char *&err) = 0;
	};

	class ResponseContentReader : public ContentReader
	{
	public:
		ResponseContentReader(URL_Rep *url_rep, const ContentMode &mode);
		virtual OP_STATUS Read(ByteBuffer &buf, OpString8 &charset, const uni_char *&err);
	private:
		URL_Rep *url_rep;
		const ContentMode &mode;
	};

	class RequestContentReader : public ContentReader
	{
	public:
		RequestContentReader(Upload_Base *element, char *net_buf, unsigned net_buf_len);
		virtual OP_STATUS Read(ByteBuffer &buf, OpString8 &charset, const uni_char *&err);
	private:
		Upload_Base *element;
		char *net_buf;
		unsigned net_buf_len;
	};

	/**
	 * A ContentFilter is a setting which describes how data should be
	 * sent to the client (bytes, string, data uri, or 'off') based
	 * on the mime type of a resource.
	 *
	 * ContentFilters can be chained, so that the parent is consulted if
	 * there is no information in the filter in question.
	 */
	class ContentFilter
	{
	public:

		/**
		 * Create a new ContentFilter.
		 *
		 * @param parent The parent ContentFilter, or NULL if there is
		 *               no parent.
		 */
		ContentFilter(ContentFilter *parent);

		/**
		 * Get the content mode for a resource with the specified mime type. This
		 * will recursively check parents if no information is found in this filter.
		 *
		 * @param [in] mime The mime type string of the resource, e.g. "text/plain".
		 * @param [out] mode The ContentMode for the resource.
		 */
		OP_STATUS GetContentMode(const char *mime, ContentMode *&mode);

		/**
		 * Reset the object to its initial state (as if it were just created).
		 */
		virtual void Reset() = 0;

	protected:

		/**
		 * Get the ContentMode for a mime type for this filter (not from
		 * parents).
		 *
		 * @param mime The mime type string of the resource, e.g. "text/plain".
		 * @return The ContentMode corresponding to the mime type, or NULL if
		 *         there is no information.
		 */
		virtual ContentMode *GetContentMode(const char *mime) = 0;

		// The parent filter, or NULL if this is a top level filter.
		ContentFilter *parent;

	}; // ContentFilter

	/**
	 * A filter which ignores the mime type, and just returns a
	 * fixed ContentMode.
	 */
	class DefaultContentFilter : public ContentFilter
	{
	public:

		/**
		 * Creates a new default filter with the initial ContentMode.
		 */
		DefaultContentFilter();

		// From ContentFilter
		void Reset();

		/**
		 * Changed the ContentMode returned by this filter.
		 *
		 * @param mode The new ContentMode.
		 */
		void Set(const ContentMode &mode);

	protected:

		// From ContentFilter
		virtual ContentMode *GetContentMode(const char *mime);

	private:

		// The one-and-only mode returned by this filter.
		ContentMode defaultMode;
	};

	/**
	 * A filter which maintains a hash table of [mime-type, ContentMode]
	 * pairs.
	 */
	class MimeContentFilter : public ContentFilter
	{
	public:

		/**
		 * Create a new MimeContentFilter with an empty hash table.
		 */
		MimeContentFilter(ContentFilter *parent);

		/**
		 * Add a [mime-type, ContentMode] pair to the hash table.
		 *
		 * @param A mime type string e.g. "text/plain".
		 * @param mode The ContentMode to use for this mime type.
		 * @return OpStatus::OK, OpStatus::ERR if the key already
		 *         exists, or OpStatus::ERR_NO_MEMORY.
		 */
		OP_STATUS Add(const char *mime, const ContentMode &mode);

		// From ContentFilter
		void Reset();

		/**
		 * Reset the entire hash table with the input vector.
		 *
		 * @param mimeList A list of [mime-type, ContentMode] pairs.
		 * @return OpStatus::OK, or OpStatus::ERR_NO_MEMORY.
		 */
		OP_STATUS Set(const OpProtobufMessageVector<MimeMode> &mimeList);

	protected:

		// From ContentFilter
		virtual ContentMode *GetContentMode(const char *mime);

	private:

		/**
		 * For each ContentMode that goes into a StringHashTable, we need
		 * to keep the key alive. In the hash table mime_modes, the key is
		 * stored in the value object itself (InternalContentMode). This
		 * should guarantee that the key is alive as long as the value is
		 * alive.
		 */
		struct Item
		{
			OpString8 key;
			ContentMode mode;
		};

		// Stores the modes for each mime type.
		OpAutoString8HashTable<Item> modes;

	}; // MimeContentFilter

	/**
	 * When a URL is loaded, its context is stored in this object, so
	 * it can be accessed later. The IDs contained in this object are
	 * generated when the object is constructed, so IDs will not be
	 * regenerated each time they are retrieved.
	 *
	 * When you would like to retrieve the *actual* Window or
	 * DoucmentManager however, we must verify that these objects
	 * still exists (they may very well have been deleted).
	 */
	class ResourceContext
	{
	public:

		/**
		 * Creates a new ResourceContext. Don't forget to call Construct
		 * right after.
		 *
		 * @param rm The parent OpScopeResourceManager. Needed to generated IDs.
		 */
		ResourceContext(OpScopeResourceManager *rm);

		/**
		 * Generates IDs for the DocumentManager and Window, and store
		 * them for retrieval later.
		 *
		 * @param docman The associated DocumentManager (can be NULL).
		 * @param window The associated Window (can only be NULL *if*
		 *               docman is not also NULL).
		 *
		 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY.
		 */
		OP_STATUS Construct(DocumentManager *docman, Window *window);

		/**
		 * Get the ID of the DocumentManager associated with this resource.
		 * (There is no guarantee that the corresponding DocumentManager
		 * actually exists).
		 */
		unsigned GetFrameID() const;

		/**
		 * Get the ID of the window associated with this resource. (There
		 * is no guarantee that the corresponding Window actually exists).
		 */
		unsigned GetWindowID() const;

		/**
		 * Returns the DocumentManager associated with the resource *if*
		 * the DocumentManager still exists in the document tree.
		 *
		 * @return The associated DocumentManager, or NULL if that
		 *         DocumentManager no longer exists. This can also
		 *         be NULL if the resource is not associated with
		 *         a DocumentManager.
		 */
		DocumentManager *GetDocumentManager();

		/**
		 * Returns the Window associated with the resource *if*
		 * the Window still exists in the list of windows.
		 *
		 * @return The associated Window, or NULL if that Window
		 *         no longer exists.
		 */
		Window *GetWindow();

	private:
		OpScopeResourceManager *resource_manager;
		DocumentManager *docman;
		Window *window;
		unsigned frame_id;
		unsigned window_id;
	}; // ResourceContext

	/**
	 * When a request created with CreateRequest needs to override
	 * or replace existing headers, we store the request in this object,
	 * so OnComposeRequest can make the necessary modifications.
	 */
	class CustomRequest
	{
	public:
		CustomRequest(URL url, CreateRequestArg::HeaderPolicy header_policy);
		void SetReloadPolicy(ReloadPolicy reload_policy);
		void SetRequestContentMode(ContentMode content_mode);
		void SetResponseContentMode(ContentMode content_mode);
		CreateRequestArg::HeaderPolicy GetHeaderPolicy() const;
		ReloadPolicy GetReloadPolicy() const;
		ContentMode *GetRequestContentMode();
		ContentMode *GetResponseContentMode();
		OpVector<Header> &GetHeaders();
		BOOL HasReloadPolicy() const;
	private:
		CreateRequestArg::HeaderPolicy header_policy;
		ReloadPolicy reload_policy;
		ContentMode request_content_mode;
		ContentMode response_content_mode;
		OpAutoVector<Header> headers;
		BOOL has_reload_policy;
		BOOL has_request_content_mode;
		BOOL has_response_content_mode;
		URL url; // Keep a strong reference to the custom URL.
	};

	/**
	 * Used to remove resource contexts and custom requests.
	 */
	class ResourceRemover
	{
	public:
		ResourceRemover(OpScopeResourceManager *resource_manager, URL_Rep *url_rep);
		~ResourceRemover();
	private:
		OpScopeResourceManager *resource_manager;
		URL_Rep *url_rep;
	};

	// Friends
	friend class ResourceContext;
	friend class ResourceRemover;

	/**
	 * If the last character of 'str' (i.e. str[strlen(str)-1]) is equal
	 * to 'c', replace it with '\0'.
	 */
	void RemoveTrailingChar(char *str, char c);
	OP_STATUS SetMethod(const URL_Rep *url_rep, OpString &str);
	OP_STATUS GetResponseMode(URL_Rep *url_rep, ContentMode *&mode);
	OP_STATUS GetRequestMode(URL_Rep *url_rep, Upload_Base *element, ContentMode *&mode);
	OP_STATUS GetMultipartRequestMode(Upload_Base *element, ContentMode *&mode);
	OP_STATUS GetContentMode(ContentFilter &f, const char *mime, ContentMode *&mode);
	OP_STATUS SetResourceData(ResourceData &msg, URL_Rep *url_rep, const ContentMode &mode);
	OP_STATUS SetContent(Content &msg, URL_Rep *url_rep, ContentReader &reader, const ContentMode &mode, const OpStringC8 &encoding, const OpStringC8 &mime);
	OP_STATUS SetHeader(Header &msg, Header_Item *item, OpAutoArray<char> &hbuf, unsigned &hbuf_len);
	OP_STATUS SetUrlFinished(UrlFinished &msg, unsigned resource_id, URLLoadResult result);
	BOOL AcceptResource(URL_Rep *url_rep);
	BOOL AcceptContext(DocumentManager *docman, Window *window);
	BOOL AcceptWindow(Window *window);
	OP_STATUS GetDocumentID(FramesDocument *frm_doc, unsigned &id);
	OP_STATUS GetFrameID(DocumentManager *docman, unsigned &id);
	OP_STATUS GetResourceID(URL_Rep *url_rep, unsigned &id);

	/**
	 * Get a URL_Rep with a certain resource ID. This function must verify
	 * (brute-force) that a URL is still valid.
	 *
	 * @param [in] id The resource ID to look for.
	 * @param [out] url_rep Variable to store the result in.
	 * @return OpStatus::OK, or SetCommandError(BadRequest) if the URL no
	 *         longer exists, or never existed in the first place.
	 */
	OP_STATUS GetResource(unsigned id, URL_Rep *&url_rep);

	OP_STATUS AddResourceContext(URL_Rep *url_rep, DocumentManager *docman, Window *window);
	void GetResourceContext(URL_Rep *url_rep, ResourceContext *&context);
	void RemoveResourceContext(URL_Rep *url_rep);
	CustomRequest *GetCustomRequest(URL_Rep *url_rep);
	void RemoveCustomRequest(URL_Rep *url_rep);
	void RemoveRedirectedResource(URL_Rep *url_rep);

	// Pointers to ID tables. The actual objects are owned by OpScopeManager.
	OpScopeIDTable<DocumentManager> *frame_ids;
	OpScopeIDTable<FramesDocument> *document_ids;
	OpScopeIDTable<unsigned> *resource_ids;

	// Contains information on all active resources registered by this service.
	// Each resource can be looked up using the URL ID.
	OpPointerHashTable<unsigned, ResourceContext> active_resources;

	// If a resource was redirected it is present in this table, and removed in OnResponseFinished.
	OpGenericPointerSet redirected_resources;

	// Global reload policy. This controls how resources are fetched
	// over the network if they already exist in cache.
	ReloadPolicy reload_policy;

	// Content filters.
	DefaultContentFilter filter_request_default;
	DefaultContentFilter filter_response_default;
	MimeContentFilter filter_request_mime;
	MimeContentFilter filter_response_mime;
	MimeContentFilter filter_request_mime_multipart;

	// Buffer used to read upload data.
	OpAutoArray<char> net_buf;
	unsigned net_buf_len;

	// Used by OnComposeRequest to overwrite headers on all requests.
	OpAutoStringHashTable<Header> header_overrides;

	// Used by OnComposeRequest to overwrite headers on custom requests.
	OpAutoPointerHashTable<URL_Rep, CustomRequest> custom_requests;

	// The XHR attribute.
	URL::URL_Uint32Attribute xhr_attr;

	// The handler for the XHR attribute.
	URL_DynamicUIntAttributeHandler xhr_attr_handler;

	// Used to get the HTTP request id.
	OpPointerIdHashTable<URL_Rep, int> http_requests;

#ifdef SCOPE_WINDOW_MANAGER_SUPPORT
	OpScopeWindowManager *window_manager;
#endif // SCOPE_WINDOW_MANAGER_SUPPORT
};

#endif // SCOPE_RESOURCE_MANAGER
#endif // SCOPE_RESOURCE_MANAGER_H
