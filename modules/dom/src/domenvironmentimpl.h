/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2003-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef DOM_ENVIRONMENTIMPL_H
#define DOM_ENVIRONMENTIMPL_H

#include "modules/dom/domenvironment.h"
#include "modules/dom/src/domobj.h"
#include "modules/util/OpHashTable.h"
#include "modules/util/simset.h"
#ifdef CLIENTSIDE_STORAGE_SUPPORT
#include "modules/dom/src/storage/storage.h"
#endif //CLIENTSIDE_STORAGE_SUPPORT
#ifdef DOM_SUPPORT_BLOB_URLS
#include "modules/dom/src/domfile/dombloburl.h"
#endif // DOM_SUPPORT_BLOB_URLS
#include "modules/dom/src/domcollectiontracker.h"

class DOM_Runtime;
class DOM_Element;
class JS_Window;
class DOM_Node;
class DOM_Document;
class DOM_NamedElementsMap;
class DOM_NamedElements;
class DOM_ProxyObject;
class DOM_Event;
class ServerName;
class CSS_DOMRule;
class DOM_AsyncCallback;
class DOM_EnvironmentImpl;
class DOM_StaticNodeList;
class DOM_MutationListener;

#ifdef USER_JAVASCRIPT
class DOM_UserJSManager;
#endif // USER_JAVASCRIPT

#ifdef MEDIA_HTML_SUPPORT
class DOM_HTMLMediaElement;
#endif // MEDIA_HTML_SUPPORT

#ifdef DOM_GADGET_FILE_API_SUPPORT
class DOM_FileBase;
#endif // DOM_GADGET_FILE_API_SUPPORT

#ifdef DOM_WEBWORKERS_SUPPORT
class DOM_WebWorkerController;
#endif // DOM_WEBWORKERS_SUPPORT

#ifdef DOM_CROSSDOCUMENT_MESSAGING_SUPPORT
class DOM_MessagePort;
#endif // DOM_CROSSDOCUMENT_MESSAGING_SUPPORT

#ifdef EVENT_SOURCE_SUPPORT
class DOM_EventSource;
#endif // EVENT_SOURCE_SUPPORT

#ifdef DOM3_LOAD
class DOM_LSLoader;
#endif // DOM3_LOAD

class DOM_FileReader;
#ifdef DOM_SUPPORT_BLOB_URLS
class DOM_BlobURL;
#endif // DOM_SUPPORT_BLOB_URLS

#ifdef DOM_XSLT_SUPPORT
class DOM_XSLTProcessor;
#endif // DOM_XSLT_SUPPORT

#ifdef DOM_JIL_API_SUPPORT
class DOM_JILAudioPlayer;
class DOM_JILCamera;
class DOM_JILVideoPlayer;
#endif // DOM_JIL_API_SUPPORT

#ifdef DOM_JIL_API_SUPPORT
class DOM_JILWidget;
#endif // DOM_JIL_API_SUPPORT

#ifdef WEBSOCKETS_SUPPORT
class DOM_WebSocket;
#endif // WEBSOCKETS_SUPPORT

#ifdef TOUCH_EVENTS_SUPPORT
class DOM_Touch;
class DOM_TouchList;
#endif // TOUCH_EVENTS_SUPPORT

#ifdef EXTENSION_SUPPORT
class DOM_ExtensionBackground;
#endif // EXTENSION_SUPPORT

class DOM_MutationListener
	: private Link
{
public:
	friend class DOM_EnvironmentImpl;

	DOM_MutationListener *Next();

	enum ValueModificationType
	{
		// Must be same order as the HTML_Element::ValueModificationType type.
		REPLACING_ALL,
		DELETING,
		INSERTING,
		REPLACING,
		SPLITTING
	};

	virtual void OnTreeDestroyed(HTML_Element *tree_root);
	virtual OP_STATUS OnAfterInsert(HTML_Element *child, DOM_Runtime *origining_runtime);
	virtual OP_STATUS OnBeforeRemove(HTML_Element *child, DOM_Runtime *origining_runtime);
	virtual OP_STATUS OnAfterValueModified(HTML_Element *element, const uni_char *new_value, ValueModificationType type, unsigned offset, unsigned length1, unsigned length2, DOM_Runtime *origining_runtime);
	virtual OP_STATUS OnAttrModified(HTML_Element *element, const uni_char *name, int ns_idx, DOM_Runtime *origining_runtime);
};

#ifdef JS_SCOPE_CLIENT
class JS_ScopeClient;
#endif // JS_SCOPE_CLIENT

class DOM_EnvironmentImpl
	: public DOM_Environment
{
public:
	/*=== Creation unloading and destruction ===*/
	virtual void BeforeUnload();
	virtual void BeforeDestroy();

	/*=== Simple getters ===*/
	virtual BOOL IsEnabled();
	virtual ES_Runtime *GetRuntime();
	DOM_Runtime *GetDOMRuntime();
	virtual ES_ThreadScheduler *GetScheduler();
	virtual ES_AsyncInterface *GetAsyncInterface();
	virtual DOM_Object *GetWindow();
	virtual DOM_Object *GetDocument();
	virtual FramesDocument *GetFramesDocument();

	OP_STATUS GetProxyWindow(DOM_Object *&object, ES_Runtime *origining_runtime);
	OP_STATUS GetProxyDocument(DOM_Object *&object, ES_Runtime *origining_runtime);
	OP_STATUS GetProxyLocation(DOM_Object *&object, ES_Runtime *origining_runtime);

	/*=== Security and current executing script ===*/
	virtual BOOL AllowAccessFrom(const URL &url);
	virtual BOOL AllowAccessFrom(ES_Thread* thread);
	virtual ES_Thread *GetCurrentScriptThread();
	virtual DocumentReferrer GetCurrentScriptURL();
	virtual BOOL SkipScriptElements();
	virtual TempBuffer *GetTempBuffer();

	/*=== Creating nodes ===*/
	virtual OP_STATUS ConstructNode(DOM_Object *&node, HTML_Element *element);
	OP_STATUS ConstructNode(DOM_Node *&node, HTML_Element *element, DOM_Document *owner_document);
	OP_STATUS ConstructDocumentNode();

	/*=== Creating DOM_CSSRules ===*/
	virtual OP_STATUS ConstructCSSRule(DOM_Object *&rule, CSS_DOMRule *css_rule);


#ifdef APPLICATION_CACHE_SUPPORT
	virtual OP_STATUS SendApplicationCacheEvent(DOM_EventType event, BOOL lengthComputable = 0, OpFileLength loaded = 0, OpFileLength total = 0);
#endif // APPLICATION_CACHE_SUPPORT

#ifdef WEBSERVER_SUPPORT
	virtual BOOL WebserverEventHasListeners(const uni_char *type);
	virtual OP_STATUS SendWebserverEvent(const uni_char *type, WebResource_Custom *web_resource_script);
#endif // WEBSERVER_SUPPORT

	/*=== Handling events ===*/
	virtual BOOL HasEventHandlers(DOM_EventType type);
	virtual BOOL HasEventHandler(HTML_Element *target, DOM_EventType type, HTML_Element **nearest_handler = NULL);
	virtual BOOL HasWindowEventHandler(DOM_EventType window_event);
	virtual BOOL HasEventHandler(DOM_Object *target, DOM_EventType event, DOM_Object **nearest_handler = NULL);
	virtual BOOL HasLocalEventHandler(HTML_Element *target, DOM_EventType event);
	virtual OP_STATUS SetEventHandler(DOM_Object *node, DOM_EventType event, const uni_char *handler, int handler_length);
	virtual OP_STATUS ClearEventHandler(DOM_Object *node, DOM_EventType event);
	virtual void AddEventHandler(DOM_EventType type);
	virtual void RemoveEventHandler(DOM_EventType type);
	virtual OP_BOOLEAN HandleEvent(const EventData &data, ES_Thread *interrupt_thread = NULL, ES_Thread **event_thread = NULL);
	OP_BOOLEAN HandleError(ES_Thread *thread_with_error, const uni_char *message, const uni_char *resource_url, int resource_line);
#ifdef MEDIA_HTML_SUPPORT
	virtual OP_BOOLEAN HandleTextTrackEvent(DOM_Object* target_object, DOM_EventType event_type, MediaTrack* affected_track = NULL);
#endif // MEDIA_HTML_SUPPORT
#if defined(SELFTEST) && defined(APPLICATION_CACHE_SUPPORT)
	virtual ES_Object* GetApplicationCacheObject();
#endif // SELFTEST && APPLICATION_CACHE_SUPPORT
	OP_BOOLEAN SendEvent(DOM_Event *event, ES_Thread *interrupt_thread = NULL, ES_Thread **event_thread = NULL);
	virtual void ForceNextEvent() { force_next_event = TRUE; }
	OP_STATUS SignalDocumentFinished();

	/*=== Notifications ===*/
	virtual void NewDocRootElement(HTML_Element *element);
	virtual OP_STATUS NewRootElement(HTML_Element *element);
	virtual OP_STATUS ElementInserted(HTML_Element *element);
	virtual OP_STATUS ElementRemoved(HTML_Element *element);
	virtual OP_STATUS ElementCharacterDataChanged(HTML_Element *element, int modification_type, unsigned offset	 = 0, unsigned length1 = 0, unsigned length2 = 0);
	virtual OP_STATUS ElementAttributeChanged(HTML_Element *element, const uni_char *name, int ns_idx);
	virtual void ElementCollectionStatusChanged(HTML_Element *element, unsigned collections);
	virtual void OnSelectionUpdated();

#ifdef USER_JAVASCRIPT
	/*=== User Javascript ===*/
	virtual OP_BOOLEAN HandleScriptElement(HTML_Element *element, ES_Thread *interrupt_thread);
	virtual OP_BOOLEAN HandleCSS(HTML_Element *element, ES_Thread *interrupt_thread);
	virtual OP_BOOLEAN HandleCSSFinished(HTML_Element *element, ES_Thread *interrupt_thread);
	virtual OP_STATUS HandleScriptElementFinished(HTML_Element *element, ES_Thread *script_thread);
	virtual OP_BOOLEAN HandleExternalScriptElement(HTML_Element *element, ES_Thread *interrupt_thread);
	virtual BOOL IsHandlingScriptElement(HTML_Element *element);
#ifdef _PLUGIN_SUPPORT_
	virtual OP_BOOLEAN PluginInitializedElement(HTML_Element *element);
#endif // _PLUGIN_SUPPORT_
	virtual OP_STATUS HandleJavascriptURL(ES_JavascriptURLThread *thread);
	virtual OP_STATUS HandleJavascriptURLFinished(ES_JavascriptURLThread *thread);
	DOM_UserJSManager *GetUserJSManager();
#endif // USER_JAVASCRIPT
#ifdef ECMASCRIPT_DEBUGGER
	virtual OP_STATUS GetESRuntimes(OpVector<ES_Runtime> &es_runtimes);
#endif // ECMASCRIPT_DEBUGGER

#ifdef JS_PLUGIN_SUPPORT
	/*=== JS plugins ===*/
	virtual JS_Plugin_Context *GetJSPluginContext();
	virtual void SetJSPluginContext(JS_Plugin_Context *context);
#endif // JS_PLUGIN_SUPPORT

#ifdef GADGET_SUPPORT
	/*=== Gadget support ===*/
	virtual OP_STATUS HandleGadgetEvent(GadgetEvent event, GadgetEventData *data = NULL);
#endif // GADGET_SUPPORT

	virtual void OnWindowActivated();
	virtual void OnWindowDeactivated();

	/*=== External callbacks ===*/
	void RegisterCallbacksL(CallbackLocation location, DOM_Object *object);

	/*=== Document modification tracking ===*/
	int GetSerialNr();

	/*=== Mutation listeners ===*/
	void AddMutationListener(DOM_MutationListener *listener);
	void RemoveMutationListener(DOM_MutationListener *listener);
	void SetCallMutationListeners(BOOL value) { call_mutation_listeners = value; }

	OP_STATUS SignalOnAfterInsert(HTML_Element *child, DOM_Runtime *origining_runtime);
	OP_STATUS SignalOnBeforeRemove(HTML_Element *child, DOM_Runtime *origining_runtime);
	OP_STATUS SignalOnAfterValueModified(HTML_Element *element, const uni_char *new_value, DOM_MutationListener::ValueModificationType type, unsigned offset, unsigned length1, unsigned length2, DOM_Runtime *origining_runtime);
	OP_STATUS SignalOnAttrModified(HTML_Element *element, const uni_char *name, int ns_idx, DOM_Runtime *origining_runtime);

#ifdef MEDIA_HTML_SUPPORT
	/*=== Media support ===*/
	void AddMediaElement(DOM_HTMLMediaElement *media);
#endif // MEDIA_HTML_SUPPORT

#ifdef DOM_WEBWORKERS_SUPPORT
	/*== Web Workers support ===*/
	DOM_WebWorkerController *GetWorkerController();
	/**< Get at the controller that manages the operation of Web Workers. Is always non-NULL. */
#endif // DOM_WEBWORKERS_SUPPORT

#ifdef DOM_CROSSDOCUMENT_MESSAGING_SUPPORT
	/*== Cross-Document Messaging support ===*/
	void AddMessagePort(DOM_MessagePort *port);
	/**< Register the given port with this environment. */
#endif // DOM_CROSSDOCUMENT_MESSAGING_SUPPORT

#ifdef EVENT_SOURCE_SUPPORT
	/*== EventSource support ===*/
	void AddEventSource(DOM_EventSource *event_source);
	/**< Register the given event source with this environment. */
#endif // EVENT_SOURCE_SUPPORT

#ifdef DOM_GADGET_FILE_API_SUPPORT
	/*=== DOM File API support ===*/
	void AddFileObject(DOM_FileBase *file);
#endif // DOM_GADGET_FILE_API_SUPPORT

#ifdef JS_SCOPE_CLIENT
	void SetScopeClient(JS_ScopeClient*);
	JS_ScopeClient* GetScopeClient();
#endif // JS_SCOPE_CLIENT

#ifdef DOM_JIL_API_SUPPORT
	void SetJILWidget(DOM_JILWidget* jil_widget);
#endif // DOM_JIL_API_SUPPORT

#ifdef EXTENSION_SUPPORT
	void SetExtensionBackground(DOM_ExtensionBackground *background);
	DOM_ExtensionBackground *GetExtensionBackground() { return extension_background; }
#endif // EXTENSION_SUPPORT

#ifdef WEBSOCKETS_SUPPORT
	/**
	 * Adds a WebSocket to the collection of WebSockets in
	 * the environment.
	 *
	 * @param[in] ws The WebSocket to add to the environments's collection
	 * of WebSockets.
	 *
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY
	 */
	OP_STATUS AddWebSocket(DOM_WebSocket *ws);

	/**
	 * Removes a WebSocket from the collection of WebSockets in
	 * the environment.
	 *
	 * @param[in] ws The WebSocket to remove from the environment's collection
	 * of WebSockets.
	 */
	void RemoveWebSocket(DOM_WebSocket *ws);

	/**
	 * Calls Dom_WebSocket::close() on all WebSockets that have
	 * previously been registered with AddWebSocket.
	 *
	 * @returns A boolean indicating if any WebSockets were affected.
	 */
	BOOL CloseAllWebSockets();

	/**
	 * Does extra GC handling on all WebSockets that have previously been
	 * registered with AddWebSocket.
	 */
	void TraceAllWebSockets();
#endif // WEBSOCKETS_SUPPORT

#ifdef CLIENTSIDE_STORAGE_SUPPORT
	void AddDOMStoragebject(DOM_Storage *);
#endif // CLIENTSIDE_STORAGE_SUPPORT

	void AddAsyncCallback(DOM_AsyncCallback* async_callback);

	/*=== Other ===*/
	void GCTrace();
	BOOL IsInDocument(HTML_Element *element);
	BOOL IsXML() { return is_xml; }
	BOOL IsXHTML() { return is_xhtml; }
	void SetIsXML() { is_xml = TRUE; }
	void SetIsXHTML() { is_xml = is_xhtml = TRUE; }
	DOM_Element *GetRootElement();
	OP_STATUS SetDocument(DOM_Document *document);
	static ServerName *GetHostName(FramesDocument *frames_doc);
	BOOL GetIsSettingAttribute() { return is_setting_attribute; }
	void AccessedOtherEnvironment(FramesDocument *other);
#ifdef OPERA_CONSOLE
	static OP_STATUS PostError(DOM_EnvironmentImpl *environment, const ES_ErrorInfo &error, const uni_char *context, const uni_char *url = NULL);
#endif // OPERA_CONSOLE
#ifdef DOM3_LOAD
	void AddLSLoader(DOM_LSLoader *lsloader);
	void RemoveLSLoader(DOM_LSLoader *lsloader);
#endif // DOM3_LOAD
	void AddFileReader(DOM_FileReader *reader);
	void RemoveFileReader(DOM_FileReader *reader);
#ifdef DOM_SUPPORT_BLOB_URLS
	OP_STATUS CreateObjectURL(DOM_Blob* blob, TempBuffer* buf);
	void RevokeObjectURL(const uni_char* blob_url);
#endif // DOM_SUPPORT_BLOB_URLS

#ifdef DOM_XSLT_SUPPORT
	void AddXSLTProcessor(DOM_XSLTProcessor *processor);
	void RemoveXSLTProcessor(DOM_XSLTProcessor *processor);
#endif // DOM_XSLT_SUPPORT

	void AddNodeCollection(DOM_CollectionLink* collection, HTML_Element* tree_root) { node_collection_tracker.Add(collection, tree_root); }
	void AddElementCollection(DOM_CollectionLink* collection, HTML_Element* tree_root) { element_collection_tracker.Add(collection, tree_root); }
	BOOL FindNodeCollection(DOM_NodeCollection*& collection, DOM_Node* root, BOOL include_root, DOM_CollectionFilter* other) { return node_collection_tracker.Find(collection, root, include_root, other); }
	BOOL FindElementCollection(DOM_NodeCollection*& collection, DOM_Node* root, BOOL include_root, DOM_CollectionFilter* other) { return element_collection_tracker.Find(collection, root, include_root, other); }
	void MoveCollection(DOM_NodeCollection* collection, HTML_Element* old_tree_root, HTML_Element* new_tree_root);
	void MigrateCollections() { node_collection_tracker.MigrateCollections(this); element_collection_tracker.MigrateCollections(this); }
	/**< Called after a node has been moved into another environment (via
	     Document.adoptNode) to move all affected collections along to the new
	     environment's collection tracker. */
	void TreeDestroyed(HTML_Element *tree_root);
	BOOL HasCollections() { return !node_collection_tracker.IsEmpty() || !element_collection_tracker.IsEmpty(); }

	/** Set to FALSE to temporarily suspend collection cache state synchronisation. It is up to the caller to ascertain that any cached collections
	    will not be accessed and use any stale cached information while synchronisation is disabled. Enabling it again via TRUE invalidates
	    any and all of the cached collections. */
	void SetTrackCollectionChanges(BOOL f);

	BOOL FindQueryResult(DOM_StaticNodeList*& node_list, const uni_char *query, unsigned data, DOM_Node *root) { return query_selector_cache.Find(this, node_list, query, data, root); }
	OP_STATUS AddQueryResult(DOM_StaticNodeList *node_list, const uni_char *query, unsigned data, DOM_Node *root) { return query_selector_cache.Add(this, node_list, query, data, root); }

	/** Sets up the current state (current_origining_runtime, current_buffer, )*/
	class CurrentState
	{
	public:
		CurrentState(DOM_EnvironmentImpl *environment, DOM_Runtime *origining_runtime = NULL);
		~CurrentState();

		void SetSkipScriptElements();
		void SetTempBuffer(TempBuffer *buffer = NULL);
		void SetOwnerDocument(DOM_Document *owner_document);
		void SetIsSettingAttribute();

	private:
		DOM_EnvironmentImpl *environment;
		DOM_Runtime *current_origining_runtime;
		TempBuffer *current_buffer;
		DOM_Document *owner_document;
		BOOL skip_script_elements;
		BOOL is_setting_attribute;
	};

private:
	friend OP_STATUS DOM_Environment::Create(DOM_Environment *&, FramesDocument *);
	friend void DOM_Environment::Destroy(DOM_Environment *);
	friend class DOM_Runtime;
	friend class CurrentState;

	DOM_EnvironmentImpl();
	DOM_EnvironmentImpl(const DOM_EnvironmentImpl &);
	virtual ~DOM_EnvironmentImpl();

	/** The different "lifecycle" states an environment can be in. */
	enum EnvironmentState
	{
		CREATED,
		/**< The state of a created but not fully initialized environment. */
		RUNNING,
		/**< The state of a fully initialized and active environment. */
		DESTROYED
		/**< The state of an environment that is being shut down
		     and destroyed. */
	};

	EnvironmentState state;

	DOM_Runtime *runtime;
	ES_ThreadScheduler *scheduler;
	ES_AsyncInterface *asyncif;
	JS_Window *window;
	DOM_Document *document;
	DOM_Document *owner_document;
	FramesDocument *frames_document;
	BOOL is_xml;
	BOOL is_xhtml;
#ifdef SVG_DOM
	BOOL is_svg;
#endif // SVG_DOM
	BOOL skip_script_elements;
	BOOL is_setting_attribute;
	BOOL is_loading;
	BOOL is_es_enabled;
	BOOL signal_collection_changes;
	unsigned document_finished_signals;

	class PrefsListener
		: public OpPrefsListener
	{
	public:
		PrefsListener()
			: owner(NULL), is_host_overridden(FALSE)
		{
		}

		virtual void PrefChanged(enum OpPrefsCollection::Collections id, int pref, int newvalue);
#ifdef PREFS_HOSTOVERRIDE
		virtual void HostOverrideChanged(OpPrefsCollection::Collections id, const uni_char *hostname);
#endif // PREFS_HOSTOVERRIDE
	private:
		friend class DOM_EnvironmentImpl;

		DOM_EnvironmentImpl *owner;
		BOOL is_host_overridden;
	};

	friend class DOM_EnvironmentImpl::PrefsListener;
	DOM_EnvironmentImpl::PrefsListener prefs_listener;

#ifdef USER_JAVASCRIPT
	/*=== User Javascript ===*/
	DOM_UserJSManager *user_js_manager;
#endif // USER_JAVASCRIPT

#ifdef TOUCH_EVENTS_SUPPORT
	HTML_Element* GetTouchTarget(int id);
#endif // TOUCH_EVENTS_SUPPORT

	/*=== Security and current executing script ===*/
	DOM_Runtime *current_origining_runtime;
	TempBuffer *current_buffer;

	/*=== Handling events ===*/
	void MarkNodesWithWaitingEventHandlers();
	unsigned short *GetEventHandlersCounter(DOM_EventType type);
	unsigned short mouse_movement_event_handlers_counter[ONMOUSELEAVE - ONMOUSEOVER + 1];
#ifdef DOM2_MUTATION_EVENTS
	unsigned short dom2_mutation_event_handlers_counter[DOMCHARACTERDATAMODIFIED - DOMSUBTREEMODIFIED + 1];
#endif // DOM2_MUTATION_EVENTS
#ifdef TOUCH_EVENTS_SUPPORT
	unsigned short touch_event_handlers_counter[TOUCHCANCEL - TOUCHSTART + 1];
#endif // TOUCH_EVENTS_SUPPORT
	BOOL force_next_event;

#ifdef MEDIA_HTML_SUPPORT
	Head media_elements;
#endif // MEDIA_HTML_SUPPORT

	Head mutation_listeners;
	BOOL call_mutation_listeners;

#ifdef DOM3_LOAD
	List<DOM_LSLoader> lsloaders;
#endif // DOM3_LOAD

	List<DOM_FileReader> file_readers;
#ifdef DOM_SUPPORT_BLOB_URLS
	OpAutoStringHashTable<DOM_BlobURL> blob_urls;
#endif // DOM_SUPPORT_BLOB_URLS

#ifdef DOM_XSLT_SUPPORT
	Head xsltprocessors;
#endif // DOM_XSLT_SUPPORT

#ifdef JS_SCOPE_CLIENT
	JS_ScopeClient *scope_client;
#endif // JS_SCOPE_CLIENT

	DOM_CollectionTracker node_collection_tracker; ///< Collections that can include non-Element nodes.
	DOM_CollectionTracker element_collection_tracker; ///< Collections that cannot include non-Element nodes.

public:
	class QuerySelectorCache
	{
	public:
		QuerySelectorCache()
			: entries(0),
			  is_valid(TRUE)
		{
		}

		~QuerySelectorCache()
		{
			query_results.Clear();
			query_matches.RemoveAll();
		}

		void Reset();

		BOOL Find(DOM_EnvironmentImpl *environment, DOM_StaticNodeList*& node_list, const uni_char *query, unsigned data, DOM_Node *root);
		OP_STATUS Add(DOM_EnvironmentImpl *environment, DOM_StaticNodeList *matched_list, const uni_char *query, unsigned data, DOM_Node *root);

		class QueryResult
			: public ListElement<QueryResult>
		{
		public:
			static OP_STATUS Make(QueryResult *&query_result, DOM_StaticNodeList *cached_list, const uni_char *q, unsigned data, DOM_Node *root);

		private:
			friend class DOM_EnvironmentImpl::QuerySelectorCache;

			QueryResult(DOM_StaticNodeList *ls, const uni_char *q, unsigned d, DOM_Node *r)
				: list(ls),
				  query(q),
				  data(d),
				  root(r)
			{
			}

			~QueryResult()
			{
				OP_DELETEA(query);
			}

			DOM_StaticNodeList *list;
			const uni_char *query;
			unsigned data;
			DOM_Node *root;

		};

	private:
		void Invalidate(DOM_EnvironmentImpl *environment);

		friend class DOM_Environment;

		List<QueryResult> query_results;
		List<DOM_StaticNodeList> query_matches;
		unsigned entries;
		BOOL is_valid;

		static const unsigned max_query_results = 0x10;
	};

private:
	DOM_EnvironmentImpl::QuerySelectorCache query_selector_cache;

#ifdef DOM_GADGET_FILE_API_SUPPORT
	Head file_objects;
#endif // DOM_GADGET_FILE_API_SUPPORT

#ifdef WEBSOCKETS_SUPPORT
	OpVector<DOM_WebSocket> websockets;
#endif //WEBSOCKETS_SUPPORT

#ifdef CLIENTSIDE_STORAGE_SUPPORT
	Head webstorage_objects;
#endif // CLIENTSIDE_STORAGE_SUPPORT

#ifdef DOM_JIL_API_SUPPORT
	DOM_JILWidget* jil_widget;
#endif // DOM_JIL_API_SUPPORT

	List<DOM_AsyncCallback> async_callbacks;

#ifdef DOM_WEBWORKERS_SUPPORT
	DOM_WebWorkerController *web_workers;

	friend class DOM_WebWorkerDomain;
#endif // DOM_WEBWORKERS_SUPPORT

#ifdef DOM_CROSSDOCUMENT_MESSAGING_SUPPORT
	List<DOM_MessagePort> message_ports;
#endif // DOM_CROSSDOCUMENT_MESSAGING_SUPPORT

#ifdef EVENT_SOURCE_SUPPORT
	List<DOM_EventSource> event_sources;
#endif // EVENT_SOURCE_SUPPORT

#ifdef DOM_WEBWORKERS_SUPPORT
	static OP_STATUS Make(DOM_Environment *&environment, FramesDocument *doc, URL *base_worker_url, DOM_Runtime::Type type = DOM_Runtime::TYPE_DOCUMENT);
#else
	static OP_STATUS Make(DOM_Environment *&environment, FramesDocument *doc);
#endif // DOM_WEBWORKERS_SUPPORT
	/**< Internal method for constructing an environment.
	 *
	 *   @param [out]environment Set to the created environment.
	 *   @param doc The document this environment is for, if any.
	 *   @param base_url If creating a web worker environment, its base URL.
	 *   @param type The kind of environment to create.
	 *
	 *   @return OpStatus::OK on success,
	 *           OpStatus::ERR if ECMAScript is disabled,
	 *           OpStatus::ERR_NO_MEMORY on OOM.
	 */

#ifdef TOUCH_EVENTS_SUPPORT
	DOM_TouchList* active_touches;
#endif // TOUCH_EVENTS_SUPPORT

#ifdef EXTENSION_SUPPORT
	DOM_ExtensionBackground *extension_background;
	/**< If the environment is hosting an extension's "background process",
	 *   the manager object supporting it. */
#endif // EXTENSION_SUPPORT
};

class DOM_ProxyEnvironmentImpl
	: public DOM_ProxyEnvironment
{
public:
	friend OP_STATUS DOM_ProxyEnvironment::Create(DOM_ProxyEnvironment *&);
	friend void DOM_ProxyEnvironment::Destroy(DOM_ProxyEnvironment *);

	/*=== Proxy/real object management ===*/
	virtual void SetRealWindowProvider(RealWindowProvider *provider);
	virtual void Update();

	virtual OP_STATUS GetProxyWindow(DOM_Object *&window, ES_Runtime *origining_runtime);
	OP_STATUS GetProxyDocument(DOM_Object *&document, ES_Runtime *origining_runtime);
	OP_STATUS GetProxyLocation(DOM_Object *&location, ES_Runtime *origining_runtime);

	void ResetRealObject(DOM_Object *window, DOM_Object *document, DOM_Object *location);
	void ResetProxyObject(DOM_Object *window, DOM_Object *document, DOM_Object *location);

	void Close();

	void RuntimeDetached(ES_Runtime *runtime, Link *group);
	void RuntimeDestroyed(ES_Runtime *runtime, Link *group);

	RealWindowProvider *GetRealWindowProvider() { return provider; }

private:
	class WindowProvider
		: public DOM_ProxyObject::Provider
	{
	public:
		WindowProvider(DOM_ProxyEnvironmentImpl *environment) : environment(environment) {}
		virtual ~WindowProvider() {}
		virtual void ProxyObjectDestroyed(DOM_Object *proxy_object);
		virtual OP_STATUS GetObject(DOM_Object *&object);
		DOM_ProxyEnvironmentImpl *environment;
	} windowprovider;

	class DocumentProvider
		: public DOM_ProxyObject::Provider
	{
	public:
		DocumentProvider(DOM_ProxyEnvironmentImpl *environment) : environment(environment) {}
		virtual ~DocumentProvider() {}
		virtual void ProxyObjectDestroyed(DOM_Object *proxy_object);
		virtual OP_STATUS GetObject(DOM_Object *&object);
		DOM_ProxyEnvironmentImpl *environment;
	} documentprovider;

	class LocationProvider
		: public DOM_ProxyObject::Provider
	{
	public:
		LocationProvider(DOM_ProxyEnvironmentImpl *environment) : environment(environment) {}
		virtual ~LocationProvider() {}
		virtual void ProxyObjectDestroyed(DOM_Object *proxy_object);
		virtual OP_STATUS GetObject(DOM_Object *&object);
		DOM_ProxyEnvironmentImpl *environment;
	} locationprovider;

	DOM_ProxyEnvironmentImpl()
		: windowprovider(this),
		  documentprovider(this),
		  locationprovider(this),
		  provider(NULL)
	{
	}

	DOM_ProxyEnvironmentImpl(const DOM_ProxyEnvironmentImpl &);

	virtual ~DOM_ProxyEnvironmentImpl();

	class ProxyObjectGroup
		: public Link
	{
	public:
		ProxyObjectGroup() : window(NULL), document(NULL), location(NULL) {}
		~ProxyObjectGroup() { if (runtime_link) { runtime_link->Out(); OP_DELETE(runtime_link); } }

		ES_Runtime *runtime;
		Link *runtime_link;
		DOM_ProxyObject *window, *document, *location;
	};

	OP_STATUS GetOrCreateProxyObjectGroup(ProxyObjectGroup *&group, ES_Runtime *runtime);

	AutoDeleteHead proxy_object_groups;
	RealWindowProvider *provider;
};

#endif // DOM_ENVIRONMENTIMPL_H
