/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA. All rights reserved.
**
** This file is part of the Opera web browser. It may not be distributed
** under any circumstances.
*/

#ifndef DOM_EXTENSIONS_MENUTEM_PROXY_H
#define DOM_EXTENSIONS_MENUTEM_PROXY_H

#ifdef DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT

#include "modules/dom/src/domobj.h"
#include "modules/dom/src/extensions/domextensionsupport.h"
#include "modules/windowcommander/OpWindowCommander.h"
#include "modules/doc/externalinlinelistener.h"

class DocumentMenuItem;
class ShownExtensionMenu;
class DOM_ExtensionMenuContextProxy;


/** Class representing MenuItem in UserJS.
 *
 *  Currently only used as a target for onclick events
 *  on opera.context.menu object. 
 */
class DOM_ExtensionMenuItemProxy
	: public DOM_Object
{
public:
	~DOM_ExtensionMenuItemProxy();
	static OP_STATUS Make(DOM_ExtensionMenuItemProxy*& new_obj
	                   , DOM_ExtensionSupport* extension
	                   , DOM_ExtensionMenuContextProxy* top_level_context
	                   , UINT referred_item_id
	                   , const uni_char* item_js_id
	                   , DOM_Runtime* origining_runtime);

	/* from DOM_Object */
	virtual ES_GetState GetName(const uni_char* property_name, int property_atom, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_GetState GetName(OpAtom property_atom, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(const uni_char* property_name, int property_atom, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_atom, ES_Value* value, ES_Runtime* origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_EXTENSION_MENUITEM_PROXY || DOM_Object::IsA(type); }
	BOOL GetIsSignificant();
	void OnTopLevelContextDestroyed();
private:
	DOM_ExtensionMenuItemProxy(DOM_ExtensionSupport* extension_support, DOM_ExtensionMenuContextProxy* top_level_context, UINT referred_item_id);

	DOM_ExtensionMenuContextProxy* m_top_level_context;
	UINT32 m_item_id; // Internal id of menu item.
	OpString m_id;    // String id visible in js.
	BOOL m_is_significant;
	OpSmartPointerWithDelete<DOM_ExtensionSupport> m_extension_support;
};

#endif // DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT

#endif // !DOM_EXTENSIONS_MENUTEM_PROXY_H