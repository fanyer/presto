/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef M2_SUPPORT

#ifdef M2_MERLIN_COMPATIBILITY

#include "MailStoreUpdateDialog.h"

#include "adjunct/quick_toolkit/widgets/OpProgressbar.h"
#include "adjunct/quick/dialogs/SimpleDialog.h"
#include "modules/widgets/OpMultiEdit.h"
#include "adjunct/m2/src/engine/engine.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"


MailStoreUpdateDialog::MailStoreUpdateDialog()
  : m_paused(FALSE)
  , m_start_tick(0)
  , m_start_progress(0)
  , m_store_updater(*MessageEngine::GetInstance()->GetStore())
{
}

MailStoreUpdateDialog::~MailStoreUpdateDialog()
{
}


const uni_char*	MailStoreUpdateDialog::GetOkText()
{
	g_languageManager->GetString(m_paused ? Str::LocaleString(Str::D_CONTINUE) : Str::LocaleString(Str::D_PAUSE), m_ok_text);
	return m_ok_text.CStr();
}


void MailStoreUpdateDialog::OnInit()
{
	OpMultilineEdit* edit = (OpMultilineEdit*)GetWidgetByName("Update_info");
	if (edit)
	{
		edit->SetLabelMode();
	}

	m_start_tick = g_op_time_info->GetWallClockMS();

	if (OpStatus::IsError(m_store_updater.Init()) ||
		OpStatus::IsError(m_store_updater.AddListener(this)) ||
		OpStatus::IsError(m_store_updater.StartUpdate()))
	{
		// Tell user that we failed
		SimpleDialog::ShowDialog(WINDOW_NAME_MAIL_STORE_UPDATE_FAIL, GetParentDesktopWindow(), Str::D_MAIL_STORE_UPDATE_FAIL, Str::D_MAIL_STORE_UPDATE_FAIL_TITLE, Dialog::TYPE_OK, Dialog::IMAGE_ERROR);

		// Close this dialog
		CloseDialog(TRUE);
	}

	Dialog::OnInit();
}


void MailStoreUpdateDialog::OnCancel()
{
	m_store_updater.StopUpdate();
}


BOOL MailStoreUpdateDialog::OnInputAction(OpInputAction* action)
{
	if (action->GetAction() == OpInputAction::ACTION_OK)
	{
		OpString tmp;
		if (m_paused)
		{
			m_start_tick = g_op_time_info->GetWallClockMS();
			m_store_updater.StartUpdate();
			g_languageManager->GetString(Str::D_PAUSE, tmp);
		}
		else
		{
			m_store_updater.StopUpdate();
			m_start_tick     = 0;
			m_start_progress = 0;
			g_languageManager->GetString(Str::D_CONTINUE, tmp);
		}
		SetButtonText(0, tmp.CStr());
		m_paused = !m_paused;

		return TRUE;
	}
	return Dialog::OnInputAction(action);
}


void MailStoreUpdateDialog::OnStoreUpdateProgressChanged(INT32 progress, INT32 total)
{
	if (!m_start_progress)
		m_start_progress = progress;

	double elapsed = g_op_time_info->GetWallClockMS() - m_start_tick;

	INT32 remaining = 0;
	if (progress!=m_start_progress && total!=m_start_progress)
	{
		double percentage = (double)(progress-m_start_progress)/(double)(total-m_start_progress);
		if (percentage != 0.0)
		{
			remaining = (INT32)((elapsed/percentage - elapsed) / 1000.0);
		}
	}

	OpProgressBar* progressbar = (OpProgressBar*) GetWidgetByName("Update_progress_bar");

	if (progressbar)
	{
		OpString progressformat;
		OpString progresssecformat;
		OpString progressminformat;

		g_languageManager->GetString(Str::S_PROGRESS_FORMAT, progressformat);
		g_languageManager->GetString(Str::S_PROGRESS_SEC_FORMAT, progresssecformat);
		g_languageManager->GetString(Str::S_PROGRESS_MIN_FORMAT, progressminformat);

		if (progressformat.HasContent() && progresssecformat.HasContent() && progressminformat.HasContent())
		{
			OpString text;
			text.AppendFormat(progressformat.CStr(), progress, total);
			OpString text_with_time_s;
			text_with_time_s.AppendFormat(progresssecformat.CStr(), progress, total, remaining);
			OpString text_with_time_m;
			text_with_time_m.AppendFormat(progressminformat.CStr(), progress, total, remaining / 60);

			progressbar->SetLabel(elapsed > 2000 ? remaining < 60 ? text_with_time_s.CStr() : text_with_time_m.CStr() : text.CStr(), TRUE);
		}

		progressbar->SetProgress(progress, total);
	}

	if (progress == total)
	{
#if defined(_DEBUG)
		fprintf(stderr, "Store update time: %ld\n", (long)(elapsed));
#endif // _DEBUG

		// Tell user that we're complete
		SimpleDialog::ShowDialog(WINDOW_NAME_MAIL_STORE_UPDATE_COMPLETE, GetParentDesktopWindow(), Str::D_MAIL_STORE_UPDATE_COMPLETE_TEXT, Str::D_MAIL_STORE_UPDATE_TITLE, Dialog::TYPE_OK, Dialog::IMAGE_INFO);

		// Close this dialog
		CloseDialog(TRUE);
	}
}

#endif // M2_MERLIN_COMPATIBILITY
#endif // M2_SUPPORT
