/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/QuickOperaApp/CocoaQuickWidgetMenu.h"
#include "platforms/mac/QuickOperaApp/QuickWidgetLibHandler.h"
#include "platforms/mac/QuickOperaApp/QuickWidgetUnicodeUtils.h"
#include "platforms/mac/QuickOperaApp/OperaExternalNSMenu.h"
#include "platforms/mac/QuickOperaApp/OperaExternalNSMenuItem.h"
#include "platforms/mac/util/systemcapabilities.h"
#include "platforms/mac/util/MachOCompatibility.h"
#include "platforms/mac/model/OperaNSWindow.h"

#define BOOL NSBOOL
#import <AppKit/AppKit.h>
#undef BOOL 

#define RESTRICT_MENU_SHORTCUTS_WITHOUT_COMMAND_MODIFIER
#define _ALLOW_CONTROL_KEY_AND_COMMAND_KEY_IN_SHORTCUTS_

#if !defined(MAC_OS_X_VERSION_10_6) || \
	MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_6
@interface NSApplication (SnowLeopardSDKDeclarations)
- (void)setHelpMenu:(NSMenu*)helpMenu;
@end
#endif


QuickWidgetMenu* gAppleQuickMenu = NULL;
int gAppleMenusOpen = 0;

#ifndef max
# define max(a,b) (((a) > (b)) ? (a) : (b))
#endif

CocoaQuickMenuMap* QuickWidgetMenu::sMenuMap = NULL;
void* QuickWidgetMenu::sMenuPool = nil;

@interface CocoaMenuResponder : NSObject <NSMenuDelegate>
{
}

- (void)menuClicked:(id)aSender;
- (void)menuWillOpen:(NSMenu*)menu;
- (void)menuNeedsUpdate:(NSMenu*)menu;

@end

@implementation CocoaMenuResponder

- (void)menuClicked:(id)aSender
{
	OperaExternalNSMenuItem *item = aSender;
	int event_type = [[NSApp currentEvent] type];
	static bool s_recursing = false;
	// If the menu is invoked by keyboard, send the original event back to Opera to give core a change to intecept it
	// Do not try clever handling of return without command: This is used when navigating to the item by keyboard
	if ((event_type == NSKeyDown || event_type == NSKeyUp) && ([[[NSApp currentEvent] characters] characterAtIndex:0] != 13 || [[NSApp currentEvent] modifierFlags] & NSCommandKeyMask) && !s_recursing)
	{
		s_recursing = true;
		[NSApp sendEvent:[NSApp currentEvent]];
		s_recursing = false;

		// Update action state and menu as result of invoked action
		EmQuickMenuFlags flags = 0;
		EmBrowserStatus browserStatus = gWidgetLibHandler->CallGetQuickMenuCommandStatusMethod([item command], flags);
		if (browserStatus == noErr)
		{
			if (flags & emQuickMenuFlagDisabled)
			{
				[item setEnabled:NO];
			}
			else
			{
				[item setEnabled:YES];
			}
			if (flags & emQuickMenuFlagHasMark)
			{
				[item setState:NSOnState];
			}
			else
			{
				[item setState:NSOffState];
			}
		}
		[[item menu] update];
	}
	else
		gWidgetLibHandler->CallHandleQuickMenuCommandMethod([item command]);
}

- (void)menuWillOpen:(NSMenu*)menu
{
//	NSLog(@"menuWillOpen %@\n", [menu title]);
	
	QuickWidgetMenu* matchedMenu = QuickWidgetMenu::FindQuickMenu(menu);
	if (matchedMenu->GetMenuKind() == emQuickMenuKindFolder)
	{
		gWidgetLibHandler->CallRebuildQuickMenuMethod((EmQuickMenuRef)matchedMenu);
	}
	gWidgetLibHandler->CallUpdateMenuTracking(++gAppleMenusOpen > 0);

	// Main menu needs to send the higlight when it's opened
	if ([menu supermenu] == [NSApp mainMenu])
		gWidgetLibHandler->CallMenuItemHighlighted((CFStringRef)[menu title], TRUE, FALSE);

	// Set this as the active menu since it just opened
	gWidgetLibHandler->CallSetActiveNSMenu((void *)menu);
//	NSLog(@"menuWillOpen Active menu %@\n", [menu title]);

	// Call the scope event
	gWidgetLibHandler->CallOnMenuShown((void *)menu, TRUE);
}

- (void)menuDidClose:(NSMenu*)menu
{
//	NSLog(@"menuDidClose %@\n", [menu title]);
	
	gWidgetLibHandler->CallUpdateMenuTracking(--gAppleMenusOpen > 0);

	// On closure set the parent of this menu, except for the main menu
	if ([menu supermenu] != [NSApp mainMenu] || ([menu supermenu] == [NSApp mainMenu] &&  !gAppleMenusOpen))
	{
//		NSLog(@"menuDidClose Active menu %@\n", [[menu supermenu] title]);
		gWidgetLibHandler->CallSetActiveNSMenu((void *)[menu supermenu]);
	}
	
	// Call the scope event
	gWidgetLibHandler->CallOnMenuShown((void *)menu, FALSE);
}

- (void)menu:(NSMenu *)menu willHighlightItem:(NSMenuItem *)item
{
	gWidgetLibHandler->CallMenuItemHighlighted((CFStringRef)[item title], FALSE, [item hasSubmenu]);
}

- (void)menuNeedsUpdate:(NSMenu*)menu
{
	QuickWidgetMenu* matchedMenu = QuickWidgetMenu::FindQuickMenu(menu);
	if (matchedMenu)
	{
		gWidgetLibHandler->CallRebuildQuickMenuMethod((EmQuickMenuRef)matchedMenu);
		NSArray *items = [menu itemArray];
		for (unsigned int i = 0; i < [items count]; i++)
		{
			EmQuickMenuFlags flags = 0;
			EmBrowserStatus browserStatus;
			OperaExternalNSMenuItem *item = [items objectAtIndex:i];
			browserStatus = gWidgetLibHandler->CallGetQuickMenuCommandStatusMethod([item command], flags);
			if (browserStatus == noErr)
			{
				if (flags & emQuickMenuFlagDisabled)
				{
					[[items objectAtIndex:i] setEnabled:NO];
				}
				else
				{
					[[items objectAtIndex:i] setEnabled:YES];
				}
				if (flags & emQuickMenuFlagHasMark)
				{
					[[items objectAtIndex:i] setState:NSOnState];
				}
				else
				{
					[[items objectAtIndex:i] setState:NSOffState];
				}
			}
		}
	}
}

@end

CocoaMenuResponder* menuResponder = nil;

QuickWidgetMenu::QuickWidgetMenu(const UniChar * title, const char * name, EmQuickMenuKind kind) :
	mMenuID(0),
	mItemCount(0),
	mKind(kind),
	mInsertSeparator(false)
{
	mMenu = nil;
	mMenuItem = nil;
	if (menuResponder) 
		[menuResponder retain];
	else				
		menuResponder = [[CocoaMenuResponder alloc] init];
	
	// FIXME: What with 	if (kind == emQuickMenuKindHelp) ?
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	mMenu = [[OperaExternalNSMenu alloc] initWithTitle:[NSString stringWithCharacters:title length:UniStrLen(title)]];
	[(OperaExternalNSMenu *)mMenu setDelegate:menuResponder];
	[(OperaExternalNSMenu *)mMenu setAutoenablesItems:NO];
	[(OperaExternalNSMenu *)mMenu setOperaMenuName:[NSString stringWithUTF8String:name]];
	[pool release];

	if (mMenu)
	{
		CocoaQuickMenuMap* newmenu = new CocoaQuickMenuMap;
		if (newmenu)
		{
			newmenu->menuRef = mMenu;
			newmenu->quickMenu = this;
			newmenu->next = NULL;
			
			if (sMenuMap)
			{
				CocoaQuickMenuMap* nextMap = sMenuMap;
				while(nextMap->next)
				{
					nextMap = nextMap->next;
				}			
				nextMap->next = newmenu;
			}
			else
			{
				sMenuMap = newmenu;
			}
		}		
	}
}

QuickWidgetMenu::~QuickWidgetMenu()
{
	if (mMenu)
	{
		if (sMenuMap)
		{
			CocoaQuickMenuMap* current = sMenuMap;
			CocoaQuickMenuMap* previous = NULL;
			
			while (current)
			{
				if (current->next)
				{
					// the next item in list is the target
					if(current->next->menuRef == mMenu)
					{
						// save items after the to-be-deleted item
						CocoaQuickMenuMap* temp = current->next->next;
						
						delete current->next;
						
						current->next = temp;
						break;
					}
				}
				else if (current->menuRef == mMenu)
				{
					if (current == sMenuMap)
					{
						// first in list, nothing after
						delete current;
						sMenuMap = NULL;
					}
					else
					{
						// last in list, nothing after
						previous->next = NULL;
						delete current;
					}
					break;
				}
				
				previous = current;
				current = current->next;
			}
		}
	}
	
	if (mMenu)
	{
		if (mKind != emQuickMenuKindHelp && mKind != quickMenuKindApple)
		{
			int count = [(NSMenu*)mMenu numberOfItems];
			while (count > 0)
			{
				NSMenuItem *item = [(NSMenu*)mMenu itemAtIndex:0];
				bool releaseit = mKind != emQuickMenuKindHelp && ![item isSeparatorItem];
				[(NSMenu*)mMenu removeItem:item];
				if (releaseit)
					[item release];
				count--;
			}
		}
		mItemCount = 0;
		[(NSMenu*)mMenu release];
	}
	
	if ([menuResponder retainCount] == 1)
	{
		[menuResponder release];
		menuResponder = nil;
	}
	else
	{
		[menuResponder release];		
	}
}

Boolean QuickWidgetMenu::AddItemToMenu(void* menu, SInt32 after_item, const EmQuickMenuItemInfo *info, UInt32 hiMenuEquv)
{
	OperaExternalNSMenuItem *item = nil;
	
	if (info->flags & emQuickMenuFlagSeperator)
	{
		item = static_cast<OperaExternalNSMenuItem*>([OperaExternalNSMenuItem separatorItem]);
		
		[(NSMenu*)menu insertItem:item atIndex:(after_item+1)];
		
		return true;
	}
	else
	{
		item = [[OperaExternalNSMenuItem alloc] init];
	}
	
	if (info->flags & emQuickMenuFlagDisabled)
		[item setEnabled:NO];
	
	if (info->title)
	{
		NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
		[item setTitle:[NSString stringWithCharacters:info->title length:UniStrLen(info->title)]];
		[pool release];
	}
	
	// Set action
	[item setTarget:menuResponder];
	[item setAction:@selector(menuClicked:)];
	
	[(NSMenu*)menu insertItem:item atIndex:(after_item+1)];
		
	[item setTag:(hiMenuEquv ? hiMenuEquv : info->hiMenuEquv)];
	[item setCommand:info->command];

	if (info->submenu)
	{
		// FIXME: For some apparent reason - this simply don't want to compile
		((QuickWidgetMenu*)(info->submenu))->InstallAsSubMenu((void *)item);
	}
	
	switch (info->hiMenuEquv)
	{
#ifdef USE_HARDCODED_MAC_SHORTCUTS
		case kHICommandClose:
			[item setKeyEquivalent:@"W"];
			[item setKeyEquivalentModifierMask:NSCommandKeyMask];
			break;
		case kHICommandPreferences:
			[item setKeyEquivalent:@","];
			[item setKeyEquivalentModifierMask:NSCommandKeyMask];
			break;
#endif
		default:
			if (info->charCode
#ifdef RESTRICT_MENU_SHORTCUTS_WITHOUT_COMMAND_MODIFIER
				&& ((info->modifers & cmdKey)
#ifdef _ALLOW_CONTROL_KEY_AND_COMMAND_KEY_IN_SHORTCUTS_
					|| (info->modifers & controlKey)
#endif
					)
#endif //RESTRICT_MENU_SHORTCUTS_WITHOUT_COMMAND_MODIFIER
				)
			{
				[item setKeyEquivalent:[NSString stringWithFormat:@"%c", tolower(info->charCode)]];
				[item setKeyEquivalentModifierMask:NSCommandKeyMask];
				if (info->modifers & shiftKey)
					[item setKeyEquivalentModifierMask:[item keyEquivalentModifierMask] | NSShiftKeyMask];
				if (info->modifers & optionKey)
					[item setKeyEquivalentModifierMask:[item keyEquivalentModifierMask] | NSAlternateKeyMask];
#ifdef _ALLOW_CONTROL_KEY_AND_COMMAND_KEY_IN_SHORTCUTS_
				if (info->modifers & controlKey)
					[item setKeyEquivalentModifierMask:[item keyEquivalentModifierMask] | NSControlKeyMask];
#endif
				if (!(info->modifers & cmdKey))
					[item setKeyEquivalentModifierMask:[item keyEquivalentModifierMask] | NSCommandKeyMask];
			}
			else if (info->glyph)
			{
				const unichar c = info->glyph;
				NSString *s = [[NSString alloc] initWithCharacters:&c length:1];
				[item setKeyEquivalent:s];
				[s release];				
				
				[item setKeyEquivalentModifierMask:0];
				if (info->modifers & shiftKey)
					[item setKeyEquivalentModifierMask:[item keyEquivalentModifierMask] | NSShiftKeyMask];
				if (info->modifers & optionKey)
					[item setKeyEquivalentModifierMask:[item keyEquivalentModifierMask] | NSAlternateKeyMask];
#ifdef _ALLOW_CONTROL_KEY_AND_COMMAND_KEY_IN_SHORTCUTS_
				if (info->modifers & controlKey)
					[item setKeyEquivalentModifierMask:[item keyEquivalentModifierMask] | NSControlKeyMask];
#endif
				if (info->modifers & cmdKey)
					[item setKeyEquivalentModifierMask:[item keyEquivalentModifierMask] | NSCommandKeyMask];
			} 
			break;
	}
		
	if (info->icon)
	{
		[item setImage:(NSImage *)info->icon];
	}

	if (info->flags & emQuickMenuFlagHasMark)
	{
		[item setState:NSOnState];
	}
	
	return true;
}

Boolean QuickWidgetMenu::AddItem(SInt32 after_item, const EmQuickMenuItemInfo *info)
{
	if (!mMenu)
		return true;

	OperaExternalNSMenuItem* item = nil;
	if ((info->hiMenuEquv == kHICommandAbout || info->hiMenuEquv == emOperaCommandCheckForUpdate || 
		 info->hiMenuEquv == emOperaCommandRegister) && gAppleQuickMenu && (gAppleQuickMenu != this))
	{
		if ((item = (OperaExternalNSMenuItem*)[[[[[NSApplication sharedApplication] mainMenu] itemAtIndex:0] submenu] itemWithTag:info->hiMenuEquv]) != nil)
		{
			// Set all the data here as this wasn't ready when the menu item was created
			[item setTag:info->hiMenuEquv];
			[item setCommand:info->command];
			[item setTarget:menuResponder];
			[item setAction:@selector(menuClicked:)];

			// Reveal any menu that was hidden earlier
			if ([item respondsToSelector: @selector(setHidden:)])
				[item setHidden:NO];
			return true;
		}
		
		Boolean result = gAppleQuickMenu->AddItem(0, info);
		
		// insert a separator between the register menu item and the preferences menu items
		[(NSMenu *)gAppleQuickMenu->mMenu insertItem:[OperaExternalNSMenuItem separatorItem]  atIndex:gAppleQuickMenu->mItemCount];
		
		return result;
	}

	if (info->hiMenuEquv == kHICommandQuit)
	{
		// Find the Quit menu item
		if ((item = (OperaExternalNSMenuItem*)[[[[[NSApplication sharedApplication] mainMenu] itemAtIndex:0] submenu] itemWithTag:kHICommandQuit]) != nil)
		{
			[item setTag:info->hiMenuEquv];
			[item setCommand:info->command];
			[item setTarget:menuResponder];
			[item setAction:@selector(menuClicked:)];
		}
		return true;	
	}
	if (info->hiMenuEquv == kHICommandPreferences)
	{
		if ((item = (OperaExternalNSMenuItem*)[[[[[NSApplication sharedApplication] mainMenu] itemAtIndex:0] submenu] itemWithTag:kHICommandPreferences]) != nil)
		{
			[item setTag:info->hiMenuEquv];
			[item setCommand:info->command];
			if ([item respondsToSelector: @selector(setHidden:)])
				[item setHidden:NO];
#ifdef USE_HARDCODED_MAC_SHORTCUTS
			[item setKeyEquivalent:@","];
			[item setKeyEquivalentModifierMask:NSCommandKeyMask];
#else
			if (info->charCode && ((info->modifers & cmdKey)
#ifdef _ALLOW_CONTROL_KEY_AND_COMMAND_KEY_IN_SHORTCUTS_
								   || (info->modifers & controlKey)
#endif
								   ))
			{
				[item setKeyEquivalent:[NSString stringWithFormat:@"%c", tolower(info->charCode)]];
				[item setKeyEquivalentModifierMask:NSCommandKeyMask];
				if (info->modifers & shiftKey)
					[item setKeyEquivalentModifierMask:[item keyEquivalentModifierMask] | NSShiftKeyMask];
				if (info->modifers & optionKey)
					[item setKeyEquivalentModifierMask:[item keyEquivalentModifierMask] | NSAlternateKeyMask];
#ifdef _ALLOW_CONTROL_KEY_AND_COMMAND_KEY_IN_SHORTCUTS_
				if (info->modifers & controlKey)
					[item setKeyEquivalentModifierMask:[item keyEquivalentModifierMask] | NSControlKeyMask];
#endif
				if (!(info->modifers & cmdKey))
					[item setKeyEquivalentModifierMask:[item keyEquivalentModifierMask] | NSCommandKeyMask];
			}
			else if (info->glyph)
			{
				const unichar c = info->glyph;
				NSString *s = [[NSString alloc] initWithCharacters:&c length:1];
				[item setKeyEquivalent:s];
				[s release];
				
				[item setKeyEquivalentModifierMask:0];
				if (info->modifers & shiftKey)
					[item setKeyEquivalentModifierMask:[item keyEquivalentModifierMask] | NSShiftKeyMask];
				if (info->modifers & optionKey)
					[item setKeyEquivalentModifierMask:[item keyEquivalentModifierMask] | NSAlternateKeyMask];
#ifdef _ALLOW_CONTROL_KEY_AND_COMMAND_KEY_IN_SHORTCUTS_
				if (info->modifers & controlKey)
					[item setKeyEquivalentModifierMask:[item keyEquivalentModifierMask] | NSControlKeyMask];
#endif
				if (info->modifers & cmdKey)
					[item setKeyEquivalentModifierMask:[item keyEquivalentModifierMask] | NSCommandKeyMask];
				
			}

#endif

		}
			
		return true;
	}
	
	if (info->submenu && ((QuickWidgetMenu*)info->submenu)->GetMenuKind() == emQuickMenuKindQuickPrefs)
	{
		NSMenu *opera_menu = [[[[NSApplication sharedApplication] mainMenu] itemAtIndex:0] submenu];

		OperaExternalNSMenuItem* item = nil;
		if ((item = (OperaExternalNSMenuItem*)[[[[[NSApplication sharedApplication] mainMenu] itemAtIndex:0] submenu] itemWithTag:emOperaCommandQuickPreferences]) != nil)
		{
			[item release];
			[opera_menu removeItem:item];
		}
		
		OperaExternalNSMenuItem *prefs_item = (OperaExternalNSMenuItem*)[opera_menu itemWithTag:kHICommandPreferences];
		if (prefs_item)
		{
			// Get the position of the "Peferences..." entry
			int itemPos = [opera_menu indexOfItem:prefs_item];
			
			// Add Quick prefs right before "Preferences..." forcing the hiMenu so it can be found next time
			AddItemToMenu(opera_menu, itemPos-1, info, (UInt32)emOperaCommandQuickPreferences);
			return true;
		}
	}
	
	if (after_item < 0)
		after_item = mItemCount - 1;

	// Is this a separator, then add it when we add an actual item
	// This solves the double separator problem and hanging separators
	// that can be left at the bottom of menus
	if (info->flags & emQuickMenuFlagSeperator)
	{
		mInsertSeparator = true;
		return false;
	}
	else
	{
		if (mInsertSeparator)
		{
			EmQuickMenuItemInfo sep_info;
			sep_info.flags = emQuickMenuFlagSeperator;
			
			if (AddItemToMenu(mMenu, after_item, &sep_info))
			{
				++mItemCount;
				after_item++;
			}
			mInsertSeparator = false;
		}

		if (AddItemToMenu(mMenu, after_item, info))
			++mItemCount;
	}
 	if (info->hiMenuEquv == kHICommandAppHelp)
	{
		if ([NSApp respondsToSelector:@selector(setHelpMenu:)])
			[NSApp setHelpMenu:static_cast<NSMenu*>(mMenu)];
	}

	return true;
}

Boolean QuickWidgetMenu::RemoveItem(SInt32 item)
{
	return true;
}

Boolean QuickWidgetMenu::RemoveAllItems()
{
	// Somewhere in the code the menu is reused so we need to reset this
	// when all the items are removed
	mInsertSeparator = false;
	
	if (mKind == quickMenuKindApple)
	{
		return true;
	}
	
	int count = [(NSMenu*)mMenu numberOfItems];
	while (count > 0)
	{
		NSMenuItem *item = [(NSMenu*)mMenu itemAtIndex:0];
		bool releaseit = mKind != emQuickMenuKindHelp && ![item isSeparatorItem];
		[(NSMenu*)mMenu removeItem:item];
		if (releaseit)
			[item release];
		count--;
	}
	mItemCount = 0;

	return true;
}

Boolean QuickWidgetMenu::InsertInMenubar(QuickWidgetMenu *beforeMenu)
{
	int beforeMenuID = 0;

	if (beforeMenu)
	{
		beforeMenuID = beforeMenu->GetMenuID();
	}
	else
	{
		if ([[[NSApplication sharedApplication] mainMenu] numberOfItems] > 0)
		{
			beforeMenuID = [[[NSApplication sharedApplication] mainMenu] numberOfItems];
		}
	}

	if (mMenu)
	{
		if (mMenuItem)
			[(OperaExternalNSMenuItem*)mMenuItem release];
		OperaExternalNSMenuItem *item = [[OperaExternalNSMenuItem alloc] initWithTitle:[(NSMenu*)mMenu title] action:NULL keyEquivalent:@""];
		[item setSubmenu:(NSMenu*)mMenu];
		NSMenu *mainMenu = [[NSApplication sharedApplication] mainMenu];
		if (mainMenu == nil)
		{
			mainMenu = [[NSMenu alloc] init];
			[[NSApplication sharedApplication] setMainMenu:mainMenu];
			[mainMenu release];
		}
		[[[NSApplication sharedApplication] mainMenu] insertItem:item atIndex:beforeMenuID];
		mMenuItem = item;
	}

	return true;
}

Boolean QuickWidgetMenu::RemoveFromMenubar()
{
	if (mMenuItem)
	{
		[(NSMenu*)[[NSApplication sharedApplication] mainMenu] removeItem:(OperaExternalNSMenuItem *)mMenuItem];
		[(NSMenuItem*)mMenuItem release];
	}

	return true;
}

short QuickWidgetMenu::GetMenuID()
{
	return [[[NSApplication sharedApplication] mainMenu] indexOfItem:(OperaExternalNSMenuItem *)mMenuItem];
}

void QuickWidgetMenu::InstallAsSubMenu(void *parentMenu)
{
	if (mMenu)
	{
		[(OperaExternalNSMenuItem*)parentMenu setSubmenu:(NSMenu*)mMenu];
	}
}

Boolean QuickWidgetMenu::Refresh()
{
	if (mMenu)
	{
		[(NSMenu*)mMenu update];
	}

	return true;
}

QuickWidgetMenu* QuickWidgetMenu::FindQuickMenu(void* inMenuRef)
{
	QuickWidgetMenu* result = NULL;
	CocoaQuickMenuMap* current = sMenuMap;
	
	if (inMenuRef)
	{
		while (current)
		{
			if (current->menuRef == inMenuRef)
			{
				result = current->quickMenu;
				break;
			}
			
			current = current->next;
		}
	}
	
	return result;
}

