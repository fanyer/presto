/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef WIDGET_RUNTIME_SUPPORT
#ifdef DOM_GEOLOCATION_SUPPORT

#include "adjunct/desktop_util/prefs/PrefsCollectionDesktopUI.h"
#include "adjunct/widgetruntime/GadgetPermissionListener.h"
#include "adjunct/widgetruntime/GadgetGeolocationDialog.h"
#include "adjunct/widgetruntime/GadgetPreferencesDialog.h"
#include "adjunct/widgetruntime/GadgetUiContext.h"
#include "adjunct/quick/Application.h"

#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/prefs/prefsmanager/collections/pc_geoloc.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/url/url_man.h"
#include "modules/gadgets/OpGadget.h"


GadgetPermissionListener::GadgetPermissionListener(DesktopWindow* parent):
	m_parent_window(parent),
	m_remember(false)
{
}

void GadgetPermissionListener::OnAskForPermission(OpWindowCommander *commander, PermissionCallback *callback)
{
	DesktopPermissionCallback *desktop_callback = OP_NEW(DesktopPermissionCallback, (callback));
	if(!desktop_callback)
	{
		return;
	}

	OpString hostname;
	if(callback->Type() == PermissionCallback::PERMISSION_TYPE_GEOLOCATION_REQUEST)
	{
		OP_ASSERT(callback->GetURL());
		URL url = urlManager->GetURL(callback->GetURL());

		if(!url.IsEmpty())
		{
			hostname.Set(url.GetServerName()->UniName());
		}
	}

	if(!hostname.HasContent())
	{
		g_languageManager->GetString(Str::SI_IDSTR_DOWNLOAD_DLG_UNKNOWN_SERVER, hostname);
	}

	desktop_callback->SetAccessingHostname(hostname);
	if(m_permission_callbacks.GetCount())
	{
		// we already have callbacks waiting for input, delay this one
		m_permission_callbacks.AddItem(desktop_callback);
	}
	else
	{
		m_permission_callbacks.AddItem(desktop_callback);

		// nothing else in the queue, show the toolbar now
		if(callback->Type() == PermissionCallback::PERMISSION_TYPE_GEOLOCATION_REQUEST)
		{
			g_main_message_handler->PostDelayedMessage(MSG_QUICK_HANDLE_NEXT_PERMISSION_CALLBACK, (MH_PARAM_1) m_parent_window, 0, 0 );
		}
	}
}


void GadgetPermissionListener::OnAskForPermissionCancelled(OpWindowCommander *wic, OpPermissionListener::PermissionCallback *callback)
{
}

void GadgetPermissionListener::OnUserConsentInformation(OpWindowCommander* comm, const OpVector<OpPermissionListener::ConsentInformation>& info_vector, INTPTR user_data) 
{
	GadgetPreferencesDialog::PermissionType permission_type = GadgetPreferencesDialog::ASK;
	const uni_char* wuid = comm->GetGadget()->GetIdentifier();
	for (int i = 0, end = info_vector.GetCount(); i < end; ++i)
	{
		OpPermissionListener::ConsentInformation* info = info_vector.Get(i);
		if (info->hostname == wuid && info->permission_type == OpPermissionListener::PermissionCallback::PERMISSION_TYPE_GEOLOCATION_REQUEST)
		{
			if (info->persistence_type == OpPermissionListener::PermissionCallback::PERSISTENCE_TYPE_ALWAYS)
			{
				permission_type = info->is_allowed == TRUE ? GadgetPreferencesDialog::ALLOW : GadgetPreferencesDialog::DISALLOW;
			}
			break;
		}
	}
	GadgetPreferencesDialog* dialog = OP_NEW(GadgetPreferencesDialog, (permission_type));
	if (dialog)
	{
		OpStatus::Ignore(dialog->Init());
	}
}

void GadgetPermissionListener::ProcessNextPermission(bool accept)
{
	if(!m_permission_callbacks.HasType(PermissionCallback::PERMISSION_TYPE_GEOLOCATION_REQUEST))
	{
		return;
	}
	DesktopPermissionCallback *callback = m_permission_callbacks.First();
	PermissionCallback *permission_callback = callback->GetPermissionCallback();

	if(accept)
		permission_callback->OnAllowed(m_remember ? OpPermissionListener::PermissionCallback::PERSISTENCE_TYPE_ALWAYS : OpPermissionListener::PermissionCallback::PERSISTENCE_TYPE_RUNTIME);
	else
		permission_callback->OnDisallowed(m_remember ? OpPermissionListener::PermissionCallback::PERSISTENCE_TYPE_ALWAYS : OpPermissionListener::PermissionCallback::PERSISTENCE_TYPE_RUNTIME);

	m_permission_callbacks.DeleteItem(callback);

	if(m_permission_callbacks.GetCount())
	{
		// we have more to process
		g_main_message_handler->PostDelayedMessage(MSG_QUICK_HANDLE_NEXT_PERMISSION_CALLBACK, (MH_PARAM_1) m_parent_window, 0, 0 );
	}
}

void GadgetPermissionListener::ProcessCallback()
{
	DesktopPermissionCallback *callback = m_permission_callbacks.First();
	PermissionCallback *permission_callback = callback->GetPermissionCallback();

	OP_ASSERT(permission_callback && permission_callback->Type() == PermissionCallback::PERMISSION_TYPE_GEOLOCATION_REQUEST);
	if(permission_callback && permission_callback->Type() == PermissionCallback::PERMISSION_TYPE_GEOLOCATION_REQUEST)
	{
		OpString hostname;
		hostname.Set(callback->GetAccessingHostname());
		ShowGeolocationDialogs(hostname);
	}
}

void GadgetPermissionListener::ShowGeolocationDialogs(OpString hostname)
{
	OpAutoPtr<GadgetGeolocationDialog> dialog(OP_NEW(GadgetGeolocationDialog, (this)));
	if (NULL != dialog.get())
	{
		if (OpStatus::IsSuccess(dialog->Init(m_parent_window,hostname)))
		{
			dialog.release();
		}
	}
}
#endif // DOM_GEOLOCATION_SUPPORT
#endif // WIDGET_RUNTIME_SUPPORT
