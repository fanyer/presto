/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Johannes Hoff
**
** Relevant documents:
**   Protocol: ../documentation/dom-inspector-protocol.txt
**
** Included in the scope service ecmascript-debugger 
*/

#include "core/pch.h"

#if defined(SCOPE_ECMASCRIPT_DEBUGGER) && defined(DOM_INSPECT_NODE_SUPPORT)

#include "modules/scope/src/scope_manager.h"
#include "modules/scope/src/scope_ecmascript_debugger.h"
#include "modules/scope/src/scope_dom_inspector.h"
#include "modules/protobuf/src/json_tools.h"
#include "modules/dom/domutils.h"
#include "modules/dom/src/domobj.h"
#include "modules/dom/src/domevents/domeventtarget.h"
#include "modules/dom/src/domevents/domeventlistener.h"
#include "modules/util/OpHashTable.h"

OP_STATUS GetViewWithChildren(ES_ScopeDebugFrontend *debugger, NodeListType nodes, DOM_Object** stack);
OP_STATUS GetAttributes(AttributeListType attributes_export, DOM_Object *node);

/**
 *  This class is just to keep all empty virtual functions in one place
 */
class InspectNodeCallbackEmpty : public DOM_Utils::InspectNodeCallback
{
public:
	// Virtual methods from InspectNodeCallback
	void SetType(DOM_Object *node, DOM_NodeType type) {}
	void SetName(DOM_Object *node, const uni_char *namespace_uri, const uni_char *prefix, const uni_char *localpart) {}
	void SetValue(DOM_Object *node, const uni_char *value) {}
	void SetDocumentTypeInformation(DOM_Object *node, const uni_char *name, const uni_char *public_id, const uni_char *system_id) {}
	void SetFrameInformation(DOM_Object *node, DOM_Object *content_document){}
	void SetDocumentInformation(DOM_Object *node, DOM_Object *parent_frame){}
	void AddAttribute(DOM_Object *node, DOM_Object *attribute) {}
	void AddChild(DOM_Object *node, DOM_Object *child) {}
	void SetParent(DOM_Object *node, DOM_Object *parent) {}
};

class DomGetDepthCallback : public InspectNodeCallbackEmpty
{
protected:
    int depth;
public:
	int GetDepth() { return depth; } // FIXME remove me!
	DomGetDepthCallback() : depth(0) {}

	/* virtual */ void SetParent(DOM_Object *node, DOM_Object *parent);
};

class InspectAttribute : public InspectNodeCallbackEmpty
{
	AttributeType attribute_export;
	OP_STATUS status;
public:
	InspectAttribute(AttributeType attribute_export)
		: attribute_export(attribute_export)
		, status(OpStatus::OK)
		{}

	OP_STATUS GetStatus() const { return status; }

	// virtual methods from InspectNodeCallback
	void SetName(DOM_Object *node, const uni_char *namespace_uri, const uni_char *prefix, const uni_char *localpart);
	void SetValue(DOM_Object *node, const uni_char *value);
};

class InspectAttributes : public InspectNodeCallbackEmpty
{
	AttributeListType attributes_export;
	OP_STATUS status;
public:
	InspectAttributes(AttributeListType attributes_export)
		: attributes_export(attributes_export)
		, status(OpStatus::OK)
		{}
	OP_STATUS GetStatus() const { return status; }

	// virtual methods from InspectNodeCallback
	void AddAttribute(DOM_Object *node, DOM_Object *attribute);
};

class ScopeDomInspector : public DomGetDepthCallback
{
	ES_ScopeDebugFrontend *debugger;
	ES_ScopeDebugFrontend::NodeType type;
	uni_char* namespace_uri;
	uni_char* prefix;
	uni_char* localpart;
	uni_char* value;
	uni_char* name;
	uni_char* public_id;
	uni_char* system_id;
	uni_char* buffer;
	unsigned children;
	DOM_Object *node;
	DOM_Object *content_document;
	DOM_Object *frame_element;
	ES_ScopeDebugFrontend::NodeInfo::MatchReason reason;
public:
	ScopeDomInspector(ES_ScopeDebugFrontend *debugger) :
	    debugger(debugger),
		namespace_uri(0),
		prefix(0),
		localpart(0),
		value(0),
		name(0),
		public_id(0),
		system_id(0),
		children(0),
		node(0),
		content_document(0),
		frame_element(0),
		reason(ES_ScopeDebugFrontend::NodeInfo::MATCHREASON_TRAVERSAL)
		{
			OP_ASSERT(debugger);
		}

	/**
	 * Get the ID for the an object.
	 *
	 * @param node The object to get the ID for.
	 * @return The ID for the object, or 0 on errors.
	 */
	unsigned GetObjectID(DOM_Object *node);

	/**
	 * Get the ID for the an ES object.
	 *
	 * @param obj The object to get the ID for.
	 * @return The ID for the object, or 0 on errors.
	 */
	unsigned GetObjectID(ES_Runtime *runtime, ES_Object *obj);

	/**
	 * Get the ID for a runtime.
	 *
	 * @param runtime The runtime to get the ID for.
	 * @return The ID for the runtime, or 0 on errors.
	 */
	unsigned GetRuntimeID(DOM_Runtime *runtime);

	/**
	 * Fill the ObjectReference message with information from 'obj'.
	 *
	 * @param [in] obj The object to retrieve information from.
	 * @param [out] ref The destination message.
	 * @return OpStatus::OK, or OOM.
	 */
	OP_STATUS GetObjectReference(DOM_Object *obj, ES_ScopeDebugFrontend::ObjectReference *&ref);

	/**
	 * Like GetObjectReference, but checks whether the specified object is
	 * a proxy object. If it is, the proxied object is used instead. If it's
	 * not, the original object it used.
	 *
	 * @param [in] obj The object (or proxy) to retrieve information from.
	 * @param [out] ref The destination message.
	 * @return OpStatus::OK, or OOM.
	 */
	OP_STATUS GetProxiedObjectReference(DOM_Object *obj, ES_ScopeDebugFrontend::ObjectReference *&ref);

	// virtual methods from InspectNodeCallback
	~ScopeDomInspector();
	void SetType(DOM_Object *node, DOM_NodeType type);
	void SetName(DOM_Object *node, const uni_char *namespace_uri, const uni_char *prefix, const uni_char *localpart);
	void SetValue(DOM_Object *node, const uni_char *value);
	void SetDocumentTypeInformation(DOM_Object *node, const uni_char *name, const uni_char *public_id, const uni_char *system_id);
	void SetFrameInformation(DOM_Object *node, DOM_Object *content_document);
	void SetDocumentInformation(DOM_Object *node, DOM_Object *frame_element);
	void AddChild(DOM_Object *node, DOM_Object *child) { children++; }

	void SetMatchReason(ES_ScopeDebugFrontend::NodeInfo::MatchReason reason);
	OP_STATUS AddNodeInfo(NodeListType nodes);

	/**
	 * Set NodeInfo based on the presence of a content_document for a
	 * frame or object node.
	 *
	 * @param [out] info Set contentDocument field on this message.
	 * @param [in] content_document The content document for the frame. NULL
	 *                              is a valid value, which will cause the function
	 *                              to fail silently (return OpStatus::OK).
	 * @return OpStatus::OK, or OOM.
	 */
	OP_STATUS AddContentDocumentInfo(ES_ScopeDebugFrontend::NodeInfo *info, DOM_Object *content_document);

	/**
	 * Set NodeInfo based on the presence of a frame_element for a
	 * (child) document node.
	 *
	 * @param [out] info  Set frameElement field on this message.
	 * @param [in] content_document The frame element for the document. NULL
	 *                              is a valid value, which will cause the function
	 *                              to fail silently (return OpStatus::OK).
	 * @return OpStatus::OK, or OOM.
	 */
	OP_STATUS AddFrameElementInfo(ES_ScopeDebugFrontend::NodeInfo *info, DOM_Object *frame_element);


	/**
	 * Set pseudo element information on NodeInfo, if pseudo elements are
	 * present.
	 *
	 * @param [in] element The element which may or may not have pseudo elements.
	 *                     May be NULL, in which case OpStatus::OK is returned.
	 * @param [out] node The object to store the information on. May not be NULL.
	 * @return OpStatus::OK on success (also if no pseudo elements were found);
	 *         or OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS AddPseudoElements(HTML_Element *element, ES_ScopeDebugFrontend::NodeInfo *node);

	/**
	 * Get the text content produced by a content property in a ::before or ::after
	 * style rule.
	 *
	 * @param [in] element The element which may or may not be affected by a ::before
	 *                     or ::after rule. May not be NULL.
	 * @param [in] type Either ELM_PSEUDO_BEFORE, or ELM_PSEUDO_AFTER.
	 * @return A string if the pseudo element was found, and contained text content.
	 *         NULL otherwise.
	 */
	const uni_char *GetBeforeOrAfterContent(HTML_Element *element, PseudoElementType type);

	BOOL RequestFailed() { return node == 0; }
};

class InspectChildren : public InspectNodeCallbackEmpty
{
protected:
	ES_ScopeDebugFrontend *debugger;
	NodeListType nodes;
	BOOL request_failed;
	DOM_Object **recursion_stack;
public:
	InspectChildren(ES_ScopeDebugFrontend *debugger, NodeListType nodes, DOM_Object **recursion_stack = 0)
		: debugger(debugger)
		, nodes(nodes)
		, request_failed(FALSE)
		, recursion_stack(recursion_stack)
		{
			OP_ASSERT(debugger);
		}

	BOOL RequestFailed() { return request_failed; }

	/* virtual */ void AddChild(DOM_Object *node, DOM_Object *child);
};

class InspectChildrenRecursively : public InspectChildren
{
	unsigned assumed_depth; // FIXME remove me
public:
	InspectChildrenRecursively(ES_ScopeDebugFrontend *debugger, NodeListType nodes, unsigned assumed_depth) : InspectChildren(debugger, nodes), assumed_depth(assumed_depth) {}
	/* virtual */ void AddChild(DOM_Object *node, DOM_Object *child) { request_failed |= OpStatus::IsError(GetDOMView(debugger, nodes, child, assumed_depth)); }
};

class DomGetParentChain : public InspectNodeCallbackEmpty
{
protected:
	unsigned over, under; // nodes over and under this one
	DOM_Object** parentStack;
	OP_STATUS status;
public:
	unsigned GetStackSize() { return over + 1; }
	DOM_Object** GetStack() { return parentStack; }
	OP_STATUS GetStatus() const { return status; }

	DomGetParentChain(unsigned under = 0)
		: over(0)
		, under(under)
		, parentStack(0)
		, status(OpStatus::OK)
	{}
	/* virtual */ void SetParent(DOM_Object *node, DOM_Object *parent);
};

class DomSearch
	: public DOM_Utils::SearchCallback
{
protected:
	ES_ScopeDebugFrontend *debugger;
public:
	DomSearch(ES_ScopeDebugFrontend *debugger, NodeListType nodes)
		: debugger(debugger)
		, nodes(nodes)
	{
		OP_ASSERT(debugger);
	}

	OP_STATUS MatchNode(DOM_Object *node)
	{
		// Parents first.

		DomGetParentChain parent_chain;
		RETURN_IF_ERROR(DOM_Utils::InspectNode(node, DOM_Utils::INSPECT_PARENT, &parent_chain));
		RETURN_IF_ERROR(parent_chain.GetStatus());

		DOM_Object **parents = parent_chain.GetStack();
		OpAutoArray<DOM_Object*> anchor(parents);
		unsigned count = parent_chain.GetStackSize();

		if (parents)
			for (unsigned i = 0; i < count && (parents[i] != node); ++i)
				RETURN_IF_ERROR(AddNodeInfo(parents[i], TRUE));

		// Then "this" node:
		return AddNodeInfo(node, FALSE);
	}

	OP_STATUS AddNodeInfo(DOM_Object *node, BOOL parent)
	{
		// If the node is already added, then don't add it again, but *do*
		// update the match reason, if the we try to add it as a direct
		// hit this time (i.e. parent == TRUE).
		if (added_nodes.Contains(node))
		{
			// If it's added as a hit, update the match reason (CORE-40979).
			if (!parent)
			{
				ES_ScopeDebugFrontend::NodeInfo *info;
				RETURN_IF_ERROR(added_nodes.GetData(node, &info));
				info->SetMatchReason(ES_ScopeDebugFrontend::NodeInfo::MATCHREASON_SEARCH_HIT);
			}

			// If it's added as a parent, don't do anything.

			return OpStatus::OK;
		}

		ScopeDomInspector inspector(debugger);
		if (parent)
			inspector.SetMatchReason(ES_ScopeDebugFrontend::NodeInfo::MATCHREASON_SEARCH_ANCESTOR);
		else
			inspector.SetMatchReason(ES_ScopeDebugFrontend::NodeInfo::MATCHREASON_SEARCH_HIT);
		RETURN_IF_ERROR(DOM_Utils::InspectNode(node, DOM_Utils::INSPECT_ALL, &inspector));
		RETURN_IF_ERROR(inspector.AddNodeInfo(nodes));

		ES_ScopeDebugFrontend::NodeInfo *last = nodes.Get(nodes.GetCount() - 1);

		return added_nodes.Add(node, last);
	}

private:
	OpPointerHashTable<DOM_Object, ES_ScopeDebugFrontend::NodeInfo> added_nodes;
	NodeListType nodes;
};

/* virtual */ void
InspectChildren::AddChild(DOM_Object *node, DOM_Object *child)
{
	ScopeDomInspector inspectNode(debugger);
	DOM_Utils::InspectNode(child, DOM_Utils::INSPECT_ALL, &inspectNode);

	if (recursion_stack && recursion_stack[0] == child)
		request_failed |= OpStatus::IsError(GetViewWithChildren(debugger, nodes, recursion_stack));
	else
		request_failed |= OpStatus::IsError(inspectNode.AddNodeInfo(nodes));
}

/* virtual */ void 
DomGetParentChain::SetParent(DOM_Object *node, DOM_Object *parent)
{
	OP_ASSERT(parent);
	if (parent)
	{
		DomGetParentChain cb(under + 1);
		DOM_Utils::InspectNode(parent, DOM_Utils::INSPECT_PARENT, &cb);
		OP_STATUS tmp_status = cb.GetStatus();
		if (OpStatus::IsError(tmp_status))
		{
			status = tmp_status;
			return;
		}

		if (!cb.parentStack)
		{
			// the node above did not have a parent
			OP_ASSERT(cb.over == 0);
			parentStack = OP_NEWA(DOM_Object*, under + 3); // 3 = 1 for over + 1 for this node + 1 for NULL
			if (!parentStack)
			{
				status = OpStatus::ERR_NO_MEMORY;
				return;
			}
			parentStack[0] = parent;
		}
		else
		{
			OP_ASSERT(cb.over);
			parentStack = cb.parentStack;
		}				
		over = cb.over + 1;
		parentStack[over] = node;
	}
}

/* virtual */ void
DomGetDepthCallback::SetParent(DOM_Object *node, DOM_Object *parent)
{
	if (parent)
	{
		DomGetDepthCallback cb;
		DOM_Utils::InspectNode(parent, DOM_Utils::INSPECT_PARENT, &cb);
		depth = cb.depth + 1;
	}
}

/* virtual */ void
InspectAttribute::SetName(DOM_Object *node, const uni_char *namespace_uri, const uni_char *prefix, const uni_char *localpart)
{
	OP_STATUS tmp_status = attribute_export.SetNamePrefix(prefix);
	if (OpStatus::IsSuccess(tmp_status))
		tmp_status = attribute_export.SetName(localpart);
	if (OpStatus::IsError(tmp_status))
		status = tmp_status;
}

/* virtual */ void
InspectAttribute::SetValue(DOM_Object *node, const uni_char *value)
{
	OP_STATUS tmp_status = attribute_export.SetValue(value);
	if (OpStatus::IsError(tmp_status))
		status = tmp_status;
}

static ES_ScopeDebugFrontend::NodeType
ConvertNodeType(DOM_NodeType type)
{
	switch (type)
	{
	default:
		OP_ASSERT(!"Please add a new case");
	case NOT_A_NODE:
		return ES_ScopeDebugFrontend::NODETYPE_UNKNOWN;
	case ELEMENT_NODE:
		return ES_ScopeDebugFrontend::NODETYPE_ELEMENT;
	case ATTRIBUTE_NODE:
		return ES_ScopeDebugFrontend::NODETYPE_ATTRIBUTE;
	case TEXT_NODE:
		return ES_ScopeDebugFrontend::NODETYPE_TEXT;
	case CDATA_SECTION_NODE:
		return ES_ScopeDebugFrontend::NODETYPE_CDATA_SECTION;
	case ENTITY_REFERENCE_NODE:
		return ES_ScopeDebugFrontend::NODETYPE_ENTITY_REFERENCE;
	case ENTITY_NODE:
		return ES_ScopeDebugFrontend::NODETYPE_ENTITY;
	case PROCESSING_INSTRUCTION_NODE:
		return ES_ScopeDebugFrontend::NODETYPE_PROCESSING_INSTRUCTION;
	case COMMENT_NODE:
		return ES_ScopeDebugFrontend::NODETYPE_COMMENT;
	case DOCUMENT_NODE:
		return ES_ScopeDebugFrontend::NODETYPE_DOCUMENT;
	case DOCUMENT_TYPE_NODE:
		return ES_ScopeDebugFrontend::NODETYPE_DOCUMENT_TYPE;
	case DOCUMENT_FRAGMENT_NODE:
		return ES_ScopeDebugFrontend::NODETYPE_DOCUMENT_FRAGMENT;
	case NOTATION_NODE:
		return ES_ScopeDebugFrontend::NODETYPE_NOTATION;
	case XPATH_NAMESPACE_NODE:
		return ES_ScopeDebugFrontend::NODETYPE_XPATH_NAMESPACE;
	case SVG_ELEMENTINSTANCE_NODE:
		return ES_ScopeDebugFrontend::NODETYPE_SVG_ELEMENTINSTANCE;
	}
}

/* virtual */  void
ScopeDomInspector::SetType(DOM_Object *node, DOM_NodeType type)
{
	this->node = node;
	this->type = ConvertNodeType(type);
}

/* virtual */ void
ScopeDomInspector::SetName(DOM_Object *node, const uni_char *namespace_uri, const uni_char *prefix, const uni_char *localpart)
{
	if (namespace_uri) this->namespace_uri = uni_strdup(namespace_uri);
	if (prefix) this->prefix = uni_strdup(prefix);
	if (localpart) this->localpart = uni_strdup(localpart);
}

/* virtual */ void
ScopeDomInspector::SetValue(DOM_Object *node, const uni_char *value)
{
	if (value) this->value = uni_strdup(value);
}

/* virtual */ void
ScopeDomInspector::SetDocumentTypeInformation(DOM_Object *node, const uni_char *name, const uni_char *public_id, const uni_char *system_id)
{
	if (name) this->name = uni_strdup(name);
	if (public_id) this->public_id = uni_strdup(public_id);
	if (system_id) this->system_id = uni_strdup(system_id);
}

void
ScopeDomInspector::SetFrameInformation(DOM_Object *node, DOM_Object *content_document)
{
	this->content_document = content_document;
}

void
ScopeDomInspector::SetDocumentInformation(DOM_Object *node, DOM_Object *frame_element)
{
	this->frame_element = frame_element;
}

/* virtual */ void
InspectAttributes::AddAttribute(DOM_Object *node, DOM_Object *attribute)
{
	OpAutoPtr<ES_ScopeDebugFrontend::NodeInfo::Attribute> attr(OP_NEW(ES_ScopeDebugFrontend::NodeInfo::Attribute, ()));
	if (!attr.get())
	{
		status = OpStatus::ERR_NO_MEMORY;
		return;
	}

	InspectAttribute inspect(*attr.get());
	DOM_Utils::InspectNode(attribute, DOM_Utils::INSPECT_BASIC, &inspect);
	OP_STATUS tmp_status = inspect.GetStatus();
	if (OpStatus::IsError(tmp_status))
	{
		status = tmp_status;
		return;
	}
	attributes_export.Add(attr.release());
}

unsigned
ScopeDomInspector::GetObjectID(DOM_Object *node)
{
	return debugger->GetObjectId(node->GetRuntime(), DOM_Utils::GetES_Object(node));
}

unsigned
ScopeDomInspector::GetObjectID(ES_Runtime *runtime, ES_Object *obj)
{
	return debugger->GetObjectId(runtime, obj);
}

unsigned
ScopeDomInspector::GetRuntimeID(DOM_Runtime *runtime)
{
	return debugger->GetRuntimeId(DOM_Utils::GetES_Runtime(runtime));
}

OP_STATUS
ScopeDomInspector::GetObjectReference(DOM_Object *obj, ES_ScopeDebugFrontend::ObjectReference *&ref)
{
	OpAutoPtr<ES_ScopeDebugFrontend::ObjectReference> objref(OP_NEW(ES_ScopeDebugFrontend::ObjectReference, ()));
	RETURN_OOM_IF_NULL(objref.get());

	objref->SetRuntimeID(GetRuntimeID(DOM_Utils::GetDOM_Runtime(obj)));
	objref->SetObjectID(GetObjectID(obj));

	ref = objref.release();
	return OpStatus::OK;
}

OP_STATUS
ScopeDomInspector::GetProxiedObjectReference(DOM_Object *obj, ES_ScopeDebugFrontend::ObjectReference *&ref)
{
	obj = DOM_Utils::GetProxiedObject(obj);

	return (obj ? GetObjectReference(obj, ref) : OpStatus::ERR);
}

/* virtual */
ScopeDomInspector::~ScopeDomInspector()
{
	op_free(value);
	op_free(namespace_uri);
	op_free(prefix);
	op_free(localpart);
	op_free(name);
	op_free(public_id);
	op_free(system_id);
}

void
ScopeDomInspector::SetMatchReason(ES_ScopeDebugFrontend::NodeInfo::MatchReason reason)
{
	this->reason = reason;
}

OP_STATUS
ScopeDomInspector::AddNodeInfo(NodeListType nodes)
{
	OpAutoPtr<ES_ScopeDebugFrontend::NodeInfo> nodeinfo(OP_NEW(ES_ScopeDebugFrontend::NodeInfo, ()));
	RETURN_OOM_IF_NULL(nodeinfo.get());
	nodeinfo->SetObjectID(GetObjectID(node));
	nodeinfo->SetRuntimeID(GetRuntimeID(DOM_Utils::GetDOM_Runtime(node)));
	nodeinfo->SetType(type);
	RETURN_IF_ERROR(nodeinfo->SetName(localpart));
	nodeinfo->SetDepth(depth);
	nodeinfo->SetMatchReason(reason);

	switch (type)
	{
	case ELEMENT_NODE:
		RETURN_IF_ERROR(nodeinfo->SetNamespacePrefix(prefix));
		RETURN_IF_ERROR(GetAttributes(nodeinfo->GetAttributeListRef(), node));
		nodeinfo->SetChildrenLength(children);
		RETURN_IF_MEMORY_ERROR(AddContentDocumentInfo(nodeinfo.get(), content_document));
		RETURN_IF_ERROR(AddPseudoElements(DOM_Utils::GetHTML_Element(node), nodeinfo.get()));
		break;
	case TEXT_NODE:
	case CDATA_SECTION_NODE:
	case PROCESSING_INSTRUCTION_NODE:
	case COMMENT_NODE:
		RETURN_IF_ERROR(nodeinfo->SetValue(value));
		break;
	case DOCUMENT_TYPE_NODE:
		RETURN_IF_ERROR(nodeinfo->SetName(name)); // CORE-28696.
		nodeinfo->SetPublicID(public_id);
		nodeinfo->SetSystemID(system_id);
	case DOCUMENT_NODE:
		RETURN_IF_MEMORY_ERROR(AddFrameElementInfo(nodeinfo.get(), frame_element));
		break;
	}

	DOM_EventTarget *event_target = node->GetEventTarget();
	if (event_target)
	{
		DOM_EventTarget::NativeIterator it = event_target->NativeListeners();
		for (; !it.AtEnd(); it.Next())
		{
			const DOM_EventListener *listener = it.Current();

			ES_ScopeDebugFrontend::EventListener *listener_info = nodeinfo->AppendNewEventListenerList();
			RETURN_OOM_IF_NULL(listener_info);
			RETURN_IF_ERROR(debugger->SetEventListenerInfo(*listener_info, *listener, node->GetRuntime()));
		}
	}

	RETURN_IF_ERROR(nodes.Add(nodeinfo.release()));
	return OpStatus::OK;
}

OP_STATUS
ScopeDomInspector::AddPseudoElements(HTML_Element *element, ES_ScopeDebugFrontend::NodeInfo *node)
{
	if (!element)
		return OpStatus::OK;

	if (element->HasAfter())
	{
		ES_ScopeDebugFrontend::PseudoElement *elm = node->GetPseudoElementListRef().AddNew();
		RETURN_OOM_IF_NULL(elm);
		elm->SetType(ES_ScopeDebugFrontend::PseudoElement::TYPE_AFTER);
		RETURN_IF_ERROR(elm->SetContent(GetBeforeOrAfterContent(element, ELM_PSEUDO_AFTER)));
	}
	
	if (element->HasBefore())
	{
		ES_ScopeDebugFrontend::PseudoElement *elm = node->GetPseudoElementListRef().AddNew();
		RETURN_OOM_IF_NULL(elm);
		elm->SetType(ES_ScopeDebugFrontend::PseudoElement::TYPE_BEFORE);
		RETURN_IF_ERROR(elm->SetContent(GetBeforeOrAfterContent(element, ELM_PSEUDO_BEFORE)));
	}

	if (element->HasFirstLetter())
	{
		ES_ScopeDebugFrontend::PseudoElement *elm = node->GetPseudoElementListRef().AddNew();
		RETURN_OOM_IF_NULL(elm);
		elm->SetType(ES_ScopeDebugFrontend::PseudoElement::TYPE_FIRST_LETTER);
	}

	if (element->HasFirstLine())
	{
		ES_ScopeDebugFrontend::PseudoElement *elm = node->GetPseudoElementListRef().AddNew();
		RETURN_OOM_IF_NULL(elm);
		elm->SetType(ES_ScopeDebugFrontend::PseudoElement::TYPE_FIRST_LINE);
	}

	return OpStatus::OK;
}

const uni_char *
ScopeDomInspector::GetBeforeOrAfterContent(HTML_Element *element, PseudoElementType type)
{
	switch (type)
	{
	case ELM_PSEUDO_BEFORE:
	case ELM_PSEUDO_AFTER:
		break;
	default:
		return NULL; // Unsupported.
	}

	HTML_Element *pseudo = static_cast<HTML_Element *>(element->FirstChild());
	DocTree *stop = element->NextSibling();

	while (pseudo && stop != pseudo)
	{
		if (pseudo->GetPseudoElement() == type)
			break;

		pseudo = static_cast<HTML_Element *>(pseudo->Suc());
	}

	if (!pseudo)
		return NULL;

	// The generated content is inserted as a child of the pseudo element,
	// so look for a child element containing text, and return it.

	HTML_Element *child = static_cast<HTML_Element *>(pseudo->FirstChild());
	stop = pseudo->NextSibling();

	while (child && child != stop)
	{
		if (child->IsText())
			return child->Content();

		child = static_cast<HTML_Element *>(child->Suc());
	}

	return NULL;
}

OP_STATUS
ScopeDomInspector::AddContentDocumentInfo(ES_ScopeDebugFrontend::NodeInfo *info, DOM_Object *content_document)
{
	if (!content_document)
		return OpStatus::OK;

	ES_ScopeDebugFrontend::ObjectReference *ref;
	RETURN_IF_ERROR(GetProxiedObjectReference(content_document, ref));
	info->SetContentDocument(ref);

	return OpStatus::OK;
}

OP_STATUS
ScopeDomInspector::AddFrameElementInfo(ES_ScopeDebugFrontend::NodeInfo *info, DOM_Object *frame_element)
{
	if (!frame_element)
		return OpStatus::OK;

	ES_ScopeDebugFrontend::ObjectReference *ref;
	RETURN_IF_ERROR(GetProxiedObjectReference(frame_element, ref));
	info->SetFrameElement(ref);

	return OpStatus::OK;
}

OP_STATUS
GetAttributes(AttributeListType attributes_export, DOM_Object *node)
{
	InspectAttributes inspectNode(attributes_export);
	DOM_Utils::InspectNode(node, DOM_Utils::INSPECT_ATTRIBUTES, &inspectNode);
	return inspectNode.GetStatus();
}

OP_STATUS
GetDOMView(ES_ScopeDebugFrontend *debugger, NodeListType nodes, DOM_Object *node, int assumed_depth)
{
	ScopeDomInspector inspectNode(debugger);
	RETURN_IF_ERROR(DOM_Utils::InspectNode(node, DOM_Utils::INSPECT_ALL, &inspectNode));

	if (inspectNode.RequestFailed())
		return OpStatus::ERR;

	OP_ASSERT(assumed_depth == -1 || assumed_depth == inspectNode.GetDepth());

	RETURN_IF_ERROR(inspectNode.AddNodeInfo(nodes));

	InspectChildrenRecursively recurse(debugger, nodes, inspectNode.GetDepth() + 1);
	RETURN_IF_ERROR(DOM_Utils::InspectNode(node, DOM_Utils::INSPECT_CHILDREN, &recurse));
	if (recurse.RequestFailed())
		return OpStatus::ERR;
	return OpStatus::OK;
}

OP_STATUS
GetChildrenView(ES_ScopeDebugFrontend *debugger, NodeListType nodes, DOM_Object* node, DOM_Object** stack)
{
	InspectChildren dom_inspector(debugger, nodes, stack);
	RETURN_IF_ERROR(DOM_Utils::InspectNode(node, DOM_Utils::INSPECT_ALL, &dom_inspector));

	return dom_inspector.RequestFailed() ? OpStatus::ERR : OpStatus::OK;
}

OP_STATUS
GetNodeView(ES_ScopeDebugFrontend *debugger, NodeListType nodes, DOM_Object* node)
{
	ScopeDomInspector inspectNode(debugger);
	OP_STATUS status = DOM_Utils::InspectNode(node, DOM_Utils::INSPECT_BASIC | DOM_Utils::INSPECT_PARENT | DOM_Utils::INSPECT_CHILDREN, &inspectNode);
	if (OpStatus::IsError(status) || inspectNode.RequestFailed())
		return OpStatus::ERR;

	RETURN_IF_ERROR(inspectNode.AddNodeInfo(nodes));

	return OpStatus::OK;
}

OP_STATUS
GetViewWithChildren(ES_ScopeDebugFrontend *debugger, NodeListType nodes, DOM_Object** stack)
{
	DOM_Object* top = stack[0];
	if (!top)
		return OpStatus::OK;
	RETURN_IF_ERROR(GetNodeView(debugger, nodes, top));
	RETURN_IF_ERROR(GetChildrenView(debugger, nodes, top, stack + 1));
	return OpStatus::OK;
}

OP_STATUS
GetParentNodeChainView(ES_ScopeDebugFrontend *debugger, NodeListType nodes, DOM_Object* node)
{
	DomGetParentChain cb;
	RETURN_IF_ERROR(DOM_Utils::InspectNode(node, DOM_Utils::INSPECT_PARENT, &cb));
	RETURN_IF_ERROR(cb.GetStatus());

	DOM_Object** stack = cb.GetStack();
	if (!stack)
		return OpStatus::ERR_NO_MEMORY;
	stack[cb.GetStackSize() - 1] = 0;

	OP_STATUS err = GetViewWithChildren(debugger, nodes, stack);

	OP_DELETEA(stack);
	return err;
}

OP_STATUS
GetDOMNodes(ES_ScopeDebugFrontend *debugger, NodeListType nodes, DOM_Object* node, TraversalType traversal)
{
	switch (traversal)
	{
	case TRAVERSAL_NODE:     return GetNodeView(debugger, nodes, node);
	case TRAVERSAL_SUBTREE:  return GetDOMView(debugger, nodes, node);
	case TRAVERSAL_CHILDREN: return GetChildrenView(debugger, nodes, node);
	case TRAVERSAL_PARENT_NODE_CHAIN_WITH_CHILDREN: return GetParentNodeChainView(debugger, nodes, node);
	}
	return OpStatus::ERR;
}

OP_STATUS
SearchDOMNodes(ES_ScopeDebugFrontend *debugger, NodeListType nodes, const DOM_Utils::SearchOptions &options)
{
	DomSearch inspector(debugger, nodes);
	return DOM_Utils::Search(options, &inspector);
}

/**
 * Finds all event listeners on @a node and stores the event-name in @a names.
 *
 * @param[out] names Unique event-names will be placed here.
 * @return OpStatus::OK if successful, or OpStatus::ERR_NO_MEMORY on OOM.
 */
OP_STATUS
AddEventNames(DOM_Object *node, EventNameSet &names)
{
	DOM_EventTarget *event_target = node->GetEventTarget();
	if (event_target)
	{
		DOM_EventTarget::NativeIterator it = event_target->NativeListeners();
		for (; !it.AtEnd(); it.Next())
		{
			const DOM_EventListener *listener = it.Current();

			DOM_EventType event_type = listener->GetKnownType();
			BOOL found = FALSE;
			if (event_type == DOM_EVENT_CUSTOM)
			{
				const uni_char *name = listener->GetCustomEventType();
				for (unsigned i = 0; i < names.GetCount(); ++i)
				{
					if (names.Get(i)->Compare(name) == 0)
					{
						found = TRUE;
						break;
					}
				}
			}
			else
			{
				const char *name = DOM_EventsAPI::GetEventTypeString(event_type);
				for (unsigned i = 0; i < names.GetCount(); ++i)
				{
					if (names.Get(i)->Compare(name) == 0)
					{
						found = TRUE;
						break;
					}
				}
			}
			if (found)
				continue;

			OpString *event_name = OP_NEW(OpString, ());
			RETURN_OOM_IF_NULL(event_name);
			RETURN_IF_ERROR(names.Add(event_name));
			if (event_type == DOM_EVENT_CUSTOM)
				RETURN_IF_ERROR(event_name->Set(listener->GetCustomEventType()));
			else
				RETURN_IF_ERROR(event_name->Set(DOM_EventsAPI::GetEventTypeString(event_type)));
		}
	}
	return OpStatus::OK;
}

/**
 * Inspects children of nodes recursively and extracts the event names from
 * any event listeners registered on the nodes and places them in the set
 * @a names.
 */
class InspectEventNameChildren : public InspectNodeCallbackEmpty
{
protected:
	EventNameSet &names;
	BOOL request_failed;
public:
	/**
	 * Initialize the inspector with the event-name set.
	 * @param[out] names Unique event-names will be placed here.
	 */
	InspectEventNameChildren(EventNameSet &names)
		: names(names)
		, request_failed(FALSE)
	{
	}

	/**
	 * @return @c TRUE if the request or any sub-requests failed, @c FALSE otherwise.
	 */
	BOOL RequestFailed() const { return request_failed; }

	// virtual methods from InspectNodeCallback
	/* virtual */ void AddChild(DOM_Object *node, DOM_Object *child);
};

/* virtual */ void
InspectEventNameChildren::AddChild(DOM_Object *node, DOM_Object *child)
{
	request_failed |= OpStatus::IsError(AddEventNames(child, names));
	if (request_failed)
		return;

	InspectEventNameChildren inspect(names);
	DOM_Utils::InspectNode(child, DOM_Utils::INSPECT_CHILDREN, &inspect);
	request_failed |= inspect.RequestFailed();
}

OP_STATUS
GetEventNames(EventNameSet &names, DOM_Object *node, BOOL include_children)
{
	OP_ASSERT(node);
	RETURN_IF_ERROR(AddEventNames(node, names));

	if (include_children)
	{
		InspectEventNameChildren inspect(names);
		DOM_Utils::InspectNode(node, DOM_Utils::INSPECT_CHILDREN, &inspect);

		if (inspect.RequestFailed())
			return OpStatus::ERR;
	}
	return OpStatus::OK;
}

#endif // SCOPE_ECMASCRIPT_DEBUGGER && DOM_INSPECT_NODE_SUPPORT
