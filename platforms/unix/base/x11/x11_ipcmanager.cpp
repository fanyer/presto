/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */

#include "core/pch.h"

#include "platforms/unix/base/x11/x11_ipcmanager.h"
#include "platforms/unix/base/x11/x11_atomizer.h"
#include "platforms/unix/base/x11/x11_gadget.h"
#include "platforms/unix/base/x11/x11_globals.h"

#include "platforms/unix/product/x11quick/ipcmessageparser.h"
#include "platforms/unix/product/x11quick/toplevelwindow.h"
#include "platforms/unix/product/config.h" // BROWSER_* defines
#include "adjunct/desktop_util/boot/DesktopBootstrap.h"
#include "adjunct/desktop_util/resources/ResourceFolders.h"
#include "adjunct/widgetruntime/GadgetStartup.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"

#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>

using X11Types::Atom;


X11IPCManager* g_ipc_manager = 0;

// static
OP_STATUS X11IPCManager::Create()
{
	if (!g_ipc_manager )
	{
		g_ipc_manager = OP_NEW(X11IPCManager,());
	}
	return g_ipc_manager ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

// static
void X11IPCManager::Destroy()
{
	OP_DELETE(g_ipc_manager);
	g_ipc_manager = 0;
}


// static
OP_STATUS X11IPCManager::Init(X11Types::Window window)
{
	X11Types::Display* display = g_x11->GetDisplay();
	OpString buf;

	// Register version in OPERA_VERSION 
#ifdef FINAL_RELEASE // what it's defined to doesn't matter (and it gets no subver)
	const char *version = "Opera " BROWSER_FULL_VERSION;
#elif defined(BETA_RELEASE) // if given a value, must be string: gcc '-DBETA_RELEASE="1"'
	const char *version = "Opera " BROWSER_FULL_VERSION " B" BETA_RELEASE;
#else // Preview, Beta and Internal builds
	const char *version = "Opera " BROWSER_FULL_VERSION
# ifdef BROWSER_SUBVER_LONG
		" " BROWSER_SUBVER_LONG
# endif
		" build " BROWSER_BUILD_NUMBER;
#endif
	RETURN_IF_ERROR(buf.Set(version));
	XChangeProperty(display, window, ATOMIZE(OPERA_VERSION), XA_STRING, 8, PropModeReplace,
					(unsigned char *)buf.CStr(), buf.Length()*2);

	// Register uid in OPERA_USER 
	buf.Empty();
	RETURN_IF_ERROR(buf.AppendFormat(UNI_L("%d"), getuid()));
	XChangeProperty(display, window, ATOMIZE(OPERA_USER), XA_STRING, 8, PropModeReplace,
					(unsigned char *)buf.CStr(), buf.Length()*2);

	// Register preference path in OPERA_PREFERENCE
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_HOME_FOLDER, buf));
	XChangeProperty(display, window, ATOMIZE(OPERA_PREFERENCE), XA_STRING, 8, PropModeReplace,
					(unsigned char *)buf.CStr(), buf.Length()*2);	

	return OpStatus::OK;
}


X11IPCManager::X11IPCManager()
{
}


X11IPCManager::~X11IPCManager()
{
}

BOOL X11IPCManager::HandleEvent(XEvent* event)
{
	switch (event->type)
	{
		case PropertyNotify:
		{
			if (event->xproperty.atom == ATOMIZE(OPERA_MESSAGE))
			{
				if (event->xproperty.state == PropertyNewValue )
				{
					OpString message;
					Get(event->xproperty.window, message);
					XDeleteProperty(event->xproperty.display, event->xproperty.window, event->xproperty.atom);

					BrowserDesktopWindow* bw = ToplevelWindow::GetBrowserDesktopWindow(event->xproperty.window);
					BOOL handled = FALSE;
					handled = IPCMessageParser::ParseCommand(bw, message);
#ifdef WIDGET_RUNTIME_SUPPORT
					if( !handled )
					{
						DesktopWindow* dw = X11Gadget::GetDesktopWindow();
						handled = IPCMessageParser::ParseWindowCommand(dw, message);
					}
#endif // WIDGET_RUNTIME_SUPPORT
				}
				return TRUE;
			}
			break;
		}
	}

	return FALSE;
}


// static
X11Types::Window X11IPCManager::FindOperaWindow(X11Types::Window window, const OpStringC& name, ErrorCode& error)
{
	OpString home_folder;
	ResourceFolders::GetFolder(OPFILE_HOME_FOLDER, g_desktop_bootstrap->GetInitInfo(), home_folder);

	error = WINDOW_NOT_FOUND;
	
	if (window != None)
	{
		error = IsOperaWindow(window, home_folder, OpStringC());
		if (error == WINDOW_NOT_FOUND)
		{
			printf( "opera: Window 0x%08x is not a valid Opera window\n",(unsigned int)window );
			return None;
		}
		else
			return window;
	}
	
	X11Types::Display* display = g_x11->GetDisplay();
	X11Types::Window root = RootWindow(display, g_x11->GetDefaultScreen());
	X11Types::Window dummy, *children=0;
	unsigned int num_children;
	
	
	if (!XQueryTree(display, root, &dummy, &dummy, &children, &num_children) )
	{
		printf( "opera: Can not search for Opera window. XQueryTree failed.\n");
		return 0;
	}

	X11Types::Window near_match = None;

	WindowList* tail = OP_NEW(WindowList, (children, num_children));
	WindowList* head = tail;
	while (head)
	{
		for (unsigned int i=0; i<head->num_windows; i++ )
		{
			X11Types::Window candidate = head->windows[i];
			
			switch (error = IsOperaWindow(candidate, home_folder, name))
			{
				case WINDOW_EXISTS:
				case GADGET_WINDOW_EXISTS:				
					// Success. Deallocate list and return window identifier
					while (head)
					{
						WindowList* tmp = head;
						head = head->next;
						XFree(tmp->windows);
						OP_DELETE(tmp);
					}
					return candidate;

				case WINDOW_EXISTS_WITH_OTHER_NAME:
					if (near_match == None)
						near_match = candidate;
				break;
			}

			// Failed. Test to see if candidate has any children. If so add them to the list
			// of windows to be tested (tail should only be 0 if head is 0)
			
			if (tail)
			{
				children = 0;
				if (!XQueryTree(display, candidate, &dummy, &dummy, &children, &num_children) || num_children == 0)
				{
					XFree(children); // Just to be sure
				}
				else if (num_children > 0)
				{
					WindowList* next = OP_NEW(WindowList, (children, num_children));
					tail = tail->next = next;
				}
			}
		}

		// We are at end of list if head == tail (head->next is then 0)
		WindowList* tmp = head;
		head = head->next;
		if (tail == tmp)
			tail = 0;
		XFree(tmp->windows);
		OP_DELETE(tmp);
	}	

	if (near_match != None)
	{
		error = WINDOW_EXISTS_WITH_OTHER_NAME;
		return near_match;
	}
	else
	{
		return WINDOW_NOT_FOUND;
	}
}


// static
X11IPCManager::ErrorCode X11IPCManager::IsOperaWindow(X11Types::Window& window, const OpStringC& home_folder, const OpStringC& name)
{
	X11Types::Display* display = g_x11->GetDisplay();
	
	X11Types::Atom type;
	int format;
	unsigned long nitems;
	unsigned long bytes_remaining;
	unsigned char *data = 0;

	int status = XGetWindowProperty(display, window, ATOMIZE(OPERA_VERSION), 0, 65536 / 4, False,
									XA_STRING, &type, &format, &nitems, &bytes_remaining, &data);
	if (!data)
		return WINDOW_NOT_FOUND;
	XFree(data);
	if (status != Success)
		return WINDOW_NOT_FOUND;

	// Found a window with the OPERA_VERSION property. Now test that the user id of the window 
	// matches our user id

	data = 0;
	status = XGetWindowProperty(display, window, ATOMIZE(OPERA_USER), 0, 65536 / 4, False,
								XA_STRING, &type, &format, &nitems, &bytes_remaining, &data);
	if (!data)
		return WINDOW_NOT_FOUND;
	if (status != Success)
	{
		XFree(data);
		return WINDOW_NOT_FOUND;
	}
	int uid = -1;
	uni_sscanf( (uni_char*)data, UNI_L("%d"), &uid );
	XFree(data);
	if (uid != (int)getuid())
		return WINDOW_NOT_FOUND;

	// Windows and user id matches. Now check the OPERA_PREFERENCE property.
	
	data = 0;
	status = XGetWindowProperty(display, window, ATOMIZE(OPERA_PREFERENCE), 0, 65536 / 4, False,
								XA_STRING, &type, &format, &nitems, &bytes_remaining, &data);
	if (!data)
		return WINDOW_NOT_FOUND;
	if (status != Success)
	{
		XFree(data);
		return WINDOW_NOT_FOUND;
	}

	if (home_folder.Compare((uni_char*)data, nitems/2))
	{
		XFree(data);
		return WINDOW_NOT_FOUND;
	}

	XFree(data);
	data = 0;

	// Detection for gadgets (widget runtime) [START]
	if (GadgetStartup::IsGadgetStartupRequested())
	{
		status = XGetWindowProperty(display, window, ATOMIZE(WM_WINDOW_ROLE), 0, 65536 / 4, False,
									XA_STRING, &type, &format, &nitems, &bytes_remaining, &data);
		if (status == Success && data && op_strcmp("opera-widget#1\0", (char*)data) == 0 )
		{
			XFree(data);
			return GADGET_WINDOW_EXISTS;
		}
		if (data)
			XFree(data);
	}
	// Detection for gadgets (widget runtime) [END]

	// We now have a valid window. Test for name
	if (name.IsEmpty())
		return WINDOW_EXISTS;

	if (!name.Compare("first") || !name.Compare("last"))
	{
		OpINT32Vector list;

		long offset = 0;
		while (offset >= 0)
		{
			data = 0;
			int rc = XGetWindowProperty(display, window, ATOMIZE(OPERA_WINDOWS), offset, 1, False,
										XA_WINDOW, &type, &format, &nitems, &bytes_remaining, &data);
			if (rc == Success && type == XA_WINDOW && format == 32 && nitems > 0)
			{
				INT32* buf = (INT32*)data;
				for (unsigned long i=0; i<nitems; i++)
					list.Add(buf[i]);
				if (bytes_remaining>0)
					offset++;
				else
					offset = -1;
			}
			else
				offset = -1;

			if (data)
				XFree(data);
		}
		if (list.GetCount() > 0)
			window = (X11Types::Window)list.Get( !name.Compare("first") ? 0 : list.GetCount()-1);
		else
		{
			; // Use the window we have already found
		}

		return WINDOW_EXISTS;
	}
	else
	{
		return HasWindowName(window, name) ? WINDOW_EXISTS : WINDOW_EXISTS_WITH_OTHER_NAME;
	}

	return WINDOW_NOT_FOUND;
}


// static
BOOL X11IPCManager::HasWindowName(X11Types::Window window, const OpStringC& name)
{
	if (name.HasContent())
	{
		X11Types::Display* display = g_x11->GetDisplay();

		X11Types::Atom type;
		int format;
		unsigned long nitems;
		unsigned long bytes_remaining;
		unsigned char *data = 0;

		int status = XGetWindowProperty(display, window, ATOMIZE(OPERA_WINDOW_NAME), 0, 65536 / 4, False,
										XA_STRING, &type, &format, &nitems, &bytes_remaining, &data);
		if (status == Success && data)
		{
			if (name.Compare((uni_char*)data) == 0)
			{
				XFree(data);
				return TRUE;
			}
		}
		if (data)
			XFree(data);
	}

	return FALSE;
}


// static
BOOL X11IPCManager::Set(X11Types::Window window, const OpStringC& message)
{
	if (!Lock(window, TRUE))
		return FALSE;

	X11Types::Display* display = g_x11->GetDisplay();

	OpString tmp;
	Get(window, tmp);
	if (tmp.HasContent())
	{
		tmp.Append("\n");
	}
	tmp.Append(message);

	XChangeProperty(display, window, ATOMIZE(OPERA_MESSAGE), XA_STRING, 8, PropModeReplace,
					(unsigned char *) tmp.CStr(), tmp.Length()*2 );

	Lock(window, FALSE);

	return TRUE;
}


// static
void X11IPCManager::Get(X11Types::Window window, OpString& message)
{
	X11Types::Display* display = g_x11->GetDisplay();

	X11Types::Atom type;
	int format;
	unsigned long nitems;
	unsigned long bytes_remaining;
	unsigned char *data = 0;

	int status = XGetWindowProperty(display, window, ATOMIZE(OPERA_MESSAGE), 0, 65536 / 4, False,
									XA_STRING, &type, &format, &nitems, &bytes_remaining, &data);
	if (status == Success && data)
		message.Set((uni_char*)data);
	if (data)
		XFree(data);
}


// static
BOOL X11IPCManager::Lock(X11Types::Window window, BOOL set)
{
	X11Types::Display* display = g_x11->GetDisplay();

	OpString8 hostname;
	hostname.Reserve(256);
	if (hostname.Capacity() == 0)
		return FALSE;
	if (gethostname(hostname.CStr(), hostname.Capacity()-1) == -1 )
		RETURN_VALUE_IF_ERROR(hostname.Set("localhost"), FALSE);	
	OpString8 lock;
	RETURN_VALUE_IF_ERROR(lock.AppendFormat("%d@%s", getuid(), hostname.CStr()), FALSE);


	if (set)
	{
		time_t start = time(0);
		while (1)
		{
			BOOL owns_lock = FALSE;

			X11Types::Atom type;
			int format;
			unsigned long nitems;
			unsigned long bytes_remaining;
			unsigned char *data = 0;
			
			XGrabServer(display);
			int status = XGetWindowProperty(display, window, ATOMIZE(OPERA_SEMAPHORE), 0, 65536 / 4, False,
											XA_STRING, &type, &format, &nitems, &bytes_remaining, &data );
		
			if (status != Success || type == None)
			{
				// The semaphore atom is not in use. Grab it by setting it
				XChangeProperty(display, window, ATOMIZE(OPERA_SEMAPHORE), XA_STRING, 8, PropModeReplace,
								(unsigned char *)lock.CStr(), lock.Length());				
				owns_lock = TRUE;
			}

			XUngrabServer(display);
			XSync(display, False);

			if (data)
				XFree(data);

			if (owns_lock)
				return TRUE;

			// Wait a little bit and try again

			while (1)
			{
				int diff_time = time(0) - start;
				if (diff_time >= 10)
				{
					printf( "opera: Can not get exclusive access to semaphore (1): timeout\n");
					return FALSE;
				}

				int fd = ConnectionNumber(display);
				fd_set set;
				FD_ZERO(&set);
				FD_SET( fd, &set );

				struct timeval timeout;
				timeout.tv_sec  = 10 - diff_time;
				timeout.tv_usec = 0;
				int result = select( fd + 1, &set, 0, 0, &timeout );
				if (result <= 0)
				{
					printf( "opera: Can not get exclusive access to "
							"semaphore: %s\n", result == -1 ? strerror(errno) :
							"timeout (2)" );
					return FALSE;
				}

				XEvent xevent;
				XNextEvent( display, &xevent );
				if (xevent.xany.type == DestroyNotify)
				{
					if (xevent.xdestroywindow.window == window)
					{
						fputs( "opera: Can not get exclusive access to semaphore: window destroyed\n", stderr);
						return FALSE;
					}
				}
				else if (xevent.xany.type == PropertyNotify)
				{
					if (xevent.xproperty.state == PropertyDelete &&
						xevent.xproperty.window == window &&
						xevent.xproperty.atom == ATOMIZE(OPERA_SEMAPHORE))
						// Try outer loop once more
						break;
				}
			}
		}
	}
	else
	{
		X11Types::Atom type;
		int format;
		unsigned long nitems;
		unsigned long bytes_remaining;
		unsigned char *data = 0;

		int status = XGetWindowProperty(display, window,ATOMIZE(OPERA_SEMAPHORE), 0, 65536 / 4, True,
										XA_STRING, &type, &format, &nitems, &bytes_remaining, &data);

		BOOL ok = TRUE;
		if (status != Success || !data || !*data || lock.Compare((char*)data) != 0)
		{
			if (status != Success)
				printf( "opera: Remove semaphore property: Failed\n");
			else if (!data || !*data)
				printf( "opera: Remove semaphore property: No data\n");
			else
				printf( "opera: Remove semaphore property: Illegal data\n");
			ok = FALSE;
		}

		if (data)
			XFree( data );	
		return ok;
	}

	return TRUE;
}

