/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef CUSTOM_WINDOW_MESSAGES_H
#define CUSTOM_WINDOW_MESSAGES_H

/*Supported and defined in Windows Vista SDK*/
#ifndef WM_MOUSEHWHEEL
#define WM_MOUSEHWHEEL 0x020E
#undef  WM_MOUSELAST
#define WM_MOUSELAST WM_MOUSEHWHEEL
#endif //WM_MOUSEHWHEEL

// Windows specific messages should be in windows code.
// Also, these messages need to be in user space, which
// they can't be if/when they are in the hardcore enum.
enum WindowsMessage
{
	WM_RUN_SLICE = WM_USER
#ifndef NS4P_COMPONENT_PLUGINS
	,WM_OPERA_DISPATCH_POSTED_MESSAGE = WM_USER
#endif // !NS4P_COMPONENT_PLUGINS
	,WM_FLASH_MESSAGE							// 0x401 is used by the flash plugin and
												// caught in the message hook, so reserve it here.
	,WM_PLUGIN_MESSAGE1							// Reserve some more messages for the silverlight plugin...
	,WM_PLUGIN_MESSAGE2
	,WM_PLUGIN_MESSAGE3
	,WM_PLUGIN_MESSAGE4
	,WM_PLUGIN_MESSAGE5
	,WM_PLUGIN_MESSAGE6
	,WM_PLUGIN_MESSAGE7
	,WM_PLUGIN_MESSAGE8
	,WM_PLUGIN_MESSAGE9
	,WM_PLUGIN_MESSAGE10
	,WM_PLUGIN_MESSAGE11
	,WM_PLUGIN_MESSAGE12
	,WM_PLUGIN_MESSAGE13
	,WM_PLUGIN_MESSAGE14
	,WM_PLUGIN_MESSAGE15
	,WM_PLUGIN_MESSAGE16
	,WM_PLUGIN_MESSAGE17
	,WM_PLUGIN_MESSAGE18
	,WM_PLUGIN_MESSAGE19
	,WM_PLUGIN_MESSAGE20
	,WM_OPERA_MEDIA_EVENT
	,WM_OPERA_DELAYED_CLOSE_WINDOW
	,WM_OPERA_CREATED
	,WM_OPERA_TIMERDOCPANNING
#ifdef GADGET_SUPPORT
	,WM_OPERA_REPAINT_GADGET
#endif // GADGET_SUPPORT
	,WM_OPERA_MDE_RENDER
	,WM_OPERA_RESIZE_FRAME
	,WM_OPERA_DRAG_CANCELLED
	,WM_OPERA_DRAG_ENDED
	,WM_OPERA_DRAG_DROP
	,WM_OPERA_DRAG_ENTER
	,WM_OPERA_DRAG_LEAVE
	,WM_OPERA_DRAG_MOVED
	,WM_OPERA_DRAG_UPDATE
	,WM_OPERA_DRAG_CAME_BACK
	,WM_OPERA_DRAG_EXTERNAL_ENTER		// drag initiated outside Opera enter the Opera window
	,WM_OPERA_DRAG_EXTERNAL_CANCEL		// external drag cancelled
	,WM_OPERA_DRAG_EXTERNAL_DROPPED		// external drag dropped
	,WM_OPERA_DRAG_COPY_FILE_TO_LOCATION	// we can't copy the file from the cache to a temp directory on the dnd thread, so offload to the main thread
	,WM_OPERA_DRAG_GET_SUGGESTED_FILENAME	// we can't access URL to get a suggested filename for a given url, so offload to the main thread
	,WM_WINSOCK_ACCEPT_CONNECTION
	,WM_WINSOCK_CONNECT_READY
	,WM_WINSOCK_DATA_READY
	,WM_WINSOCK_HOSTINFO_READY
	,WM_OP_TASKBAR
	,WM_BAD_DLL
	,WM_WRAPPER_WINDOW_READY
	,WM_FORCED_RUN_SLICE
	,WM_IN_LOOP_TEST
	//Message that can get posted to the componentplatforms' message only window
	//These are really messages addressed to the thread, but they need to go to
	//a window because we want to receive them when dealing with a modal
	//message loop created by the os (e.g.: menus message loops)
	,FIRST_PSEUDO_THREAD_MESSAGE
	,WM_MAIN_THREAD_MESSAGE = FIRST_PSEUDO_THREAD_MESSAGE
	,WM_DEAD_COMPONENT_MANAGER
	,LAST_PSEUDO_THREAD_MESSAGE
	,WM_DEAD_CM0
	,WM_SM_READ_READY
//---------------------------------------------//
	,WM_DELAYED_FOCUS_CHANGE = WM_APP
};

// struct used for WM_OPERA_DRAG_COPY_FILE_TO_LOCATION
struct CopyImageFileToLocationData
{
	const char*		file_url;		// url of the image on the web page
	OpString		destination;	// destination filename where the file can be found after copying
	Image			image;			// image data for the CF_DIB
};

// struct used for WM_OPERA_DRAG_GET_SUGGESTED_FILENAME
struct DNDGetSuggestedFilenameData
{
	const char*		file_url;		// url to get a suggested filename on
	OpString		destination;	// destination suggested filename
};

#endif //CUSTOM_WINDOW_MESSAGES_H
