/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#include "core/pch.h"

#if defined SEARCH_ENGINE && defined SELFTEST && defined SEARCH_ENGINE_LOG

#include "modules/search_engine/tests/LogPlayer.h"

#if defined SEARCH_ENGINE_FOR_MAIL && (SEARCH_ENGINE_LOG & SEARCH_ENGINE_LOG_STRINGTABLE)

#define is_msg(m) (op_strcmp(log->Id(), (m)) == 0)
BOOL IsError(OP_STATUS status)
{
	if (OpStatus::IsError(status))
		return TRUE;
	return FALSE;
}
#define BREAK_IF_ERROR(cmd) if (IsError(status = (cmd))) break

OP_STATUS StringTablePlayer::Play(StringTable &table, const uni_char *table_path, const uni_char *table_name, const uni_char *log_name, OpFileFolder folder)
{
	InputLogDevice *log;
	OP_BOOLEAN status;
	OpString path;
	int last_flush_req = 0;  // 0 = PreFlush, 1 = Flush
	int last_flush_status = -1;  // -1 = none, 0 = timeout, 1 = finished

	error_count = 0;

	if ((log = SearchEngineLog::OpenLog(log_name, folder)) == NULL)
		return OpStatus::ERR_FILE_NOT_FOUND;

	while ((status = log->Read()) == OpBoolean::IS_TRUE)
	{
		if (is_msg("ax"))
		{
			table.Close();

			path.Empty();
			path.AppendFormat(UNI_L("%s%c%s.axx"), table_path, PATHSEPCHAR, table_name);

			BREAK_IF_ERROR(log->SaveData(path));

			continue;
		}

		if (is_msg("ax-j"))
		{
			table.Close();

			path.Empty();
			path.AppendFormat(UNI_L("%s%c%s.axx-j"), table_path, PATHSEPCHAR, table_name);

			BREAK_IF_ERROR(log->SaveData(path));

			continue;
		}

		if (is_msg("ax-g"))
		{
			table.Close();

			path.Empty();
			path.AppendFormat(UNI_L("%s%c%s.axx-g"), table_path, PATHSEPCHAR, table_name);

			BREAK_IF_ERROR(log->SaveData(path));

			continue;
		}

		if (is_msg("bx"))
		{
			table.Close();

			path.Empty();
			path.AppendFormat(UNI_L("%s%c%s.bx"), table_path, PATHSEPCHAR, table_name);

			BREAK_IF_ERROR(log->SaveData(path));

			continue;
		}

		if (is_msg("bx-j"))
		{
			table.Close();

			path.Empty();
			path.AppendFormat(UNI_L("%s%c%s.bx-j"), table_path, PATHSEPCHAR, table_name);

			BREAK_IF_ERROR(log->SaveData(path));

			continue;
		}

		if (is_msg("bx-g"))
		{
			table.Close();

			path.Empty();
			path.AppendFormat(UNI_L("%s%c%s.bx-g"), table_path, PATHSEPCHAR, table_name);

			BREAK_IF_ERROR(log->SaveData(path));

			continue;
		}

		if (is_msg("Open"))
		{
			if (OpStatus::IsError(table.Open(table_path, table_name, op_atoi(log->Message()))))
				++error_count;

			continue;
		}

		if (is_msg("Close"))
		{
			table.Close();

			continue;
		}

		if (is_msg("Clear"))
		{
			table.Clear();

			continue;
		}

		if (is_msg("PreFlush"))
		{
			if (last_flush_req != 0 || last_flush_status != 1)
			{
				switch (table.PreFlush(op_atoi(log->Message())))
				{
				case OpBoolean::IS_TRUE:
					last_flush_status = 1;
					break;
				case OpBoolean::IS_FALSE:
					last_flush_status = 0;
					break;
				default:
					last_flush_status = -1;
					++error_count;
				}
			}

			last_flush_req = 0;
			continue;
		}

		if (is_msg("Flush"))
		{
			if (last_flush_req != 1 || last_flush_status != 1)
			{
				switch (table.Flush(op_atoi(log->Message())))
				{
				case OpBoolean::IS_TRUE:
					last_flush_status = 1;
					break;
				case OpBoolean::IS_FALSE:
					last_flush_status = 0;
					break;
				default:
					last_flush_status = -1;
					++error_count;
				}
			}

			last_flush_req = 1;
			continue;
		}

		if (is_msg("Commit"))
		{
			if (OpStatus::IsError(table.Commit()))
				++error_count;

			continue;
		}

		if (is_msg("Insert"))
		{
			if (OpStatus::IsError(table.Insert(log->UMessage() + 9, op_strtoul(log->Message(), NULL, 16))))
				++error_count;

			continue;
		}

		if (is_msg("Delete"))
		{
			if (OpStatus::IsError(table.Delete(log->UMessage() + 9, op_strtoul(log->Message(), NULL, 16))))
				++error_count;

			continue;
		}

		OP_ASSERT(0);  // unknown log message
	}

	OP_DELETE(log);

	return status == OpBoolean::IS_FALSE ? OpStatus::OK : status;
}

#endif  // SEARCH_ENGINE_FOR_MAIL

#endif  // SEARCH_ENGINE && SELFTEST

