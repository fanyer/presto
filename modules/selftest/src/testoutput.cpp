/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; -*-
**
** Copyright (C) 2002-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/
#include "core/pch.h"

#ifdef SELFTEST
#include "modules/dom/domenvironment.h"
#include "modules/dochand/win.h"
#include "modules/selftest/operaselftestdispatcher.h"
#include "modules/selftest/src/testutils.h"
#include "modules/selftest/src/testoutput.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/str.h"
#include "modules/windowcommander/src/WindowCommander.h"

#ifdef BREW
# include <aeestdlib.h>
#endif // BREW

#ifdef _MSC_VER
#define LINENUMBER_FMT "(%d): "
#else
#define LINENUMBER_FMT ":%d: "
#endif

/**
 * We encapsulate the writing of debug strings since we do different things
 * on different platforms.
 */
/*static*/
void
TS_WriteDebugString(const char* str)
{
#ifdef OVERRIDE_SELFTEST_OUTPUT
	if(OperaSelftestDispatcher::isSelftestDispatcherRunning()){
		if (OperaSelftestDispatcher::selftestWriteOutput(str))
			return;
	}
#endif // OVERRIDE_SELFTEST_OUTPUT
#if defined(BREW)
	DBGPRINTF(str);
#elif defined(_MSC_VER) || defined(WINGOGI)
	OutputDebugStringA(str);
#elif defined(UNIX) || defined(DARWIN) || defined(_MACINTOSH_) || defined(LINGOGI) || defined(GOGI_SDK) || defined(LINUX)
	printf( "%s", str );
	fflush(stdout);
#else
	OP_ASSERT(FALSE); 
	//We currently dont have a 
	//way to write testresults to console
#endif
}

/**
 * We encapsulate the cleanup of debug strings since we do different things
 * on different platforms.
 */
/*static*/
void
TS_FinishDebugString()
{
#ifdef OVERRIDE_SELFTEST_OUTPUT
	if(OperaSelftestDispatcher::isSelftestDispatcherRunning())
		OperaSelftestDispatcher::selftestFinishOutput();
#endif // OVERRIDE_SELFTEST_OUTPUT
}

TestOutput::TestOutput(BOOL force_stdout)
 :
	output_file(NULL),
	force_stdout(force_stdout),
	always_verbose(TRUE)
{}

TestOutput::~TestOutput()
{
	OP_DELETE(output_file);
}


OP_STATUS
TestOutput::Construct(char* output_filename)
{
	if (output_filename)
	{
		OpFile * emit = OP_NEW(OpFile, ());
		if( !emit )
		{
			OutputFormatted("Failed to allocate handler for output file '%s'\n", output_filename );
			return OpStatus::ERR_NO_MEMORY;
		}
		
		OP_STATUS err = emit->Construct(ST_up(output_filename));
		if( OpStatus::IsError(err) )
		{
			OP_DELETE(emit);
			OutputFormatted("Failed to create output file '%s'\n", output_filename );
			return err;
		}
		
		err = emit->Open( OPFILE_WRITE );
		if( OpStatus::IsError(err) )
		{
			OP_DELETE(emit);
			OutputFormatted("Failed to open output file '%s'\n", output_filename );
			return err;
		}

		OutputFormatted("Note: Output sent to '%s'. No further selftest output here.\n", output_filename );
		output_file = emit;
	}
	return OpStatus::OK;
}

void TestOutput::Warning( const char *fmt, ...)
{
	va_list args;
	va_start( args, fmt );
	OutputFormattedv(0, 0, 1, fmt, args);
	va_end( args );
}

void TestOutput::OutputFormattedv( const char* file, int line, int verbose, const char *fmt, va_list args )
{
	if( !fmt || (!verbose && !always_verbose) )
		return;
	if( file )
	{
		Output( file );
		// A linenumber is very seldom larger than 2^64, which is 20
		// characters in base 10, add a sign (for negative lines. :-))
		// add the '():' and whitespace, and we get up to 25 characters.
		//
        // You need 128bit integers to overrun this buffer.
		//
		char buffer[32]; /* ARRAY OK 2007-03-09 marcusc */
		op_sprintf( buffer, LINENUMBER_FMT, line );
		Output( buffer );
	}

	OpString8 buffer;
	OP_STATUS ret = buffer.AppendVFormat( fmt, args );
	if ( OpStatus::IsError( ret ) )
	{
		if ( OpStatus::IsMemoryError( ret ) )
			g_memory_manager->RaiseCondition( OpStatus::ERR_NO_MEMORY );
		return;
	}
	Output( buffer.CStr() );
}

void
TestOutput::OutputFormatted( const char *file, int line, int verbose, const char *fmt, ... )
{
	va_list args;
	va_start( args, fmt );
	OutputFormattedv(file, line, verbose, fmt, args);
	va_end( args );
}

void
TestOutput::OutputFormatted( const char *fmt, ... )
{
	va_list args;
	va_start( args, fmt );
	OutputFormattedv(0, 0, 1, fmt, args);
	va_end( args );
}

void
TestOutput::Output( const char* txt )
{
	if (output_file)
	{
		output_file->Write(txt, op_strlen(txt));
		output_file->Flush();
	}
	else
	{
		TS_WriteDebugString(txt);
	}
}

const char*
TestOutput::OpStatusToDescription(OP_STATUS status)
{
#define M_OP_STATUS_CASE_TO_STR(value) if((status) == (value)) return #value;
	M_OP_STATUS_CASE_TO_STR(OpStatus::OK)
	M_OP_STATUS_CASE_TO_STR(OpStatus::ERR)
	M_OP_STATUS_CASE_TO_STR(OpStatus::ERR_NO_MEMORY)
	M_OP_STATUS_CASE_TO_STR(OpStatus::ERR_NULL_POINTER)
	M_OP_STATUS_CASE_TO_STR(OpStatus::ERR_OUT_OF_RANGE)
	M_OP_STATUS_CASE_TO_STR(OpStatus::ERR_NO_ACCESS)
	M_OP_STATUS_CASE_TO_STR(OpStatus::ERR_NOT_DELAYED)
	M_OP_STATUS_CASE_TO_STR(OpStatus::ERR_FILE_NOT_FOUND)
	M_OP_STATUS_CASE_TO_STR(OpStatus::ERR_NO_DISK)
	M_OP_STATUS_CASE_TO_STR(OpStatus::ERR_NOT_SUPPORTED)
	M_OP_STATUS_CASE_TO_STR(OpStatus::ERR_PARSING_FAILED)
	M_OP_STATUS_CASE_TO_STR(OpStatus::ERR_NO_SUCH_RESOURCE)
	M_OP_STATUS_CASE_TO_STR(OpStatus::ERR_BAD_FILE_NUMBER)
	M_OP_STATUS_CASE_TO_STR(OpStatus::ERR_YIELD)
	M_OP_STATUS_CASE_TO_STR(OpStatus::ERR_SOFT_NO_MEMORY)
	M_OP_STATUS_CASE_TO_STR(OpBoolean::IS_TRUE)
	M_OP_STATUS_CASE_TO_STR(OpBoolean::IS_FALSE)

	op_snprintf(m_op_status_desc, 64, "%d", OpStatus::GetIntValue(status));
	return m_op_status_desc;
}

/// StandardTestOutput
StandardTestOutput::StandardTestOutput()
 :	num_tests(0),
	num_skipped(0),
	num_failed_tests(0)
{
}

void
StandardTestOutput::GroupHeader(const char* file_name, unsigned int line, const char* group_name)
{
	OutputFormatted("\n=========================================================================\n");
	OutputFormatted("%s(%d):  %s\n", file_name, line, group_name );
	OutputFormatted("=========================================================================\n");
}

void
StandardTestOutput::TestDescribe(const char* /*module_name*/, const char* /*group_name*/, const char* test_name)
{
	num_tests++;

	int l, i;
	char dots[63]; /* ARRAY OK 2007-03-09 marcusc */

	l = op_strlen(test_name);

	for(i=0; i<62-l; i++ )
		dots[i]='.';

	dots[i]=0;
	OutputFormatted("  %s %s ", test_name, dots );
}

void
StandardTestOutput::TestFailed(const char* file_name, int line, const char* fmt, va_list args)
{
	num_failed_tests++;
	
	OpString8 buffer;
	RETURN_VOID_IF_ERROR(buffer.AppendVFormat(fmt, args));
	
	if (file_name)
		failed_report.Add(file_name, line, buffer.CStr());

	OutputFormatted(file_name, line, FALSE, "%s\n", buffer.CStr());
}

void
StandardTestOutput::TestFailedWithStatus(const char* file_name, int line, const char* fmt, OP_STATUS status)
{
	num_failed_tests++;
	
	OpString8 buffer;
	if (fmt)
		RETURN_VOID_IF_ERROR(buffer.AppendFormat("%s, returned %s", fmt, OpStatusToDescription(status)));
	else
		RETURN_VOID_IF_ERROR(buffer.AppendFormat("error: %s", OpStatusToDescription(status)));
	
	if (file_name)
		failed_report.Add(file_name, line, buffer.CStr());

	OutputFormatted(file_name, line, FALSE, "%s\n", buffer.CStr());
}

void
StandardTestOutput::TestSkipped(const char* /*module_name*/, const char* /*group_name*/, const char* test_name, const char* reason)
{
	num_skipped++;

	int l, i;
	char dots[63]; /* ARRAY OK 2007-03-09 marcusc */
	l = op_strlen(test_name);
	for(i=0; i<62-l; i++ ) dots[i]='.';
	dots[i]=0;
	OutputFormatted("  %s %s Skipped %s\n", test_name, dots,reason );
}

void
StandardTestOutput::Passed()
{
	OutputFormatted("Passed\n");
}

void
StandardTestOutput::Failed(const char* reason, const char* file, int line, va_list* args)
{
	OutputFormatted("Failed\n");
}

void
StandardTestOutput::FailedWithStatus(const char* reason, const char* file, int line, OP_STATUS status)
{
	OutputFormatted(OpStatus::IsMemoryError(status) ? "Out of memory\n" : "Failed\n");
}

void
StandardTestOutput::Leak(unsigned count)
{
	/*
	if (count)
		OutputFormatted("    Leaked in %i location%s\n", count, count > 1 ? "s" : "");
		*/
}

void
StandardTestOutput::Timer(unsigned ms)
{
	if (ms)
		OutputFormatted("    %i ms\n", ms);
}

void
StandardTestOutput::Summary()
{
	int passed_count = num_tests-num_failed_tests;
	OutputFormatted("=========================================================================\n" );
	OutputFormatted("%d test%s run: %d passed, %d failed (%d test%s skipped)\n",
		   num_tests, (num_tests!=1?"s":""),
		   passed_count,
		   num_failed_tests,
		   num_skipped, (num_skipped!=1?"s":"")
		);
	if( num_failed_tests )
		OutputFormatted("Place a breakpoint in testsuite_break_here() to debug the failed test%s\n",
			  (num_failed_tests!=1?"s":""));
	OutputFormatted("=========================================================================\n" );

	const char* report = failed_report.GetReport();

	if (report)
	{
		if (num_failed_tests == 1)
			OutputFormatted("The failed test:\n");
		else
			OutputFormatted("All failed tests:\n");
		OutputFormatted("%s", report);
		OutputFormatted("=========================================================================\n");
	}

	OutputFormatted("\n");

#ifdef GOGI_SELFTEST_FINISHED_NOTIFICATION
	if( g_selftest.utils->window )
	{
		Window* w = g_selftest.utils->GetWindow();
		if( w )
		{
			WindowCommander* wc = w->GetWindowCommander();
			if( wc )
			{
				wc->GetSelftestListener()->OnSelftestFinished();
			}
		}
	}
#endif // GOGI_SELFTEST_FINISHED_NOTIFICATION
}

void
StandardTestOutput::PublicOutputFormatted( const char* file, int line, int verbose, const char* fmt, ...)
{
	va_list args;
	va_start( args, fmt );
	OutputFormattedv(file, line, verbose, fmt, args);
	va_end( args );
}

void
StandardTestOutput::PublicOutputFormattedv( const char *file, int line, int verbose, const char *fmt, va_list args)
{
	OutputFormattedv(file, line, verbose, fmt, args);
}


OP_STATUS
StandardTestOutput::FailedReport::Add( const char *file, int line, const char *format, va_list args )
{
	if (!format)
		return OpStatus::ERR_NULL_POINTER;
	
	OpString8 buffer;
	RETURN_IF_ERROR(buffer.AppendVFormat(format, args));
	Add( file, line, buffer.CStr() );

	return OpStatus::OK;
}

OP_STATUS
StandardTestOutput::FailedReport::Add( const char *file, int line, const char *txt )
{
	const unsigned offset = report ? (insertion_point - report) : 0;
	const unsigned needed = offset + (txt ? op_strlen(txt) : 0) + (file ? 32 + op_strlen(file) : 0) + 10;

	if (!report || allocated < needed)
	{
		unsigned new_allocated = report ? allocated + allocated : 16384;
		while (new_allocated < needed)
			new_allocated += new_allocated;
		char *new_report = OP_NEWA(char, new_allocated);
		if (!new_report)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
		if (report)
		{
			op_memcpy(new_report, report, offset + 1);
			OP_DELETEA(report);
		}
		else
			new_report[0]='\0';

		report = new_report;
		insertion_point = report + offset;
		allocated = new_allocated;
	}

	if( file )
	{
		*insertion_point++ = ' ';
		op_strcpy( insertion_point, file );

		// A number is very seldom larger than 2^64, which is 20
		// characters in base 10, add a sign (for negative lines. :-))
		// and the (): with whitespace, and we get up to 25 characters.
		//
        // You need 128bit integers to overrun this buffer.
		char buffer[32];  /* ARRAY OK 2007-03-09 marcusc */
		op_sprintf( buffer, LINENUMBER_FMT, line );
		op_strcat( insertion_point, buffer );
	}

	if (txt)
		op_strcat( insertion_point, txt );
	op_strcat( insertion_point, "\n" );

	insertion_point = insertion_point + op_strlen(insertion_point);

	return OpStatus::OK;
}

const char*
StandardTestOutput::FailedReport::GetReport() const
{
	return report;
}

void
SpartanTestOutput::TestDescribe(const char* /*module_name*/, const char* group_name, const char* test_name)
{
	// Strip away absolute paths in test names.
	// This should preferably be done some other way.
	const char mypath[] = "/modules/selftest/src/testoutput.cpp";
	int baselen = sizeof(__FILE__)-sizeof(mypath); // $OPERADIR
	OP_ASSERT(op_strcmp(__FILE__ + baselen, mypath) == 0);
	(void)mypath;

	char* basestr = OP_NEWA(char, baselen + 1);
	if (!basestr)
		return;
	op_strncpy(basestr, g_selftest.utils->GetCurrentFile(), baselen);
	basestr[baselen] = 0;
	
	const char* basepath_start = op_strstr(test_name, basestr);
	if (basepath_start)
	{
		char* new_testname = OP_NEWA(char, op_strlen(test_name)-baselen+1);
		if (!new_testname)
			return;
		int pre_bp_len = (int)(basepath_start - test_name);
		op_strncpy(new_testname, test_name, pre_bp_len);	// Copy first part
		op_strncpy(new_testname + pre_bp_len,				// Copy second part
			test_name + pre_bp_len + baselen, 
			op_strlen(test_name) - pre_bp_len - baselen); 
		new_testname[op_strlen(test_name)-baselen] = 0;		// Terminate string

		OutputFormatted("%s:%s\t", group_name, new_testname);

		OP_DELETEA(new_testname);
	}
	else
		OutputFormatted("%s:%s\t", group_name, test_name);

	OP_DELETEA(basestr);
}

void
SpartanTestOutput::TestSkipped(const char* module_name, const char* group_name, const char* test_name, const char* reason)
{
	TestDescribe(module_name, group_name, test_name);
	OutputFormatted("SKIP\t# %s\n", reason);
}

void
SpartanTestOutput::Passed()
{
	OutputFormatted("PASS\n");
}

#define MSG_SZ 256
void
SpartanTestOutput::Failed(const char* reason, const char* file, int line, va_list *args)
{
	if(reason)
	{
		char msg_t[MSG_SZ] = {0}; /* ARRAY OK joaoe 2009-11-04 */
		char msg[MSG_SZ] = {0}; /* ARRAY OK joaoe 2009-11-04 */
		
		if (args)
		{
			op_snprintf(msg_t, MSG_SZ, "FAIL\t# %s(%d): %s\n", file ? file : "<no file given>", line, reason);
			op_vsnprintf(msg, MSG_SZ, msg_t, *args);
		}
		else
			op_snprintf(msg, MSG_SZ, "FAIL\t# %s(%d): %s\n", file ? file : "<no file given>", line, reason);
		
		for(unsigned k = 6; k < MSG_SZ && msg[k + 1] != 0; k++)
		{
			//need to replace newlines with whitespace, and while we're
			//at it, replace other kind of bugs chars
			if (msg[k] < 32)
			{
				msg[k] = ' ';
			}
		}
		if (msg[MSG_SZ - 3] != '\n')
		{
			//this handles really long truncated messages
			msg[MSG_SZ - 2] = '\n';
			msg[MSG_SZ - 1] = 0;
		}
		OutputFormatted(msg);
	}
	else
	{
		OutputFormatted("FAIL\n");
	}
}

void
SpartanTestOutput::FailedWithStatus(const char* reason, const char* file, int line, OP_STATUS status)
{
	char msg[MSG_SZ] = {0}; /* ARRAY OK joaoe 2009-11-04 */
	if (OpStatus::IsMemoryError(status))
	{
		op_snprintf(msg, MSG_SZ, "ERROR\t# %s(%d): %s, returned %s\n", file ? file : "<no file given>", line, reason, OpStatusToDescription(status));
		OutputFormatted(msg);
		return;
	}

	op_snprintf(msg, MSG_SZ, "%s, returned %s\n", reason, OpStatusToDescription(status));

	Failed(msg, file, line);
}

void 
SpartanTestOutput::Summary()
{
#ifdef GOGI_SELFTEST_FINISHED_NOTIFICATION
	if( g_selftest.utils->window )
	{
		Window* w = g_selftest.utils->GetWindow();
		if( w )
		{
			WindowCommander* wc = w->GetWindowCommander();
			if( wc )
			{
				wc->GetSelftestListener()->OnSelftestFinished();
			}
		}
	}
#endif // GOGI_SELFTEST_FINISHED_NOTIFICATION
}

#endif // SELFTEST
