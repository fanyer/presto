/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  1995-2011
 *
 * Porting interface abstractions, to be used by the engine
 * Lars T Hansen
 */

#include "core/pch.h"

#include "modules/ecmascript/carakan/ecmascript_parameters.h"
#include "modules/ecmascript/carakan/src/ecma_pi.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/prefs/prefsmanager/collections/pc_app.h"
#include "modules/prefs/prefsmanager/collections/pc_js.h"

/* Ad-hoc API exported by Opera: post a message to trigger an ECMAScript GC */
#include "modules/hardcore/mh/mh.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/hardcore/mem/mem_man.h"

/* static */ void
ES_ImportedAPI::PostGCMessage(unsigned int timeout)
{
	g_main_message_handler->PostDelayedMessage(MSG_ES_COLLECT, timeout > 0, 0, timeout);
}

/* static */ void
ES_ImportedAPI::PostMaintenanceGCMessage(unsigned int timeout)
{
	g_main_message_handler->PostDelayedMessage(MSG_ES_COLLECT, timeout > 0, TRUE, timeout);
}

#if defined OPERA_CONSOLE

#include "modules/doc/frm_doc.h"
#include "modules/dochand/win.h"
#include "modules/ecmascript_utils/essched.h"		// for info about failing thread
#include "modules/console/opconsoleengine.h"

/* static */ void
ES_ImportedAPI::PostToConsole(const uni_char* message, FramesDocument* fd)
{
	if (!g_console->IsLogging())
		return;

	URL* url = NULL;
	const uni_char *url_name = UNI_L("Script of unknown origin");

	if (fd != NULL)
	{
		url = &fd->GetURL();
		url_name = url->GetAttribute(URL::KUniName_Username_Password_Hidden).CStr();
	}

	OpConsoleEngine::Message cmessage(OpConsoleEngine::EcmaScript, OpConsoleEngine::Error);

	OP_STATUS rc1, rc2, rc3;
	rc1 = cmessage.message.Set(message);

	if (url)
	{
		if (0 == uni_strcmp(url_name, UNI_L("POSTED")))
		{
			// Message from opera.postError()
			cmessage.url.Empty();
			cmessage.severity = OpConsoleEngine::Information;
			rc2 = OpStatus::OK;
		}
		else
			rc2 = cmessage.url.Set(url_name);
	}
	else
		rc2 = cmessage.url.Set(url_name);

	if (fd && fd->GetWindow())
		cmessage.window = fd->GetWindow()->Id();

	if (fd && fd->GetESScheduler())
		rc3 = cmessage.context.Set(fd->GetESScheduler()->GetThreadInfoString());
	else
		rc3 = cmessage.context.Set("Unknown thread");

	if (OpStatus::IsSuccess(rc1) && OpStatus::IsSuccess(rc2) &&
	    OpStatus::IsSuccess(rc3))
	{
		TRAPD(rc, g_console->PostMessageL(&cmessage));
		OpStatus::Ignore(rc); // FIXME:OOM
	}
}

#endif // defined OPERA_CONSOLE

/* Ad-hoc API exported by Opera: date and time */
/* static */ OP_STATUS
ES_ImportedAPI::FormatLocalTime(ES_ImportedAPI::DateFormatSpec how, uni_char *buf, unsigned length, ES_ImportedAPI::TimeElements* time)
{
#if defined(MSWIN) || defined(WINGOGI)
    SYSTEMTIME dtime;
    unsigned len = 0;

    dtime.wYear = time->year;
    dtime.wMonth = time->month + 1;
    dtime.wDayOfWeek = time->day_of_week;
    dtime.wDay = time->day_of_month;
    dtime.wHour = time->hour;
    dtime.wMinute = time->minute;
    dtime.wSecond = time->second;
    dtime.wMilliseconds = time->millisecond;

	if (how == GET_DATE_AND_TIME || how == GET_DATE)
		if ((len = GetDateFormat(LOCALE_USER_DEFAULT, DATE_LONGDATE, &dtime, NULL, buf, length)) == 0)
			goto failure;

	if (how == GET_DATE_AND_TIME)
		buf[len - 1] = ' ';

	if (how == GET_DATE_AND_TIME || how == GET_TIME)
        if ((len = GetTimeFormat(LOCALE_USER_DEFAULT, 0, &dtime, NULL, buf + len, length - len)) == 0)
			goto failure;

	return OpStatus::OK;

failure:
	// It seems that GetDateFormat or GetTimeFormat fails on Windows 95 (and perhaps some
	// installations of Windows 98).  It would be bad to return an empty string, so just
	// handle it by the default action.
#endif // MSWIN || WINGOGI
	return OpStatus::ERR;
}
