/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef DOCHAND_WINMAN_CONSTANTS_H
#define DOCHAND_WINMAN_CONSTANTS_H

enum DM_PrintType
{
	PRINT_AS_SCREEN = 0,
	PRINT_ACTIVE_FRAME,
	PRINT_ALL_FRAMES
};

enum ST_MESSAGETYPE
{
	ST_ALINK,
	ST_ATITLE,
	ST_ASTRING,
	ST_ASTATUS,
	ST_ABUTTON
};

enum WinState
{
	BUSY,
	CLICKABLE,
	NOT_BUSY,
	RESERVED
};

enum WinSizeState
{
	NORMAL,
	ICONIC,
	MAXIMIZED,
	FULLSCREEN,

	INVALID_WINSIZESTATE	//	last enum
};

enum FOCUS_REQUEST
{
    FOCUS_REQUEST_NO_REQUEST,              //Will clear any previously set requests
    FOCUS_REQUEST_STORE,                   //Store current focus
    FOCUS_REQUEST_STORE_WITH_SELECTION,    //Store current focus, and selection if the element with focus is an editcontrol
    FOCUS_REQUEST_RESTORE,                 //Restore stored focus if window is active
    FOCUS_REQUEST_RESTORE_AND_ACTIVATE,    //Restore stored focus, and activate window 
    FOCUS_REQUEST_RESTORE_AND_ACTIVATE_WITH_SELECTION,  //Restore stored focus and selection, and activate window
    FOCUS_REQUEST_URL,                     //Set (or store, if window is inactive) focus in addressbar
    FOCUS_REQUEST_SEARCH_OR_URL,           //Set (or store..) focus in searchbar, or addressbar is searchbar is hidden
    FOCUS_REQUEST_DOCUMENT_OR_URL,         //Set (or store..) focus in document, or addressbar is no document is loaded
    FOCUS_REQUEST_DOCUMENT                 //Set (or store..) focus in document
};

enum FOCUS_REQUEST_TIME
{
    FOCUS_REQUEST_TIME_NOT_SET              = 0,
    FOCUS_REQUEST_TIME_NOW                  = 1, //Should be in ascending order, so action can be performed like 'if (time < ...'
    FOCUS_REQUEST_TIME_ACTIVATED            = 2, //Executed when window is activated
    FOCUS_REQUEST_TIME_DOCUMENTINFO_LOADED  = 3, //Executed when documentinfo is loaded
    FOCUS_REQUEST_TIME_DOCUMENT_LOADED      = 4, //Executed when document is loaded

    FOCUS_REQUEST_TIME_NEVER                = 99 //Never happens...
};

enum TOPTOOLBARTYPE
{
	NO_TOOLBAR,
	BROWSER_TOOLBAR
};

enum Window_Type
{
    WIN_TYPE_NORMAL = 0,
	WIN_TYPE_DOWNLOAD,
	WIN_TYPE_CACHE,
	WIN_TYPE_PLUGINS,
	WIN_TYPE_HISTORY,
	WIN_TYPE_HELP,
	WIN_TYPE_MAIL_VIEW,
	WIN_TYPE_MAIL_COMPOSE,
	WIN_TYPE_NEWSFEED_VIEW,
	WIN_TYPE_IM_VIEW,
	WIN_TYPE_P2P_VIEW,
	WIN_TYPE_BRAND_VIEW,
	WIN_TYPE_PRINT_SELECTION,
	WIN_TYPE_JS_CONSOLE,
	WIN_TYPE_GADGET,
	WIN_TYPE_CONTROLLER,					// Controller window in a Browser in Browser context.
	WIN_TYPE_INFO,							// this type is used for "input dead" info type areas, like displaying a Contact's picture
	WIN_TYPE_DIALOG,							// for the HTML dialogs used in core-gogi
	WIN_TYPE_THUMBNAIL,						// used for thumbnail generation
	WIN_TYPE_DEVTOOLS,						// Developer Tools, has access to opera.scopeAddClient() and friends JS API
	WIN_TYPE_NORMAL_HIDDEN                  // A normal window, not exposed trough scope. Used for example by the gogi UIs
};

enum Window_Feature
{
	WIN_FEATURE_NAVIGABLE		= 0x001,
	WIN_FEATURE_PRINTABLE		= 0x002,
	WIN_FEATURE_RELOADABLE		= 0x004,
	WIN_FEATURE_FOCUSABLE		= 0x008,
	WIN_FEATURE_HOMEABLE		= 0x010,
	WIN_FEATURE_PROGRESS		= 0x020,
	WIN_FEATURE_OUTSIDE			= 0x040,			// window is outside/sdi and should not connect to main toolbar etc.
	WIN_FEATURE_BOOKMARKABLE	= 0x080,			// ok to at window to bookmark lists.
	WIN_FEATURE_MENU			= 0x100,	   		// window can have a menu (but it will only be displayed in SDI mode)
	WIN_FEATURE_ALERTS			= 0x200				// window will show ecmascript alert dialogs
};

#endif // DOCHAND_WINMAN_CONSTANTS_H
