/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_ELEMENT_H
#define DOM_ELEMENT_H

#include "modules/dom/src/domcore/node.h"
#include "modules/dom/src/domcore/nodemap.h"

#include "modules/util/simset.h"

#include "modules/util/adt/opvector.h"

class DOM_Attr;
class DOM_XPathNamespace;
class DOM_ClientRectList;

class DOM_Element
	: public DOM_Node
{
protected:
	ES_GetState	GetOffsetProperties(FramesDocument *frames_doc, LogicalDocument *hl_doc, HTML_Element *element, OpAtom property_name, ES_Value* value);
	ES_GetState GetCollection(ES_Value *value, DOM_HTMLElement_Group group, const char *class_name, int private_name);

	HTML_Element *this_element;
	DOM_Attr *first_attr;
#ifdef DOM3_XPATH
	DOM_XPathNamespace *first_ns;
#endif // DOM3_XPATH

	OP_STATUS GetAttributeNode(DOM_Attr *&node, Markup::AttrType attr, const uni_char *name, int ns_idx, BOOL &has_attribute, int &index, BOOL create_if_has_attribute, BOOL create_always, BOOL case_sensitive);

public:
	DOM_Element();
	virtual ~DOM_Element();

	virtual ES_GetState GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_GetState GetNameRestart(const uni_char *property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object);
	virtual ES_GetState GetNameRestart(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object);
	virtual ES_PutState PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

	virtual ES_DeleteStatus DeleteName(const uni_char *property_name, ES_Runtime *origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_ELEMENT || DOM_Node::IsA(type); }
	virtual void GCTraceSpecial(BOOL via_tree);

	virtual void DOMChangeRuntime(DOM_Runtime *new_runtime);
	virtual void DOMChangeOwnerDocument(DOM_Document *new_ownerDocument);

	virtual OP_STATUS GetBoundingClientRect(DOM_Object *&object);
	virtual OP_STATUS GetClientRects(DOM_ClientRectList *&object);
	static OP_STATUS MakeClientRect(DOM_Object *&object, RECT &rect, DOM_Runtime *runtime);

	HTML_Element *GetThisElement() { return this_element; }
	void SetThisElement(HTML_Element *element) { this_element = element; }
	void ClearThisElement() { this_element = NULL; }

	DOM_Attr *GetFirstAttributeNode() { return first_attr; }
	void SetFirstAttributeNode(DOM_Attr *attr) { first_attr = attr; }

	OP_STATUS GetAttributeNode(DOM_Attr *&node, Markup::AttrType attr, const uni_char *name, int ns_idx, BOOL create_if_has_attribute, BOOL create_always, BOOL case_sensitive);

	const uni_char *GetTagName(TempBuffer *buffer, BOOL uppercase, BOOL include_prefix);

#ifdef DOM3_XPATH
	OP_STATUS GetXPathNamespaceNode(DOM_XPathNamespace *&nsnode, const uni_char *prefix, const uni_char *uri);
#endif // DOM3_XPATH

#ifdef DOM2_MUTATION_EVENTS
	BOOL HasAttrModifiedHandlers();
	OP_STATUS SendAttrModified(ES_Thread *interrupt_thread, DOM_Attr *attr, const uni_char *prevValue, const uni_char *newValue);
	OP_STATUS SendAttrModified(ES_Thread *interrupt_thread, const uni_char *name, int ns_idx, const uni_char *prevValue, const uni_char *newValue);
#endif /* DOM2_MUTATION_EVENTS */

	OP_STATUS SetAttribute(Markup::AttrType attr, const uni_char *name, int ns_idx, const uni_char *value, unsigned value_length, BOOL case_sensitive, ES_Runtime *origining_runtime);

#ifdef DOM_INSPECT_NODE_SUPPORT
	OP_STATUS InspectAttributes(DOM_Utils::InspectNodeCallback *callback);
#endif // DOM_INSPECT_NODE_SUPPORT

	BOOL HasCaseSensitiveNames();

	enum accessAttributeData
	{
		GET_ATTRIBUTE,
		GET_ATTRIBUTE_NS,
		HAS_ATTRIBUTE,
		HAS_ATTRIBUTE_NS,
		SET_ATTRIBUTE,
		SET_ATTRIBUTE_NS,
		REMOVE_ATTRIBUTE,
		REMOVE_ATTRIBUTE_NS
	};

	DOM_DECLARE_FUNCTION(removeAttributeNode);
	DOM_DECLARE_FUNCTION(scrollIntoView);
	DOM_DECLARE_FUNCTION(getClientRects);
	DOM_DECLARE_FUNCTION(getBoundingClientRect);
#ifdef DOM_FULLSCREEN_MODE
	DOM_DECLARE_FUNCTION(requestFullscreen);
#endif // DOM_FULLSCREEN_MODE
	enum
	{
		FUNCTIONS_BASIC = 4,
#ifdef DOM_FULLSCREEN_MODE
		FUNCTIONS_requestFullscreen,
#endif // DOM_FULLSCREEN_MODE
		FUNCTIONS_ARRAY_SIZE
	};

	DOM_DECLARE_FUNCTION_WITH_DATA(accessAttribute); // {has,get,set,remove}Attribute{,NS}
	DOM_DECLARE_FUNCTION_WITH_DATA(getAttributeNode); // getAttributeNode{,NS}
	DOM_DECLARE_FUNCTION_WITH_DATA(setAttributeNode); // setAttributeNode{,NS}
	DOM_DECLARE_FUNCTION_WITH_DATA(getElementsByTagName); // getElementsByTagName{,NS}
	enum { FUNCTIONS_WITH_DATA_ARRAY_SIZE = 19 };
};

class DOM_ClientRectList
	: public DOM_Object
{
public:
	DOM_ClientRectList() : object_list(NULL) {}
	virtual ~DOM_ClientRectList() { OP_DELETEA(object_list); }

	static OP_STATUS Make(DOM_ClientRectList *&list, DOM_Runtime *runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_CLIENTRECTLIST || DOM_Object::IsA(type); }

	virtual ES_GetState	GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_GetState GetIndex(int property_index, ES_Value* value, ES_Runtime *origining_runtime);
	virtual void GCTrace();

	virtual ES_GetState GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime);

	DOM_DECLARE_FUNCTION(item);

	enum {
		FUNCTIONS_ARRAY_SIZE = 2
	};

	OpVector<RECT> &GetList() { return rect_list; }

private:
	OpAutoVector<RECT> rect_list;
	DOM_Object **object_list;
};

class DOM_AttrMapImpl
	: public DOM_NamedNodeMapImpl
{
protected:
	DOM_Element *element;

	OP_STATUS Start(DOM_Attr *&attr, int &ns_idx, const uni_char *ns_uri, const uni_char *name, BOOL case_sensitive);

public:
	DOM_AttrMapImpl(DOM_Element *element)
		: element(element)
	{
	}

	virtual void GCTrace()
	{
		DOM_Object::GCMark(element);
	}

	virtual int Length();
	virtual int Item(int index, ES_Value *return_value, ES_Runtime *origining_runtime);

	virtual int GetNamedItem(const uni_char *ns_uri, const uni_char *name, BOOL case_sensitive, ES_Value *return_value, DOM_Runtime *origining_runtime);
	virtual int RemoveNamedItem(const uni_char *ns_uri, const uni_char *name, BOOL case_sensitive, ES_Value *return_value, DOM_Runtime *origining_runtime);
	virtual int SetNamedItem(DOM_Node *node, BOOL ns, ES_Value *return_value, DOM_Runtime *origining_runtime);
};

#include "modules/doc/frm_doc.h"
#include "modules/ecmascript_utils/esthread.h"
#include "modules/ecmascript_utils/essched.h"

/* Used for delaying reading of properties whose values might depend
   on style information that the layout engine is still waiting for. */
class DOM_DelayedLayoutListener
	: public DelayedLayoutListener,
	  public ES_ThreadListener
{
private:
	FramesDocument *frames_doc;
	ES_Thread *thread;

public:
	DOM_DelayedLayoutListener(FramesDocument *frames_doc, ES_Thread* thread);

	virtual void LayoutContinued();
	virtual OP_STATUS Signal(ES_Thread *, ES_ThreadSignal signal);
};

#endif  // DOM_ELEMENT_H
