/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @shuais
 */

#include "core/pch.h"

#include "adjunct/quick/dialogs/CrashLoggingDialog.h"
#include "modules/widgets/OpMultiEdit.h"
#include "modules/widgets/OpEdit.h"
#include "modules/widgets/OpRadioButton.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"
#include "adjunct/desktop_util/sessions/opsessionmanager.h"
#include "adjunct/desktop_util/sessions/opsession.h"
#include "adjunct/quick/managers/CommandLineManager.h"

#include "modules/widgets/OpDropDown.h"
#include "adjunct/quick_toolkit/widgets/OpProgressbar.h"
#include "adjunct/quick/dialogs/SimpleDialog.h"
#include "adjunct/quick_toolkit/widgets/OpExpand.h"
#include "adjunct/quick_toolkit/widgets/OpGroup.h"
#include "modules/skin/skin_module.h"
#include "modules/skin/OpSkinManager.h"

#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"

#ifdef WEBSERVER_SUPPORT
# include "modules/prefs/prefsmanager/collections/pc_webserver.h"
#endif

CrashLoggingDialog::CrashLoggingDialog(OpStringC& file, BOOL& restart, BOOL& minimal)
  : m_crash_time(g_timecache->CurrentTime())
  , p_restart(&restart)
  , p_minimal(&minimal)
  , m_spinner(NULL)
  , m_logsender(true)
  , m_actions_field(NULL)
{
	*p_restart = FALSE;
	*p_minimal = FALSE;
	op_memset(&m_original_color,0,sizeof(m_original_color));

	m_dmp_path.Set(file);
	m_logsender.SetListener(this);
}

void CrashLoggingDialog::OnInit()
{
	SetWidgetValue("Restart", TRUE);

	OpString text;
	g_languageManager->GetString(Str::D_CRASH_LOGGING_DIALOG_URL_GHOST, text);
	OpEdit* url = GetWidgetByName<OpEdit>("Url", WIDGET_TYPE_EDIT);
	if(url)
	{
		url->SetGhostText(text.CStr());
		TRAPD(err,GetLastActiveTabL(text));
		url->SetText(text.CStr());
		url->SetForceTextLTR(TRUE);
	}

	//email
	g_languageManager->GetString(Str::D_CRASH_LOGGING_DIALOG_EMAIL_GHOST, text);
	OpEdit* email = (OpEdit*)GetWidgetByName("Email");
	if(email)
	{
		email->SetGhostText(text.CStr());
		OpString user_email;
		// fill up stored email address
		user_email.Set(g_pcui->GetStringPref(PrefsCollectionUI::UserEmail));
#if defined WIDGETS_AUTOCOMPLETION_SUPPORT || defined WAND_ECOMMERCE_SUPPORT
		if(user_email.IsEmpty())
			user_email.Set(g_pccore->GetStringPref(PrefsCollectionCore::EMail));
#endif
		email->SetText(user_email.CStr());
	}

	g_languageManager->GetString(Str::D_CRASH_LOGGING_DIALOG_ACTION_GHOST, m_ghost_string);
	m_actions_field = (OpMultilineEdit*)GetWidgetByName("Actions");
	if(m_actions_field)
	{
		m_original_color = m_actions_field->GetColor();
		SetActionsGhostText();
	}

	OpMultilineEdit* details = (OpMultilineEdit*)GetWidgetByName("Details");
	if (details) details->SetFlatMode();

	OpMultilineEdit* privacy_details = (OpMultilineEdit*)GetWidgetByName("Privacy_details");
	if (privacy_details) privacy_details->SetFlatMode();

	OpExpand* span = (OpExpand*)GetWidgetByName("Details_expand");
	if (span) span->SetBold(TRUE);

	if (CommandLineManager::GetInstance()->GetArgument(CommandLineManager::OWIInstall) || CommandLineManager::GetInstance()->GetArgument(CommandLineManager::OWIUninstall))
	{
		OpWidget* default_radio = GetWidgetByName("Radio_dont_restart");
		if (default_radio)
			default_radio->SetValue(TRUE);

		OpWidget* after_closing_group = GetWidgetByName("After_closing_group");
		if (after_closing_group)
			after_closing_group->SetVisibility(FALSE);

		CompressGroups();

		if (OpStatus::IsSuccess(g_languageManager->GetString(Str::D_INSTALLER_CRASH_LOGGING_DIALOG_DETAILS, text)))
		{
			OpMultilineEdit* details = (OpMultilineEdit*)GetWidgetByName("Details");
			if (details) details->SetText(text);
		}
	}
	else
	{
		// if Opera was exiting when the crash happend, the option should default to not restart
		BOOL exiting = !g_pcui->GetIntegerPref(PrefsCollectionUI::Running);

		OpRadioButton* default_radio = (OpRadioButton*)GetWidgetByName(exiting ? "Radio_dont_restart" : "Radio_reopen_all");
		if (default_radio)
		{
			default_radio->SetValue(TRUE);
		}
	}

	//hide the left-bottom progress bar,only used to display "Failed" in case sending failed
	UpdateProgress(0,0,UNI_L(""),TRUE);

	//the spinner at the right end of the expand,spinning when sending log
	m_spinner = (OpProgressBar*)GetWidgetByName("Uploading_progress");
	if (m_spinner)
	{
		m_spinner->SetVisibility(FALSE);
		m_spinner->SetType(OpProgressBar::Spinner);
	}

	m_logsender.SetLogFile(m_dmp_path);
}

void CrashLoggingDialog::OnButtonClicked(INT32 index)
{
	if(index == 1)
	{
		OnNotSend();
	}
	else if(index == 0)
	{
		SetSendingStatus(OpStatus::IsSuccess(OnSend()));
	}
}


void CrashLoggingDialog::GetButtonInfo(INT32 index,
									  OpInputAction*& action,
									  OpString& text,
									  BOOL& is_enabled,
									  BOOL& is_default,
									  OpString8& name)
{
	switch (index)
	{
		case 1:
			g_languageManager->GetString(Str::D_CRASH_LOGGING_DIALOG_DONT_SEND, text);
			name.Set(WIDGET_NAME_BUTTON_CANCEL);
			break;
		case 0:
			g_languageManager->GetString(Str::D_CRASH_LOGGING_DIALOG_SEND, text);
			is_default = TRUE;
			name.Set(WIDGET_NAME_BUTTON_OK);
			break;
		default:;
	}
}

OP_STATUS CrashLoggingDialog::OnSend()
{
	{
		OpString url;
		GetWidgetText("Url", url);
		RETURN_IF_ERROR(m_logsender.SetURL(url));

		OpString email;
		GetWidgetText("Email", email);
		RETURN_IF_ERROR(m_logsender.SetEmail(email));

		OpString comments;
		GetWidgetText("Actions", comments);
		if(comments == m_ghost_string)
			comments.Empty();
		RETURN_IF_ERROR(m_logsender.SetComments(comments));
	}

	m_logsender.SetCrashTime(m_crash_time);

	return m_logsender.Send();
}


void CrashLoggingDialog::OnNotSend()
{
	SaveRecoveryStrategy();
	CloseDialog(TRUE);
}

void CrashLoggingDialog::OnClick(OpWidget *widget, UINT32 id)
{
	if( widget->GetName().CStr() && uni_strcmp(widget->GetName().CStr(),UNI_L("View_report")) == 0 )
	{
		g_op_system_info->ExecuteApplication(m_dmp_path.CStr(), NULL);
	}

	Dialog::OnClick(widget,id);
}

void CrashLoggingDialog::OnSent()
{
	SaveRecoveryStrategy();
	OpString email;
	GetWidgetText("Email",email);
#if defined WIDGETS_AUTOCOMPLETION_SUPPORT || defined WAND_ECOMMERCE_SUPPORT
	if(email.Compare(g_pccore->GetStringPref(PrefsCollectionCore::EMail)) != 0) // only save the email if it's different from PersonalInfo|EMail
#endif
	{
		TRAPD(err, g_pcui->WriteStringL(PrefsCollectionUI::UserEmail,email));
	}
}


void CrashLoggingDialog::OnClose(BOOL user_initiated)
{
	OpString email;
	GetWidgetText("Email",email);
	if (email.IsEmpty())
	{
		TRAPD(err, g_pcui->WriteStringL(PrefsCollectionUI::UserEmail,UNI_L("")));
	}

	Dialog::OnClose(user_initiated);
}

// TRUE: Sending  FALSE: Failed
void CrashLoggingDialog::SetSendingStatus(BOOL status)
{
	if(status)
	{
		UpdateProgress(0,0,UNI_L(""),TRUE);
		if(m_spinner)
			m_spinner->SetVisibility(TRUE);
		// disable Send button
		EnableButton(0,FALSE);
	}
	else
	{
		OpString text;
		g_languageManager->GetString(Str::D_CRASH_LOGGING_DIALOG_PROGRESS_FAILURE, text);
		UpdateProgress(0,0,text.CStr(),TRUE);
		if(m_spinner)
			m_spinner->SetVisibility(FALSE);

		// enable send button and set text: Send Again
		g_languageManager->GetString(Str::D_CRASH_LOGGING_DIALOG_SEND_AGAIN, text);
		SetButtonText(0,text.CStr());
		EnableButton(0,TRUE);
	}
}

void CrashLoggingDialog::GetLastActiveTabL(OpString& text)
{
	text.Empty();
	OpSession* session= g_session_manager->ReadSessionL(UNI_L("autosave"));
	if(session)
	{
		INT32 count = session->GetWindowCountL();
		for(int i=0; i<count; i++)
		{
			OpSessionWindow* session_win = session->GetWindowL(i);
			if(session_win)
			{
				BOOL active = session_win->GetValueL("active");
				if(active)
				{
					OpSessionWindow::Type type = session_win->GetTypeL();
					switch(type)
					{
						case OpSessionWindow::BROWSER_WINDOW:
							text.Set("BROWSER_WINDOW");
							break;
#ifdef DEVTOOLS_INTEGRATED_WINDOW
						case OpSessionWindow::DEVTOOLS_WINDOW:
							text.Set("DEVTOOLS_WINDOW");
							break;
#endif // DEVTOOLS_INTEGRATED_WINDOW
						case OpSessionWindow::DOCUMENT_WINDOW:
							// read url
							{
							UINT32 current_history = session_win->GetValueL("current history");
							OpAutoVector<OpString> history_url_list; ANCHOR(OpAutoVector<OpString>, history_url_list);
							session_win->GetStringArrayL("history url", history_url_list);
							if(history_url_list.GetCount() != 0 && current_history >=1 && current_history <= history_url_list.GetCount())
								text.Set(*history_url_list.Get(current_history-1));
							}
							break;
#ifdef M2_SUPPORT
						case OpSessionWindow::MAIL_WINDOW:
							text.Set("MAIL_WINDOW");
							break;
						case OpSessionWindow::MAIL_COMPOSE_WINDOW:
							text.Set("MAIL_COMPOSE_WINDOW");
							break;
						case OpSessionWindow::CHAT_WINDOW:
							text.Set("CHAT_WINDOW");
							break;
#endif //M2_SUPPORT
						case OpSessionWindow::PANEL_WINDOW:
							text.Set("PANEL_WINDOW");
							break;
#ifdef GADGET_SUPPORT
						case OpSessionWindow::GADGET_WINDOW:
							text.Set("PANEL_WINDOW");
							break;
#endif //GADGET_SUPPORT
					}


				}
			}
		}
	}
	OP_DELETE(session);
}

void CrashLoggingDialog::OnKeyboardInputGained(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason)
{
	if(m_actions_field == new_input_context)
		ClearActionsGhostText();

	Dialog::OnKeyboardInputGained(new_input_context,old_input_context,reason);
}

void CrashLoggingDialog::OnKeyboardInputLost(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason)
{
	if(m_actions_field == old_input_context && m_actions_field->IsEmpty())
		SetActionsGhostText();

	Dialog::OnKeyboardInputLost(new_input_context,old_input_context,reason);
}

void CrashLoggingDialog::SetActionsGhostText()
{
	if(m_actions_field)
	{
		INT32 ghost_color =
#ifdef SKIN_SUPPORT
		g_skin_manager->GetSystemColor(OP_SYSTEM_COLOR_TEXT_DISABLED);
#else
		g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_TEXT_DISABLED);
#endif
		m_actions_field->SetText(m_ghost_string.CStr());
		m_actions_field->SetForegroundColor(ghost_color);
	}
}

void CrashLoggingDialog::ClearActionsGhostText()
{
	if(m_actions_field)
	{
		OpString text;
		m_actions_field->GetText(text);
		if(text.Compare(m_ghost_string) == 0)
		{
			m_actions_field->SetText(UNI_L(""));
			m_actions_field->SetColor(m_original_color);
		}
	}
}

void CrashLoggingDialog::SaveRecoveryStrategy()
{
	OP_STATUS err;
	if (GetWidgetValue("Radio_reopen_all"))
	{
		/* We want to perform crash recovery, so we have to make sure
		 * opera know we crashed.  Probably, the correct solution is
		 * to change the check to not rely on the Running flag, but
		 * that's how it currently works.  (See the documentation of
		 * OpBootManager::SetOperaRunning(), the implementation of
		 * STARTUP_SEQUENCE_START_SETTINGS in OpStartupSequence.cpp
		 * and DSK-319515.)
		 */
		TRAP(err, g_pcui->WriteIntegerL(PrefsCollectionUI::Running, TRUE); \
				  g_pcui->WriteIntegerL(PrefsCollectionUI::WindowRecoveryStrategy, Restore_AutoSavedWindows));
		*p_restart = TRUE;
		*p_minimal = FALSE;
	}
	else if (GetWidgetValue("Radio_restart_blank"))
	{
		/* See the comment for the equivalent line in the
		 * "Radio_reopen_all" section above.
		 */
		TRAP(err, g_pcui->WriteIntegerL(PrefsCollectionUI::Running, TRUE); \
				  g_pcui->WriteIntegerL(PrefsCollectionUI::WindowRecoveryStrategy, Restore_NoWindows));

#ifdef WEBSERVER_SUPPORT // don't start unite
		TRAP(err, g_pcui->WriteIntegerL(PrefsCollectionUI::RestartUniteServicesAfterCrash, FALSE); \
				  g_pcwebserver->WriteIntegerL(PrefsCollectionWebserver::WebserverEnable, FALSE));
#endif

		*p_restart = TRUE;
		*p_minimal = TRUE;
	}
	else
	{
		*p_restart = FALSE;
		*p_minimal = FALSE;
	}

	TRAP(err, g_prefsManager->CommitL());

}

void CrashLoggingDialog::OnSendSucceeded(LogSender* sender)
{
	OnSent();
	CloseDialog(TRUE,TRUE,FALSE);
}
