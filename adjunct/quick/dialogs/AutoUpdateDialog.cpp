/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** @author Manuela Hutter (manuelah)
*/

#include "core/pch.h"

#ifdef AUTO_UPDATE_SUPPORT

#include "AutoUpdateDialog.h"

#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "adjunct/quick_toolkit/widgets/OpBar.h"
#include "adjunct/quick_toolkit/widgets/OpProgressbar.h"
#include "adjunct/quick_toolkit/widgets/OpButtonStrip.h"
#include "adjunct/autoupdate/updater/pi/aufileutils.h"
#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"
#include "modules/widgets/OpButton.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"

#ifdef MSWIN
#include "platforms/windows/utils/authorization.h"
#include "platforms/windows/installer/OperaInstaller.h"
#endif // MSWIN

/***********************************************************************************
**	Constructor
**  Adds itself as a listener of AutoUpdateManager, retrieves some default strings.
**
**  AutoUpdateDialog::AutoUpdateDialog
**
***********************************************************************************/
AutoUpdateDialog::AutoUpdateDialog()
: m_progress(NULL),
  m_summary(NULL),
  m_info(NULL),
  m_ready_for_install(FALSE)
{
	g_autoupdate_manager->AddListener(this);
	g_languageManager->GetString(Str::D_UPDATE_MINIMIZE, m_minimize_text);
	g_languageManager->GetString(Str::D_UPDATE_DOWNLOADING_INFO, m_downloading_info_text);
}


/***********************************************************************************
**  Destructor
**  Removes itself as a listener of AutoUpdateManager.
**
**	AutoUpdateDialog::~AutoUpdateDialog
**
***********************************************************************************/
AutoUpdateDialog::~AutoUpdateDialog()
{
	g_autoupdate_manager->RemoveListener(this);
}

/***********************************************************************************
**  Sets the text of the OK button. The OK button is always the one that isn't
**  minimizing the update. In case of downloading progress, it shows 'Cancel' text,
**  if it is ready for install, it shows 'Install and Restart Now' text.
**
**	AutoUpdateDialog::GetOkText
**
***********************************************************************************/
const uni_char* AutoUpdateDialog::GetOkText()
{
	if (m_ready_for_install)
	{
		g_languageManager->GetString(Str::D_UPDATE_INSTALL_AND_RESTART, m_cancel_text);
	}
	else
	{
		g_languageManager->GetString(Dialog::GetCancelTextID(), m_cancel_text);
	}
	return m_cancel_text.CStr();
}


/***********************************************************************************
**  Sets the action of the OK button. The OK button is always the one that isn't
**  minimizing the update. In case of downloading progress, it sets a 'Cancel' action,
**  if it is ready for install, it sets 'Install and Restart Now'.
**
**	AutoUpdateDialog::GetOkAction
**
***********************************************************************************/
OpInputAction* AutoUpdateDialog::GetOkAction()
{
	if (m_ready_for_install)
	{
		return OP_NEW(OpInputAction, (OpInputAction::ACTION_RESTART_OPERA));
	}
	else
	{
		return OP_NEW(OpInputAction, (OpInputAction::ACTION_CANCEL_AUTO_UPDATE));
	}
}


/***********************************************************************************
**  Sets the text of the Cancel button. The Cancel button the one that minimizes the 
**  update.
**
**	AutoUpdateDialog::GetCancelText
**
***********************************************************************************/
const uni_char* AutoUpdateDialog::GetCancelText()	
{
	if (m_ready_for_install)
	{
		g_languageManager->GetString(Str::D_UPDATE_INSTALL_ON_NEXT_RESTART, m_minimize_text);
	}
	else
	{
		INT32 statusbar_alignment = g_pcui->GetIntegerPref(PrefsCollectionUI::StatusbarAlignment);
		if(statusbar_alignment == OpBar::ALIGNMENT_OFF && m_progress && m_progress->IsVisible())
		{
			g_languageManager->GetString(Str::D_UPDATE_MINIMIZE, m_minimize_text);
		}
		else
		{
			g_languageManager->GetString(Str::D_UPDATE_HIDE_DOWNLOAD_PROGRESS, m_minimize_text);
		}
	}
	return m_minimize_text.CStr();
}


/***********************************************************************************
**  Initializes the dialog. Saves regularly updated widgets for later use.
**
**	AutoUpdateDialog::OnInit
**
***********************************************************************************/
void AutoUpdateDialog::OnInit()
{
	// set controls that are regularly updated
	m_progress = (OpProgressBar*)GetWidgetByName("update_progress");
	m_summary = (OpLabel*)GetWidgetByName("update_summary_label");
	m_info = (OpLabel*)GetWidgetByName("update_info_label");

	OP_ASSERT(m_progress);
	OP_ASSERT(m_summary);
	OP_ASSERT(m_info);

	if (m_info)
	{
		m_info->SetWrap(TRUE);
	}
	if (m_progress)
	{
		m_progress->SetType(OpProgressBar::Percentage_Label);
	}
}


/***********************************************************************************
**  Handles dialog actions, triggered by the custom actions set for OK and Cancel
**  button, and the 'resume' button. It informs the autoupdate manager about
**  decisions made by the user.
**
**	AutoUpdateDialog::OnInputAction
**  @param	action	The action that needs to be handled
**
***********************************************************************************/
BOOL AutoUpdateDialog::OnInputAction(OpInputAction* action)
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

	case OpInputAction::ACTION_RESUME_AUTO_UPDATE:
		{
			g_autoupdate_manager->DownloadUpdate();
			return TRUE;
		}
	}

	return Dialog::OnInputAction(action);
}


/***********************************************************************************
**  Cancel minimizes the update, ie. closes the dialog and shows the 'minimized update'
**  button in the toolbar. Triggered both by the 'X' in the dialog decoration and the
**  candel button.
**
**	AutoUpdateDialog::OnCancel
**
***********************************************************************************/
void AutoUpdateDialog::OnCancel()
{
	g_autoupdate_manager->SetUpdateMinimized(TRUE);
}


/***********************************************************************************
**  
**
**	AutoUpdateDialog::SetContext
**  @param	context	The current download progress context
**  @see	AutoUpdateManager::UpdateProgressContext
**
***********************************************************************************/
OP_STATUS AutoUpdateDialog::SetContext(UpdateProgressContext* context)
{
	OP_ASSERT(context);
	// todo: check if dialog is initialized

	if (!context)
	{
		return OpStatus::ERR;
	}

	if (context->is_ready)
	{
		SetReadyContext(context);
	}
	else if (context->is_preparing)
	{
		SetPreparingContext(context);
	}
	else
	{
		SetDownloadingContext(context);
	}
	
	return OpStatus::OK;
}


/***********************************************************************************
**  
**
**	AutoUpdateDialog::OnUpToDate
**
***********************************************************************************/
void AutoUpdateDialog::OnUpToDate()
{
	 // this callback should never be called within this dialog 
	OP_ASSERT(FALSE);
	CloseDialog(TRUE);
}


/***********************************************************************************
**  
**
**	AutoUpdateDialog::OnChecking
**
***********************************************************************************/
void AutoUpdateDialog::OnChecking()
{
	 // this callback should never be called within this dialog 
	OP_ASSERT(FALSE);
	CloseDialog(TRUE);
}


/***********************************************************************************
**  
**
**	AutoUpdateDialog::OnUpdateAvailable
**  @param	context	The context of the available update
**
***********************************************************************************/
void AutoUpdateDialog::OnUpdateAvailable(AvailableUpdateContext* context)
{
	// this callback should never be called within this dialog 
	OP_ASSERT(FALSE);
	CloseDialog(TRUE);
}


/***********************************************************************************
**  
**
**	AutoUpdateDialog::OnDownloading
**  @param	context	Information about the ongoing download
**
***********************************************************************************/
void AutoUpdateDialog::OnDownloading(UpdateProgressContext* context)
{
	SetContext(context);
}


/***********************************************************************************
 **  
 **
 **	AutoUpdateDialog::OnDownloadingFailed
 **
 ***********************************************************************************/
void AutoUpdateDialog::OnDownloadingFailed()
{
	CloseDialog(FALSE);
}


/***********************************************************************************
**  
**
**	AutoUpdateDialog::OnPreparing
**  @param	context	Information about the ongoing update
**
***********************************************************************************/
void AutoUpdateDialog::OnPreparing(UpdateProgressContext* context)
{
	SetContext(context);
}

/***********************************************************************************
**  
**
**	AutoUpdateDialog::OnReadyToInstall
**  @param	version	Opera Version downloaded and to be installed
**
***********************************************************************************/
void AutoUpdateDialog::OnReadyToInstall(UpdateProgressContext* context)
{
	SetContext(context);
}


/***********************************************************************************
**  
**
**	AutoUpdateDialog::OnError
**  @param	context
**
***********************************************************************************/
void AutoUpdateDialog::OnError(UpdateErrorContext* context)
{
	CloseDialog(FALSE); // errors are handled in UpdateErrorDialogs
}


/***********************************************************************************
**  Minimizing the dialog is not actually minimizing it, but closing and re-opening
**  it.
**
**	AutoUpdateDialog::OnMinimizedStateChanged
**  @param	minimized	If auto-update dialogs are minimized or not
**  @see	AutoUpdateDialog::OnCancel
**
***********************************************************************************/
void AutoUpdateDialog::OnMinimizedStateChanged(BOOL minimized)
{
	 // do nothing: the dialog is about to close (see OnCancel)
}


/***********************************************************************************
** Adds UAC shield to button "Install now" when it's needed.
**
**	AutoUpdateDialog::AddUACShieldIfNeeded
**
***********************************************************************************/
void AutoUpdateDialog::AddUACShieldIfNeeded()
{
#ifdef MSWIN
	OpAutoPtr<OperaInstaller> installer(OP_NEW(OperaInstaller, ()));
	if (!installer.get()) // OOM
		return;

	BOOL needs_elevation = FALSE;
	if (!WindowsUtils::IsPrivilegedUser(FALSE))
	{
		OperaInstaller::Settings settings = installer->GetSettings();
		if (settings.all_users)
			needs_elevation = TRUE;
		else if (OpStatus::IsError(installer->PathRequireElevation(needs_elevation)))
			needs_elevation = TRUE;
	}

	if (!needs_elevation)
		return;

	Image uacShield;
	static_cast<DesktopOpSystemInfo*>(g_op_system_info)->GetImage(DesktopOpSystemInfo::PLATFORM_IMAGE_SHIELD, uacShield);
	OpButtonStrip* btn_strip = GetButtonStrip();
	OP_ASSERT(btn_strip);
	if(btn_strip)
	{
		OpButton *button = static_cast<OpButton*>(btn_strip->GetWidgetByTypeAndId(WIDGET_TYPE_BUTTON, 0));
		if(button)
		{
			button->GetForegroundSkin()->SetBitmapImage(uacShield, FALSE);
			button->SetButtonStyle(OpButton::STYLE_IMAGE_AND_TEXT_CENTER);
		}
	}
#endif // MSWIN
}

/***********************************************************************************
**  When the dialog changes into another state (eg from 'downloading' into 'ready
**  for installation', the text and action on the dialog's button panel need to
**  be changed. There's no automated way to do this, therefore this function.
**  Knowing that the dialog is of type TYPE_OK_CANCEL and that the functions to
**  retrieve text and action are in place, they are used to refresh button 0 and 1.
**  As a last step, the dialog is told to reset itself to apply these changes
**  (also layouting-wise).
**
**	AutoUpdateDialog::RefreshButtons
**
***********************************************************************************/
void AutoUpdateDialog::RefreshButtons()
{
	SetButtonText(0, GetOkText());
	SetButtonAction(0, GetOkAction());

	SetButtonText(1, GetCancelText());
	SetButtonAction(1, GetCancelAction());

	AddUACShieldIfNeeded();

	ResetDialog();
}


/***********************************************************************************
**
**	AutoUpdateDialog::SetDownloadingContext
**  @param	context
**
***********************************************************************************/
void AutoUpdateDialog::SetDownloadingContext(UpdateProgressContext* context)
{
	OpString tmp;
	
	if (m_summary)
	{
		OpString main_string, type_string;
		g_languageManager->GetString(Str::D_UPDATE_DOWNLOADING_TEXT, main_string);

		switch (context->type)
		{
		case UpdateProgressContext::BrowserUpdate:
			{
				g_languageManager->GetString(Str::D_UPDATE_DOWNLOADING_TYPE_BROWSER, type_string);
				break;
			}
		case UpdateProgressContext::DictionaryUpdate:
			{
				g_languageManager->GetString(Str::D_UPDATE_DOWNLOADING_TYPE_DICTIONARY, type_string);
				break;
			}
		case UpdateProgressContext::ResourceUpdate:
		default:
			{
				g_languageManager->GetString(Str::D_UPDATE_DOWNLOADING_TYPE_RESOURCE, type_string);
			}
		}
		tmp.AppendFormat(main_string.CStr(), type_string.CStr());
		m_summary->SetText(tmp);
		tmp.Empty();
	}
	if (m_info)
	{
		OpString time;
		OpString format_string;
		int total_time = context->time_estimate;
		int hours = total_time / 3600;
		if(hours > 0)
		{
			if(hours == 1)
			{
				g_languageManager->GetString(Str::D_UPDATE_DOWNLOADING_INFO_HOUR, format_string);
			}
			else
			{
				if(hours < 24)
				{
					g_languageManager->GetString(Str::D_UPDATE_DOWNLOADING_INFO_HOURS, format_string);
				}
				else
				{
					g_languageManager->GetString(Str::D_UPDATE_DOWNLOADING_INFO_TIME_UNKNOWN, format_string);
				}
			}
			time.AppendFormat(format_string, hours);
		}
		else
		{
			int minutes = total_time / 60;
			if(minutes > 0)
			{
				if(minutes == 1)
				{
					g_languageManager->GetString(Str::D_UPDATE_DOWNLOADING_INFO_MINUTE, format_string);
				}
				else
				{
					g_languageManager->GetString(Str::D_UPDATE_DOWNLOADING_INFO_MINUTES, format_string);
				}
				time.AppendFormat(format_string, minutes);
			}
			else
			{
				int seconds = total_time;
				if(seconds == 1)
				{
					g_languageManager->GetString(Str::D_UPDATE_DOWNLOADING_INFO_SECOND, format_string);
				}
				else
				{
					g_languageManager->GetString(Str::D_UPDATE_DOWNLOADING_INFO_SECONDS, format_string);
				}
				time.AppendFormat(format_string, seconds);
			}
		}
		tmp.AppendFormat(m_downloading_info_text, time.CStr(), context->kbps);
		m_info->SetText(tmp);
	}
	if (m_progress)
	{
		m_progress->SetVisibility(TRUE);
		m_progress->SetProgress(context->downloaded_size, context->total_size);
	}
}


/***********************************************************************************
**
**	AutoUpdateDialog::SetPreparingContext
**  @param	context
**
***********************************************************************************/
void AutoUpdateDialog::SetPreparingContext(UpdateProgressContext* context)
{
	OpString tmp;
	if (m_summary)
	{
		g_languageManager->GetString(Str::D_UPDATE_PREPARING, tmp);
		m_summary->SetText(tmp.CStr());
	}
	if (m_info)
	{
		m_info->SetText(UNI_L(""));
	}
	if (m_progress)
	{
		m_progress->SetVisibility(FALSE);
	}	
}


/***********************************************************************************
**
**	AutoUpdateDialog::SetReadyContext
**  @param	context
**
***********************************************************************************/
void AutoUpdateDialog::SetReadyContext(UpdateProgressContext* context)
{
	OpString tmp;
	m_ready_for_install = TRUE;
	if (m_summary)
	{
		OpString tmp2, version;
		g_languageManager->GetString(Str::D_UPDATE_READY_TEXT, tmp);
		g_autoupdate_manager->GetVersionString(version);
		tmp2.AppendFormat(tmp.CStr(), version.CStr());
		m_summary->SetText(tmp2.CStr());
	}
	if (m_info)
	{
		g_languageManager->GetString(Str::D_UPDATE_READY_INFO, tmp);
		m_info->SetText(tmp.CStr());
	}
	if (m_progress)
	{
		m_progress->SetVisibility(FALSE);
	}
	RefreshButtons();
}

#endif // AUTO_UPDATE_SUPPORT
