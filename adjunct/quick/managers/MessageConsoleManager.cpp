/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef OPERA_CONSOLE

#include "adjunct/quick/dialogs/MessageConsoleDialog.h"
#include "adjunct/quick/managers/MessageConsoleManager.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/util/opstrlst.h"


MessageConsoleManager::MessageConsoleManager()
	: m_message_console_dialog(NULL)
	, m_mail_message_dialog(NULL)
	, m_using_view(NULL)
	, m_last_source(OpConsoleEngine::SourceTypeLast)
	, m_last_severity(OpConsoleEngine::Verbose)
{
}

OP_STATUS MessageConsoleManager::Init()
{
	// Set up the filter from preferences. Default to retrieve all critical
	// errors, no matter the source, and then whatever the user has selected.
	OpConsoleFilter filter(OpConsoleEngine::Critical);
	ReadFilterFromPrefs(filter);

	m_using_view = OP_NEW(OpConsoleView, ());
	if (!m_using_view)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	OP_STATUS construct_result = m_using_view->Construct(this, &filter);
	if (OpStatus::IsError(construct_result))
	{
		// If Construct failed due to OOM (or something else)
		OP_DELETE(m_using_view);
		m_using_view = NULL;
	}
	return construct_result;
}

MessageConsoleManager::~MessageConsoleManager()
{
	// Must close dialog to prevent bug #184872 (Crash when exiting with ctrl+q on the javascript console)
	// Close(TRUE) will call destructor
	if( m_message_console_dialog )
		m_message_console_dialog->Close(TRUE);

	if (m_using_view) // Could be 0 if there was an OOM situation during Construct.
		OP_DELETE(m_using_view);

}


OP_STATUS MessageConsoleManager::PostNewMessage(const OpConsoleEngine::Message* message)
{
	// If the console is not open and the message is from the mailer, open a mail message dialog.
	// If the console is already open, handle the message normally.

	if ( (message->source == OpConsoleEngine::M2) &&                        // IF  the message is from the mailer
		 (message->severity != OpConsoleEngine::Critical) &&                // AND the message is not critical
		 (!m_message_console_dialog) &&                                     // AND the console is not already open
		 (g_pcui->GetIntegerPref(PrefsCollectionUI::ShowMailErrorDialog)) ) // AND the user hasn't decided to disable the mail message dialog
	{
		// This is a Mailer error, and it should get special attention, as this is the only case that "normal" users will see the console.
		if (!m_mail_message_dialog)
		{
			m_mail_message_dialog = OP_NEW(MailMessageDialog, (*this, message->message));
			if (!m_mail_message_dialog)
				return OpStatus::ERR_NO_MEMORY;
			RETURN_IF_ERROR(m_mail_message_dialog->Init(NULL));	// MailMessageDialogClosed() will set m_mail_message_dialog = NULL
		}
		else
		{
			m_mail_message_dialog->SetMessage(message->message);
		}
		return OpStatus::OK;
	}

	if (m_message_console_dialog)
	{
		// If the console is already open, just post the message to it.
		return m_message_console_dialog->PostConsoleMessage(message);
	}
	else
	{
		// Create the dialog if it doesen't exist, bring to front based on message severity. Return if oom
		// This will not put any messages on the console. That needs to be done below, after determining if the
		// filter needs to go into a special state.
		RETURN_IF_ERROR(MakeNewMessageConsole(message->severity == OpConsoleEngine::Critical ? TRUE : FALSE));
	}

	if (message->source == OpConsoleEngine::EcmaScript)
	{
		// Initiate the console with only JavaScript errors displayed.
		return m_message_console_dialog->SelectSpecificSourceAndSeverity(OpConsoleEngine::EcmaScript, OpConsoleEngine::Error);
	}
	// This is the standard case, but will
	// rarely happen, the user would have to manually edit the filter to open
	// the console on a special error type.
	SetSelectedAllComponentsAndSeverity(OpConsoleEngine::Verbose);
	return OpStatus::OK;
}

void MessageConsoleManager::BeginUpdate()
{
	m_message_console_dialog->BeginUpdate();
}

void MessageConsoleManager::EndUpdate()
{
	m_message_console_dialog->EndUpdate();
}

OP_STATUS MessageConsoleManager::OpenDialog()
{
	// Open the console.
	RETURN_IF_ERROR(MakeNewMessageConsole(TRUE));
	// Set the filter to match the default settings in the console.
	if (m_last_source == OpConsoleEngine::SourceTypeLast)
		SetSelectedAllComponentsAndSeverity(m_last_severity);
	else
		SetSelectedComponentAndSeverity(m_last_source, m_last_severity);
	return OpStatus::OK;
}

OP_STATUS MessageConsoleManager::OpenDialog(OpConsoleEngine::Source component, OpConsoleEngine::Severity severity)
{
	// Open the console.
	RETURN_IF_ERROR(MakeNewMessageConsole(TRUE));
	// Set the filter to match the default settings in the console.
	m_message_console_dialog->SelectSpecificSourceAndSeverity(component, severity);
	return OpStatus::OK;
}

void MessageConsoleManager::CloseDialog()
{
	// Close the console dialog, if open.
	if (m_message_console_dialog)
		m_message_console_dialog->Close(FALSE, FALSE);
}

void MessageConsoleManager::ConsoleDialogClosed()
{
	// Reread console settings from prefs, any changes the user has made to the console settings should be dropped.
	OpConsoleFilter prefs_filter;
	ReadFilterFromPrefs(prefs_filter);
	OpConsoleFilter* running_filter = m_using_view->GetCurrentFilter();
	unsigned int lowest_id = running_filter->GetLowestId(); // Need to capture the lowest id before duplicating, because the lowest id will also be duplicated, removing the user's clear point.
	running_filter->Duplicate(&prefs_filter);
	running_filter->SetLowestId(lowest_id);
	m_message_console_dialog = NULL;
}

void MessageConsoleManager::MailMessageDialogClosed()
{
	m_mail_message_dialog = NULL;
}

void MessageConsoleManager::GetSelectedComponentAndSeverity(OpConsoleEngine::Source& source, OpConsoleEngine::Severity& severity)
{
	source = m_last_source;
	severity = m_last_severity;
}

void MessageConsoleManager::SetSelectedComponentAndSeverity(OpConsoleEngine::Source source, OpConsoleEngine::Severity severity)
{
	OpConsoleFilter* filter = m_using_view->GetCurrentFilter();
	filter->Clear();
	filter->SetDefaultSeverity(OpConsoleEngine::Critical);
	filter->SetOrReplace(source, severity);
	m_using_view->SendAllComponentMessages(filter);

	m_last_source = source;
	m_last_severity = severity;
}

void MessageConsoleManager::SetSelectedAllComponentsAndSeverity(OpConsoleEngine::Severity severity)
{
	OpConsoleFilter* filter = m_using_view->GetCurrentFilter();
	filter->SetAllExistingSeveritiesTo(severity);
	filter->SetDefaultSeverity(severity);
	m_using_view->SendAllComponentMessages(filter);

	m_last_source = OpConsoleEngine::SourceTypeLast; // Magic value that means "show from all sources"
	m_last_severity = severity;
}

void MessageConsoleManager::UserClearedConsole()
{
	OpConsoleFilter* filter = m_using_view->GetCurrentFilter();
	filter->SetLowestId(g_console->GetHighestId());
	m_using_view->SendAllComponentMessages(filter);
}

void MessageConsoleManager::SetM2ConsoleLogging(BOOL enable)
{
	// If enabled, set to information, if not set to critical.
	OpConsoleEngine::Severity severity = OpConsoleEngine::Critical;
	if (enable)
	{
		severity = OpConsoleEngine::Information;
	}
	if (m_message_console_dialog)
	{
		// If the console is open, the filter may be modified. We have to modify the real filter from prefs.
		OpConsoleFilter filter;
		ReadFilterFromPrefs(filter);
		filter.SetOrReplace(OpConsoleEngine::M2, severity);
		WriteFilterToPrefs(filter);
	}
	else
	{
		// The console is closed and the filter in use is identical to the prefs filter.
		OpConsoleFilter* filter = m_using_view->GetCurrentFilter();
		filter->SetOrReplace(OpConsoleEngine::M2, severity);
		WriteFilterToPrefs(*filter);
	}
}

void MessageConsoleManager::SetJavaScriptConsoleLogging(BOOL enable)
{
	// If enabled, set to error, if not set to critical.
	OpConsoleEngine::Severity severity = OpConsoleEngine::Critical;
	if (enable)
	{
		severity = OpConsoleEngine::Error;
	}
	if (m_message_console_dialog)
	{
		// If the console is open, the filter may be modified. We have to modify the real filter from prefs.
		OpConsoleFilter filter;
		ReadFilterFromPrefs(filter);
		filter.SetOrReplace(OpConsoleEngine::EcmaScript, severity);
		WriteFilterToPrefs(filter);
	}
	else
	{
		// The console is closed and the filter in use is identical to the prefs filter.
		OpConsoleFilter* filter = m_using_view->GetCurrentFilter();
		filter->SetOrReplace(OpConsoleEngine::EcmaScript, severity);
		WriteFilterToPrefs(*filter);
	}

}

BOOL MessageConsoleManager::GetJavaScriptConsoleSetting()
{
	OpConsoleFilter filter;
	ReadFilterFromPrefs(filter);
	return (filter.Get(OpConsoleEngine::EcmaScript) <= OpConsoleEngine::Error);
}

void MessageConsoleManager::SetJavaScriptConsoleLoggingSite(const OpString &site, BOOL enable)
{
	// Only write the setting if it is being changed from what it was set to.
	if (GetJavaScriptConsoleSettingSite(site) != enable)
	{
		// If the site setting is changed into what the general setting is,
		// we should remove the site setting and let the general setting apply.
		// If the setting is changed away from what the general setting is, a
		// site setting must be added.
		if (enable == GetJavaScriptConsoleSetting())
		{
			if (m_message_console_dialog)
			{
				// If the console is open, the filter may be modified. We have to modify the real filter from prefs.
				OpConsoleFilter filter;
				ReadFilterFromPrefs(filter);
				filter.RemoveDomain(site.CStr());
				WriteFilterToPrefs(filter);
			}
			else
			{
				// The console is closed, and the filter in use is identical to the prefs filter.
				OpConsoleFilter* filter = m_using_view->GetCurrentFilter();
				filter->RemoveDomain(site.CStr());
				WriteFilterToPrefs(*filter);
			}
		}
		else
		{
			// If enabled, set to Error, if not set to critical.
			OpConsoleEngine::Severity severity = OpConsoleEngine::Critical;
			if (enable)
			{
				severity = OpConsoleEngine::Error;
			}
			if (m_message_console_dialog)
			{
				// If the console is open, the filter may be modified. We have to modify the real filter from prefs.
				OpConsoleFilter filter;
				ReadFilterFromPrefs(filter);
				filter.SetDomain(site.CStr(), severity);
				WriteFilterToPrefs(filter);
			}
			else
			{
				// The console is closed, and the filter in use is identical to the prefs filter.
				OpConsoleFilter* filter = m_using_view->GetCurrentFilter();
				filter->SetDomain(site.CStr(), severity);
				WriteFilterToPrefs(*filter);
			}
		}
	}
}

BOOL MessageConsoleManager::GetJavaScriptConsoleSettingSite(const OpString &site)
{
	OpConsoleFilter filter;
	ReadFilterFromPrefs(filter);
	// If the site is overridden, return the setting for the site
	OpString_list* host_list = filter.GetOverriddenHosts();
	if( host_list )
	{
		unsigned long count = host_list->Count();
		for (unsigned long i = 0; i < count; i++)
		{
			if (site.Compare(host_list->Item(i)) == 0)
			{
				return (filter.Get(site.CStr(), FALSE) <= OpConsoleEngine::Error);
			}
		}
	}

	// Otherwise return the general setting
	return GetJavaScriptConsoleSetting();
}

OP_STATUS MessageConsoleManager::MakeNewMessageConsole(BOOL front_and_focus)
{
	if (!m_message_console_dialog)
	{
		m_message_console_dialog = OP_NEW(MessageConsoleDialog, (*this));
		if (!m_message_console_dialog)
			return OpStatus::ERR_NO_MEMORY;

		OP_STATUS status = m_message_console_dialog->init_status;
		if (OpStatus::IsError(status))
		{
			OP_DELETE(m_message_console_dialog);					// ConsoleDialogClosed() will set m_message_console_dialog = NULL
			return status;
		}
		RETURN_IF_ERROR(m_message_console_dialog->Init(NULL));	// ConsoleDialogClosed() will set m_message_console_dialog = NULL

		m_message_console_dialog->Show(TRUE);
	}
	if (front_and_focus)
	{
		m_message_console_dialog->GetOpWindow()->Raise();

		// Clear the console, yes it's super hacky, but this band-aid will keep it working
		m_message_console_dialog->Clear();
	}
	return OpStatus::OK;
}

OP_STATUS MessageConsoleManager::ReadFilterFromPrefs(OpConsoleFilter &filter)
{
	return filter.SetFilter(g_pcui->GetStringPref(PrefsCollectionUI::ErrorConsoleFilter));
}

OP_STATUS MessageConsoleManager::WriteFilterToPrefs(const OpConsoleFilter &filter)
{
	OpString serialized_filter;
	OP_STATUS status = filter.GetFilter(serialized_filter);
	if (OpStatus::IsSuccess(status))
	{
		TRAP(status, g_pcui->WriteStringL(PrefsCollectionUI::ErrorConsoleFilter, serialized_filter));
	}
	return status;
}

#endif // OPERA_CONSOLE
