/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Manuela Hutter
*/

#include "core/pch.h"

#include "adjunct/quick/dialogs/ConfirmExitDialog.h"
#include "adjunct/desktop_pi/DesktopGlobalApplication.h"
#include "modules/locale/oplanguagemanager.h"

/***********************************************************************************
**
**	ConfirmExitDialog
**
***********************************************************************************/
ConfirmExitDialog::ConfirmExitDialog(PrefsCollectionUI::integerpref pref)
: m_pref(pref)
, m_timer()
, m_secs_counter(s_timer_secs)
, m_title()
, m_message()
{
	m_timer.SetTimerListener(this);
}


/***********************************************************************************
**
**	GetDialogType
**
***********************************************************************************/
/*virtual*/ Dialog::DialogType
ConfirmExitDialog::GetDialogType()
{
// hiding Opera isn't currently supported on Mac, where it is done by the platform
#if defined(_MACINTOSH_)
	return TYPE_OK_CANCEL;
#else
	return TYPE_YES_NO_CANCEL;
#endif
}


/***********************************************************************************
**
**	GetCancelTextID
**
***********************************************************************************/
/*virtual*/ Str::LocaleString
ConfirmExitDialog::GetCancelTextID()
{
	if (GetDialogType() == TYPE_YES_NO_CANCEL)
	{
		return Str::M_HIDE_OPERA;
	}
	else // GetDialogType() == TYPE_OK_CANCEL (on Mac)
	{
		return Str::DI_ID_CANCEL;
	}
}


/***********************************************************************************
**
**	Init
**
***********************************************************************************/
/*virtual*/ OP_STATUS 
ConfirmExitDialog::Init(Str::LocaleString title, Str::LocaleString message, DesktopWindow* parent_window)
{
	g_languageManager->GetString(title, m_title);
	g_languageManager->GetString(message, m_message);

	return Dialog::Init(parent_window);
}


/***********************************************************************************
**
**	OnActivate
**
***********************************************************************************/
/*virtual*/ void
ConfirmExitDialog::OnActivate(BOOL activate, BOOL first_time)
{
	m_timer.Start(1000); // fire every second
}


/***********************************************************************************
**
**	OnInit
**
***********************************************************************************/
/*virtual*/ void
ConfirmExitDialog::OnInit()
{
	SetTitle(m_title.CStr());
	SetWidgetText("message_label", m_message.CStr());
	UpdateTime(m_secs_counter);
}


/***********************************************************************************
**
**	OnOk
**
***********************************************************************************/
/*virtual*/ UINT32
ConfirmExitDialog::OnOk()
{
	if (IsDoNotShowDialogAgainChecked())
	{
		g_pcui->WriteIntegerL(m_pref, ExitStrategyExit);
	}

	if (!g_desktop_global_application->OnConfirmExit(TRUE))
		g_input_manager->InvokeAction(OpInputAction::ACTION_EXIT, 1);
	return 1;
}


/***********************************************************************************
**
**	OnCancel
**
***********************************************************************************/
/*virtual*/ void
ConfirmExitDialog::OnYNCCancel()
{
	// handle pressing of "Hide Opera"
	// if dialog type is yes_no_cancel, the actual 'Cancel' is handled in OnYNCancel
	if (GetDialogType() == TYPE_YES_NO_CANCEL)
	{
#if !defined(_MACINTOSH_)
		if (IsDoNotShowDialogAgainChecked())
		{
			g_pcui->WriteIntegerL(m_pref, ExitStrategyHide);
		}
#endif

		if (!g_desktop_global_application->OnConfirmExit(FALSE))
			g_input_manager->InvokeAction(OpInputAction::ACTION_HIDE_OPERA, 1);
	}
}


/*virtual*/ void 
ConfirmExitDialog::OnCancel()
{
	g_desktop_global_application->OnConfirmExit(FALSE);
}


/***********************************************************************************
**
**	OnTimeOut
**
***********************************************************************************/
/*virtual*/ void
ConfirmExitDialog::OnTimeOut(OpTimer* timer)
{
	if (timer == &m_timer)
	{
		m_secs_counter--;
		if (m_secs_counter > 0)
		{
			UpdateTime(m_secs_counter);
			m_timer.Start(1000); // fire again in one second
		}
		else // time has run out
		{
			if (!g_desktop_global_application->OnConfirmExit(TRUE))
				g_input_manager->InvokeAction(OpInputAction::ACTION_EXIT, 1);
		}
	}
	else
	{
		Dialog::OnTimeOut(timer);
	}
}

/***********************************************************************************
**
**	UpdateTime
**
***********************************************************************************/
void 
ConfirmExitDialog::UpdateTime(INT32 seconds)
{
	OpString counter_str, fillin_str;

	g_languageManager->GetString(Str::D_EXIT_COUNTER, fillin_str);
	counter_str.AppendFormat(fillin_str.CStr(), seconds);

	SetWidgetText("counter_label", counter_str.CStr());
}
