/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef GADGET_PERMISSION_LISTENER_H
#define GADGET_PERMISSION_LISTENER_H

#ifdef WIDGET_RUNTIME_SUPPORT
#ifdef DOM_GEOLOCATION_SUPPORT

#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "adjunct/quick/permissions/DesktopPermissionCallback.h"

#include "modules/windowcommander/OpWindowCommander.h"
#include "modules/inputmanager/inputcontext.h"

/**
 * @author Blazej Kazmierczak (bkazmierczak)
 */
class GadgetPermissionListener : public OpPermissionListener, public OpInputContext
{
	public:

		GadgetPermissionListener(DesktopWindow* parent);

	// == OpPermissionListener ===================
		virtual void	OnAskForPermission(OpWindowCommander *commander, PermissionCallback *callback);
		virtual void	OnAskForPermissionCancelled(OpWindowCommander *wic, PermissionCallback *callback);
		virtual void	OnUserConsentInformation(OpWindowCommander* comm, const OpVector<OpPermissionListener::ConsentInformation>& info, INTPTR user_data);
		virtual void	OnUserConsentError(OpWindowCommander* comm, OP_STATUS status, INTPTR user_data) { OP_ASSERT(!"GetUserConsent failed"); }
		
	// == OpInputContext ===================
		virtual const char*		GetInputContextName() { return "Gadget Permission Listener"; }
		virtual void	ProcessCallback();
	protected: 		
		void			ShowGeolocationDialogs( OpString hostname );
		void			ProcessNextPermission(bool accept);

		DesktopPermissionCallbackQueue	m_permission_callbacks;
		DesktopWindow*					m_parent_window;
		bool							m_remember; // remember my choice checkbox on the dialog
};

#endif // DOM_GEOLOCATION_SUPPORT
#endif // WIDGET_RUNTIME_SUPPORT
#endif // GADGET_PERMISSION_LISTENER_H
