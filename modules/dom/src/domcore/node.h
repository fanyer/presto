/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_NODE_H
#define DOM_NODE_H

#include "modules/dom/src/domobj.h"
#include "modules/dom/src/domcore/nodemap.h"
#include "modules/dom/src/domevents/domeventtarget.h"
#include "modules/dom/domutils.h"

class HTML_Element;
class DOM_EventTarget;
class ES_Thread;
class DOM_Document;
class DOM_CSSRule;

/* See comments in core/ecma.cpp for information about object structure
   and storage management.
   */

class DOM_Node
	: public DOM_Object,
	  public DOM_EventTargetOwner
{
protected:
	DOM_NodeType node_type:5;
	unsigned is_significant:1;
	unsigned have_native_property:1;
#ifdef USER_JAVASCRIPT
	unsigned waiting_for_beforecss:1;
#endif // USER_JAVASCRIPT

	DOM_Document *owner_document;

public:
	DOM_Node(DOM_NodeType type);

#ifdef DOM_XPATH_REFERENCE_IMPLEMENTATION
	unsigned document_order_index;
#endif // DOM_XPATH_REFERENCE_IMPLEMENTATION

	virtual ~DOM_Node();
	virtual BOOL Clean() { return FALSE; }

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutNameRestart(const uni_char *property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime, ES_Object *restart_object);
	virtual ES_PutState PutNameRestart(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime, ES_Object *restart_object);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_NODE || DOM_Object::IsA(type); }

	virtual void GCTrace();
	/**< Calls GCTraceSpecial(FALSE).  Sub-classes should not override this
	     function; they should override GCTraceSpecial() instead. */
	virtual void GCTraceSpecial(BOOL via_tree);
	/**< Essentially the same as GCTrace() for other types of objects.  The
	     argument 'via_tree' is TRUE if this node was marked and traced via the
	     tree of elements/nodes by another node.  The idea is that one node's
	     GCTrace() is called, which triggers a call to this function with
	     via_tree==FALSE, which triggers a trace of the tree and calls to other
	     node's GCTraceSpecial() with via_tree==TRUE, during which those nodes
	     refrain from starting another trace of the tree.

	     Essentially: when via_tree is FALSE, trace all references, when
	     via_tree is TRUE, trace only non-node references and references to
	     nodes that (might) belong to other trees. */

	static void GCMarkAndTrace(DOM_Runtime *runtime, DOM_Node *node, BOOL only_if_significant = FALSE);
	/**< Call ES_Runtime::GCMark(node, mark_host_as_excluded=TRUE) and then
	     node->GCTraceSpecial(via_tree=TRUE).  The second argument in the GCMark
	     call will prevent the garbage collector to call node->GCTrace() later
	     on.  If 'node' is NULL, this function doesn't crash. */

	void TraceElementTree(HTML_Element *element);
	/**< Trace all significant nodes in the tree to which 'element' belongs,
	     using calls to GCMarkAndTrace() to do the actual marking and tracing.
	     The document node will also be marked and traced, if it is an ancestor
	     of the element.  Note: 'this' and 'element' are supposedly "the same",
	     but it doesn't matter that much, since 'this' is just used as context
	     reference to find the owner document node and environment. */

	void FreeElementTree(HTML_Element *element);
	/**< Clean up the tree to which 'element' belongs, resetting all pointers
	     between nodes and elements, and deallocate the element tree by calling
	     HTML_Element::DOMFreeElement() on the root element. */

	void FreeElement(HTML_Element *element);
	/**< If 'node' is significant, call FreeElementTree() to free the tree to
	     which it belongs, otherwise just reset the pointers between 'node' and
	     'element'. */

	virtual void DOMChangeRuntime(DOM_Runtime *new_runtime);
	virtual void DOMChangeOwnerDocument(DOM_Document *new_ownerDocument);

	//HTML_Element	*GetThisElement() { return associated_element; }
	DOM_NodeType	GetNodeType() { return (DOM_NodeType) node_type; }

	OP_STATUS GetParentNode(DOM_Node *&node);
	OP_STATUS GetFirstChild(DOM_Node *&node);
	OP_STATUS GetLastChild(DOM_Node *&node);
	OP_STATUS GetPreviousSibling(DOM_Node *&node);
	OP_STATUS GetNextSibling(DOM_Node *&node);
	OP_STATUS GetPreviousNode(DOM_Node *&node);
	OP_STATUS GetNextNode(DOM_Node *&node, BOOL skip_children = FALSE);
	BOOL IsAncestorOf(DOM_Node *other);

	OP_STATUS InsertChild(DOM_Node *child, DOM_Node *reference, DOM_Runtime *origining_runtime);
	OP_STATUS RemoveFromParent(ES_Runtime *origining_runtime);

	int RemoveAllChildren(BOOL restarted, ES_Value *return_value, DOM_Runtime *origining_runtime);

#ifdef DOM2_MUTATION_EVENTS
	// Call after modification.
	OP_STATUS SendNodeInserted(ES_Thread *interrupt_thread);
	// Call before modification.
	OP_STATUS SendNodeRemoved(ES_Thread *interrupt_thread);
#endif /* DOM2_MUTATION_EVENTS */

	BOOL GetIsInDocument();
	BOOL GetIsSignificant() { return (is_significant == 1); }
	/**< GetIsSignificant() means that there is information in the Node that
	     can't be reproduced by recreating the node based on an HTML_Element
	     in the element tree. Custom properties, event handlers and lots of
	     other small stuff. For trees outside the normal document tree the
	     root is normally significant as well to keep it alive. */
	void SetIsSignificant() { is_significant = 1; }

	BOOL HaveNativeProperty() { return (have_native_property == 1); }
	/**< HaveNativeProperty() returns TRUE if one or more custom / native properties have
	     been directly put on this node. A node returning TRUE will also be 'significant'. */
	void SetHaveNativeProperty() { have_native_property = 1; }

	/* These call SetIsSignificant and then EcmaScript_Object::PutPrivate. */
	OP_STATUS PutPrivate(int private_name, ES_Object *object);
	OP_STATUS PutPrivate(int private_name, DOM_Object *object);
	OP_STATUS PutPrivate(int private_name, ES_Value &value);

	static BOOL ShouldBlockWaitingForStyle(FramesDocument* frames_doc, ES_Runtime* origining_runtime);
	/**< Returns whether we should block the current thread waiting for
	     CSS to be loaded and parsed. Should be used instead of
	     frames_doc->IsWaitingForStyles() since there is at least
	     one case ("BeforeCSS" handlers) when we need to disregard the
	     frames_doc flag.

	     @param frames_doc The document or NULL. If NULL then this will return FALSE.
	     @param origining_runtime the source runtime, must not be NULL. */

	static HTML_Element *GetEventTargetElement(HTML_Element *element);
	/**< Returns the element that has the node that has the event target
	     that events to 'element' should really be fired to.  This is
	     'element' except if 'element' is a special SVG element. */
	static HTML_Element *GetEventPathParent(HTML_Element *currentTarget, HTML_Element *target);
	/**< Returns the parent of 'currentTarget' in the context of event
	     propagation.  This is Parent(), if the parent is an SVG shadow
	     node, and ParentActual(TRUE) otherwise. */
	static HTML_Element *GetActualEventTarget(HTML_Element *element);
	/**< Returns the element that will be returned by Event.target for
	     an event fired with 'target' as its target through a call to
	     DOM_Environment::HandleEvent.  The actual target will
	     typically be the nearest ancestor non-text element that is
	     not inserted by the layout engine. */
	static DOM_EventTarget *GetEventTargetFromElement(HTML_Element *element);
	/**< Returns the event target for 'element'.  Might not use
	     'element's own node; see GetEventTargetElement. */

	/* From DOM_EventTargetOwner: */
	virtual DOM_EventTarget *FetchEventTarget();
	virtual OP_STATUS CreateEventTarget();
	virtual DOM_Object *GetOwnerObject();

	/* Helper method for OP_ATOM_baseURI */
	ES_GetState GetBaseURI(ES_Value* value, ES_Runtime* origining_runtime);

	HTML_Element *GetThisElement();
	HTML_Element *GetPlaceholderElement();
	BOOL IsReadOnly();

	DOM_Document *GetOwnerDocument() { return owner_document; }
	void SetOwnerDocument(DOM_Document *document) { owner_document = document; }

	LogicalDocument *GetLogicalDocument();

	ES_GetState DOMSetElement(ES_Value *value, HTML_Element *element);
	/**< If 'value' is NULL, returns GET_SUCCESS; otherwise if
	     'element' is NULL sets value to null and returns
	     GET_SUCCESS; otherwise creates a node for 'element', sets
	     'value' to the created node and returns GET_SUCCESS.

	     @param value Value to set.  Can be NULL.
	     @param element Element to create node for.  Can be NULL.

	     @return GET_SUCCESS or GET_NO_MEMORY on OOM. */

	ES_GetState DOMSetParentNode(ES_Value *value);
	/**< If 'value' is NULL, returns GET_SUCCESS; otherwise if calls
	     GetParentNode() and sets the returned node as the value, or
	     null if no parent node was returned.

	     @param value Value to set.  Can be NULL.

	     @return GET_SUCCESS or GET_NO_MEMORY on OOM. */

	ES_GetState GetStyleSheet(ES_Value *value, DOM_CSSRule *import_rule, DOM_Runtime *origining_runtime);

	ES_GetState GetTextContent(ES_Value *value);
	/**< Returns the text content of the node as defined in
	     DOM3 Core, i.e. without any processing instructions or
		 comments.

 	     @return PUT_SUCCESS, PUT_FAILED or PUT_NO_MEMORY on OOM. */

	ES_PutState SetTextContent(ES_Value *value, DOM_Runtime *origining_runtime, ES_Object *restart_object = NULL);

#ifdef DOM_INSPECT_NODE_SUPPORT
	OP_STATUS InspectNode(unsigned flags, DOM_Utils::InspectNodeCallback *callback);

	OP_STATUS InspectDocumentNode(DOM_Utils::InspectNodeCallback *callback);
	/**< Used by InspectNode to perform additional inspection for document
	     nodes.

	     This function will get (or create) a reference to the parent document
	     and report this to 'callback'. The function deliberatley does not respect
	     origin security issues, as it is intended for debugging.

	     @param callback The callback to send the information to.
	     @return OpStatus::OK, or OpStatus::ERR_NO_MEMORY. */

	OP_STATUS InspectFrameNode(DOM_Utils::InspectNodeCallback *callback);
	/**< Used by InspectNode to perform additional inspection for frame
	     nodes (and other nodes that can contain documents).

	     This function will get (or create) a reference to the child document
	     and report this to 'callback'. The function deliberatley does not respect
	     origin security issues, as it is intended for debugging.

	     @param callback The callback to send the information to.
	     @return OpStatus::OK, or OpStatus::ERR_NO_MEMORY. */
#endif // DOM_INSPECT_NODE_SUPPORT

#ifdef USER_JAVASCRIPT
	static ES_GetState GetCssContents(DOM_Node *node, ES_Value *value, DOM_Runtime *origining_runtime, TempBuffer* buffer);
	static ES_PutState SetCssContents(DOM_Node *node, ES_Value *value, DOM_Runtime *origining_runtime);

	/**
	 * IsWaitingForBeforeCSS tells if this node is the target of
	 * a queued BeforeCSS event, or is the target of a BeforeCSS
	 * event being processed at this very moment. Because the
	 * UserJSEvent object GCMarks the target stylesheet, it's
	 * unneeded to mark the node as significant so the value of
	 * this flag will not be lost during a garbage collect.
	 */
	BOOL GetIsWaitingForBeforeCSS() { return waiting_for_beforecss; }
	void SetIsWaitingForBeforeCSS(BOOL v) { waiting_for_beforecss = v; }
#endif // USER_JAVASCRIPT

	/** Initialize the global variable "Node". */
	static void ConstructNodeObjectL(ES_Object *object, DOM_Runtime *runtime);

	/** Make MSIE-compatible event handling methods hard to detect. */
	static OP_STATUS HideMSIEEventAPI(ES_Object *object, DOM_Runtime *runtime);

	DOM_DECLARE_FUNCTION(insertBefore);
	DOM_DECLARE_FUNCTION(replaceChild);
	DOM_DECLARE_FUNCTION(removeChild);
	DOM_DECLARE_FUNCTION(appendChild);
	DOM_DECLARE_FUNCTION(hasChildNodes);
	DOM_DECLARE_FUNCTION(cloneNode);
	DOM_DECLARE_FUNCTION(normalize);
	DOM_DECLARE_FUNCTION(isSupported);
	DOM_DECLARE_FUNCTION(hasAttributes);
	DOM_DECLARE_FUNCTION(dispatchEvent);
	DOM_DECLARE_FUNCTION(isSameNode);
	DOM_DECLARE_FUNCTION(isEqualNode);
	DOM_DECLARE_FUNCTION(getFeature);
	DOM_DECLARE_FUNCTION(compareDocumentPosition);
	DOM_DECLARE_FUNCTION(contains);
	enum {
		FUNCTIONS_START,
		FUNCTIONS_insertBefore,
		FUNCTIONS_replaceChild,
		FUNCTIONS_removeChild,
		FUNCTIONS_appendChild,
		FUNCTIONS_hasChildNodes,
		FUNCTIONS_cloneNode,
#ifndef DOM_LIBRARY_FUNCTIONS
		FUNCTIONS_normalize,
#endif // DOM_LIBRARY_FUNCTIONS
		FUNCTIONS_isSupported,
		FUNCTIONS_hasAttributes,
		FUNCTIONS_dispatchEvent,
		FUNCTIONS_isSameNode,
		FUNCTIONS_isEqualNode,
		FUNCTIONS_getFeature,
		FUNCTIONS_compareDocumentPosition,
		FUNCTIONS_contains,
		FUNCTIONS_ARRAY_SIZE
	};

	DOM_DECLARE_FUNCTION_WITH_DATA(accessEventListener); // {add,remove}EventListener{,NS}, willTriggerNS and hasEventListenersNS
	DOM_DECLARE_FUNCTION_WITH_DATA(attachOrDetachEvent); // {attach,detach}Event
	DOM_DECLARE_FUNCTION_WITH_DATA(lookupNamespace); // lookup{Prefix,NamespaceURI} and isDefaultNamespace
#ifdef DOM3_XPATH
	DOM_DECLARE_FUNCTION_WITH_DATA(select); // select{Nodes,SingleNode}
#endif // DOM3_XPATH
	enum {
		FUNCTIONS_WITH_DATA_BASIC = 4,
		FUNCTIONS_WITH_DATA_lookupPrefix,
		FUNCTIONS_WITH_DATA_isDefaultNamespace,
		FUNCTIONS_WITH_DATA_lookupNamespaceURI,
#ifdef DOM3_XPATH
		FUNCTIONS_WITH_DATA_selectNodes,
		FUNCTIONS_WITH_DATA_selectSingleNode,
#endif // DOM3_XPATH
		FUNCTIONS_WITH_DATA_ARRAY_SIZE
	};

#ifdef DOM_LIBRARY_FUNCTIONS
	enum {
		LIBRARY_FUNCTIONS_BASIC = 1,
		LIBRARY_FUNCTIONS_ARRAY_SIZE
	};
#endif // DOM_LIBRARY_FUNCTIONS
};

#ifdef DOM3_XPATH

class DOM_XPathResult;

class DOM_XPathResult_NodeList
	: public DOM_Object
{
protected:
	DOM_XPathResult *result;

public:
	DOM_XPathResult_NodeList(DOM_XPathResult *result);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_GetState GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_XPATHRESULT_NODELIST || DOM_Object::IsA(type); }
	virtual void GCTrace();

	virtual ES_GetState GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime);

	DOM_DECLARE_FUNCTION(item);
	enum
	{
		FUNCTIONS_BASIC = 1,
		FUNCTIONS_ARRAY_SIZE
	};
};

#endif // DOM3_XPATH

#endif // DOM_NODE_H
