/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/widgets/OpPluginCrashedBar.h"

#include "adjunct/desktop_util/string/i18n.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/controller/PluginCrashController.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "adjunct/quick_toolkit/widgets/OpProgressbar.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/pi/system/OpFolderLister.h"
#include "modules/viewers/plugins.h"

OtlList<OpPluginCrashedBar*> OpPluginCrashedBar::s_visible_bars;

OpPluginCrashedBar::OpPluginCrashedBar()
  : m_found_log_file(false)
  , m_sending_report(false)
  , m_sending_failed(false)
  , m_spinner(NULL)
  , m_progress_label(NULL)
  , m_send_button(NULL)
{
}

OP_STATUS OpPluginCrashedBar::ShowCrash(const OpStringC& path, const OpMessageAddress& address)
{
	time_t crash_time = g_timecache->CurrentTime();
	m_logsender.reset(OP_NEW(LogSender, (false)));
	RETURN_OOM_IF_NULL(m_logsender.get());

	m_logsender->SetCrashTime(g_timecache->CurrentTime());
	m_logsender->SetListener(this);

	RETURN_IF_ERROR(FindFile(crash_time));

	PluginViewer* plugin = g_plugin_viewers->FindPluginViewerByPath(path);
	OpLabel* label = static_cast<OpLabel*>(GetWidgetByName("tbb_plugin_crashed_desc"));
	if (plugin && label)
	{
		OpString description;
		RETURN_IF_ERROR(I18n::Format(description, Str::D_PLUGIN_CRASH_DESCRIPTION, plugin->GetProductName()));
		RETURN_IF_ERROR(label->SetText(description));
	}

	m_spinner = static_cast<OpProgressBar*>(GetWidgetByType(WIDGET_TYPE_PROGRESSBAR, FALSE));
	if (!m_spinner)
		return OpStatus::ERR;

	m_spinner->SetType(OpProgressBar::Only_Label);
	m_spinner->SetText(NULL);

	m_send_button = GetWidgetByName("tbb_crash_report");
	if (!m_send_button)
		return OpStatus::ERR;

	m_address = address;
	m_sending_report = false;
	m_sending_failed = false;
	RETURN_IF_ERROR(s_visible_bars.Append(this));

	Show();

	return OpStatus::OK;
}

BOOL OpPluginCrashedBar::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();
			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_CANCEL:
					child_action->SetEnabled(TRUE);
					return TRUE;
				case OpInputAction::ACTION_CRASH_REPORT_ISSUE:
					child_action->SetEnabled(m_found_log_file && !m_sending_report);
					return TRUE;
			}
			break;
		}

		case OpInputAction::ACTION_CANCEL:
		{
			Hide();
			return TRUE;
		}

		case OpInputAction::ACTION_CRASH_REPORT_ISSUE:
		{
			if (m_sending_failed)
			{
				OpStatus::Ignore(m_logsender->Send());
			}
			else
			{
				const uni_char* url = GetParentDesktopWindow()->GetWindowCommander()->GetCurrentURL(FALSE);
				PluginCrashController* controller = OP_NEW(PluginCrashController, (m_send_button, url, *m_logsender));
				OpStatus::Ignore(ShowDialog(controller, g_global_ui_context, GetParentDesktopWindow()));
			}
			return TRUE;
		}
	}

	return OpInfobar::OnInputAction(action);
}

OP_STATUS OpPluginCrashedBar::FindFile(time_t crashtime)
{
	m_found_log_file = false;

	OpString crashlog_path;
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_CRASHLOG_FOLDER, crashlog_path));

	OpFolderLister* lister;
	RETURN_IF_ERROR(OpFolderLister::Create(&lister));
	OpAutoPtr<OpFolderLister> holder (lister);

	// File name format: crashyyyymmddhhmmss.txt, find newest
	RETURN_IF_ERROR(lister->Construct(crashlog_path, UNI_L("crash*.txt")));

	OpString file;
	while (lister->Next())
	{
		if (file.Compare(lister->GetFullPath()) < 0)
			RETURN_IF_ERROR(file.Set(lister->GetFullPath()));
	}

	if (file.IsEmpty())
		return OpStatus::OK;

	// Check if the timestamp looks reasonable
	const uni_char* timestamp_start = file.CStr() + file.Length() + 1 - sizeof("yyyymmddhhmmss.txt");
	uni_char* endptr;
	UINT64 timespec = uni_strtoui64(timestamp_start, &endptr, 10);

	if (endptr - timestamp_start != sizeof("yyyymmddhhmmss") - 1)
		return OpStatus::OK;

	// Check for some time in advance, in case it took some time for the process to warn us
	crashtime -= MaxCrashTimePassed;

	struct tm* broken_crashtime = op_localtime(&crashtime);
	if (!broken_crashtime)
		return OpStatus::ERR;

	UINT64 min_timespec = (broken_crashtime->tm_year + 1900) * OP_UINT64(10000000000)
		+ (broken_crashtime->tm_mon + 1) * OP_UINT64(100000000)
		+ broken_crashtime->tm_mday * OP_UINT64(1000000)
		+ broken_crashtime->tm_hour * OP_UINT64(10000)
		+ broken_crashtime->tm_min * OP_UINT64(100)
		+ broken_crashtime->tm_sec;

	if (timespec < min_timespec)
		return OpStatus::OK;

	m_found_log_file = true;
	return m_logsender->SetLogFile(file);
}

BOOL OpPluginCrashedBar::Hide()
{
	m_sending_report = false;
	m_sending_failed = false;
	m_logsender.reset();
	s_visible_bars.RemoveItem(this);
	return OpInfobar::Hide();
}

void OpPluginCrashedBar::OnSendingStarted(LogSender* sender)
{
	m_sending_failed = false;

	for (OtlList<OpPluginCrashedBar*>::Iterator it = s_visible_bars.Begin(); it != s_visible_bars.End(); ++it)
		(*it)->OnSendingStarted(m_address);

	g_input_manager->UpdateAllInputStates();
}

void OpPluginCrashedBar::OnSendingStarted(const OpMessageAddress& address)
{
	if (m_address != address)
		return;

	m_spinner->SetType(OpProgressBar::Spinner);

	m_sending_report = true;

	Relayout();
}

void OpPluginCrashedBar::OnSendSucceeded(LogSender* sender)
{
	for (OtlList<OpPluginCrashedBar*>::Iterator it = s_visible_bars.Begin(); it != s_visible_bars.End(); ++it)
		(*it)->OnSendSucceeded(m_address);

	g_input_manager->UpdateAllInputStates();
}

void OpPluginCrashedBar::OnSendSucceeded(const OpMessageAddress& address)
{
	if (m_address != address)
		return;

	m_spinner->SetType(OpProgressBar::Only_Label);
	OpString status;
	OpStatus::Ignore(g_languageManager->GetString(Str::D_PLUGIN_CRASH_REPORT_SENT, status));
	m_spinner->SetText(status.CStr());

	m_sending_report = false;
	m_found_log_file = false;

	Relayout();
}

void OpPluginCrashedBar::OnSendFailed(LogSender* sender)
{
	m_sending_failed = true;

	OpString send_again;
	OpStatus::Ignore(g_languageManager->GetString(Str::D_CRASH_LOGGING_DIALOG_SEND_AGAIN, send_again));
	m_send_button->SetText(send_again.CStr());

	for (OtlList<OpPluginCrashedBar*>::Iterator it = s_visible_bars.Begin(); it != s_visible_bars.End(); ++it)
		(*it)->OnSendFailed(m_address);

	g_input_manager->UpdateAllInputStates();
}

void OpPluginCrashedBar::OnSendFailed(const OpMessageAddress& address)
{
	if (m_address != address)
		return;

	m_spinner->SetType(OpProgressBar::Only_Label);
	OpString status;
	OpStatus::Ignore(g_languageManager->GetString(Str::D_CRASH_LOGGING_DIALOG_PROGRESS_FAILURE, status));
	m_spinner->SetText(status.CStr());

	m_sending_report = false;

	Relayout();
}
