/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

/** \file
 *
 * \brief Implement (most) functionality of the Debug class.
 *
 * Some of the methods in the Debug class must be implemented
 * by the platform somewhere else.
 */

#include "core/pch.h"

#ifdef _DEBUG

#include "modules/debug/debug_module.h"
#include "modules/debug/src/debug_internal.h"

#include "modules/util/opfile/opfile.h"
#include "modules/pi/OpTimeInfo.h"

#ifdef OPERA_CONSOLE
#  include "modules/console/opconsoleengine.h"
#endif

#if defined(STATIC_OPERA_OBJECT)
   /** Nonzero if it is safe to access the "global" variables */
#  define DBG_VARS_VALID 1
#else
#  define DBG_VARS_VALID (g_opera != 0)
#endif

/** Nonzero if the debug module seems to be initialized */
#define DBG_INIT_OK (DBG_VARS_VALID && g_dbg_init_magic == DBG_INIT_MAGIC)

void
DebugLink::Out()
{
	if (suc)
		// If we've a successor, unchain us

	    suc->pred = pred;
	else
		if (parent)
			// If we don't, we're the last in the list

			parent->last = pred;

	if (pred)
		// If we've a predecessor, unchain us

		pred->suc = suc;
	else
		if (parent)
			// If we don't, we're the first in the list

			parent->first = suc;

	pred = NULL;
	suc = NULL;
	parent = NULL;
}

void
DebugLink::Into(DebugHead* list)
{
	pred = list->last;

	if (pred)
		pred->suc = this;
	else
		list->first = this;

    list->last = this;
	parent = list;
}

DebugHead::~DebugHead(void)
{
	DebugLink* del;
	while ( (del = First()) != 0 )
	{
		del->Out();
		OP_DELETE(del);
	}
}

StringLink::StringLink(const uni_char* str, DebugHead* list)
{
	data = uni_strdup(str);
	Into(list);
}

StringLink::~StringLink()
{
	Out();
	op_free(data);
}

Debug::Debug(const char* function, const char* key)
{
	do_debugging = DoDebugging(key);

	if ( do_debugging )
	{
		function_name = uni_up_strdup(function);  // FIXME: Copy strings?
		debugging_key = uni_up_strdup(key);

		if (g_dbg_tracing)
		{
			g_dbg_level++;
			Enter();
		}
	}
	else
	{
		function_name = 0;
		debugging_key = 0;
	}
}

Debug::~Debug()
{
	if (do_debugging && g_dbg_tracing)
	{
		Exit();
		g_dbg_level--;

		if (g_dbg_level == 0 && g_dbg_out != NULL)
		{
			g_dbg_out->Close();
			OP_DELETE(g_dbg_out);
			g_dbg_out = NULL;
		}
	}

	op_free(function_name);
	function_name = 0;

	op_free(debugging_key);
	debugging_key = 0;
}

Debug& Debug::operator<<(const uni_char* text)
{
	return AddDbg(UNI_L("%s"), text);
}

Debug& Debug::operator<<(int number)
{
	return AddDbg(UNI_L("%d"), number);
}

Debug& Debug::operator<<(unsigned int number)
{
	return AddDbg(UNI_L("%u"), number);
}

Debug& Debug::operator<<(long number)
{
	return AddDbg(UNI_L("%ld"), number);
}

Debug& Debug::operator<<(unsigned long number)
{
	return AddDbg(UNI_L("%lu"), number);
}

Debug& Debug::operator<<(double number)
{
	return AddDbg(UNI_L("%f"), number);
}

Debug& Debug::operator<<(const void* pointer)
{
	return AddDbg(UNI_L("%p"), pointer);
}

Debug& Debug::operator<<(const OpPoint& p)
{
	return AddDbg(UNI_L("(%d,%d)"), p.x, p.y);
}

Debug& Debug::operator<<(const OpRect& r)
{
	return AddDbg(UNI_L("(%d,%d)+%dx%d"), r.x, r.y, r.width, r.height);
}

//
// This function can and should be called as late as possible by the
// platform, so the debug functionality is available for as long as
// possible. The porting interface for file-IO must still be
// operational when calling this function, and the global Opera
// object must still exist, even though it may have had its Destroy
// method called.
//
void
Debug::Free()
{
	if ( ! DBG_INIT_OK )
	{
		// The variables in the global opera object are not valid
		OP_ASSERT(!"Debug::Free called twise or with invalid global vars!");
		return;
	}

	g_dbg_init_magic = 0;

	for (;;)
	{
		StringLink* s = static_cast<StringLink*>(g_dbg_keywords.First());
		if ( s == 0 )
			break;
		OP_DELETE(s);
	}

	for (;;)
	{
		TimeStringLink* t = static_cast<TimeStringLink*>(g_dbg_timers.First());
		if ( t == 0 )
			break;
		OP_DELETE(t);
	}

	op_free(g_dbg_filename);
	g_dbg_filename = 0;

	if ( g_dbg_out != 0 )
	{
		g_dbg_out->Close();
		OP_DELETE(g_dbg_out);
		g_dbg_out = 0;
	}
}

void Debug::SystemDebugOutput(const uni_char* str)
{
	dbg_systemoutput(str);
}

uni_char*
Debug::InitSettings(const char* filename)
{
	// Load settings from file

	OpFile f;
	OpFile* in = &f;
	uni_char* uf = uni_up_strdup(filename);
	BOOL ok = uf && OpStatus::IsSuccess(in->Construct(uf));
	ok = ok && OpStatus::IsSuccess(in->Open(OPFILE_READ));
	op_free(uf);
	if (!ok)
	{
		uni_char* ufilename = uni_up_strdup(filename);
		if ( ufilename == 0 )
			return 0;

		const uni_char* full_path_and_name = f.GetFullPath();
		if (!full_path_and_name || !*full_path_and_name)
			full_path_and_name = ufilename;
		uni_char* buf = OP_NEWA(uni_char, 100 + uni_strlen(full_path_and_name));
		if ( buf != 0 )
			uni_sprintf(buf, UNI_L("Could not open debug settings file: %s"),
			full_path_and_name);
		op_free(ufilename);
		return buf;
	}

#define READLINE_BUFFER_SIZE 300

	uni_char str[READLINE_BUFFER_SIZE]; // ARRAY OK 2007-03-09 mortenro
	uni_char* tmp;
	int sLen;

	BOOL clearfile = FALSE;

	// Read all lines in file
	while (ReadLine(in, str, READLINE_BUFFER_SIZE))
	{
		if (*str == 0)
			continue;

		// Skip whitespace in the beginning of the line
		while (str[0] == ' ' || str[0] == '\t')
			uni_strcpy(str, str+1);

		if (*str == 0)
			continue;

		// Skip \r character at end of line
		if (str[uni_strlen(str) - 1] == '\r')
			str[uni_strlen(str) - 1] = '\0';

		if (*str == 0)
			continue;

		// Skip whitespace in the end of the line
		while (*str && (str[uni_strlen(str) - 1] == ' ' || str[uni_strlen(str) - 1] == '\t'))
			str[uni_strlen(str) - 1] = '\0';

		sLen = uni_strlen(str);

		if (sLen == 0)
			continue;

		// Skip lines starting with #
		if (*str == '#')
			continue;

		// Make all chars before equal sign lower case so we don't need to use FindI
		// which uses a table manager.
		uni_char* eq = uni_strstr(str, UNI_L("="));
		int eq_pos = -1;
		if (eq)
		{
			eq_pos = eq - str;

			for (int i=0; i < eq_pos && i < sLen; ++i)
			{
				str[i] = uni_tolower(str[i]);
			}
		}

		// Check for output=filename
		tmp = uni_strstr(str, UNI_L("output="));
		if (tmp)
		{
			tmp += 7;
			if (*tmp)
			{
				Debug::SetOutputFile(tmp);
			}
			continue;
		}

		// Check for clearfile=[on/off]
		tmp = uni_strstr(str, UNI_L("clearfile=on"));
		if (tmp == str)
		{
			clearfile = TRUE;
			continue;
		}

		// Check for debugging=[on/off]
		tmp = uni_strstr(str, UNI_L("debugging=on"));
		if (tmp == str)
		{
			g_dbg_debugging = TRUE;
			continue;
		}

		// Check for tracing=[on/off]
		tmp = uni_strstr(str, UNI_L("tracing=on"));
		if (tmp == str)
		{
			g_dbg_tracing = TRUE;
			continue;
		}

		// Check for prefix=off
		tmp = uni_strstr(str, UNI_L("prefix=off"));
		if (tmp == str)
		{
			g_dbg_prefix = FALSE;
			continue;
		}

		// Check for timing=[on/off]
		tmp = uni_strstr(str, UNI_L("timing=on"));
		if (tmp == str)
		{
			g_dbg_timing = TRUE;
			continue;
		}

		// Check for timestamp=[on/off]
		tmp = uni_strstr(str, UNI_L("timestamp=on"));
		if (tmp == str)
		{
			g_dbg_timestamp = TRUE;
			continue;
		}

		// Check for systemdebug=[on/off]
		tmp = uni_strstr(str, UNI_L("systemdebug=on"));
		if (tmp == str)
		{
			g_dbg_system_debug = TRUE;
			continue;
		}

		// Check for console=[on/off]
		tmp = uni_strstr(str, UNI_L("console=on"));
		if (tmp == str)
		{
			g_dbg_console = TRUE;
			continue;
		}

		// Add keyword
		if (uni_str_eq(str, "*")) 
			g_dbg_all_keywords = TRUE;
		else
			Debug::AddKeyword(str);
	}
	in->Close();

	// Clear output file if specified
	if (clearfile && g_dbg_filename && *g_dbg_filename)
	{
		uni_char* uf = uni_up_strdup(g_dbg_filename);
		if (uf)
		{
			OpFile f;
			f.Construct(uf);
			f.Open(OPFILE_WRITE);
			f.Close();
			op_free(uf);
		}
	}

	return NULL;
}

BOOL
Debug::DoDebugging(const uni_char* keyword)
{
    if (!g_opera || !g_dbg_debugging)
		return FALSE;

	return Find(&g_dbg_keywords, keyword) != NULL;
}

BOOL
Debug::DoDebugging(const char* keyword)
{
    if (!g_opera || !g_dbg_debugging)
		return FALSE;

	return Find(&g_dbg_keywords, keyword) != NULL;
}

int Debug::mkindent(uni_char* buf, int level, int bufsize)
{
	uni_char* ptr = buf;

	while ( bufsize > 2 && level > 1 )
	{
		*ptr++ = ' ';
		*ptr++ = ' ';
		level--;
		bufsize -= 2;
	}
	*ptr = 0;

	return ptr - buf;
}

void
Debug::Enter()
{
	Timestamp();

	int len = mkindent(g_dbg_mybuff, g_dbg_level, DEBUG_DEBUGBUFFERSIZE/2);

	uni_snprintf(g_dbg_mybuff + len, DEBUG_DEBUGBUFFERSIZE - len,
				 UNI_L("=> %s\n"),
				 function_name != 0 ? function_name : UNI_L("?"));

	Debug::FlushDbgBuffer();

	if (g_dbg_timing)
		GetTime(Start);
}

void
Debug::Exit()
{
	Timestamp();

	int len = mkindent(g_dbg_mybuff, g_dbg_level, DEBUG_DEBUGBUFFERSIZE/2);

	uni_snprintf(g_dbg_mybuff + len, DEBUG_DEBUGBUFFERSIZE - len,
				 UNI_L("<= %s%s\n"),
				 function_name != 0 ? function_name : UNI_L("?"),
				 Time_stuff());

	Debug::FlushDbgBuffer();
}

const uni_char*
Debug::Time_stuff()
{
	if (!g_dbg_timing)
		return UNI_L("");

	struct DebugTime time;
	GetTime(time);

	long int secs, millis;
	millis = time.msec - Start.msec;
	secs = time.sec - Start.sec;
	if(millis < 0)
	{
		millis += 1000;
		-- secs;
	}
	uni_snprintf(g_dbg_timebuff, DEBUG_DEBUGBUFFERSIZE, UNI_L("         %ld.%03ld seconds"), secs, millis);

    return g_dbg_timebuff;
}

void
Debug::Indent()
{
	Timestamp();

	if (g_dbg_tracing)
	{
		mkindent(g_dbg_mybuff, g_dbg_level + 1, DEBUG_DEBUGBUFFERSIZE);
		Debug::FlushDbgBuffer();
	}
}

void
Debug::Timestamp()
{
	if (g_dbg_timestamp)
	{
		double now = g_op_time_info->GetRuntimeMS();
		uni_snprintf(g_dbg_mybuff, DEBUG_DEBUGBUFFERSIZE, UNI_L("%14.1f "), now);
		Debug::FlushDbgBuffer();
	}
}

Debug&
Debug::Dbg(const uni_char* format, ...)
{
	if (!do_debugging)
		return *this;

	va_list arglist;
	va_start(arglist, format);

	VDbg(format, arglist);

	va_end(arglist);
	return *this;
}

Debug&
Debug::Dbg(const char* format, ...)
{
	if (!do_debugging)
		return *this;

	va_list arglist;
	va_start(arglist, format);

	VDbg(format, arglist);

	va_end(arglist);
	return *this;
}

void
Debug::DbgLen(const uni_char* s, int len)
{
	if (!do_debugging)
		return;

    Indent();

	uni_snprintf(g_dbg_mybuff, DEBUG_DEBUGBUFFERSIZE, UNI_L("%s:%s: "),
				 debugging_key != 0 ? debugging_key : UNI_L("?"),
				 function_name != 0 ? function_name : UNI_L("?"));
	int pos = uni_strlen(g_dbg_mybuff);

	if (pos + len + 2 < DEBUG_DEBUGBUFFERSIZE)
	{
		uni_strncat(g_dbg_mybuff, s, len);
		g_dbg_mybuff[pos + len] = 0;
		uni_strcat(g_dbg_mybuff, UNI_L("\n"));
		Debug::FlushDbgBuffer();
	}
}

Debug&
Debug::DbgIf(BOOL expression, const uni_char* format, ...)
{
	if (!do_debugging || !expression)
		return *this;

	va_list arglist;
	va_start(arglist, format);

	VDbg(format, arglist);

	va_end(arglist);
	return *this;
}

Debug&
Debug::DbgIf(BOOL expression, const char* format, ...)
{
	if (!do_debugging || !expression)
		return *this;

	va_list arglist;
	va_start(arglist, format);

	VDbg(format, arglist);

	va_end(arglist);
	return *this;
}

void
Debug::VDbg(const uni_char* format, va_list arglist)
{
	if (!do_debugging)
		return;

    Indent();

	int mybuffsize = 0;
	uni_char* mybuff_ptr = g_dbg_mybuff;

	if (g_dbg_prefix)
	{
		uni_snprintf(g_dbg_mybuff, DEBUG_DEBUGBUFFERSIZE, UNI_L("%s:%s: "),
					 debugging_key != 0 ? debugging_key : UNI_L("?"),
					 function_name != 0 ? function_name : UNI_L("?"));
		mybuffsize += uni_strlen(g_dbg_mybuff);
		mybuff_ptr += mybuffsize;
	}

	if (mybuffsize < DEBUG_DEBUGBUFFERSIZE - 2)
	{
		uni_vsnprintf(mybuff_ptr, DEBUG_DEBUGBUFFERSIZE - mybuffsize - 2, format, arglist);
		Debug::FlushDbgBuffer();
	}
}

void
Debug::VDbg(const char* format, va_list arglist)
{
	if (!do_debugging)
		return;

    Indent();

	int mybuffsize = 0;
	uni_char* mybuff_ptr = g_dbg_mybuff;

	if (g_dbg_prefix)
	{
		uni_snprintf(g_dbg_mybuff, DEBUG_DEBUGBUFFERSIZE, UNI_L("%s:%s: "),
					 debugging_key != 0 ? debugging_key : UNI_L("?"),
					 function_name != 0 ? function_name : UNI_L("?"));

		mybuffsize += uni_strlen(g_dbg_mybuff);
		mybuff_ptr += mybuffsize;
	}

	if (mybuffsize < DEBUG_DEBUGBUFFERSIZE - 2)
	{
		op_vsnprintf(g_dbg_mycbuff, DEBUG_DEBUGBUFFERSIZE - mybuffsize - 2, format, arglist);

		make_doublebyte_in_buffer(g_dbg_mycbuff, op_strlen(g_dbg_mycbuff), mybuff_ptr, op_strlen(g_dbg_mycbuff)+1);
		Debug::FlushDbgBuffer();
	}
}

void
Debug::TimeStart(const uni_char* key, const uni_char* s)
{
	if (!g_dbg_timing || !DoDebugging(key))
		return;

	struct DebugTime time;
	GetTime(time);

	TimeStringLink* link = (TimeStringLink*)Find(&g_dbg_timers, key);
	if (link == NULL)
	{
		// Not found, so put one in.
		OP_NEW(TimeStringLink, (time, key, &g_dbg_timers));
	}
	else
	{
		link->time = time;
	}

	Indent();

    uni_snprintf(g_dbg_mybuff, DEBUG_DEBUGBUFFERSIZE, UNI_L("%s: %s %ld.%03d\n"), key, s, time.sec, time.msec);

	if (g_dbg_use_file)
		PrintToFile(g_dbg_mybuff);

	if (g_dbg_system_debug)
		dbg_systemoutput(g_dbg_mybuff);

#ifdef OPERA_CONSOLE
	if (g_dbg_console)
		ConsoleOutput(g_dbg_mybuff);
#endif
}

void
Debug::TimeStart(const char* key, const char* s)
{
	uni_char* ukey = uni_up_strdup(key);
	uni_char* us = uni_up_strdup(s);

	if ( ukey != 0 && us != 0 )
		Debug::TimeStart(ukey, us);

	op_free(ukey);
	op_free(us);
}

void
Debug::TimeEnd(const uni_char* key, const uni_char* s)
{
	if (!g_dbg_timing || !DoDebugging(key))
		return;

	struct DebugTime time;
	GetTime(time);

	TimeStringLink* link = (TimeStringLink*)Find(&g_dbg_timers, key);
	if (link == NULL)
	{
		// Not found!
		// This puts an entry in the g_dbg_timers vector
		// it won't be very informative, but it keeps
		// things going OK.
		link = OP_NEW(TimeStringLink, (time, key, &g_dbg_timers));
	}

    int secs, millis;
    millis = time.msec - link->time.msec;
    secs = time.sec - link->time.sec;
    if(millis < 0)
	{
		millis += 1000;
		--secs;
	}

	Indent();

    uni_snprintf(g_dbg_mybuff, DEBUG_DEBUGBUFFERSIZE, UNI_L("%s: %s %d.%03d\n"), key, s, secs, millis);

	if (g_dbg_use_file)
		PrintToFile(g_dbg_mybuff);

	if (g_dbg_system_debug)
		dbg_systemoutput(g_dbg_mybuff);

#ifdef OPERA_CONSOLE
	if (g_dbg_console)
		ConsoleOutput(g_dbg_mybuff);
#endif
}

void
Debug::TimeEnd(const char* key, const char* s)
{
	uni_char* ukey = uni_up_strdup(key);
	uni_char* us = uni_up_strdup(s);

	if ( ukey != 0 && us != 0 )
		Debug::TimeEnd(ukey, us);

	op_free(ukey);
	op_free(us);
}

void
Debug::Sync()
{
	// FIXME: Sync
}

StringLink*
Debug::AddKeyword(const uni_char* keyword)
{
	return OP_NEW(StringLink, (keyword, &g_dbg_keywords));
}

void
Debug::SetOutputFile(const uni_char* filename)
{
	if (!g_dbg_filename || !uni_str_eq(filename, g_dbg_filename))
	{
		op_free(g_dbg_filename);
		g_dbg_filename = uni_down_strdup(filename);
		if ( g_dbg_filename == 0 )
		{
			g_dbg_use_file = FALSE;
			return;
		}

		g_dbg_use_file = TRUE;
		SetupOutput();
	}
}

#ifdef OPERA_CONSOLE
/* static */ void
Debug::ConsoleOutput(const uni_char* str)
{
	// Send all debug output to the console.

	if (g_console && g_console->IsLogging())
	{
		OpConsoleEngine::Message cmessage(OpConsoleEngine::Internal, OpConsoleEngine::Debugging);
		OpStatus::Ignore(cmessage.message.Set(str));
		TRAPD(rc, g_console->PostMessageL(&cmessage));
		OpStatus::Ignore(rc);
	}
}
#endif // OPERA_CONSOLE

void
Debug::SetupOutput()
{
	if (g_dbg_out != NULL)
	{
		g_dbg_out->Close();
		OP_DELETE(g_dbg_out);
		g_dbg_out = NULL;
	}
	if (!g_dbg_use_file)
		return;

	uni_char* uf = uni_up_strdup(g_dbg_filename);
	if (uf)
	{
		g_dbg_out = OP_NEW(OpFile, ());
		if (!g_dbg_out || OpStatus::IsError(g_dbg_out->Construct(uf)) || OpStatus::IsError(g_dbg_out->Open(OPFILE_APPEND)))
		{
			OP_DELETE(g_dbg_out);
			g_dbg_out = NULL;
		}
		op_free(uf);
	}
}

DebugLink*
Debug::Find(DebugHead* list, const uni_char* s)
{
	for (StringLink* sl = (StringLink*)list->First(); sl; sl = (StringLink*)sl->Suc())
	{
		if (uni_str_eq(sl->data, s))
			return sl;
	}

	if (g_dbg_all_keywords)
		return AddKeyword(s);

	return NULL;
}

DebugLink*
Debug::Find(DebugHead* list, const char* s)
{
	for (StringLink* sl = (StringLink*)list->First(); sl; sl = (StringLink*)sl->Suc())
	{
		if (uni_str_eq(sl->data, s))
			return sl;
	}

	if (g_dbg_all_keywords)
	{
		OpString str;
		if (OpStatus::IsError(str.Set(s)))
			return NULL;
		return AddKeyword(str.CStr());
	}

	return NULL;
}

void
Debug::PrintToFile(const uni_char* str)
{
	if (g_dbg_out == NULL)
		SetupOutput();

	if (g_dbg_out == NULL)
		return;

	//FIXME: This is not the way to go.

	char* singlebyte_str = uni_down_strdup(str);
	if (singlebyte_str == NULL)
		return;

	g_dbg_out->Write(singlebyte_str, op_strlen(singlebyte_str));
	Sync();

	op_free(singlebyte_str);
}

BOOL
Debug::ReadLine(OpFile* file, uni_char* str, int len)
{
	if (file->Eof())
	{
		return FALSE;
	}

	char c;
	int pos = 0;

	while (pos < len)
	{
		if ( OpStatus::IsError(file->ReadBinByte(c)) )
			return FALSE;

		if (file->Eof() || c == '\n')
		{
			str[pos] = '\0';
			return TRUE;
		}

		str[pos] = c;
		++pos;
	}

	str[len-1] = '\0';
	return TRUE;
}

Debug& Debug::AddDbg(const uni_char* format, ...)
{
	if (do_debugging)
	{
		va_list arglist;
		va_start(arglist, format);

		uni_vsnprintf(g_dbg_mybuff, DEBUG_DEBUGBUFFERSIZE, format, arglist);
		Debug::FlushDbgBuffer();

		va_end(arglist);
	}
	return *this;
}

Debug& Debug::AddDbg(const char* format, ...)
{
	if (do_debugging)
	{
		va_list arglist;
		va_start(arglist, format);

		op_vsnprintf(g_dbg_mycbuff, DEBUG_DEBUGBUFFERSIZE, format, arglist);
		size_t c_len = op_strlen(g_dbg_mycbuff);
		make_doublebyte_in_buffer(g_dbg_mycbuff, c_len, g_dbg_mybuff, c_len+1);
		Debug::FlushDbgBuffer();

		va_end(arglist);
	}
	return *this;
}

/* static */
void Debug::FlushDbgBuffer()
{
	if (g_dbg_use_file)
		PrintToFile(g_dbg_mybuff);

	if (g_dbg_system_debug)
		dbg_systemoutput(g_dbg_mybuff);

#ifdef OPERA_CONSOLE
	if (g_dbg_console)
		ConsoleOutput(g_dbg_mybuff);
#endif
}

#ifdef USE_TIMING

DebugTimer::timerdata_t DebugTimer::g_timerdata[N_MODULES];

DebugTimer::DebugTimer(module_t module)
	: m_module(module)
{
	g_timerdata[module].timers++;
	if (g_timerdata[module].timers == 1)
	{
		millisecs_t now = GetTime();
		g_timerdata[module].start = now;
	}
}

DebugTimer::~DebugTimer()
{
	g_timerdata[m_module].timers--;
	if (g_timerdata[m_module].timers == 0)
	{
		millisecs_t now     = GetTime();
		millisecs_t then    = g_timerdata[m_module].start;
		millisecs_t elapsed = now - then;

		g_timerdata[m_module].total += elapsed;
	}
}

// static
const char*
DebugTimer::GetSummary()
{
	/* Each module contributes its total and a semicolon to g_buf.  We also need
	 * space for one '\0' at the end.  We're going to return the buffer, so it
	 * needs to be static; hence so does its size. */
	static const size_t each = UINT_STRSIZE(g_timerdata[0].total) + 1;
	static char g_buf[1 + each * N_MODULES]; // ARRAY OK 2012-02-15 eddy
	char *tail = g_buf;
	tail[0] = '\0';

	for (int i = 0; i < N_MODULES; i++)
	{
		op_snprintf(tail, each, "%ld;", g_timerdata[i].total);
		tail += op_strlen(tail);

		g_timerdata[i].total = 0;
	}
	OP_ASSERT(tail < g_buf + sizeof(g_buf));

	return g_buf;
}

#endif // USE_TIMING

#endif // _DEBUG
