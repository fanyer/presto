/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4;  -*-
**
** Copyright (C) 2002-now Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/
#include "core/pch.h"

#ifdef SELFTEST
#include "modules/dochand/win.h"
#include "modules/doc/frm_doc.h"

#include "modules/selftest/optestsuite.h"

#include "modules/selftest/generated/testgroups.h"

#include "modules/selftest/src/testutils.h"
#include "modules/selftest/src/testoutput.h"

#define const_strlen( X ) (sizeof(X) - sizeof(""))
#define PREFIX( A, B ) !op_strncmp( A, B, const_strlen(B) )

class SelftestCLParser
{
public:
	SelftestCLParser();
	~SelftestCLParser();

	void Parse( int *argc, char *argv[] );

	BOOL ShowHelp() const { return show_help; }
	BOOL ListModules() const { return list_modules; }
	BOOL DoRun() const { return do_run; }
	BOOL PostInit() const { return postinit; }
	BOOL SpartanReadable() const { return spartan_readable; }
	BOOL ForceStdout() const { return force_stdout; }

	int NumGlobs() const { return globs; }

	char* OutputFile() const { return output_file; }
	char* TestdataDir() const { return testdata_dir; }

private:
	BOOL
		show_help,
		list_modules,
		do_run,
		postinit,
		spartan_readable,
		force_stdout;

	int globs;

	char* output_file;
	char* testdata_dir;
};

SelftestCLParser::SelftestCLParser() :
	show_help(FALSE),
	list_modules(FALSE),
	do_run(FALSE),
	postinit(TRUE),
	spartan_readable(FALSE),
	force_stdout(FALSE),
	globs(0),
	output_file(NULL),
	testdata_dir(NULL)
{
}

SelftestCLParser::~SelftestCLParser()
{
	op_free(output_file);
	op_free(testdata_dir);
}

void
SelftestCLParser::Parse( int *argc, char *argv[] )
{
	for( int i = 1; i<*argc; i++ )
	{
		const char *p = argv[i];
		TestUtils::PatternType pattern_type = TestUtils::PATTERN_NORMAL;
		while(*p=='-' || *p=='/')
			p++;

		if( PREFIX(p,"run") ) // COMPAT
		{
			p += 3;
			while( *p=='-' ) p++;
		}

		if( PREFIX(p, "exclude") )
		{
			pattern_type = TestUtils::PATTERN_EXCLUDE;
			p += const_strlen("exclude");
			while(*p=='-')p++;
		}

		if( PREFIX(p,"test") )
		{
			// This argument is ours. Let's remove argv[i].
			for( int j = i; j<*argc-1; j++ )
				argv[j] = argv[j+1];


			(*argc)--;i--;

			p += const_strlen("test");
			if(*p=='s')p++;
			while(*p=='-')p++;

			if( PREFIX( p, "help" ) )
				show_help = TRUE;
			if( PREFIX(p, "output" ) )
			{
				p += const_strlen("output");
				if(*p=='=')p++;
				output_file = op_strdup( p );	// OOM safe
				continue;
			}

			if( PREFIX(p, "testdata" ) )
			{
				p += const_strlen("testdata");
				if(*p=='=')p++;
				testdata_dir = op_strdup( p );	// OOM safe
				continue;
			}

			if( PREFIX(p, "spartan-readable") )
			{
				p += const_strlen("spartan-readable");
				spartan_readable = TRUE;
				continue;
			}

			if( PREFIX(p, "force-stdout") )
			{
				p += const_strlen("force-stdout");
				force_stdout = TRUE;
				continue;
			}

			if( PREFIX(p, "manual" ) )
			{
				g_selftest.utils->RunManual( true );
				p += const_strlen("manual");
				while(*p=='-')p++;
			}
			if( PREFIX(p,"silent") )
			{
				g_selftest.utils->SetVerbose( 0 );
				p += const_strlen("silent");
				while(*p=='-')p++;
			}
			if( PREFIX(p, "now") )
			{
				postinit = FALSE;
				p += const_strlen("now");
				while(*p=='-')p++;
			}
			if( PREFIX(p, "exclude") )
			{
				pattern_type = TestUtils::PATTERN_EXCLUDE;
				p += const_strlen("exclude");
				while(*p=='-')p++;
			}
			if( PREFIX(p, "list") )
				list_modules = TRUE;
			if( PREFIX(p, "module") )
			{
				pattern_type = TestUtils::PATTERN_MODULE;
				p += const_strlen("module");
				while(*p=='-')p++;
			}
			if( *p == '=' )
			{
				p++;
				int read = 0;				
				OpStatus::Ignore(g_selftest.utils->ParsePatterns(p, read, pattern_type));

				globs += read;
			}
			do_run = TRUE;
		}
	}
}


// The testsuite looks for quite a few different arguments to decide
// what it should do.
//
// --runtests-verbose, --runtestsverbose, --tests-verbose
// --tests, /runtestsverboselater /run-tests-verbose-later
// --tests=*:*, /tests-later=Foo*:*,Bar*:gazonk*  etc.
//
// The modifiers are:
//
// 'verbose': Print some output for all tests, not
// only tests that fail. This data is normally sent to the default
// debug file. (c:\klient\debug.txt or similar on windows)
//
// 'later': Do not run the test at the very beginning of main,
// instead, wait for opera to be initialized before any tests are run.
// This allows more tests to be run, but you will have a window on the
// screen for a while.
//
//	<whatever>=glob[:glob][,glob[:glob]...]
//
// 	Only run some of the tests. Specifically, those whose testgroup
// 	match the first part of the :-separated glob, and the test-name
// 	match the second part.
//
// 	The default second glob is '*'. You can specify more than one
//  glob, by using , to separate the globs.

TestSuite::TestSuite()
	: opera_is_initialized(0), looped(FALSE), do_run(FALSE), group(NULL), message_handler(NULL)
{}

OP_STATUS
TestSuite::Construct()
{
	message_handler = OP_NEW(MessageHandler, (NULL));
	if (!message_handler)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(message_handler->SetCallBack(this, MSG_TESTSUITE_STEP, 0));

	return OpStatus::OK;
}

TestSuite::~TestSuite()
{
	message_handler->UnsetCallBack(this, MSG_TESTSUITE_STEP, 0);

	OP_DELETE(message_handler);
	OP_DELETE(group);
}

/* static */
void TestSuite::main( int *argc, char *argv[] )
{
	extern void TS_WriteDebugString(const char* str);
	if( !g_opera  || !g_selftest.suite )
	{
		TS_WriteDebugString("******* TestSuite::main(c,v) can not be called before the\n"
							"******* g_opera object has been intialized.\n");
		TS_WriteDebugString("******* Please move the call to after opera initialization\n");
	}
	else
		g_selftest.suite->_main( argc, argv );
}

/* static */
void TestSuite::run( int initialized, int force )
{
	g_selftest.suite->_run( initialized, force );
}

/* static */
int TestSuite::step( int test_id )
{
	return g_selftest.suite->_step( test_id );
}

/* static */
void TestSuite::Cleanup(  )
{
	g_selftest.suite->_cleanup( );
}

void TestSuite::_main( int *argc, char *argv[] )
{
	BOOL do_cleanup = FALSE;
	
	SelftestCLParser parser;
	parser.Parse(argc, argv);

	do_run = parser.DoRun();	
	if (!do_run)
	{
		// Hack to reset the web-UI on bogus input while avoiding shutting down on startup.
#ifdef OVERRIDE_SELFTEST_OUTPUT
		extern void TS_FinishDebugString();
		TS_FinishDebugString();
#endif // OVERRIDE_SELFTEST_OUTPUT
		return;
	}


	if( do_run && !parser.NumGlobs() )
		g_selftest.utils->AddPattern( "*", "*", TestUtils::PATTERN_NORMAL );
	else
	if( parser.NumGlobs() )
	{
		// Process imports
		const char *group, *module;
		for(int i=0; GroupInfo( i, group, module ); i++ )
		{
			int patterns;
			if (g_selftest.utils->MatchPatterns(module, group, NULL))
				g_selftest.utils->ParsePatterns(g_selftest.groups->g[i].imports, patterns, TestUtils::PATTERN_NORMAL);
		}
	}
	
	
	// If _main is run multiple times we just replace the output handler.
	OP_DELETE(g_selftest.utils->output);

	TestOutput* output;
	if (parser.SpartanReadable())
	{
		output = OP_NEW(SpartanTestOutput, ());
	}
	else
	{
		output = OP_NEW(StandardTestOutput, ());
	}

	if (output && OpStatus::IsSuccess(output->Construct(parser.OutputFile())))
	{
		g_selftest.utils->output = output;
	}
	else
	{
		OP_DELETE(output);
		do_cleanup = TRUE;
		return;
	}

	if( parser.ShowHelp() )
	{
		do_cleanup = TRUE;

		OUTPUT_STR("\n\n=========================================================================\n" );
		OUTPUT_STR("Selftest argument summary, short version\n");
		OUTPUT_STR("=========================================================================\n" );
		OUTPUT_STR("Behaviour modifying arguments:\n"
				   " --test-help:				Show this help\n"
				   " --test-output=PATH:		Write testoutput to the given file\n"
				   " --test-manual:				Run manual tests in addition to normal tests\n"
				   " --test-now:				Run tests before the event loop has been started\n"
				   " --test-list:				Only list the testgroups that would have been run\n"
				   " --test-spartan-readable	Format output in a spartan readable way\n"
				   " --test-testdata=PATH:		Change testdata directory (does not reset)\n"
				   "\n"
				   "Test selection arguments (these might be repeated multiple times):\n"
				   " --test[=PATTERN[,PATTERN[,...]]]\n" );
		OUTPUT_STR("       Run tests. If a pattern is given, add it to the list of patterns a\n"
				   "       test must match in order to be run.\n"
				   " --test-exclude=PATTERN[,PATTERN[,...]]\n"
				   "       Exclude all tests matching the given PATTERN(s)\n"
				   " --test-module=MODULEPATTERN[,MODULEPATTERN[,...]]\n"
				   "       Limit tests to the ones defined in the given module. Can be\n"
				   "       combined with --test and --test-exclude.\n"
				   "\n");
		OUTPUT_STR("Patterns:\n"
				   "  Patterns are a simple glob-style pattern. Special characters are * and ?.\n"
				   "  Use ':' to separate testname and groupnames.\n"
				   "  Example:  *png*:*icicles*  Test all tests with icicles in their name in\n"
				   "  any groups named png.\n"
				   "\n"
				   "  Module patterns are like patterns, only consist of one part.\n");
		if( !parser.ListModules() )
			OUTPUT_STR("=========================================================================\n\n" );
	}
	if( parser.ListModules() )
	{
		const char *group, *module;
		do_cleanup = TRUE;
		OUTPUT_STR("=========================================================================\n" );
		OUTPUT(0,0,1,"%-20s %s\n", "Module","Group");
		OUTPUT_STR("=========================================================================\n" );
		for( int i=0; GroupInfo( i, group, module ); i++ )
		{
			if (g_selftest.utils->MatchPatterns(module, group, NULL))
				OUTPUT( 0, 0, 1, "%-20s %s\n", module, group );
		}
		OUTPUT_STR("=========================================================================\n" );
	}

	if (do_cleanup)
	{
		do_run = FALSE;
		_cleanup();
		return;
	}

	if( parser.TestdataDir() != 0 )
	{
		OpString tmp_storage;
		OpStringC folder_path(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_SELFTEST_DATA_FOLDER, tmp_storage));

		//need utf pga commandline
		char* folder_path_utf8 = new char[256];
		folder_path.UTF8(folder_path_utf8, 256);

		//Output
		OUTPUT_STR("\nChange testdata directory <OPFILE_SELFTEST_DATA_FOLDER>\n" );
		OUTPUT_STR("-From: ");
		OUTPUT_STR(folder_path_utf8);
		OUTPUT_STR("\n-To: ");
		OUTPUT_STR(parser.TestdataDir());
		OUTPUT_STR("\n");
		
		//Change
		OpString16 new_folder_path;
		new_folder_path.Set(parser.TestdataDir());
		g_folder_manager->SetFolderPath(OPFILE_SELFTEST_DATA_FOLDER, new_folder_path);
		
		//Clean
		delete [] folder_path_utf8;
	}

	if( do_run && !parser.PostInit() )
		run( 0, 1 );
}

void TestSuite::_run( int inited, int force )
{
	if( (!force && !do_run) || g_selftest.running)
		return;

	g_selftest.running = TRUE;
	looped = FALSE;
	opera_is_initialized = inited;

	output("\n\n");
	output("=========================================================================\n" );
	output("Opera Testsuite\n" );
	output("=========================================================================\n" );

	g_selftest.utils->WarnAboutNoMatches();

	if( inited )
	{
		g_selftest.utils->AsyncStep();
		return;
	}

	group = g_selftest.groups->next();
	while( group )
	{
		int r = group->TS_step();
		if( !r )
		{
			OP_DELETE(group);
			group = g_selftest.groups->next();
		}
	}
	g_selftest.utils->Summary();
}

/* static */
BOOL TestSuite::GroupInfo( int n, const char *&name, const char *&module )
{
	name = module = NULL;
	if( !g_selftest.groups )
		return FALSE;
	return g_selftest.groups->info( n, name, module );
}

int TestSuite::_step( int test_id )
{
	// Safety check. Starting a new step while an asynchronous test is running
	// causes problems.
	if (g_selftest.utils->running_async)
		return -1;

	if ( test_id != g_selftest.utils->last_test_index)
	{
		// This probably means that the last test sent multiple
		// messages. Just ignore for now.
		return -1;
	}

	if( looped ) // Already done.
	{
		// Selftest received a step message after the test run was completed.
		// Please report to the selftest module owner is this happens.
		OP_ASSERT(!"Message was received after the test run was completed");
		return 0;
	}

	// GetWindow() is called for its side-effects, as it ensures that there is a valid window available.
	// This avoids a crash in Debug Desktop configuration, during a switch between one test group and another.
	g_selftest.utils->GetWindow();

	g_selftest.utils->doc = g_selftest.GetTestDoc();

	if( !group )
		group =  g_selftest.groups->next();
	while( group )
	{
		// Run the next test
		int test_res = group->TS_step();

		switch( test_res )
		{
			case 1:
				// Continue.
				g_selftest.utils->AsyncStep();
			case -1:
				return -1;
			case 0:
				// Done with group
				OP_DELETE(group);
				group = NULL;
				g_selftest.utils->GetMemoryCleanupUtil()->Cleanup();
				g_selftest.utils->AsyncStep();
				return -1;
		}
	}

	// Done with the test suite
	if ( g_selftest.utils->running_async )
		return -1;

	looped = TRUE;
	g_selftest.utils->Summary();
	return 0;
}

void TestSuite::Reset()
{
	// Nothing to do.
}

void TestSuite::_cleanup()
{
	OP_ASSERT(!group);

	//With GOGI/ opera:selftest, we dont know if we wanted to exit
	//after reset, so we check it now
	BOOL doCallExitAfterCompletedTests = g_selftest.utils->ShouldExitAfterCompletedTests();
	
	//Finish final output!
#ifdef OVERRIDE_SELFTEST_OUTPUT
	extern void TS_FinishDebugString();
	TS_FinishDebugString();
#endif // OVERRIDE_SELFTEST_OUTPUT

	int exit_code = g_selftest.utils->failed_tests;

	//Reset everything
	g_selftest.Reset();

	//Exit if we should
	if(doCallExitAfterCompletedTests){
		g_selftest.utils->ExitAfterCompletedTests(exit_code);
	}
}

void
TestSuite::PostStepMessage(int delay)
{
	message_handler->PostMessage(MSG_TESTSUITE_STEP, g_selftest.utils->last_test_index, 0, delay);
}

void
TestSuite::HandleCallback(OpMessage msg, MH_PARAM_1 test_id, MH_PARAM_2 par2)
{
	switch (msg)
	{
	case MSG_TESTSUITE_STEP:
		switch( _step( test_id )  )
		{
			case 1:
				PostStepMessage();
				break;
			case 0:
				_cleanup();
				break;
			default:
				// It's up to the test that returned -1 to
				// re-post a MSG_TESTSUITE_STEP message
				break;
		}
		break;
	}
}

void SelftestModule::seed_deterministic_rand(unsigned int seed)
{
	OP_ASSERT(running);
	deterministic_rand_seed = seed;
}

int SelftestModule::deterministic_rand()
{
	OP_ASSERT(running);
	// This random generator is only used for selftest, so it doesn't matter
	// how simple it is. Using previous glibc "rand()" implementation
	deterministic_rand_seed = deterministic_rand_seed * 1103515245 + 12345;
	return deterministic_rand_seed & 0x7fffffff;
}

FramesDocument *
SelftestModule::GetTestDoc()
{
	return g_selftest.utils->window ? g_selftest.utils->window->GetCurrentDoc() : NULL;
}

#endif // SELFTEST
