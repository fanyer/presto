/*
 *  MacGadgetApplicationListener.cpp
 *  Opera
 *
 *  Created by Mateusz Berezecki on 5/13/09.
 *  Copyright 2009 Opera Software. All rights reserved.
 *
 */

#include "core/pch.h"

#ifdef WIDGET_RUNTIME_SUPPORT
#include "MacGadgetApplicationListener.h"
#define BOOL NSBOOL
#import <AppKit/AppKit.h>
#undef BOOL
#include "platforms/mac/quick_support/MacHostQuickMenu.h"
#include "platforms/mac/quick_support/CocoaInternalQuickMenu.h"
#include "adjunct/widgetruntime/GadgetStartup.h"
#include "adjunct/quick/Application.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/pi/OpBitmap.h"
#include "platforms/mac/model/DockManager.h"
extern Boolean gIsRebuildingMenu;
extern CocoaInternalQuickMenu* gDockMenu;

#define PIXEL_SWAP(a) (((a)>>24)|(((a)>>8)&0xff00)|(((a)&0xff00)<<8)|(((a)&0xff)<<24))

void MacGadgetApplicationListener::OnStart()
{	
	gMenuBar = new MacHostQuickMenuBar();
	if (gMenuBar)
	{
		gIsRebuildingMenu = true;		
		g_application->GetMenuHandler()->AddMenuItems(gMenuBar, "Widget Menu Bar", 0, 0, g_input_manager->GetKeyboardInputContext());
		LocaliseOperaMenu();
		gDockMenu = new CocoaInternalQuickMenu(FALSE, "Dock Widgets Menu", 0, 0);
		g_application->GetMenuHandler()->AddMenuItems(gDockMenu, "Dock Widgets Menu", 1, 0, g_input_manager->GetKeyboardInputContext());
		gIsRebuildingMenu = false;
		g_dock_manager->Init();
		
	}	
	

}

#endif
