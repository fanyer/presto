/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Espen Sand 
*/

#ifndef X11_ATOMIZER_H
#define X11_ATOMIZER_H

#include "platforms/utilix/x11types.h"
#include "modules/util/adt/opvector.h"

#define ATOMIZE(a) (X11Atomizer::Get(X11Atomizer::a))
#define IS_NET_SUPPORTED(a) (X11Atomizer::IsSupported(X11Atomizer::a))


class X11Atomizer
{
public:
	enum AtomId {
		_NET_SUPPORTED,
		_NET_WM_DECORATION,
		_NET_WM_MOVERESIZE,
		_NET_WM_PING,
		_NET_WM_PID,
		_NET_WM_SYNC_REQUEST,
		_NET_WM_SYNC_REQUEST_COUNTER,
		_NET_WM_NAME,
		_NET_WM_ICON,
		_NET_WM_STATE,
		_NET_WM_STATE_FULLSCREEN,
		_NET_WM_STATE_HIDDEN,
		_NET_WM_STATE_MAXIMIZED_HORZ,
		_NET_WM_STATE_MAXIMIZED_VERT,
		_NET_WM_STATE_SHADED,
		_NET_WM_STATE_STAYS_ON_TOP,
		_NET_WM_STATE_ABOVE,
		_NET_WM_STATE_BELOW,
		_NET_WM_STATE_DEMANDS_ATTENTION,
		_NET_WM_STATE_MODAL,
		_NET_WM_WINDOW_TYPE,
		_NET_WM_WINDOW_TYPE_TOOLBAR,
		_NET_WM_WINDOW_TYPE_MENU,
		_NET_WM_WINDOW_TYPE_UTILITY,
		_NET_WM_WINDOW_TYPE_SPLASH,
		_NET_WM_WINDOW_TYPE_DIALOG,
		_NET_WM_WINDOW_TYPE_DROPDOWN_MENU,
		_NET_WM_WINDOW_TYPE_POPUP_MENU,
		_NET_WM_WINDOW_TYPE_TOOLTIP,
		_NET_WM_WINDOW_TYPE_NOTIFICATION,
		_NET_WM_WINDOW_TYPE_COMBO,
		_NET_WM_WINDOW_TYPE_DND,
		_NET_WM_WINDOW_TYPE_NORMAL,
		_NET_WM_STATE_SKIP_TASKBAR,
		_NET_FRAME_EXTENTS,
		_NET_FRAME_WINDOW,
		_NET_DESKTOP_GEOMETRY,
		_NET_NUMBER_OF_DESKTOPS,
		_NET_CURRENT_DESKTOP,
		_NET_WORKAREA,
		_NET_SUPPORTING_WM_CHECK,
		_NET_STARTUP_INFO_BEGIN,
		_NET_STARTUP_INFO,
		_MOTIF_WM_HINTS,
		COMPOUND_TEXT,
		OPERA_CLIPBOARD,
		OPERA_VERSION,
		OPERA_MESSAGE,
		OPERA_SEMAPHORE,
		OPERA_USER,
		OPERA_PREFERENCE,
		OPERA_WINDOW_NAME,
		OPERA_WINDOWS,
		SAVE_TARGETS,
		TARGETS,
		TEXT,
		SM_CLIENT_ID,
		WM_CLIENT_LEADER,
		WM_CLIENT_MACHINE,
		WM_DELETE_WINDOW,
		WM_NAME,
		WM_STATE,
		WM_TAKE_FOCUS,
		WM_WINDOW_ROLE,
		WM_PROTOCOLS,
		UTF8_STRING,
		CLIPBOARD,
		CLIPBOARD_MANAGER,
		XdndAware,
		XdndEnter,
		XdndLeave,
		XdndDrop,
		XdndFinished,
		XdndPosition,
		XdndStatus,
		XdndTypeList,
		XdndActionList,
		XdndSelection,
		XdndProxy,
		XdndActionCopy,
		XdndActionMove,
		XdndActionLink,
		XdndActionPrivate,
		NUM_ATOMS // Keep this one as the last element!
	};

private:
	struct Entry
	{
		int supported; // Only used for NET_ atoms.  -1: Not tested, 0: No, 1: Yes
		X11Types::Atom atom;
		const char* name;
	};

public:
	static X11Types::Atom Get(AtomId id);	
	static bool IsSupported(AtomId id);

private:
	static void Init();

private:
	static Entry m_atom_list[NUM_ATOMS];
	static OpAutoVector<X11Types::Atom> m_net_supported_list;
	static bool m_has_init_supported_list;


};

#endif // X11_ATOMIZER_H
