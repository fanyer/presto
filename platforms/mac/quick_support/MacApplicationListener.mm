/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/quick_support/MacApplicationListener.h"
#include "platforms/mac/util/systemcapabilities.h"
#include "platforms/mac/util/macutils.h"
#include "adjunct/quick/managers/KioskManager.h"
#include "platforms/mac/QuickOperaApp/OperaExternalNSMenu.h"
#include "platforms/mac/quick_support/CocoaInternalQuickMenu.h"

#define BOOL NSBOOL
#import <AppKit/NSApplication.h>
#import <AppKit/NSMenu.h>
#import <AppKit/NSMenuItem.h>
#undef BOOL

#ifdef NO_CARBON

/* static */
BOOL MacApplicationListener::s_bookmarks_loaded;

void MacApplicationListener::OnStarted()
{
	// Ref. DSK-293741
	if (KioskManager::GetInstance()->GetEnabled())
	{
		if (GetOSVersion() >= 0x1060)
		{
			// NSApplicationPresentationHideDock                   = (1 <<  1),    // Dock is entirely unavailable
			// NSApplicationPresentationHideMenuBar                = (1 <<  3),    // Menu Bar is entirely unavailable
			// NSApplicationPresentationDisableAppleMenu           = (1 <<  4),    // all Apple menu items are disabled
			// NSApplicationPresentationDisableProcessSwitching    = (1 <<  5),    // Cmd+Tab UI is disabled
			// NSApplicationPresentationDisableForceQuit           = (1 <<  6),    // Cmd+Opt+Esc panel is disabled
			// NSApplicationPresentationDisableSessionTermination  = (1 <<  7),    // PowerKey panel and Restart/Shut Down/Log Out disabled
			// NSApplicationPresentationDisableHideApplication     = (1 <<  8),    // Application "Hide" menu item is disabled
			uint options = (1 << 1) | (1 << 3);
			if (KioskManager::GetInstance()->GetNoExit()) {
				options |= (1 << 4) | (1 << 5) | (1 << 6) | (1 << 7) | (1 << 8);
			}
			[NSApp setPresentationOptions:options];
		}
	}
}

BOOL MacApplicationMenuListener::ListPopupMenuInfo(OpScopeDesktopWindowManager_SI::QuickMenuList &out)
{
	NSMenu *active_menu = (NSMenu *)GetActiveMenu();
	
	while (active_menu)
	{
		OpAutoPtr<OpScopeDesktopWindowManager_SI::QuickMenuInfo> info(OP_NEW(OpScopeDesktopWindowManager_SI::QuickMenuInfo, ()));
		if (info.get() == NULL)
			return OpStatus::ERR;
		
		OP_STATUS rtn = OpStatus::ERR;

		// Depending of which menu this is (internal or external) call the correct function
		if (active_menu == [NSApp mainMenu] || [active_menu respondsToSelector: @selector(operaMenuName)])	// Test the selector just to be sure
			rtn = SetQuickExternalMenuInfo((void *)active_menu, *info.get());
		else
			rtn = SetQuickInternalMenuInfo((void *)active_menu, *info.get());
			
		if (OpStatus::IsSuccess(rtn))
		{
			out.GetMenuListRef().Add(info.get());
			info.release();

			// If there is another menu above this one then add it too
			if ([active_menu supermenu])
				active_menu = [active_menu supermenu];
			else
				return TRUE;
		}
	}
	
	return FALSE;
}

void MacApplicationListener::OnBookmarkModelLoaded()
{
	s_bookmarks_loaded = TRUE;
};

#endif // NO_CARBON

void MacApplicationMenuListener::BuildPlatformCustomMenuByName(void* menu, const char* menu_name)
{
	if (op_stricmp(menu_name, "Content Sharing") == 0) {
		[[NSClassFromString(@"SharingContentDelegate") performSelector:@selector(getInstance)] performSelector: @selector(fillMenuWithSharers:) withObject:(NSMenu*)((CocoaInternalQuickMenu*)menu)->GetMenu()];
	}
}
