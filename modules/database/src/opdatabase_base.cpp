/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/database/src/opdatabase_base.h"

#if defined DATABASE_MODULE_MANAGER_SUPPORT || defined OPSTORAGE_SUPPORT

#include "modules/pi/OpSystemInfo.h"

#ifdef SQLITE_SUPPORT
#include "modules/sqlite/sqlite3.h"
#endif //SQLITE_SUPPORT

/**
 * While this does not make into a pi interface, it'll need to be here
 */
BOOL OpDbUtils::IsFilePathAbsolute(const uni_char* p)
{
	OP_ASSERT(p != NULL);
//as seen on modules/dom/src/js/location.cpp
#ifdef SYS_CAP_FILESYSTEM_HAS_DRIVES
	return p[0] && op_isalpha(p[0]) && p[1] && p[1] == ':' && p[2] && (p[2] == '/' || p[2] == '\\');
#else
	return p[0] == PATHSEPCHAR;
#endif
}

OP_STATUS OpDbUtils::DuplicateString(const uni_char* src_value, uni_char** dest_value)
{
	OP_ASSERT(dest_value != NULL);
	unsigned src_value_length  = src_value != NULL ? uni_strlen(src_value) : 0;
	unsigned dest_value_length = (*dest_value) != NULL ? uni_strlen(*dest_value) : 0;
	return DuplicateString(src_value, src_value_length, dest_value, &dest_value_length);
}

OP_STATUS OpDbUtils::DuplicateString(const uni_char* src_value, unsigned src_length,
		uni_char** dest_value, unsigned *dest_length)
{
	OP_ASSERT(dest_value != NULL && dest_length != NULL);
	if (src_value == NULL)
	{
		if (*dest_value != NULL)
		{
			OP_DELETEA(*dest_value);
			*dest_value = NULL;
			*dest_length = 0;
		}
	}
	else if (*dest_value != NULL && *dest_length > 0 && src_length <= *dest_length)
	{
		op_memcpy(*dest_value, src_value, UNICODE_SIZE(src_length));
		(*dest_value)[src_length] = 0;
		*dest_length = src_length;
	}
	else //if(new_value_length>*m_field_length)
	{
		uni_char *new_field_value = OP_NEWA(uni_char, src_length+1);
		if (new_field_value == NULL)
			return SIGNAL_OP_STATUS_ERROR(OpStatus::ERR_NO_MEMORY);
		OP_DELETEA(*dest_value);
		*dest_value = new_field_value;
		op_memcpy(*dest_value, src_value, UNICODE_SIZE(src_length));
		*dest_length = src_length;
		(*dest_value)[src_length] = 0;
	}

	return OpStatus::OK;
}

void
OpDbUtils::ReportCondition(OP_STATUS status)
{
	if (GetMessageHandler() == NULL)
		return;

	if (status == OpStatus::ERR_NO_MEMORY)
	{
#ifdef SQLITE_SUPPORT
		sqlite3_release_memory(50 << 20);
#endif //SQLITE_SUPPORT
		GetMessageHandler()->PostOOMCondition(TRUE);
	}
	else if (status == OpStatus::ERR_SOFT_NO_MEMORY)
	{
#ifdef SQLITE_SUPPORT
		sqlite3_release_memory(10 << 20);
#endif //SQLITE_SUPPORT
		GetMessageHandler()->PostOOMCondition(FALSE);
	}
	else if (status == OpStatus::ERR_NO_DISK)
	{
		GetMessageHandler()->PostOODCondition();
	}
	else
		OpStatus::Ignore(status);
}

/*static*/
BOOL OpDbUtils::StringsEqual(const uni_char* lhs_value, unsigned lhs_length,
		const uni_char* rhs_value, unsigned rhs_length)
{
	return lhs_length == rhs_length &&
			(lhs_value == rhs_value ||
				(lhs_value != NULL && rhs_value != NULL && uni_strncmp(lhs_value, rhs_value, lhs_length) == 0)
			);
}

void
PS_ObjDelListener::FireShutdownCallbacks()
{
	ResourceShutdownCallback::ResourceShutdownCallbackLinkElem *m, *n = m_shutdown_listeners.First();
	while (n != NULL)
	{
		m = n->Suc();
		RemoveShutdownCallback(n->m_cb);
		n->m_cb->HandleResourceShutdown();
		n = m;
	}
}

#ifdef DATABASE_MODULE_DEBUG
/*static*/
OP_STATUS
OpDbUtils::SignalOpStatusError(OP_STATUS status, const char* msg, const char* file, int line)
{
	if (OpStatus::IsError(status))
		dbg_printf("db-error: %s(line %d): OP_STATUS = %d, %s \n", file, line, status, msg);
	return status;
}
#ifdef SUPPORT_DATABASE_INTERNAL
#include "modules/database/opdatabase.h"
const char *OpDbUtils::DownsizeQuery(const SqlText &sql_text)
{
	if (sql_text.Is8Bit())
		return sql_text.m_sql8;

	// 1kb is good enough for debugging.
	// Note: being static this will be shared between threads.
	// FIXME to use thread local storage , when threading is introduced, although it's
	// not a strict requirement because the number of database slaves running queries
	// with debuging output will be limited, and most likely just one.
	static char singlebyte_sql[1024]; /* ARRAY OK joaoe 2012-04-02 */
	make_singlebyte_in_buffer(sql_text.m_sql16, sql_text.m_sql_length, singlebyte_sql, ARRAY_SIZE(singlebyte_sql) - 1);
	singlebyte_sql[MIN(ARRAY_SIZE(singlebyte_sql) - 1, sql_text.m_sql_length)] = 0;
	return singlebyte_sql;
}
#endif //  SUPPORT_DATABASE_INTERNAL
#endif //DATABASE_MODULE_DEBUG

void
OpStopWatch::Start()
{
	if (!m_is_running)
	{
		m_accumulated = 0;
		m_last_start = g_op_time_info->GetWallClockMS();
	}
	m_is_running = TRUE;
}

void
OpStopWatch::Continue()
{
	if (!m_is_running)
	{
		m_last_start = g_op_time_info->GetWallClockMS();
	}
	m_is_running = TRUE;
}

void
OpStopWatch::Stop()
{
	if (m_is_running)
	{
		m_accumulated += (unsigned)(g_op_time_info->GetWallClockMS()-m_last_start);
	}
	m_is_running = FALSE;
}

unsigned
OpStopWatch::GetEllapsedMS() const
{
	return m_is_running ?
		m_accumulated + (unsigned)(g_op_time_info->GetWallClockMS() - m_last_start) :
		m_accumulated;
}

#endif //defined DATABASE_MODULE_MANAGER_SUPPORT || defined OPSTORAGE_SUPPORT

