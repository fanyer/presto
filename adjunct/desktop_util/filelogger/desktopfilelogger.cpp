/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Alexander Remen (alexr)
*/

#include "core/pch.h"

#include "adjunct/desktop_util/filelogger/desktopfilelogger.h"
#include "adjunct/quick/managers/CommandLineManager.h"

#include "modules/util/opfile/opfile.h"
#include "modules/util/datefun.h"

struct ProfileLogger* g_profile_logger;

OP_STATUS DesktopFileLogger::Log(OpFile*& file, const OpStringC8& heading, const OpStringC8& text)
{
    if (!file)
        return OpStatus::ERR_NULL_POINTER;

	OP_STATUS ret = OpStatus::OK;
    if (!file->IsOpen())
    {
		if (OpStatus::IsError(ret=file->Open(OPFILE_APPEND|OPFILE_SHAREDENYWRITE)))
        {
            delete file;
            file = NULL;
            return ret;
        }

        if (OpStatus::IsError(ret=file->Write("\r\n\r\n==== Logging started ====\r\n", 31)))
        {
            delete file;
            file = NULL;
            return ret;
        }
    }


	time_t log_time = g_timecache->CurrentTime();
    struct tm * local_time = localtime(&log_time);

	OpString8 time_string;
	// local_time will be NULL if CurrentTime() returned something weird
	if (local_time)
	{
		OpString time_string16;
		// let's use this format: "01/01-2009 20:19:20 "
		time_string16.Reserve(20);
		FMakeDate(*local_time, "\247D/\247M-\247Y \247h:\247m:\247s ", time_string16.CStr(), time_string16.Capacity());
		time_string.Set(time_string16.CStr());
	}
	if ( OpStatus::IsError(ret) || 
		OpStatus::IsError(ret=file->Write(time_string.CStr(), time_string.Length())) ||
		(!heading.IsEmpty() && OpStatus::IsError(ret=file->Write(heading.CStr(), heading.Length()))) ||
		OpStatus::IsError(ret=file->Write("\r\n", 2)) ||
		(!text.IsEmpty() && OpStatus::IsError(ret=file->Write(text.CStr(), text.Length()))) ||
		OpStatus::IsError(ret=file->Write("\r\n", 2)) )
    {
        delete file;
        file = NULL;
        return ret;
    }

    return file->Flush();
}
void DesktopFileLogger::Init()
{
	OP_ASSERT(g_profile_logger == NULL && "Don't call OP_PROFILE_INIT() twice");

	CommandLineArgument* log_filename = CommandLineManager::GetInstance()->GetArgument(CommandLineManager::ProfilingLog);
	
	if(log_filename && log_filename->m_string_value.HasContent())
	{
		g_profile_logger = OP_NEW(ProfileLogger, ());
		if(g_profile_logger)
		{
			if(OpStatus::IsSuccess(OpTimeInfo::Create(&g_profile_logger->logger_time_info)))
			{
				g_profile_logger->filelog_target = OP_NEW(FileLogTarget, ());
				if(g_profile_logger->filelog_target)
				{
					RETURN_VOID_IF_ERROR(g_profile_logger->filelog_target->Init(log_filename->m_string_value));
					g_profile_logger->filelog_target->StartLogging();
					return;
				}
			}
		}
	}
}

void DesktopFileLogger::Exit()
{
	if(g_profile_logger && g_profile_logger->filelog_target)
	{
		g_profile_logger->filelog_target->EndLogging();
	}
	OP_DELETE(g_profile_logger);
	g_profile_logger = NULL;
}

void DesktopFileLogger::Msg(const char *x)
{
	if(g_profile_logger && g_profile_logger->filelog_target)
	{
		OpString8 tmp, date_str, out;

		double time_utc = g_profile_logger->logger_time_info->GetTimeUTC();
		time_t time_s = time_utc / 1000;
		int time_ms = (INT64)time_utc % 1000;

		struct tm* date = op_gmtime(&time_s);

		OpStatus::Ignore(tmp.Set(x));

		if(date)
		{
			if (date_str.Reserve(128))
			{
				strftime(date_str.CStr(), date_str.Capacity(), "%Y-%m-%d %H:%M:%S", date);
				out.AppendFormat("%s.%03d -- %s", date_str.CStr(), time_ms, tmp.CStr());

				g_profile_logger->filelog_target->OutputEntry(out);
			}
		}
	}
}

void DesktopFileLogger::CheckpointStart(const OpWindowCommander* commander, const char* text)
{
	if(g_profile_logger && g_profile_logger->filelog_target)
	{
		CheckpointEntry *entry = OP_NEW(CheckpointEntry, (commander, text));
		if(entry)
		{
			// ignore errors, they're not critical
			OpStatus::Ignore(g_profile_logger->checkpoints.Add(commander, entry));
		}
	}
}

void DesktopFileLogger::CheckpointEnd(const OpWindowCommander* commander)
{
	if(g_profile_logger && g_profile_logger->filelog_target)
	{
		double endticks = g_profile_logger->logger_time_info->GetRuntimeMS();

		CheckpointEntry *entry;
		if(OpStatus::IsSuccess(g_profile_logger->checkpoints.Remove(commander, &entry)))
		{
			double ms_used = endticks - entry->m_startticks;
			OpString8 out;
			OpString8 url;

			url.Set(const_cast<OpWindowCommander*>(commander)->GetCurrentURL(FALSE));

			if(OpStatus::IsSuccess(out.AppendFormat("%3.0f ms - %s '%s'", ms_used, entry->m_text, url.CStr())))
			{
				Msg(out.CStr());
			}
			OP_DELETE(entry);
		}
	}
}

DesktopFileLogger::CheckpointEntry::CheckpointEntry(const OpWindowCommander* commander, const char *text)
{
	m_commander = commander;
	m_text = text;
	m_startticks = g_profile_logger->logger_time_info->GetRuntimeMS();
}

OP_STATUS FileLogTarget::OutputEntryInternal(const OpStringC8& entry)
{
	RETURN_IF_ERROR(m_file->Write(entry.CStr(), entry.Length()));
	RETURN_IF_ERROR(m_file->Write("\r\n", 2));
	RETURN_IF_ERROR(m_file->Flush());

	return OpStatus::OK;
}

const OpStringC8 FileLogTarget::s_log_header = "-- START LOGGING --";
const OpStringC8 FileLogTarget::s_log_footer = "-- STOP LOGGING --";

OP_STATUS FileLogTarget::Init(const OpStringC & filename)
{
	if (!m_file && filename.HasContent())
	{
		if(OpStatus::IsError(OpLowLevelFile::Create(&m_file, filename.CStr())))
		{
			m_file = NULL;
			return OpStatus::ERR;
		}
		if(OpStatus::IsError(m_file->Open(OPFILE_APPEND | OPFILE_SHAREDENYWRITE)))
		{
			OP_DELETE(m_file);
			m_file = NULL;
			return OpStatus::ERR;
		}
	}
	return OpStatus::OK;
}

void FileLogTarget::StartLogging()
{
	if(m_file && m_file->IsOpen())
	{
		RETURN_VOID_IF_ERROR(m_file->Write(s_log_header.CStr(), s_log_header.Length()));
		RETURN_VOID_IF_ERROR(m_file->Write("\r\n", 2));
	}
}


void FileLogTarget::EndLogging()
{
	if(m_file && m_file->IsOpen())
	{
		RETURN_VOID_IF_ERROR(m_file->Write(s_log_footer.CStr(), s_log_footer.Length()));
		RETURN_VOID_IF_ERROR(m_file->Write("\r\n", 2));
		m_file->Close();
	}
	OP_DELETE(m_file);
	m_file = NULL;
}
