/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA. All rights reserved.
**
** This file is part of the Opera web browser. It may not be distributed
** under any circumstances.
*/

#ifndef DOM_EXTENSIONS_MENUCONTEXT_PROXY_H
#define DOM_EXTENSIONS_MENUCONTEXT_PROXY_H

#ifdef DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT

#include "modules/dom/src/domobj.h"
#include "modules/dom/src/extensions/domextensioncontexts.h"
#include "modules/dom/src/extensions/domextensionmenuitem_proxy.h"

class DOM_ExtensionMenuItem;
class DOM_ExtensionMenuItemProxy;

/** Class representing global opera.contexsts.menu object in extension UserJS.
 *
 *  Currently only supports listening to click events on menu items
 *  created in the background process.
 */
class DOM_ExtensionMenuContextProxy
	: public DOM_Object
	, public DOM_EventTargetOwner
{
	friend class DOM_ExtensionMenuItemProxy;
public:
	static OP_STATUS Make(DOM_ExtensionMenuContextProxy*& new_obj, DOM_ExtensionSupport* extension_support, DOM_Runtime* origining_runtime);

	~DOM_ExtensionMenuContextProxy();

	/* From DOM_Object: */
	virtual ES_GetState GetName(const uni_char* property_name, int property_atom, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(const uni_char* property_name, int property_atom, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_GetState FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime* origining_runtime);

	virtual void GCTrace();
	virtual BOOL IsA(int type) { return type == DOM_TYPE_EXTENSION_MENUCONTEXT_PROXY || DOM_Object::IsA(type); }

	enum
	{
		FUNCTIONS_WITH_DATA_addEventListener = 1,
		FUNCTIONS_WITH_DATA_removeEventListener,
		FUNCTIONS_WITH_DATA_ARRAY_SIZE
	};

	/* From DOM_EventTargetOwner: */
	DOM_Object* GetOwnerObject() { return this; }

	void OnMenuItemClicked(UINT32 item_id, const uni_char* item_js_id, OpDocumentContext* document_context, FramesDocument* document, HTML_Element* element);
	void ProxyMenuItemDestroyed(UINT32 item_id);

private:
	DOM_ExtensionMenuContextProxy(DOM_ExtensionSupport* extension_support);

	OpSmartPointerWithDelete<DOM_ExtensionSupport> m_extension_support;

	DOM_ExtensionMenuItemProxy* FindOrCreateProxyItem(UINT32 item_id, const uni_char* js_id);
	OpINT32HashTable<DOM_ExtensionMenuItemProxy> m_item_cache;

	class GCTraceItemCacheIterator : public OpHashTableForEachListener
	{
	public:
		GCTraceItemCacheIterator(DOM_Runtime* runtime) :  runtime(runtime) {}
		virtual void HandleKeyData(const void* key, void* data) ;
		DOM_Runtime* runtime;
	};
};

#endif // DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT

#endif // !DOM_EXTENSIONS_MENUCONTEXT_PROXY_H
