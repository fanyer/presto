/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef SELFTEST
#include "modules/hardcore/mh/messages.h"

#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"

#include "modules/doc/frm_doc.h"
#ifdef USE_SYSTEM_ZLIB
# include <zlib.h>
#else
# include "modules/zlib/zlib.h"
#endif // USE_SYSTEM_ZLIB
#include "modules/doc/frm_doc.h"

#include "modules/windowcommander/OpWindowCommander.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/windowcommander/OpWindowCommanderManager.h"

#include "modules/selftest/src/testoutput.h"
#include "modules/selftest/src/testutils.h"
#include "modules/selftest/optestsuite.h"
#include "modules/selftest/operaselftestdispatcher.h"
#include "modules/selftest/generated/testgroups.h"

#include "modules/ecmascript/ecmascript.h"
#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/ecmascript_utils/essched.h"

#include "modules/memory/src/memory_debug.h"

#include "modules/pi/OpSystemInfo.h"
#include "modules/encodings/encoders/utf8-encoder.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/locale/locale-enum.h"

#ifdef CORE_GOGI
#include "platforms/core/coregogi.h"
#endif // CORE_GOGI

#ifdef DOM_JIL_API_SUPPORT
#include "modules/device_api/jil/JILFSMgr.h"
#endif // DOM_JIL_API_SUPORT

/* Bypass public API barrier to obtain DOM_GetHostObject and DOM_Element. */
#include "modules/dom/src/domobj.h"
#include "modules/dom/src/domcore/element.h"

#ifdef ENABLE_MEMORY_DEBUGGING
# define MEMDBG_GUARD_BEGIN	g_mem_debug->Action(MEMORY_ACTION_SET_MARKER_M3_AUTO);
# define MEMDBG_GUARD_END	g_mem_debug->Action(MEMORY_ACTION_SET_MARKER_M3_NOAUTO);
#else
# define MEMDBG_GUARD_BEGIN
# define MEMDBG_GUARD_END
#endif

// Differs from op_streq since it handles that h and/or n are NULL.
static BOOL ST_streq( const char *h, const char *n )
{
	if( !h || !n ) return h==n;
	if( h==n ) return TRUE;

	for( int i = 0; h[i]; i++ )
		if( n[i] != h[i] )
			return FALSE;

	return TRUE;
}


int TestUtils::SetVerbose( int v )
{
	int old = always_verbose;
	always_verbose = v;
	return old;
}

void TestSuitePattern::add_pattern( TestSuitePattern *p )
{
	if( !p ) return;

	if( next )
		next->add_pattern( p );
	else
		next = p;
}


TestSuitePattern::TestSuitePattern( const char *_format, const char *_format2 )
{
	format = op_strdup( _format );
	format2 = op_strdup( _format2 );
	next = NULL;
	matched = FALSE;
}

TestSuitePattern::~TestSuitePattern( )
{
	if( next )
		OP_DELETE(next);
	op_free(format);
	op_free(format2);
}

BOOL TestSuitePattern::match( const char *first, const char *second )
{
	if (!second)
	{
		if( glob( format, first ) ) { return matched = TRUE; }
	}
	else
	{
		if( glob( format, first ) && glob( format2, second ) ) { return matched = TRUE; }
	}

	// Call next pattern recursively.
	if( next ) return next->match( first,second );
	return FALSE;
}

BOOL TestSuitePattern::glob( const char *format, const char *data )
{
	return low_glob( data, op_strlen(data), format, op_strlen(format), 0, 0 );
}

BOOL TestSuitePattern::low_glob(const char *s, int sl, const char *f, int fl, int j, int i)
{
	for (; i<fl; i++)
	{
		switch ( f[i] )
		{
		case '?':
			if(j++>=sl) return FALSE;
			break;

		case '*':
			i++;
			if (i==fl) return TRUE;	/* finished */

			for (;j<sl;j++)
				if (low_glob(s,sl,f,fl,j,i))
					return TRUE;
			return FALSE;

		default:
			if(j>=sl || f[i] != s[j] )
				return FALSE;
			j++;
		}
	}
	return j==sl;
}


void TestUtils::AddPattern( const char *a, const char *b, PatternType pattern_type )
{
	switch( pattern_type )
	{
	case PATTERN_NORMAL:
		if( patterns )
			patterns->add_pattern( OP_NEW(TestSuitePattern, ( a,b )) ); // OOM safe
		else
			patterns = OP_NEW(TestSuitePattern, ( a,b )); // Ok if it's 0.
		break;

	case PATTERN_EXCLUDE:
		if( npatterns )
			npatterns->add_pattern( OP_NEW(TestSuitePattern, ( a,b )) ); // OOM safe
		else
		npatterns = OP_NEW(TestSuitePattern, ( a,b )); // Ok if it's 0.
		break;

	case PATTERN_MODULE:
		if( mpatterns )
			mpatterns->add_pattern( OP_NEW(TestSuitePattern, ( a,b )) ); // OOM safe
		else
			mpatterns = OP_NEW(TestSuitePattern, ( a,b )); // Ok if it's 0.
		break;
	}
}

OP_STATUS
TestUtils::ParsePatterns(const char* str, int &patterns_read, TestUtils::PatternType pattern_type)
{
	patterns_read = 0;

	char *op, *pp;
	op = pp = op_strdup( str );
	if(!op)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	int i = 0;
	do
	{
		int eoo = 0;
		if( pp[i] == ',' || !pp[i] )
		{
			const char *p2 = "*";
			if( pp[i] == 0 )
				eoo= 1;
			pp[i] = 0;
			for( int j = 0; j<i; j++ )
			{
				if( pp[j] == ':' )
				{
					pp[j] = 0;
					p2 = pp+j+1;
					break;
				}
			}
			g_selftest.utils->AddPattern( pp, p2, pattern_type );
			pp += i+1;
			patterns_read++;
			if( eoo || !pp[i] )
				break;
			i = 0;
		} else
			i++;
	} while(1);
	op_free(op);

	return OpStatus::OK;
}

BOOL
TestUtils::MatchPatterns(const char* module, const char* group, const char* test)
{
	BOOL match_module = FALSE;
	BOOL match_group = FALSE;

	if (module)
	{
		match_module = TRUE;
		if (!g_selftest.utils->mpatterns || !g_selftest.utils->mpatterns->match(module))
			match_module = FALSE;
	}

	if (group)
	{
		match_group = TRUE;
		if ((g_selftest.utils->patterns && !g_selftest.utils->patterns->match( group, test )) ||
			(g_selftest.utils->npatterns && g_selftest.utils->npatterns->match( group, test )) )
		{
			match_group = FALSE;
		}
	}

	return match_module || match_group;
}

void TestUtils::RunManual( bool run )
{
	run_manual_tests = run;
}

void TestUtils::AsyncStep( int delay )
{
	if( g_selftest.suite->IsOperaInitialized() )
		g_selftest.suite->PostStepMessage(delay);
}

void TestUtils::AsyncStop()
{
	if( g_selftest.suite->IsOperaInitialized() )
		g_main_message_handler->PostMessage( WM_OPERA_CLOSE_NO_ASK, 0, 0 );
}

void TestUtils::TestDescribe( const char *t )
{
	const char *module_name, *group_name;
	TestSuite::GroupInfo(-1, group_name, module_name);

	OP_ASSERT (output);
	if (output)
		output->TestDescribe(module_name, group_name, t);
}

int TestUtils::TestSet( const char *t, int manual, int test_index)
{
	OP_NEW_DBG("TestUtils::TestSet()", "selftest");
	OP_DBG(("test: %s; index: %d -> %d%s", t, last_test_index, test_index, manual?"; manual":""));
	MEMDBG_GUARD_BEGIN
	unsigned ret = 0;

	if (MatchPatterns(current_module, current_group, t))
	{
		if( manual && !run_manual_tests )
		{
			TestSkipped( t, "(manual)");
			return 0;
		}
		if (last_test_index != test_index)
		{
			TestDescribe( t );
			current_test = t;
			OP_DBG(("has test result -> FALSE"));
			has_test_result = FALSE;
			last_test_index = test_index;
		}

		ret = 1;
	}
	MEMDBG_GUARD_END
	return ret;
}


int TestUtils::GroupSet( const char *t, const char *f, int l )
{
	current_file = f;

	OP_ASSERT(patterns);
	TestSuite::GroupInfo(-1,current_group,current_module);

	if (MatchPatterns(current_module, t, NULL))
	{
		OP_ASSERT (output);
		if (output)
			output->GroupHeader(f, l, t);

		current_group = t;
		last_test_index = -1;
		return 1;
	}
	return 0;
}

int TestUtils::TestSkipped( const char *t, const char *reason, bool do_continue )
{
	const char *module_name, *group_name;
	TestSuite::GroupInfo(-1, group_name, module_name);

	OP_ASSERT(output);
	if (output)
		output->TestSkipped(module_name, group_name, t, reason);
	if( do_continue )
	{
		return 1;
	}
	return 0;
}

void testsuite_break_here()
{
	g_selftest.utils->failed_tests++;
}

const uni_char *TestUtils::MakeKey( const char *a, const char *b )
{
	int l = (a?op_strlen(a)+2:0)+op_strlen(b)+1;
	uni_char *p;
	if( l > keyspace_len )
	{
		keyspace_len = l*2;
		OP_DELETEA(keyspace);
		keyspace = OP_NEWA(uni_char, keyspace_len);
	}
	if( !keyspace ) 		//  OOM
	{
		keyspace_len = 0;
		return 0;
	}

	p = keyspace;

	if( a )
	{
		for( l=0; a[l]; l++ )
			*p++ = a[l];
		*p++='-';
		*p++='>';
	}
	for( l=0; b[l]; l++ )	*p++ = b[l];
	*p++=0;
	return keyspace;
}

void TestUtils::TestPassed( )
{
	OP_NEW_DBG("TestUtils::TestPassed()", "selftest");
	if (!g_selftest.running)
		return;
	OP_ASSERT (output);

	MEMDBG_GUARD_BEGIN
	const uni_char* key = MakeKey( current_group, current_test );
	uni_char *dd = 0;
	if (key && (dd = uni_strdup(key)) ) // !OOM
	{
		if( passed.Add( dd, dd ) != OpStatus::OK )
			op_free( dd ); // Already there, or OOM.

		if (!has_test_result && output)
		{
			OP_DBG(("PASSED"));
			output->Passed();
			if (timer)
			{
				timer = g_op_time_info->GetWallClockMS() - timer;
				output->Timer((int)timer);
				timer = 0;
			}
		}
	}
	else
	{
		if (output && !has_test_result)
			output->Failed("OOM", __FILE__, __LINE__);
		g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
	}
	OP_DBG(("has test result -> TRUE"));
	has_test_result = TRUE;
	MEMDBG_GUARD_END
}


int TestUtils::TestWasOk( const char *t )
{
	void *v;

	// 1: Check for test in current group.
	const uni_char* key1 = MakeKey( current_group, t );
	if (key1)
	{
		if( passed.GetData( key1, &v ) == OpStatus::OK && v)
			return 1;
	}
	else	// OOM
		return 0;

	// 2: Check for full name.
	const uni_char* key2 = MakeKey( 0,t );
	if (key2)
	{
		if( passed.GetData( key2, &v ) == OpStatus::OK && v)
			return 1;
	}

	return 0;
}

void TestUtils::TestFailed( const char *file, int line, const char *ttst, ... )
{
	OP_NEW_DBG("TestUtils::TestFailed()", "selftest");
	if (!g_selftest.running)
		return;
	OP_ASSERT (output);

	if (output)
	{
		va_list args;
		va_start( args, ttst );
		if (!has_test_result)
		{
			OP_DBG(("FAILED"));
			output->Failed(ttst, g_selftest.utils->GetCurrentFile(), line, &args);
		}
		va_end( args );

		has_test_result = TRUE;
		OP_DBG(("has test result -> TRUE"));

		va_start( args, ttst );
		output->TestFailed(g_selftest.utils->GetCurrentFile(), line, ttst, args);
		va_end( args );
	}

	testsuite_break_here();
}

void TestUtils::TestFailedWithStatus( const char *file, int line, const char *ttst, OP_STATUS status)
{
	OP_NEW_DBG("TestUtils::TestFailedWithStatus()", "selftest");
	if (!g_selftest.running)
		return;
	OP_ASSERT (output);

	if (output)
	{
		if (!has_test_result)
		{
			OP_DBG(("FAILED"));
			output->FailedWithStatus(ttst, g_selftest.utils->GetCurrentFile(), line, status);
		}
		has_test_result = TRUE;
		OP_DBG(("has test result -> TRUE"));
		output->TestFailedWithStatus(g_selftest.utils->GetCurrentFile(), line, ttst, status);
	}

	testsuite_break_here();
}

static void TestFailed( const uni_char *t, const uni_char *a1, const uni_char *a2,
						const uni_char *l, const uni_char *f )
{
	OP_NEW_DBG("TestFailed()", "selftest");
	char *tt  = uni_down_strdup( t );
	const char *ta1 = ST_down( a1 );
	const char *ta2 = ST_down( a2 );
	char *tf  = uni_down_strdup( f );
	char *tl  = uni_down_strdup( l );

	if(tt && ta1 && ta2 && tf && tl)
		g_selftest.utils->TestFailed( tf, (int)op_atoi(tl), tt, ta1, ta2  );
	else // OOM
	{
		OP_ASSERT (g_selftest.utils->output);
		if (g_selftest.utils->output && !g_selftest.utils->has_test_result)
		{
			OP_DBG(("FAILED"));
			g_selftest.utils->output->Failed("OOM", __FILE__, __LINE__);
		}
		g_selftest.utils->has_test_result = TRUE;
		OP_DBG(("has test result -> TRUE"));
	}

	op_free( tl );
	op_free( tf );
	op_free( tt );
}

class AsyncTestDone : public ES_AsyncCallback
{
public:
	AsyncTestDone()
	 : m_was_called(FALSE)
	 , async(FALSE)
	{}

	virtual OP_STATUS HandleCallback( ES_AsyncOperation /*op*/, ES_AsyncStatus status, const ES_Value& result)
	{
		if( status != ES_ASYNC_SUCCESS && status != ES_ASYNC_CANCELLED )
		{
			if (g_selftest.utils->IsRunningEcmascriptTest())
			{
				switch (status)
				{
				case ES_ASYNC_NO_MEMORY:
					g_selftest.utils->TestFailedWithStatus("-", 0, NULL, OpStatus::ERR_NO_MEMORY);
					g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
					break;
				case ES_ASYNC_EXCEPTION:
					HandleException(result, NULL, g_selftest.utils->doc->GetESRuntime());
					return OpStatus::OK;
				default:
					g_selftest.utils->TestFailed("-", 0, "Error information not available");
				}
			}

			g_selftest.utils->SetEcmascriptTestDone();
			g_selftest.utils->AsyncStep( );
			return OpStatus::OK;
		}

		if (!async)
		{
			g_selftest.utils->SetEcmascriptTestDone();
			g_selftest.utils->AsyncStep( );
		}

		return OpStatus::OK;
	}

	void HandleException(const ES_Value& result, ES_Thread *thread, ES_Runtime *runtime)
	{
		ES_Value v;
		const uni_char *message = NULL;
		int line_number = -1;

		if (result.type == VALUE_OBJECT)
		{
			if (runtime->GetName(result.value.object, UNI_L("message"),&v) == OpBoolean::IS_TRUE)
				message = GetAsString(v);

			ES_Runtime::CallerInformation call_info;
			if (thread != NULL && OpStatus::IsSuccess(runtime->GetErrorStackCallerInformation(thread->GetContext(), result.value.object, 0, TRUE, call_info)))
				line_number = call_info.line_no;
		}
		else
			message = GetAsString(result);

		enum { STR_BUF_SIZE = 1<<9 };
		char message_8[STR_BUF_SIZE]; // ARRAY OK 2012-03-30 joaoe

		int dummy;
		UTF16toUTF8Converter decoder;
		if (message)
		{
			decoder.Convert(message, UNICODE_SIZE(uni_strlen(message) + 1), message_8, STR_BUF_SIZE - 1, &dummy);
			message_8[STR_BUF_SIZE - 1] = 0;
		}

		g_selftest.utils->SetEcmascriptTestDone();
		g_selftest.utils->TestFailed("-", line_number, message ? "exception: %s" : "exception thrown", message_8);
		g_selftest.utils->AsyncStep( );
	}

	const uni_char* GetAsString(const ES_Value& value)
	{
		switch(value.type)
		{
		case VALUE_STRING:
			return value.value.string;
		case VALUE_STRING_WITH_LENGTH:
			return value.value.string_with_length->string;
		case VALUE_NULL:
			return UNI_L("null");
		case VALUE_UNDEFINED:
			return UNI_L("undefined");
		case VALUE_NUMBER:
		{
			uni_snprintf(m_number, 63, UNI_L("%.16g"), value.value.number);
			m_number[63] = 0;
			return m_number;
		}
		case VALUE_BOOLEAN:
			return value.value.boolean ? UNI_L("true") : UNI_L("false");
		}
		return NULL;
	}

	void SetAsync(BOOL value) { async = value; }

	BOOL m_was_called;

private:
	uni_char m_number[64]; /* ARRAY OK joaoe 2010-01-20 */
	BOOL async;
};


class SelftestSubThreadListener : public ES_ThreadListener
{
public:
	SelftestSubThreadListener() {}

	/**
	 * To be used for other es threads traced by TS_CheckSelfTest
	 */
	virtual OP_STATUS Signal(ES_Thread *thread, ES_ThreadSignal signal)
	{
		//delete when completed
		ANCHOR_PTR(SelftestSubThreadListener, this);

		void **dummy;
		g_selftest.utils->threads_listened.Remove(thread, &dummy);

		if (!g_selftest.utils->IsRunningEcmascriptTest())
			return OpStatus::OK;

		ES_Value v;
		switch (signal)
		{
		case ES_SIGNAL_FAILED:
			if (thread->GetScheduler()->GetLastError() == OpStatus::ERR_NO_MEMORY)
				g_selftest.utils->async_test_done->HandleCallback(ES_ASYNC_LAST_OPERATION, ES_ASYNC_NO_MEMORY, v);
			else if (!g_selftest.utils->allow_exceptions)
			{
				if (thread->ReturnedValue())
				{
					OpStatus::Ignore(thread->GetReturnedValue(&v));
					g_selftest.utils->async_test_done->HandleException(v, thread, thread->GetScheduler()->GetRuntime());
				}
				else
					g_selftest.utils->async_test_done->HandleCallback(ES_ASYNC_LAST_OPERATION, ES_ASYNC_FAILURE, v);
			}
			break;
		}
		return OpStatus::OK;
	}
};

static void do_delete_a( const void* /*a*/, void *b )
{
	op_free(b);
}

void TestUtils::Summary()
{
	OP_ASSERT (output);
	if (output)
		output->Summary();

	passed.ForEach( &do_delete_a );
	passed.RemoveAll();
}

class TS_Failed : public EcmaScript_Object
{
public:
	TS_Failed(int async = 0)
	 : async(async)
	{}

	virtual int Call( ES_Object* /*this_object*/, ES_Value* argv, int argc,
			  ES_Value* return_value, ES_Runtime* origining_runtime)
	{
		// why [arg1] [arg2] file line
		if( argc < 3 )
			return ES_FAILED;

		if (!g_selftest.utils->IsRunningEcmascriptTest())
		{
			ES_ThreadScheduler *sched = origining_runtime->GetESScheduler();
			sched->CancelThread(sched->GetCurrentThread());
			return ES_ABORT;
		}
		switch( argc )
		{
			case 3:
				TestFailed( UNI_L("%s"), argv[0].value.string, UNI_L(""),
							argv[2].value.string, argv[1].value.string );
				break;
			case 4:
				TestFailed( argv[0].value.string, argv[1].value.string, UNI_L(""),
							argv[2].value.string, argv[3].value.string );
				break;
			case 5:
				TestFailed( argv[0].value.string, argv[1].value.string, argv[2].value.string,
							argv[3].value.string, argv[4].value.string );
				break;
			default:
				return ES_FAILED;
		}
		return_value->type = VALUE_NUMBER;
		return_value->value.number = 0;
		g_selftest.utils->SetEcmascriptTestDone();
		if (async)
			g_selftest.utils->AsyncStep( );

		return ES_VALUE;
	}
private:
	int async;
};

class TS_Passed : public EcmaScript_Object
{
public:
	TS_Passed(int async = 0)
	 : async(async)
	{}

	virtual int Call( ES_Object* /*this_object*/, ES_Value* /*argv*/, int /*argc*/,
			  ES_Value* return_value, ES_Runtime* origining_runtime)
	{
		if (!g_selftest.utils->IsRunningEcmascriptTest())
		{
			ES_ThreadScheduler *sched = origining_runtime->GetESScheduler();
			sched->CancelThread(sched->GetCurrentThread());
			return ES_ABORT;
		}
		g_selftest.utils->TestPassed( );
		g_selftest.utils->SetEcmascriptTestDone();
		if (async)
			g_selftest.utils->AsyncStep( );

		return_value->type = VALUE_NUMBER;
		return_value->value.number = 0;
		return ES_VALUE;
	}

private:
	int async;
};

class TS_Continue : public EcmaScript_Object
{
	virtual int Call( ES_Object* /*this_object*/, ES_Value* /*argv*/, int /*argc*/,
			  ES_Value* return_value, ES_Runtime* /*origining_runtime*/ )
	{
		g_selftest.utils->AsyncStep( 1 );

		return_value->type = VALUE_NUMBER;
		return_value->value.number = 0;
		return ES_VALUE;
	}
};

class TS_Output : public EcmaScript_Object
{
	virtual int Call( ES_Object* /*this_object*/, ES_Value* argv, int argc,
			  ES_Value* return_value, ES_Runtime* /*origining_runtime*/ )
	{
		enum { STR_BUF_SIZE = 1024 };
		char string[STR_BUF_SIZE]; /* ARRAY OK joaoe 2010-06-19 */
		for(int k = 0; k < argc; k++)
		{
			OP_ASSERT(argv[k].type == VALUE_STRING);
			if (argv[k].type == VALUE_STRING && *argv[k].value.string)
			{
				int dummy;
				UTF16toUTF8Converter decoder;
				decoder.Convert(argv[k].value.string, UNICODE_SIZE(uni_strlen(argv[k].value.string) + 1),
						string, STR_BUF_SIZE - 1, &dummy);
				output("%s", string);
			}
		}
		return ES_FAILED;
	}
};

class TS_CheckSelfTest : public EcmaScript_Object
{
public:
	TS_CheckSelfTest(int _async = 0)
	 : async(_async)
	{}
	virtual int Call( ES_Object* /*this_object*/, ES_Value* argv, int argc,
			  ES_Value* return_value, ES_Runtime* origining_runtime)
	{
		if (argc == -1)
			//restart -> proceed !
			return ES_FAILED;

		if (argc < 2 || argv[0].type != VALUE_STRING || argv[1].type != VALUE_STRING)
		{
			return_value->type = VALUE_STRING;
			return_value->value.string = UNI_L("WRONG_ARGUMENTS_ERR");
			return ES_EXCEPTION;
		}

		ES_Thread* thread = origining_runtime->GetESScheduler()->GetCurrentThread();

		if (g_selftest.utils->threads_listened.Contains(thread))
			return ES_FAILED;

		BOOL is_test_ok =
			g_selftest.utils->IsRunningEcmascriptTest() &&
			uni_str_eq(g_selftest.utils->GetCurrentGroupName(), argv[0].value.string) &&
			uni_str_eq(g_selftest.utils->GetCurrentTestName(), argv[1].value.string);

		if (!is_test_ok)
		{
			//the es thread that called this function is not
			//part of the current running selftest so we abort it
			origining_runtime->GetESScheduler()->CancelThread(thread);
			return ES_ABORT;
		}

		SelftestSubThreadListener * listener = OP_NEW(SelftestSubThreadListener,());

		if (listener == NULL || OpStatus::IsMemoryError(g_selftest.utils->threads_listened.Add(thread,NULL)))
		{
			OP_DELETE(listener);
			g_selftest.utils->SetEcmascriptTestDone();
			g_selftest.utils->TestFailedWithStatus(__FILE__, __LINE__, NULL, OpStatus::ERR_NO_MEMORY);
			if (async)
				g_selftest.utils->AsyncStep( );
			return ES_NO_MEMORY;
		}

		thread->AddListener(listener);
		if (!g_selftest.utils->allow_exceptions)
			thread->SetWantExceptions();

		return ES_SUSPEND | ES_RESTART;
	}
	int async;
};

class TS_BinaryCompareFiles : public EcmaScript_Object
{
	virtual int Call(ES_Object* /*this_object*/, ES_Value* argv, int argc,
					 ES_Value* return_value, ES_Runtime* /*origining_runtime*/)
	{
		if (argc < 2 ||
			argv[0].type != VALUE_STRING ||
			argv[1].type != VALUE_STRING)
			return ES_FAILED;

		const uni_char* reference_filename = argv[0].value.string;
		const uni_char* tested_filename = argv[1].value.string;

		FileComparisonTest comparator(reference_filename, tested_filename);
		OP_BOOLEAN result = comparator.CompareBinary();

		// TODO: handling of comparison errors should be added.
		return_value->type = VALUE_BOOLEAN;
		return_value->value.boolean = (result == OpBoolean::IS_TRUE);

		return ES_VALUE;
	}
};

#ifdef DOM_JIL_API_SUPPORT
class TS_JILToSystemPath : public EcmaScript_Object
{
	virtual int Call(ES_Object* /*this_object*/, ES_Value* argv, int argc,
					 ES_Value* return_value, ES_Runtime* /*origining_runtime*/)
	{
		if (argc < 1 || argv[0].type != VALUE_STRING)
			return ES_FAILED;

		system_path_result.Empty();
		OP_STATUS conversion_status = g_DAPI_jil_fs_mgr->JILToSystemPath(argv[0].value.string, system_path_result);
		if (conversion_status == OpStatus::ERR_FILE_NOT_FOUND)
			// I don't usually like returning error messages as values but in case of tests it should be easily noticeable.
			system_path_result.AppendFormat(UNI_L("cannot convert JIL path '%s' to system path"), argv[0].value.string);
		else
			CALL_FAILED_IF_ERROR(conversion_status);

		return_value->type = VALUE_STRING;
		return_value->value.string = system_path_result.CStr();

		return ES_VALUE;
	}

private:
	OpString system_path_result;
};
#endif // DOM_JIL_API_SUPPORT

/**
 * Paint current document to a bitmap. Handy for tests relying on
 * events triggered by painting.
 */
class TS_Paint : public EcmaScript_Object
{
	virtual int Call( ES_Object* /*this_object*/, ES_Value* argv, int argc,
			ES_Value* return_value, ES_Runtime* origining_runtime )
	{
		FramesDocument* doc = origining_runtime->GetFramesDocument();
		VisualDevice* vis_dev = doc->GetVisualDevice();
		OpRect clip_rect(0, 0, doc->GetVisualViewWidth(), doc->GetVisualViewHeight());
		OpBitmap* bitmap_p = 0;

		OP_STATUS status = OpBitmap::Create(&bitmap_p, clip_rect.width, clip_rect.height, FALSE, FALSE, 0, 0, TRUE);
		if (OpStatus::IsMemoryError(status))
			return ES_NO_MEMORY;
		else if (OpStatus::IsError(status))
		{
			return_value->type = VALUE_STRING;
			return_value->value.string = UNI_L("ST_Paint: Failed to create bitmap.");
			return ES_EXCEPTION;
		}

		OpAutoPtr<OpBitmap> bitmap(bitmap_p);
		OpPainter* painter = bitmap->GetPainter();
		if (!painter)
			return ES_NO_MEMORY;

		painter->SetColor(0, 0, 0, 0);
		painter->ClearRect(clip_rect);
		vis_dev->GetContainerView()->Paint(clip_rect, painter, 0, 0, TRUE, FALSE);
		bitmap->ReleasePainter();

		return ES_FAILED;
	}
};

/**
 * Inject mouse events into the current document.
 *
 * @param x X coordinate, in document coordinates.
 * @param y Y coordinate, in document coordinates.
 * @param type "mousedown", "mouseup" or "mousemove".
 * @param button 0 = No button, 1 = Left button, 2 = Right button, 3 = Middle button.
 */
class TS_MouseEvent : public EcmaScript_Object
{
	virtual int Call(ES_Object* /*this_object*/, ES_Value* argv, int argc,
					 ES_Value* return_value, ES_Runtime* origining_runtime)
	{
		if (argc < 4 ||
			argv[0].type != VALUE_NUMBER || argv[1].type != VALUE_NUMBER ||
			argv[2].type != VALUE_STRING || argv[3].type != VALUE_NUMBER)
		{
			return_value->type = VALUE_STRING;
			return_value->value.string = UNI_L("WRONG_ARGUMENTS_ERR");
			return ES_EXCEPTION;
		}

		FramesDocument* doc = origining_runtime->GetFramesDocument();
		int x = static_cast<int>(argv[0].value.number);
		int y = static_cast<int>(argv[1].value.number);
		const uni_char* type = argv[2].value.string;
		int button = static_cast<int>(argv[3].value.number);

		if (uni_str_eq(type, "mousedown"))
		{
			if (button >= 1 && button <= 3)
				doc->MouseDown(x, y, FALSE, FALSE, FALSE, static_cast<MouseButton>(MOUSE_BUTTON_1 - 1 + button), 1);
		}
		else if (uni_str_eq(argv[2].value.string, "mouseup"))
		{
			if (button >= 1 && button <= 3)
				doc->MouseUp(x, y, FALSE, FALSE, FALSE, static_cast<MouseButton>(MOUSE_BUTTON_1 - 1 + button), 1);
		}
		else /* mousemove */
			doc->MouseMove(x, y, FALSE, FALSE, FALSE, button == 1, button == 3, button == 2);

		return ES_FAILED;
	}
};

/**
 * Inject keyboard events into the current document.
 *
 * @param target HTML element to receive event.
 * @param type "keydown" or "keyup"
 * @param key Key code.
 */
class TS_KeyEvent : public EcmaScript_Object
{
	virtual int Call(ES_Object* /*this_object*/, ES_Value* argv, int argc,
					 ES_Value* return_value, ES_Runtime* origining_runtime)
	{
		if (argc < 4 || argv[0].type != VALUE_OBJECT || argv[1].type != VALUE_STRING
			|| argv[2].type != VALUE_NUMBER || argv[3].type != VALUE_NUMBER)
		{
			return_value->type = VALUE_STRING;
			return_value->value.string = UNI_L("WRONG_ARGUMENTS_ERR");
			return ES_EXCEPTION;
		}

		DOM_Object* target = DOM_GetHostObject(argv[0].value.object);

		if (!target || !target->IsA(DOM_TYPE_HTML_ELEMENT))
		{
			return_value->type = VALUE_STRING;
			return_value->value.string = UNI_L("ST_KeyEvent expects HTML element as first parameter.");
			return ES_EXCEPTION;
		}

		FramesDocument* doc = origining_runtime->GetFramesDocument();
		HTML_Element* element = static_cast<DOM_Element*>(target)->GetThisElement();
		const uni_char* type = argv[1].value.string;
		OpKey::Code key = static_cast<OpKey::Code>(static_cast<unsigned>(argv[2].value.number));
		uni_char key_value[2];
		key_value[0] = static_cast<uni_char>(argv[2].value.number);
		key_value[1] = 0;
		ShiftKeyState modifiers = static_cast<ShiftKeyState>(argv[3].value.number);
		OP_BOOLEAN ret;

		if (uni_str_eq(type, "keydown"))
			ret = FramesDocument::SendDocumentKeyEvent(doc, element, ONKEYDOWN, key, key_value, NULL, modifiers, FALSE, OpKey::LOCATION_STANDARD);
		else
			ret = FramesDocument::SendDocumentKeyEvent(doc, element, ONKEYUP, key, key_value, NULL, modifiers, FALSE, OpKey::LOCATION_STANDARD);

		if (ret == OpStatus::ERR_NO_MEMORY)
			return ES_NO_MEMORY;

		return ES_FAILED;
	}
};

int TestUtils::LoadNewDocument(const char *url_str, const char *data, const char *type, unsigned data_line_number)
{
	const int error_rv = -1;

	GetWindow();
	if (!window)
	{
		AsyncStep();
		return error_rv;
	}

	// If the url is specified, then data must be too, else the URL data storage
	// will be empty, so when opening the URL, there will be network accesses,
	// which will clear the KIsGeneratedBySelfTests flag and produce unpredictable
	// results.
	OP_ASSERT(!(url_str && *url_str) || (data && *data));

	URL url = g_url_api->GetURL(url_str && *url_str ? url_str : "opera:blanker");
	if (url.IsEmpty())
		return error_rv; // OOM
	g_url_api->MakeUnique(url);
	url.Unload();
	RETURN_VALUE_IF_ERROR(url.SetAttribute(URL::KIsGeneratedBySelfTests, TRUE), error_rv);

	if (type && *type)
	{
		RETURN_VALUE_IF_ERROR(url.SetAttribute(URL::KMIME_ForceContentType, type), error_rv);
		RETURN_VALUE_IF_ERROR(url.SetAttribute(URL::KHTTP_ContentType, type), error_rv);
	}

	if (data && *data)
	{
		RETURN_VALUE_IF_ERROR(url.SetAttribute(URL::KCachePolicy_NoStore, TRUE), error_rv);
		RETURN_VALUE_IF_ERROR(url.WriteDocumentData(URL::KNormal, data), error_rv);
		url.WriteDocumentDataFinished();
		g_selftest.doc_start_line_number = data_line_number;
	}
	else
		g_selftest.doc_start_line_number = 0;

	if (OpStatus::IsError(window->OpenURL(url, DocumentReferrer(url), FALSE, FALSE, -1, TRUE, FALSE, WasEnteredByUser, FALSE)))
		return error_rv;
	return 1;
}

Window *TestUtils::GetWindow()
{
	if (!g_windowManager)
	{
		OP_ASSERT(!"g_windowManager is NULL. Cannot get window.");
		return NULL;
	}

	if (!window)
	{
		// Always return a new window for the initial window. Trying to reuse
		// an existing window here is begging for trouble, as there are many
		// weird windows and window states out there.
		return (window = CreateNewWindow());
	}

	// See if the window we used previously still exists
	for (Window *win_iter = g_windowManager->FirstWindow();
		 win_iter;
		 win_iter = win_iter->Suc())
	{
		// This is really horrible! We have no idea if the window is still in a
		// good state (we could look for known bad things, but it's hard to be
		// sure). This is an artifact of the crappy GetWindow() API.
		// /Ulf
		if (win_iter == window)
		{
			// The window is still in the window manager list at least.
			// Perform some basic checks for bad state.
			if (!window->DocManager()->GetCurrentDoc() || window->IsAboutToClose())
				break;

			return window;
		}
	}

	// We didn't find the window we used last, so it must have closed. Create
	// and return a new window.
	return (window = CreateNewWindow());
}

Window *TestUtils::CreateNewWindow()
{
	if (!g_windowManager)
	{
		OP_ASSERT(!"g_windowManager is NULL. Cannot create window.");
		return NULL;
	}

	BOOL3 new_window = YES, background = NO;
	Window *window = g_windowManager->GetAWindow(FALSE, new_window, background, 200, 200,
		FALSE, MAYBE, MAYBE, NULL, NULL, FALSE, g_windowManager->FirstWindow());

	if (window)
		window->DocManager()->CreateInitialEmptyDocument(FALSE, FALSE, TRUE);
	else
		OP_ASSERT(!"Failed to create window using GetAWindow().");

	return window;
}

static HTML_Element *rec_find_element( HTML_Element *cur, const uni_char *elm, int *nelm )
{
	const uni_char *cn = cur->Type() != HE_DOC_ROOT ? cur->GetTagName() : UNI_L("html");
	if( !elm )
	{
		if ( !cn && !--*nelm )
			return cur;
	}
	else if( cn && !uni_stricmp( cn, elm ) && !--*nelm )
		return cur;
	for( HTML_Element* he = cur->FirstChild(); he; he = he->Suc() )
	{
		HTML_Element *res = rec_find_element( he, elm, nelm );
		if( res )
			return res;
	}
	return NULL;
}


HTML_Element *find_element( const uni_char *elem, int nelm )
{
	int _nelmc = nelm;
	if( !g_selftest.utils->doc || !g_selftest.utils->doc->GetDocRoot() )
	{
		return NULL;
	}
	return rec_find_element( g_selftest.utils->doc->GetDocRoot(), elem, &_nelmc );
}

HTML_Element *find_element_id( const uni_char *id)
{
	if( !g_selftest.utils->doc || !g_selftest.utils->doc->GetDocRoot() )
	{
		return NULL;
	}
	if( LogicalDocument *logdoc = g_selftest.utils->doc->GetLogicalDocument())
	{
		NamedElementsIterator iterator;

		int found = logdoc->SearchNamedElements(iterator, g_selftest.utils->doc->GetDocRoot(), id, TRUE, FALSE);

		if (found == -1)
			return 0;
		else if (found > 0)
			return iterator.GetNextElement();
	}
	return 0;
}

HTML_Element *find_element( const char *elem, int n )
{
	uni_char *x = uni_up_strdup( elem );
	if (x)
	{
		HTML_Element *res;
		res=find_element( x, n );
		op_free(x);
		return res;
	}

	OUTPUT_STR("Warning: find_element failed due to OOM\n");
	return 0;
}

HTML_Element *find_element_id( const char *id )
{
	uni_char *x = uni_up_strdup( id );
	if (x)
	{
		HTML_Element *res;
		res=find_element_id( x );
		op_free(x);
		return res;
	}
	OUTPUT_STR("Warning: find_element_id failed due to OOM\n");
	return 0;
}

class AskUserOkReply : public OpDocumentListener::DialogCallback
{
public:
	OP_STATUS Construct(const char* file, int line)
	{
		RETURN_IF_ERROR(m_file.Set(file));
		m_line = line;
		return OpStatus::OK;
	}

	virtual void OnDialogReply(Reply reply)
	{
		if (reply == REPLY_YES)
			g_selftest.utils->TestPassed();
		else
			g_selftest.utils->TestFailed( m_file.CStr(), m_line, "%s",  "User indicated that the test failed\n" );

		g_selftest.utils->AsyncStep();
		OP_DELETE(this);
	}
private:
	OpString8	m_file;
	int			m_line;
};

void TestUtils::AskUserOk(const char *file, int line, const uni_char *question)
{
	Window* window = GetWindow();

	if( !window )
	{
		TestFailed(file, line, "%s", "No operawindow available (?)\n");
		AsyncStep();
		return;
	}

	AskUserOkReply *callback = OP_NEW(AskUserOkReply, ());
	if (!callback || OpStatus::IsError(callback->Construct(file, line)))
	{
		TestFailed(file, line, "%s", "Failed: OOM\n");
		AsyncStep();
		return;
	}

	// Display confirm dialog
	window->GetWindowCommander()->GetDocumentListener()->OnConfirm(window->GetWindowCommander(), question, callback);
}

BOOL TestUtils::DocumentLoaded()
{
	MEMDBG_GUARD_BEGIN
	GetWindow();
	FramesDocument *frames_doc = NULL;
	DocumentManager *doc_man = window ? window->DocManager( ) : NULL;
	if( doc_man )
		frames_doc = doc_man->GetCurrentDoc( );

	BOOL doc_man_is_stable = frames_doc && (doc_man->GetLoadStatus() == NOT_LOADING || doc_man->GetLoadStatus() == DOC_CREATED);
	if (!frames_doc ||
		!frames_doc->IsLoaded(TRUE) ||
		!doc_man_is_stable ||
		OpStatus::IsError(frames_doc->ConstructDOMEnvironment()) ||
		frames_doc->GetDOMEnvironment() == NULL ||
		frames_doc->GetESScheduler()->IsDraining() ||
		(frames_doc->GetURL().GetAttribute(URL::KIsGeneratedBySelfTests) &&
			(!frames_doc->HasCalledOnLoad() || // need to wait for onload handlers to be called
			frames_doc->IsESActive(TRUE)))) // onload scripts executing ?
	{
		AsyncStep( 5 );
		MEMDBG_GUARD_END
		return FALSE;
	}

	MEMDBG_GUARD_END
	return TRUE;
}

int TestUtils::TestEcma( BOOL async, BOOL fails, BOOL _allow_exceptions, unsigned line_number, const uni_char *ecmascript)
{
	OP_NEW_DBG("TestUtils::TestEcma()", "selftest");
	GetWindow();
	FramesDocument *frames_doc = NULL;
	DocumentManager *doc_man = window ? window->DocManager( ) : NULL;
	if( doc_man )
		frames_doc = doc_man->GetCurrentDoc( );

	if( !DocumentLoaded() )
	{
		// Let's try again later.
		if( always_verbose && delay_counter++ > 200 ) {
			delay_counter = 0;
			if( frames_doc )
				OUTPUT_STR("Delayed (waiting for loading to finish).\n");
			else
				OUTPUT_STR("Delayed (no doc).\n");
		}
		return -1;
	}
	delay_counter = 150;

	ES_Runtime *runtime = frames_doc->GetESRuntime();
	OP_ASSERT(runtime);

	TS_Passed *ts_passed = OP_NEW(TS_Passed, (async));
	TS_Failed *ts_failed = OP_NEW(TS_Failed, (async));
	TS_Continue *ts_continue= OP_NEW(TS_Continue, ());
	TS_Output *ts_output= OP_NEW(TS_Output, ());
	TS_CheckSelfTest *ts_chk_st= OP_NEW(TS_CheckSelfTest, (async));
	TS_BinaryCompareFiles *ts_binary_compare_files = OP_NEW(TS_BinaryCompareFiles, ());
#ifdef DOM_JIL_API_SUPPORT
	TS_JILToSystemPath *ts_jil_to_system_path = OP_NEW(TS_JILToSystemPath, ());
#endif // DOM_JIL_API_SUPPORT
	TS_Paint *ts_paint = OP_NEW(TS_Paint, ());
	TS_MouseEvent *ts_mouse_event = OP_NEW(TS_MouseEvent, ());
	TS_KeyEvent *ts_key_event = OP_NEW(TS_KeyEvent, ());

	if(!(ts_passed && ts_failed && ts_continue && ts_output && ts_chk_st && ts_binary_compare_files && ts_paint
				&& ts_mouse_event && ts_key_event))
	{
		OP_DELETE(ts_passed);
		OP_DELETE(ts_failed);
		OP_DELETE(ts_continue);
		OP_DELETE(ts_output);
		OP_DELETE(ts_chk_st);
		OP_DELETE(ts_binary_compare_files);
		OP_DELETE(ts_paint);
		OP_DELETE(ts_mouse_event);
		OP_DELETE(ts_key_event);
		if (!has_test_result)
		{
			OP_DBG(("FAILED"));
			output->Failed("OOM", __FILE__, __LINE__);
		}
		has_test_result = TRUE;
		OP_DBG(("has test result -> TRUE"));
		return 1;
	}

	ts_failed->SetFunctionRuntimeL( runtime, UNI_L("failed"), NULL, "sssss");
	ts_passed->SetFunctionRuntimeL( runtime, UNI_L("passed"), NULL, NULL);
	ts_continue->SetFunctionRuntimeL( runtime, UNI_L("continue"), NULL, NULL);
	ts_output->SetFunctionRuntimeL( runtime, UNI_L("output"), NULL, "s");
	ts_chk_st->SetFunctionRuntimeL( runtime, UNI_L("$check_selftest$"), NULL, "s");
	ts_binary_compare_files->SetFunctionRuntimeL( runtime, UNI_L("binary_compare_files"), NULL, "ss-");
#ifdef DOM_JIL_API_SUPPORT
	ts_jil_to_system_path->SetFunctionRuntimeL( runtime, UNI_L("jil_to_system_path"), NULL, "s-");
#endif // DOM_JIL_API_SUPPORT
	ts_paint->SetFunctionRuntimeL( runtime, UNI_L("paint"), NULL, NULL );
	ts_mouse_event->SetFunctionRuntimeL( runtime, UNI_L("mouse_event"), NULL, NULL );
	ts_key_event->SetFunctionRuntimeL( runtime, UNI_L("key_event"), NULL, NULL );

	runtime->PutInGlobalObject( ts_failed,	UNI_L("ST_failed") );
	runtime->PutInGlobalObject( ts_passed,	UNI_L("ST_passed") );
	runtime->PutInGlobalObject( ts_continue,UNI_L("ST_continue") );
	runtime->PutInGlobalObject( ts_output,	UNI_L("output") );
	runtime->PutInGlobalObject( ts_chk_st,	UNI_L("$check_selftest$") );
	runtime->PutInGlobalObject( ts_binary_compare_files, UNI_L("ST_binary_compare_files") );
#ifdef DOM_JIL_API_SUPPORT
	runtime->PutInGlobalObject( ts_jil_to_system_path, UNI_L("ST_jil_to_system_path") );
#endif // DOM_JIL_API_SUPPORT
	runtime->PutInGlobalObject( ts_paint, UNI_L("ST_paint") );
	runtime->PutInGlobalObject( ts_mouse_event, UNI_L("ST_mouse_event") );
	runtime->PutInGlobalObject( ts_key_event, UNI_L("ST_key_event") );

	ES_ProgramText program;
	program.program_text = ecmascript;
	program.program_text_length = uni_strlen(ecmascript);

	if( !async_test_done )
	{
		async_test_done = OP_NEW(AsyncTestDone, ());
		if( !async_test_done )
		{
			if (!has_test_result)
			{
				OP_DBG(("FAILED"));
				output->Failed("OOM", __FILE__, __LINE__);
			}
			has_test_result = TRUE;
			OP_DBG(("has test result -> TRUE"));
			return 1;
		}
	}
	async_test_done->SetAsync(async);
	async_test_done->m_was_called = FALSE;
	expect_fail = fails;
	allow_exceptions = _allow_exceptions;
	running_async = async;

	frames_doc->GetESAsyncInterface()->SetStartLine(line_number);

	OP_STATUS status = frames_doc->GetESAsyncInterface()->Eval(&program, 1, NULL, 0, async_test_done, NULL );

	if (OpStatus::IsMemoryError(status))
	{
		if (!has_test_result)
		{
			OP_DBG(("FAILED"));
			output->Failed("OOM", __FILE__, __LINE__);
		}
		OP_DBG(("has test result -> TRUE"));
		has_test_result = TRUE;
		return 1;
	}

	if (status == OpStatus::ERR)
	{
		TestFailed(__FILE__, __LINE__, "%s", "Compilation failed");
		running_async = FALSE;
		failed_tests++;
		OUTPUT_STR("FAILED (compilation error, or failure to start program)\n");
		OUTPUT_STR("Failed program was:\n");
		char *x = uni_down_strdup( program.program_text );
		if (x) // OOM check
		{
			OUTPUT_STR(x);
			op_free( x );
		}
		OUTPUT_STR("\n");
		return 1;
	}
	OP_ASSERT(OpStatus::IsSuccess(status));

	return -2;
}

#ifdef QUICK
#include "adjunct/quick/Application.h"
#endif

char*
TestUtils::GetTempBuf()
{
	char* buf = tmpbuffs[buff];

	if( ++buff > ST_NUM_TEMP_BUFFERS - 1 )
		buff = 0;

	return buf;
}

char*
TestUtils::GetPathBuf()
{
	char* buf = pathbuff;
	return buf;
}

void
TestUtils::memSetMarker()
{
#ifdef ENABLE_MEMORY_DEBUGGING
	g_mem_debug->Action(MEMORY_ACTION_SET_MARKER_M3_ALL);
#endif
}

void
TestUtils::memCheckForLeaks()
{
#ifdef ENABLE_MEMORY_DEBUGGING
	unsigned leaks = g_mem_debug->Action(MEMORY_ACTION_COUNT_NOT_M3);
	if (leaks)
		output->Leak(leaks);
#endif
}

void
TestUtils::timerStart()
{
	timer = g_op_time_info->GetWallClockMS();
}

void
TestUtils::timerReset()
{
	timer = 0;
}

void
TestUtils::WarnAboutNoMatches() const
{
	TestSuitePattern* p = mpatterns;
	while(p)
	{
		if(!p->did_match())
		{
			if( p->group_pattern() && op_strcmp(p->group_pattern(),"*"))
				g_selftest.utils->output->Warning(
					"Warning: Pattern '%s:%s' did not match any tests\n",p->module_pattern(),p->group_pattern());
			else
				g_selftest.utils->output->Warning(
					"Warning: Pattern '%s' did not match any tests\n",p->module_pattern());

			BOOL existing_module = FALSE;
			for( int i=0; i<g_selftest.groups->allgroups; ++i )
			{
				if( op_strcmp(p->module_pattern(), g_selftest.groups->g[i].module) == 0 )
				{
					existing_module = TRUE;
					break;
				}
			}

			if( !existing_module )
				g_selftest.utils->output->Warning(
					"Warning: There is no module named '%s'\n",p->module_pattern());

		}
		p = p->get_next();
	}
}

BOOL
TestUtils::IsRunningEcmascriptTest() const
{
	return g_selftest.running && async_test_done != NULL && !async_test_done->m_was_called;
}

void
TestUtils::SetEcmascriptTestDone()
{
	if (async_test_done)
		async_test_done->m_was_called = TRUE;
	running_async = FALSE;
}

static char hex( int j )
{
	if( j > 9 )	return j+'a'-10;
	return j+'0';
}

const char *ST_trim_fn( const char *x )
{
	int x_len = op_strlen(x);

	const char* current_file = g_selftest.utils->GetCurrentFile();
	OpString folder_path;
	if (OpStatus::IsError(g_folder_manager->GetFolderPath(OPFILE_SELFTEST_DATA_FOLDER, folder_path)))
		return x;
	int folder_path_len = folder_path.Length();

	if (folder_path_len > x_len)
		return x;

	int separator = op_strlen(current_file);
	while(separator >= 0 && current_file[separator] != '/')
		--separator;

	const char* dd = x + folder_path_len;
	for (int i = 0; i < separator; i++)
		if (! (dd[i] == current_file[i] || current_file[i] == '/'))
			return x;

	return x + separator + folder_path_len + 1;
}

const uni_char *ST_up( const char *x )
{
	uni_char *p = (uni_char *)g_selftest.utils->GetTempBuf();
	int i;
	for( i=0; x[i]&&i<300; i++ )
		p[i]=x[i];
	p[i]=0;

	return p;
}

const char *ST_down( const uni_char *x )
{
	int d = 0;
	char *p = g_selftest.utils->GetTempBuf();

	if( x )
	{
		int i;
		for( i=0; x[i] && d<999; i++,d++ )
		{
			if( /*(uni_isalnum(x[i]) && !uni_isspace(x[i])) ||*/ x[i] > 256  )
			{
				p[d++] = '\\';
				p[d++] = 'x';
				if( x[i] > (1<<12) ) p[d++] = hex( ((x[i]>>12)&15) );
				if( x[i] > (1<<8) )  p[d++] = hex( ((x[i]>>8)&15) );
				if( x[i] > (1<<4) )  p[d++] = hex( ((x[i]>>4)&15) );
				p[d] = hex( (x[i]&15) );
				if( (x[i+1] >= '0' && x[i+1] <= '9') ||	(x[i+1] >= 'a' && x[i+1] <= 'f') ||	(x[i+1] >= 'A' && x[i+1] <= 'F') )
				{
					p[++d] = '\"';
					p[++d] = '\"';
				}
			}
			else
				p[d] = (unsigned char)x[i];
		}
	}
	p[d]=0;
	return p;
}

char *ST_make_path( const char* x )
{
	char *buf, *p;
	buf = p = g_selftest.utils->GetPathBuf();

	// Copy selftest data folder
	OpString folder_path;
	// FIXME: Deal with errors properly
	OpStatus::Ignore(g_folder_manager->GetFolderPath(OPFILE_SELFTEST_DATA_FOLDER, folder_path));

	p += folder_path.UTF8(p, ST_TEMPBUFFER_SIZE) - 1;

	// Copy current file path
	const char* current_file = g_selftest.utils->GetCurrentFile();
	int separator = op_strlen(current_file);
	while(separator >= 0 && current_file[separator] != '/')
		--separator;

	for (int i = 0; i<=separator; i++)
	{
		if (current_file[i] == '/')
			*p = PATHSEPCHAR;
		else
			*p = current_file[i];
		p++;
	}

	// Copy x
	op_strncpy(p, x, ST_TEMPBUFFER_SIZE - (p - buf));

	while(*p)
	{
		if (*p == '/')
			*p = PATHSEPCHAR;
		p++;
	}

	return buf;
}

const uni_char *ST_uni_make_path( const char* x )
{
	// FIXME!! - Shouldn't use 2 temp buffers.
	return ST_up(ST_make_path(x));
}

void ST_passed()
{
	g_selftest.utils->TestPassed();
	g_selftest.utils->AsyncStep();
}

void ST_failed( const char *fmt, ... )
{
	OP_NEW_DBG("ST_failed()", "selftest");
	//FIXME: Currently no line numbers for failed async tests.
	va_list args;
	OP_ASSERT (g_selftest.utils->output);

	if (g_selftest.utils->output)
	{
		va_start( args, fmt );
		if (!g_selftest.utils->has_test_result)
		{
			OP_DBG(("FAILED"));
			g_selftest.utils->output->Failed(fmt, g_selftest.utils->GetCurrentFile(), 0, &args);
		}
		va_end( args );

		OP_DBG(("has test result -> TRUE"));
		g_selftest.utils->has_test_result = TRUE;

		va_start( args, fmt );
		g_selftest.utils->output->TestFailed(g_selftest.utils->GetCurrentFile(), 0, fmt, args);
		va_end( args );
	}

	testsuite_break_here();

	g_selftest.utils->AsyncStep();
}

void ST_failed_with_status(const char *comment, OP_STATUS status, int line)
{
	OP_ASSERT(OpStatus::IsError(status));
	g_selftest.utils->TestFailedWithStatus(g_selftest.utils->GetCurrentFile(), line, comment, status);
	g_selftest.utils->AsyncStep();
}

void output( const char *fmt, ... )
{
	MEMDBG_GUARD_BEGIN
	va_list args;
	va_start( args, fmt );
	OP_ASSERT (g_selftest.utils->output);
	if (g_selftest.utils->output)
		g_selftest.utils->output->PublicOutputFormattedv(0, 0, 1, (char*)fmt, args);
	va_end( args );
	MEMDBG_GUARD_END
}

void _compare_failure( const char *file, int line, const char *a, const char *b,
					   const char *lhs, const char *rhs, const char *compare_txt,
					   int one_only )
{
	char *report = (char *)op_malloc(op_strlen( a ) + op_strlen( b ) +
									 op_strlen( lhs ) + op_strlen( rhs ) +
									 op_strlen( compare_txt ) + 100);
	if( !report )
		g_selftest.utils->TestFailed( file, line, "OOM\n" );
	else
	{
		if( !one_only )
			op_sprintf( report, "'%s' %s '%s'. lhs was %s and rhs was %s.", lhs,
						compare_txt, rhs, a, b );
		else
			op_sprintf( report, "'%s' %s '%s'. Value was %s.", lhs, compare_txt, rhs, a );
		g_selftest.utils->TestFailed( file, line, report);
		op_free( report );
	}
}

template<typename T>
	static int _direct_compare_test( const char *fmt, const char *file, int line,
									 const T &a, const T &b,
									 const char *lhs, const char *rhs, int one_only,
									 const char *compare_txt, int invert )
{
	int cmp = a!=b;
	if( cmp == invert )
	{
		char aa[100]; /* ARRAY OK 2007-03-09 marcusc */
		op_snprintf( aa, 99, fmt, a );
		char bb[100]; /* ARRAY OK 2007-03-09 marcusc */
		op_snprintf( bb, 99, fmt, b );
		_compare_failure( file, line, aa, bb, lhs, rhs, compare_txt, one_only );
		return 0;
	}
	g_selftest.utils->TestPassed();
	return 1;
}

int _compare_test( const char *file, int line,
					   void *a, void *b,
					   const char *lhs, const char *rhs, int one_only,
					   const char *compare_txt, int invert )
{
	return _direct_compare_test( "%p", file, line, (const void *)a, (const void *)b, lhs, rhs, one_only, compare_txt, invert );
}

int _compare_test( const char *file, int line,
				   int a, int b,
				   const char *lhs, const char *rhs, int one_only,
				   const char *compare_txt, int invert )
{
	return _direct_compare_test( "%d", file, line, a, b, lhs, rhs, one_only, compare_txt, invert );
}

int _compare_test( const char *file, int line,
				   unsigned int a, unsigned int b,
				   const char *lhs, const char *rhs, int one_only,
				   const char *compare_txt, int invert )
{
	return _direct_compare_test( "%u", file, line, a, b, lhs, rhs, one_only, compare_txt, invert );
}

int _compare_test( const char *file, int line,
				   long a, long b,
				   const char *lhs, const char *rhs, int one_only,
				   const char *compare_txt, int invert )
{
	return _direct_compare_test( "%d", file, line, a, b, lhs, rhs, one_only, compare_txt, invert );
}

int _compare_test( const char *file, int line,
				   unsigned long a, unsigned long b,
				   const char *lhs, const char *rhs, int one_only,
				   const char *compare_txt, int invert )
{
	return _direct_compare_test( "%u", file, line, a, b, lhs, rhs, one_only, compare_txt, invert );
}

int _compare_test( const char *file, int line,
				   float a, float b,
				   const char *lhs, const char *rhs, int one_only,
				   const char *compare_txt, int invert )
{
	return _direct_compare_test( "%f", file, line, (double)a, (double)b, lhs, rhs, one_only, compare_txt, invert );
}

int _compare_test( const char *file, int line,
				   double a, double b,
				   const char *lhs, const char *rhs, int one_only,
				   const char *compare_txt, int invert )
{
	return _direct_compare_test( "%f", file, line, (double)a, (double)b, lhs, rhs, one_only, compare_txt, invert );
}

int _compare_test( const char *file, int line, const char *a, const char *b, const char *lhs,
				   const char *rhs, int one_only, const char *compare_txt, int invert )
{
	int cmp = (a!=b) && !ST_streq(a,b);
	if( cmp == invert )
	{
		if( !a ) a = "<NULL>";
		if( !b ) b = "<NULL>";
		_compare_failure( file, line, a, b, lhs, rhs, compare_txt, one_only );
		return 0;
	}
	g_selftest.utils->TestPassed( );
	return 1;
}

int _compare_test( const char *file, int line,
				   const uni_char *a, const uni_char *b,
				   const char *lhs, const char *rhs, int one_only,
				   const char *compare_txt, int invert )
{
	int cmp = (a!=b) && ((!a && b) || (!b && a) || uni_strcmp(a,b));
	if( cmp == invert )
	{
		if( !a ) a = UNI_L("<NULL>");
		if( !b ) b = UNI_L("<NULL>");
		_compare_failure( file, line, ST_down(a), ST_down(b), lhs, rhs, compare_txt, one_only );
		return 0;
	}
	g_selftest.utils->TestPassed();
	return 1;
}

void TestUtils::InitL()
{
	always_verbose = 0;
	buff = 0;
	current_group = 0;
	run_manual_tests = false;
	current_file = NULL;
	current_module = NULL;
	current_test = NULL;
	doc = NULL;
	failed_tests = 0;
	window = 0;
	delayed = 0;
	delay_counter=150;
	last_test_index=-1;
	keyspace_len = 0;
	keyspace = NULL;
	patterns = NULL;
	npatterns = NULL;
	mpatterns = NULL;
	decompress_buffer_uni=NULL;
	decompress_buffer_uni_size=0;
	decompress_buffer=NULL;
	decompress_buffer_size=0;
	running_async = FALSE;
	timer = 0;
	expect_fail = FALSE;
	allow_exceptions = FALSE;
}

TestUtils::TestUtils()
{
	output = NULL;
	async_test_done = NULL;
	InitL(); // Not really leaving.
}



void TestUtils::Reset()
{
	passed.ForEach( &do_delete_a );
	passed.RemoveAll();
	threads_listened.RemoveAll();

	OP_DELETE(patterns);
	OP_DELETE(npatterns);
	OP_DELETE(mpatterns);
	OP_DELETEA(decompress_buffer_uni);
	OP_DELETEA(decompress_buffer);
	OP_DELETEA(keyspace);
	InitL();

	OP_DELETE(output);
	output = NULL;
}

BOOL TestUtils::ShouldExitAfterCompletedTests(){
	//With GOGI/ opera:selftest, we dont know if we wanted to exit
	//after reset, so we check it now
	BOOL doCallExitAfterCompletedTests = TRUE;
	if(OperaSelftestDispatcher::isSelftestConnectedToWindow() || OperaSelftestDispatcher::isSelftestConnectedToScope())
		doCallExitAfterCompletedTests = FALSE;
	return doCallExitAfterCompletedTests;
}

void TestUtils::ExitAfterCompletedTests(int exit_code)
{

#ifdef QUICK
	if( g_application )
	{
		OpInputAction a(OpInputAction::ACTION_EXIT, OP_KEY_Q, 0, FALSE, OpKey::LOCATION_STANDARD, OpInputAction::METHOD_OTHER);
		a.SetActionKeyValue("q", 1);
		g_application->GetInputContext()->OnInputAction( &a );
		return;
	}
#endif // QUICK

#ifdef CORE_GOGI
	g_coregogi->Exit(exit_code);
#endif // CORE_GOGI
}

#define StringCompareTest_STR_CMP(a, b, fn) ((a != NULL && b != NULL) ? (fn(a, b)) : (a != NULL ? -1 : (b == NULL ? 0 : 1)))

void StringCompareTest::Init(const char* a, const char* b)
{
	FillIn(a, b, StringCompareTest_STR_CMP(a, b, op_strcmp));
}

void StringCompareTest::Init(const uni_char* a, const uni_char* b)
{
	op_memset(m_downsize_buf, 0, DOWNSIZE_BUF_SIZE);

	char* lhs = a ? m_downsize_buf : NULL;
	char* rhs = b ? m_downsize_buf + DOWNSIZE_BUF_SIZE / 2 : NULL;

	int dummy;
	UTF16toUTF8Converter decoder;
	if (a)
		decoder.Convert(a, UNICODE_SIZE(uni_strlen(a) + 1), lhs, DOWNSIZE_BUF_SIZE / 2 - 1, &dummy);
	if (b)
		decoder.Convert(b, UNICODE_SIZE(uni_strlen(b) + 1), rhs, DOWNSIZE_BUF_SIZE / 2 - 1, &dummy);

	FillIn(lhs, rhs, StringCompareTest_STR_CMP(a, b, uni_strcmp));
}

void StringCompareTest::Init(const uni_char* a, const char* b)
{
	if (a)
	{
		op_memset(m_downsize_buf, 0, sizeof(DOWNSIZE_BUF_SIZE));

		char* lhs = m_downsize_buf;

		int dummy;
		UTF16toUTF8Converter decoder;
		decoder.Convert(a, UNICODE_SIZE(uni_strlen(a) + 1), lhs, DOWNSIZE_BUF_SIZE - 1, &dummy);

		FillIn(lhs, b, StringCompareTest_STR_CMP(a, b, uni_strcmp));
	}
	else
	{
		FillIn(NULL, b, StringCompareTest_STR_CMP(a, b, uni_strcmp));
	}
}

void StringCompareTest::Init(const char* a, const uni_char* b)
{
	Init(b, a);
	//flip again
	FillIn(m_rhs, m_lhs, -m_result);
}

StringCompareTest::StringCompareTest(const char* a, Str::LocaleString b)
{
	Init(a, b);
}

StringCompareTest::StringCompareTest(const uni_char* a, Str::LocaleString b)
{
	Init(a, b);
}

StringCompareTest::StringCompareTest(const OpStringC8& a, Str::LocaleString b)
{
	Init(a.CStr(), b);
}

StringCompareTest::StringCompareTest(const OpStringC16& a, Str::LocaleString b)
{
	Init(a.CStr(), b);
}

void StringCompareTest::Init(const char* a, Str::LocaleString b)
{
	OpString tmp;
	TRAPD(err, g_languageManager->GetStringL(b, tmp));
	OpStatus::Ignore(err);
	Init(a, tmp.CStr());
}

void StringCompareTest::Init(const uni_char* a, Str::LocaleString b)
{
	OpString tmp;
	TRAPD(err, g_languageManager->GetStringL(b, tmp));
	OpStatus::Ignore(err);
	Init(a, tmp.CStr());
}

void StringCompareTest::FillIn(const char* a, const char* b, int result)
{
	m_result = result;
	m_lhs = a;
	m_rhs = b;
}

FileComparisonTest::FileComparisonTest(const uni_char* reference_filename, const uni_char* tested_filename)
	: m_tested_filename(tested_filename), m_reference_filename(reference_filename)
{
	OP_ASSERT(m_tested_filename);
	OP_ASSERT(m_reference_filename);

	op_memset(m_filenames_buffer, 0, FILENAMES_BUF_SIZE);

	int dummy;
	UTF16toUTF8Converter decoder;

	m_reference_filename8 = m_filenames_buffer;
	int utf16_size = UNICODE_SIZE(uni_strlen(m_reference_filename) + 1);
	decoder.Convert(m_reference_filename, utf16_size, m_reference_filename8, FILENAMES_BUF_SIZE / 2 - 1, &dummy);


	m_tested_filename8 = m_filenames_buffer + FILENAMES_BUF_SIZE / 2;
	utf16_size = UNICODE_SIZE(uni_strlen(m_tested_filename) + 1);
	decoder.Convert(m_tested_filename, utf16_size, m_tested_filename8, FILENAMES_BUF_SIZE / 2 - 1, &dummy);
}

OP_BOOLEAN FileComparisonTest::CompareBinary()
{
	OpFile file1, file2;
	RETURN_IF_ERROR(file1.Construct(m_reference_filename));
	RETURN_IF_ERROR(file1.Open(OPFILE_READ));
	RETURN_IF_ERROR(file2.Construct(m_tested_filename));
	RETURN_IF_ERROR(file2.Open(OPFILE_READ));
	const OpFileLength BUFFER_LEN = 512;
	char buffer1[BUFFER_LEN], buffer2[BUFFER_LEN];
	OpFileLength bytes_read1, bytes_read2;
	do
	{
		RETURN_IF_ERROR(file1.Read(buffer1, BUFFER_LEN, &bytes_read1));
		RETURN_IF_ERROR(file2.Read(buffer2, BUFFER_LEN, &bytes_read2));
		if (bytes_read1 != bytes_read2 || op_memcmp(buffer1, buffer2, static_cast<size_t>(bytes_read1)) != 0 )
			return OpBoolean::IS_FALSE;
		if (bytes_read1 < BUFFER_LEN)
			return OpBoolean::IS_TRUE;
	}
	while(TRUE);
}

TestUtils::~TestUtils()
{
	Reset();
	OP_DELETE(async_test_done);
}

#ifdef USE_ZLIB
const char *TestUtils::DecompressString( const unsigned char *input, int input_len,
										 int len, BOOL is_uni )
{
	char *output_buffer;

	if( is_uni )
	{
		if( len > decompress_buffer_uni_size )
		{
			OP_DELETEA(decompress_buffer_uni);
			decompress_buffer_uni = (char *)OP_NEWA(uni_char, len);
			if( decompress_buffer_uni )
				decompress_buffer_uni_size = len;
			else
				decompress_buffer_uni_size = 0;
		}

		len *= 2; // 2 bytes per char
		output_buffer = decompress_buffer_uni;
	}
	else
	{
		if( len > decompress_buffer_size )
		{
			OP_DELETEA(decompress_buffer);
			decompress_buffer = OP_NEWA(char, len);
			if( decompress_buffer )
				decompress_buffer_size = len;
			else
				decompress_buffer_size = 0;
		}
		output_buffer = decompress_buffer;
	}

	if( !output_buffer ) // OOM
		return 0;

	z_stream d_stream;
	op_memset(	&d_stream, 0, sizeof(d_stream));
    d_stream.next_in  =	(Bytef*)input;
	d_stream.avail_in = input_len;
	d_stream.next_out = (Bytef*)output_buffer;
	d_stream.avail_out = len;

	int err	= inflateInit(&d_stream);
	if(err != Z_OK)
		return NULL; // OOM again.
	inflate( &d_stream, Z_FINISH );
	if( inflateEnd( &d_stream ) != Z_OK )
		return NULL;

#ifndef OPERA_BIG_ENDIAN
	if( is_uni )
	{
		for( int i = 0; i<len; i+=2 )
		{
			char tmp = output_buffer[i];
			output_buffer[i] = output_buffer[i+1];
			output_buffer[i+1] = tmp;
		}
	}
#endif
	return output_buffer;
}
#endif // USE_ZLIB

#endif // SELFTEST
