// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Espen Sand
//

#include "core/pch.h"

#include "KioskResetDialog.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/managers/KioskManager.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"

//#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/widgets/OpMultiEdit.h"


KioskResetDialog::KioskResetDialog()
	:m_secs_to_shutdown(10),
	 m_timer(0)
{
}


KioskResetDialog::~KioskResetDialog()
{
	OP_DELETE(m_timer);
}


void KioskResetDialog::OnInit()
{
	if( !m_timer )
	{
		m_timer = OP_NEW(OpTimer, ());
	}
	m_timer->SetTimerListener( this );
	m_timer->Start(1000);

	UpdateMessage();
}


void KioskResetDialog::OnTimeOut(OpTimer* timer)
{
	if (timer != m_timer)
	{
		Dialog::OnTimeOut(timer);
		return;
	}

	if( m_secs_to_shutdown <= 1 )
	{
		CloseDialog(FALSE,TRUE,FALSE);

		if( g_application )
		{
			KioskManager::GetInstance()->OnAutoReset();
		}
	}
	else
	{
		m_secs_to_shutdown --;
		UpdateMessage();
		timer->Start(1000);
	}
}


void KioskResetDialog::OnClose(BOOL user_initiated)
{
	Dialog::OnClose(user_initiated);
	if( g_application )
		KioskManager::GetInstance()->OnDialogClosed();
}


void KioskResetDialog::UpdateMessage()
{
	OpMultilineEdit* edit = (OpMultilineEdit*) GetWidgetByName("Edit");
	if (edit)
	{
		edit->SetLabelMode();
		OpString msg;
		if( m_secs_to_shutdown > 1 )
		{
			OpString format;
			g_languageManager->GetString(Str::S_KIOSK_RESET_SECONDS,format);
			if( format.CStr() )
			{
				msg.AppendFormat(format.CStr(),m_secs_to_shutdown);
			}
		}
		else
		{
			g_languageManager->GetString(Str::S_KIOSK_RESET_SECOND,msg);
		}
		edit->SetText(msg.CStr());
	}
}
