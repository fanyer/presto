/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/dom/src/domcore/node.h"

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domcore/domdoc.h"
#include "modules/dom/src/domcore/docfrag.h"
#include "modules/dom/src/domcore/attr.h"
#include "modules/dom/src/domcore/comment.h"
#include "modules/dom/src/domcore/chardata.h"
#include "modules/dom/src/domcore/domxmldocument.h"
#include "modules/dom/src/domcore/entity.h"
#include "modules/dom/src/domcore/text.h"
#include "modules/dom/src/domhtml/htmldoc.h"
#include "modules/dom/src/domhtml/htmlcoll.h"
#include "modules/dom/src/domhtml/htmlelem.h"
#include "modules/dom/src/domevents/domevent.h"
#include "modules/dom/src/domevents/domeventdata.h"
#include "modules/dom/src/domevents/domeventthread.h"
#include "modules/dom/src/domevents/domeventtarget.h"
#include "modules/dom/src/domevents/domeventlistener.h"
#include "modules/dom/src/domevents/dommutationevent.h"
#include "modules/dom/src/domcss/cssstylesheet.h"
#include "modules/dom/src/domxpath/xpathnamespace.h"
#include "modules/dom/src/domxpath/xpathresult.h"
#include "modules/dom/src/domxpath/xpathevaluator.h"
#include "modules/dom/src/domxpath/xpathnsresolver.h"
#include "modules/dom/src/js/window.h"
#include "modules/dom/src/userjs/userjsmanager.h"
#include "modules/dom/src/domsvg/domsvgelementinstance.h"

#include "modules/doc/frm_doc.h"
#include "modules/dochand/fdelm.h"
#include "modules/ecmascript_utils/essched.h"
#include "modules/ecmascript_utils/esthread.h"
#include "modules/logdoc/datasrcelm.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/xmlenum.h"
#include "modules/probetools/probepoints.h"
#include "modules/util/tempbuf.h"
#include "modules/xmlutils/xmldocumentinfo.h"
#include "modules/xpath/xpath.h"

#if defined SVG_DOM
# include "modules/svg/SVGManager.h"
#endif

#ifdef DRAG_SUPPORT
# include "modules/pi/OpDragObject.h"
# include "modules/dragdrop/dragdrop_manager.h"
#endif // DRAG_SUPPORT

#ifdef DOM_EXPENSIVE_DEBUG_CODE
#include "modules/util/OpHashTable.h"

class DOM_NodeElm
{
public:
	DOM_Node *node;
	DOM_NodeElm *next;
	unsigned signalled;
} *g_DOM_all_nodes;

static void
DOM_AddNode(DOM_Node *node)
{
	DOM_NodeElm *elm = OP_NEW(DOM_NodeElm, ());
	elm->node = node;
	elm->next = g_DOM_all_nodes;
	elm->signalled = FALSE;
	g_DOM_all_nodes = elm;
}

static void
DOM_RemoveNode(DOM_Node *node)
{
	DOM_NodeElm **elmp = &g_DOM_all_nodes;
	BOOL found = FALSE;

	while (*elmp)
	{
		if ((*elmp)->node == node)
		{
			found = TRUE;

			DOM_NodeElm *discard = *elmp;
			*elmp = (*elmp)->next;
			OP_DELETE(discard);

			if (!node->IsA(DOM_TYPE_DOCUMENT))
				break;
		}
		else
		{
			if ((*elmp)->node->GetOwnerDocument() == node)
				(*elmp)->node->SetOwnerDocument(NULL);
			elmp = &(*elmp)->next;
		}
	}

	OP_ASSERT(found);
}

OpGenericPointerSet g_DOM_element_with_node;

void
DOM_VerifyAllNodes()
{
	DOM_NodeElm *iter = g_DOM_all_nodes;

	while (iter)
	{
		if ((iter->signalled & 2) == 0)
			if (iter->node->GetNodeType() == ELEMENT_NODE)
				if (!iter->node->GetThisElement())
				{
					OP_ASSERT(FALSE);
					iter->signalled |= 2;
				}

		if ((iter->signalled & 4) == 0)
			if (HTML_Element *element = iter->node->GetThisElement())
				if (OpStatus::IsError(g_DOM_element_with_node.Add(element)))
				{
					OP_ASSERT(FALSE);
					iter->signalled |= 4;
				}

		iter = iter->next;
	}

	g_DOM_element_with_node.RemoveAll();
}
#endif // DOM_EXPENSIVE_DEBUG_CODE

OP_STATUS
DOM_Node::GetParentNode(DOM_Node *&node)
{
	if (HTML_Element *this_element = GetThisElement())
		if (HTML_Element *parent = this_element->ParentActual())
			return GetEnvironment()->ConstructNode(node, parent, owner_document);

	node = NULL;
	return OpStatus::OK;
}

OP_STATUS
DOM_Node::GetFirstChild(DOM_Node *&node)
{
	if (node_type == ATTRIBUTE_NODE)
		RETURN_IF_ERROR(static_cast<DOM_Attr *>(this)->CreateValueTree(NULL));

	if (HTML_Element *placeholder = GetPlaceholderElement())
		if (HTML_Element *first_child = placeholder->FirstChildActual())
			return GetEnvironment()->ConstructNode(node, first_child, owner_document);

	node = NULL;
	return OpStatus::OK;
}

OP_STATUS
DOM_Node::GetLastChild(DOM_Node *&node)
{
	if (node_type == ATTRIBUTE_NODE)
		RETURN_IF_ERROR(static_cast<DOM_Attr *>(this)->CreateValueTree(NULL));

	if (HTML_Element *placeholder = GetPlaceholderElement())
		if (HTML_Element *last_child = placeholder->LastChildActual())
			return GetEnvironment()->ConstructNode(node, last_child, owner_document);

	node = NULL;
	return OpStatus::OK;
}

OP_STATUS
DOM_Node::GetPreviousSibling(DOM_Node *&node)
{
	if (HTML_Element *this_element = GetThisElement())
		if (HTML_Element *previous_sibling = this_element->PredActual())
			return GetEnvironment()->ConstructNode(node, previous_sibling, owner_document);

	node = NULL;
	return OpStatus::OK;
}

OP_STATUS
DOM_Node::GetNextSibling(DOM_Node *&node)
{
	if (HTML_Element *this_element = GetThisElement())
		if (HTML_Element *next_sibling = this_element->SucActual())
			return GetEnvironment()->ConstructNode(node, next_sibling, owner_document);

	node = NULL;
	return OpStatus::OK;
}

OP_STATUS
DOM_Node::GetPreviousNode(DOM_Node *&node)
{
	if (HTML_Element *this_element = GetThisElement())
		if (HTML_Element *previous_node = this_element->PrevActual())
			return GetEnvironment()->ConstructNode(node, previous_node, owner_document);

	node = NULL;
	return OpStatus::OK;
}

OP_STATUS
DOM_Node::GetNextNode(DOM_Node *&node, BOOL skip_children)
{
	HTML_Element *element = GetThisElement();

	if (!element)
		element = GetPlaceholderElement();

	if (element)
	{
		HTML_Element *next_node;

		if (skip_children || element->Type() == Markup::HTE_TEXTGROUP)
			next_node = element->NextSiblingActual();
		else
			next_node = element->NextActual();

		if (next_node)
			return GetEnvironment()->ConstructNode(node, next_node, owner_document);
	}

	node = NULL;
	return OpStatus::OK;
}

BOOL
DOM_Node::IsAncestorOf(DOM_Node *other)
{
	if (this == other)
		return TRUE;
	else
	{
		HTML_Element *ancestor = GetPlaceholderElement();
		HTML_Element *descendant = other->GetThisElement();

		return ancestor && descendant && ancestor->IsAncestorOf(descendant);
	}
}

OP_STATUS
DOM_Node::PutPrivate(int private_name, ES_Object *object)
{
	SetIsSignificant();
	return EcmaScript_Object::PutPrivate(private_name, object);
}

OP_STATUS
DOM_Node::PutPrivate(int private_name, DOM_Object *object)
{
	return PutPrivate(private_name, *object);
}

OP_STATUS
DOM_Node::PutPrivate(int private_name, ES_Value &value)
{
	SetIsSignificant();
	return EcmaScript_Object::PutPrivate(private_name, value);
}

/* static */
BOOL
DOM_Node::ShouldBlockWaitingForStyle(FramesDocument* frames_doc, ES_Runtime* origining_runtime)
{
	if (frames_doc && frames_doc->IsWaitingForStyles())
	{
		ES_Thread* thread = GetCurrentThread(origining_runtime);
		while (thread)
		{
			if (thread->Type() == ES_THREAD_EVENT)
			{
				DOM_Event* event_object = static_cast<DOM_EventThread*>(thread)->GetEvent();

				if (event_object->GetKnownType() == DOM_EVENT_CUSTOM && uni_str_eq(event_object->GetType(), "BeforeCSS"))
					return FALSE; // Can't have BeforeCSS block waiting for style since that would mean waiting for itself.
				break;
			}

			thread = thread->GetInterruptedThread();
		}

		return TRUE;
	}
	return FALSE;
}

/* static */
HTML_Element *
DOM_Node::GetEventTargetElement(HTML_Element *element)
{
#ifdef SVG_DOM
	if (element->GetNsType() == NS_SVG)
		// "shadow" nodes share event target with the real node
		return g_svg_manager->GetEventTarget(element);
#endif // SVG_DOM

	return element;
}

/* static */ HTML_Element *
DOM_Node::GetEventPathParent(HTML_Element *currentTarget, HTML_Element *target)
{
	HTML_Element *parent;

#ifdef SVG_DOM
	parent = currentTarget->Parent();
	if (!parent || parent->GetInserted() != HE_INSERTED_BY_SVG)
#endif // SVG_DOM
	{
		// Find elements up the chain. Accepts visible nodes
		// plus the HE_DOC_ROOT which represents the document
		for (parent = currentTarget->Parent(); parent; parent = parent->Parent())
		{
			if (parent->Type() == Markup::HTE_DOC_ROOT || parent->IsIncludedActual() && !parent->IsText())
				break;
		}
	}

	return parent;
}

/* static */ HTML_Element *
DOM_Node::GetActualEventTarget(HTML_Element *element)
{
	HTML_Element *parent = element;
	while (parent && (parent->GetInserted() == HE_INSERTED_BY_LAYOUT || parent->IsText()))
		parent = parent->Parent();
	if (parent)
		return parent;
	else
		return element;
}

/* static */
DOM_EventTarget *
DOM_Node::GetEventTargetFromElement(HTML_Element *element)
{
	element = GetEventTargetElement(element);

	if (DOM_Node* node = (DOM_Node *) element->GetESElement())
		return node->GetEventTarget();
	else
		return NULL;
}

/* virtual */ DOM_EventTarget *
DOM_Node::FetchEventTarget()
{
#ifdef SVG_DOM
	if (HTML_Element *this_element = GetThisElement())
	{
		HTML_Element *element = GetEventTargetElement(this_element);
		if (element != this_element)
		{
			DOM_Node* node = (DOM_Node *) element->GetESElement();

			if (node)
				return node->FetchEventTarget();
			else
				return NULL;
		}
	}
#endif // SVG_DOM

	return event_target;
}

/* virtual */ OP_STATUS
DOM_Node::CreateEventTarget()
{
#ifdef SVG_DOM
	if (HTML_Element *this_element = GetThisElement())
	{
		HTML_Element *element = GetEventTargetElement(this_element);
		if (element != this_element)
		{
			DOM_Node* node;

			RETURN_IF_ERROR(GetEnvironment()->ConstructNode(node, element, owner_document));

			return node->CreateEventTarget();
		}
	}
#endif // SVG_DOM

	SetIsSignificant();

	return DOM_EventTargetOwner::CreateEventTarget();
}

HTML_Element *
DOM_Node::GetThisElement()
{
	if (node_type == ELEMENT_NODE)
		return static_cast<DOM_Element *>(this)->GetThisElement();
	else if (IsA(DOM_TYPE_CHARACTERDATA) || node_type == PROCESSING_INSTRUCTION_NODE)
		return static_cast<DOM_CharacterData *>(this)->GetThisElement();
	else if (IsA(DOM_TYPE_DOCUMENTTYPE))
		return static_cast<DOM_DocumentType *>(this)->GetThisElement();
#ifdef SVG_DOM
	else if(node_type == SVG_ELEMENTINSTANCE_NODE)
		return static_cast<DOM_SVGElementInstance *>(this)->GetThisElement();
#endif // SVG_DOM
	else
		return NULL;
}

HTML_Element *
DOM_Node::GetPlaceholderElement()
{
	if (node_type == ELEMENT_NODE)
		return static_cast<DOM_Element *>(this)->GetThisElement();
	else if (node_type == DOCUMENT_FRAGMENT_NODE)
		return static_cast<DOM_DocumentFragment *>(this)->GetPlaceholderElement();
	else if (node_type == DOCUMENT_NODE)
		return static_cast<DOM_Document *>(this)->GetPlaceholderElement();
#ifdef DOM_SUPPORT_ENTITY
	else if (node_type == ENTITY_NODE)
		return static_cast<DOM_Entity *>(this)->GetPlaceholderElement();
#endif // DOM_SUPPORT_ENTITY
	else if (node_type == ATTRIBUTE_NODE)
		return static_cast<DOM_Attr *>(this)->GetPlaceholderElement();

	return NULL;
}

BOOL
DOM_Node::IsReadOnly()
{
#ifdef DOM_SUPPORT_ENTITY
	if (node_type == ENTITY_NODE)
		return !static_cast<DOM_Entity *>(this)->IsBeingParsed();
#endif // DOM_SUPPORT_ENTITY

	return FALSE;
}

LogicalDocument *
DOM_Node::GetLogicalDocument()
{
	if (owner_document)
		return owner_document->GetLogicalDocument();
	else
	{
		OP_ASSERT(node_type == DOCUMENT_NODE);
		return static_cast<DOM_Document *>(this)->GetLogicalDocument();
	}
}

ES_GetState
DOM_Node::DOMSetElement(ES_Value *value, HTML_Element *element)
{
	if (value)
		if (element)
		{
			DOM_Document *document = owner_document;
			if (!document)
			{
				OP_ASSERT(node_type == DOCUMENT_NODE);
				document = (DOM_Document *) this;
			}

			DOM_Node *node;
			GET_FAILED_IF_ERROR(GetEnvironment()->ConstructNode(node, element, document));
			value->type = VALUE_OBJECT;
			value->value.object = *node;
		}
		else
			value->type = VALUE_NULL;

	return GET_SUCCESS;
}

ES_GetState
DOM_Node::DOMSetParentNode(ES_Value *value)
{
	if (value)
	{
		DOM_Node *node;
		GET_FAILED_IF_ERROR(GetParentNode(node));
		DOMSetObject(value, node);
	}

	return GET_SUCCESS;
}

ES_GetState
DOM_Node::GetStyleSheet(ES_Value *value, DOM_CSSRule *import_rule, DOM_Runtime *origining_runtime)
{
	HTML_Element *element = GetThisElement();

	if (element)
	{
		BOOL has_stylesheet = element->IsStyleElement();

		if (!has_stylesheet && element->IsLinkElement())
		{
			if (element->Type() == HE_PROCINST)
				has_stylesheet = TRUE;
			else
			{
				const uni_char *rel = element->GetStringAttr(ATTR_REL);
				if (rel)
				{
					unsigned kinds = LinkElement::MatchKind(rel);
					has_stylesheet = (kinds & LINK_TYPE_STYLESHEET) != 0;
				}
			}
		}

		if (has_stylesheet)
		{
			if (value)
			{
				ES_GetState result = DOMSetPrivate(value, DOM_PRIVATE_sheet);
				if (result != GET_FAILED)
					return result;

				DOM_CSSStyleSheet *stylesheet;

				GET_FAILED_IF_ERROR(DOM_CSSStyleSheet::Make(stylesheet, this, import_rule));
				GET_FAILED_IF_ERROR(PutPrivate(DOM_PRIVATE_sheet, *stylesheet));

				DOMSetObject(value, stylesheet);
			}

			return GET_SUCCESS;
		}
	}

	return GET_FAILED;
}

#ifdef DOM2_MUTATION_EVENTS
OP_STATUS DOM_Node::SendNodeInserted(ES_Thread *interrupt_thread)
{
	DOM_EnvironmentImpl *environment = GetEnvironment();
	HTML_Element *this_element = GetThisElement();

	if (environment->IsEnabled() && this_element)
	{
		BOOL has_nodeinserted_handlers = environment->HasEventHandlers(DOMNODEINSERTED);
		BOOL has_nodeinsertedintodocument_handlers = environment->HasEventHandlers(DOMNODEINSERTEDINTODOCUMENT);

		if (has_nodeinserted_handlers || has_nodeinsertedintodocument_handlers)
		{
			if (has_nodeinserted_handlers && environment->HasEventHandler(this_element, DOMNODEINSERTED))
			{
				DOM_Node *node;
				RETURN_IF_ERROR(environment->ConstructNode(node, this_element->ParentActual(), owner_document));
				RETURN_IF_ERROR(DOM_MutationEvent::SendNodeInserted(interrupt_thread, this, node));
			}

			if (has_nodeinsertedintodocument_handlers && GetIsInDocument())
			{
				HTML_Element *elm;
				elm = this_element;
				while (elm && this_element->IsAncestorOf(elm))
				{
					if (environment->HasEventHandler(elm, DOMNODEINSERTEDINTODOCUMENT))
					{
						DOM_Node *node;
						RETURN_IF_ERROR(environment->ConstructNode(node, elm, owner_document));
						RETURN_IF_ERROR(DOM_MutationEvent::SendNodeInsertedIntoDocument(interrupt_thread, node));
					}
					elm = (HTML_Element *) elm->Next();
				}
			}
		}
	}

	return OpStatus::OK;
}

OP_STATUS DOM_Node::SendNodeRemoved(ES_Thread *interrupt_thread)
{
	DOM_EnvironmentImpl *environment = GetEnvironment();
	HTML_Element *this_element = GetThisElement();

	if (environment->IsEnabled() && this_element)
	{
		BOOL has_noderemove_handlers = environment->HasEventHandlers(DOMNODEREMOVED);
		BOOL has_noderemovedfromdocument_handlers = environment->HasEventHandlers(DOMNODEREMOVEDFROMDOCUMENT);

		if (has_noderemove_handlers || has_noderemovedfromdocument_handlers)
		{
			if (has_noderemove_handlers && environment->HasEventHandler(this_element, DOMNODEREMOVED))
			{
				DOM_Node *node;
				RETURN_IF_ERROR(environment->ConstructNode(node, this_element->ParentActual(), owner_document));
				RETURN_IF_ERROR(DOM_MutationEvent::SendNodeRemoved(interrupt_thread, this, node));
			}

			if (has_noderemovedfromdocument_handlers && GetIsInDocument())
			{
				HTML_Element *elm;
				elm = this_element;
				while (elm && this_element->IsAncestorOf(elm))
				{
					if (environment->HasEventHandler(elm, DOMNODEREMOVEDFROMDOCUMENT))
					{
						DOM_Node *node;
						RETURN_IF_ERROR(environment->ConstructNode(node, elm, owner_document));
						RETURN_IF_ERROR(DOM_MutationEvent::SendNodeRemovedFromDocument(interrupt_thread, node));
					}
					elm = (HTML_Element *) elm->Next();
				}
			}
		}
	}

	return OpStatus::OK;
}
#endif /* DOM2_MUTATION_EVENTS */

BOOL DOM_Node::GetIsInDocument()
{
	return owner_document ? owner_document->IsAncestorOf(this) : IsA(DOM_TYPE_DOCUMENT);
}

ES_GetState DOM_Node::GetBaseURI(ES_Value* value, ES_Runtime* origining_runtime)
{
	if (value)
	{
		DOMSetNull(value);

		/**
		 * Specification:
		 *  http://www.whatwg.org/specs/web-apps/current-work/multipage/urls.html#document-base-url
		 * Lookup in order:
		 *  - xml:base in current scope
		 *  - <base> element
		 *  - document url
		 */
		if (owner_document)
		{
			URL result;
			if (LogicalDocument* logdoc = owner_document->GetLogicalDocument())
			{
				if (HTML_Element* elm = GetThisElement())
					// lookup xml:base
					result = elm->DeriveBaseUrl(logdoc);

				if (result.IsEmpty() && logdoc->GetBaseURL())
					// lookup <base> uri
					result = *logdoc->GetBaseURL();
			}

			if (result.IsEmpty())
				// use document url
				result = owner_document->GetURL();

			if (!result.IsEmpty())
			{
				OpString url_str;
				GET_FAILED_IF_ERROR(result.GetAttribute(URL::KUniName, url_str));
				TempBuffer *buffer = GetEmptyTempBuf();
				GET_FAILED_IF_ERROR(buffer->Append(url_str.CStr()));
				DOMSetString(value, buffer);
			}
		}
	}

	return GET_SUCCESS;
}

OP_STATUS DOM_Node::InsertChild(DOM_Node *child, DOM_Node *reference, DOM_Runtime *origining_runtime)
{
	DOM_EnvironmentImpl *environment = GetEnvironment();
	HTML_Element *this_elm = GetPlaceholderElement();
	HTML_Element *child_elm = child->GetThisElement();
	HTML_Element *reference_elm = reference ? reference->GetThisElement() : NULL;

	OP_ASSERT(this_elm);

	/* The child should have been removed by the caller prior to calling
	   this function. */
	OP_ASSERT(!child_elm->Parent());

	DOM_EnvironmentImpl::CurrentState state(environment, origining_runtime);
	RETURN_IF_ERROR(this_elm->DOMInsertChild(environment, child_elm, reference_elm));

	if (node_type == DOCUMENT_NODE)
		if (child->node_type == ELEMENT_NODE && ((DOM_Document *) this)->GetRootElement() != child)
		{
			OP_ASSERT(!((DOM_Document *) this)->GetRootElement());
			((DOM_Document *) this)->SetRootElement((DOM_Element *) child);
		}
		else if (child->node_type == DOCUMENT_TYPE_NODE)
			RETURN_IF_ERROR(((DOM_Document *) this)->UpdateXMLDocumentInfo());

	return OpStatus::OK;
}

OP_STATUS
DOM_Node::RemoveFromParent(ES_Runtime *origining_runtime)
{
	DOM_EnvironmentImpl *environment = GetEnvironment();
	HTML_Element *element = GetThisElement();

	RETURN_IF_ERROR(environment->SignalOnBeforeRemove(GetThisElement(), static_cast<DOM_Runtime *>(origining_runtime)));
	environment->SetCallMutationListeners(FALSE);

	SetIsSignificant();

	DOM_EnvironmentImpl::CurrentState state(environment, static_cast<DOM_Runtime *>(origining_runtime));

	OP_STATUS status = element->DOMRemoveFromParent(environment);
	environment->SetCallMutationListeners(TRUE);
	RETURN_IF_ERROR(status);

	if (owner_document->GetRootElement() == this)
		owner_document->SetRootElement(NULL);

	return OpStatus::OK;
}

int
DOM_Node::RemoveAllChildren(BOOL restarted, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_EnvironmentImpl *environment = GetEnvironment();

#ifdef DOM2_MUTATION_EVENTS
	if (restarted)
	{
		int result = removeChild(this, NULL, -1, return_value, origining_runtime);
		if (result != ES_VALUE)
			return result;
	}

	BOOL has_mutation_listeners = FALSE;

	if (environment->HasEventHandlers(DOMNODEREMOVED))
		has_mutation_listeners = TRUE;
	else if (environment->HasEventHandlers(DOMNODEREMOVEDFROMDOCUMENT) && GetOwnerDocument()->IsAncestorOf(this))
		has_mutation_listeners = TRUE;

	if (has_mutation_listeners)
	{
		DOM_Node *child;
		CALL_FAILED_IF_ERROR(GetFirstChild(child));

		while (child)
		{
			int result;

			ES_Value arguments[1];
			DOMSetObject(&arguments[0], child);

			result = DOM_Node::removeChild(this, arguments, 1, return_value, origining_runtime);

			if (result != ES_VALUE)
				return result;

			CALL_FAILED_IF_ERROR(GetFirstChild(child));
		}
	}
	else
#endif // DOM2_MUTATION_EVENTS
	{
		if (HTML_Element *placeholder = GetPlaceholderElement())
		{
			OP_STATUS status = OpStatus::OK;
			OpStatus::Ignore(status);

			DOM_EnvironmentImpl::CurrentState state(environment, origining_runtime);

			status = placeholder->DOMRemoveAllChildren(environment);

			CALL_FAILED_IF_ERROR(status);
		}
	}

	return ES_FAILED;
}

/* virtual */ DOM_Object *
DOM_Node::GetOwnerObject()
{
	return this;
}

DOM_Node::DOM_Node(DOM_NodeType type)
	: node_type(type),
	  is_significant(type == DOCUMENT_NODE || type == DOCUMENT_FRAGMENT_NODE || type == ATTRIBUTE_NODE ? 1 : 0),
	  have_native_property(0),
#ifdef USER_JAVASCRIPT
	  waiting_for_beforecss(FALSE),
#endif // USER_JAVASCRIPT
	  owner_document(NULL)
{
#ifdef DOM_EXPENSIVE_DEBUG_CODE
	DOM_AddNode(this);
#endif // DOM_EXPENSIVE_DEBUG_CODE
#ifdef DOM_XPATH_REFERENCE_IMPLEMENTATION
	document_order_index = ~0u;
#endif // DOM_XPATH_REFERENCE_IMPLEMENTATION
}

/* virtual */
DOM_Node::~DOM_Node()
{
#ifdef DOM_EXPENSIVE_DEBUG_CODE
	DOM_RemoveNode(this);
#endif // DOM_EXPENSIVE_DEBUG_CODE
}

/* virtual */ void
DOM_Node::GCTrace()
{
	GCTraceSpecial(FALSE);
}

/* virtual */ void
DOM_Node::GCTraceSpecial(BOOL via_tree)
{
	if (!via_tree && owner_document && !owner_document->IsAncestorOf(this))
		GCMark(owner_document);
	GCMark(event_target);
}

/* static */ void
DOM_Node::GCMarkAndTrace(DOM_Runtime *runtime, DOM_Node *node, BOOL only_if_significant)
{
	if (node)
	{
		if (!only_if_significant || node->GetIsSignificant())
			runtime->GCMark(node, TRUE);
		else
			ES_Runtime::GCExcludeHostFromTrace(node);

		node->GCTraceSpecial(TRUE);
	}
}

void
DOM_Node::TraceElementTree(HTML_Element *element)
{
	while (HTML_Element *parent = element->Parent())
		element = parent;

	do
		if (DOM_Node *node = (DOM_Node *) element->GetESElement())
			DOM_Node::GCMarkAndTrace(GetRuntime(), node, TRUE);
	while ((element = (HTML_Element *) element->Next()) != NULL);
}

void
DOM_Node::FreeElementTree(HTML_Element *element)
{
	OP_PROBE5(OP_PROBE_DOM_FREEELEMENTTREE);

	while (element->Parent())
		element = element->Parent();

	GetEnvironment()->TreeDestroyed(element);

	HTML_Element *iter = element;
	while (iter)
	{
		if (DOM_Node *node = (DOM_Node *) iter->GetESElement())
		{
			DOM_NodeType node_type = node->GetNodeType();
			if (node_type == ELEMENT_NODE)
				static_cast<DOM_Element *>(node)->ClearThisElement();
			else if (node_type == DOCUMENT_NODE)
				static_cast<DOM_Document *>(node)->ClearPlaceholderElement();
			else if (node_type == DOCUMENT_FRAGMENT_NODE)
				static_cast<DOM_DocumentFragment *>(node)->ClearPlaceholderElement();
			else if (node_type == ATTRIBUTE_NODE)
				static_cast<DOM_Attr *>(node)->ClearPlaceholderElement();
			else if (node_type == DOCUMENT_TYPE_NODE)
			{
				static_cast<DOM_DocumentType *>(node)->SetThisElement(NULL);
			}
#ifdef DOM_SUPPORT_ENTITY
			else if (node_type == ENTITY_NODE)
				static_cast<DOM_Entity *>(node)->ClearPlaceholderElement();
#endif // DOM_SUPPORT_ENTITY
#ifdef SVG_DOM
			else if (node_type == SVG_ELEMENTINSTANCE_NODE)
				static_cast<DOM_SVGElementInstance *>(node)->ClearThisElement();
#endif // SVG_DOM
			else
			{
				OP_ASSERT(node_type == TEXT_NODE || node_type == COMMENT_NODE || node_type == PROCESSING_INSTRUCTION_NODE || node_type == CDATA_SECTION_NODE);
				static_cast<DOM_CharacterData *>(node)->ClearThisElement();
			}

			iter->SetESElement(NULL);
		}

		iter = (HTML_Element *) iter->Next();
	}

	if (GetRuntime()->InGC())
		if (FramesDocument *frames_doc = GetRuntime()->GetFramesDocument())
			frames_doc->SetDelayDocumentFinish(TRUE);

	DOM_EnvironmentImpl *environment = GetEnvironment();

	environment->SetCallMutationListeners(FALSE);

	HTML_Element::DOMFreeElement(element, environment);

	environment->SetCallMutationListeners(TRUE);
}

void
DOM_Node::FreeElement(HTML_Element *element)
{
	if (!GetIsSignificant())
	{
#ifdef _DEBUG
		/* Check that there is at least one significant node in this subtree or
		   that the subtree is infact the document.

		   Note: at this point GetOwnerDocument() may point to a deleted object.
		   This can only happen if 'element' is in a detached subtree, since if
		   it was in the subtree rooted by the owner document, this node and its
		   element would have been decoupled when the owner document was
		   deleted.  The usage of GetOwnerDocument() below should be safe, since
		   we're just using the pointer value, and the memory can't have been
		   reused to store a different document node yet, having been freed by
		   the same GC run and all. */
		BOOL ok = FALSE;
		HTML_Element *iter = element;
		while (iter->Parent())
		{
			iter = iter->Parent();
			if (iter->Type() == HE_DOC_ROOT && iter->GetESElement() == GetOwnerDocument())
			{
				ok = TRUE;
				break;
			}
		}
		if (!ok)
			while (iter)
			{
				if (DOM_Node *node = (DOM_Node *) iter->GetESElement())
				{
					OP_ASSERT(GetOwnerDocument() == node->GetOwnerDocument());
					if (node->GetIsSignificant())
					{
						ok = TRUE;
						break;
					}
				}
				iter = (HTML_Element *) iter->Next();
			}
		OP_ASSERT(ok);
#endif // _DEBUG

		element->SetESElement(NULL);
	}
	else
		FreeElementTree(element);
}

/* virtual */ void
DOM_Node::DOMChangeRuntime(DOM_Runtime *new_runtime)
{
	DOM_Object::DOMChangeRuntime(new_runtime);
	DOMChangeRuntimeOnPrivate(DOM_PRIVATE_sheet);
	DOMChangeRuntimeOnPrivate(DOM_PRIVATE_style);
}

/* virtual */ void
DOM_Node::DOMChangeOwnerDocument(DOM_Document *new_ownerDocument)
{
	SetOwnerDocument(new_ownerDocument);
}

/* virtual */ ES_GetState
DOM_Node::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_nodeType:
		DOMSetNumber(value, node_type);
		/* Put this constant value on the native object, 'caching' it there.
		   Should [[Put]] of "nodeType" be attempted, the operation will be
		   delegated to this host object, DOM_Node, which will issue a
		   DOMException. If the property has already been made special
		   (by definining a getter, say), do not overwrite. */
		if (value && !GetRuntime()->IsSpecialProperty(*this, UNI_L("nodeType")))
			GET_FAILED_IF_ERROR(GetRuntime()->PutName(GetNativeObject(), UNI_L("nodeType"), *value, PROP_READ_ONLY | PROP_HOST_PUT));

		return GET_SUCCESS;

	case OP_ATOM_nodeValue:
	case OP_ATOM_parentNode:
	case OP_ATOM_firstChild:
	case OP_ATOM_lastChild:
	case OP_ATOM_previousSibling:
	case OP_ATOM_nextSibling:
	case OP_ATOM_attributes:
	case OP_ATOM_namespaceURI:
	case OP_ATOM_prefix:
		DOMSetNull(value);
		return GET_SUCCESS;

	case OP_ATOM_baseURI:
		return GetBaseURI(value, origining_runtime);

	case OP_ATOM_childNodes:
		if (value)
		{
			ES_GetState result = DOMSetPrivate(value, DOM_PRIVATE_childNodes);
			if (result != GET_FAILED)
				return result;
			else
			{
				DOM_Collection *collection;
				DOM_SimpleCollectionFilter filter(CHILDNODES);

				GET_FAILED_IF_ERROR(DOM_Collection::MakeNodeList(collection, GetEnvironment(), NULL, FALSE, FALSE, filter));
				GET_FAILED_IF_ERROR(PutPrivate(DOM_PRIVATE_childNodes, *collection));

				DOMSetObject(value, collection);
			}
		}
		return GET_SUCCESS;

	case OP_ATOM_ownerDocument:
		DOMSetObject(value, owner_document);
		return GET_SUCCESS;

	case OP_ATOM_localName:
		if (value)
			if (node_type == ELEMENT_NODE)
			{
				TempBuffer *buffer = GetEmptyTempBuf();
				const uni_char *localpart = static_cast<DOM_Element *>(this)->GetTagName(buffer, FALSE, FALSE);
				if (!localpart)
					return GET_NO_MEMORY;

				DOMSetString(value, localpart);
			}
			else
				DOMSetNull(value);
		return GET_SUCCESS;

	case OP_ATOM_sheet:
		return GetStyleSheet(value, NULL, static_cast<DOM_Runtime *>(origining_runtime));

	case OP_ATOM_textContent:
		return GetTextContent(value);
	}

	return GET_FAILED;
}

ES_GetState
DOM_Node::GetTextContent(ES_Value* value)
{
	if (value)
	{
		DOM_EnvironmentImpl *environment = GetEnvironment();
		HTML_Element *root = GetThisElement();

		if (!root)
			root = GetPlaceholderElement();

		TempBuffer *buffer = GetEmptyTempBuf();
		const uni_char *string_value = NULL;
		BOOL first = TRUE;

		if (root)
			for (HTML_Element *iter = root, *stop = root->NextSiblingActual(); iter != stop;)
			{
				if (iter->IsText())
				{
					if (first)
					{
						if (!(string_value = iter->DOMGetContentsString(environment, buffer)))
							return GET_NO_MEMORY;

						first = FALSE;
					}
					else
					{
						if (string_value && buffer->Length() == 0)
							GET_FAILED_IF_ERROR(buffer->Append(string_value));

						string_value = NULL;

						GET_FAILED_IF_ERROR(iter->DOMGetContents(environment, buffer));
					}
					iter = iter->NextSiblingActual();
				}
				else
					iter = iter->NextActual();
			}

		if (string_value)
			DOMSetString(value, string_value);
		else
			DOMSetString(value, buffer);
	}
	return GET_SUCCESS;
}

ES_PutState
DOM_Node::SetTextContent(ES_Value *value, DOM_Runtime *origining_runtime, ES_Object *restart_object)
{
	if (value->type != VALUE_STRING)
		return PUT_NEEDS_STRING;

	while (1)
	{
		OP_BOOLEAN isRemoveChild = OpBoolean::IS_FALSE;
		ES_Value arguments[1], return_value;
		int result = ES_VALUE;

		if (restart_object)
		{
			DOMSetObject(&return_value, restart_object);

			ES_Value dummy;
			isRemoveChild = origining_runtime->GetName(restart_object, UNI_L("isRemoveChild"), &dummy);
			PUT_FAILED_IF_ERROR(isRemoveChild);

			if (isRemoveChild == OpBoolean::IS_TRUE)
				result = DOM_Node::removeChild(NULL, NULL, -1, &return_value, origining_runtime);
			else
				result = DOM_Node::appendChild(NULL, NULL, -1, &return_value, origining_runtime);

			restart_object = NULL;
		}
		else
		{
			DOM_Node *child;
			PUT_FAILED_IF_ERROR(GetFirstChild(child));

			if (child)
			{
				DOMSetObject(&arguments[0], child);

				result = DOM_Node::removeChild(this, arguments, 1, &return_value, origining_runtime);
				isRemoveChild = OpBoolean::IS_TRUE;
			}
			else if (*value->value.string)
			{
				DOM_Text *text;
				PUT_FAILED_IF_ERROR(DOM_Text::Make(text, this, value->value.string));
				DOMSetObject(&arguments[0], text);
				result = DOM_Node::appendChild(this, arguments, 1, &return_value, origining_runtime);
			}
		}

		if (result == (ES_SUSPEND | ES_RESTART))
		{
			ES_Value dummy;

			if (isRemoveChild == OpBoolean::IS_TRUE)
				PUT_FAILED_IF_ERROR(origining_runtime->PutName(return_value.value.object, UNI_L("isRemoveChild"), dummy));

			*value = return_value;
		}

		if (result != ES_VALUE)
			return ConvertCallToPutName(result, value);

		if (isRemoveChild == OpBoolean::IS_FALSE)
			break;
	}

	return PUT_SUCCESS;
}

/* virtual */ ES_PutState
DOM_Node::PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	ES_PutState state = DOM_Object::PutName(property_name, property_code, value, origining_runtime);
	if (state == PUT_FAILED && !HaveNativeProperty())
	{
		SetIsSignificant();
		SetHaveNativeProperty();
		return PUT_FAILED_DONT_CACHE;
	}
	return state;
}

/* virtual */ ES_PutState
DOM_Node::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_nodeType:
	case OP_ATOM_parentNode:
	case OP_ATOM_ownerDocument:
	case OP_ATOM_nodeName:
	case OP_ATOM_firstChild:
	case OP_ATOM_lastChild:
	case OP_ATOM_previousSibling:
	case OP_ATOM_nextSibling:
	case OP_ATOM_childNodes:
	case OP_ATOM_attributes:
	case OP_ATOM_namespaceURI:
	case OP_ATOM_localName:
	case OP_ATOM_sheet:
	case OP_ATOM_children:
	case OP_ATOM_parentElement:
	case OP_ATOM_baseURI:
		return PUT_READ_ONLY;

	case OP_ATOM_nodeValue:
	case OP_ATOM_prefix:
		return PUT_SUCCESS;

	case OP_ATOM_textContent:
		if (value->type == VALUE_NULL)
			DOMSetString(value);
		return SetTextContent(value, (DOM_Runtime *) origining_runtime);
	}

	return PUT_FAILED;
}

/* virtual */ ES_PutState
DOM_Node::PutNameRestart(const uni_char *property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime, ES_Object *restart_object)
{
	return DOM_Object::PutNameRestart(property_name, property_code, value, origining_runtime, restart_object);
}

/* virtual */ ES_PutState
DOM_Node::PutNameRestart(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime, ES_Object *restart_object)
{
	OP_ASSERT(property_name == OP_ATOM_text || property_name == OP_ATOM_textContent);
	if (property_name == OP_ATOM_textContent && value->type == VALUE_NULL)
		DOMSetString(value);
	return SetTextContent(value, (DOM_Runtime *) origining_runtime, restart_object);
}

#ifdef DOM_INSPECT_NODE_SUPPORT

OP_STATUS
DOM_Node::InspectNode(unsigned flags, DOM_Utils::InspectNodeCallback *callback)
{
	HTML_Element *this_element = GetThisElement();
	DOM_Document *document = GetOwnerDocument();
	const uni_char *namespace_uri = NULL, *prefix = NULL, *localpart = NULL;
	const uni_char *value = NULL;
	int ns_idx;
	TempBuffer value_buffer;
	TempBuffer name_buffer;
	BOOL has_children = FALSE;

	callback->SetType(this, node_type);

	switch (node_type)
	{
	case ELEMENT_NODE:
		if (!(localpart = this_element->DOMGetElementName(GetEnvironment(), &name_buffer, ns_idx, TRUE)))
			return OpStatus::ERR_NO_MEMORY;

		GetThisElement()->DOMGetNamespaceData(GetEnvironment(), ns_idx, namespace_uri, prefix);
		has_children = TRUE;

		switch (this_element->Type())
		{
		case HE_FRAME:
		case HE_OBJECT:
		case HE_IFRAME:
			RETURN_IF_MEMORY_ERROR(InspectFrameNode(callback));
			break;
		}

		break;

	case ATTRIBUTE_NODE:
		namespace_uri = static_cast<DOM_Attr *>(this)->GetNsUri();
		prefix = static_cast<DOM_Attr *>(this)->GetNsPrefix();
		localpart = static_cast<DOM_Attr *>(this)->GetName();
		value = static_cast<DOM_Attr *>(this)->GetValue();
		break;

	case PROCESSING_INSTRUCTION_NODE:
		localpart = this_element->DOMGetPITarget(GetEnvironment());
		/* fall through */

	case TEXT_NODE:
	case CDATA_SECTION_NODE:
	case COMMENT_NODE:
		RETURN_IF_ERROR(static_cast<DOM_CharacterData *>(this)->GetValue(&value_buffer));
		value = value_buffer.GetStorage();
		if (!value)
			value = UNI_L("");
		break;

	case DOCUMENT_NODE:
		has_children = TRUE;
		RETURN_IF_MEMORY_ERROR(InspectDocumentNode(callback));
		break;

	case DOCUMENT_FRAGMENT_NODE:
		has_children = TRUE;
		break;

	case DOCUMENT_TYPE_NODE:
		if ((flags & DOM_Utils::INSPECT_BASIC) != 0)
			callback->SetDocumentTypeInformation(this, static_cast<DOM_DocumentType *>(this)->GetName(), static_cast<DOM_DocumentType *>(this)->GetPublicId(), static_cast<DOM_DocumentType *>(this)->GetSystemId());
		break;

	default:
		return OpStatus::OK;
	}

	if ((flags & DOM_Utils::INSPECT_BASIC) != 0)
	{
		if (localpart)
			callback->SetName(this, namespace_uri, prefix, localpart);

		if (value)
			callback->SetValue(this, value);
	}

	if (node_type == ELEMENT_NODE && (flags & DOM_Utils::INSPECT_ATTRIBUTES) != 0)
		RETURN_IF_ERROR(static_cast<DOM_Element *>(this)->InspectAttributes(callback));

	if (GetThisElement() && document && (flags & DOM_Utils::INSPECT_PARENT) != 0)
		if (HTML_Element *parent_element = GetThisElement()->ParentActual())
		{
			DOM_Node *parent_node;
			RETURN_IF_ERROR(GetEnvironment()->ConstructNode(parent_node, parent_element, document));
			callback->SetParent(this, parent_node);
		}

	if (has_children && document && (flags & DOM_Utils::INSPECT_CHILDREN) != 0)
	{
		HTML_Element *child_element = GetPlaceholderElement()->FirstChildActual();

		while (child_element)
		{
			DOM_Node *child_node;
			RETURN_IF_ERROR(GetEnvironment()->ConstructNode(child_node, child_element, document));
			callback->AddChild(this, child_node);
			child_element = child_element->SucActual();
		}
	}

	return OpStatus::OK;
}

OP_STATUS
DOM_Node::InspectDocumentNode(DOM_Utils::InspectNodeCallback *callback)
{
	DOM_Document *document = static_cast<DOM_Document *>(this);

	FramesDocument *frm_doc = document->GetFramesDocument();

	if (!frm_doc)
		return OpStatus::ERR;

	DocumentManager *docman = frm_doc->GetDocManager();
	FramesDocument *pdoc = docman->GetParentDoc();
	FramesDocElm *frame = docman->GetFrame();

	if (!pdoc || !frame)
		return OpStatus::ERR;

	HTML_Element *element = frame->GetHtmlElement();

	if (element)
	{
		RETURN_IF_ERROR(pdoc->ConstructDOMEnvironment());
		DOM_EnvironmentImpl *environment = static_cast<DOM_EnvironmentImpl*>(pdoc->GetDOMEnvironment());
		RETURN_IF_ERROR(environment->ConstructDocumentNode());
		DOM_Document *doc = static_cast<DOM_Document*>(environment->GetDocument());

		ES_Value value;
		if (doc->DOMSetElement(&value, element) == GET_SUCCESS)
			callback->SetDocumentInformation(this, DOM_Utils::GetDOM_Object(value.value.object));
	}

	return OpStatus::OK;
}

OP_STATUS
DOM_Node::InspectFrameNode(DOM_Utils::InspectNodeCallback *callback)
{
	HTML_Element *this_element = GetThisElement();

	DOM_ProxyEnvironment *frame_environment;
	FramesDocument *frame_frames_doc;
	DOM_Runtime *runtime = GetEnvironment()->GetDOMRuntime();
	RETURN_IF_ERROR(this_element->DOMGetFrameProxyEnvironment(frame_environment, frame_frames_doc, GetEnvironment()));

	if (frame_environment)
	{
		DOM_Object *content_document = NULL;
		RETURN_IF_ERROR(static_cast<DOM_ProxyEnvironmentImpl *>(frame_environment)->GetProxyDocument(content_document, runtime));
		callback->SetFrameInformation(this, content_document);
	}

	return OpStatus::OK;
}

#endif // DOM_INSPECT_NODE_SUPPORT

class DOM_Mutation_State
	: public DOM_Object
{
public:
	enum Point { ADOPT, REMOVE, INSERT, FINISHED } point;
	DOM_Node *this_node, *new_node, *old_node, *extra_node;
	ES_Value return_value;

	DOM_Mutation_State(Point point, DOM_Node *this_node, DOM_Node *new_node, DOM_Node *old_node, DOM_Node *extra_node)
		: point(point), this_node(this_node), new_node(new_node), old_node(old_node), extra_node(extra_node)
	{
	}

	virtual void GCTrace()
	{
		GCMark(this_node);
		GCMark(new_node);
		GCMark(old_node);
		GCMark(extra_node);
		GCMark(return_value);
	}
};

/* static */ int
DOM_Node::insertBefore(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_Mutation_State *state = NULL;
	DOM_Mutation_State::Point point = DOM_Mutation_State::ADOPT;

	DOM_Node *this_node;
	DOM_Node *new_node;
	DOM_Node *old_node;

	if (argc >= 0)
	{
		DOM_THIS_OBJECT_EXISTING(this_node, DOM_TYPE_NODE, DOM_Node);
		DOM_CHECK_ARGUMENTS("oO");
		DOM_ARGUMENT_OBJECT_EXISTING(new_node, 0, DOM_TYPE_NODE, DOM_Node);
		DOM_ARGUMENT_OBJECT_EXISTING(old_node, 1, DOM_TYPE_NODE, DOM_Node);
	}
	else
	{
		state = DOM_VALUE2OBJECT(*return_value, DOM_Mutation_State);
		point = state->point;
		this_object = this_node = state->this_node;
		new_node = state->new_node;
		old_node = state->old_node;
		*return_value = state->return_value;
	}

	HTML_Element *this_elm = this_node->GetPlaceholderElement();
	HTML_Element *new_elm = new_node->GetThisElement();
	HTML_Element *old_elm = old_node ? old_node->GetThisElement() : NULL;

	if (!new_elm && new_node->GetNodeType() == DOCUMENT_FRAGMENT_NODE)
		new_elm = ((DOM_DocumentFragment *) new_node)->GetPlaceholderElement();

	if (!this_elm && this_node->GetNodeType() == DOCUMENT_NODE)
		return DOM_CALL_DOMEXCEPTION(NO_MODIFICATION_ALLOWED_ERR);

	if (!this_elm || !new_elm || new_elm->IsAncestorOf(this_elm))
		return DOM_CALL_DOMEXCEPTION(HIERARCHY_REQUEST_ERR);

	if (this_node->IsReadOnly())
		return DOM_CALL_DOMEXCEPTION(NO_MODIFICATION_ALLOWED_ERR);

	if (this_node->GetNodeType() == DOCUMENT_NODE || this_node->GetNodeType() == ATTRIBUTE_NODE)
	{
		BOOL ok = TRUE, element_found = FALSE, doctype_found = FALSE, is_document = this_node->GetNodeType() == DOCUMENT_NODE;

		DOM_Node *iter;

		if (new_node->GetNodeType() == DOCUMENT_FRAGMENT_NODE)
			CALL_FAILED_IF_ERROR(new_node->GetFirstChild(iter));
		else
			iter = new_node;

		while (iter)
		{
			BOOL ok0 = FALSE;

			if (is_document)
				switch (iter->GetNodeType())
				{
				case COMMENT_NODE:
				case PROCESSING_INSTRUCTION_NODE:
					ok0 = TRUE;
					break;

				case ELEMENT_NODE:
					if (!element_found)
					{
						if (DOM_Element *element = ((DOM_Document *) this_node)->GetRootElement())
						{
							if (element == new_node)
								ok0 = TRUE;
						}
						else
							ok0 = TRUE;
						element_found = TRUE;
					}
					break;

				case TEXT_NODE:
					CALL_FAILED_IF_ERROR(((DOM_Text *) new_node)->IsWhitespace(ok0));
					break;

				case DOCUMENT_TYPE_NODE:
					if (!doctype_found)
					{
						ok0 = TRUE;
						doctype_found = TRUE;
					}
				}
			else
				ok0 = iter->GetNodeType() == TEXT_NODE;

			if (!ok0)
			{
				ok = FALSE;
				break;
			}

			if (new_node->GetNodeType() == DOCUMENT_FRAGMENT_NODE)
				CALL_FAILED_IF_ERROR(iter->GetNextSibling(iter));
			else
				break;
		}

		if (!ok)
			return DOM_CALL_DOMEXCEPTION(HIERARCHY_REQUEST_ERR);
	}
	else if (new_node->GetNodeType() == DOCUMENT_TYPE_NODE)
		return DOM_CALL_DOMEXCEPTION(HIERARCHY_REQUEST_ERR);

	if (new_elm->IsAncestorOf(this_elm))
		return DOM_CALL_DOMEXCEPTION(HIERARCHY_REQUEST_ERR);

	if (old_node && (!old_elm || old_elm->ParentActual() != this_elm))
		return DOM_CALL_DOMEXCEPTION(NOT_FOUND_ERR);

	if (new_elm == old_elm)
	{
		DOMSetObject(return_value, new_node);
		return ES_VALUE;
	}

	BOOL is_restarted = state != NULL;

#define IS_POINT(p) point == p
#define SET_POINT(p) if (state) state->point = p; point = p; is_restarted = FALSE
#define IS_RESTARTED (is_restarted)

	if (IS_POINT(DOM_Mutation_State::ADOPT))
	{
		int result;

		if (!IS_RESTARTED)
		{
			if (this_node->GetOwnerDocument() != new_node->GetOwnerDocument())
			{
				ES_Value arguments[1];
				DOMSetObject(&arguments[0], new_node);

				result = DOM_Document::adoptNode(this_node->GetOwnerDocument(), arguments, 1, return_value, origining_runtime);
			}
			else
				result = ES_VALUE;
		}
		else
			result = DOM_Document::adoptNode(NULL, NULL, -1, return_value, origining_runtime);

		if (result == (ES_SUSPEND | ES_RESTART))
			goto suspend;

		if (result != ES_VALUE)
			return result;

		SET_POINT(DOM_Mutation_State::REMOVE);
	}

	if (new_node->GetNodeType() == DOCUMENT_FRAGMENT_NODE)
	{
		while (1)
		{
			int result;

			if (!IS_RESTARTED)
				if (HTML_Element *elm = new_elm->FirstChild())
				{
					DOM_Node *node;
					CALL_FAILED_IF_ERROR(this_node->GetEnvironment()->ConstructNode(node, elm, this_node->owner_document));

					ES_Value arguments[2];
					DOMSetObject(&arguments[0], node);
					DOMSetObject(&arguments[1], old_node);

					result = insertBefore(this_node, arguments, 2, return_value, origining_runtime);
				}
				else
					break;
			else
				result = insertBefore(NULL, NULL, -1, return_value, origining_runtime);

			if (result == (ES_SUSPEND | ES_RESTART))
				goto suspend;

			if (result != ES_VALUE)
				return result;

			is_restarted = FALSE;
		}
	}
	else
	{
		if (IS_POINT(DOM_Mutation_State::REMOVE))
		{
			int result = ES_VALUE;

			if (!IS_RESTARTED)
			{
				DOM_Node *new_parent;
				CALL_FAILED_IF_ERROR(new_node->GetParentNode(new_parent));

				if (new_parent)
				{
					ES_Value argument;
					DOMSetObject(&argument, new_node);

					result = removeChild(new_parent, &argument, 1, return_value, origining_runtime);
				}
			}
#ifdef DOM2_MUTATION_EVENTS
			else
				result = removeChild(NULL, NULL, -1, return_value, origining_runtime);

			if (result == (ES_SUSPEND | ES_RESTART))
				goto suspend;
#endif // DOM2_MUTATION_EVENTS

			if (result != ES_VALUE)
				return result;

			SET_POINT(DOM_Mutation_State::INSERT);
		}

		if (IS_POINT(DOM_Mutation_State::INSERT))
		{
			CALL_FAILED_IF_ERROR(this_node->InsertChild(new_node, old_node, origining_runtime));

#ifdef DOM2_MUTATION_EVENTS
			ES_Thread *thread = GetCurrentThread(origining_runtime);

			CALL_FAILED_IF_ERROR(new_node->SendNodeInserted(thread));

			if (thread && thread->IsBlocked())
			{
				SET_POINT(DOM_Mutation_State::FINISHED);
				goto suspend;
			}
#endif // DOM2_MUTATION_EVENTS
		}
	}

	DOMSetObject(return_value, new_node);
	return ES_VALUE;

#undef IS_POINT
#undef SET_POINT
#undef IS_RESTARTED

suspend:
	if (!state)
		CALL_FAILED_IF_ERROR(DOMSetObjectRuntime(state = OP_NEW(DOM_Mutation_State, (point, this_node, new_node, old_node, NULL)), this_node->GetRuntime()));
	state->return_value = *return_value;
	DOMSetObject(return_value, state);
	return ES_SUSPEND | ES_RESTART;
}

/* static */ int
DOM_Node::replaceChild(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_Mutation_State *state = NULL;
	DOM_Mutation_State::Point point = DOM_Mutation_State::REMOVE;

	DOM_Node* this_node;
	DOM_Node* new_node;
	DOM_Node* old_node;
	DOM_Node* extra_node;

	if (argc >= 0)
	{
		DOM_THIS_OBJECT_EXISTING(this_node, DOM_TYPE_NODE, DOM_Node);
		DOM_CHECK_ARGUMENTS("oo");
		DOM_ARGUMENT_OBJECT_EXISTING(new_node, 0, DOM_TYPE_NODE, DOM_Node);
		DOM_ARGUMENT_OBJECT_EXISTING(old_node, 1, DOM_TYPE_NODE, DOM_Node);

		if (new_node->IsAncestorOf(this_node))
			return DOM_CALL_DOMEXCEPTION(HIERARCHY_REQUEST_ERR);

		if (new_node->GetNodeType() == DOCUMENT_NODE || (new_node->GetNodeType() == DOCUMENT_TYPE_NODE && this_node->GetNodeType() != DOCUMENT_NODE))
			return DOM_CALL_DOMEXCEPTION(HIERARCHY_REQUEST_ERR);

		CALL_FAILED_IF_ERROR(old_node->GetNextSibling(extra_node));
	}
	else
	{
		state = DOM_VALUE2OBJECT(*return_value, DOM_Mutation_State);
		point = state->point;
		this_object = this_node = state->this_node;
		new_node = state->new_node;
		old_node = state->old_node;
		extra_node = state->extra_node;
		*return_value = state->return_value;
	}

#define IS_POINT(p) point == p
#define SET_POINT(p) if (state) state->point = p; point = p
#define IS_RESTARTED (state != NULL)

	int result = ES_FAILED;

	if (IS_POINT(DOM_Mutation_State::REMOVE))
	{
		if (!IS_RESTARTED)
		{
			ES_Value argument;
			DOMSetObject(&argument, old_node);

			result = removeChild(this_node, &argument, 1, return_value, origining_runtime);
		}
#ifdef DOM2_MUTATION_EVENTS
		else
			result = removeChild(NULL, NULL, -1, return_value, origining_runtime);

		if (result == (ES_SUSPEND | ES_RESTART))
			goto suspend;
#endif // DOM2_MUTATION_EVENTS

		if (result != ES_VALUE)
			return result;

		SET_POINT(DOM_Mutation_State::INSERT);
	}

	if (IS_POINT(DOM_Mutation_State::INSERT))
	{
		if (!IS_RESTARTED || result == ES_VALUE)
		{
			ES_Value arguments[2];
			DOMSetObject(&arguments[0], new_node);
			DOMSetObject(&arguments[1], extra_node);

			result = insertBefore(this_node, arguments, 2, return_value, origining_runtime);
		}
		else
			result = insertBefore(NULL, NULL, -1, return_value, origining_runtime);

		if (result == (ES_SUSPEND | ES_RESTART))
			goto suspend;

		if (result != ES_VALUE)
			return result;

		DOMSetObject(return_value, old_node);
		return result;
	}

#undef IS_POINT
#undef SET_POINT
#undef IS_RESTARTED

suspend:
	if (!state)
		CALL_FAILED_IF_ERROR(DOMSetObjectRuntime(state = OP_NEW(DOM_Mutation_State, (point, this_node, new_node, old_node, extra_node)), this_node->GetRuntime()));
	state->return_value = *return_value;
	DOMSetObject(return_value, state);
	return ES_SUSPEND | ES_RESTART;
}

/* static */ int
DOM_Node::appendChild(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	if (argc >= 0)
	{
		DOM_CHECK_ARGUMENTS("o");

		ES_Value arguments[2];
		arguments[0] = argv[0];
		arguments[1].type = VALUE_NULL;

		return insertBefore(this_object, arguments, 2, return_value, origining_runtime);
	}
	else
		return insertBefore(NULL, NULL, -1, return_value, origining_runtime);
}

/* static */ int
DOM_Node::removeChild(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
#ifdef DOM2_MUTATION_EVENTS
	DOM_Mutation_State *state = NULL;
#endif // DOM2_MUTATION_EVENTS

	DOM_Node* this_node;
	DOM_Node* old_node;

#ifdef DOM2_MUTATION_EVENTS
	if (argc >= 0)
#endif // DOM2_MUTATION_EVENTS
	{
		DOM_THIS_OBJECT_EXISTING(this_node, DOM_TYPE_NODE, DOM_Node);
		DOM_CHECK_ARGUMENTS("o");
		DOM_ARGUMENT_OBJECT_EXISTING(old_node, 0, DOM_TYPE_NODE, DOM_Node);

		if (this_node->GetEnvironment() != old_node->GetEnvironment())
			return DOM_CALL_DOMEXCEPTION(NOT_FOUND_ERR);

		if (this_node->IsReadOnly())
			return DOM_CALL_DOMEXCEPTION(NO_MODIFICATION_ALLOWED_ERR);

		/* Removing the root element from a displayed document would
		   require special handling (I think). */
		//if (this_node->GetEnvironment()->GetDocument() == this_node)
		//	return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);
	}
#ifdef DOM2_MUTATION_EVENTS
	else
	{
		state = DOM_VALUE2OBJECT(*return_value, DOM_Mutation_State);
		this_object = this_node = state->this_node;
		old_node = state->old_node;
	}
#endif // DOM2_MUTATION_EVENTS

	DOM_Node *parentNode;
	CALL_FAILED_IF_ERROR(old_node->GetParentNode(parentNode));
	if (parentNode != this_node)
		return DOM_CALL_DOMEXCEPTION(NOT_FOUND_ERR);

#ifdef DOM2_MUTATION_EVENTS
	if (!state)
	{
		ES_Thread *thread = GetCurrentThread(origining_runtime);

		CALL_FAILED_IF_ERROR(old_node->SendNodeRemoved(thread));

		if (thread && thread->IsBlocked())
		{
			CALL_FAILED_IF_ERROR(DOMSetObjectRuntime(state = OP_NEW(DOM_Mutation_State, (DOM_Mutation_State::REMOVE, this_node, NULL, old_node, NULL)), this_node->GetRuntime()));
			DOMSetObject(return_value, state);
			return ES_SUSPEND | ES_RESTART;
		}
	}
#endif // DOM2_MUTATION_EVENTS

	CALL_FAILED_IF_ERROR(old_node->RemoveFromParent(origining_runtime));

	DOMSetObject(return_value, old_node);
	return ES_VALUE;
}

/* static */ int
DOM_Node::hasChildNodes(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(node, DOM_TYPE_NODE, DOM_Node);

	HTML_Element *placeholder = node->GetPlaceholderElement();

	DOMSetBoolean(return_value, placeholder && placeholder->FirstChildActual());
	return ES_VALUE;
}

/* static */ int
DOM_Node::cloneNode(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(this_node, DOM_TYPE_NODE, DOM_Node);
	DOM_CHECK_ARGUMENTS("|b");

	BOOL deep = argc == 0 || argv[0].value.boolean;

	DOM_EnvironmentImpl *environment = this_node->GetEnvironment();

	if (this_node->IsA(DOM_TYPE_ELEMENT) || this_node->IsA(DOM_TYPE_CHARACTERDATA) || this_node->IsA(DOM_TYPE_PROCESSINGINSTRUCTION) || this_node->IsA(DOM_TYPE_DOCUMENTTYPE))
	{
		HTML_Element *this_elm = this_node->GetThisElement();
		HTML_Element *clone_elm;
		DOM_Node *clone_node;

		CALL_FAILED_IF_ERROR(HTML_Element::DOMCloneElement(clone_elm, this_node->GetEnvironment(), this_elm, deep));
		CALL_FAILED_IF_ERROR(environment->ConstructNode(clone_node, clone_elm, this_node->owner_document));

		DOMSetObject(return_value, clone_node);
		return ES_VALUE;
	}
	else if (this_node->IsA(DOM_TYPE_DOCUMENTFRAGMENT))
	{
		HTML_Element *this_elm = ((DOM_DocumentFragment *) this_node)->GetPlaceholderElement();
		DOM_DocumentFragment *clone_frag;

		CALL_FAILED_IF_ERROR(DOM_DocumentFragment::Make(clone_frag, this_node));

		if (deep)
		{
			HTML_Element *clone_elm;
			CALL_FAILED_IF_ERROR(HTML_Element::DOMCloneElement(clone_elm, this_node->GetEnvironment(), this_elm, deep));
			clone_frag->SetPlaceholderElement(clone_elm);
		}

		DOMSetObject(return_value, clone_frag);
		return ES_VALUE;
	}
	else if (this_node->GetNodeType() == ATTRIBUTE_NODE)
	{
		DOM_Attr *clone_attr;

		CALL_FAILED_IF_ERROR(((DOM_Attr *) this_node)->Clone(clone_attr));

		DOMSetObject(return_value, clone_attr);
		return ES_VALUE;
	}
	else if (this_node->GetNodeType() == DOCUMENT_NODE)
	{
		DOM_Document *this_doc = static_cast<DOM_Document *>(this_node);

		// First create empty stub document
		DOM_Document *clone_doc;

		// To keep things simple: if the cloned document is totally empty,
		// pretend we're supposed to do a shallow clone.  Result would be the
		// same either way, and the deep cloning code path needs a placeholder
		// element.
		if (!this_doc->GetPlaceholderElement())
			deep = FALSE;

		if (this_node->IsA(DOM_TYPE_HTML_DOCUMENT))
		{
			DOM_HTMLDocument *htmldocument;
			CALL_FAILED_IF_ERROR(DOM_HTMLDocument::Make(htmldocument, this_doc->GetDOMImplementation(), !deep, FALSE));
			clone_doc = htmldocument;
		}
		else
		{
			if (this_node->IsA(DOM_TYPE_XML_DOCUMENT))
				CALL_FAILED_IF_ERROR(DOM_XMLDocument::Make(clone_doc, this_doc->GetDOMImplementation(), !deep));
			else
			{
				BOOL is_xml = this_doc->IsXML();
				CALL_FAILED_IF_ERROR(DOM_Document::Make(clone_doc, this_doc->GetDOMImplementation(), !deep, is_xml));
			}
		}
		clone_doc->SetIsSignificant();
		OP_ASSERT(clone_doc->GetRootElement() == NULL);

		// Second, clone the tree.
		if (deep)
		{
			HTML_Element *clone_elm;
			CALL_FAILED_IF_ERROR(HTML_Element::DOMCloneElement(clone_elm, environment, this_doc->GetPlaceholderElement(), deep));
			clone_doc->ResetPlaceholderElement(clone_elm);

			// Find a proper root
			OP_ASSERT(clone_doc->GetRootElement() == NULL);
			HTML_Element *root_candidate = clone_elm->FirstChildActual();
			DOM_Node *root_cand_elm;
			while (root_candidate && !Markup::IsRealElement(root_candidate->Type()))
				root_candidate = root_candidate->NextSiblingActual();
			if (root_candidate)
			{
				CALL_FAILED_IF_ERROR(environment->ConstructNode(root_cand_elm, root_candidate, clone_doc));
				OP_ASSERT(root_cand_elm->IsA(DOM_TYPE_ELEMENT));
				CALL_FAILED_IF_ERROR(clone_doc->SetRootElement(static_cast<DOM_Element *>(root_cand_elm)));
			}

			// And deal with the doctype
			if (this_node->GetHLDocProfile())
			{
				// On this document, doctype info is fetched from the HlDocprofile, which
				// does not exist in the cloned document, so the doctype needs to be filled
				DOM_Node *this_dt;
				CALL_FAILED_IF_ERROR(this_doc->GetDocumentType(this_dt));
				if (this_dt)
				{
					OP_ASSERT(this_dt->IsA(DOM_TYPE_DOCUMENTTYPE));
					DOM_Node *clone_dt = NULL;
					CALL_FAILED_IF_ERROR(clone_doc->GetDocumentType(clone_dt));
					OP_ASSERT(clone_dt);
					OP_ASSERT(clone_dt->IsA(DOM_TYPE_DOCUMENTTYPE));
					static_cast<DOM_DocumentType *>(clone_dt)->CopyFrom(static_cast<DOM_DocumentType *>(this_dt));
					clone_dt->SetIsSignificant();
				}
			}
		}

		DOMSetObject(return_value, clone_doc);
		return ES_VALUE;
	}
	else
		return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);
}

#ifdef DOM2_MUTATION_EVENTS

class DOM_NormalizeState
	: public DOM_Object
{
public:
	enum Point { SEARCH, REMOVE_EMPTY, REMOVE_EMPTY_RESTARTED, REMOVE_SIBLINGS, REMOVE_SIBLINGS_RESTARTED } point;

	DOM_Node *root, *current;
	ES_Value return_value;

	DOM_NormalizeState(DOM_Node *root)
		: root(root), current(NULL)
	{
	}

	virtual void GCTrace()
	{
		GCMark(root);
		GCMark(current);
		GCMark(return_value);
	}
};

#endif // DOM2_MUTATION_EVENTS

/* static */ int
DOM_Node::normalize(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
#ifdef DOM2_MUTATION_EVENTS
	DOM_NormalizeState *state = NULL;
	DOM_NormalizeState::Point point = DOM_NormalizeState::SEARCH;

	DOM_Node *root, *current;

	if (argc >= 0)
	{
		DOM_THIS_OBJECT_EXISTING(root, DOM_TYPE_NODE, DOM_Node);

		if (root->GetNodeType() == DOCUMENT_NODE || root->GetNodeType() == DOCUMENT_FRAGMENT_NODE)
		{
			CALL_FAILED_IF_ERROR(root->GetFirstChild(current));

			if (!current)
				return ES_FAILED;
		}
		else if (root->GetNodeType() == ELEMENT_NODE)
			current = root;
		else
			return ES_FAILED;
	}
	else
	{
		state = DOM_VALUE2OBJECT(*return_value, DOM_NormalizeState);
		point = state->point;

		this_object = root = state->root;
		current = state->current;
		*return_value = state->return_value;

		if (current)
		{
			if (!root->IsAncestorOf(current))
				if (root->GetNodeType() == DOCUMENT_NODE || root->GetNodeType() == DOCUMENT_FRAGMENT_NODE)
				{
					CALL_FAILED_IF_ERROR(root->GetFirstChild(current));

					if (!current)
						return ES_FAILED;
				}
				else
					current = root;
		}
	}

	DOM_EnvironmentImpl *environment = root->GetEnvironment();
	ES_Thread *thread = GetCurrentThread(origining_runtime);

	HTML_Element *element = current ? current->GetThisElement() : NULL, *stop;

	if (root->GetNodeType() == ELEMENT_NODE)
		stop = ((DOM_Element *) root)->GetThisElement()->NextSiblingActual();
	else
		stop = NULL;

again:
	while (point == DOM_NormalizeState::SEARCH)
	{
		if (element == stop || !element)
			return ES_FAILED;

		if (element->IsText() && !element->GetIsCDATA())
		{
			if (element->Type() == HE_TEXT && (!element->Content() || !*element->Content()))
			{
				CALL_FAILED_IF_ERROR(environment->ConstructNode(current, element, root->GetOwnerDocument()));
				point = DOM_NormalizeState::REMOVE_EMPTY;
			}
			else
			{
				TempBuffer buffer;

				HTML_Element *next_sibling = element->SucActual();
				BOOL found = FALSE;

				while (next_sibling && next_sibling->IsText() && !next_sibling->GetIsCDATA())
				{
					CALL_FAILED_IF_ERROR(next_sibling->DOMGetContents(environment, &buffer));
					next_sibling = next_sibling->SucActual();
					found = TRUE;
				}

				if (found)
				{
					CALL_FAILED_IF_ERROR(environment->ConstructNode(current, element, root->GetOwnerDocument()));
					point = DOM_NormalizeState::REMOVE_SIBLINGS;

					if (buffer.Length() > 0)
					{
						ES_Value arguments[1];
						DOMSetString(&arguments[0], buffer.GetStorage());

						int result = DOM_CharacterData::appendData(current, arguments, 1, return_value, origining_runtime);
						if (result != ES_FAILED)
							return result;

						if (thread->IsBlocked())
							goto suspend;
					}
				}
			}

			element = element->NextSiblingActual();
		}
		else
			element = element->NextActual();
	}

	while (point != DOM_NormalizeState::SEARCH)
	{
		if (point == DOM_NormalizeState::REMOVE_EMPTY || point == DOM_NormalizeState::REMOVE_SIBLINGS)
		{
			HTML_Element *remove;

			if (point == DOM_NormalizeState::REMOVE_EMPTY)
				remove = current->GetThisElement();
			else
			{
				remove = current->GetThisElement()->SucActual();

				if (!remove || !remove->IsText() || remove->GetIsCDATA())
				{
					if (remove)
						element = remove;
					else if (current->GetThisElement()->IsText())
						element = current->GetThisElement()->NextSiblingActual();
					else
						element = current->GetThisElement()->NextActual();

					point = DOM_NormalizeState::SEARCH;
					break;
				}
			}

			DOM_Node *parent;
			CALL_FAILED_IF_ERROR(environment->ConstructNode(parent, remove->ParentActual(), root->GetOwnerDocument()));

			DOM_Node *sibling;
			CALL_FAILED_IF_ERROR(environment->ConstructNode(sibling, remove, root->GetOwnerDocument()));

			ES_Value arguments[1];
			DOMSetObject(&arguments[0], sibling);

			int result = removeChild(parent, arguments, 1, return_value, origining_runtime);

			if (result == (ES_SUSPEND | ES_RESTART))
			{
				if (point == DOM_NormalizeState::REMOVE_EMPTY)
				{
					CALL_FAILED_IF_ERROR(current->GetNextNode(current));
					point = DOM_NormalizeState::REMOVE_EMPTY_RESTARTED;
				}
				else
					point = DOM_NormalizeState::REMOVE_SIBLINGS_RESTARTED;

				goto suspend;
			}
			else if (result != ES_VALUE)
				return result;
		}
		else
		{
			int result = removeChild(NULL, NULL, -1, return_value, origining_runtime);

			if (result != ES_VALUE)
				return result;

			if (point == DOM_NormalizeState::REMOVE_SIBLINGS_RESTARTED)
				point = DOM_NormalizeState::REMOVE_SIBLINGS;
		}

		if (point == DOM_NormalizeState::REMOVE_EMPTY || point == DOM_NormalizeState::REMOVE_EMPTY_RESTARTED)
			point = DOM_NormalizeState::SEARCH;
	}

	goto again;

suspend:
	if (!state)
		CALL_FAILED_IF_ERROR(DOMSetObjectRuntime(state = OP_NEW(DOM_NormalizeState, (root)), root->GetRuntime()));

	state->point = point;
	state->current = current;
	state->return_value = *return_value;

	DOMSetObject(return_value, state);
	return ES_SUSPEND | ES_RESTART;
#else // DOM2_MUTATION_EVENTS
	DOM_THIS_OBJECT(this_node, DOM_TYPE_NODE, DOM_Node);

	TempBuffer *buffer = this_node->GetTempBuf();
	HTML_Element *element;

	if (this_node->GetNodeType() == ELEMENT_NODE)
		element = ((DOM_Element *) this_node)->GetThisElement();
	else if (this_node->GetNodeType() == DOCUMENT_FRAGMENT_NODE)
		element = ((DOM_DocumentFragment *) this_node)->GetPlaceholderElement();
	else if (this_node->GetNodeType() == DOCUMENT_NODE && ((DOM_Document *) this_node)->GetRootElement())
		element = ((DOM_Document *) this_node)->GetRootElement()->GetThisElement();
	else
		return ES_FAILED;

	HTML_Element *stop = element->NextSiblingActual();

	while (element != stop)
	{
		HTML_Element *next = element->IsText() ? element->NextSiblingActual() : element->NextActual();

		if (element->IsText() && !element->GetIsCDATA())
			if (element->Type() == HE_TEXT && (!element->Content() || !*element->Content()))
				if (DOM_Node *node = (DOM_Node *) element->GetESElement())
					node->RemoveFromParent(origining_runtime);
				else
					element->DOMRemoveFromParent(this_node->GetEnvironment());
			else
			{
				buffer->Clear();

				HTML_Element *current = element, *next_sibling = element->SucActual();
				while (next_sibling && next_sibling->IsText() && !next_sibling->GetIsCDATA())
				{
					CALL_FAILED_IF_ERROR(current->DOMGetContents(this_node->GetEnvironment(), buffer));
					current = next_sibling;
					next_sibling = current->SucActual();
				}

				if (current != element)
				{
					next = current->NextSiblingActual();

					CALL_FAILED_IF_ERROR(current->DOMGetContents(this_node->GetEnvironment(), buffer));
					CALL_FAILED_IF_ERROR(element->SetText(buffer->GetStorage(), buffer->Length()));

					HTML_Element *remove = element->SucActual();
					while (remove != next_sibling)
					{
						HTML_Element *next = remove->SucActual();

						if (DOM_Node *node = (DOM_Node *) remove->GetESElement())
							node->RemoveFromParent(origining_runtime);
						else
							remove->DOMRemoveFromParent(this_node->GetEnvironment());

						remove = next;
					}
				}
			}

		element = next;
	}

	return ES_FAILED;
#endif // DOM2_MUTATION_EVENTS
}

/* static */ int
DOM_Node::isSupported(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_NODE);
	return DOM_DOMImplementation::accessFeature(NULL, argv, argc, return_value, origining_runtime, 0);
}

/* static */ int
DOM_Node::hasAttributes(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(this_node, DOM_TYPE_NODE, DOM_Node);

	BOOL has_attributes;

	if (this_node->GetNodeType() == ELEMENT_NODE)
		has_attributes = this_node->GetThisElement()->DOMGetAttributeCount() > 0;
	else
		has_attributes = FALSE;

	DOMSetBoolean(return_value, has_attributes);
	return ES_VALUE;
}

/* static */ int
DOM_Node::isSameNode(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(node, DOM_TYPE_NODE, DOM_Node);
	DOM_CHECK_ARGUMENTS("o");
	DOM_ARGUMENT_OBJECT(other, 0, DOM_TYPE_NODE, DOM_Node);

	DOMSetBoolean(return_value, node == other);
	return ES_VALUE;
}

/* static */ int
DOM_Node::isEqualNode(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(node, DOM_TYPE_NODE, DOM_Node);
	DOM_CHECK_ARGUMENTS("O");
	DOM_ARGUMENT_OBJECT(other, 0, DOM_TYPE_NODE, DOM_Node);

	if (other && !other->OriginCheck(origining_runtime))
		return ES_EXCEPT_SECURITY;

	BOOL are_equal;
	if (!other || other == node)
		// Catches null argument or self.
		are_equal = other == node;
	else
	{
		// Document nodes don't have a this_element, so need to retrieve the placeholder instead.
		HTML_Element *left = node->GetThisElement();
		if (!left)
			left = node->GetPlaceholderElement();
		HTML_Element *right = other->GetThisElement();
		if (!right)
			right = other->GetPlaceholderElement();

		if (left && right)
		{
			BOOL case_sensitive = (node->GetHLDocProfile() && node->GetHLDocProfile()->IsXml()) ||
					(other->GetHLDocProfile() && other->GetHLDocProfile()->IsXml()) ||
					(node->GetOwnerDocument() && node->GetOwnerDocument()->IsXML()) ||
					(other->GetOwnerDocument() && other->GetOwnerDocument()->IsXML());

			OP_BOOLEAN status = HTML_Element::DOMAreEqualNodes(left, node->GetHLDocProfile(), right, other->GetHLDocProfile(), case_sensitive);
			if (status == OpStatus::ERR_NO_MEMORY)
				return ES_NO_MEMORY;
			are_equal = status == OpBoolean::IS_TRUE;
		}
		else
		{
			OP_ASSERT(!"Can this ever happen?");
			are_equal = FALSE;
		}
	}
	DOMSetBoolean(return_value, are_equal);
	return ES_VALUE;
}

/* static */ int
DOM_Node::dispatchEvent(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_OBJECT);

	if (argc >= 0)
	{
		DOM_CHECK_ARGUMENTS("o");
		DOM_ARGUMENT_OBJECT(event, 0, DOM_TYPE_EVENT, DOM_Event);

		if (!this_object->OriginCheck(origining_runtime))
			return ES_EXCEPT_SECURITY;

		if (event->GetKnownType() == DOM_EVENT_NONE)
			return DOM_CALL_EVENTEXCEPTION(UNSPECIFIED_EVENT_TYPE_ERR);

		if (event->GetThread())
			return DOM_CALL_EVENTEXCEPTION(DISPATCH_REQUEST_ERR);

#ifdef DRAG_SUPPORT
		DOM_EventType type = event->GetKnownType();
		if (event->IsA(DOM_TYPE_DRAGEVENT) && (type == ONDRAGENTER || type == ONDRAGOVER || type == ONDRAGLEAVE || type == ONDROP))
		{
			DOM_DragEvent *drag_event = static_cast<DOM_DragEvent*>(event);
			OpDragObject *data_store = drag_event->GetDataStore();
			if (data_store)
			{
				URL origin_url = origining_runtime->GetOriginURL();
				// Do not allow to send the synthetic events to the disallowed target origin.
				if (!DragDropManager::IsURLAllowed(data_store, origin_url))
					return ES_FAILED;
			}
		}
#endif // DRAG_SUPPORT

		DOM_EnvironmentImpl *environment = this_object->GetEnvironment();

#ifdef USER_JAVASCRIPT
		if (this_object->IsA(DOM_TYPE_OPERA))
		{
			DOM_UserJSManager *user_js_manager = origining_runtime->GetEnvironment()->GetUserJSManager();
			if (!user_js_manager || !user_js_manager->GetIsActive(origining_runtime))
				return ES_FAILED;
		}
#endif // USER_JAVASCRIPT

		CALL_FAILED_IF_ERROR(this_object->CreateEventTarget());

		// Checkboxes are checked before the click event is dispatched.
		if (event->GetKnownType() == ONCLICK)
			CALL_FAILED_IF_ERROR(DOM_HTMLElement::BeforeClick(this_object));

		ES_Thread *interrupt_thread = GetCurrentThread(origining_runtime);

		event->SetTarget(this_object);
		event->SetSynthetic();

		OP_BOOLEAN result = environment->SendEvent(event, interrupt_thread);

		if (OpStatus::IsMemoryError(result))
			return ES_NO_MEMORY;
		else if (result == OpBoolean::IS_TRUE)
		{
			DOMSetObject(return_value, event);
			return ES_SUSPEND | ES_RESTART;
		}
		else
		{
			event->SetTarget(NULL);
			return ES_FAILED;
		}
	}
	else
	{
		DOM_Event *event = DOM_VALUE2OBJECT(*return_value, DOM_Event);

		DOMSetBoolean(return_value, !event->GetPreventDefault());
		return ES_VALUE;
	}
}

/* static */ int
DOM_Node::getFeature(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(node, DOM_TYPE_NODE, DOM_Node);

	int result = DOM_DOMImplementation::accessFeature(NULL, argv, argc, return_value, origining_runtime, 0);

	if (result == ES_VALUE && return_value->value.boolean)
		DOMSetObject(return_value, node);
	else
		DOMSetNull(return_value);

	return result;
}

#define DOCUMENT_POSITION_DISCONNECTED				0x1
#define DOCUMENT_POSITION_PRECEDING					0x2
#define DOCUMENT_POSITION_FOLLOWING					0x4
#define DOCUMENT_POSITION_CONTAINS					0x8
#define DOCUMENT_POSITION_CONTAINED_BY				0x10
#define DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC	0x20

/**
 * This method returns the parent node as needed by the compareDocumentNodePosition algorithm.
 */
static OP_STATUS GetNodeOrderParent(DOM_Node* node, DOM_Node*& out_root)
{
	OP_ASSERT(node);
	switch (node->GetNodeType())
	{
		case TEXT_NODE:
		case CDATA_SECTION_NODE:
		case COMMENT_NODE:
		case DOCUMENT_TYPE_NODE:
		case ELEMENT_NODE:
		case PROCESSING_INSTRUCTION_NODE:
		case ENTITY_REFERENCE_NODE:
			return node->GetParentNode(out_root);

		case ATTRIBUTE_NODE:
			out_root = static_cast<DOM_Attr *>(node)->GetOwnerElement();
			return OpStatus::OK;

		case NOTATION_NODE:
		case ENTITY_NODE:
			return node->GetOwnerDocument()->GetDocumentType(out_root);

		default:
			OP_ASSERT(!"Not implemented");
		case DOCUMENT_NODE:
		case DOCUMENT_FRAGMENT_NODE:
			break;
	}
	out_root = NULL;
	return OpStatus::OK;
}

/**
 * Returns the root and the depth of the node tree, counting only
 * the nodes from the root to the node itself.
 *
 * A root node (or lonely node) is it's own root and has depth = 1
 */
static OP_STATUS GetNodeOrderRoot(DOM_Node* node, DOM_Node*& out_root, int& out_depth)
{
	OP_ASSERT(node);
	DOM_Node* next = node;
	out_depth = 0;
	do
	{
		OP_ASSERT(next);
		node = next;
		out_depth++;
		RETURN_IF_ERROR(GetNodeOrderParent(node, next));
	}
	while (next);

	OP_ASSERT(node);
	out_root = node;
	return OpStatus::OK;
}

static int GetNodeOrderStringCompare(const uni_char* ref_string, const uni_char* other_node_string)
{
	if (ref_string)
	{
		// Let the namespace ones come after the namespace less ones
		if (!other_node_string)
			return -1;

		return uni_strcmp(ref_string, other_node_string);
	}
	else if (other_node_string)
		return 1;

	// Same, same
	return 0;
}

/* static */ int
DOM_Node::compareDocumentPosition(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(ref_node, DOM_TYPE_NODE, DOM_Node);
	DOM_CHECK_ARGUMENTS("o");
	DOM_ARGUMENT_OBJECT(other_node, 0, DOM_TYPE_NODE, DOM_Node);

	// Same object?
	// "If the two nodes being compared are the same node, then no flags are set on the return."
	if (other_node == ref_node)
	{
		DOMSetNumber(return_value, 0);
		return ES_VALUE;
	}

	// Same "tree"? Check by comparing roots

	DOM_Node* other_node_root;
	int other_node_depth;
	CALL_FAILED_IF_ERROR(GetNodeOrderRoot(other_node, other_node_root, other_node_depth));
	DOM_Node* ref_node_root;
	int ref_node_depth;
	CALL_FAILED_IF_ERROR(GetNodeOrderRoot(ref_node, ref_node_root, ref_node_depth));

	if (other_node_root != ref_node_root)
	{
		// Different "subtrees". Compare pointers
		int follow_flag = other_node_root < ref_node_root ? DOCUMENT_POSITION_FOLLOWING : DOCUMENT_POSITION_PRECEDING;
		DOMSetNumber(return_value, follow_flag |
								DOCUMENT_POSITION_DISCONNECTED |
								DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC);
		return ES_VALUE;
	}

	// Find the common parent
	int min_depth = MIN(other_node_depth, ref_node_depth);
	int depth = other_node_depth;
	DOM_Node* other_node_min_depth_parent = other_node;
	while (depth > min_depth)
	{
		CALL_FAILED_IF_ERROR(GetNodeOrderParent(other_node_min_depth_parent, other_node_min_depth_parent));
		depth--;
	}
	depth = ref_node_depth;
	DOM_Node* ref_node_min_depth_parent = ref_node;
	while (depth > min_depth)
	{
		CALL_FAILED_IF_ERROR(GetNodeOrderParent(ref_node_min_depth_parent, ref_node_min_depth_parent));
		depth--;
	}

	// Two nodes at the same depth
	DOM_Node* ref_node_parent = ref_node_min_depth_parent;
	DOM_Node* ref_node_common_parent_child = ref_node_parent;
	DOM_Node* other_node_parent = other_node_min_depth_parent;
	DOM_Node* other_node_common_parent_child = other_node_min_depth_parent;
	while (ref_node_parent != other_node_parent)
	{
		ref_node_common_parent_child = ref_node_parent;
		other_node_common_parent_child = other_node_parent;
		CALL_FAILED_IF_ERROR(GetNodeOrderParent(other_node_parent, other_node_parent));
		CALL_FAILED_IF_ERROR(GetNodeOrderParent(ref_node_parent, ref_node_parent));
		OP_ASSERT(other_node_parent);
		OP_ASSERT(ref_node_parent);
	}

	DOM_Node* common_parent = other_node_parent;
	OP_ASSERT(common_parent);

	// Check if one contains the other
	if (other_node == common_parent ||
		ref_node == common_parent)
	{
		// Contained
		// The container precedes the contained element
		int following_flag = other_node == common_parent ? DOCUMENT_POSITION_PRECEDING : DOCUMENT_POSITION_FOLLOWING;
		int contained_flag = other_node == common_parent ? DOCUMENT_POSITION_CONTAINS : DOCUMENT_POSITION_CONTAINED_BY;
		DOMSetNumber(return_value, following_flag | contained_flag);
		return ES_VALUE;
	}

	// If we get here, then we have two nodes in the same tree, but in different parts of the tree.
	// Compare the children to the common parent
	HTML_Element* ref_node_elm_common_parent_child = ref_node_common_parent_child->GetThisElement();
	HTML_Element* elm_common_parent_child = other_node_common_parent_child->GetThisElement();
	if (ref_node_elm_common_parent_child && elm_common_parent_child &&
		elm_common_parent_child->ParentActual() == ref_node_elm_common_parent_child->ParentActual())
	{
		OP_ASSERT(ref_node_elm_common_parent_child != elm_common_parent_child);
		// See if elm_common_parent_child comes after ref_node_elm_common_parent_child
		HTML_Element* suc = ref_node_elm_common_parent_child;
		while (suc && suc != elm_common_parent_child)
		{
			suc = suc->SucActual();
		}
		if (suc)
		{
			// Found node after ref_node;
			DOMSetNumber(return_value, DOCUMENT_POSITION_FOLLOWING);
		}
		else
		{
			// Didn't find node after ref_node so it must be before
			DOMSetNumber(return_value, DOCUMENT_POSITION_PRECEDING);
		}
		return ES_VALUE;
	}

	// So one of the nodes isn't a child. A non-child comes before any child
	if (ref_node_elm_common_parent_child || elm_common_parent_child)
	{
		if (!ref_node_elm_common_parent_child)
		{
			// ref_node isn't a child, so node must be a child and thus following ref_node
			DOMSetNumber(return_value, DOCUMENT_POSITION_FOLLOWING);
		}
		else
		{
			// Or the other way around
			DOMSetNumber(return_value, DOCUMENT_POSITION_PRECEDING);
		}
		return ES_VALUE;
	}

	// if both are non-children, then we compare nodeType

	if (ref_node_common_parent_child->GetNodeType() != other_node_common_parent_child->GetNodeType())
	{
		// Different types
		// "one determining node has a greater value of nodeType than the other,
		// then the corresponding node precedes the other"
		// So sorted in decreasing order
		if (ref_node_common_parent_child->GetNodeType() > other_node_common_parent_child->GetNodeType())
		{
			DOMSetNumber(return_value, DOCUMENT_POSITION_FOLLOWING);
		}
		else
		{
			// Or the other way around
			DOMSetNumber(return_value, DOCUMENT_POSITION_PRECEDING);
		}
		return ES_VALUE;
	}

	// Same nodeType but not children
	// "then an implementation-dependent order between the
	// determining nodes is returned. This order is stable"

	if (ref_node_common_parent_child->GetNodeType() == ATTRIBUTE_NODE)
	{
		// Let's sort them by name
		DOM_Attr* ref_attr = static_cast<DOM_Attr*>(ref_node_common_parent_child);
		DOM_Attr* other_attr = static_cast<DOM_Attr*>(other_node_common_parent_child);

		int order = GetNodeOrderStringCompare(ref_attr->GetNsUri(), other_attr->GetNsUri());
		if (order == 0) // Same namespace, compare names
			order = GetNodeOrderStringCompare(ref_attr->GetName(), other_attr->GetName());

		OP_ASSERT(order != 0); // Or we have the same node which should have been caught earlier

		if (order < 0)
			DOMSetNumber(return_value, DOCUMENT_POSITION_PRECEDING | DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC);
		else
			DOMSetNumber(return_value, DOCUMENT_POSITION_FOLLOWING | DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC);
		return ES_VALUE;
	}

	// Not sure it's stable to compare DOM_Node pointers, but some fun must be left to the debugger (let it not be me!)
	DOMSetNumber(return_value, (other_node < ref_node ? DOCUMENT_POSITION_FOLLOWING : DOCUMENT_POSITION_PRECEDING) |
							DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC);
	return ES_VALUE;
//	return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);
}

int
DOM_Node::contains(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	// http://dvcs.w3.org/hg/domcore/raw-file/tip/Overview.html#dom-node-contains
	DOM_THIS_OBJECT(node, DOM_TYPE_NODE, DOM_Node);
	DOM_CHECK_ARGUMENTS("O");

	if (argv[0].type == VALUE_NULL || argv[0].type == VALUE_UNDEFINED)
		DOMSetBoolean(return_value, FALSE);
	else
	{
		DOM_ARGUMENT_OBJECT(child, 0, DOM_TYPE_NODE, DOM_Node);
		DOMSetBoolean(return_value, node->IsAncestorOf(child));
	}

	return ES_VALUE;
}

/* static */ int
DOM_Node::accessEventListener(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_OBJECT);

	if (!this_object->OriginCheck(origining_runtime))
		return ES_EXCEPT_SECURITY;

	DOM_EnvironmentImpl *environment = this_object->GetEnvironment();

	DOM_CHECK_ARGUMENTS("sO|b");

	if (argv[1].type != VALUE_OBJECT)
		return ES_FAILED;

	const uni_char *type = argv[0].value.string;
	ES_Object *native_listener = argv[1].value.object;
	BOOL useCapture = argc > 2 && argv[2].value.boolean;

#ifdef USER_JAVASCRIPT
	if (this_object->IsA(DOM_TYPE_OPERA))
	{
		DOM_UserJSManager *user_js_manager = origining_runtime->GetEnvironment()->GetUserJSManager();
		if (!user_js_manager || !user_js_manager->GetIsActive(origining_runtime))
			return ES_FAILED;
	}
#endif // USER_JAVASCRIPT

	DOM_EventType known_type = DOM_Event::GetEventType(type, FALSE);

	if (known_type == DOM_EVENT_NONE)
		known_type = DOM_EVENT_CUSTOM;

	CALL_FAILED_IF_ERROR(this_object->CreateEventTarget());

	if (data == 0 || data == 2)
	{
		DOM_EventListener *listener = OP_NEW(DOM_EventListener, ());

#ifdef ECMASCRIPT_DEBUGGER
		if (data == 0)
		{
			ES_Context *context = DOM_Object::GetCurrentThread(origining_runtime)->GetContext();
			if (origining_runtime->IsContextDebugged(context))
			{
				ES_Runtime::CallerInformation call_info;
				OP_STATUS status = ES_Runtime::GetCallerInformation(context, call_info);
				if (OpStatus::IsSuccess(status))
					listener->SetCallerInformation(call_info.script_guid, call_info.line_no);
			}
		}
#endif // ECMASCRIPT_DEBUGGER

		if (!listener || OpStatus::IsMemoryError(listener->SetNative(environment, known_type, type, useCapture, FALSE, NULL, native_listener)))
		{
			OP_DELETE(listener);
			return ES_NO_MEMORY;
		}
		DOM_EventTarget *event_target = this_object->GetEventTarget();
		DOM_EventTarget::NativeIterator it = event_target->NativeListeners();

		while (!it.AtEnd())
		{
			if (listener->CanOverride(const_cast<DOM_EventListener *>(it.Current())))
			{
				OP_DELETE(listener);
				return ES_FAILED;
			}
			it.Next();
		}
		event_target->AddListener(listener);
	}
	else
	{
		DOM_EventListener listener;

		CALL_FAILED_IF_ERROR(listener.SetNative(environment, known_type, type, useCapture, FALSE, NULL, native_listener));

		this_object->GetEventTarget()->RemoveListener(&listener);
	}

	return ES_FAILED;
}

/* static */ int
DOM_Node::attachOrDetachEvent(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_CHECK_ARGUMENTS("so");

	ES_Value new_argv[3];

	new_argv[0] = argv[0];
	new_argv[1] = argv[1];
	DOMSetBoolean(&new_argv[2], FALSE);

	if (new_argv[0].type == VALUE_STRING && uni_strni_eq(new_argv[0].value.string, "ON", 2))
		new_argv[0].value.string += 2;

	int result = DOM_Node::accessEventListener(this_object, new_argv, 3, return_value, origining_runtime, data);

	if (data == 0 && result == ES_FAILED)
	{
		// attachEvent always returns true.
		DOMSetBoolean(return_value, TRUE);
		return ES_VALUE;
	}
	else
		return result;
}

/* static */ int
DOM_Node::lookupNamespace(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(node, DOM_TYPE_NODE, DOM_Node);
	DOM_CHECK_ARGUMENTS("S");

	const uni_char *argument = argv[0].type == VALUE_STRING ? argv[0].value.string : NULL;
	HTML_Element *element = NULL;

	switch (node->node_type)
	{
	case ATTRIBUTE_NODE:
		if (DOM_Element *owner_element = ((DOM_Attr *) node)->GetOwnerElement())
			element = owner_element->GetThisElement();
		break;

	case DOCUMENT_NODE:
		if (DOM_Element *document_element = ((DOM_Document *) node)->GetRootElement())
			element = document_element->GetThisElement();
		break;

	case ENTITY_NODE:
	case DOCUMENT_TYPE_NODE:
	case DOCUMENT_FRAGMENT_NODE:
	case NOTATION_NODE:
		break;

#ifdef DOM3_XPATH
	case XPATH_NAMESPACE_NODE:
		element = ((DOM_XPathNamespace *) node)->GetOwnerElement()->GetThisElement();
		break;
#endif // DOM3_XPATH

	default:
		element = node->GetThisElement();
	}

	// 0, lookupPrefix(namespaceURI) : string
	// 1, isDefaultNamespace(namespaceURI) : bool
	// 2, lookupNamespaceURI(prefix) : string

	if (!element)
		if (data == 1)
			DOMSetBoolean(return_value, FALSE);
		else
			DOMSetNull(return_value);
	else if (data == 0)
	{
		const uni_char *prefix = element->DOMLookupNamespacePrefix(node->GetEnvironment(), argument);

		if (prefix && *prefix)
			DOMSetString(return_value, prefix);
		else
			DOMSetNull(return_value);
	}
	else
	{
		const uni_char *uri = element->DOMLookupNamespaceURI(node->GetEnvironment(), data == 1 ? NULL : argument);

		if (data == 1)
			// Both null, or both equal.
			DOMSetBoolean(return_value, (!uri && (!argument || !*argument)) || (argument && uri && uni_str_eq(argument, uri)));
		else if (uri)
			DOMSetString(return_value, uri);
		else
			DOMSetNull(return_value);
	}

	return ES_VALUE;
}

#ifdef DOM3_XPATH

DOM_XPathResult_NodeList::DOM_XPathResult_NodeList(DOM_XPathResult *result)
	: result(result)
{
}

/* virtual */ ES_GetState
DOM_XPathResult_NodeList::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name == OP_ATOM_length)
	{
		DOMSetNumber(value, result->GetCount());
		return GET_SUCCESS;
	}
	else
		return GET_FAILED;
}

/* virtual */ ES_GetState
DOM_XPathResult_NodeList::GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_index >= 0 && (unsigned) property_index < result->GetCount())
	{
		DOMSetObject(value, result->GetNode(property_index));
		return GET_SUCCESS;
	}
	else
		return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_XPathResult_NodeList::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	return property_name == OP_ATOM_length ? PUT_SUCCESS : PUT_FAILED;
}

/* virtual */ ES_PutState
DOM_XPathResult_NodeList::PutIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime)
{
	return PUT_SUCCESS;
}

/* virtual */ void
DOM_XPathResult_NodeList::GCTrace()
{
	GCMark(result);
}

/* virtual */ ES_GetState
DOM_XPathResult_NodeList::GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime)
{
	count = result->GetCount();
	return GET_SUCCESS;
}

/* static */ int
DOM_XPathResult_NodeList::item(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(nodelist, DOM_TYPE_XPATHRESULT_NODELIST, DOM_XPathResult_NodeList);
	DOM_CHECK_ARGUMENTS("n");

	double index = argv[0].value.number;

	if (!op_isintegral(index) || index < 0. || index > (double) nodelist->result->GetCount())
		DOMSetNull(return_value);
	else
		DOMSetObject(return_value, nodelist->result->GetNode(op_double2uint32(index)));

	return ES_VALUE;
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_XPathResult_NodeList)
	DOM_FUNCTIONS_FUNCTION(DOM_XPathResult_NodeList, DOM_XPathResult_NodeList::item, "item", "n")
DOM_FUNCTIONS_END(DOM_Node)

/* static */ int
DOM_Node::select(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(node, DOM_TYPE_NODE, DOM_Node);

	DOM_Document *document;

	if (node->IsA(DOM_TYPE_DOCUMENT))
		document = (DOM_Document *) node;
	else
		document = node->GetOwnerDocument();

	if (argc >= 0)
	{
		ES_Object *nsresolver = NULL;

		if (argc == 1 || argc == 2 && (argv[1].type == VALUE_NULL || argv[1].type == VALUE_UNDEFINED))
		{
			DOM_CHECK_ARGUMENTS("s");

			DOM_XPathNSResolver *xpathnsresolver;
			DOM_Node *element;
			if (node->IsA(DOM_TYPE_DOCUMENT))
				element = ((DOM_Document *) node)->GetRootElement();
			else
				element = node;
			if (element)
			{
				CALL_FAILED_IF_ERROR(DOM_XPathNSResolver::Make(xpathnsresolver, element));
				nsresolver = *xpathnsresolver;
			}
		}
		else if (argc == 2)
		{
			DOM_CHECK_ARGUMENTS("so");
			nsresolver = argv[1].value.object;
		}
		else
			return DOM_CALL_INTERNALEXCEPTION(WRONG_ARGUMENTS_ERR);

		const uni_char *expression = argv[0].value.string;

		/* Defined in xpathresult.cpp. */
		extern double DOM_ExportXPathType(unsigned type);

		ES_Value evalute_argv[5];
		DOMSetString(&evalute_argv[0], expression);
		DOMSetObject(&evalute_argv[1], node);
		DOMSetObject(&evalute_argv[2], nsresolver);
		DOMSetNumber(&evalute_argv[3], DOM_ExportXPathType(XPathExpression::Evaluate::NODESET_FLAG_ORDERED | (data == 0 ? XPathExpression::Evaluate::NODESET_SNAPSHOT : XPathExpression::Evaluate::NODESET_SINGLE)));
		DOMSetNull(&evalute_argv[4]);

		int result = DOM_XPathEvaluator::createExpressionOrEvaluate(document, evalute_argv, 5, return_value, origining_runtime, 1);

		if (result != ES_VALUE)
			return result;
	}
	else
	{
		int result = DOM_XPathEvaluator::createExpressionOrEvaluate(document, NULL, -1, return_value, origining_runtime, 1);

		if (result != ES_VALUE)
			return result;
	}

	DOM_HOSTOBJECT_SAFE(result, return_value->value.object, DOM_TYPE_XPATHRESULT, DOM_XPathResult);

	if (!result)
		return ES_FAILED;
	else if (data == 0)
	{
		DOM_XPathResult_NodeList *nodelist;

		CALL_FAILED_IF_ERROR(DOMSetObjectRuntime(nodelist = OP_NEW(DOM_XPathResult_NodeList, (result)), origining_runtime, origining_runtime->GetPrototype(DOM_Runtime::XPATHRESULT_NODELIST_PROTOTYPE), "NodeList"));

		DOMSetObject(return_value, nodelist);
	}
	else if (result->GetCount() != 0)
		DOMSetObject(return_value, result->GetNode(0));
	else
		DOMSetNull(return_value);

	return ES_VALUE;
}

#endif // DOM3_XPATH

#ifdef USER_JAVASCRIPT
ES_GetState
DOM_Node::GetCssContents(DOM_Node *node, ES_Value *value, DOM_Runtime *origining_runtime, TempBuffer* buffer)
{
#ifdef _DEBUG
	// This function is only called from UserJSEvent.cssText, so the
	// user js manager is ensured to be active.
	DOM_UserJSManager *user_js_manager = origining_runtime->GetEnvironment()->GetUserJSManager();
	OP_ASSERT(user_js_manager);
	OP_ASSERT(user_js_manager->GetIsActive(origining_runtime));
#endif

	if (!value)
		return GET_SUCCESS;

	HTML_Element *he = node->GetThisElement();
	OP_ASSERT(he->IsLinkElement() || he->IsStyleElement());

	if (he->IsLinkElement())
		GET_FAILED_IF_ERROR(he->DOMGetDataSrcContents(node->GetEnvironment(), buffer));
	else
		GET_FAILED_IF_ERROR(he->DOMGetContents(node->GetEnvironment(), buffer));

	DOM_Object::DOMSetString(value, buffer);
	return GET_SUCCESS;
}

ES_PutState
DOM_Node::SetCssContents(DOM_Node *node, ES_Value *value, DOM_Runtime *origining_runtime)
{
#ifdef _DEBUG
	// This function is only called from UserJSEvent.cssText, so the
	// user js manager is ensured to be active.
	DOM_UserJSManager *user_js_manager = origining_runtime->GetEnvironment()->GetUserJSManager();
	OP_ASSERT(user_js_manager);
	OP_ASSERT(user_js_manager->GetIsActive(origining_runtime));
#endif

	HTML_Element *he = node->GetThisElement();
	OP_ASSERT(he->IsLinkElement() || he->IsStyleElement());

	if (value->type != VALUE_STRING)
		return PUT_NEEDS_STRING;

	if (he->Type() == HE_PROCINST)
	{
		DataSrc* src_head = he->GetDataSrc();
		if (src_head)
		{
			URL src_origin = src_head->GetOrigin();
			src_head->DeleteAll();
			PUT_FAILED_IF_ERROR(src_head->AddSrc(value->value.string, uni_strlen(value->value.string), src_origin));
		}
	}
	else if (he->IsStyleElement())
		PUT_FAILED_IF_ERROR(node->SetTextContent(value, origining_runtime, NULL));
	else // Link
	{
		DOM_EnvironmentImpl::CurrentState state(node->GetEnvironment(), origining_runtime);
		PUT_FAILED_IF_ERROR(he->DOMSetContents(node->GetEnvironment(), value->value.string));
	}
	return PUT_SUCCESS;
};
#endif // USER_JAVASCRIPT

/* static */ void
DOM_Node::ConstructNodeObjectL(ES_Object *object, DOM_Runtime *runtime)
{
	DOM_Object::PutNumericConstantL(object, "ELEMENT_NODE", ELEMENT_NODE, runtime);
	DOM_Object::PutNumericConstantL(object, "ATTRIBUTE_NODE", ATTRIBUTE_NODE, runtime);
	DOM_Object::PutNumericConstantL(object, "TEXT_NODE", TEXT_NODE, runtime);
	DOM_Object::PutNumericConstantL(object, "CDATA_SECTION_NODE", CDATA_SECTION_NODE, runtime);
	DOM_Object::PutNumericConstantL(object, "ENTITY_REFERENCE_NODE", ENTITY_REFERENCE_NODE, runtime);
	DOM_Object::PutNumericConstantL(object, "ENTITY_NODE", ENTITY_NODE, runtime);
	DOM_Object::PutNumericConstantL(object, "PROCESSING_INSTRUCTION_NODE", PROCESSING_INSTRUCTION_NODE, runtime);
	DOM_Object::PutNumericConstantL(object, "COMMENT_NODE", COMMENT_NODE, runtime);
	DOM_Object::PutNumericConstantL(object, "DOCUMENT_NODE", DOCUMENT_NODE, runtime);
	DOM_Object::PutNumericConstantL(object, "DOCUMENT_TYPE_NODE", DOCUMENT_TYPE_NODE, runtime);
	DOM_Object::PutNumericConstantL(object, "DOCUMENT_FRAGMENT_NODE", DOCUMENT_FRAGMENT_NODE, runtime);
	DOM_Object::PutNumericConstantL(object, "NOTATION_NODE", NOTATION_NODE, runtime);

	DOM_Object::PutNumericConstantL(object, "DOCUMENT_POSITION_DISCONNECTED", DOCUMENT_POSITION_DISCONNECTED, runtime);
	DOM_Object::PutNumericConstantL(object, "DOCUMENT_POSITION_PRECEDING", DOCUMENT_POSITION_PRECEDING, runtime);
	DOM_Object::PutNumericConstantL(object, "DOCUMENT_POSITION_FOLLOWING", DOCUMENT_POSITION_FOLLOWING, runtime);
	DOM_Object::PutNumericConstantL(object, "DOCUMENT_POSITION_CONTAINS", DOCUMENT_POSITION_CONTAINS, runtime);
	DOM_Object::PutNumericConstantL(object, "DOCUMENT_POSITION_CONTAINED_BY", DOCUMENT_POSITION_CONTAINED_BY, runtime);
	DOM_Object::PutNumericConstantL(object, "DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC", DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC, runtime);
	LEAVE_IF_ERROR(HideMSIEEventAPI(object, runtime));
}

/* static */ OP_STATUS
DOM_Node::HideMSIEEventAPI(ES_Object *object, DOM_Runtime *runtime)
{
	/* Hide these MSIEisms to throw off sniffing scripts; a step towards
	   retiring them completely. */
	ES_Value hidden_value;
	OP_BOOLEAN result;

	RETURN_IF_ERROR(result = runtime->GetName(object, UNI_L("attachEvent"), &hidden_value));
	if (result == OpBoolean::IS_TRUE && hidden_value.type == VALUE_OBJECT)
		ES_Runtime::MarkObjectAsHidden(hidden_value.value.object);

	RETURN_IF_ERROR(result = runtime->GetName(object, UNI_L("detachEvent"), &hidden_value));
	if (result == OpBoolean::IS_TRUE && hidden_value.type == VALUE_OBJECT)
		ES_Runtime::MarkObjectAsHidden(hidden_value.value.object);

	return OpStatus::OK;
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_Node)
	DOM_FUNCTIONS_FUNCTION(DOM_Node, DOM_Node::dispatchEvent, "dispatchEvent", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_Node, DOM_Node::insertBefore, "insertBefore", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_Node, DOM_Node::replaceChild, "replaceChild", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_Node, DOM_Node::removeChild, "removeChild", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_Node, DOM_Node::appendChild, "appendChild", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_Node, DOM_Node::hasChildNodes, "hasChildNodes", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_Node, DOM_Node::cloneNode, "cloneNode", "b-")
	DOM_FUNCTIONS_FUNCTIONX(DOM_Node, DOM_Node::normalize, "normalize", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_Node, DOM_Node::isSupported, "isSupported", "sS-")
	DOM_FUNCTIONS_FUNCTION(DOM_Node, DOM_Node::hasAttributes, "hasAttributes", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_Node, DOM_Node::isSameNode, "isSameNode", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_Node, DOM_Node::isEqualNode, "isEqualNode", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_Node, DOM_Node::getFeature, "getFeature", "s-")
	DOM_FUNCTIONS_FUNCTION(DOM_Node, DOM_Node::compareDocumentPosition, "compareDocumentPosition", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_Node, DOM_Node::contains, "contains", 0)
DOM_FUNCTIONS_END(DOM_Node)

DOM_FUNCTIONS_WITH_DATA_START(DOM_Node)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Node, DOM_Node::accessEventListener, 0, "addEventListener", "s-b-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Node, DOM_Node::accessEventListener, 1, "removeEventListener", "s-b-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Node, DOM_Node::attachOrDetachEvent, 0, "attachEvent", "s-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Node, DOM_Node::attachOrDetachEvent, 1, "detachEvent", "s-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Node, DOM_Node::lookupNamespace, 0, "lookupPrefix", "S-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Node, DOM_Node::lookupNamespace, 1, "isDefaultNamespace", "S-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Node, DOM_Node::lookupNamespace, 2, "lookupNamespaceURI", "S-")
#ifdef DOM3_XPATH
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Node, DOM_Node::select, 0, "selectNodes", "s-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Node, DOM_Node::select, 1, "selectSingleNode", "s-")
#endif // DOM3_XPATH
DOM_FUNCTIONS_WITH_DATA_END(DOM_Node)

DOM_LIBRARY_FUNCTIONS_START(DOM_Node)
	DOM_LIBRARY_FUNCTIONS_FUNCTION(DOM_Node, DOM_Node_normalize, "normalize")
DOM_LIBRARY_FUNCTIONS_END(DOM_Node)
