/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2003-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/
#include "core/pch.h"

#include "modules/dom/domutils.h"

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domobj.h"
#include "modules/dom/src/domcore/domdoc.h"
#include "modules/dom/src/domcore/node.h"
#include "modules/dom/src/domcss/cssrule.h"
#include "modules/dom/src/domcss/cssstylesheet.h"
#include "modules/dom/src/domevents/domevent.h"
#include "modules/dom/src/domevents/domeventthread.h"
#include "modules/dom/src/domhtml/htmlelem.h"
#include "modules/dom/src/domevents/domeventtarget.h"
#include "modules/dom/src/domevents/domeventlistener.h"
#include "modules/dom/src/js/window.h"
#include "modules/dom/src/js/location.h"

#include "modules/gadgets/OpGadget.h"

#include "modules/logdoc/htm_elm.h"

#ifdef EXTENSION_SUPPORT
#include "modules/dom/src/userjs/userjsmanager.h"
#include "modules/dom/src/extensions/domextensionmanager.h"
#include "modules/gadgets/OpGadget.h"
#endif // EXTENSION_SUPPORT

#include "modules/style/css_dom.h"
#include "modules/regexp/include/regexp_api.h"
#include "modules/xpath/xpath.h"
#include "modules/url/url_man.h"

#ifdef DOM_WEBWORKERS_SUPPORT
#include "modules/dom/src/domwebworkers/domwebworkers.h"
#include "modules/dom/src/domwebworkers/domwwcontroller.h"
#endif // DOM_WEBWORKERS_SUPPORT

/* static */ DOM_Environment *
DOM_Utils::GetDOM_Environment(DOM_Object *object)
{
	return object ? object->GetEnvironment() : NULL;
}

/* static */ DOM_Environment *
DOM_Utils::GetDOM_Environment(ES_Runtime *runtime)
{
	return runtime ? ((DOM_Runtime *) runtime)->GetEnvironment() : NULL;
}

/* static */ DOM_Runtime *
DOM_Utils::GetDOM_Runtime(DOM_Object *object)
{
	return object ? object->GetRuntime() : NULL;
}

/* static */ BOOL
DOM_Utils::IsA(DOM_Object *object, DOM_ObjectType type)
{
	return object && object->IsA(type);
}

/* static */ DOM_NodeType
DOM_Utils::GetNodeType(DOM_Object *node)
{
	return node && node->IsA(DOM_TYPE_NODE) ? ((DOM_Node *) node)->GetNodeType() : NOT_A_NODE;
}

/* static */ ES_Runtime *
DOM_Utils::GetES_Runtime(DOM_Runtime *runtime)
{
	return runtime;
}

/* static */ DOM_Runtime *
DOM_Utils::GetDOM_Runtime(ES_Runtime *runtime)
{
	return (DOM_Runtime *) runtime;
}

/* static */ URL
DOM_Utils::GetOriginURL(DOM_Runtime *runtime)
{
	URL empty;
	return runtime ? runtime->GetOriginURL() : empty;
}

/* static */ DocumentOrigin *
DOM_Utils::GetMutableOrigin(DOM_Runtime *runtime)
{
	return runtime ? runtime->GetMutableOrigin() : NULL;
}

/* static */ const uni_char *
DOM_Utils::GetDomain(DOM_Runtime *runtime)
{
	return runtime ? runtime->GetDomain() : UNI_L("");
}

/* static */ BOOL
DOM_Utils::HasOverriddenDomain(DOM_Runtime *runtime)
{
	return runtime && runtime->HasOverriddenDomain();
}

/* static */ DOM_Object *
DOM_Utils::GetDocumentNode(DOM_Runtime *runtime)
{
	if (!runtime)
		return NULL;

	DOM_EnvironmentImpl *environment = runtime->GetEnvironment();
	RETURN_VALUE_IF_ERROR(environment->ConstructDocumentNode(), NULL);
	if (DOM_Document *document = static_cast<DOM_Document *>(environment->GetDocument()))
		if (!document->GetPlaceholderElement())
			RETURN_VALUE_IF_ERROR(document->ResetPlaceholderElement(), NULL);

	return environment->GetDocument();
}

/* static */ FramesDocument *
DOM_Utils::GetDocument(DOM_Runtime *runtime)
{
	return runtime ? runtime->GetFramesDocument() : NULL;
}

#ifdef DOM_WEBWORKERS_SUPPORT
/* static */ FramesDocument *
DOM_Utils::GetWorkerOwnerDocument(DOM_Runtime *runtime)
{
	if (!runtime)
		return NULL;

	if (DOM_WebWorkerController *controller = runtime->GetEnvironment()->GetWorkerController())
		if (DOM_WebWorker *worker = controller->GetWorkerObject())
			return worker->GetWorkerOwnerDocument();

	return NULL;
}
#endif // DOM_WEBWORKERS_SUPPORT

#ifdef EXTENSION_SUPPORT
/* static */ FramesDocument *
DOM_Utils::GetExtensionUserJSOwnerDocument(DOM_Runtime *runtime)
{
	if (!runtime
	 || !runtime->HasSharedEnvironment()  // check if this is actually UserJS.
	 ||  runtime->GetFramesDocument())    // check if this is extension.
		return NULL;

	return runtime->GetEnvironment()->GetFramesDocument();
}
#endif // EXTENSION_SUPPORT

/* static */ BOOL
DOM_Utils::HasDebugPrivileges(DOM_Runtime *runtime)
{
	return runtime && runtime->HasDebugPrivileges();
}

/* static */ BOOL
DOM_Utils::IsRuntimeEnabled(DOM_Runtime *runtime)
{
	if (runtime->HasSharedEnvironment())
		return (runtime->GetEnvironment()->GetFramesDocument() && runtime->GetEnvironment()->GetFramesDocument()->IsCurrentDoc());
	else
		return (runtime->GetEnvironment()->IsEnabled());
}


#ifdef SECMAN_ALLOW_DISABLE_XHR_ORIGIN_CHECK

/* static */ BOOL
DOM_Utils::HasRelaxedLoadSaveSecurity(DOM_Runtime *runtime)
{
	return runtime ? runtime->HasRelaxedLoadSaveSecurity() : FALSE;
}

/* static */ void
DOM_Utils::SetRelaxedLoadSaveSecurity(DOM_Runtime *runtime, BOOL value)
{
	if (runtime)
		runtime->SetRelaxedLoadSaveSecurity(value);
}

#endif // SECMAN_ALLOW_DISABLE_XHR_ORIGIN_CHECK

/* static */ DOM_Object *
DOM_Utils::GetDOM_Object(ES_Object *object, ObjectType type)
{
	DOM_Object *hostobject = DOM_GetHostObject(object);

	if (type == TYPE_LIVECONNECT && hostobject)
	{
		ES_Value jval;
		OP_BOOLEAN ret = hostobject->GetPrivate(DOM_PRIVATE_plugin_scriptable, &jval);
		if (ret == OpBoolean::IS_TRUE)
			return GetDOM_Object(jval.value.object);
	}

	return hostobject;
}

/* static */ ES_Object *
DOM_Utils::GetES_Object(DOM_Object *object)
{
	return object ? (ES_Object *) *object : NULL;
}

/* static */ DOM_Object *
DOM_Utils::GetProxiedObject(DOM_Object *proxy)
{
	DOM_Object *ret = proxy;

	if (proxy && proxy->IsA(DOM_TYPE_PROXY_OBJECT))
	{
		DOM_ProxyObject *obj = static_cast<DOM_ProxyObject*>(proxy);
		RETURN_VALUE_IF_ERROR(obj->GetObject(ret), NULL);
	}

	return ret;
}

/* static */ HTML_Element *
DOM_Utils::GetHTML_Element(DOM_Object *node)
{
	return node && node->IsA(DOM_TYPE_NODE) ? ((DOM_Node *) node)->GetThisElement() : NULL;
}

/* static */ DOM_Object *
DOM_Utils::GetEventTarget(ES_Thread *thread)
{
	if (thread && thread->Type() == ES_THREAD_EVENT)
		return ((DOM_EventThread *) thread)->GetEventTarget();
	else
		return NULL;
}

/* static */ const uni_char *
DOM_Utils::GetCustomEventName(ES_Thread *thread)
{
	if (!thread || thread->Type() != ES_THREAD_EVENT)
		return NULL;

	DOM_Event *e = static_cast<DOM_EventThread*>(thread)->GetEvent();

	return e->GetType();
}

/* static */ const uni_char *
DOM_Utils::GetNamespaceURI(ES_Thread *thread)
{
#ifdef DOM3_EVENTS
	if (!thread || thread->Type() != ES_THREAD_EVENT)
		return NULL;

	DOM_Event *e = static_cast<DOM_EventThread*>(thread)->GetEvent();

	return e->GetNamespaceURI();
#else // DOM3_EVENTS
	(void)thread;
	return NULL;
#endif // DOM3_EVENTS
}

/* static */ HTML_Element *
DOM_Utils::GetEventPathParent(HTML_Element *currentTarget, HTML_Element *target)
{
	return DOM_Node::GetEventPathParent(currentTarget, target);
}

/* static */ HTML_Element *
DOM_Utils::GetActualEventTarget(HTML_Element *target)
{
	return DOM_Node::GetActualEventTarget(target);
}

/* static */ HTML_Element *
DOM_Utils::GetEventTargetElement(HTML_Element *element)
{
	return DOM_Node::GetEventTargetElement(element);
}

/* static */ BOOL
DOM_Utils::IsInlineScriptOrWindowOnLoad(ES_Thread* thread)
{
	// thread may be NULL.
	while (thread)
	{
		if (thread->Type() == ES_THREAD_INLINE_SCRIPT)
			return TRUE;

		if (thread->Type() == ES_THREAD_EVENT)
		{
			DOM_EventThread* event_thread = static_cast<DOM_EventThread*>(thread);
			if (event_thread->GetEventType() == ONLOAD)
			{
				if (event_thread->GetEvent()->GetWindowEvent())
					return TRUE;
			}
		}

		thread = thread->GetInterruptedThread();
	}
	return FALSE;
}

#ifdef JS_PLUGIN_SUPPORT

/* static */ ES_Object *
DOM_Utils::GetJSPluginObject(DOM_Object *object)
{
	if (object && object->IsA(DOM_TYPE_HTML_ELEMENT) && ((DOM_HTMLElement *) object)->GetThisElement()->Type() == HE_OBJECT)
		return ((DOM_HTMLObjectElement *) object)->GetJSPluginObject();
	else
		return NULL;
}

/* static */ void
DOM_Utils::SetJSPluginObject(DOM_Object *object, ES_Object *plugin_object)
{
	if (object->IsA(DOM_TYPE_HTML_ELEMENT) && ((DOM_HTMLElement *) object)->GetThisElement()->Type() == HE_OBJECT)
		((DOM_HTMLObjectElement *) object)->SetJSPluginObject(plugin_object);
}

#endif // JS_PLUGIN_SUPPORT

#ifdef DOM_INSPECT_NODE_SUPPORT
/* static */ OP_STATUS
DOM_Utils::InspectNode(DOM_Object *node, unsigned flags, InspectNodeCallback *callback)
{
	if (!node || !node->IsA(DOM_TYPE_NODE))
		return OpStatus::ERR;

	return static_cast<DOM_Node *>(node)->InspectNode(flags, callback);
}

/**
 * A DOM-private class that contains the implementation for DOM_Utils::Search.
 */
class DOM_SearchUtils
{
public:

	/**
	 * Options for Searcher::Search.
	 */
	struct SearchOptions
	{
		const uni_char *query;
		/**< The query string for the search. */

		BOOL ignore_case;
		/**< TRUE to ignore case, FALSE to respect case. */

		DOM_Utils::SearchCallback *callback;
		/**< Matches will be reported to this callback. */

		DOM_Node *node;
		/**< The root node for the search. */

		HTML_Element *element;
		/**< The HTML_Element associated with the node. */

		DOM_EnvironmentImpl *environment;
		/**< The environment that contains the node. */

		LogicalDocument *logdoc;
		/**< The document that contains the node. */
	};

	/**
	 * Base class for classes that implement a search mechanism (plaintext, regexp,
	 * XPath and CSS).
	 */
	class Searcher
	{
	public:
		virtual OP_STATUS Search(const SearchOptions &options) = 0;
		/**< This function should execute the search, and call ElementMatched
		     whenever a matching element is encountered.

			 @param options The search options.
			 @return OpStatus::OK if the search went well; OpStatus::ERR if an
			         error occurred (e.g. XPath expression could not be compiled);
			         or OpStatus::ERR_NO_MEMORY on OOM. */
	protected:
		OP_STATUS ElementMatched(const SearchOptions &options, HTML_Element *element);
		/**< Report to the callback that a matching element has been found.

		     @param options The search options previously passed to Search.
		     @return OpStatus::ERR_NO_MEMORY or OpStatus::OK. */
	};

	/**
	 * An ElementSearcher is a special kind of Searcher which traverses the
	 * document tree, and checks each element for a match by calling
	 * MatchElement().
	 *
	 * This class allows us to reuse the exact same searching algorithm,
	 * while letting subclasses decide the match condition by implementing
	 * MatchElement().
	 *
	 * Subclasses can initiate the search by calling SearchTree().
	 */
	class ElementSearcher
		: public Searcher
	{
	public:
		OP_STATUS SearchTree(const SearchOptions &options);
		/**< Executes search by iterating over all elements in the tree and call
		     MatchElement() on each of them. If MatchElement() returns TRUE
		     then the element is reported with ElementMatched().

		     @param options The search options.
		     @return OpStatus::OK if the search went well; OpStatus::ERR if an
		             error occurred;
		             or OpStatus::ERR_NO_MEMORY on OOM. */
	protected:
		virtual BOOL MatchElement(HTML_Element *element) = 0;
		/**< Check whether the element matches the search criteria.

		     @param element The HTML element to inspect.
		     @return TRUE if match, FALSE if no match. */
	};

	/**
	 * Implements searching for event-listeners by event name. The query
	 * string can match anywhere in the event name, an empty string means
	 * that all elements with event listeners will be matched.
	 */
	class EventSearcher
		: public ElementSearcher
	{
	public:
		virtual OP_STATUS Search(const SearchOptions &options);
		/**< Implements ElementSearcher. Initializes the searcher from the
		     search options by building an array of event types to look for. */
	protected:
		virtual BOOL MatchElement(HTML_Element *element);
		/**< Check whether the element contains the expected event listener.

		     @param element The HTML element to inspect.
		     @return TRUE if match, FALSE if no match. */

		virtual OP_STATUS Construct(const SearchOptions &options);
		/**< Second stage constructor which initializes the searcher from the
		     the search options by building an array of event types to look for.

		     @param options The options for the search.
		     @return OpStatus::OK on success; or OpStatus::ERR_NO_MEMORY on OMM */
	private:
		SearchOptions options;
		/** An array of booleans which spans all event types in the system.
		    The event type is the index and is used to figure out if an event
		    listener matches the event(s) we are after. */
		BOOL matching_events[DOM_EVENTS_COUNT];
	};

	/**
	 * Implements searching using CSS selectors.
	 */
	class SelectorSearcher
		: public Searcher
		, public CSS_QuerySelectorListener
	{
	public:
		virtual OP_STATUS Search(const SearchOptions &options);
		/**< Implements Searcher. */

		virtual OP_STATUS MatchElement(HTML_Element *element);
		/**< Implements CSS_QuerySelectorListener. */
	private:
		SearchOptions options;
		/**< Stores the options from Search. */
	};

	/**
	 * A Matcher is a special kind of Searcher which ("manually") traverses
	 * the document tree, and checks each node for a match.
	 *
	 * This class allows us to reuse the exact same same searching algortihm,
	 * while letting subclasses decide the match condition.
	 */
	class Matcher
		: public Searcher
	{
	public:
		virtual OP_STATUS Search(const SearchOptions &options);
		/**< Implements Searcher. */
	protected:
		virtual BOOL Match(const uni_char *haystack) = 0;
		/**< Check whether the query string matches a certain haystack.

		     @param haystack The string which may or may not contain 'arg.query'.
		     @return TRUE if match, FALSE if no match. */

		virtual OP_STATUS Construct(const uni_char *query, BOOL ignore_case) = 0;
		/**< Second stage constructor. (Needed by RegExp).

		     @param query The string to look for.
			 @param case_insensitive TRUE to ignore case, FALSE to respect case.
			 @return OpStatus::OK on success; OpStatus::ERR_NO_MEMORY; or OpStatus::ERR
			         for other errors (e.g. regexp syntax error). */
	};

	/**
	 * Implements plaintext search.
	 */
	class PlainTextSearcher
		: public Matcher
	{
	public:
		PlainTextSearcher();
		/**< Constructor. Sets members to zero. */
	protected:
		virtual BOOL Match(const uni_char *haystack);
		/**< Implements Matcher. */
		virtual OP_STATUS Construct(const uni_char *query, BOOL ignore_case);
		/**< Implements Matcher. */
	private:
		const uni_char *query; /**< Stored argument from Match. */
		BOOL ignore_case; /**< Stored argument from Match. */
	};

	/**
	 * Implements regular expression search.
	 */
	class RegExpSearcher
		: public Matcher
	{
	public:
		RegExpSearcher();
		/**< Constructor. Sets member(s) to zero. */
		~RegExpSearcher();
		/**< Frees 're', if present. */
	protected:
		virtual BOOL Match(const uni_char *haystack);
		/**< Implements Matcher. */
		virtual OP_STATUS Construct(const uni_char *query, BOOL ignore_case);
		/**< Implements Matcher. */
	private:
		OpRegExp *re; /**< Compiled regexp. */
	};

	/**
	 * Implements XPath searching.
	 */
	class XPathSearcher
		: public Searcher
	{
	public:
		XPathSearcher();
		/**< Constructors. Sets members to zero. */
		~XPathSearcher();
		/**< Frees members, if set. */
		virtual OP_STATUS Search(const SearchOptions &options);
		/**< Implements Searcher. */
	private:

		/** XPath search variables. They are only strictly needed in the Search
		    function, but by keeping them here as members, we simplify error
		    handling. (These types use a "non-standard" deletion protocol, i.e.
		    they can not simply be OP_DELETEd). */

		XPathExpression *expression;
		XMLTreeAccessor *tree;
		XPathExpression::Evaluate *evaluate;
		LogicalDocument *logdoc;
	};
};

OP_STATUS
DOM_SearchUtils::Searcher::ElementMatched(const SearchOptions &options, HTML_Element *element)
{
	DOM_Node *matched_node;
	RETURN_IF_ERROR(options.environment->ConstructNode(matched_node, element, options.node->GetOwnerDocument()));
	RETURN_IF_MEMORY_ERROR(options.callback->MatchNode(matched_node));
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
DOM_SearchUtils::ElementSearcher::SearchTree(const SearchOptions &options)
{
	HTML_Element *elm = options.element;
	HTML_Element *stop = elm->NextSiblingActual();

	for (; elm != stop; elm = elm->NextActual())
	{
		// Check element.
		if (MatchElement(elm))
			RETURN_IF_ERROR(ElementMatched(options, elm));
	}

	return OpStatus::OK;
}

/* virtual */ OP_STATUS
DOM_SearchUtils::EventSearcher::Search(const SearchOptions &options)
{
	RETURN_IF_ERROR(Construct(options));

	return SearchTree(options);
}

/* virtual */ BOOL
DOM_SearchUtils::EventSearcher::MatchElement(HTML_Element *element)
{
	DOM_EventTarget *event_target = NULL;
	if (element->DOMHasEventHandlerAttributes(options.environment))
	{
		DOM_Node *dom_node;
		RETURN_IF_ERROR(options.environment->ConstructNode(dom_node, element, options.node->GetOwnerDocument()));
		if (dom_node)
			event_target = dom_node->GetEventTarget();
	}
	else
	{
		if (element->GetESElement())
			event_target = element->GetESElement()->GetEventTarget();
	}

	if (!event_target)
		return FALSE;

	DOM_EventTarget::NativeIterator it = event_target->NativeListeners();
	for (; !it.AtEnd(); it.Next())
	{
		const DOM_EventListener *listener = it.Current();

		DOM_EventType event_type = listener->GetKnownType();
		if (event_type < DOM_EVENTS_COUNT && matching_events[event_type])
			return TRUE;

		if (event_type == DOM_EVENT_CUSTOM)
		{
			const uni_char *custom_event_name = listener->GetCustomEventType();
			if (options.ignore_case)
			{
				if (uni_stristr(custom_event_name, options.query) != NULL)
					return TRUE;
			}
			else
			{
				if (uni_strstr(custom_event_name, options.query) != NULL)
					return TRUE;
			}
		}
	}
	return FALSE;
}

/* virtual */ OP_STATUS
DOM_SearchUtils::EventSearcher::Construct(const SearchOptions &options)
{
	this->options = options;

	// Find all event types that matches options.query and store them in an array.
	OpString uni_event_name;
	for (int i = 0; i < DOM_EVENTS_COUNT; ++i)
	{
		matching_events[i] = FALSE;
		DOM_EventType event_type = static_cast<DOM_EventType>(i);
		const char *event_name = DOM_Environment::GetEventTypeString(event_type);
		RETURN_IF_ERROR(uni_event_name.Set(event_name));
		if (options.ignore_case)
			matching_events[i] = uni_event_name.FindI(options.query) != KNotFound;
		else
			matching_events[i] = uni_event_name.Find(options.query) != KNotFound;
	}

	return OpStatus::OK;
}

/* virtual */ OP_STATUS
DOM_SearchUtils::SelectorSearcher::Search(const SearchOptions &options)
{
	this->options = options;
	CSS_DOMException exception;
	return CSS_QuerySelector(options.node->GetHLDocProfile(), options.query, options.element, CSS_QUERY_ALL, this, exception);
}

/* virtual */ OP_STATUS
DOM_SearchUtils::SelectorSearcher::MatchElement(HTML_Element *element)
{
	return ElementMatched(options, element);
}

/* virtual */ OP_STATUS
DOM_SearchUtils::Matcher::Search(const SearchOptions &options)
{
	RETURN_IF_ERROR(Construct(options.query, options.ignore_case));

	HTML_Element *elm = options.element;
	HTML_Element *stop = elm->NextSiblingActual();

	while (elm != stop)
	{
		HTML_Element *next;

		// Check tag.
		BOOL match = Match(elm->GetTagName());

		if (!match)
		{
			// Check value.
			if (elm->IsText())
			{
				TempBuffer buffer;

				const uni_char *content = elm->DOMGetContentsString(options.environment, &buffer);
				if (!content)
					return OpStatus::ERR_NO_MEMORY;

				match = match || Match(content);

				next = elm->NextSiblingActual();
			}
			else
			{
				// Check attributes.
				HTML_AttrIterator attr_iter(elm);

				const uni_char *attr_name;
				const uni_char *attr_value;

				while (attr_iter.GetNext(attr_name, attr_value))
				{
					match = match || Match(attr_name);
					match = match || Match(attr_value);
				}

				next = elm->NextActual();
			}
		}
		else
			next = elm->NextActual();

		if (match)
			RETURN_IF_ERROR(ElementMatched(options, elm));

		elm = next;
	}

	return OpStatus::OK;
}

DOM_SearchUtils::PlainTextSearcher::PlainTextSearcher()
	: query(NULL)
	, ignore_case(FALSE)
{
}

/* virtual */ OP_STATUS
DOM_SearchUtils::PlainTextSearcher::Construct(const uni_char *query, BOOL ignore_case)
{
	this->query = query;
	this->ignore_case = ignore_case;

	return OpStatus::OK;
}

/* virtual */ BOOL
DOM_SearchUtils::PlainTextSearcher::Match(const uni_char *haystack)
{
	if (ignore_case)
		return haystack && (uni_stristr(haystack, query) != NULL);
	else
		return haystack && (uni_strstr(haystack, query) != NULL);
}

DOM_SearchUtils::RegExpSearcher::RegExpSearcher()
	: re(NULL)
{
}

DOM_SearchUtils::RegExpSearcher::~RegExpSearcher()
{
	OP_DELETE(re);
}

/* virtual */ OP_STATUS
DOM_SearchUtils::RegExpSearcher::Construct(const uni_char *query, BOOL ignore_case)
{
	OpREFlags flags;
	flags.case_insensitive = ignore_case;
	flags.multi_line = FALSE;
	flags.ignore_whitespace = FALSE;

	return OpRegExp::CreateRegExp(&re, query, &flags);
}

/* virtual */ BOOL
DOM_SearchUtils::RegExpSearcher::Match(const uni_char *haystack)
{
	if (!haystack)
		return FALSE;

	OpREMatchLoc match;
	RETURN_VALUE_IF_ERROR(re->Match(haystack, &match), FALSE);
	return match.matchloc != REGEXP_NO_MATCH;
}

DOM_SearchUtils::XPathSearcher::XPathSearcher()
	: expression(NULL)
	, tree(NULL)
	, evaluate(NULL)
	, logdoc(NULL)
{
}

DOM_SearchUtils::XPathSearcher::~XPathSearcher()
{
	XPathExpression::Evaluate::Free(evaluate);

	if (logdoc)
		logdoc->FreeXMLTreeAccessor(tree);

	XPathExpression::Free(expression);
}

/* virtual */ OP_STATUS
DOM_SearchUtils::XPathSearcher::Search(const SearchOptions &options)
{
	if (!options.logdoc)
		return OpStatus::ERR;

	logdoc = options.logdoc;

	XPathExpression::ExpressionData data;
	data.source = options.query;

	RETURN_IF_ERROR(XPathExpression::Make(expression, data));

	XMLTreeAccessor::Node *rootnode;
	RETURN_IF_ERROR(logdoc->CreateXMLTreeAccessor(tree, rootnode));

	RETURN_IF_ERROR(XPathExpression::Evaluate::Make(evaluate, expression));

	evaluate->SetRequestedType(XPathExpression::Evaluate::NODESET_SNAPSHOT);

	XMLTreeAccessor::Node *xml_node = logdoc->GetElementAsNode(tree, options.element);

	XPathNode *xpath_node;
	RETURN_IF_ERROR(XPathNode::Make(xpath_node, tree, xml_node));

	evaluate->SetContext(xpath_node); // Ownership transfer.

	OP_BOOLEAN finished = OpBoolean::IS_FALSE;

	while (finished == OpBoolean::IS_FALSE)
	{
		unsigned type;
		finished = evaluate->CheckResultType(type);
	}

	RETURN_IF_ERROR(finished);

	unsigned count = 0;

	finished = OpBoolean::IS_FALSE;

	while (finished == OpBoolean::IS_FALSE)
		finished = evaluate->GetNodesCount(count);

	RETURN_IF_ERROR(finished);

	for (unsigned i = 0; i < count; ++i)
	{
		XPathNode *xpath_node;
		RETURN_IF_ERROR(evaluate->GetNode(xpath_node, i));

		XMLTreeAccessor::Node *xml_node = xpath_node->GetNode();
		HTML_Element *elm = logdoc->GetNodeAsElement(tree, xml_node);

		RETURN_IF_ERROR(ElementMatched(options, elm));
	}

	return OpStatus::OK;
}

DOM_Utils::SearchOptions::SearchOptions(const uni_char *query, DOM_Runtime *runtime)
	: query(query)
	, runtime(runtime)
	, type(DOM_Utils::SEARCH_PLAINTEXT)
	, root(NULL)
	, ignore_case(FALSE)
{
	OP_ASSERT(query);
	OP_ASSERT(runtime);
}

void
DOM_Utils::SearchOptions::SetType(SearchType type)
{
	this->type = type;
}

void
DOM_Utils::SearchOptions::SetRoot(DOM_Object *root)
{
	this->root = root;
}

void
DOM_Utils::SearchOptions::SetIgnoreCase(BOOL ignore_case)
{
	this->ignore_case = ignore_case;
}

/* static */ OP_STATUS
DOM_Utils::Search(const SearchOptions &search_options, SearchCallback *callback)
{
	DOM_Object *root = search_options.root;

	if (root && !root->IsA(DOM_TYPE_NODE))
		return OpStatus::ERR;

	DOM_EnvironmentImpl *environment = search_options.runtime->GetEnvironment();

	// If not specified, use the document node as the root.
	if (!search_options.root)
	{
		RETURN_IF_ERROR(environment->ConstructDocumentNode());
		root = environment->GetDocument();
	}

	FramesDocument *frm_doc = environment->GetFramesDocument();

	if (!root || !frm_doc)
		return OpStatus::ERR; // Possibly a WebWorker runtime.

	DOM_SearchUtils::SearchOptions options;
	options.callback = callback;
	options.node = static_cast<DOM_Node*>(root);
	options.environment = environment;
	options.logdoc = frm_doc->GetLogicalDocument();
	options.query = search_options.query;
	options.element = options.node->GetPlaceholderElement();
	options.ignore_case = search_options.ignore_case;

	if (!options.element)
		return OpStatus::ERR;

	switch (search_options.type)
	{
	default:
		OP_ASSERT(!"Please add a new case.");
	case SEARCH_PLAINTEXT:
		return DOM_SearchUtils::PlainTextSearcher().Search(options);
	case SEARCH_REGEXP:
		return DOM_SearchUtils::RegExpSearcher().Search(options);
	case SEARCH_SELECTOR:
		return DOM_SearchUtils::SelectorSearcher().Search(options);
	case SEARCH_XPATH:
		return DOM_SearchUtils::XPathSearcher().Search(options);
	case SEARCH_EVENT:
		return DOM_SearchUtils::EventSearcher().Search(options);
	};
}
#endif // DOM_INSPECT_NODE_SUPPORT

#ifdef _PLUGIN_SUPPORT_
/* static */ OP_STATUS
DOM_Utils::GetWindowLocationObject(DOM_Object* window, DOM_Object*& location)
{
	if (window->IsA(DOM_TYPE_WINDOW))
	{
		location = static_cast<JS_Window*>(window)->GetLocation();
		return OpStatus::OK;
	}
	return OpStatus::ERR;
}
#endif // _PLUGIN_SUPPORT_

#ifdef EXTENSION_SUPPORT
/* static */ BOOL
DOM_Utils::WillUseExtensions(FramesDocument *frames_doc)
{
	/* Two cases to consider: extension background pages always
	 * have a DOM environment + extension UserJS may activate
	 * on a document.
	 */
	if (frames_doc->GetWindow()->GetType() == WIN_TYPE_GADGET && frames_doc->GetWindow()->GetGadget()->IsExtension())
		return TRUE;
	else
		return DOM_UserJSManager::CheckExtensionScripts(frames_doc);
}
#endif // EXTENSION_SUPPORT

#ifdef GADGET_SUPPORT
/* static */ OpGadget*
DOM_Utils::GetRuntimeGadget(DOM_Runtime *runtime)
{
#ifdef EXTENSION_SUPPORT
	if (runtime->HasSharedEnvironment())
		return runtime->GetEnvironment()->GetUserJSManager()->FindRuntimeGadget(runtime);
#endif // EXTENSION_SUPPORT
	if (runtime->GetFramesDocument())
		return runtime->GetFramesDocument()->GetWindow()->GetGadget();
	return NULL;
}
#endif // GADGET_SUPPORT

#ifdef DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT
/* static */ OP_STATUS
DOM_Utils::AppendExtensionMenu(OpWindowcommanderMessages_MessageSet::PopupMenuRequest::MenuItemList& menu_items, OpDocumentContext* document_context, FramesDocument* document, HTML_Element* element)
{
	return g_extension_manager->AppendExtensionMenu(menu_items, document_context, document, element);
}
#endif // DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT

/* static */ OP_STATUS
DOM_Utils::GetDOM_CSSStyleSheet(DOM_Object *node, DOM_Object *import_rule, DOM_Object *&stylesheet)
{
	if (!node || !node->IsA(DOM_TYPE_NODE))
		return OpStatus::ERR;

	DOM_Node *dom_node = static_cast<DOM_Node *>(node);
	DOM_CSSRule *dom_cssrule = static_cast<DOM_CSSRule*>(import_rule);

	ES_Value value;
	RETURN_IF_ERROR(dom_node->GetStyleSheet(&value, dom_cssrule, node->GetRuntime()));
	stylesheet = DOM_VALUE2OBJECT(value, DOM_Object);

	return (stylesheet ? OpStatus::OK : OpStatus::ERR);
}

/* static */ OP_STATUS
DOM_Utils::GetDOM_CSSStyleSheetOwner(DOM_Object *stylesheet, DOM_Object *&owner)
{
	if (!stylesheet || !stylesheet->IsA(DOM_TYPE_CSS_STYLESHEET))
		return OpStatus::ERR;

	DOM_CSSStyleSheet *dom_stylesheet = static_cast<DOM_CSSStyleSheet*>(stylesheet);

	owner = dom_stylesheet->GetOwnerNode();

	return (owner ? OpStatus::OK : OpStatus::ERR);
}

/* static */ BOOL
DOM_Utils::IsOperaIllegalURL(const URL &url)
{
    if (url.Type() == URL_OPERA)
    {
        const char *path = url.GetAttribute(URL::KPath).CStr();
        return path && op_strncmp(path, "illegal-url-", 12) == 0;
    }
    else
        return FALSE;
}

/* static */ OP_STATUS
DOM_Utils::GetSerializedOrigin(const URL &origin_url, TempBuffer &buffer, unsigned flags)
{
	if (!origin_url.GetAttribute(URL::KIsUniqueOrigin))
	{
		URLType type = origin_url.Type();
		const uni_char *domain = origin_url.GetServerName() ? origin_url.GetServerName()->UniName() : NULL;
		int port = origin_url.GetAttribute(URL::KServerPort, TRUE);

		if (type != URL_NULL_TYPE && (type != URL_FILE || (flags & SERIALIZE_FILE_SCHEME)) && domain)
		{
			RETURN_IF_ERROR(buffer.Append(urlManager->MapUrlType(type)));
			RETURN_IF_ERROR(buffer.Append("://"));
			RETURN_IF_ERROR(buffer.Append(domain));

			if (port > 0 && (type != URL_HTTP && type != URL_HTTPS || type == URL_HTTP && port != 80 || type == URL_HTTPS && port != 443))
			{
				RETURN_IF_ERROR(buffer.Append(":"));
				RETURN_IF_ERROR(buffer.AppendUnsignedLong(port));
			}
			return OpStatus::OK;
		}
	}

	if (flags & SERIALIZE_FALLBACK_AS_NULL)
		RETURN_IF_ERROR(buffer.Append("null"));

	return OpStatus::OK;
}

/* static */ BOOL
DOM_Utils::IsValidTokenValue(const uni_char *token, unsigned length)
{
	OP_ASSERT(token || length == 0 || !"Empty token string, but a non-zero length.");
	if (!token || length == 0)
		return FALSE;

	while (length-- > 0)
		if (*token <= 0x20 || (*token) >= 0x7f || uni_strchr(UNI_L("()<>@,;:\\\"/[]?={}"), *token) != NULL)
			return FALSE;
		else
			token++;

	return TRUE;
}

/* static */ BOOL
DOM_Utils::IsClipboardAccessAllowed(URL &origin_url, Window *window)
{
	BOOL allowed = FALSE;
#if defined(DOCUMENT_EDIT_SUPPORT) && defined(USE_OP_CLIPBOARD)
	OpStatus::Ignore(g_secman_instance->CheckSecurity(OpSecurityManager::DOM_CLIPBOARD_ACCESS,
	                                                  OpSecurityContext(origin_url, window),
	                                                  OpSecurityContext(),
	                                                  allowed));
#endif // DOCUMENT_EDIT_SUPPORT && USE_OP_CLIPBOARD
	return allowed;
}
