/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA. All rights reserved.
**
** This file is part of the Opera web browser. It may not be distributed
** under any circumstances.
*/

#ifndef DOM_EXTENSIONS_MENUCONTEXT_H
#define DOM_EXTENSIONS_MENUCONTEXT_H

#ifdef DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT

#include "modules/dom/src/domobj.h"
#include "modules/dom/src/extensions/domextensioncontexts.h"

class DOM_ExtensionMenuItem;

class DOM_ExtensionMenuContext
	: public DOM_Object
	, public DOM_EventTargetOwner
{
public:

	/* From DOM_Object: */
	virtual ES_GetState GetIndex(int property_index, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutIndex(int property_index, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_GetState GetName(const uni_char* property_name, int property_atom, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_GetState GetName(OpAtom property_atom, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(const uni_char* property_name, int property_atom, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_atom, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_GetState FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime* origining_runtime);

	virtual void GCTrace();
	virtual BOOL IsA(int type) { return type == DOM_TYPE_EXTENSION_MENUCONTEXT || DOM_Object::IsA(type); }

	/** Append all DocumentMenuItem entries matching specified document object in this context. */
	virtual OP_STATUS AppendContextMenu(OpWindowcommanderMessages_MessageSet::PopupMenuRequest::MenuItemList& menu_items, OpDocumentContext* document_context, FramesDocument* document, HTML_Element* element) = 0;

	DOM_DECLARE_FUNCTION(addItem);
	DOM_DECLARE_FUNCTION(removeItem);
	DOM_DECLARE_FUNCTION(item);

	enum {
		FUNCTIONS_addItem = 1,
		FUNCTIONS_removeItem,
		FUNCTIONS_item,
		FUNCTIONS_ARRAY_SIZE
	};

	enum
	{
		FUNCTIONS_WITH_DATA_addEventListener = 1,
		FUNCTIONS_WITH_DATA_removeEventListener,
		FUNCTIONS_WITH_DATA_ARRAY_SIZE
	};

	// From DOM_EventTargetOwner:
	DOM_Object* GetOwnerObject() { return this; }
protected:
	DOM_ExtensionMenuContext(DOM_ExtensionSupport* extension_support);

	/** @name protected virtual interface reimplemented by classes inheriting.
	 *
	 *  DOM_ExtensionMenuItem overrides these functions to provide full
	 *  add/remove/get functionality. The base DOM_ExtensionMenuContext
	 *  only accepts one menu item.
	 *  @{ */

	class ContextMenuAddStatus : public OpStatus
	{
	public:
		enum {
			ERR_WRONG_HIERARCHY = OpStatus::USER_ERROR + 1,
			ERR_WRONG_MENU_TYPE
		};
	};
	typedef OP_STATUS CONTEXT_MENU_ADD_STATUS;

	/** Adds item to this menu context.
	  * @param item menu item which will be added.
	  * @param before menu item before the item will be inserted.
	  * @return
	  *   - OK
	  *   - ERR_NO_MEMORY - oom.
	  *   - ERR_OUT_OF_RANGE - If no more items can be added to this menu.
	  *   - ERR_WRONG_HIERARCHY - either item is ancestor of this menu
	  *     or before is not a child of this menu.
	  *   - ERR_WRONG_NODE_TYPE - it this menu context doesn't support
	  *     adding items.
	  */
	virtual CONTEXT_MENU_ADD_STATUS AddItem(DOM_ExtensionMenuItem* item, DOM_ExtensionMenuItem* before) = 0;

	/** Removes item from this menu context.
	  * @param index - position at which to remove item.
	  */
	virtual void RemoveItem(unsigned int index) = 0;

	/** Retrieves item from this menu contex.
	  * @param index - position at which the item is retrieved.
	  * @return Retrieved menu item or NULL if out of range.
	  */
	virtual DOM_ExtensionMenuItem* GetItem(unsigned int index) = 0;

	/** Gets the number of menu items in this menu context. */
	virtual unsigned int GetLength() = 0;
	/**  @} */

	OpSmartPointerWithDelete<DOM_ExtensionSupport> m_extension_support;
};

class DOM_ExtensionRootMenuContext : public DOM_ExtensionMenuContext
{
public:
	static OP_STATUS Make(DOM_ExtensionMenuContext*& new_obj, DOM_ExtensionSupport* extension_support, DOM_Runtime* origining_runtime);

	DOM_DECLARE_FUNCTION(createItem);

	enum {
		FUNCTIONS_createItem = 1,
		FUNCTIONS_ARRAY_SIZE
	};

	virtual BOOL IsA(int type) { return type == DOM_TYPE_EXTENSION_ROOTMENUCONTEXT || DOM_ExtensionMenuContext::IsA(type); }
	virtual void GCTrace();
private:
	DOM_ExtensionRootMenuContext(DOM_ExtensionSupport* extension_support);

	virtual OP_STATUS AppendContextMenu(OpWindowcommanderMessages_MessageSet::PopupMenuRequest::MenuItemList& menu_items, OpDocumentContext* document_context, FramesDocument* document, HTML_Element* element);

	virtual CONTEXT_MENU_ADD_STATUS AddItem(DOM_ExtensionMenuItem* item, DOM_ExtensionMenuItem* before);
	virtual void RemoveItem(unsigned int index);
	virtual DOM_ExtensionMenuItem* GetItem(unsigned int index);
	virtual unsigned int GetLength();

	DOM_ExtensionMenuItem* m_root_item;
};

#endif // DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT

#endif // !DOM_EXTENSIONS_MENUCONTEXT_H
