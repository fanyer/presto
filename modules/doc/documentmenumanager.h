/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef DOC_DOCUMENTMENUMANAGER_H
#define DOC_DOCUMENTMENUMANAGER_H

#include "modules/util/objectmanager.h"


class OpDocumentContext;
class OpBitmap;

/** Base class for any menu item added by document to the context menu.
 *  It provides basic unique id generation and any menu item is searchable
 *  using g_menu_manager->FindMenuItemById().
 */
class DocumentMenuItem : public ObjectManager<DocumentMenuItem>::Object
{
public:
	DocumentMenuItem();

	/** Called when the menu item is clicked. */
	virtual void OnClick(OpDocumentContext* document_context, FramesDocument* document, HTML_Element* element) = 0;

	/** Gets the icon for menu item.
	 *
	 *  TODO : Icon bitmaps should be just included to OpContextMenuRequestMessage
	 *  when the request is generated so it should not be needed here. This will be
	 *  fixed when we have mechanisms for passing bitmaps in messages.
	 */
	virtual OP_STATUS GetIconBitmap(OpBitmap*& icon_bitmap) = 0;
};

/** This class is used for storage and id management of DocumentMenuItem.
 *
 *  Currently all the DOM_MenuItems are managed by a global g_menu_manager.
 *
 *  @note in MultiProcess Phase 2 DocumentMenuManager will be global object
 *  CoreService component and all the communication will be done via messages.
 */
class DocumentMenuManager : private ObjectManager<DocumentMenuItem>
{
	friend class DocumentMenuItem;
public:
	/** Called when the menu item is clicked. */
	void OnDocumentMenuItemClicked(UINT32 id, OpDocumentContext* context);

	/** Retrieves menu item for a given id. */
	DocumentMenuItem* FindMenuItemById(UINT32 id) { return FindItemById(id); }
};

#endif // DOC_DOCUMENTMENUMANAGER_H

