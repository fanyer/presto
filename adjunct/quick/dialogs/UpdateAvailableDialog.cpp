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

#ifdef AUTO_UPDATE_SUPPORT

#include "UpdateAvailableDialog.h"

#include "adjunct/quick/WindowCommanderProxy.h"
#include "adjunct/quick_toolkit/widgets/OpBrowserView.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "modules/doc/frm_doc.h"
#include "modules/dochand/win.h"
#include "modules/widgets/OpButton.h"
#include "modules/widgets/OpMultiEdit.h"
#include "modules/locale/oplanguagemanager.h"

/***********************************************************************************
**
**	UpdateAvailableDialog
**
***********************************************************************************/

UpdateAvailableDialog::UpdateAvailableDialog(AvailableUpdateContext* context)
{
	m_update_size = context->update_size;
	m_update_info_url.Set(context->update_info_url.CStr());

#if defined(AUTOUPDATE_PACKAGE_INSTALLATION)
	g_languageManager->GetString(Str::D_UPDATE_DOWNLOAD_INSTALL, m_ok_text);
	g_languageManager->GetString(Str::D_UPDATE_REMIND_LATER, m_cancel_text);
#else
	g_languageManager->GetString(GetOkTextID(), m_ok_text);
#endif

}


/***********************************************************************************
**
**	~UpdateAvailableDialog
**
***********************************************************************************/

UpdateAvailableDialog::~UpdateAvailableDialog()
{
}

/***********************************************************************************
**
**	GetDialogType
**
***********************************************************************************/

Dialog::DialogType UpdateAvailableDialog::GetDialogType()
{
#if defined(AUTOUPDATE_PACKAGE_INSTALLATION)
	return TYPE_OK_CANCEL;
#else
	return TYPE_OK;
#endif
}

/***********************************************************************************
**
**	GetOkAction
**
***********************************************************************************/
OpInputAction* UpdateAvailableDialog::GetOkAction()
{
	return OP_NEW(OpInputAction, (OpInputAction::ACTION_START_AUTO_UPDATE));
}

/***********************************************************************************
**
**	GetCancelAction
**
***********************************************************************************/
OpInputAction* UpdateAvailableDialog::GetCancelAction()
{
#if defined(AUTOUPDATE_PACKAGE_INSTALLATION)
	return OP_NEW(OpInputAction, (OpInputAction::ACTION_MINIMIZE_AUTO_UPDATE_DIALOG));
#else
	// should never go there, doesn't have this button
	OP_ASSERT(FALSE);
	return NULL;
#endif
}

/***********************************************************************************
**
**	OnInit
**
***********************************************************************************/
void UpdateAvailableDialog::OnInit()
{
	// check check-box
	OpCheckBox* checkbox = (OpCheckBox*)GetWidgetByName("auto_install_checkbox");
	if (checkbox)
	{
#ifndef AUTOUPDATE_PACKAGE_INSTALLATION
		checkbox->SetVisibility(FALSE);
#else
		int automationLevel = g_pcui->GetIntegerPref(PrefsCollectionUI::LevelOfUpdateAutomation);
		int autoInstallUpdates = ( automationLevel == AutoInstallUpdates ) ? 1 : 0;
		checkbox->SetValue(autoInstallUpdates);
#endif // !defined(MSWIN) && !defined(_MACINTOSH_)
	}

	// initialize browser view and loading elements
	OpBrowserView* view = (OpBrowserView*)GetWidgetByName("update_browserview");
	if(WindowCommanderProxy::HasWindow(view))
	{
		view->GetWindowCommander()->SetScriptingDisabled(TRUE);
		WindowCommanderProxy::OpenURL(view, m_update_info_url.CStr());
		view->AddListener(this); // To be able to block context menus

		view->SetVisibility(FALSE);
	}

	OpMultilineEdit* description = (OpMultilineEdit*)GetWidgetByName("install_description_edit");
	if (description)
	{
		description->SetLabelMode();
#ifndef AUTOUPDATE_PACKAGE_INSTALLATION
		OpString tmp;
		g_languageManager->GetString(Str::D_UPDATE_DESCRIPTION_UNIX, tmp);
		description->SetText(tmp.CStr());
#endif
	}

	OpLabel* label = (OpLabel*)GetWidgetByName("size_label");
	if (label)
	{
#if defined(AUTOUPDATE_PACKAGE_INSTALLATION)
		if (m_update_size > 0)
		{
			OpString size_string, tmp;
			g_languageManager->GetString(Str::D_UPDATE_SIZE, tmp);
			size_string.AppendFormat(tmp.CStr(), (float) m_update_size / (float) 1048576);
			label->SetText(size_string.CStr());
		}
		else
#endif // !defined(MSWIN) && !defined(_MACINTOSH_)
		{
			label->SetVisibility(FALSE);
		}
	}
}

BOOL UpdateAvailableDialog::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
/* Uncomment this if there are actual cases, otherwise MSVC issues warnings

	case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
			}
			break;
		}
*/
	case OpInputAction::ACTION_START_AUTO_UPDATE:
		{
			g_autoupdate_manager->DownloadUpdate();
			OnOk();	// is not called automatically, as the default ok-action is overridden
			CloseDialog(FALSE);
			return TRUE;
		}

	case OpInputAction::ACTION_MINIMIZE_AUTO_UPDATE_DIALOG:
		{
			// see OnCancel()
			CloseDialog(TRUE);
			return TRUE;
		}
	}

	return Dialog::OnInputAction(action);
}

UINT32 UpdateAvailableDialog::OnOk()
{
	OpCheckBox* checkbox = (OpCheckBox*)GetWidgetByName("auto_install_checkbox");
	if (checkbox)
	{
		LevelOfAutomation levelOfAutomation = CheckForUpdates;
		if (checkbox->GetValue() == 1) // if checkbox is checked
		{
			levelOfAutomation = AutoInstallUpdates;			
		}
		OP_STATUS st;
		TRAP_AND_RETURN_VALUE_IF_ERROR(st, g_pcui->WriteIntegerL(PrefsCollectionUI::LevelOfUpdateAutomation, levelOfAutomation), 0);
		TRAP_AND_RETURN_VALUE_IF_ERROR(st, g_prefsManager->CommitL(), 0);
	}

	return 0;
}

void UpdateAvailableDialog::OnCancel()
{
	g_autoupdate_manager->DeferUpdate();
}

/***********************************************************************************
**
**	OnPagePopupMenu
**
***********************************************************************************/
BOOL UpdateAvailableDialog::OnPagePopupMenu(OpWindowCommander* commander, OpDocumentContext& context)
{
	// Disable all context menus for this page
	return TRUE;
}

/***********************************************************************************
**
**	OnLoadingFinished
**
***********************************************************************************/
void UpdateAvailableDialog::OnPageLoadingFinished(OpWindowCommander* commander, OpLoadingListener::LoadingFinishStatus status, BOOL stopped_by_user)
{
	if (!stopped_by_user &&
		WindowCommanderProxy::HasCurrentDoc(commander) &&
		!WindowCommanderProxy::HasDownloadError(commander))
	{
		ShowBrowserView();
	}
	else
	{
		ShowBrowserViewError();
	}
}
						

/***********************************************************************************
**
**	ShowBrowserView
**
***********************************************************************************/
void UpdateAvailableDialog::ShowBrowserView()
{
	ShowWidget("update_browserview");

	// hide 'loading' widgets
	ShowWidget("update_available_label", FALSE);
	ShowWidget("update_loading_info_label", FALSE);
	ShowWidget("loading_icon", FALSE);
}

/***********************************************************************************
**
**	ShowBrowserViewError
**
***********************************************************************************/
void UpdateAvailableDialog::ShowBrowserViewError()
{
//	SetWidgetText("update_loading_info_label", Str::D_UPDATE_INFO_LOADING_ERROR);
	SetWidgetText("update_loading_info_label", UNI_L("error loading update")); // TODO: localize
	ShowWidget("loading_icon", FALSE);
}

#endif // AUTO_UPDATE_SUPPORT
