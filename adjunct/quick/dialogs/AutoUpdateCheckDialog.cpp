/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef AUTO_UPDATE_SUPPORT

#include "adjunct/quick/dialogs/AutoUpdateCheckDialog.h"
#include "adjunct/quick_toolkit/widgets/OpBar.h"

#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"

/***********************************************************************************
**	Constructor
**  Adds itself as a listener of AutoUpdateManager, retrieves some default strings.
**
**  AutoUpdateCheckDialog::AutoUpdateCheckDialog
**
***********************************************************************************/
AutoUpdateCheckDialog::AutoUpdateCheckDialog()
{
	g_autoupdate_manager->AddListener(this);
	g_languageManager->GetString(Str::D_UPDATE_MINIMIZE, m_minimize_text);
}


/***********************************************************************************
**  Destructor
**  Removes itself as a listener of AutoUpdateManager.
**
**	AutoUpdateCheckDialog::~AutoUpdateCheckDialog
**
***********************************************************************************/
AutoUpdateCheckDialog::~AutoUpdateCheckDialog()
{
	g_autoupdate_manager->RemoveListener(this);
}

/***********************************************************************************
**  Sets the text of the OK button.
**
**	AutoUpdateCheckDialog::GetOkText
**
***********************************************************************************/
const uni_char* AutoUpdateCheckDialog::GetOkText()
{
	g_languageManager->GetString(Dialog::GetCancelTextID(), m_cancel_text);
	return m_cancel_text.CStr();
}


/***********************************************************************************
**  Sets the action of the OK button.
**
**	AutoUpdateCheckDialog::GetOkAction
**
***********************************************************************************/
OpInputAction* AutoUpdateCheckDialog::GetOkAction()
{
	return OP_NEW(OpInputAction, (OpInputAction::ACTION_CANCEL_AUTO_UPDATE));
}


/***********************************************************************************
**  Sets the text of the Cancel button. The Cancel button the one that minimizes the 
**  update.
**
**	AutoUpdateCheckDialog::GetCancelText
**
***********************************************************************************/
const uni_char* AutoUpdateCheckDialog::GetCancelText()	
{
	g_languageManager->GetString(Str::D_UPDATE_MINIMIZE, m_minimize_text);
	return m_minimize_text.CStr();
}


/***********************************************************************************
**  Initializes the dialog. Saves regularly updated widgets for later use.
**
**	AutoUpdateCheckDialog::OnInit
**
***********************************************************************************/
void AutoUpdateCheckDialog::OnInit()
{
}


/***********************************************************************************
**  Handles dialog actions, triggered by the custom actions set for OK and Cancel
**  button, and the 'resume' button. It informs the autoupdate manager about
**  decisions made by the user.
**
**	AutoUpdateCheckDialog::OnInputAction
**  @param	action	The action that needs to be handled
**
***********************************************************************************/
BOOL AutoUpdateCheckDialog::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_MINIMIZE_AUTO_UPDATE_DIALOG:
				{
					INT32 statusbar_alignment = g_pcui->GetIntegerPref(PrefsCollectionUI::StatusbarAlignment);
					if(statusbar_alignment == OpBar::ALIGNMENT_OFF)
					{
						child_action->SetEnabled(FALSE);
					}
					return TRUE;
				}
			}
			break;
		}

		case OpInputAction::ACTION_CANCEL_AUTO_UPDATE:
		{
			CloseDialog(FALSE); // cancel means: minimize
			g_autoupdate_manager->CancelUpdate();
			return TRUE;
		}

		case OpInputAction::ACTION_MINIMIZE_AUTO_UPDATE_DIALOG:
		{
			CloseDialog(TRUE); // cancel means: minimize
			return TRUE;
		}
	}

	return Dialog::OnInputAction(action);
}


/***********************************************************************************
**  Cancel minimizes the update, ie. closes the dialog and shows the 'minimized update'
**  button in the toolbar. Triggered both by the 'X' in the dialog decoration and the
**  cancel button.
**
**	AutoUpdateCheckDialog::OnCancel
**
***********************************************************************************/
void AutoUpdateCheckDialog::OnCancel()
{
	g_autoupdate_manager->SetUpdateMinimized(TRUE);
}


/***********************************************************************************
**  
**
**	AutoUpdateCheckDialog::OnUpToDate
**
***********************************************************************************/
void AutoUpdateCheckDialog::OnUpToDate()
{
	CloseDialog(FALSE);
}


/***********************************************************************************
**  
**
**	AutoUpdateCheckDialog::OnChecking
**
***********************************************************************************/
void AutoUpdateCheckDialog::OnChecking()
{
	 // this callback should never be called within this dialog 
	OP_ASSERT(FALSE);
	CloseDialog(FALSE);
}


/***********************************************************************************
**  
**
**	AutoUpdateCheckDialog::OnUpdateAvailable
**  @param	context	The context of the available update
**
***********************************************************************************/
void AutoUpdateCheckDialog::OnUpdateAvailable(AvailableUpdateContext* context)
{
	CloseDialog(FALSE);
}


/***********************************************************************************
**  
**
**	AutoUpdateCheckDialog::OnError
**  @param	context
**
***********************************************************************************/
void AutoUpdateCheckDialog::OnError(UpdateErrorContext* context)
{
	CloseDialog(FALSE);
}


/***********************************************************************************
**  Minimizing the dialog is not actually minimizing it, but closing and re-opening
**  it.
**
**	AutoUpdateCheckDialog::OnMinimizedStateChanged
**  @param	minimized	If auto-update dialogs are minimized or not
**  @see	AutoUpdateCheckDialog::OnCancel
**
***********************************************************************************/
void AutoUpdateCheckDialog::OnMinimizedStateChanged(BOOL minimized)
{
	 // do nothing: the dialog is about to close (see OnCancel)
}

#endif // AUTO_UPDATE_SUPPORT
