/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2003-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/
#ifndef DOM_UTILS_H
#define DOM_UTILS_H

/** @file domutils.h
  *
  * Utility API for the DOM module.  Defines one class, DOM_Utils,
  * with static functions.
  *
  * @author Jens Lindstrom
  */

#include "modules/dom/domtypes.h"
#include "modules/windowcommander/OpWindowCommander.h"

class DOM_Environment;
class DOM_Object;
class DOM_Runtime;
class ES_Object;
class ES_Thread;
class HTML_Element;
class FramesDocument;
class DocumentOrigin;
class OpGadget;

/** Container for DOM utility functions.
  *
  * Functions used to check the type of a DOM object and to convert
  * between DOM objects and types from other modules.
  */
class DOM_Utils
{
public:
	static DOM_Environment *GetDOM_Environment(DOM_Object *object);
	/**< Retrieves a DOM object's associated DOM environment.

	     @param object A DOM object or NULL.
	     @return A DOM environment or NULL if 'object' is NULL. */

	static DOM_Environment *GetDOM_Environment(ES_Runtime *runtime);
	/**< Retrieves a ES runtime's associated DOM environment.

	     @param runtime An ES runtime or NULL.
	     @return A DOM environment or NULL if 'runtime' is NULL. */

	static DOM_Runtime *GetDOM_Runtime(DOM_Object *object);
	/**< Retrieves a DOM object's associated DOM runtime.

	     @param object A DOM object or NULL.
	     @return A DOM runtime or NULL if 'object' is NULL. */

	static BOOL IsA(DOM_Object *object, DOM_ObjectType type);
	/**< Returns TRUE if 'object' is of 'type' or a sub-type thereof
	     (for instance, for objects of the type 'DOM_TYPE_ELEMENT',
	     this will also return TRUE for 'DOM_TYPE_NODE').

	     @param object A DOM object or NULL.
	     @param type Type to check.
	     @return TRUE if 'object' is of the given type, FALSE if
	             'object' is NULL or not of the given type. */

	static DOM_NodeType GetNodeType(DOM_Object *object);
	/**< Retrieves a DOM objects's node type.

	     @param object A DOM object or NULL.
	     @return The object's Node type or NOT_A_NODE if 'object' is
	             not a Node object (if 'IsA(object, DOM_TYPE_NODE)'
	             returns FALSE or if 'object' is NULL. */

	static ES_Runtime *GetES_Runtime(DOM_Runtime *runtime);
	/**< Retrieves a DOM_Runtime's associated ES_Runtime object.

	     @param runtime A DOM_Runtime or NULL.
	     @return A pointer to an ES_Runtime object or NULL if
	             'runtime' is NULL. */

	static DOM_Runtime *GetDOM_Runtime(ES_Runtime *runtime);
	/**< Retrieves an ES_Runtime's associated DOM_Runtime object.

	     @param runtime An ES_Runtime or NULL.
	     @return A pointer to a DOM_Runtime object or NULL if
	             'runtime' is NULL. */

	static URL GetOriginURL(DOM_Runtime *runtime);
	/**< Retrieves the origin URL of a DOM_Runtime.  This is the URL
	     that together with the domain name returned by GetDomain()
	     constitutes the runtime's security context.

	     @param runtime A DOM_Runtime or NULL.
	     @return A URL.  If 'runtime' is NULL, an empty URL is
	             returned. */

	static DocumentOrigin *GetMutableOrigin(DOM_Runtime *runtime);
	/**< Retrieves the DocumentOrigin of a DOM_Runtime.  This is the object
	     constitutes the runtime's security context. Modifying this _will_ change
	     the security context of the document and its runtime.

	     @param runtime A DOM_Runtime or NULL.
	     @return A DocumentOrigin object. If runtime is NULL, then NULL is returned. */

	static const uni_char *GetDomain(DOM_Runtime *runtime);
	/**< Retrieves the domain name of a DOM_Runtime.  This is
	     originally the host name part of the URL return by
	     GetOriginURL(), but can be modified by a script by assigning
	     a different value to the property document.domain.

	     @param runtime A DOM_Runtime or NULL.
	     @return A domain name or an empty string if 'runtime' is
	             NULL. */

	static BOOL HasOverriddenDomain(DOM_Runtime *runtime);
	/**< Returns TRUE if the runtime's domain (as returned by
	     GetDomain()) has been modified by a script by assigning a
	     value to document.domain, even if the value was the same as
	     before.  That is, "document.domain=document.domain" will not
	     change the domain, but it will make it "overridden".

	     @param runtime A DOM_Runtime or NULL.

	     @return TRUE if runtime is not NULL and that runtime's domain
	             has been overridden, FALSE otherwise. */

	static DOM_Object *GetDocumentNode(DOM_Runtime *runtime);
	/**< Retrieves the document node associated with a DOM_Runtime.

	     @param runtime A DOM_Runtime or NULL.
	     @return A document node, or NULL if 'runtime' is NULL or if the
	             document node could not be created. */

	static FramesDocument *GetDocument(DOM_Runtime *runtime);
	/**< Retrieves the document associated with a DOM_Runtime.

	     @param runtime A DOM_Runtime or NULL.
	     @return A document or NULL if 'runtime' is NULL or if the
	             document that used to be associated with the runtime
	             has been freed. */
#ifdef DOM_WEBWORKERS_SUPPORT
	static FramesDocument *GetWorkerOwnerDocument(DOM_Runtime *runtime);
	/**< Retrieves the owner document associated with a WebWorker's DOM_Runtime.

	     @param runtime A DOM_Runtime of a WebWorker or NULL.
	     @return The document of the context that created the web worker or
	               NULL if this is not a WebWorker runtime. */

#endif // DOM_WEBWORKERS_SUPPORT

#ifdef EXTENSION_SUPPORT
	static FramesDocument *GetExtensionUserJSOwnerDocument(DOM_Runtime *runtime);
	/**< Retrieves the document associated with extension UserJS runtimes shared environmment.

	     @param runtime A DOM_Runtime of a UserJS or NULL.
	     @return The document of the shared environment in which the runtime
	               runs or NULL if this is not an extension UserJS runtime. */
#endif // EXTENSION_SUPPORT

	static BOOL HasDebugPrivileges(DOM_Runtime *runtime);
	/**< Returns TRUE if the DOM_Runtime has debug privileges.

	     @param runtime A DOM_Runtime or NULL.
	     @return TRUE or FALSE. */

	static BOOL IsRuntimeEnabled(DOM_Runtime *runtime);
	/**< Returns TRUE if the DOM_Runtime is enabled and further
	     operations can be performed in its context. TRUE
	     except when a runtime is being detached from its
	     associated document and shutting down.

	     @param runtime A DOM_Runtime.
	     @return TRUE or FALSE. */


#ifdef SECMAN_ALLOW_DISABLE_XHR_ORIGIN_CHECK
	static BOOL HasRelaxedLoadSaveSecurity(DOM_Runtime *runtime);
	/**< Returns TRUE if scripts in the specified DOM_Runtime should be
	     subjected to relaxed load/save (LSParser, LSSerializer,
	     XMLHttpRequest and the like) security constraints.  Only
	     returns TRUE if SetRelaxedLoadSaveSecurity() has been called
	     for the specified DOM_Runtime.

	     Note: This function should only be used by code in the
	           security_manager module!

	     @param runtime A DOM_Runtime or NULL.
	     @return TRUE or FALSE. */

	static void SetRelaxedLoadSaveSecurity(DOM_Runtime *runtime, BOOL value);
	/**< Signals whether scripts executing in the specified DOM_Runtime
	     should be subjected to relaxed load/save (LSParser,
	     LSSerializer, XMLHttpRequest and the like) security
	     constraints.

	     Note: This function should only be used by code in the
	           security_manager module!

	     @param runtime A DOM_Runtime or NULL.
	     @param flag TRUE to relax security, FALSE to restore the
	                 default, strict security policy. */
#endif // SECMAN_ALLOW_DISABLE_XHR_ORIGIN_CHECK

	enum ObjectType
	{
		TYPE_DEFAULT,
		TYPE_LIVECONNECT
	};

	static DOM_Object *GetDOM_Object(ES_Object *object, ObjectType type = TYPE_DEFAULT);
	/**< Retrieves an ES_Object's associated DOM object.

	     @param object An ECMAScript object or NULL.
	     @param type TYPE_DEFAULT or TYPE_LIVECONNECT. TYPE_LIVECONNECT
	                 returns JavaObject representation for OBJECT/EMBED
	                 if object is a HTMLObjectElement or
	                 HTMLEmbedElement.
	     @return A DOM object if 'object' has an associated DOM object
	             or NULL if it has none or if 'object' is NULL. */

	static ES_Object *GetES_Object(DOM_Object *object);
	/**< Retrieves a DOM object's associated ES_Object.

	     @param object A DOM object or NULL.
	     @return An ECMAScript object or NULL if 'object' is NULL. */

	static DOM_Object *GetProxiedObject(DOM_Object *proxy);
	/**< If the specified object is a proxy object, return the object
	     being proxied.

	     If the specified object is NOT a proxy object, the same object
	     is returned.

	     If the specified object IS a proxy object, but the proxied object
	     could currently not be retrieved, this function returns NULL.

	     Please note that the returned DOM_Object may reside in a different
	     runtime than the incoming object.

	     @param proxy The (proposed) proxy object. If NULL, NULL is returned.
	     @return The proxied object, NULL, or 'proxy'. */

	static HTML_Element *GetHTML_Element(DOM_Object *object);
	/**< Retrieves a DOM object's associated HTML_Element.  Only
	     objects for whom 'IsA(DOM_TYPE_ELEMENT)' or
	     'IsA(DOM_TYPE_CHARACTERDATA)' returns TRUE have associated
	     HTML elements.

	     @param object A DOM object or NULL.
	     @return An HTML element if 'object' has an associated HTML
	             element or NULL if it has none or if 'object' is
	             NULL. */

	static DOM_Object *GetEventTarget(ES_Thread *thread);
	/**< Retrieves the target node of the event handled by an
	     event thread or NULL if the thread is not an event
	     thread or is NULL.

	     @param thread A thread or NULL.
	     @return A DOM_Object representing a Node or NULL. */

	static const uni_char *GetCustomEventName(ES_Thread *thread);
	/**< Get the custom event name for the event handled by an
	     event thread. NULL if the thread is not an event thread,
	     or if the event is not a custom event.

	     @param thread A thread, or NULL.
	     @return A string containing the custom event name, or
	             NULL. The string is owned by the associated
	             DOM_Event. */

	static const uni_char *GetNamespaceURI(ES_Thread *thread);
	/**< Get the namespace URI for the event handled by an
	     event thread. NULL if the thread is not an event thread,
	     or if the event has no namespace. Also NULL if feature
	     DOM3_EVENTS is turned off.

	     @param thread A thread, or NULL.
	     @return A string containing the namespace name, or
	             NULL. The string is owned by the associated
	             DOM_Event. */

	static HTML_Element *GetEventPathParent(HTML_Element *currentTarget, HTML_Element *target);
	/**< Return the parent of 'currentTarget' in the event path of
	     an element with the 'target' as its parent.

	     @param currentTarget Element whos parent to return.
	     @param target Target of the event.
	     @return The parent element or NULL. */

	static HTML_Element *GetActualEventTarget(HTML_Element *target);
	/**< Return the element that will be returned by Event.target for
	     an event fired with 'target' as its target through a call to
	     DOM_Environment::HandleEvent.  The actual target will
	     typically be the nearest ancestor non-text element that is
	     not inserted by the layout engine.

	     @param target Target element.
	     @return Actual target element. */

	static HTML_Element *GetEventTargetElement(HTML_Element *element);
	/**< Returns the element whose event handlers would be fired
	     instead of element's, while processing an event for which
	     'element' is in the event path. */

	static BOOL IsInlineScriptOrWindowOnLoad(ES_Thread* thread);
	/**< Returns whether the thread is inside an onload handler or in
	     an inline script. This is different to checking the origin
	     info in case of timeouts, since they origin in the onload
	     handler but doesn't execute inside them.

	     @param thread The current thread. Can be NULL.
	     @return TRUE if we're currently inside a window onload handler or inline script. */

#ifdef JS_PLUGIN_SUPPORT
	static ES_Object *GetJSPluginObject(DOM_Object *object);
	/**< Retrieve a DOM object's associated JS plugin object.

	     @param object A DOM object or NULL.
	     @return An ECMAScript object or NULL. */

	static void SetJSPluginObject(DOM_Object *object, ES_Object *plugin_object);
	/**< Set a DOM object's associated JS plugin object.

	     @param object A DOM object or NULL.
	     @param plugin_object An ECMAScript object or NULL. */
#endif // JS_PLUGIN_SUPPORT

#ifdef SVG_DOM

	static void ReleaseSVGDOMObjectFromLists(DOM_Object* dom_obj);
	/**< Releases an object from the list it is currently a member
         of. The reason that this function exists is that two
         SVGPathSegLists (one normalized and one un-normalized) from
         the same path attribute can affect each other. When this
         happens the svg module needs to signal to a DOM_Onbject that
         is has lost its place in the list. Implemented in
         modules/dom/src/domsvg/domsvglist.cpp.

         @param dom_obj A DOM object set by a SVGDOMList::SetDOMObject
                        call. */

#endif // SVG_DOM

#ifdef _PLUGIN_SUPPORT_
	/**
	 * Returns the location object for a window or OpStatus::ERR. Used
	 * as a shortcircuit in the plugin handling.
	 */
	static OP_STATUS GetWindowLocationObject(DOM_Object* window, DOM_Object*& location);
#endif // _PLUGIN_SUPPORT

#ifdef DOM_INSPECT_NODE_SUPPORT

	class InspectNodeCallback
	{
	public:
		virtual void SetType(DOM_Object *node, DOM_NodeType type) = 0;
		/**< Reports the node's type.  The type determines what further
		     functions will/may be called for the node. */
		virtual void SetName(DOM_Object *node, const uni_char *namespace_uri, const uni_char *prefix, const uni_char *localpart) = 0;
		/**< Reports the node's name.  Called for element, attribute
		     and processing instruction nodes.  For a processing
		     instruction, 'localpart' is the target of the instruction
		     and the 'namespace_uri' and 'prefix' are NULL. */
		virtual void SetValue(DOM_Object *node, const uni_char *value) = 0;
		/**< Reports the node's value.  Called for text, comment,
		     CDATA section, comment, processing instruction and
		     attribute nodes. */
		virtual void SetDocumentTypeInformation(DOM_Object *node, const uni_char *name, const uni_char *public_id, const uni_char *system_id) = 0;
		/**< Reports information about a document type node. */

		virtual void SetFrameInformation(DOM_Object *node, DOM_Object *content_document) = 0;
		/**< Called on HE_FRAME/HE_OBJECT element nodes. */

		virtual void SetDocumentInformation(DOM_Object *node, DOM_Object *frame_element) = 0;
		/**< Called on document nodes in (i)frames. */

		virtual void AddAttribute(DOM_Object *node, DOM_Object *attribute) = 0;
		/**< Called for element nodes, once per attribute. */

		virtual void SetParent(DOM_Object *node, DOM_Object *parent) = 0;
		/**< Called for all nodes except document and document
		     fragment nodes.  If the node has no parent, 'parent' will
		     be NULL. */
		virtual void AddChild(DOM_Object *node, DOM_Object *child) = 0;
		/**< Called for element, document and document fragment nodes,
		     once per child node. */
	protected:
		virtual ~InspectNodeCallback() {}
		/**< Objects are not deleted via this interface. */
	};

	/** Flags used to indicate what type of information should be
		reported back when inspecting a node.  The different flags
		should be OR:ed together. */
	enum InspectFlags
	{
		INSPECT_BASIC = 1,
		/**< Report basic information: SetType, SetNodeName,
		     SetNodeValue, SetDocumentTypeInformation.) */
		INSPECT_ATTRIBUTES = 2,
		/**< Report attributes. */
		INSPECT_PARENT = 4,
		INSPECT_CHILDREN = 8,
		/**< Report child nodes. */

		INSPECT_ALL = INSPECT_BASIC | INSPECT_ATTRIBUTES | INSPECT_PARENT | INSPECT_CHILDREN
	};

	static OP_STATUS InspectNode(DOM_Object *node, unsigned flags, InspectNodeCallback *callback);
	/**< Inspects a node by calling functions on the callback object.
	     Nodes can be inspected recursively, that is, from calls to
	     the callback.

	     @return OpStatus::OK, OpStatus::ERR if 'node' is not a node
	             object (NULL or another type of object) and
	             OpStatus::ERR_NO_MEMORY on OOM. */

	/** Available search types. The chosen type decides the meaning
	    of the 'query' argument to Search. */
	enum SearchType
	{
		SEARCH_PLAINTEXT,
		/**< Search for a specified string in tag names, attribute names,
		     attribute values, and text content. */

		SEARCH_REGEXP,
		/**< Like SEARCH_PLAINTEXT, but use a regular expression instead. */

		SEARCH_XPATH,
		/**< Search using XPath (namespaces not supported). */

		SEARCH_SELECTOR,
		/**< Search by CSS selectors. */

		SEARCH_EVENT
		/**< Search for nodes containing event listeners matching the
		     specified event name. The match is performed by looking for the
		     query string in the event name of every listener. This means that
		     a query string of 'click' will match both the 'click' and
		    'dblclick' event. An empty query string will match all listeners. */
	};

	/**
	 * This class is used to report the matching DOM nodes to the
	 * caller of Search.
	 */
	class SearchCallback
	{
	public:
		virtual OP_STATUS MatchNode(DOM_Object *node) = 0;
		/**< Called when a Node matches a previous search.

		     @param node The matching node. Never NULL.
		     @return OpStatus::ERR_NO_MEMORY will abort further matching. All
		             other return values will be ignored, and cause the
		             search to continue. */
	};

	/**
	 * Options for the Search function.
	 */
	class SearchOptions
	{
	public:
		friend class DOM_Utils;

		SearchOptions(const uni_char *query, DOM_Runtime *runtime);
		/**< Constructor. Sets members to defaults.

		     @param query The string to match for. The format varies according to type
		                  (which is SEARCH_PLAINTEXT by default).
		     @param runtime The runtime to search in. */

		void SetType(SearchType type);
		/**< Which type of search to use (plaintext [default], regexp, XPath, CSS) */

		void SetRoot(DOM_Object *root);
		/**< If set, begin the search with this node as the root. Default is unset,
		     which means the root node of the runtime is used. */

		void SetIgnoreCase(BOOL ignore_case);
		/**< Set to TRUE to ignore case. Default is FALSE. */
	private:
		/** Required arguments: */
		const uni_char *query;
		DOM_Runtime *runtime;

		/** Optional arguments: */
		SearchType type;
		DOM_Object *root;
		BOOL ignore_case;
	};

	static OP_STATUS Search(const SearchOptions &options, SearchCallback *callback);
	/**< Search for nodes in the DOM (sub)tree.

	     @param options Object with arguments and options for the search.
	     @param callback This object is notified when we encounter a matching node.
	     @return OpStatus::OK on success; OpStatus::ERR_NO_MEMORY; or OpStatus::ERR
	             for other errors (e.g. XPath/RegExp could not be compiled).*/
#endif // DOM_INSPECT_NODE_SUPPORT

#ifdef EXTENSION_SUPPORT
	static BOOL WillUseExtensions(FramesDocument *frames_doc);
	/**< Determine if an extension would activate on the given document.
	     The check may load newly installed scripts but in general is low
	     overhead.

	     @return TRUE if extension UserJS is to be activated,
	             FALSE if not. */
#endif // EXTENSION_SUPPORT

#ifdef GADGET_SUPPORT
	static OpGadget* GetRuntimeGadget(DOM_Runtime *runtime);
	/**< Gets a gadget associated with a given runtime. This may be either a runtime
	     associated with a gadget document or extensions UserJS.
		 Returns NULL if no gadget is associated with a given runtime. */
#endif // GADGET_SUPPORT

#ifdef DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT
	static OP_STATUS AppendExtensionMenu(OpWindowcommanderMessages_MessageSet::PopupMenuRequest::MenuItemList& menu_items, OpDocumentContext* document_context, FramesDocument* document, HTML_Element* element);
	/**< Fills a vector of menu items which extensions(via ContextMenu API) requested
	 *   to add to the context menu.
	 *
	 *   @param menu_items List of menu items to which the menu items will be added.
	 *   @param document_context Context in which the menu has been requested.
	 *   @param document Document for which the menu has been requested.
	 *   @param element Element for which the menu has been requested. May be NULL.
	 *   @return
	 *      OpStatus::OK
	 *      OpStatus::ERR_NO_MEMORY
	 */
#endif //DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT

	static OP_STATUS GetDOM_CSSStyleSheet(DOM_Object *node, DOM_Object *import_rule, DOM_Object *&stylesheet);
	/**< Get the DOM_CSSStyleSheet for the specified DOM_Node.

	     @param node [in] The node to get the stylesheet for.
	     @param import_rule [in] The dom object of the CSSImportRule if the
	             stylesheet is owned by an @import rule. NULL otherwise.
	     @param stylesheet [out] The stylesheet for this Node. Undefined on errors.
	     @return OpStatus::OK on success; OpStatus::ERR if 'node' is not a
	             node, or if the node did not have a stylesheet; or
	             OpStatus::ERR_NO_MEMORY on OOM. */

	static OP_STATUS GetDOM_CSSStyleSheetOwner(DOM_Object *stylesheet, DOM_Object *&owner);
	/**< Get the DOM_Node that owns the specified DOM_CSSStyleSheet.

	     @param stylesheet [in] The stylesheet to get the owner for.
	     @param owner [out] The Node that owns the stylesheet. Undefined on errors.
	     @return OpStatus::OK on success; OpStatus::ERR if 'stylesheet' is not a
	             stylesheet, or if the stylesheet did not have an owner; or
	             OpStatus::ERR_NO_MEMORY on OOM. */

	static BOOL IsOperaIllegalURL(const URL &url);
	/**< Returns TRUE if url is an opera:illegal-url-XX; FALSE otherwise. */

	/** Flags for GetSerializedOrigin. */
	enum OriginSerializationFlags
	{
		SERIALIZE_FILE_SCHEME   = 1 << 0,
		/**< Serialize file: urls also as file://host:port despite not having a port.
		     Not part of the specification. */
		SERIALIZE_FALLBACK_AS_NULL = 1 << 1
		/**< Use "null" as the fallback value when a url that does not fit the
		     form scheme://host:port. Else, the TempBuffer is not touched.
		     Behavior part of the specification. */
	};

	static OP_STATUS GetSerializedOrigin(const URL &origin_url, TempBuffer &buffer, unsigned flags = SERIALIZE_FALLBACK_AS_NULL);
	/**< Derive the serialized origin of the given origin, following the HTML
	     specification: if the URL has a (scheme, host, port) triple as
	     origin, it is serialized as "scheme://host:port", with ":port"
	     only being included if a non-default port. URLs that don't have
	     such a triple are serialized as "null".

	     @param origin_url The URL to serialize.
	     @param buffer The temporary output buffer to emit the serialization
	            into.
	     @param flags Flags, see OriginSerializationFlags. The default value
	            ensures specification compliance.
	     @returns OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM. */

	static BOOL IsValidTokenValue(const uni_char *string, unsigned length);
	/**< Returns TRUE if 'string' is a valid token value, per RFC-2616 (HTTP/1.1).
	     FALSE if 'string' is empty (including NULL), or it contains one or
	     more illegal token characters.

	     @param string The string to validate.
	     @param length The (codepoint) length of 'string'.
	     @returns TRUE if a valid token value, FALSE otherwise. */

	static BOOL IsClipboardAccessAllowed(URL &origin_url, Window *window);
	/**< Returns TRUE if the origin of the provided URL is allowed to access
	     and modify system clipboard.

	     @param origin_url The URL that will be checked.
	     @param window The window within which the check is performed. Used to
	            allow access for specific window types. Can be NULL in which
	            case only origin of a URL is taken into consideration.
	     @return TRUE if given URL is allowed to access clipboard. */

private:
	DOM_Utils();
	DOM_Utils(DOM_Utils &);
};

#endif // DOM_UTILS_H
