/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/quick-widget-names.h"
#include "adjunct/desktop_pi/desktop_pi_util.h"
#include "platforms/mac/util/macutils.h"
#include "platforms/mac/bundledefines.h"
#include "platforms/mac/quick_support/MacHostQuickMenu.h"
#include "platforms/mac/QuickOperaApp/OperaExternalNSMenu.h"
#include "platforms/mac/QuickOperaApp/OperaExternalNSMenuItem.h"
#include "platforms/mac/quick_support/OperaNSMenuItem.h"
#include "platforms/mac/util/MachOCompatibility.h"

#define BOOL NSBOOL
#import <Foundation/NSAppleEventManager.h>
#import <Foundation/NSAppleEventDescriptor.h>
#import <Foundation/NSString.h>
#import <AppKit/NSWorkspace.h>
#import <AppKit/AppKit.h>
#undef BOOL

BOOL OpenFileFolder(const OpStringC & file_path)
{
	return [[NSWorkspace sharedWorkspace] selectFile:[NSString stringWithCharacters:(const unichar *)file_path.CStr() length:file_path.Length()] inFileViewerRootedAtPath:nil];
}


void PostDummyEvent()
{
	NSEvent* dummy = [NSEvent mouseEventWithType:NSMouseMoved
										location:[NSEvent mouseLocation]
								   modifierFlags:0 timestamp:[NSDate timeIntervalSinceReferenceDate]
									windowNumber:[[NSApp mainWindow] windowNumber]
										 context:[[NSApp mainWindow] graphicsContext]
									 eventNumber:0
									  clickCount:1
										pressure:0.0];

	// Workaround for DSK-298450: Need to kick both Opera and the App to finish processing of the action
	[NSApp postEvent:dummy atStart:NO];
	g_component_manager->RunSlice();
}

OpStringC GetCFBundleName()
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	[NSApplication sharedApplication];
	NSDictionary *infoDict = [[NSBundle mainBundle] infoDictionary];
	
	OpString product_name;
	
	SetOpString(product_name, (CFStringRef)[infoDict objectForKey:(NSString *)kCFBundleNameKey]);
	
	[pool release];
	
	return product_name;
}

CFStringRef GetCFBundleID()
{
	CFStringRef bundle_id = nil;
	
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	[NSApplication sharedApplication];
	NSDictionary *infoDict = [[NSBundle mainBundle] infoDictionary];
	
	bundle_id = (CFStringRef)[infoDict objectForKey:(NSString *)kCFBundleIdentifierKey];
	
	[pool release];
	
	return bundle_id;
}

CFStringRef GetOperaBundleID()
{
	CFStringRef bundle_id = nil;
	
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	[NSApplication sharedApplication];
	NSDictionary *infoDict = [[NSBundle mainBundle] infoDictionary];
	
	bundle_id = (CFStringRef)[infoDict objectForKey:@"OperaBundleIdentifier"];
	
	[pool release];
	
	return bundle_id;
}

void *GetMainMenu()
{
	return (void*)[NSApp mainMenu];
}


OP_STATUS SetQuickExternalMenuInfo(void *menu, OpScopeDesktopWindowManager_SI::QuickMenuInfo& info)
{
	if (!menu || !gMenuBar)
		return OpStatus::ERR;
	
	NSMenu *nsmenu = (NSMenu *)menu;
	
	// Set the Menu information
	OpString name;
	// Force the name on the main menu
	if (nsmenu == [NSApp mainMenu])
		name.Set(UNI_L(MENU_NAME_MAIN_MENU));
	else if ([nsmenu respondsToSelector: @selector(operaMenuName)])	// Test the selector just to be sure
	{
		if ([(OperaExternalNSMenu *)nsmenu operaMenuName])
			SetOpString(name, (CFStringRef)[(OperaExternalNSMenu *)nsmenu operaMenuName]);
	}
	info.GetMenuIdRef().SetMenuName(name.CStr());

	// Loop all the items in the menu and add them to the list member
	for (int i = 0; i < [nsmenu numberOfItems]; i++)
	{
		NSMenuItem *nsmenu_item = [[nsmenu itemArray] objectAtIndex: i];
		OpInputAction *action = NULL;
		if (![nsmenu_item isSeparatorItem])
			action = gMenuBar->FindAction([(OperaExternalNSMenuItem *)nsmenu_item command]);

		RETURN_IF_ERROR(SetQuickMenuItemInfo(nsmenu_item, action, i, info));
	}
	
	return OpStatus::OK;
}

OP_STATUS SetQuickInternalMenuInfo(void *menu, OpScopeDesktopWindowManager_SI::QuickMenuInfo& info)
{
	if (!menu)
		return OpStatus::ERR;
	
	NSMenu *nsmenu = (NSMenu *)menu;
	
	// Set the Menu information (the name is in the title for internal menus)
	OpString name;
	SetOpString(name, (CFStringRef)[nsmenu title]);
	info.GetMenuIdRef().SetMenuName(name.CStr());
	
	// Loop all the items in the menu and add them to the list member
	for (int i = 0; i < [nsmenu numberOfItems]; i++)
	{
		NSMenuItem *nsmenu_item = [[nsmenu itemArray] objectAtIndex: i];
		OpInputAction *action = NULL;
		if (![nsmenu_item isSeparatorItem])
			action = [(OperaNSMenuItem *)nsmenu_item getAction];
		
		RETURN_IF_ERROR(SetQuickMenuItemInfo(nsmenu_item, action, i, info));
	}
	
	return OpStatus::OK;
}

OP_STATUS SetQuickMenuItemInfo(void *menu_item, OpInputAction *action, int row, OpScopeDesktopWindowManager_SI::QuickMenuInfo& info)
{
	NSMenuItem *nsmenu_item = (NSMenuItem *)menu_item;
	
	OpAutoPtr<OpScopeDesktopWindowManager_SI::QuickMenuItemInfo> item(OP_NEW(OpScopeDesktopWindowManager_SI::QuickMenuItemInfo, ()));
	if (item.get() == NULL)
		return OpStatus::ERR;
		
	// Add differently if it's a separator or not
	if (![nsmenu_item isSeparatorItem])
	{	
		OpString text;
		SetOpString(text, (CFStringRef)[nsmenu_item title]);
		// Hardcode the Opera menu since for some stange reason it's set to the Apple symbol in CocoaApplication.mm
		if (kMenuAppleLogoFilledGlyph == text[0])
			item.get()->SetText(UNI_L("Opera"));
		else
			item.get()->SetText(text);
		item.get()->SetEnabled([nsmenu_item isEnabled]);  

		if ([nsmenu_item hasSubmenu])
		{
			OpString submenu_name;
			
			if ([[nsmenu_item submenu] respondsToSelector: @selector(operaMenuName)])	// Test the selector
			{
				if ([(OperaExternalNSMenu *)[nsmenu_item submenu] operaMenuName])
					SetOpString(submenu_name, (CFStringRef)[(OperaExternalNSMenu *)[nsmenu_item submenu] operaMenuName]);
			}
			else
				SetOpString(submenu_name, (CFStringRef)[[nsmenu_item submenu] title]);
			item.get()->SetSubmenu(submenu_name);
		}
		
		if (action)
		{
			const char* action_string = OpInputAction::GetStringFromAction(action->GetAction());
			if (action_string)
			{
				OpString str;
				str.Set(action_string);
				item.get()->SetAction(str);

				// Param, if any
				const uni_char* param = action->GetActionDataString();
				if (param && *param)
					item.get()->SetActionParam(param);
			}
		}
		
		item.get()->SetChecked([nsmenu_item state] == NSOnState); 
		item.get()->SetBold(FALSE);
		
		// Set shortcutletter to nothing
		OpString shortcut;
		item.get()->SetShortcutLetter(shortcut);
		
		// Get the shortcut key
		SetOpString(shortcut, (CFStringRef)[nsmenu_item keyEquivalent]);

		// Only build the modifiers if there is  shortcut key
		if (shortcut.HasContent())
		{
			// Build and set the actual shortcut based on how it looks on Windows and Unix
			// Yes these string are meant to be hardcoded!! adamm
			OpString modifiers, modifier_string;
			if (([nsmenu_item keyEquivalentModifierMask] & NSCommandKeyMask) == NSCommandKeyMask)
				modifiers.Append(UNI_L("Ctrl+"));
			if (([nsmenu_item keyEquivalentModifierMask] & NSAlternateKeyMask) == NSAlternateKeyMask)
				modifiers.Append(UNI_L("Alt+"));
			if (([nsmenu_item keyEquivalentModifierMask] & NSShiftKeyMask) == NSShiftKeyMask)
				modifiers.Append(UNI_L("Shift+"));
			if (([nsmenu_item keyEquivalentModifierMask] & NSControlKeyMask) == NSControlKeyMask)
				modifiers.Append(UNI_L("Meta+"));
			
			shortcut.MakeUpper();
			modifiers.Append(shortcut);
			shortcut.Set(modifiers);
		}
		
		item.get()->SetShortcut(shortcut);
	}
	else {
		// Separator
		item.get()->SetSeparator(TRUE);

		// Default all the rest
		OpString blank;
		item.get()->SetText(blank);
		item.get()->SetEnabled(TRUE);  
		item.get()->SetChecked(FALSE); 
		item.get()->SetBold(FALSE);
		item.get()->SetShortcutLetter(blank);
	}
	
	// Set the position this is
	item.get()->SetRow(row);
	
	// Cannot get the rect on Mac so just set it blank
	item.get()->GetRectRef().SetX(0);
	item.get()->GetRectRef().SetY(0);
	item.get()->GetRectRef().SetWidth(0);
	item.get()->GetRectRef().SetHeight(0);

	// Add this item to the list
	info.GetMenuItemListRef().Add(item.get());
	item.release();
	
	return OpStatus::OK;
}

void SelectPopMenuItemWithTitle(void *popmenu, const OpStringC& item_text)
{
	// Find the item with the matching text
	int index = [(NSPopUpButton *)popmenu indexOfItemWithTitle:[NSString stringWithCharacters:(const unichar *)item_text.CStr() length:item_text.Length()]];
	
	// Select the item
	[(NSPopUpButton *)popmenu selectItemAtIndex:index];
	// Perform the action (i.e. call the callback)
	[[(NSPopUpButton *)popmenu menu] performActionForItemAtIndex:index];
	// Close the pop-menu
	PressScopeKey(OP_KEY_ESCAPE, SHIFTKEY_NONE);
}
