/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "platforms/quix/environment/DesktopEnvironment.h"

#include "modules/pi/system/OpFolderLister.h"
#include "modules/prefs/prefsmanager/collections/pc_unix.h"
#include "platforms/unix/base/x11/x11_globals.h"
#include "platforms/utilix/x11_all.h"

// See DSK-376030. Too many issues with GTK3 so auto detection has been turned off
#define NO_AUTODETECT_GTK3

using X11Types::Atom;


DesktopEnvironment& DesktopEnvironment::GetInstance()
{
    static DesktopEnvironment s_environment;
    return s_environment;
}

DesktopEnvironment::DesktopEnvironment()
  : m_toolkit(TOOLKIT_UNKNOWN)
  , m_toolkit_environment(ENVIRONMENT_TO_BE_DETERMINED)
{
}

DesktopEnvironment::ToolkitEnvironment DesktopEnvironment::GetToolkitEnvironment()
{
	if (m_toolkit_environment == ENVIRONMENT_TO_BE_DETERMINED)
	{
		if (IsUbuntuUnityRunning())
		{
			m_toolkit_environment = ENVIRONMENT_UNITY;
			OP_ASSERT(!IsKDERunning() && !IsXfceRunning());
		}
		else if (IsGnomeRunning())
		{
			if (IsGnome3Running())
				m_toolkit_environment = ENVIRONMENT_GNOME_3;
			else
				m_toolkit_environment = ENVIRONMENT_GNOME;
			OP_ASSERT(!IsKDERunning() && !IsXfceRunning());
		}
		else if (IsKDERunning())
		{
			m_toolkit_environment = ENVIRONMENT_KDE;
			OP_ASSERT(!IsXfceRunning());
		}
		else if (IsXfceRunning())
		{
			m_toolkit_environment = ENVIRONMENT_XFCE;
		}
		else
		{
			m_toolkit_environment = ENVIRONMENT_UNKNOWN;
		}
	}

	return m_toolkit_environment;
}

void DesktopEnvironment::GetDesktopEnvironmentName(OpString8& name)
{
	if (m_toolkit_environment == ENVIRONMENT_TO_BE_DETERMINED)
		GetToolkitEnvironment();
	OP_ASSERT(m_toolkit_environment != ENVIRONMENT_TO_BE_DETERMINED);

	switch (m_toolkit_environment)
	{
		case ENVIRONMENT_GNOME:		name.Set("Gnome 2"); break;
		case ENVIRONMENT_GNOME_3:	name.Set("Gnome 3"); break;
		case ENVIRONMENT_UNITY:		name.Set("Unity"); break;
		case ENVIRONMENT_KDE:		name.Set("KDE"); break;
		case ENVIRONMENT_XFCE:		name.Set("Xfce"); break;
		case ENVIRONMENT_UNKNOWN:	name.Set("Unknown"); break;
		default:
			OP_ASSERT(!"missing environment name");
	}
}

bool DesktopEnvironment::IsGnome3Running()
{
	//
	// Works for Ubuntu >= 11.10 and derivatives
	//

	OpStringC8 desktop_session = op_getenv("DESKTOP_SESSION");

	if (desktop_session == "gnome-shell" || desktop_session == "gnome-fallback")
		return true;

	//
	// Some distributions report gnome 3 as 'gnome' or 'default'; in such
	// cases we can't tell the difference between gnome 2, gnome 3 (and Unity in
	// Ubuntu 11.04), so we need to look for process, that is named accordingly.
	//
	// This fails for gnome 3 in fallback mode, because it is
	// indistinguishable from gnome 2.
	//

	OpFolderLister* lister = NULL;
	RETURN_VALUE_IF_ERROR(OpFolderLister::Create(&lister), false);
	OP_ASSERT(lister);
	OpAutoPtr<OpFolderLister> lister_holder(lister);
	RETURN_VALUE_IF_ERROR(lister->Construct(UNI_L("/proc"), UNI_L("*")), false);

	while (lister->Next())
	{
		if (!lister->IsFolder())
			continue;

		OpString path;
		path.Set(lister->GetFullPath());
		path.Append("/status");

		OpFile file;
		if (OpStatus::IsError(file.Construct(path)))
			continue;

		BOOL exists;
		if (OpStatus::IsError(file.Exists(exists)))
			continue;

		if (!exists)
			continue;

		if (OpStatus::IsError(file.Open(OPFILE_READ)))
			continue;

		OpString8 line;
		if (OpStatus::IsError(file.ReadLine(line)))
		{
			file.Close();
			continue;
		}

#if defined(__linux__)
		bool found = (line == "Name:\tgnome-shell");
#elif defined(__FreeBSD__)
		bool found = (line.Compare("gnome-shell ", 12) == 0);
#else
		bool found = false;
#endif // __linux__

		file.Close();
		if (found)
			return true;
	}

	return false;
}

bool DesktopEnvironment::IsGnomeRunning()
{
	OpStringC8 xdg_current_desktop = op_getenv("XDG_CURRENT_DESKTOP");

	if (xdg_current_desktop == "GNOME")
		return true;

	//
	// fallback, this variable is deprecated
	//

	OpStringC8 gnome_desktop_session_id = op_getenv("GNOME_DESKTOP_SESSION_ID");

	return gnome_desktop_session_id.HasContent();
}

bool DesktopEnvironment::IsKDERunning()
{
	OpStringC8 kde_full_session = op_getenv("KDE_FULL_SESSION");

	return kde_full_session.HasContent();
}

bool DesktopEnvironment::IsUbuntuUnityRunning()
{
	//
	// Works for Ubuntu >= 11.10 and derivatives
	//
	// Previous versions had DESKTOP_SESSION set to gnome or gnome-classic
	// or gnome-2d depending on window manager, so we really couldn't tell
	// the difference between gnome 2, gnome 3 and unity.
	//

	OpStringC8 xdg_current_desktop = op_getenv("XDG_CURRENT_DESKTOP");

	return xdg_current_desktop == "Unity";
}

bool DesktopEnvironment::IsXfceRunning()
{
	X11Types::Display* dpy = g_x11->GetDisplay();
	X11Types::Window root  = RootWindowOfScreen( DefaultScreenOfDisplay(dpy));

	bool is_xfce_running = false;

	X11Types::Atom dt_save_mode;
	dt_save_mode = XInternAtom (dpy, "_DT_SAVE_MODE", True);
	if (dt_save_mode != None)
	{
		X11Types::Atom type;
		int format;
		unsigned long nitems, after = 1;
		unsigned char *name = 0;
		XGetWindowProperty( dpy, root,
							dt_save_mode, 0, 65536 / 4, False,
							XA_STRING, &type, &format, &nitems, &after,
							&name );
		if (name)
		{
			is_xfce_running = op_strstr( (char*)name, "xfce" );
			XFree(name);
		}
	}

	return is_xfce_running;
}

DesktopEnvironment::ToolkitType DesktopEnvironment::GetPreferredToolkit()
{
    if (m_toolkit == TOOLKIT_UNKNOWN)
	{
		unsigned toolkit = g_pcunix->GetIntegerPref(PrefsCollectionUnix::FileSelectorToolkit);
		if (toolkit < TOOLKIT_UNKNOWN && toolkit != TOOLKIT_AUTOMATIC)
			m_toolkit = (DesktopEnvironment::ToolkitType)toolkit;
		else
			DetermineToolkitAutomatically();
	}

    return m_toolkit;
}

void DesktopEnvironment::DetermineToolkitAutomatically()
{
	switch (GetToolkitEnvironment())
	{
		case ENVIRONMENT_KDE:
			m_toolkit = TOOLKIT_KDE;
			break;
		case ENVIRONMENT_GNOME: /* fallthrough */
		case ENVIRONMENT_XFCE: /* fallthrough */
		case ENVIRONMENT_UNKNOWN:
			m_toolkit = TOOLKIT_GTK;
			break;
		case ENVIRONMENT_GNOME_3:
		case ENVIRONMENT_UNITY:
#if defined(NO_AUTODETECT_GTK3)
			m_toolkit = TOOLKIT_GTK;
#else
			m_toolkit = TOOLKIT_GTK3;
#endif
			break;
		default:
			OP_ASSERT(!"Added environment, forgot toolkit?");
	}
}
