/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/display/vis_dev.h"
#include "modules/inputmanager/inputmanager.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/pi/OpScreenInfo.h"

#include "adjunct/desktop_util/actions/delayed_action.h"
#include "adjunct/desktop_util/rtl/uidirection.h"
#include "adjunct/desktop_pi/desktop_menu_context.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "adjunct/quick/managers/SpeedDialManager.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/quick/widgets/OpAddressDropDown.h"
#include "adjunct/quick/WindowCommanderProxy.h"
#include "adjunct/quick/managers/DesktopBookmarkManager.h"

#include "platforms/mac/quick_support/CocoaInternalQuickMenu.h"
#include "platforms/mac/quick_support/OperaNSMenuItem.h"
#include "platforms/mac/util/CTextConverter.h"
#include "platforms/mac/pi/CocoaOpWindow.h"
#include "platforms/mac/quick_support/CocoaQuickSupport.h"
#include "platforms/mac/embrowser/EmBrowserQuick.h"
#include "platforms/mac/util/macutils.h"
#include "platforms/mac/util/MachOCompatibility.h"
#include "platforms/mac/model/CocoaOperaListener.h"
#include "platforms/mac/model/OperaNSWindow.h"
#include "platforms/mac/model/OperaNSView.h"
#include "platforms/mac/util/systemcapabilities.h"

#define BOOL NSBOOL
#import <AppKit/AppKit.h>
#import <AppKit/NSMenuItem.h>
#import <Foundation/NSDate.h>
#undef BOOL 

extern action_handler *g_delayed_action_handler;

int gPopupAppleMenusOpen = 0;

namespace {

	// Purpose of this function is to check for all
	// menus which can have extension included.
	// Since Apple has weird technique of rebuilding menu
	// so we need to check eventual need of adding
	// extensions to menu.
	bool HandleAsSubmenu(const char* name)
	{
		return op_strcmp("Hotclick Popup Menu", name) &&
		       op_strcmp("Document Popup Menu", name) &&
		       op_strcmp("Link Popup Menu", name) &&
		       op_strcmp("Video Popup Menu", name) &&
		       op_strcmp("Audio Popup Menu", name) &&
		       op_strcmp("Image Link Popup Menu", name) &&
		       op_strcmp("Image Popup Menu", name);
	}
} // namespace

@interface CocoaInternalQuickMenuDelegate : NSObject <NSMenuDelegate>
{
	CocoaInternalQuickMenu *_internal_menu;
}

- (id)initWithMenu:(CocoaInternalQuickMenu*)menu;
- (void)menuNeedsUpdate:(NSMenu*)menu;
- (void)menuClicked:(id)aSender;
- (void)menu:(NSMenu *)menu willHighlightItem:(NSMenuItem *)item;
- (void)menuDidClose:(NSMenu*)menu;
- (void)menuWillOpen:(NSMenu*)menu;

@end

@implementation CocoaInternalQuickMenuDelegate

- (id)initWithMenu:(CocoaInternalQuickMenu*)menu
{
	[super init];
	_internal_menu = menu;
	return self;
}

- (void)menuNeedsUpdate:(NSMenu*)menu
{
	if (_internal_menu)
		_internal_menu->RebuildMenu();
}
- (void)menuClicked:(id)aSender
{
	OperaNSMenuItem *item = aSender;

	// DSK-293909: Bring Opera to front first
	[[NSApplication sharedApplication] activateIgnoringOtherApps:YES];

	switch ([item getAction]->GetAction()) {
		case OpInputAction::ACTION_NEW_PAGE:
			// DSK-314374: Also check if we need to deminiaturize a window
			CocoaOpWindow::DeminiaturizeAnyWindow();
			break;
		case OpInputAction::ACTION_ADD_TO_STARTPAGE:
		{
			DocumentDesktopWindow* ddw = NULL;
			const uni_char* doc_url = NULL;

			if (g_application->GetActiveBrowserDesktopWindow() &&
				(ddw = g_application->GetActiveBrowserDesktopWindow()->GetActiveDocumentDesktopWindow()) != NULL &&
				(doc_url = ddw->GetWindowCommander()->GetCurrentURL(TRUE)) != NULL)
			{
				SpeedDialData sd_data;
				RETURN_VOID_IF_ERROR(sd_data.SetURL(doc_url));
				RETURN_VOID_IF_ERROR(sd_data.SetTitle(ddw->GetWindowCommander()->GetCurrentTitle(), FALSE));
				RETURN_VOID_IF_ERROR(g_speeddial_manager->InsertSpeedDial(g_speeddial_manager->GetTotalCount() - 1, &sd_data));

				OpAddressDropDown* add = ddw->GetAddressDropdown();
				if (add)
					add->AnimateBookmarkNotification(true, true);
			}
			return;
		}
		case OpInputAction::ACTION_REMOVE_FROM_STARTPAGE:
		{
			DocumentDesktopWindow* ddw = NULL;
			const uni_char* doc_url = NULL;
		
			if (g_application->GetActiveBrowserDesktopWindow() &&
				(ddw = g_application->GetActiveBrowserDesktopWindow()->GetActiveDocumentDesktopWindow()) != NULL &&
				(doc_url = ddw->GetWindowCommander()->GetCurrentURL(TRUE)) != NULL)
			{
				INT32 pos = g_speeddial_manager->FindSpeedDialByUrl(doc_url, true);
				if (pos != -1)
					g_speeddial_manager->RemoveSpeedDial(pos);

				OpAddressDropDown* add = ddw->GetAddressDropdown();
				if (add)
					add->AnimateBookmarkNotification(true, false);
			}
			return;
		}
		case OpInputAction::ACTION_ADD_TO_FAVORITES:
		{
			DocumentDesktopWindow* ddw = NULL;
			if (g_application->GetActiveBrowserDesktopWindow() &&
				(ddw = g_application->GetActiveBrowserDesktopWindow()->GetActiveDocumentDesktopWindow()) != NULL)
			{
				INT32 item_id = -1;
				BookmarkItemData item_data;
				WindowCommanderProxy::AddToBookmarks(ddw->GetWindowCommander(), item_data, 0, item_id);
				g_desktop_bookmark_manager->NewBookmark(item_data, 0);

				OpAddressDropDown* add = ddw->GetAddressDropdown();
				if (add)
					add->AnimateBookmarkNotification(true, true);
			}
			return;

		}
		case OpInputAction::ACTION_REMOVE_FROM_FAVORITES:
		{
			DocumentDesktopWindow* ddw = NULL;
			const uni_char *doc_url = NULL;
			HotlistModelItem* item = NULL;

			if (g_application->GetActiveBrowserDesktopWindow() &&
				(ddw = g_application->GetActiveBrowserDesktopWindow()->GetActiveDocumentDesktopWindow()) != NULL &&
				(doc_url = ddw->GetWindowCommander()->GetCurrentURL(TRUE)) != NULL &&
				(item = g_hotlist_manager->GetBookmarksModel()->GetByURL(doc_url, false, true)) != NULL)
			{
				BookmarkModel* model = g_hotlist_manager->GetBookmarksModel();
				if (!model->DeleteItem(item, TRUE))
					return;

				OpAddressDropDown* add = ddw->GetAddressDropdown();
				if (add)
					add->AnimateBookmarkNotification(true, false);
			}
			return;
		}
	}

	// Set the position of the mouse and invoke the action
	[item getAction]->SetActionPosition(*_internal_menu->GetContextMousePos());
	[item getAction]->SetActionMethod(_internal_menu->IsKeyboardContext() ? OpInputAction::METHOD_KEYBOARD : OpInputAction::METHOD_MENU);
	if (g_delayed_action_handler)
	{
		g_delayed_action_handler->action = [item getAction];
		g_delayed_action_handler->context = _internal_menu->IsKeyboardContext() ? g_input_manager->GetKeyboardInputContext() : g_input_manager->GetMouseInputContext();
	}
	else
	{
		g_input_manager->InvokeAction([item getAction], _internal_menu->IsKeyboardContext() ? g_input_manager->GetKeyboardInputContext() : g_input_manager->GetMouseInputContext());
	}
}

- (void)menu:(NSMenu *)menu willHighlightItem:(NSMenuItem *)item
{
	SendMenuHighlightToScope((CFStringRef)[item title], FALSE, [item hasSubmenu]);
}

- (void)menuDidClose:(NSMenu*)menu
{
	SendMenuTrackingToScope(--gPopupAppleMenusOpen > 0);

	// If there is no parent set the active menu back to the main menu
	if (![menu supermenu])
		SendSetActiveNSMenu((void *)[NSApp mainMenu]);
	else
		SendSetActiveNSMenu((void *)[menu supermenu]);

	OpScopeDesktopWindowManager_SI::QuickMenuInfo info;
	SetQuickInternalMenuInfo(menu, info);
	g_application->GetMenuHandler()->OnMenuShown(FALSE, info);
}

- (void)menuWillOpen:(NSMenu*)menu
{
	SendMenuTrackingToScope(++gPopupAppleMenusOpen > 0);
	SendSetActiveNSMenu((void *)menu);

	OpScopeDesktopWindowManager_SI::QuickMenuInfo info;
	SetQuickInternalMenuInfo(menu, info);
	g_application->GetMenuHandler()->OnMenuShown(TRUE, info);
}

@end

@interface NSMenu (PopUpRegularMenuAdditions)
+ (void)popUpMenu:(NSMenu *)menu atPoint:(NSPoint)point inView:(NSView *)view;
@end

@implementation NSMenu (PopUpRegularMenuAdditions)
+ (void)popUpMenu:(NSMenu *)menu atPoint:(NSPoint)point inView:(NSView *)view
{
	CocoaOperaListener tempListener(TRUE);
	
	point.y = MAX(1.0,point.y);

	// Super hack but if you make a fake event you can send that into the popUpContextMenu function.
	// This has the advantage that you don't need extra menu items for NSPopUpButtonCell and
	// the positioning of the menu is totally controlled by the system!
	NSEvent* fake = [NSEvent mouseEventWithType:NSRightMouseDown
						location:point
						modifierFlags:0 timestamp:[NSDate timeIntervalSinceReferenceDate]
						windowNumber:[[view window] windowNumber]
						context:[[view window] graphicsContext]
						eventNumber:0
						clickCount:1
						pressure:0.0];

	if (GetOSVersion() >= 0x1060)
	{
		// Disable all extra services entries that the OS wants to add to the menu.
		[menu setAllowsContextMenuPlugIns:FALSE];
	}

	// Tell input manager and screen that the middle button (and others) have been released (DSK-312795)
	[static_cast<OperaNSView*>(view) releaseOtherMouseButtons:fake];

	[NSMenu popUpContextMenu:menu withEvent:fake forView:view];
}
@end

Boolean ParseShortcut(const uni_char *shortcut, uni_char &theChar, UInt32 &theMods, SInt16 &theGlyph);

CocoaInternalQuickMenu::CocoaInternalQuickMenu(Boolean use_keyboard_context, const char* popup_name, INTPTR menu_value, INT32 start_at_index)
	: mMenu(NULL),
	mMenuID(0),
	itemCount(0),
	mKeyboardContext(use_keyboard_context),
	mIgnoreNextIfSeperator(true),
	mShowIconsInMenu(false)
{
#ifdef USE_SKIN_ICONS_IN_MENUS
	if ( (op_strcmp(popup_name, "Browser Bookmarks Menu") == 0) ||
				(op_strcmp(popup_name, "Bookmark Folder Menu") == 0) ||
				(op_strcmp(popup_name, "Internal Popular List") == 0) ||
				(op_strcmp(popup_name, "Internal Back History") == 0) ||
				(op_strcmp(popup_name, "Insert Smiley Menu") == 0) ||
				(op_strcmp(popup_name, "Open in menu") == 0) ||
				(op_strcmp(popup_name, "Label Image Menu") == 0) ||
				(op_strcmp(popup_name, "Page Sharing Menu") == 0) ||
				(op_strncmp(popup_name, "document-context-menu-", 22) == 0))
	{
		mShowIconsInMenu = true;
	}
#endif // USE_SKIN_ICONS_IN_MENUS
	mName.Set(popup_name);

	mMenuValue = menu_value;

	mMenu = [[NSMenu alloc] initWithTitle:[NSString stringWithUTF8String:popup_name]];
	[(NSMenu *)mMenu setDelegate:[[CocoaInternalQuickMenuDelegate alloc] initWithMenu:this]];

	[(NSMenu *)mMenu setAutoenablesItems:NO];
}

CocoaInternalQuickMenu::~CocoaInternalQuickMenu()
{
	[[(NSMenu*)mMenu delegate] release];
	[(NSMenu*)mMenu release];
} 

void CocoaInternalQuickMenu::Show(const PopupPlacement& placement)
{
	if (!(itemCount + [(NSMenu*)mMenu numberOfItems]))
		return;

	DesktopWindow *active_desktop_window = g_application->GetActiveDesktopWindow();
	if (!active_desktop_window)
		return;

	CocoaOpWindow *cocoa_window = (CocoaOpWindow *)active_desktop_window->GetOpWindow()->GetRootWindow();
	if (!cocoa_window)
		return;

	NSPoint screen_cursor_pt = [NSEvent mouseLocation];
	NSPoint cursor_pt = [[cocoa_window->GetContentView() window] convertScreenToBase:screen_cursor_pt];
	m_context_menu_mousepos = OpPoint(cursor_pt.x, cursor_pt.y);
	NSScreen* menu_screen = [[NSScreen screens] objectAtIndex:0];

	NSPoint pt = NSMakePoint(0, 0);
	switch (placement.type)
	{
		case PopupPlacement::POPUP_WINDOW_CENTER: // Centre of application
			pt.x = [[cocoa_window->GetContentView() window] frame].size.width  / 2;
			pt.y = [[cocoa_window->GetContentView() window] frame].size.height / 2;
			break;

		case PopupPlacement::POPUP_CURSOR:
			pt = cursor_pt;
			if (UiDirection::Get() == UiDirection::RTL)
			{
				const int menu_width = [(NSMenu *)mMenu size].width;
				if (screen_cursor_pt.x - menu_width >= 0)
					pt.x -= menu_width;
			}
			break;

		case PopupPlacement::POPUP_CENTER:
		case PopupPlacement::POPUP_BELOW:
		{
			OpPoint p;
			if (UiDirection::Get() == UiDirection::LTR)
			{
				p = placement.rect.BottomLeft();
			}
			else
			{
				p = placement.rect.BottomRight();
				p.x -= [(NSMenu *)mMenu size].width;
				if (p.x < 0)
					p.x = placement.rect.BottomLeft().x;
			}
			pt.x = p.x;
			// Always adjust the coords from the menu screen where the origins are
			pt.y = [menu_screen frame].size.height - p.y;
			pt = [[cocoa_window->GetContentView() window] convertScreenToBase:pt];
			break;
		}

		case PopupPlacement::POPUP_RIGHT:
		{
			OpPoint p;
			if (UiDirection::Get() == UiDirection::LTR)
			{
				p = placement.rect.TopRight();
			}
			else
			{
				p = placement.rect.TopLeft();
				p.x -= [(NSMenu *)mMenu size].width;
				if (p.x < 0)
					p.x = placement.rect.TopRight().x;
			}
			pt.x = p.x;
			// Always adjust the coords from the menu screen where the origins are
			pt.y = [menu_screen frame].size.height - p.y;
			pt = [[cocoa_window->GetContentView() window] convertScreenToBase:pt];
			break;
		}

		default:
			OP_ASSERT(!"Unhandled placement type");
	}

	g_input_manager->ResetInput();

	if (UiDirection::Get() == UiDirection::RTL)
	{
		// Need this for pixel-perfect alignment with cursor / anchor.
		pt.x -= 1;

		// Prevent the OS from reflecting the menu at the right screen edge
		// when the menu is too close to the edge.
		NSPoint screen_right = NSMakePoint([menu_screen frame].origin.x + [menu_screen frame].size.width, 0);
		screen_right = [[cocoa_window->GetContentView() window] convertScreenToBase:screen_right];
		pt.x = MIN(pt.x, screen_right.x - [(NSMenu *)mMenu size].width - 7);
	}

	// Popup the menu!
	[NSMenu popUpMenu:(NSMenu *)mMenu atPoint:pt inView:cocoa_window->GetContentView()];
}

void CocoaInternalQuickMenu::AddItem(BOOL seperator, const uni_char* item_name, OpWidgetImage* image, const uni_char* shortcut, MacQuickMenu *submenu, BOOL checked, BOOL selected, BOOL disabled, OpInputAction* input_action)
{
	if(!seperator && !item_name)
	{
		return;
	}

	if (seperator && mIgnoreNextIfSeperator)
		return;
	mIgnoreNextIfSeperator = seperator;

	NSMenuItem *item = nil;

	if (seperator)
	{
		item = [NSMenuItem separatorItem];
	}
	else
	{
		item = [[[OperaNSMenuItem alloc] initWithAction:input_action] autorelease];
	}

	if (item_name)
	{
		[item setTitle:[NSString stringWithCharacters:(const unichar *)item_name length:uni_strlen(item_name)]];
	}

	[(NSMenu*)mMenu addItem:item];

	itemCount++;

	if (submenu)
		[(NSMenuItem*)item setSubmenu:(NSMenu*)submenu->GetMenu()];
	else
	{
		// Set action
		[item setTarget:[((NSMenu *)mMenu) delegate]];
		[item setAction:@selector(menuClicked:)];
	}

	if (checked || selected)
		[item setState:NSOnState];

	[item setEnabled:!disabled];

#ifdef USE_SKIN_ICONS_IN_MENUS
	if (mShowIconsInMenu && image)
	{
		NSImage *nsimage = (NSImage *)GetNSImageFromOpWidgetImage(image);
		if (nsimage)
			[item setImage:nsimage];
	}
#endif // USE_SKIN_ICONS_IN_MENUS
}

void CocoaInternalQuickMenu::RebuildMenu()
{
	// Clean out all menu items
	while ([((NSMenu *)mMenu) numberOfItems])
		[((NSMenu *)mMenu) removeItemAtIndex:0], itemCount--;
		

	// Add items
	g_application->GetMenuHandler()->AddMenuItems(this, mName.CStr(), mMenuValue, 0, IsKeyboardContext() ? g_input_manager->GetKeyboardInputContext() : g_input_manager->GetMouseInputContext(), HandleAsSubmenu(mName.CStr()));
}

// Translates a single menu item based on title
OP_STATUS LocaliseMenu(const uni_char *app_name, NSMenu *opera_menu, int tag, Str::LocaleString num)
{
	// Find the menu item
	NSMenuItem *menu_item = [opera_menu itemWithTag:tag];
	if (menu_item)
	{
		OpString translated_string, menu_title;
		RETURN_IF_ERROR(g_languageManager->GetString(num, translated_string));

		if (!translated_string.IsEmpty())
		{
			if (uni_strstr(translated_string.CStr(), UNI_L("%s")))
				menu_title.AppendFormat(translated_string.CStr(), app_name);
			else 
				menu_title.Set(translated_string);
			
			[menu_item setTitle:[NSString stringWithCharacters:(const unichar *)menu_title.CStr() length:menu_title.Length()]];
		}
	}

	return OpStatus::OK;
}

// Localises the Hardcoded strings in the Opera menu like "Preferences..." etc 

OP_STATUS LocaliseOperaMenu()
{
	NSMenu *opera_menu = [[[[NSApplication sharedApplication] mainMenu] itemAtIndex:0] submenu];

	NSDictionary *infoDict = [[NSBundle mainBundle] infoDictionary];
	NSString *appName  = [infoDict valueForKey:@"CFBundleDisplayName"];
	if (appName == nil)
		appName = @"Opera";
	
	OpString app_name;
	if (SetOpString(app_name, (CFStringRef)appName))
	{
		RETURN_IF_ERROR(LocaliseMenu(app_name.CStr(), opera_menu, kHICommandAbout,			Str::M_OPERA_MAC_ABOUT_OPERA));
		RETURN_IF_ERROR(LocaliseMenu(app_name.CStr(), opera_menu, kHICommandPreferences,	Str::M_OPERA_MAC_PREFERENCES));
		RETURN_IF_ERROR(LocaliseMenu(app_name.CStr(), opera_menu, emOperaCommandServices,	Str::M_OPERA_MAC_SERVICES));
		RETURN_IF_ERROR(LocaliseMenu(app_name.CStr(), opera_menu, kHICommandHide,			Str::M_OPERA_MAC_HIDE_OPERA));
		RETURN_IF_ERROR(LocaliseMenu(app_name.CStr(), opera_menu, kHICommandHideOthers,		Str::M_OPERA_MAC_HIDE_OTHERS));
		RETURN_IF_ERROR(LocaliseMenu(app_name.CStr(), opera_menu, kHICommandShowAll,		Str::M_OPERA_MAC_SHOW_ALL));
		RETURN_IF_ERROR(LocaliseMenu(app_name.CStr(), opera_menu, kHICommandQuit,			Str::M_OPERA_MAC_QUIT_OPERA));
	}
	
	return OpStatus::OK;
}

void DisableMenuShortcut(int tag)
{
	NSMenu *opera_menu = [[[[NSApplication sharedApplication] mainMenu] itemAtIndex:0] submenu];
	if (opera_menu)
	{
		NSMenuItem *menu_item = [opera_menu itemWithTag:tag];
		
		if (menu_item)
			[menu_item setKeyEquivalent:@""];
	}
}
