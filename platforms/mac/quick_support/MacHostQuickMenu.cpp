/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/Application.h"
#include "platforms/mac/quick_support/MacHostQuickMenu.h"
#include "platforms/mac/quick_support/CocoaInternalQuickMenu.h"
#include "platforms/mac/quick_support/MacApplicationListener.h"
#include "modules/inputmanager/inputmanager.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/display/vis_dev.h"
#include "modules/util/opfile/opfile.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "platforms/mac/util/MachOCompatibility.h"
#include "adjunct/quick/managers/DesktopBookmarkManager.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "platforms/mac/quick_support/CocoaQuickSupport.h"

//#define USE_HARDCODED_MAC_SHORTCUTS
//#define RESTRICT_MENU_SHORTCUTS_WITHOUT_COMMAND_MODIFIER

extern EmQuickBrowserAppProcs *gQuickAppMethods;

Boolean ParseShortcut(const uni_char *shortcut, uni_char &theChar, UInt32 &theMods, SInt16 &theGlyph)
{
	const uni_char *end;
	int len;
	theChar = 0;
	theMods = 0;
	theGlyph = 0;
	end = uni_strchr(shortcut, UNI_L('+'));

	static OpString meta;
	static OpString ctrl;
	static OpString alt;
	static OpString shift;

	if(alt.Length() == 0) {
		TRAPD(err, g_languageManager->GetStringL(Str::M_KEY_ALT, alt));
	}
	if(meta.Length() == 0) {
		TRAPD(err, g_languageManager->GetStringL(Str::M_KEY_META, meta));
	}
	if(ctrl.Length() == 0) {
		TRAPD(err, g_languageManager->GetStringL(Str::M_KEY_CTRL, ctrl));
	}
	if(shift.Length() == 0) {
		TRAPD(err, g_languageManager->GetStringL(Str::M_KEY_SHIFT, shift));
	}

	if (meta.Length() == 0 ||
		ctrl.Length() == 0 ||
		alt.Length() == 0 ||
		shift.Length() == 0)
	{
		return false;
	}

	while (end)
	{
		len = (end - shortcut);
		if ((len == alt.Length()) && (uni_strnicmp(shortcut, alt.CStr(), len) == 0))
			theMods |= optionKey;
#ifdef _ALLOW_CONTROL_KEY_AND_COMMAND_KEY_IN_SHORTCUTS_
		else if ((len == meta.Length()) && (uni_strnicmp(shortcut, meta.CStr(), len) == 0))
			theMods |= controlKey;
#endif
		else if ((len == ctrl.Length()) && (uni_strnicmp(shortcut, ctrl.CStr(), len) == 0))
			theMods |= cmdKey;
		else if ((len == shift.Length()) && (uni_strnicmp(shortcut, shift.CStr(), len) == 0))
			theMods |= shiftKey;
		shortcut = end + 1;
		end = uni_strchr(shortcut, UNI_L('+'));
		if (end == shortcut)
			end = 0;
	}
	if (shortcut[0] != 0 && shortcut[1] == 0)
	{
		theChar = shortcut[0];
		return true;
	}
	else if (shortcut[0] == 'F' && shortcut[1] >= '0' && shortcut[1] <= '9' && (shortcut[2] == 0 || (shortcut[2] >= '0' && shortcut[2] <= '9' && shortcut[3] == 0)))
	{
		switch (shortcut[1])
		{
			case '1':
				switch (shortcut[2])
				{
					case '0':
						theGlyph = 63245; // NSF10FunctionKey
						return true;
					case '1':
						theGlyph = 63246; // NSF11FunctionKey
						return true;
					case '2':
						theGlyph = 63247; // NSF12FunctionKey
						return true;
					case '3':
						theGlyph = 63248; // NSF13FunctionKey
						return true;
					case '4':
						theGlyph = 63249; // NSF14FunctionKey
						return true;
					case '5':
						theGlyph = 63250; // NSF15FunctionKey
						return true;
				}
				theGlyph = 63236; // NSF1FunctionKey
				return true;
			case '2':	//Enter Address
#ifdef USE_HARDCODED_MAC_SHORTCUTS
				theMods |= cmdKey;
				theChar = 'L';
#else
				theGlyph = 63237; // NSF2FunctionKey
#endif
				return true;
			case '3':	//Find again
#ifdef USE_HARDCODED_MAC_SHORTCUTS
				if ((theMods == 0) || (theMods == shiftKey))
				{
					theMods |= cmdKey;
					theChar = 'G';
				}
#else
				theGlyph = 63238; // NSF3FunctionKey
#endif
				return true;
			case '4':	//Close Window
#ifdef USE_HARDCODED_MAC_SHORTCUTS
				if ((theMods == cmdKey) || (theMods == optionKey))
				{
					theMods |= cmdKey;
					theMods ^= optionKey;
					theChar = 'W';
				}
#else
				theGlyph = 63239; // NSF4FunctionKey
#endif
				return true;
			case '5':	// Relaod
#ifdef USE_HARDCODED_MAC_SHORTCUTS
				if (!(theMods & shiftKey))
				{
					if (theMods == optionKey)
						theMods = shiftKey;
					else if (theMods == cmdKey)
						theMods = optionKey;
					theMods |= cmdKey;
					theChar = 'R';
					return true;
				}
#else
				theGlyph = 63240; // NSF5FunctionKey
				return true;
#endif
				break;
			case '6':
				theGlyph = 63241; // NSF6FunctionKey
				return true;
			case '7':
				theGlyph = 63242; // NSF7FunctionKey
				return true;
			case '8':
				theGlyph = 63243; // NSF8FunctionKey
				return true;
			case '9':
				theGlyph = 63244; // NSF9FunctionKey
				return true;

		}
	}
#if 1
	else if(uni_strstr(UNI_L("Backspace"), shortcut))
	{
		theGlyph = kMenuDeleteLeftGlyph;
	}
	else if(uni_strstr(UNI_L("Esc"), shortcut))
	{
		theGlyph = kMenuEscapeGlyph;
	}
	else if(uni_strstr(UNI_L("Insert"), shortcut))
	{
		theGlyph = kMenuHelpGlyph;
	}
	else if(uni_strstr(UNI_L("Del"), shortcut))
	{
		theGlyph = kMenuDeleteRightGlyph;
	}
	else if(uni_strstr(UNI_L("Home"), shortcut))
	{
		theGlyph = kMenuNorthwestArrowGlyph;
	}
	else if(uni_strstr(UNI_L("End"), shortcut))
	{
		theGlyph = kMenuSoutheastArrowGlyph;
	}
	else if(uni_strstr(UNI_L("PageUp"), shortcut))
	{
		theGlyph = kMenuPageUpGlyph;
	}
	else if(uni_strstr(UNI_L("PageDown"), shortcut))
	{
		theGlyph = kMenuPageDownGlyph;
	}
	else if(uni_strstr(UNI_L("Up"), shortcut) && !uni_strstr(UNI_L("PageUp"), shortcut))
	{
		theGlyph = kMenuUpArrowGlyph;
	}
	else if(uni_strstr(UNI_L("Down"), shortcut) && !uni_strstr(UNI_L("PageDown"), shortcut))
	{
		theGlyph = kMenuDownArrowGlyph;
	}
	else if(uni_strstr(UNI_L("Left"), shortcut))
	{
		theGlyph = kMenuLeftArrowGlyph;
	}
	else if(uni_strstr(UNI_L("Right"), shortcut))
	{
		theGlyph = kMenuRightArrowGlyph;
	}
#if 0
	// the space glyph is invisible...so there's no use showing it
	else if(uni_strstr(UNI_L("Space"), shortcut))
	{
		theGlyph = kMenuSpaceGlyph;
	}
#endif
	else if(uni_strstr(UNI_L("Enter"), shortcut) || uni_strstr(UNI_L("Return"), shortcut))
	{
		theGlyph = kMenuReturnGlyph;
	}
	else if(uni_strstr(UNI_L("Tab"), shortcut))
	{
		theGlyph = kMenuTabRightGlyph;
	}

	if(theGlyph)
		return true;
#endif

	return false;
}

MacHostQuickMenuBar::~MacHostQuickMenuBar()
{
}

void MacHostQuickMenuBar::AddItem(BOOL seperator, const uni_char* item_name, OpWidgetImage* image, const uni_char* shortcut, MacQuickMenu *submenu, BOOL checked, BOOL selected, BOOL disabled, OpInputAction* input_action)
{
	if (submenu)
	{
		mSubmenus.Add((MacHostQuickMenu*) submenu);
		if (((MacHostQuickMenu*)submenu)->GetEmMenuRef() && gQuickAppMethods && gQuickAppMethods->insertMenu)
		{
			gQuickAppMethods->insertMenu(((MacHostQuickMenu*)submenu)->GetEmMenuRef(), NULL);
		}
	}
}

OpInputAction *MacHostQuickMenuBar::FindAction(EmQuickMenuCommand command)
{
	MacHostQuickMenu * menu = FindMenu(command >> 16);
	if (menu)
	{
		return menu->GetAction(command & 0xFFFF);
	}
	return NULL;
}

void MacHostQuickMenuBar::ActionExecuted(EmQuickMenuCommand command)
{
	MacHostQuickMenu * menu = FindMenu(command >> 16);
	if (menu)
	{
		menu->UpdateMenu();
	}
}

MacHostQuickMenu* MacHostQuickMenuBar::FindMenu(UInt16 inMenuID)
{
	MacHostQuickMenu* child;
	int count = mSubmenus.GetCount();
	for (int i = 0; i < count; i++)
	{
		child = mSubmenus.Get(i);
		if (child)
			child = child->FindMenu(inMenuID);
		if (child)
			return child;
	}
	return NULL;
}

MacHostQuickMenu* MacHostQuickMenuBar::FindMenu(EmQuickMenuRef inMenu)
{
	MacHostQuickMenu* child;
	int count = mSubmenus.GetCount();
	for (int i = 0; i < count; i++)
	{
		child = mSubmenus.Get(i);
		if (child)
			child = child->FindMenu(inMenu);
		if (child)
			return child;
	}
	return NULL;
}


#ifdef _MAC_DEBUG
void MacHostQuickMenuBar::PrintAllMenus()
{
	MacHostQuickMenu* child;
	int count = mSubmenus.GetCount();
	fprintf(stderr, "Menubar:\n");
	for (int i = 0; i < count; i++)
	{
		child = mSubmenus.Get(i);
		if (child)
		{
			child->PrintAllMenus();
		}
	}
}

void MacHostQuickMenu::PrintAllMenus()
{
	fprintf(stderr, "%s: id=%d, em=%p.\n", mMenuName.CStr(), mMenuID, mEmMenuRef);

	MacHostQuickMenu* child;
	int count = mSubmenus.GetCount();
	for (int i = 0; i < count; i++)
	{
		child = mSubmenus.Get(i);
		if (child)
			child->PrintAllMenus();
	}
}
#endif // _MAC_DEBUG

void MacHostQuickMenuBar::RebuildMenu(EmQuickMenuRef menu)
{
	MacHostQuickMenu* theMenu = NULL;

	theMenu = FindMenu(menu);
	if(theMenu)
	{
		// DSK-311905: Let us only rebuild bookmark menus when bookmarks have actually changed on file
		if (MacApplicationListener::s_bookmarks_loaded &&(theMenu->GetMenuKind() == emQuickMenuKindBookmarks || theMenu->GetMenuKind() == emQuickMenuKindFolder))
		{
			OpFile hotlistfile;
			time_t modtime;
			TRAPD(rc, g_pcfiles->GetFileL(PrefsCollectionFiles::HotListFile, hotlistfile));
			hotlistfile.GetLastModified(modtime);
			if (modtime == theMenu->GetModTime())
				return;
			theMenu->SetModTime(modtime);
		}

		theMenu->mSubmenus.DeleteAll();
		theMenu->mActions.DeleteAll();
		gQuickAppMethods->removeMenuItems(menu);
		g_application->GetMenuHandler()->AddMenuItems(theMenu, theMenu->GetMenuName(), theMenu->mMenuValue, 0, g_input_manager->GetKeyboardInputContext());
	}
}

void MacHostQuickMenuBar::RebuildAllMenus()
{
	MacHostQuickMenu* theMenu;
	int count = mSubmenus.GetCount();
	for (int i = 0; i < count; i++)
	{
		theMenu = mSubmenus.Get(i);
		if (theMenu)
		{
			theMenu->mSubmenus.DeleteAll();
			g_application->GetMenuHandler()->AddMenuItems(theMenu, theMenu->GetMenuName(), theMenu->mMenuValue, 0, g_input_manager->GetKeyboardInputContext());
		}
	}
}

void MacHostQuickMenuBar::RemoveAllMenus()
{
	MacHostQuickMenu* theMenu;
	int count = mSubmenus.GetCount();
	for (int i = 0; i < count; i++)
	{
		theMenu = mSubmenus.Get(i);
		if (theMenu)
		{
			gQuickAppMethods->removeMenu(theMenu->GetEmMenuRef());
			theMenu->mSubmenus.DeleteAll();
		}
	}
	mSubmenus.DeleteAll();
}

#pragma mark -

UInt16 MacHostQuickMenu::sMenuCount = 1;

MacHostQuickMenu::MacHostQuickMenu(const uni_char * menu_name, const char *menu_type, INTPTR menu_value)
{
	mEmMenuRef = NULL;
	mMenuID = sMenuCount++;
	mMenuValue = menu_value;
	EmBrowserStatus err;
	mMenuKind = emQuickMenuKindUnknown;
	if (op_strcmp(menu_type, "Browser File Menu") == 0) {
		mMenuKind = emQuickMenuKindFile;
	} else if (op_strcmp(menu_type, "Browser Edit Menu") == 0) {
		mMenuKind = emQuickMenuKindEdit;
	} else if (op_strcmp(menu_type, "Browser Help Menu") == 0) {
		mMenuKind = emQuickMenuKindHelp;
	} else if (op_strcmp(menu_type, "Quick Preferences Menu") == 0) {
		mMenuKind = emQuickMenuKindQuickPrefs;
	} else if (op_strcmp(menu_type, "Browser Bookmarks Menu") == 0) {
		mMenuKind = emQuickMenuKindBookmarks;
	} else if ( (op_strcmp(menu_type, "Active Bookmark Folder Menu") == 0) ||
				(op_strcmp(menu_type, "Bookmark Folder Menu") == 0) ) {
		mMenuKind = emQuickMenuKindFolder;
	} else if (op_strcmp(menu_type, "Browser Widgets Menu") == 0) {
		mMenuKind = emQuickMenuKindGadgets;
	} else if (op_strcmp(menu_type, "Dock Menu") == 0) {
		mMenuKind = emQuickMenuKindDock;
	}

	if (gQuickAppMethods && gQuickAppMethods->createMenu)
	{
		err = gQuickAppMethods->createMenu((const EmBrowserChar *) menu_name, menu_type, mMenuKind, &mEmMenuRef);
		if (err != emBrowserNoErr)
		{
			mEmMenuRef = 0;
		}
	}

	mMenuName.Set(menu_type);
	mModtime = 0;

	if (g_desktop_bookmark_manager && g_desktop_global_application)
		g_desktop_bookmark_manager->AddBookmarkListener(static_cast<MacApplicationListener*>(g_desktop_global_application));
}

MacHostQuickMenu::~MacHostQuickMenu()
{
	if (mEmMenuRef && gQuickAppMethods && gQuickAppMethods->disposeMenu)
	{
		gQuickAppMethods->disposeMenu(mEmMenuRef);
	}
	int last = mActions.GetCount();
	while (last--) {
		OpInputAction::DeleteInputActions(mActions.Get(last));
	}
}

void MacHostQuickMenu::AddItem(BOOL seperator, const uni_char* item_name, OpWidgetImage* image, const uni_char* shortcut, MacQuickMenu *submenu, BOOL checked, BOOL selected, BOOL disabled, OpInputAction* input_action)
{
	EmQuickMenuFlags flags = 0;
	EmQuickMenuRef sub = 0;
	EmQuickMenuCommand command = 0;
	UInt32 hiCommand = 0;

	EmQuickMenuItemInfo info;
	memset(&info, 0, sizeof(info));

	if (seperator)
		flags |= emQuickMenuFlagSeperator;
	if (disabled)
		flags |= emQuickMenuFlagDisabled;
	if (checked)
		flags |= emQuickMenuFlagHasMark;
	if (selected)
		flags |= emQuickMenuFlagHasMark; // | emQuickMenuFlagUseBulletMark

	if (submenu)
	{
		sub = ((MacHostQuickMenu*) submenu)->GetEmMenuRef();
		mSubmenus.Add((MacHostQuickMenu*) submenu);
	}

	if (input_action)
	{
		const uni_char* url;
		mActions.Add(input_action);
		command = (mMenuID << 16) | mActions.GetCount();
		switch (input_action->GetAction())
		{
			case OpInputAction::ACTION_OPEN:
				hiCommand = kHICommandOpen;
				break;
			case OpInputAction::ACTION_SAVE:
				hiCommand = kHICommandSave;
				break;
			case OpInputAction::ACTION_NEW_BROWSER_WINDOW:
				hiCommand = kHICommandNew;
				break;
			case OpInputAction::ACTION_CLOSE_WINDOW:
				hiCommand = kHICommandClose;
				break;
			case OpInputAction::ACTION_EXIT:
				hiCommand = kHICommandQuit;
				break;
			case OpInputAction::ACTION_SHOW_PREFERENCES:
				hiCommand = kHICommandPreferences;
				break;
			case OpInputAction::ACTION_UNDO:
				hiCommand = kHICommandUndo;
				break;
			case OpInputAction::ACTION_REDO:
				hiCommand = kHICommandRedo;
				break;
			case OpInputAction::ACTION_COPY:
				hiCommand = kHICommandCopy;
				break;
			case OpInputAction::ACTION_PASTE:
				hiCommand = kHICommandPaste;
				break;
			case OpInputAction::ACTION_CUT:
				hiCommand = kHICommandCut;
				break;
			case OpInputAction::ACTION_REMOVE:
				hiCommand = kHICommandClear;
				break;
			case OpInputAction::ACTION_SELECT_ALL:
				hiCommand = kHICommandSelectAll;
				break;
			case OpInputAction::ACTION_PRINT_DOCUMENT:
				hiCommand = kHICommandPrint;
				break;
			case OpInputAction::ACTION_SHOW_PRINT_SETUP:
				hiCommand = kHICommandPageSetup;
				break;
#if defined(WIDGET_RUNTIME_SUPPORT) && defined(_MACINTOSH_)
          case OpInputAction::ACTION_WIDGET_ABOUT_PAGE:
              hiCommand = kHICommandAbout;
              break;
            case OpInputAction::ACTION_WIDGET_PREFERENCE_PANE:
                hiCommand = kHICommandPreferences;
                break;              
#endif // WIDGET_RUNTIME_SUPPORT				
			case OpInputAction::ACTION_OPEN_URL_IN_NEW_PAGE:
			case OpInputAction::ACTION_GO_TO_PAGE:
				url = input_action->GetActionDataString();
				if (url && uni_strcmp(url, UNI_L("opera:about")) == 0) {
					hiCommand = kHICommandAbout;
				}
				break;
			case OpInputAction::ACTION_CHECK_FOR_UPDATE:
				hiCommand = emOperaCommandCheckForUpdate;
				break;
			case OpInputAction::ACTION_NEW_PAGE:
				hiCommand = emOperaCommandNewTab;
				break;		
			case OpInputAction::ACTION_REPORT_SITE_PROBLEM:
				hiCommand = kHICommandAppHelp;
				break;		
		}
	}

	if (mEmMenuRef && gQuickAppMethods && gQuickAppMethods->addMenuItem)
	{
		info.majorVersion = 1;
		info.minorVersion = 0;
		info.title = (const EmBrowserChar *) item_name;
		info.flags = flags;
		info.command = command;
		info.submenu = sub;
		info.hiMenuEquv = hiCommand;
		info.icon = 0;

		if ((mMenuKind == emQuickMenuKindBookmarks || mMenuKind == emQuickMenuKindFolder) && input_action && ((input_action->GetAction() == OpInputAction::ACTION_OPEN_LINK) || (input_action->GetAction() == OpInputAction::ACTION_MENU_FOLDER)) ||
			uni_strcmp(mMenuName.CStr(), UNI_L("Browser Feeds Menu")) == 0 || uni_strcmp(mMenuName.CStr(), UNI_L("Browser Widgets Menu")) == 0)
		{
			if (image)
				info.icon = GetNSImageFromOpWidgetImage(image);
		}
		if (shortcut)
		{
			uni_char theChar = 0;
			UInt32 theMods = 0;
			SInt16 theGlyph = 0;
			if (ParseShortcut(shortcut, theChar, theMods, theGlyph)
#ifdef RESTRICT_MENU_SHORTCUTS_WITHOUT_COMMAND_MODIFIER
			 && (theMods & cmdKey)
#endif //RESTRICT_MENU_SHORTCUTS_WITHOUT_COMMAND_MODIFIER
			 )
			{
				if( ((theGlyph == kMenuDeleteLeftGlyph) || (theGlyph == kMenuDeleteRightGlyph)) &&
					(theMods == 0) )
				{
					theGlyph = 0;
				}
				info.charCode = theChar;
				info.modifers = theMods;
				info.glyph = theGlyph;
			}
		}
		gQuickAppMethods->addMenuItem(mEmMenuRef, -1, &info);
		if (info.icon) {
			CFRelease(info.icon);
		}
	}
}

EmQuickMenuRef MacHostQuickMenu::GetEmMenuRef()
{
	return mEmMenuRef;
}

MacHostQuickMenu* MacHostQuickMenu::FindMenu(UInt16 inMenuID)
{
	if (inMenuID == mMenuID)
		return this;

	MacHostQuickMenu* child;
	int count = mSubmenus.GetCount();
	for (int i = 0; i < count; i++)
	{
		child = mSubmenus.Get(i);
		if (child)
			child = child->FindMenu(inMenuID);
		if (child)
			return child;
	}
	return NULL;
}

MacHostQuickMenu* MacHostQuickMenu::FindMenu(EmQuickMenuRef inMenu)
{
	if (inMenu == mEmMenuRef)
		return this;

	MacHostQuickMenu* child;
	int count = mSubmenus.GetCount();
	for (int i = 0; i < count; i++)
	{
		child = mSubmenus.Get(i);
		if (child)
			child = child->FindMenu(inMenu);
		if (child)
			return child;
	}
	return NULL;
}

OpInputAction *MacHostQuickMenu::GetAction(UInt16 actionID)
{
	OpInputAction * action = mActions.Get(actionID - 1);
	if (action)
	{
		return action;
	}
	return NULL;
}

void MacHostQuickMenu::UpdateMenu()
{
	if (gQuickAppMethods && gQuickAppMethods->refreshMenu)
	{
		gQuickAppMethods->refreshMenu(mEmMenuRef);
	}
}

const char* MacHostQuickMenu::GetMenuName()
{
	return mMenuName.CStr();
}
