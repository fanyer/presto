/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#include "core/pch.h"

#ifdef DOM_GEOLOCATION_SUPPORT

#include "adjunct/quick/dialogs/GeolocationPrivacyDialog.h"

#include "adjunct/quick_toolkit/widgets/OpBrowserView.h"
#include "adjunct/quick/WindowCommanderProxy.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/collections/pc_geoloc.h"
#include "modules/widgets/OpButton.h"
#include "modules/windowcommander/OpWindowCommander.h"

/***********************************************************************************
**  GeolocationPrivacyDialog::OnInit
************************************************************************************/
void
GeolocationPrivacyDialog::OnInit()
{
	// initialize browser view and loading elements
	OpBrowserView* view = (OpBrowserView*)GetWidgetByName("license_browserview");
	if(view && view->GetWindow())
	{
		view->GetBorderSkin()->SetImage("Dialog Browser Skin");
		view->GetWindowCommander()->SetScriptingDisabled(TRUE);
		WindowCommanderProxy::OpenURL(view, GEOLOCATION_PRIVACY_TERMS);
		view->AddListener(this); // To be able to block context menus
	}
	// hide browser view
	ShowWidget("browserview_group", FALSE);
	SetWidgetValue("enable_geolocation", 1);

}

GeolocationPrivacyDialog::~GeolocationPrivacyDialog()
{
	OpBrowserView* view = (OpBrowserView*)GetWidgetByName("license_browserview");
	if(view)
		view->RemoveListener(this);
}

/***********************************************************************************
**  GeolocationPrivacyDialog::OnOk
************************************************************************************/
UINT32
GeolocationPrivacyDialog::OnOk()
{
	BOOL enable = FALSE;
	OpButton *checkbox = (OpButton *)GetWidgetByName("enable_geolocation");
	if(checkbox)
	{
		enable = checkbox->GetValue();
	}
	if(enable)
	{
		OP_STATUS s;
		// don't show the dialog again
		TRAP(s, g_pcui->WriteIntegerL(PrefsCollectionUI::ShowGeolocationLicenseDialog, FALSE));

		// make sure geolocation is enabled
		TRAP(s, g_pcgeolocation->WriteIntegerL(PrefsCollectionGeolocation::EnableGeolocation, TRUE));

		m_desktop_window.HandleCurrentPermission(true, m_persistence);
	}
	else
	{
		m_desktop_window.HandleCurrentPermission(false, OpPermissionListener::PermissionCallback::PERSISTENCE_TYPE_RUNTIME);
	}
	return 1;
}

/***********************************************************************************
**  GeolocationPrivacyDialog::OnCancel
************************************************************************************/
void
GeolocationPrivacyDialog::OnCancel()
{
	m_desktop_window.HandleCurrentPermission(false, OpPermissionListener::PermissionCallback::PERSISTENCE_TYPE_RUNTIME);
}

/***********************************************************************************
**	GeolocationPrivacyDialog::OnPagePopupMenu
***********************************************************************************/
BOOL
GeolocationPrivacyDialog::OnPagePopupMenu(OpWindowCommander* commander, OpDocumentContext& context)
{
	// Disable all context menus for this page
	return TRUE;
}

/***********************************************************************************
**	GeolocationPrivacyDialog::OnPageLoadingFinished
***********************************************************************************/
void GeolocationPrivacyDialog::OnPageLoadingFinished(OpWindowCommander* commander, OpLoadingListener::LoadingFinishStatus status, BOOL was_stopped_by_user)
{
	if (commander->HttpResponseIs200() == YES)
	{
		ShowBrowserView();
	}
	else
	{
		ShowBrowserViewError();
	}

}

/***********************************************************************************
**	GeolocationPrivacyDialog::ShowBrowserView
***********************************************************************************/
void
GeolocationPrivacyDialog::ShowBrowserView()
{
	// show license
	ShowWidget("browserview_group");

	// hide 'loading' info
	ShowWidget("loading_info_group", FALSE);

	// make sure relayouting is done correctly
	CompressGroups(); 
}

/***********************************************************************************
**	ShowBrowserViewError
***********************************************************************************/
void
GeolocationPrivacyDialog::ShowBrowserViewError()
{
	OpString error_text;
	g_languageManager->GetString(Str::D_FEATURE_LICENSE_LOADING_ERROR, error_text);
	SetWidgetText("license_loading_info_label", error_text.CStr());
	ShowWidget("loading_icon", FALSE);
}


void GeolocationPrivacyDialog::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if (widget == NULL)
	{
		return;
	}

	if (widget->IsNamed("enable_geolocation"))
	{			
			INT32 enable = widget->GetValue();
			OpString button_text;
			if (enable)
			{
				g_languageManager->GetString( Str::D_GEOLOCATION_PRIVACY_DIALOG_ACCEPT, button_text);
			}		
			else
			{
				g_languageManager->GetString( Str::S_GEOLOCATION_DENY, button_text);
			}
						 
			SetButtonText(0, button_text);
	}
}

#endif // DOM_GEOLOCATION_SUPPORT
