/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

// Yet another template file for the selftest system. This one
// contains the parts that make up the tests inserted into the other
// template file.

test_array_init: ecma {       ___TesTS___[N]=&CLASS::FUNCTION;
}

suite:  {
	class CLASS : public TestGroup GROUP_BASE
	{
	public:
		virtual int TS_step(); // Runs the next test, or switched to the next suite if there are no more tests.
		CLASS();
		virtual ~CLASS();

		// Biggest int type supported by the platform, used by verify(int op int).
#ifdef HAVE_INT64
		typedef INT64 big_int_t;
#else // HAVE_INT64
		typedef INT32 big_int_t;
#endif // HAVE_INT64

#STARTCOND
		GLOBALS
		;
	private:
		int TS_skip_group;
		int TS_next_test;
		int TS_doc_was_reset;
		int TS_has_loaded_document;

		typedef int (CLASS::*TestCallback)();
		TestCallback ___TesTS___[MAX(NTESTS,1)];
		const uni_char *_TS_STRINGS_;
		const unsigned char *_TS_CSTRINGS_;

		int TS_init()
		{
			REQUIRE_INIT;
			if( !_TS_STRINGS_ || !_TS_CSTRINGS_ )
				TS_skip_group=true;
			else
				TS_skip_group = !g_selftest.utils->GroupSet( GROUPNAME, FILE, LINE );
			if( TS_skip_group )
				return 0;
			INITIALIZERS
			;
			TS_next_test=1;
			return 1;
		}
		int TS_reset_document()
		{
			// Reset document for 1st test.
			if (!TS_doc_was_reset && -1 == g_selftest.utils->LoadNewDocument(NULL, NULL, NULL, 0))
				return 0;
			TS_doc_was_reset = 1;
			return 1;
		}
		void TS_exit()
		{
			if( !TS_skip_group )
			{
				EXIT_CODE
				;
			}
		}
		TESTS_PROTOS
#endif
	};

OUTLINED

    int CLASS::TS_step()
    {
#STARTCOND
		if( TS_skip_group || (!TS_next_test && !TS_init()) || (TS_next_test > NTESTS) )
		{
			return 0;
		}
		if (!TS_reset_document())
			return -1;
		switch( (this->*___TesTS___[TS_next_test++ - 1])() )
		{
			case 0:
			case 1:
				TS_has_loaded_document = 0;
				return 1;
			case -1:
				TS_next_test--;
				/* fallthrough */
			default:
				return -1;

		}
#else
		return 0;
#endif
    }

	CLASS::~CLASS()
	{
#STARTCOND
		TS_exit();
#endif
	}

	CLASS::CLASS()
	{
#STARTCOND
		static const unsigned char ustring[]={ UNI_STRING };  // FIXME: Global static array
		static const unsigned char cstring[]={ STRING };      // FIXME: Global static array
		TS_skip_group = 0;
		TS_next_test = 0;
		TS_doc_was_reset = 0;
		TS_has_loaded_document = 0;

		ARRAYS_INIT

		TESTS_INIT

		if( UNI_STRING_LEN )
			_TS_STRINGS_ = (const uni_char *)g_selftest.utils->DecompressString(ustring,sizeof(ustring),UNI_STRING_LEN,TRUE);
		else
			_TS_STRINGS_=(const uni_char*)"\0";
		if( STRING_LEN )
			_TS_CSTRINGS_ = (const unsigned char *)g_selftest.utils->DecompressString(cstring,sizeof(cstring), STRING_LEN,FALSE);
		else
			_TS_CSTRINGS_=(const unsigned char *)"";
#endif
	}

#ifndef OUTPUT_SUITE_FAIL
#  define OUTPUT_SUITE_FAIL(X) output("Failed to create group (" #X ") due to OOM. Aborting\n")
#endif

	void *CONCAT(CLASS,_Create)(
#STARTCOND
		int probe
#else
		int /*probe*/
#endif
		)
	{
#STARTCOND
		if( probe ) return (void *)1;
		TestGroup *ss = OP_NEW(CLASS, ());
		if( !ss )
			OUTPUT_SUITE_FAIL(CLASS);
		return (void *)ss;
#else
		return 0;
#endif
	}
#STARTCOND
	TESTS
#endif
}


/* Code used when initialization is required for a group*/
require_group_init: {
    if(!g_selftest.suite->IsOperaInitialized())
    {
		TS_skip_group=1;
		return 0;
    }
}

/* Code used when initialization is required to be false for a group*/
require_group_uninit: {
    if(g_selftest.suite->IsOperaInitialized())
    {
		TS_skip_group=1;
		return 0;
    }
}

/* Code put inside each loop in a multi-test */
multi_loop: {
	{
		char name[255]; /* ARRAY OK 2007-03-09 marcusc */
		op_snprintf( name,254, FORMAT, FORMATARGS );
		name[254]=0;
		g_selftest.utils->last_test_index = -1;
		CONCAT(TESTFUNC,_)(name, TESTARGS);
	}
}

/* Pre-delay code */
delay_pre: {
     if( !g_selftest.utils->delayed )
	 {
         g_selftest.utils->delayed = 1;
         g_selftest.utils->AsyncStep(DELAY);
         return -1;
     }
	 g_selftest.utils->delayed = 0;
}

/* Post-delay code */
delay_post: {
         g_selftest.utils->AsyncStep(DELAY);
		 g_selftest.utils->delayed = 0;
}

/* Basic structure of a single test */
test_base: {
	if( g_selftest.utils->TestSet( NAME, MANUAL, TS_next_test) )
	{
		if (!g_selftest.utils->DocumentLoaded())
			return -1;
		g_selftest.utils->expect_fail = FAIL;
		TIMER_START
		REPEAT_START
		BODY
		;
		REPEAT_END
		FINALLY
		;
	}
}

/* Test failed code. Scope needed. */
test_failed: {	{g_selftest.utils->TestFailed(FILE,LINE,REASON ARGS);FAILED}}

test_failed_status: {	{g_selftest.utils->TestFailedWithStatus(FILE,LINE,REASON,ARG);FAILED}}

/* Test passed code */
test_passed: {g_selftest.utils->TestPassed();}

/* Test skipped code */
test_skipped: {
	return g_selftest.utils->TestSkipped( NAME, REASON, 0 );
}

/* Conditional test skip code */
test_skip_if: {
	if( WHEN )
	{
		return g_selftest.utils->TestSkipped( NAME, REASON, 0 );
	}
}

/* Code to ask user for success of a test */
test_ask_user_ok: {
	g_selftest.utils->AskUserOk( FILE, LINE, QUERY );
	return -2;
}

test_leakcheck: {
	int GROUPNAME::LOW_TESTNAME() {
		g_selftest.utils->memSetMarker();
		int res = TESTNAME();
		if (res != -1) 
			g_selftest.utils->memCheckForLeaks();
			return res;
		}
}


/* Javascript versions */

// All on one line for error reporting
test_js_verify_base:   ecma  {do { var _TS_=LHS; if( !(_TS_ OPERATOR RHS) ) { ST_failed( REPORT, $ST_filename$, LINE ); return;}} while(0);}
test_js_failed_if_not: ecma  {if(!(CONDITION)) { ST_failed( REPORT, $ST_filename$, LINE); return; }}
test_js_passed_if_not: ecma {if(!(CONDITION)) { ST_passed(); return; } }
test_js_verify_files_equal: ecma { do { if (! (ST_binary_compare_files(LHS, RHS))) { ST_failed (REPORT, $ST_filename$, LINE ); return;}} while(0);}
test_js_failed:        ecma { ST_failed( REPORT, $ST_filename$, LINE); }
test_js_passed:        ecma { ST_passed(); }
test_js_check_fn:      ecma { window.$check_selftest$($ST_group$,$ST_testname$); }
test_js_base:          ecma
{ var $ST_filename$=FILE; try { (function(){ var $ST_group$ = GROUPNAME, $ST_testname$ = TESTNAME; CODE
	/* The code above is in one line, so the template variable 'CODE' falls in the
	   first line of the code block, so exception reporting reports the correct line
	   number relative to the ot file. */
		})();
	} catch(e) {
		if( FAIL )
			ST_passed();
		else
		{
			var st = e && e.stacktrace;
			var line = '', line_no = 0;
			if (st) {
				st = st.split('\n');
				if (st[0].match(new RegExp('at line (\\d+)')))
					line_no = parseInt(RegExp.$1);
				if (st[1])
					line = st[1].replace(new RegExp('^\\s+'),'').replace(new RegExp('\\s+$'),'');
			}
			opera.postError(e && (e.message || e.stacktrace) ? e.message + '\n' + e.stacktrace : e);
			ST_failed('Exception: "' + String((e && e.message) || e) + '"' + (line ? ', Code: "' + line + '"' : ''), $ST_filename$, line_no);
		}
	}
}

test_js_base_cc: {
	if( g_selftest.utils->TestSet( NAME, MANUAL, TS_next_test) )
		return g_selftest.utils->TestEcma( ASYNC, FAIL, ALLOWEXCEPTIONS, SCRIPTLINENUMBER, PROG );
}


// HTML/DATA
test_html_base_cc: {
	int CLASS::TEST()
	{
		if (!TS_has_loaded_document && (!g_selftest.utils->DocumentLoaded() || g_selftest.utils->LoadNewDocument(URL, DATA, TYPE, DOCUMENTBLOCKLINENUMBER) == -1))
			return -1; /* Call this function again */
		TS_has_loaded_document = 1;
		return 0;
	}
}

// test_eq/test_neq
test_compare: {
	{
		if( _compare_test( FILE,LINE,(LHS),(RHS),LHSS,RHSS,ONE_ONLY,COMPARE_TXT,INVERT_TEST) )
			return 1;
	}
	// Fallthrough to test base above.
}



test_groups: {
PROTOS

	typedef void *(*construct_callback)(int);
	class TestGroups
	{
	public:
		TestGroups();
		TestGroup *next();
		BOOL info(int n, const char*&name, const char*&module);
		void Reset(){current=0;}
		int allgroups;
		int ngroups;
		int current;
		struct group {
			construct_callback constructor;
			const char *name;
			const char *module;
			const char *imports;
		} g[NGROUPS];
	};

    inline TestGroups::TestGroups() : allgroups(NGROUPS), current(0)
	{
		int i,j;
		INIT;


		for( j=0,i=0; i<NGROUPS; i++ )
			if( g[i].constructor(1) )
			{
				if( j != i )
					g[j]=g[i];
				j++;
			}
		ngroups=j;
	}

	inline TestGroup *TestGroups::next()
	{
		if(current >= ngroups)  return NULL;
		return (TestGroup*)g[current++].constructor(0);
	}

	inline BOOL TestGroups::info( int n, const char*&name, const char*&module )
	{
		if( n == -1 ) n = current-1;
		if( n < 0 || n >= ngroups ) return FALSE;

		name = g[n].name;
		module = g[n].module;
		return TRUE;
	}
}

