/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA. All rights reserved.
**
** This file is part of the Opera web browser. It may not be distributed
** under any circumstances.
*/

#include <core/pch.h>

#ifdef DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT

#include "modules/dom/src/extensions/domextensionmenucontext.h"
#include "modules/dom/src/extensions/domextensionmenuitem.h"

DOM_ExtensionMenuContext::DOM_ExtensionMenuContext(DOM_ExtensionSupport* extension_support)
	: m_extension_support(extension_support)
{
}

/* virtual */ ES_GetState
DOM_ExtensionMenuContext::GetName(const uni_char* property_name, int property_atom, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name[0] == 'o' && property_name[1] == 'n')
	{
		ES_GetState res = GetEventProperty(property_name, value, static_cast<DOM_Runtime*>(origining_runtime));
		if (res != GET_FAILED)
			return res;
	}

	return DOM_Object::GetName(property_name, property_atom, value, origining_runtime);
}

/* virtual */ ES_GetState
DOM_ExtensionMenuContext::GetName(OpAtom property_atom, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_atom)
	{
	case OP_ATOM_length:
		DOMSetNumber(value, GetLength());
		return GET_SUCCESS;
	default:
		return DOM_Object::GetName(property_atom, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOM_ExtensionMenuContext::PutName(const uni_char* property_name, int property_atom, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name[0] == 'o' && property_name[1] == 'n')
	{
		ES_PutState res = PutEventProperty(property_name, value, static_cast<DOM_Runtime*>(origining_runtime));
		if (res != PUT_FAILED)
			return res;
	}
	return DOM_Object::PutName(property_name, property_atom, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_ExtensionMenuContext::PutName(OpAtom property_atom, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_atom)
	{
	case OP_ATOM_length:
		return PUT_READ_ONLY;
	default:
		return DOM_Object::PutName(property_atom, value, origining_runtime);
	}
}

/* virtual */ ES_GetState
DOM_ExtensionMenuContext::GetIndex(int property_index, ES_Value* value, ES_Runtime* origining_runtime)
{

	DOMSetObject(value, property_index >= 0 ? GetItem(property_index) : NULL);
	return GET_SUCCESS;
}

/* virtual */ ES_PutState
DOM_ExtensionMenuContext::PutIndex(int property_index, ES_Value* value, ES_Runtime* origining_runtime)
{
	return PUT_READ_ONLY;
}

/* virtual */ ES_GetState
DOM_ExtensionMenuContext::FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime)
{
	ES_GetState result = DOM_Object::FetchPropertiesL(enumerator, origining_runtime);
	if (result != GET_SUCCESS)
		return result;

	enumerator->AddPropertyL("onclick");
	return GET_SUCCESS;
}

/* virtual */ void
DOM_ExtensionMenuContext::GCTrace()
{
	GCMark(FetchEventTarget());
}

/* static */ int
DOM_ExtensionMenuContext::addItem(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(menu_context, DOM_TYPE_EXTENSION_MENUCONTEXT, DOM_ExtensionMenuContext);
	DOM_CHECK_ARGUMENTS("o");
	DOM_ARGUMENT_OBJECT(uiitem, 0, DOM_TYPE_EXTENSION_MENUITEM, DOM_ExtensionMenuItem);
	DOM_ExtensionMenuItem* before = NULL;
	if (argc > 1)
	{
		switch (argv[1].type)
		{
		case VALUE_NUMBER:
			before = menu_context->GetItem(TruncateDoubleToUInt(argv[1].value.number));
			break;
		case VALUE_OBJECT:
			DOM_ARGUMENT_OBJECT_EXISTING(before, 1, DOM_TYPE_EXTENSION_MENUITEM, DOM_ExtensionMenuItem);
			break;
		}
	}

	OP_STATUS err = menu_context->AddItem(uiitem, before);
	if (OpStatus::IsSuccess(err))
		return ES_FAILED;
	switch (err)
	{
		case OpStatus::ERR_NOT_SUPPORTED:
		case OpStatus::ERR_OUT_OF_RANGE:
			return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);
		case ContextMenuAddStatus::ERR_WRONG_MENU_TYPE:
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		case ContextMenuAddStatus::ERR_WRONG_HIERARCHY:
			return DOM_CALL_DOMEXCEPTION(HIERARCHY_REQUEST_ERR);
		default:
			CALL_FAILED_IF_ERROR(err);
	}
	return ES_FAILED;
}

/* static */ int
DOM_ExtensionMenuContext::removeItem(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(menu_context, DOM_TYPE_EXTENSION_MENUCONTEXT, DOM_ExtensionMenuContext);
	DOM_CHECK_ARGUMENTS("n");

	menu_context->RemoveItem(TruncateDoubleToUInt(argv[0].value.number));
	return ES_FAILED;
}

/* static */ int
DOM_ExtensionMenuContext::item(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(menu_context, DOM_TYPE_EXTENSION_MENUCONTEXT, DOM_ExtensionMenuContext);
	DOM_CHECK_ARGUMENTS("n");

	unsigned int index = TruncateDoubleToUInt(argv[0].value.number);
	DOMSetObject(return_value, menu_context->GetItem(index));
	return ES_VALUE;
}

DOM_ExtensionRootMenuContext::DOM_ExtensionRootMenuContext(DOM_ExtensionSupport* extension_support)
	: DOM_ExtensionMenuContext(extension_support)
	, m_root_item(NULL)
{
}

OP_STATUS
DOM_ExtensionRootMenuContext::AppendContextMenu(OpWindowcommanderMessages_MessageSet::PopupMenuRequest::MenuItemList& menu_items, OpDocumentContext* document_context, FramesDocument* document, HTML_Element* element)
{
	if (m_root_item)
		return m_root_item->AppendContextMenu(menu_items, document_context, document, element);

	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_ExtensionRootMenuContext::Make(DOM_ExtensionMenuContext *&new_obj, DOM_ExtensionSupport *extension_support, DOM_Runtime *origining_runtime)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(new_obj = OP_NEW(DOM_ExtensionRootMenuContext, (extension_support)), origining_runtime, origining_runtime->GetPrototype(DOM_Runtime::EXTENSION_ROOTMENUCONTEXT_PROTOTYPE), "MenuContext"));
	return OpStatus::OK;
}

/* virtual */ void
DOM_ExtensionRootMenuContext::GCTrace()
{
	GCMark(m_root_item);
	DOM_ExtensionMenuContext::GCTrace();
}

/* virtual */ DOM_ExtensionMenuContext::CONTEXT_MENU_ADD_STATUS
DOM_ExtensionRootMenuContext::AddItem(DOM_ExtensionMenuItem* item, DOM_ExtensionMenuItem* /* ignored_before */)
{
	if (m_root_item) // Only one top level item allowed
		return OpStatus::ERR_OUT_OF_RANGE;
	m_root_item = item;
	m_root_item->m_parent_menu = this;
	return OpStatus::OK;
}

/* virtual */ void
DOM_ExtensionRootMenuContext::RemoveItem(unsigned int index)
{
	if (index == 0 && m_root_item)
	{
		m_root_item->m_parent_menu = NULL;
		m_root_item = NULL;
	}
}

/* virtual */ DOM_ExtensionMenuItem*
DOM_ExtensionRootMenuContext::GetItem(unsigned int index)
{
	if (index == 0)
		return m_root_item;
	else
		return NULL;
}

/* virtual */ unsigned int
DOM_ExtensionRootMenuContext::GetLength()
{
	return m_root_item ? 1 : 0;
}

/* static */ int
DOM_ExtensionRootMenuContext::createItem(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(menu_context, DOM_TYPE_EXTENSION_ROOTMENUCONTEXT, DOM_ExtensionRootMenuContext);
	DOM_CHECK_ARGUMENTS("o");

	DOM_ExtensionMenuItem* new_obj;
	CALL_FAILED_IF_ERROR(DOM_ExtensionMenuItem::Make(new_obj, menu_context->m_extension_support, origining_runtime));
	CALL_FAILED_IF_ERROR(new_obj->SetMenuItemProperties(argv[0].value.object));
	DOMSetObject(return_value, new_obj);
	return ES_VALUE;
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_ExtensionMenuContext)
	DOM_FUNCTIONS_FUNCTION(DOM_ExtensionMenuContext, DOM_ExtensionMenuContext::addItem, "addItem", "-?(#MenuItem|n)-")
	DOM_FUNCTIONS_FUNCTION(DOM_ExtensionMenuContext, DOM_ExtensionMenuContext::removeItem, "removeItem", "n-")
	DOM_FUNCTIONS_FUNCTION(DOM_ExtensionMenuContext, DOM_ExtensionMenuContext::item, "item", "n-")
DOM_FUNCTIONS_END(DOM_ExtensionMenuContext)

DOM_FUNCTIONS_WITH_DATA_START(DOM_ExtensionMenuContext)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_ExtensionMenuContext, DOM_Node::accessEventListener, 0, "addEventListener", "s-b-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_ExtensionMenuContext, DOM_Node::accessEventListener, 1, "removeEventListener", "s-b-")
DOM_FUNCTIONS_WITH_DATA_END(DOM_ExtensionMenuContext)

DOM_FUNCTIONS_START(DOM_ExtensionRootMenuContext)
	DOM_FUNCTIONS_FUNCTION(DOM_ExtensionRootMenuContext, DOM_ExtensionRootMenuContext::createItem, "createItem", "{type:s,contexts:[s],disabled:b,title:s,id:s,icon:S,documentURLPatterns:?[s],targetURLPatterns:?[s],onclick:o}-")
DOM_FUNCTIONS_END(DOM_ExtensionRootMenuContext)


#endif // DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT
