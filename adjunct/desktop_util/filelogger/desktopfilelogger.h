/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Petter Nilsen (pettern) / Alexander Remen (alexr)
*/

#ifndef DESKTOPFILELOGGER_H
#define DESKTOPFILELOGGER_H

#include "modules/pi/system/OpLowLevelFile.h"
#include "modules/pi/OpLocale.h"
#include "modules/util/adt/oplisteners.h"
#include "adjunct/quick/managers/CommandLineManager.h"
#include "modules/pi/OpTimeInfo.h"
#include "modules/util/OpHashTable.h"
#include "modules/windowcommander/OpWindowCommander.h"

#include <time.h>

#define OP_PROFILE_METHOD(x) OpAutoPtr<MethodProfiler> profiler (g_profile_logger ? OP_NEW(MethodProfiler, (0, x)) : 0)
#define OP_PROFILE_MSG(x)	DesktopFileLogger::Msg(x)
#define OP_PROFILE_INIT()	DesktopFileLogger::Init()
#define OP_PROFILE_EXIT()	DesktopFileLogger::Exit()
#define OP_PROFILE_CHECKPOINT_START(id, text)	DesktopFileLogger::CheckpointStart(id, text) 
#define OP_PROFILE_CHECKPOINT_END(id)			DesktopFileLogger::CheckpointEnd(id)

class OpFile;

class FileLogTarget
{
public:
	FileLogTarget() : m_file(NULL) {}
	~FileLogTarget() { OP_DELETE(m_file); }

	OP_STATUS Init(const OpStringC & filename);

	OP_STATUS OutputEntry(const OpStringC8& entry)
		{ return IsLogging() ? OutputEntryInternal(entry) : OpStatus::OK; }

	BOOL IsLogging() { return m_file && m_file->IsOpen(); }
	void StartLogging();
	void EndLogging();

private:
	OP_STATUS OutputEntryInternal(const OpStringC8& entry);

	OpLowLevelFile *m_file;	// we will not use OpFile as it is in a module not initialized yet and might or might not have dependencies

	static const OpStringC8		s_log_header;
	static const OpStringC8		s_log_footer;
};

namespace DesktopFileLogger
{
	OP_STATUS Log(OpFile*& file, const OpStringC8& heading, const OpStringC8& text);

	void Msg(const char *x);

	void CheckpointStart(const OpWindowCommander* commander, const char *text);
	void CheckpointEnd(const OpWindowCommander* commander);

	void Init();
	void Exit();

	class CheckpointEntry
	{
	private:
		CheckpointEntry() {}

	public:
		CheckpointEntry(const OpWindowCommander* commander, const char *text);

		const char*		m_text;				// message to print at the end of the checkpoint
		const OpWindowCommander* m_commander;		// WindowCommander for this check point
		double			m_startticks;		// start time
	};
};

struct ProfileLogger
{
	ProfileLogger() : log_level(0), filelog_target(NULL), logger_time_info(NULL) { }
	~ProfileLogger() { OP_DELETE(filelog_target); OP_DELETE(logger_time_info); }

	UINT32 log_level;						// nested level, 1 is top level
	FileLogTarget *filelog_target;
	OpTimeInfo *logger_time_info;		// pi is not initialized when we need this
	OpPointerHashTable<OpWindowCommander, DesktopFileLogger::CheckpointEntry> checkpoints;		// Hash table used for the check points
};
extern struct ProfileLogger* g_profile_logger;

// Will profile the time taken from the constructor to the destructor
class MethodProfiler
{
public:
	MethodProfiler(int not_used, const char *method)
	{
		g_profile_logger->log_level++;
		m_method = method;
		m_startticks = g_profile_logger->logger_time_info->GetRuntimeMS();
	}
	virtual ~MethodProfiler()
	{
		double endticks = g_profile_logger->logger_time_info->GetRuntimeMS();
		double ms_used = endticks - m_startticks;
		OpString8 tmp, date_str, out;

		double time_utc = g_profile_logger->logger_time_info->GetTimeUTC();
		time_t time_s = time_utc / 1000;
		int time_ms = (INT64)time_utc % 1000;

		struct tm* date = op_gmtime(&time_s);

		OpStatus::Ignore(tmp.Set(m_method));

		if(date)
		{
			if (date_str.Reserve(128))
			{
				strftime(date_str.CStr(), date_str.Capacity(), "%Y-%m-%d %H:%M:%S", date);
				out.AppendFormat("%s.%03d %3.0f ms - ", date_str.CStr(), time_ms, ms_used);

				UINT32 tmp_level = g_profile_logger->log_level - 1;
				while(tmp_level)
				{
					out.Append("+");
					tmp_level--;
				}
				if(g_profile_logger->log_level > 1)
				{
					out.Append(" ");
				}
				out.Append(tmp.CStr());

				if(g_profile_logger->filelog_target)
				{
					g_profile_logger->filelog_target->OutputEntry(out);
				}
			}
		}
		g_profile_logger->log_level--;
	}

private:
	const char	*m_method;
	double	m_startticks;
};
#endif // DESKTOPFILELOGGER_H
