/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#if defined SCOPE_CONSOLE_LOGGER

/**
 * The function GetSourceName used to return its own strings
 * without involving g_console. Tragically, not all the strings
 * matched those in the console module, which means that certain
 * message sources must still be special-cased to maintain compat.
 */
#define SCOPE_GET_SOURCE_NAME_COMPAT

#include "modules/scope/src/scope_manager.h"
#include "modules/scope/src/scope_console_logger.h"
#include "modules/scope/src/scope_window_manager.h"
#include "modules/xmlutils/xmlfragment.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/dochand/winman.h"

OpScopeConsoleLogger::OpScopeConsoleLogger()
	: OpScopeConsoleLogger_SI()
#ifdef SCOPE_WINDOW_MANAGER_SUPPORT
	, window_manager(NULL)
#endif // SCOPE_WINDOW_MANAGER_SUPPORT
{
}

/* virtual */
OpScopeConsoleLogger::~OpScopeConsoleLogger()
{
	g_console->UnregisterConsoleListener(this);
}

/*virtual*/
OP_STATUS
OpScopeConsoleLogger::Construct()
{
	g_console->RegisterConsoleListener(this);
	return OpStatus::OK;
}

/* virtual */
OP_STATUS
OpScopeConsoleLogger::OnPostInitialization()
{
#ifdef SCOPE_WINDOW_MANAGER_SUPPORT
	if (OpScopeServiceManager *manager = GetManager())
		window_manager = OpScopeFindService<OpScopeWindowManager>(manager, "window-manager");
	if (!window_manager)
		return OpStatus::ERR_NULL_POINTER;
#endif // SCOPE_WINDOW_MANAGER_SUPPORT

	return OpStatus::OK;
}

static const char *GetSourceName(OpConsoleEngine::Source msg_source)
{
#ifdef SCOPE_GET_SOURCE_NAME_COMPAT
	switch (msg_source)
	{
	case OpConsoleEngine::EcmaScript:
		return "ecmascript";
# ifdef M2_SUPPORT
	case OpConsoleEngine::M2:
		return "m2";
# endif // M2_SUPPORT
# ifdef GADGET_SUPPORT
	case OpConsoleEngine::Gadget:
		return "widget";
# endif // GADGET_SUPPORT

	}
#endif // SCOPE_GET_SOURCE_NAME_COMPAT

	return g_console->GetSourceKeyword(msg_source);
}

static const char *GetSeverityName(OpConsoleEngine::Severity severity)
{
	switch (severity)
	{
#ifdef _DEBUG
	case OpConsoleEngine::Debugging:   return "debug";
#endif
	case OpConsoleEngine::Verbose:     return "verbose";
	case OpConsoleEngine::Information: return "information";
	case OpConsoleEngine::Error:	   return "error";
	case OpConsoleEngine::Critical:    return "critical";
	}

	return NULL;
}

// FIXME: OOM
OP_STATUS
OpScopeConsoleLogger::NewConsoleMessage(unsigned /* id */, const OpConsoleEngine::Message *message)
{
	OP_ASSERT(message != NULL);

	if (IsEnabled() && AcceptWindowID(message->window))
	{
		ConsoleMessage msg;
		RETURN_IF_ERROR(SetConsoleMessage(message, msg));
		RETURN_IF_ERROR(SendOnConsoleMessage(msg));
	}

	return OpStatus::OK;
}

OP_STATUS
OpScopeConsoleLogger::DoClear()
{
	g_console->Clear();
	return OpStatus::OK;
}

OP_STATUS
OpScopeConsoleLogger::DoListMessages(ConsoleMessageList &out)
{
	unsigned low = g_console->GetLowestId();
	unsigned high = g_console->GetHighestId();

	if (low > high)
		return OpStatus::OK; // No messages.

	for (unsigned i = low; i <= high; ++i)
	{
		const OpConsoleEngine::Message *msg = g_console->GetMessage(i);

		if (msg && AcceptWindowID(msg->window))
		{
			ConsoleMessage *cmsg = out.GetConsoleMessageListRef().AddNew();
			RETURN_OOM_IF_NULL(cmsg);
			RETURN_IF_ERROR(SetConsoleMessage(msg, *cmsg));
		}
	}

	return OpStatus::OK;
}

BOOL
OpScopeConsoleLogger::AcceptWindowID(unsigned window_id)
{
#ifdef SCOPE_WINDOW_MANAGER_SUPPORT
	return g_scope_manager->window_manager->AcceptWindow(window_id);
#else // SCOPE_WINDOW_MANAGER_SUPPORT
	return TRUE;
#endif // SCOPE_WINDOW_MANAGER_SUPPORT
}

OP_STATUS
OpScopeConsoleLogger::SetConsoleMessage(const OpConsoleEngine::Message *msg, ConsoleMessage &out)
{
	const char *source_str = GetSourceName(msg->source);
	const char *severity_str = GetSeverityName(msg->severity);

	out.SetTime(static_cast<UINT32>(msg->time));
	RETURN_IF_ERROR(out.SetUri(msg->url));
	RETURN_IF_ERROR(out.SetContext(msg->context));
	if (severity_str)
		RETURN_IF_ERROR(out.SetSeverity(severity_str));
	if (source_str)
		RETURN_IF_ERROR(out.SetSource(source_str));
	out.SetWindowID(msg->window);
	RETURN_IF_ERROR(out.SetDescription(msg->message));

	return OpStatus::OK;
}

#endif // SCOPE_CONSOLE_LOGGER
