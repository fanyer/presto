/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#include "core/pch.h"

#include "adjunct/quick/view/FeatureLicenseDialog.h"

#include "adjunct/quick/WindowCommanderProxy.h"
#include "adjunct/quick_toolkit/widgets/OpBrowserView.h"
#include "adjunct/quick/WindowCommanderProxy.h"
#include "modules/locale/oplanguagemanager.h"


/***********************************************************************************
**  FeatureLicenseDialog::OnInit
************************************************************************************/
void
FeatureLicenseDialog::OnInit()
{
	// initialize browser view and loading elements
	OpBrowserView* view = (OpBrowserView*)GetWidgetByName("license_browserview");
	if(view)
	{
		view->GetBorderSkin()->SetImage("Dialog Browser Skin");
		view->GetWindowCommander()->SetScriptingDisabled(TRUE);
		WindowCommanderProxy::OpenURL(view, MYOPERA_LICENSE_TERMS);
		view->AddListener(this); // To be able to block context menus
	}
	// hide browser view
	ShowWidget("browserview_group", FALSE);
}

/***********************************************************************************
**  FeatureLicenseDialog::OnOk
************************************************************************************/
UINT32
FeatureLicenseDialog::OnOk()
{
	g_input_manager->InvokeAction(OpInputAction::ACTION_ACCEPT_FEATURE_LICENSE, 0, NULL, GetParentInputContext());
	return 1;
}

/***********************************************************************************
**  FeatureLicenseDialog::OnCancel
************************************************************************************/
void
FeatureLicenseDialog::OnCancel()
{
	g_input_manager->InvokeAction(OpInputAction::ACTION_DECLINE_FEATURE_LICENSE, 0, NULL, GetParentInputContext());
}

/***********************************************************************************
**  FeatureLicenseDialog::OnInputAction
************************************************************************************/
BOOL
FeatureLicenseDialog::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
    case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
			case OpInputAction::ACTION_OK:
			case OpInputAction::ACTION_CANCEL:
				{
					// I'm not really sure why this is necessary, but I need to explicitly set
					// the action state to TRUE to make sure the OK/Cancel buttons are always enabled
					child_action->SetEnabled(TRUE);
					return TRUE;
				}
			}
			break;
		}
	}
	return Dialog::OnInputAction(action);
}

/***********************************************************************************
**	FeatureLicenseDialog::OnPagePopupMenu
***********************************************************************************/
BOOL
FeatureLicenseDialog::OnPagePopupMenu(OpWindowCommander* commander, OpDocumentContext* ctx)
{
	// Disable all context menus for this page
	return TRUE;
}

/***********************************************************************************
**	FeatureLicenseDialog::OnLoadingFinished
***********************************************************************************/
void
FeatureLicenseDialog::OnPageLoadingFinished(OpWindowCommander* commander, OpLoadingListener::LoadingFinishStatus status, BOOL stopped_by_user)
{
	if (stopped_by_user ||
		WindowCommanderProxy::HasDownloadError(commander))
	{
		ShowBrowserViewError();
	}
	else
	{
		ShowBrowserView();
	}
}

/***********************************************************************************
**	FeatureLicenseDialog::ShowBrowserView
***********************************************************************************/
void
FeatureLicenseDialog::ShowBrowserView()
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
FeatureLicenseDialog::ShowBrowserViewError()
{
	OpString error_text;
	g_languageManager->GetString(Str::D_FEATURE_LICENSE_LOADING_ERROR, error_text);
	SetWidgetText("license_loading_info_label", error_text.CStr());
	ShowWidget("loading_icon", FALSE);
}
