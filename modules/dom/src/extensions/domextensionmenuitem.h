/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA. All rights reserved.
**
** This file is part of the Opera web browser. It may not be distributed
** under any circumstances.
*/

#ifndef DOM_EXTENSIONS_MENUITEM_H
#define DOM_EXTENSIONS_MENUITEM_H

#ifdef DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT

#include "modules/dom/src/extensions/domextensionmenucontext.h"
#include "modules/windowcommander/OpWindowCommander.h"
#include "modules/doc/externalinlinelistener.h"
#include "modules/doc/documentmenumanager.h"

class DOM_ExtensionMenuItem
	: public DOM_ExtensionMenuContext
	, public DocumentMenuItem
{
public:
	enum MenuContextType
	{
		MENU_CONTEXT_NONE        = 0x00,
		MENU_CONTEXT_PAGE        = 0x01,
		MENU_CONTEXT_FRAME       = 0x02,
		MENU_CONTEXT_SELECTION   = 0x04,
		MENU_CONTEXT_LINK        = 0x08,
		MENU_CONTEXT_EDITABLE    = 0x10,
		MENU_CONTEXT_IMAGE       = 0x20,
		MENU_CONTEXT_VIDEO       = 0x40,
		MENU_CONTEXT_AUDIO       = 0x80,
		MENU_CONTEXT_ALL         = 0xFF
	};

	static OP_STATUS Make(DOM_ExtensionMenuItem*& new_obj, DOM_ExtensionSupport* opera, DOM_Runtime* origining_runtime);

	/* From DOM_Object: */
	virtual ES_GetState GetName(OpAtom property_atom, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(const uni_char* property_name, int property_atom, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_atom, ES_Value* value, ES_Runtime* origining_runtime);

	virtual void GCTrace();
	virtual BOOL IsA(int type) { return type == DOM_TYPE_EXTENSION_MENUITEM || DOM_ExtensionMenuContext::IsA(type); }

	/** Assigns contents of props object to MenuItem.
	 *
	 *  Helper for DOM_ExtensionMenuContext::createItem.
	 *  @props Properties to assign to a menu item. It is assumed that this object
	 *         matches MenuItemProperties interface.
	 *  @return
	 *    - OK
	 *    - ERR_NO_MEMORY
	 *  @see http://devrel.oslo.osa/extensions/specs/features/contextmenus/Overview.html?view=co#menuitemproperties
	 */
	OP_STATUS SetMenuItemProperties(ES_Object* props);

	enum ItemType
	{
		/** Normal clickable menu item. */
		MENU_ITEM_COMMAND   = 1,
		/** Container for other menu items. */
		MENU_ITEM_SUBMENU   = 2,
		/** Separator between groups of menu items. */
		MENU_ITEM_SEPARATOR = 3
	};


	const uni_char* GetTitle();
	const uni_char* GetTypeString() { return TypeEnum2TypeString(m_type); }
	ItemType        GetType()       { return m_type; }
	const uni_char* GetId()         { return m_id.CStr(); }
	BOOL            GetDisabled()   { return m_disabled; }
	const uni_char* GetIconURL()    { return m_icon_url.GetAttribute(URL::KUniName_With_Fragment); }

	OP_STATUS SetTitle(const uni_char* title)   { return m_title.Set(title); }
	OP_STATUS SetId(const uni_char* id)         { return m_id.Set(id); }
	void      SetDisabled(BOOL disabled)        { m_disabled = disabled; }
	/** Sets an icon to this menu icon and if needed initiates downloading it. */
	OP_STATUS SetIcon(const uni_char* icon_url);

	/** Append DocumentMenuItem object representing this menu item (along with it's children) if it maches document_context. */
	virtual OP_STATUS AppendContextMenu(OpWindowcommanderMessages_MessageSet::PopupMenuRequest::MenuItemList& menu_items, OpDocumentContext* document_context, FramesDocument* document, HTML_Element* element);

	virtual DOM_ExtensionMenuItem* GetItem(unsigned int index);
	virtual unsigned int GetLength();

protected:
	virtual CONTEXT_MENU_ADD_STATUS AddItem(DOM_ExtensionMenuItem* item, DOM_ExtensionMenuItem* before);
	virtual void RemoveItem(unsigned int index);
private:
	DOM_ExtensionMenuItem(DOM_ExtensionSupport* extension_support);
	~DOM_ExtensionMenuItem();

	/** Checks if this menu item is allowed in specified document context. */
	OP_BOOLEAN IsAllowedInContext(OpDocumentContext* document_context, FramesDocument* document, HTML_Element* element);

	/** Helper to check if 'context' property matches document_context. */
	OP_BOOLEAN CheckContexts(OpDocumentContext* document_context, FramesDocument* document, HTML_Element* element);

	/** Helper to check if 'documentURLPatterns' property matches document_context. */
	OP_BOOLEAN CheckDocumentPatterns(OpDocumentContext* document_context, FramesDocument* document, HTML_Element* element);

	/** Helper to check if 'targetURLPatterns' property matches document_context. */
	OP_BOOLEAN CheckLinkPatterns(OpDocumentContext* document_context, FramesDocument* document, HTML_Element* element);

	/** Checks if url matches url rule. */
	static BOOL URLMatchesRule(const uni_char* url, const uni_char* rule);

	/** Checks if this item is ancestor of a given item. */
	BOOL IsAncestorOf(DOM_ExtensionMenuContext* item);

	/** Converts string representation of ItemType into enum.
	  * Defaults to MENU_ITEM_COMMAND.
	  */

	ItemType TypeString2TypeEnum(const uni_char* type);
	/** Converts ItemType enum to string representation.
	  * Defaults to "entry";
	  */

	const uni_char* TypeEnum2TypeString(ItemType type);

	/** Converts string representation of MenuContextType into enum.
	  * Defaults to MENU_CONTEXT_NONE.
	  */
	MenuContextType ContextNameToEnum(const uni_char* string);

	/** Called when this menu item is clicked. */
	virtual void OnClick(OpDocumentContext* document_context, FramesDocument* document, HTML_Element* element);

	virtual OP_STATUS GetIconBitmap(OpBitmap*& icon_bitmap);

	OpString m_title;
	OpString m_id;
	URL m_icon_url;
	OpVector<DOM_ExtensionMenuItem> m_items;
	BOOL m_disabled;
	ItemType m_type;
	class DummyInlineListener : public ExternalInlineListener
	{
	public:
		DummyInlineListener() {}
	};
	DummyInlineListener m_icon_loading_listener;
	DOM_ExtensionMenuContext* m_parent_menu;

	void ResetIconImage();
	Image m_icon_image;
	OpBitmap* m_icon_bitmap;

	friend class DOM_ExtensionRootMenuContext;

	static void GetMediaType(HTML_Element* element, const uni_char*& type, const uni_char*& url);

	/** Constructs a native array object and clones the contents of es_array into it.
	  * The resulting array is written back to es_array.
	  */
	static OP_STATUS CloneArray(ES_Value& es_array, DOM_Runtime* runtime);
};

#endif // DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT

#endif // !DOM_EXTENSIONS_MENUITEM_H