/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA. All rights reserved.
**
** This file is part of the Opera web browser. It may not be distributed
** under any circumstances.
*/

#include <core/pch.h>

#ifdef DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT

#include "modules/dom/src/extensions/domextensionmenuitem_proxy.h"
#include "modules/dom/src/extensions/domextensionmenucontext_proxy.h"
#include "modules/dom/src/extensions/domextensionmenuitem.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/doc/frm_doc.h"
#include "modules/doc/documentinteractioncontext.h"
#include "modules/dom/src/extensions/domextensionmenuevent.h"
#include "modules/dom/src/domcore/node.h"

/* static */ OP_STATUS
DOM_ExtensionMenuItemProxy::Make(DOM_ExtensionMenuItemProxy*& new_obj
                               , DOM_ExtensionSupport* extension_support
                               , DOM_ExtensionMenuContextProxy* top_level_context
                               , UINT referred_item_id
                               , const uni_char* item_js_id
                               , DOM_Runtime* origining_runtime)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(new_obj = OP_NEW(DOM_ExtensionMenuItemProxy, (extension_support, top_level_context, referred_item_id)), origining_runtime, origining_runtime->GetPrototype(DOM_Runtime::EXTENSION_MENUITEM_PROXY_PROTOTYPE), "MenuItemProxy"));
	return new_obj->m_id.Set(item_js_id);
}

DOM_ExtensionMenuItemProxy::DOM_ExtensionMenuItemProxy(DOM_ExtensionSupport* extension_support, DOM_ExtensionMenuContextProxy* top_level_context, UINT referred_item_id)
	: m_top_level_context(top_level_context)
	, m_item_id(referred_item_id)
	, m_is_significant(FALSE)
	, m_extension_support(extension_support)
{
}

DOM_ExtensionMenuItemProxy::~DOM_ExtensionMenuItemProxy()
{
	if (m_top_level_context)
		m_top_level_context->ProxyMenuItemDestroyed(m_item_id);
}

/* virtual */ ES_PutState
DOM_ExtensionMenuItemProxy::PutName(const uni_char* property_name, int property_atom, ES_Value *value, ES_Runtime* origining_runtime)
{
	ES_PutState put_state = DOM_Object::PutName(property_name, property_atom, value, origining_runtime);
	if (put_state == PUT_FAILED || (property_name[0] == 'o' && property_name[1] == 'n'))
		m_is_significant = TRUE;
	return put_state;
}

/* virtual */ ES_PutState
DOM_ExtensionMenuItemProxy::PutName(OpAtom property_atom, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_atom)
	{
	case OP_ATOM_id:
		return PUT_READ_ONLY;
	}
	return DOM_Object::PutName(property_atom, value, origining_runtime);
}

/* virtual */ ES_GetState
DOM_ExtensionMenuItemProxy::GetName(const uni_char* property_name, int property_atom, ES_Value* value, ES_Runtime* origining_runtime)
{
	return DOM_Object::GetName(property_name, property_atom, value, origining_runtime);
}

/* virtual */ ES_GetState
DOM_ExtensionMenuItemProxy::GetName(OpAtom property_atom, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_atom)
	{
		case OP_ATOM_id:
			DOMSetString(value, m_id.CStr());
			return GET_SUCCESS;
	}
	return DOM_Object::GetName(property_atom, value, origining_runtime);
}

void
DOM_ExtensionMenuItemProxy::OnTopLevelContextDestroyed()
{
	m_top_level_context = NULL;
}

BOOL
DOM_ExtensionMenuItemProxy::GetIsSignificant()
{
	return m_is_significant && g_menu_manager->FindMenuItemById(m_item_id);
}

#endif // DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT