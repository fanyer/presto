/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Espen Sand
*/

#include "core/pch.h"

#include "x11_atomizer.h"

#include "platforms/quix/commandline/StartupSettings.h"
#include "platforms/utilix/x11_all.h"
#include "x11_globals.h"


using X11Types::Atom;


X11Atomizer::Entry X11Atomizer::m_atom_list[X11Atomizer::NUM_ATOMS] = {
	{ -1, None, "_NET_SUPPORTED" },
	{ -1, None, "_NET_WM_DECORATION" },
	{ -1, None, "_NET_WM_MOVERESIZE" },
	{ -1, None, "_NET_WM_PING" },
	{ -1, None, "_NET_WM_PID" },
	{ -1, None, "_NET_WM_SYNC_REQUEST" },
	{ -1, None, "_NET_WM_SYNC_REQUEST_COUNTER" },
	{ -1, None, "_NET_WM_NAME" },
	{ -1, None, "_NET_WM_ICON" },
	{ -1, None, "_NET_WM_STATE" },
	{ -1, None, "_NET_WM_STATE_FULLSCREEN" },
	{ -1, None, "_NET_WM_STATE_HIDDEN" },
	{ -1, None, "_NET_WM_STATE_MAXIMIZED_HORZ" },
	{ -1, None, "_NET_WM_STATE_MAXIMIZED_VERT" },
	{ -1, None, "_NET_WM_STATE_SHADED" },
	{ -1, None, "_NET_WM_STATE_STAYS_ON_TOP" },
	{ -1, None, "_NET_WM_STATE_ABOVE" },
	{ -1, None, "_NET_WM_STATE_BELOW" },
	{ -1, None, "_NET_WM_STATE_DEMANDS_ATTENTION" },
	{ -1, None, "_NET_WM_STATE_MODAL" },
	{ -1, None, "_NET_WM_WINDOW_TYPE" },
	{ -1, None, "_NET_WM_WINDOW_TYPE_TOOLBAR" },
	{ -1, None, "_NET_WM_WINDOW_TYPE_MENU" },
	{ -1, None, "_NET_WM_WINDOW_TYPE_UTILITY" },
	{ -1, None, "_NET_WM_WINDOW_TYPE_SPLASH" },
	{ -1, None, "_NET_WM_WINDOW_TYPE_DIALOG" },
	{ -1, None, "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU" },
	{ -1, None, "_NET_WM_WINDOW_TYPE_POPUP_MENU" },
	{ -1, None, "_NET_WM_WINDOW_TYPE_TOOLTIP" },
	{ -1, None, "_NET_WM_WINDOW_TYPE_NOTIFICATION" },
	{ -1, None, "_NET_WM_WINDOW_TYPE_COMBO" },
	{ -1, None, "_NET_WM_WINDOW_TYPE_DND" },
	{ -1, None, "_NET_WM_WINDOW_TYPE_NORMAL" },
	{ -1, None, "_NET_WM_STATE_SKIP_TASKBAR" },
	{ -1, None, "_NET_FRAME_EXTENTS" },
	{ -1, None, "_NET_FRAME_WINDOW" },
	{ -1, None, "_NET_DESKTOP_GEOMETRY" },
	{ -1, None, "_NET_NUMBER_OF_DESKTOPS" },
	{ -1, None, "_NET_CURRENT_DESKTOP" },
	{ -1, None, "_NET_WORKAREA" },
	{ -1, None, "_NET_SUPPORTING_WM_CHECK" },
	{ -1, None, "_NET_STARTUP_INFO_BEGIN" },
	{ -1, None, "_NET_STARTUP_INFO" },
	{  1, None, "_MOTIF_WM_HINTS" },
	{  1, None, "COMPOUND_TEXT" },
	{  1, None, "OPERA_CLIPBOARD" },
	{  1, None, "OPERA_VERSION" },
	{  1, None, "OPERA_MESSAGE" },
	{  1, None, "OPERA_SEMAPHORE" },
	{  1, None, "OPERA_USER" },
	{  1, None, "OPERA_PREFERENCE" },
	{  1, None, "OPERA_WINDOW_NAME" },
	{  1, None, "OPERA_WINDOWS" },
	{  1, None, "SAVE_TARGETS" },
	{  1, None, "TARGETS" },
	{  1, None, "TEXT" },
	{  1, None, "SM_CLIENT_ID" },
	{  1, None, "WM_CLIENT_LEADER" },
	{  1, None, "WM_CLIENT_MACHINE" },
	{  1, None, "WM_DELETE_WINDOW" },
	{  1, None, "WM_NAME" },
	{  1, None, "WM_STATE" },
	{  1, None, "WM_TAKE_FOCUS" },
	{  1, None, "WM_WINDOW_ROLE" },
	{  1, None, "WM_PROTOCOLS" },
	{  1, None, "UTF8_STRING" },
	{  1, None, "CLIPBOARD" },
	{  1, None, "CLIPBOARD_MANAGER" },
	{  1, None, "XdndAware"},
	{  1, None, "XdndEnter"},
	{  1, None, "XdndLeave"},
	{  1, None, "XdndDrop"},
	{  1, None, "XdndFinished"},
	{  1, None, "XdndPosition"},
	{  1, None, "XdndStatus"},
	{  1, None, "XdndTypeList"},
	{  1, None, "XdndActionList"},
	{  1, None, "XdndSelection"},
	{  1, None, "XdndProxy"},
	{  1, None, "XdndActionCopy"},
	{  1, None, "XdndActionMove"},
	{  1, None, "XdndActionLink"},
	{  1, None, "XdndActionPrivate"}
};

OpAutoVector<X11Types::Atom> X11Atomizer::m_net_supported_list;
bool X11Atomizer::m_has_init_supported_list = false;


X11Types::Atom X11Atomizer::Get(AtomId id)
{
	if (m_atom_list[id].atom == None)
	{
		m_atom_list[id].atom = XInternAtom(g_x11->GetDisplay(), m_atom_list[id].name, False);
	}

	return m_atom_list[id].atom;
}



// static
bool X11Atomizer::IsSupported(AtomId id)
{
	if( m_atom_list[id].supported == -1 )
	{
		m_atom_list[id].supported = 0;

		if( !m_has_init_supported_list )
		{
			m_has_init_supported_list = true;
			Init();
		}

		int count = m_net_supported_list.GetCount();
		if( count > 0 )
		{
			X11Types::Atom atom = Get(id);
			for( int i=0; i < count; i++ )
			{
				if( atom == *m_net_supported_list.Get(i) )
				{
					m_atom_list[id].supported = 1;
					break;
				}
			}
		}
	}

	return m_atom_list[id].supported == 1;
}


// static
void X11Atomizer::Init()
{
	if( m_net_supported_list.GetCount() == 0 )
	{
		X11Types::Display* dpy = g_x11->GetDisplay();
		X11Types::Window root  = DefaultRootWindow(dpy);
		X11Types::Atom net_supported_atom = Get(_NET_SUPPORTED);

		X11Types::Atom type;
		int format;
		long offset = 0;
		unsigned long nitems, after;
		unsigned char *data;

		int e = XGetWindowProperty(
			dpy, root, net_supported_atom, 0, 0,
			False, XA_ATOM, &type, &format, &nitems, &after, &data);
		if (e == Success && data)
			XFree(data);

		if (g_startup_settings.debug_atom)
		{
			printf("Test atoms: Data remaining to be read: %ld bytes\n", after );
		}

		if (e == Success && type == XA_ATOM && format == 32)
		{
			OpAutoVector<X11Types::Atom> list;

			while (after > 0)
			{
				XGetWindowProperty(
					dpy, root, net_supported_atom, offset, 1024,
					False, XA_ATOM, &type, &format, &nitems, &after, &data);

				if (g_startup_settings.debug_atom)
				{
					printf("Read atoms: Items read: %ld. Data remaining to be read: %ld bytes\n", nitems, after );
				}

				if (type == XA_ATOM && format == 32)
				{
					X11Types::Atom* tmp = (X11Types::Atom*)data;
					for( unsigned long i=0; i<nitems; i++ )
					{
						m_net_supported_list.Add( new Atom(tmp[i]));
					}
					offset += nitems;
				}
				else
				{
					if (g_startup_settings.debug_atom)
					{
						if( type != XA_ATOM )
							printf("Read atoms: Type is not XA_ATOM, aborting\n");
						if( format != 32)
							printf("Read atoms: Format is not 32 (%d), aborting\n", format);
					}
					after = 0;
				}
				
				if (data)
					XFree(data);
			}
			
			if (g_startup_settings.debug_atom)
			{
				int count = m_net_supported_list.GetCount();
				printf("Supported window manager hints (total: %d). Root window: 0x%08x\n", count, (unsigned int)root);
				for ( int i = 0; i < count; i++)
				{
					char* text = XGetAtomName( dpy, *m_net_supported_list.Get(i));
					printf("%s\n", text );
					XFree(text);
				}
			}

		}
	}
}

