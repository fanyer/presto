/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#include "core/pch.h"

#ifdef OPERA_CONSOLE_LOGFILE

#include "modules/console/opconsolelogger.h"
#include "modules/console/opconsolefilter.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/datefun.h" // FMakeDate
#include "modules/util/handy.h" // ARRAY_SIZE

void OpConsoleLogger::Construct(OpFileDescriptor *logfile, OpConsoleFilter *filter)
{
	m_file = logfile;
	m_filter = filter;
	g_console->RegisterConsoleListener(this);
}

void OpConsoleLogger::ConstructL(const OpFileDescriptor *logfile, const OpConsoleFilter *filter)
{
	LEAVE_IF_NULL(m_file = logfile->CreateCopy());
	m_filter = OP_NEW_L(OpConsoleFilter, ());
	LEAVE_IF_ERROR(m_filter->Duplicate(filter));
	g_console->RegisterConsoleListener(this);
}

OpConsoleLogger::~OpConsoleLogger()
{
	g_console->UnregisterConsoleListener(this);
	OP_DELETE(m_file);
	OP_DELETE(m_filter);
}

OP_STATUS OpConsoleLogger::NewConsoleMessage(unsigned int id,
                                             const OpConsoleEngine::Message *message)
{
	// OpConsoleLogger uses the OpConsoleFilter class to determine
	// what to log. An inherited implementation of this class might
	// want to extend this, for instance by also looking at the
	// context, for example to create a log for a specific M2 account.

	if (m_filter->DisplayMessage(id, message))
	{
		// We want to log this message
		return LogMessage(id, message);
	}

	return OpStatus::OK;
}

OP_STATUS OpConsoleLogger::LogMessage(int, const OpConsoleEngine::Message *message)
{
	OP_ASSERT(message);
	OP_STATUS rc = OpStatus::OK;

	// Open the log file and write to it
	if (OpStatus::IsSuccess(m_file->Open(OPFILE_APPEND)))
	{
		uni_char timebuf[64]; /* ARRAY OK 2011-11-11 peter */
		struct tm *now_tm = op_localtime(&message->time);

		const char *timetemplate =
			message->severity >= OpConsoleEngine::Error
				? "\n--- \247D \247n \247Y \247h:\247m:\247s ---"
				: "\n+++ \247D \247n \247Y \247h:\247m:\247s +++";

		OpFileLength curpos = 0;
		if (OpStatus::IsSuccess(m_file->GetFilePos(curpos)) && 0 == curpos)
			++ timetemplate; // Avoid linebreak on the very first line

		FMakeDate(*now_tm, timetemplate, timebuf, ARRAY_SIZE(timebuf));

		OP_STATUS rc = m_file->WriteUTF8Line(timebuf);
		if (OpStatus::IsSuccess(rc) && !message->url.IsEmpty())
		{
			rc = m_file->WriteUTF8Line(message->url);
		}
		if (OpStatus::IsSuccess(rc) && !message->context.IsEmpty())
		{
			rc = m_file->WriteUTF8Line(message->context);
		}
		if (OpStatus::IsSuccess(rc) && !message->message.IsEmpty())
		{
			rc = m_file->WriteUTF8Line(message->message);
		}
		m_file->Close();
	}

	return rc;
}

#endif // OPERA_CONSOLE_LOGFILE
