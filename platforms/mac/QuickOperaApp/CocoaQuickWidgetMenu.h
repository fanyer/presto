/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "platforms/mac/embrowser/EmBrowserQuick.h"

// internally used as EmQuickMenuKind
enum {
	quickMenuKindApple = 0xFFFF
};

class QuickWidgetMenu;

struct CocoaQuickMenuMap
{
	void* menuRef;
	QuickWidgetMenu* quickMenu;
	CocoaQuickMenuMap* next;
};

class QuickWidgetMenu
{
public:
	QuickWidgetMenu(const UniChar * title, const char * name, EmQuickMenuKind kind);
	~QuickWidgetMenu();
	
	static Boolean AddItemToMenu(void* menu, SInt32 after_item, const EmQuickMenuItemInfo *info, UInt32 hiMenuEquv = 0);
	Boolean AddItem(SInt32 after_item, const EmQuickMenuItemInfo *info);
	Boolean RemoveItem(SInt32 item);
	Boolean RemoveAllItems();
	Boolean InsertInMenubar(QuickWidgetMenu *afterItem);
	Boolean RemoveFromMenubar();
	Boolean Refresh();
	EmQuickMenuKind	GetMenuKind() {return mKind;}
	
	
	static QuickWidgetMenu* FindQuickMenu(void* inMenuRef);
		
		
protected:
	short			GetMenuID();
	void			InstallAsSubMenu(void *parentMenu);
	
private:
	short			mMenuID;
	unsigned short	mItemCount;
	void*			mMenu;
	void*			mMenuItem;
	EmQuickMenuKind mKind;
	Boolean			mInsertSeparator;

	static CocoaQuickMenuMap* sMenuMap;
	static void* sMenuPool;
};		

