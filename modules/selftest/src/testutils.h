/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef TESTSUITE_TESTUTILS_H
#define TESTSUITE_TESTUTILS_H

#if defined(SELFTEST)

#define OUTPUT g_selftest.utils->output->PublicOutputFormatted
#define OUTPUT_STR(X) g_selftest.utils->output->PublicOutputFormatted( 0,0,1,"%s",(X))
#define TEST_FAILED  g_selftest.utils->TestFailed
#define TEST_PASSED  g_selftest.utils->TestPassed
#define TEST_SET(X,F,Y)  g_selftest.utils->TestSet(X,Y)

#define ST_TEMPBUFFER_SIZE 1024

extern "C" {
	// Make it easy to place breakpoints here, even when running old
	// GDB versions. :-)
	void testsuite_break_here();
}

class Window;
class OpFile;

#include "modules/selftest/src/testoutput.h"
#include "modules/selftest/src/selftestmemorycleanuputil.h"
#include "modules/opdata/OpData.h"
#include "modules/opdata/UniString.h"

/**
 * A set of two patterns that tests are matched against. The first pattern matches
 * the test module name and the second matches the test name.
 *
 * The following wildcard symbols are supported in the pattern strings:
 *
 * '?' - matches a single char
 * '*' - matches a string
 *
 * The class also works as a listitem in a linked list of patterns. Deleting the
 * first item will cause the whole list to be removed and deleted.
 */
class TestSuitePattern
{
	TestSuitePattern *next;
	char *format, *format2;
	BOOL matched;

	/**
	 * Low-level glob. Used internally by glob().
	 */
	BOOL low_glob(const char *s, int sl, const char *f, int fl, int j, int i);

	/**
	 * Glob. Checks if the data matches the format string.
	 *
	 * @param format The format to match data againts
	 * @param data The data to match
	 * @return TRUE if the data matches the format, FALSE otherwise.
	 */
	BOOL glob( const char *format, const char *data );

public:
	/**
	 * Simple constructor for the TestSuitePattern class.
	 *
	 * @param _format The module pattern
	 * @param _format2 The group pattern
	 */
	TestSuitePattern( const char *_format, const char *_format2 );

	/**
	 * Destroys the test suite pattern and deletes the next pattern in the list.
	 */
	~TestSuitePattern( );

	/**
	 * Adds a test suite pattern to the end of the list.
	 *
	 * @param p The pattern to add
	 */
	void add_pattern( TestSuitePattern *p );

	/**
	 * Matches two string against the respective patterns. If the second argument
	 * is NULL only the first are matched.
	 *
	 * @param first The first string to match
	 * @param second The second string to match
	 * @return TRUE is the strings matches, FALSE otherwise.
	 */
	BOOL match( const char *first, const char *second = 0 );

	/**
	 * Return the module pattern.
	 */
	const char* module_pattern() const { return format; }

	/**
	 * Return the group pattern.
	 */
	const char* group_pattern() const { return format2; }

	/**
	 * Get the next pattern in the list
	 * @return Pointer to next pattern in the list or NULL at the end of the list
	 */
	TestSuitePattern* get_next() const { return next; }

	/**
	 * @return TRUE if this pattern has previously matched anything
	 */
	BOOL did_match() const { return matched; }
};

/**
 * Selftests API towards the generated tests. Contains utility functions for running tests
 * and reporting the results.
 */
class TestUtils
{
	/**
	 * Creates a new window for running selftests in.
	 */
	Window *CreateNewWindow();

	/**
	 * Outputs a description of the test before it is run.
	 *
	 * @param t	The name of the test
	 */
	void TestDescribe( const char *t );

	/**
	 * Creates a key from the group name and the test name that can be used to
	 * uniquely identify tests.
	 *
	 * @param a The group name
	 * @param b The test name
	 * @return A unique key constructed by a and b
	 */
	const uni_char *MakeKey( const char *a, const char *b );
	uni_char *keyspace;							///< Temporary storage for MakeKey()
	int keyspace_len;							///< Length of keyspace

	bool  run_manual_tests;
	const char *current_module;
	const char *current_group;					///< Name of current group
	const char *current_test;					///< Name of current test
	const char *current_file;					///< Name of current testfile

	char *decompress_buffer_uni;
	int decompress_buffer_uni_size;
	char *decompress_buffer;
	int decompress_buffer_size;

	class AsyncTestDone *async_test_done;
	friend class SelftestSubThreadListener;
	friend class TS_CheckSelfTest;
	OpAutoPointerHashTable<void, void*> threads_listened; ///< Tracks which threads have a SelftestSubThreadListener listener

	OpGenericStringHashTable passed;
	int always_verbose;							///< Always run in verbose mode

	int delay_counter;

	TestSuitePattern       *patterns;			///< List of patterns.
	TestSuitePattern       *mpatterns;			///< List of module patterns.
	TestSuitePattern       *npatterns;			///< List of exclude patterns.

	char pathbuff[ST_TEMPBUFFER_SIZE];
	char tmpbuffs[ST_NUM_TEMP_BUFFERS][ST_TEMPBUFFER_SIZE];		///< Temporaty buffers used by ST_down and ST_up.
	int buff;									///< The temporary buffer currently being used.

	double timer;

	SelftestMemoryCleanupUtil mem_cleanup_util;
public:
	enum PatternType {
		PATTERN_NORMAL,							///< Include tests matching group name and test name.
		PATTERN_EXCLUDE,						///< Exclude tests matching group name and test name.
		PATTERN_MODULE							///< Include tests matching module.
	};

	TestOutput *output;
	FramesDocument *doc;						///< The current test document. The value of this property can only be reliably read during tests.
	int				failed_tests;				///< Number of failed tests.
	int				delayed;					///< The current test is a delayed test.
	BOOL			running_async;				///< Is the current test asynchronous.
	BOOL			expect_fail;				///< If the current test is expected to fail
	BOOL			allow_exceptions;			///< If excetpions thrown by the current async test should nt be regarded as an error
	/** Initialised to FALSE for each test (by TestSet()). A TestPassed() or
	 * TestFailed() result will set this to TRUE. A second TestPassed() or
	 * TestFailed() will not report the "PASSED" or "FAILED" status again. */
	BOOL			has_test_result;
	int last_test_index;
	Window *window;

	/**
	 * A simple constructor for the TestUtils class.
	 */
	TestUtils();

	/**
	 * Destructor
	 */
	~TestUtils();

	/**
	 * Returns a temporary buffer for use in string conversions and such. The buffer
	 * returned will cycle every ST_NUM_TEMP_BUFFERS times. This method is intended
	 * for internal use only.
	 *
	 * @return A temporary buffer.
	 */
	char* GetTempBuf();
	char* GetPathBuf();

	/**
	 * Decompresses a selftest string.
	 *
	 * @param input The compressed string
	 * @param in_len Length of the compressed string
	 * @param len Length of the target string
	 * @param is_uni TRUE if the compressed string is unicode, FALSE otherwise
	 * @return Pointer to the decompressed string
	 */
	const char *DecompressString( const unsigned char *input, int in_len, int len, BOOL is_uni );

	/**
	 * Adds a test suite pattern used to check which tests to run.
	 *
	 * @param a The module pattern
	 * @param b The test pattern
	 * @param pattern_type Type of pattern
	 */
	void AddPattern( const char *a, const char *b, PatternType pattern_type );

	/**
	 *
	 */
	OP_STATUS ParsePatterns(const char* str, int &patters_read, TestUtils::PatternType pattern_type);

	/**
	 * Match a set of string to the test patterns. Does only try to match arguments actually
	 * supplied. Any of the arguments could be NULL.
	 *
	 * @param module Module name.
	 * @param group Group name.
	 * @param test Test name.
	 * @return TRUE if match, FALSE otherwise.
	 */
	BOOL MatchPatterns( const char* module, const char* group, const char* test );

	/**
	 * Sets if selftest should run in manual mode.
	 *
	 * @param do_run_manual The new status of run manual
	 */
	void RunManual( bool do_run_manual );

	/**
	 * Sets if selftest should run in verbose mode.
	 *
	 * @param newval The new status of verbose
	 * @return The old status of verbose
	 */
	int SetVerbose( int newval );


	/**
	 * Called when a test has failed. Adds the test to a list of failed tests and output a failed
	 * message to the selftest output.
	 *
	 * @param file	File the test is located in
	 * @param line	Line the test is located at
	 * @param ttst  Output string
	 * @param ...	Output string parameters
	 */
	void TestFailed( const char *file, int line, const char *ttst, ... );
	void TestFailedWithStatus( const char *file, int line, const char *ttst, OP_STATUS status);

	/**
	 * Checks if a test has been run successfully in the current run. Used to check test
	 * dependencies.
	 *
	 * @param test The test to check
	 */
	int TestWasOk( const char* test );

	/**
	 * Initializes all member variables.
	 */
	void InitL();

	/**
	 * Deletes all allocated memory and resets all member variables.
	 */
	void Reset();

	/**
	 * Compares two strings. Used to test uni_strcmp amongst others.
	 *
	 * @param s1 First string to compare
	 * @param s2 First string to compare
	 * @return 0 if the strings are equal, non-zero otherwise.
	 */
	int compareStr( const uni_char *s1, const uni_char *s2 )
	{
		if(!s1 || !s2) return 1;
		while (*s1 && *s1 == *s2)
			s1++, s2++;
		return (int) *s1 - (int) *s2;
	}

	/**
	 * Returns the current test window or creates a new window if none exists.
	 */
	Window *GetWindow();

	/**
	 * Returns the file name of the current test file. Will not return NULL if a
	 * selftest is being run.
	 */
	const char* GetCurrentFile() const { return current_file; }

	/**
	 * Returns the name of the current group. Will not return NULL if a
	 * selftest is being run.
	 */
	const char* GetCurrentGroupName() const { return current_group; }

	/**
	 * Returns the name of the current test. Will not return NULL if a
	 * selftest is being run.
	 */
	const char* GetCurrentTestName() const { return current_test; }

	/**
	 * Asks the user if a test has failed or succeeded. Only used in manual test when
	 * running selftest in manual mode.
	 *
	 * <strong>NOTE:</strong> This is not asynchronous and should be rewritten.
	 *
	 * @param file The file the test is located in
	 * @param line The line the test is located at
	 * @param question Question to ask the user
	 */
	void AskUserOk( const char *file, int line, const uni_char *question );

	/**
	 * Checks if a test should be run and set up for the test run.
	 *
	 * @param t Name of the test
	 * @param manual_test The test is manual
	 * @param test_index Index of the test
	 * @retval 1 The test should be run
	 * @retval 0 The test should not be run
	 */
	int  TestSet( const char *t, int manual_test, int test_index = op_rand() );

	/**
	 * Called when a test has been skipped.
	 *
	 * @param t The name of the test
	 * @param reason The reason the test has been skipped
	 * @param do_continue Flag (probably means we should continue)
	 * @return 0 unless do_continue, in which case 1
	 */
	int  TestSkipped( const char *t, const char *reason, bool do_continue=false );

	/**
	 * Called when a test has passed. Add the test to a list of passed tests and outputs a passed
	 * message.
	 */
	void TestPassed( );

	/**
	 * Check if the document used for the next selftest is loaded.
	 * A document is considered loaded if it has been loaded completely,
	 * including inlines, the scriping environment created and the load
	 * event dispatched.
	 * If the previous conditions aren't verified, a delayed step message is sent.
	 */
	BOOL DocumentLoaded();

	/**
	 * Runs an ecmascript test.
	 *
	 * @param async            Run the test in async mode
	 * @param fails            If the test should fail
	 * @param allow_exceptions If TRUE, exceptions will not cause the test to fail.
	 *                         The test needs to call ST_failed() explicitly for that.
	 * @param line_number      Line number of the script in the .ot file.
	 * @param ecmascript       The ecmascript program
	 *
	 * @retval 1 The test has failed to start, the next test should be run
	 * @retval -1 Not started yet, TestEcma has signalled that it should be called again
	 * @retval -2 The test has started, the test will signal when it is done
	 */
	int TestEcma( BOOL async, BOOL fails, BOOL allow_exceptions, unsigned line_number, const uni_char *ecmascript);

	/**
	 * Clears the current test document, and replaces it with a new one.
	 * If you just need a blank document, then pass NULL in all arguments.
	 *
	 * @param url_str  URL name of the new document, which can be used to spoof the origin.
	 *                 No network requests will be done, instead the data will be used.
	 * @param data     utf-8 data to write and parse into the new document.
	 *                 Must not be empty if url_str is also not empty.
	 * @param type     mime-type of the data.
	 * @param data_line_number  Line number in the .ot file of the document block.
	 * @retval  1      The document has loaded, the following test can run.
	 * @retval -1      There was an error loading the url. This function should be
	 *                 called again later.
	 */
	int LoadNewDocument(const char *url_str, const char *data, const char *type, unsigned data_line_number);

	/**
	 * Sets up a group before running tests. Checks if the test should be run and
	 * prints a group header to the selftest output.
	 *
	 * @param t The test group
	 * @param f File the test group is located in
	 * @param l Line the test group is located at
	 * @retval 0 The group should be skipped
	 * @retval 1 The group should be run
	 */
	int GroupSet( const char *t, const char *f, int l );

	/**
	 * Writes a summary of a test run to the selftest output.
	 */
	void Summary();

	/**
	 * Signals to the message handler that we want to run the next step in the
	 * suite.
	 * @param delay Run next step after <strong>delay</strong> ms
	 */
	void AsyncStep(int delay = 0);

	/**
	 * Signals to the message handler to shut down opera without asking.
	 */
	void AsyncStop();

	/**
	 * Check if we should exit after tests
	 * are completed. By separating the test
	 * and the call, we can clean everything
	 * in between
	 * (Moved from reset methods)
	 */
	BOOL ShouldExitAfterCompletedTests();

	/**
	 * Depending on system and callback, we
	 * decide to exit or not
	 *
	 * (Moved from reset methods)
	 */
	void ExitAfterCompletedTests(int exit_code);

	/**
	 * Used by the internal leakchecking functionality. Should not be
	 * called directly;
	 */
	void memSetMarker();
	void memCheckForLeaks();

	/**
	 * Used by the selftest timer functionality. Should not be called directly.
	 */
	void timerStart();
	void timerReset();

	/**
	 * Returns util for managing heap alocated object in async selftests
	 */
	SelftestMemoryCleanupUtil* GetMemoryCleanupUtil() { return &mem_cleanup_util; }

	/**
	 * Print warning for each pattern that has currently not matched anything
	 */
	void WarnAboutNoMatches() const;

	/**
	 * Tells if an ecmascript selftest is running, both
	 * synchronous and asynchronous.
	 */
	BOOL IsRunningEcmascriptTest() const;

	/**
	 * Marks the current running ecmascript test as
	 * having completed.
	 */
	void SetEcmascriptTestDone();
};

/**
 * This class can be used to execute different selftest steps by posting a
 * message to the main message handler. You can inherit this class, implement
 * the virtual abstract method OnNextStep() and call NextStep() whenever your
 * test shall execute another async test-step.
 *
 * Example:
 * @code
 * global
 * {
 *     class MyTest : public TestStep
 *     {
 *         enum MySteps {
 *             STEP_0, STEP_1, STEP_2, STEP_TIMEOUT
 *         };
 *     public:
 *         MyTest() {}
 *         virtual ~MyTest() {}
 *         OP_STATUS Init()
 *             {
 *                 RETURN_IF_ERROR(TestStep::Init());
 *                 NextStep(STEP_0);
 *                 NextStep(STEP_TIMEOUT, 1000);
 *             }
 *         virtual void OnNextStep(unsigned int step)
 *             {
 *                 switch (step) {
 *                 case STEP_0: NextStep(STEP_1); break;
 *                 case STEP_1: NextStep(STEP_2); break;
 *                 case STEP_2: ST_passed(); break;
 *                 case STEP_TIMEOUT: ST_failed("test timed out after 1s"); break;
 *                 default: ST_failed("Unknown step");
 *                 }
 *             }
 *     };
 * }
 *
 * test("simple test step") async;
 * {
 *     MyTest* test = OP_NEW(MyTest, ());
 *     if (!test)
 *         ST_failed("Could not create MyTest");
 *     else if (OpStatus::IsError(MyTest::Init()))
 *         ST_failed("Could not start test");
 *     else
 *         ST_delete_after_group(test);
 * }
 * @endcode
 */
class TestStep : public MessageObject
{
public:
	/** Removes any pending delayed MSG_SELFTEST_STEP for this instance and
	 * unsets this instance as callback for that message. */
	virtual ~TestStep()
		{
			g_main_message_handler->RemoveDelayedMessages(MSG_SELFTEST_STEP, reinterpret_cast<MH_PARAM_1>(this));
			g_main_message_handler->UnsetCallBack(this, MSG_SELFTEST_STEP);
		}

	/** Registers itself as message-handler for the MSG_SELFTEST_STEP
	 * message. That message is posted on calling NextStep() and handled in
	 * HandleCallback() by calling OnNextStep(). */
	OP_STATUS Init()
		{
			RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_SELFTEST_STEP, reinterpret_cast<MH_PARAM_1>(this)));
			return OpStatus::OK;
		}

	/** Can be called to post another MSG_SELFTEST_STEP. This will cause a call
	 * to OnNextStep() with the specified step when the next messages are
	 * handled in the message-loop.
	 *
	 * @note It is possible to call NextStep() multiple times (with the same or
	 *  different step arguments). Each call to NextStep() will cause a call to
	 *  OnNextStep() with the corresponding argument. That can e.g. be used to
	 *  add a timeout for the test by calling NextStep() with a delay and handle
	 *  that step in OnNextStep() by calling ST_failed().
	 *
	 * @param step is a caller-defined identifier which is passed on to the
	 *  method OnNextStep(). An implementation of this class can thus identify
	 *  which step to execute. This could e.g. be a counter or an enum.
	 * @param delay specifies a delay in ms. OnNextStep() is not called before
	 *  this delay has passed.
	 */
	void NextStep(unsigned int step=0, unsigned long delay=0)
		{
			g_main_message_handler->PostDelayedMessage(MSG_SELFTEST_STEP, reinterpret_cast<MH_PARAM_1>(this), static_cast<MH_PARAM_2>(step), delay);
		}

	/**
	 * This method is called on handling the message that was posted by a call
	 * to NextStep().
	 * @param step is the same argument that was used on calling NextStep().
	 */
	virtual void OnNextStep(unsigned int step) = 0;

	/** @name Implementation of MessageObject
	 * @{ */

	/** Called when this instance has received a message. This implementation
	 * handles only the message MSG_SELFTEST_STEP by calling OnNextStep(). */
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
		{
			if (msg == MSG_SELFTEST_STEP)
			{
				OP_ASSERT(reinterpret_cast<MH_PARAM_1>(this) == par1);
				OnNextStep(static_cast<unsigned int>(par2));
			}
		}

	/** @} */ // Implementation of MessageObject
};

/**
 * Performs a string comparison test and returns an object
 * that represents that result
 */
class StringCompareTest
{
	static const char* CStr(const OpStringC8& s)
	{
		return s.IsEmpty() ? "" : s.CStr();
	}

	static const uni_char* CStr(const OpStringC16& s)
	{
		return s.IsEmpty() ? UNI_L("") : s.CStr();
	}

public:
	StringCompareTest(const char* a, const char* b)       { Init(a, b); }
	StringCompareTest(const char* a, const uni_char* b)   { Init(a, b); }
	StringCompareTest(const char* a, const OpStringC8& b) { Init(a, CStr(b)); }
	StringCompareTest(const char* a, const OpStringC16& b){ Init(a, CStr(b)); }
	StringCompareTest(const char* a, Str::LocaleString b);
	StringCompareTest(const char* a, const OpData& b)     { Init(a, b.Data(true)); }
	StringCompareTest(const char* a, const UniString& b)  { Init(a, b.Data(true)); }

	StringCompareTest(const uni_char* a, const uni_char* b)   { Init(a, b); }
	StringCompareTest(const uni_char* a, const char* b)       { Init(a, b); }
	StringCompareTest(const uni_char* a, const OpStringC8& b) { Init(a, CStr(b)); }
	StringCompareTest(const uni_char* a, const OpStringC16& b){ Init(a, CStr(b)); }
	StringCompareTest(const uni_char* a, Str::LocaleString b);
	StringCompareTest(const uni_char* a, const OpData& b)     { Init(a, b.Data(true)); }
	StringCompareTest(const uni_char* a, const UniString& b)  { Init(a, b.Data(true)); }

	StringCompareTest(const OpStringC8& a, const char* b)       { Init(CStr(a), b); }
	StringCompareTest(const OpStringC8& a, const uni_char* b)   { Init(CStr(a), b); }
	StringCompareTest(const OpStringC8& a, const OpStringC8& b) { Init(CStr(a), CStr(b)); }
	StringCompareTest(const OpStringC8& a, const OpStringC16& b){ Init(CStr(a), CStr(b)); }
	StringCompareTest(const OpStringC8& a, Str::LocaleString b);
	StringCompareTest(const OpStringC8& a, const OpData& b)     { Init(CStr(a), b.Data(true)); }
	StringCompareTest(const OpStringC8& a, const UniString& b)  { Init(CStr(a), b.Data(true)); }

	StringCompareTest(const OpStringC16& a, const char* b)       { Init(CStr(a), b); }
	StringCompareTest(const OpStringC16& a, const uni_char* b)   { Init(CStr(a), b); }
	StringCompareTest(const OpStringC16& a, const OpStringC8& b) { Init(CStr(a), CStr(b)); }
	StringCompareTest(const OpStringC16& a, const OpStringC16& b){ Init(CStr(a), CStr(b)); }
	StringCompareTest(const OpStringC16& a, Str::LocaleString b);
	StringCompareTest(const OpStringC16& a, const OpData& b)     { Init(CStr(a), b.Data(true)); }
	StringCompareTest(const OpStringC16& a, const UniString& b)  { Init(CStr(a), b.Data(true)); }

	StringCompareTest(const OpData& a, const char* b)       { Init(a.Data(true), b); }
	StringCompareTest(const OpData& a, const uni_char* b)   { Init(a.Data(true), b); }
	StringCompareTest(const OpData& a, const OpStringC8& b) { Init(a.Data(true), CStr(b)); }
	StringCompareTest(const OpData& a, const OpStringC16& b){ Init(a.Data(true), CStr(b)); }
	StringCompareTest(const OpData& a, const OpData& b)     { Init(a.Data(true), b.Data(true)); }
	StringCompareTest(const OpData& a, const UniString& b)  { Init(a.Data(true), b.Data(true)); }

	StringCompareTest(const UniString& a, const char* b)       { Init(a.Data(true), b); }
	StringCompareTest(const UniString& a, const uni_char* b)   { Init(a.Data(true), b); }
	StringCompareTest(const UniString& a, const OpStringC8& b) { Init(a.Data(true), CStr(b)); }
	StringCompareTest(const UniString& a, const OpStringC16& b){ Init(a.Data(true), CStr(b)); }
	StringCompareTest(const UniString& a, const OpData& b)     { Init(a.Data(true), b.Data(true)); }
	StringCompareTest(const UniString& a, const UniString& b)  { Init(a.Data(true), b.Data(true)); }

	const char* m_lhs;
	const char* m_rhs;

	int m_result;
private:

	void Init(const char* a, const char* b);
	void Init(const uni_char* a, const uni_char* b);
	void Init(const char* a, const uni_char* b);
	void Init(const uni_char* a, const char* b);
	void Init(const char* a, Str::LocaleString b);
	void Init(const uni_char* a, Str::LocaleString b);

	void FillIn(const char* a, const char* b, int result);

	StringCompareTest(){}
	enum{ DOWNSIZE_BUF_SIZE = 512 };
	char m_downsize_buf[DOWNSIZE_BUF_SIZE]; /* ARRAY OK joaoe 2009-11-12 */
};


/** Compares two files, binary.
 */
class FileComparisonTest
{
public:
	FileComparisonTest(const uni_char* reference_filename, const uni_char* tested_filename);

	OP_BOOLEAN CompareBinary();

	const char* TestedFilenameUTF8() { return m_tested_filename8; }
	const char* ReferenceFilenameUTF8() { return m_reference_filename8; }

private:
	void Init();

	const uni_char* m_tested_filename;
	const uni_char* m_reference_filename;
	char* m_tested_filename8;
	char* m_reference_filename8;

	// Not enough for long filenames made all of UTF8 chars but sane.
	enum { FILENAMES_BUF_SIZE = 2048 };
	char m_filenames_buffer[FILENAMES_BUF_SIZE];
};

/**
 * Trim the prefix of a path.
 *
 * @param x Path to trim
 * @return A trimmed path
 */
const char *ST_trim_fn( const char *x );

/**
 * Convert a string from unicode. The call does not allocate any memory. The result is instead
 * stored in three static buffer. The returned memory will be overwritten every 3 call to
 * ST_down or ST_up.
 *
 * @param x The unicode string to convert
 * @return The converted string
 */
const char *ST_down( const uni_char *x );

/**
 * Convert a string to unicode. The call does not allocate any memory. The result is instead
 * stored in three static buffer. The returned memory will be overwritten every 3 call to
 * ST_down or ST_up.
 *
 * @param x The string to convert
 * @return The converted unicode string
 */
const uni_char *ST_up( const char *x );
template<typename T> static inline T& ST_down( T &x ) { return x; }
static inline char *ST_down( uni_char *x ){ return (char *)ST_down( (const uni_char *)x ); }

// FIXME: This should return const as soon as all test compile with this.
/*const*/ char *ST_make_path( const char* x );
const uni_char *ST_uni_make_path( const char* x );

/**
 * Pass the current test, used in ecmascript and asynchronous tests.
 */
void ST_passed();

/**
 * Fail the current test, used in ecmascript and asynchronous tests.
 *
 * @param fmt Error string
 * @param ... Error string parameters
 */
void ST_failed( const char *fmt, ... );


void ST_failed_with_status(const char *comment, OP_STATUS status, int line);

/** Schedules object to be deleted after selftest group
 *
 * @param object - object which will be scheduled for deletion. If this macro fails(OOM)
 * then the test is failed immidietly. This function fails also if object is NULL, so that
 * NULL check can be ommited.
 * NOTE1: As deallocation uses OP_DELETE object must be allocted by OP_NEW
 * (not malloc, or array new allocator)
 * NOTE2: this will properly report OOM's which are not handled a bit differently then
 * normal fails by SPARTAN
 */
#define ST_delete_after_group(object)                                                                             \
do {                                                                                                              \
	OP_STATUS error = g_selftest.utils->GetMemoryCleanupUtil()->DeleteAfterTest(object);                          \
	if (OpStatus::IsError(error))                                                                                 \
	{                                                                                                             \
		ST_failed_with_status("Scheduling object for deletion failed", OpStatus::ERR_NO_MEMORY, __LINE__); \
		return -2;                                                                                                \
	}                                                                                                             \
} while(FALSE)

/**
 * Output a formatted string to the selftest output. Usable in the actual tests.
 * Mostly intended for debugging.
 *
 * @param fmt Format string
 * @param ... Format string parameters
 */
void output( const char *fmt, ... );

/**
 * Called when a compare test has failed. Outputs the failure to the selftest output
 * and adds it to the failed report.
 *
 * @param file File the test is located in
 * @param line Line the test is located at
 * @param a First test value
 * @param b Second test value
 * @param lhs Left hand side text
 * @param rhs Right hand side text
 * @param compare_txt Text used in the failed message
 * @param one_only The test has only one value
 */
void _compare_failure( const char *file, int line, const char *a, const char *b,
					   const char *lhs, const char *rhs, const char *compare_txt,
					   int one_only );

/**
 * Try to find the n:th HTML element of a specific type in the current test document.
 *
 * @param elem The type of element to find
 * @param nelm Which element in order to find, 2 means second element of type elem etc
 * @return The n:th matching element. NULL if it could not be found.
 */
class HTML_Element *find_element( const uni_char *elem, int nelm=1 );

/**
 * @overload
 */
class HTML_Element *find_element( const char *elem, int n=1 );

/**
 * Try to find the HTML element with a specific id the current test document.
 *
 * @param id id for the element to find
 * @return The matching element. NULL if it could not be found.
 */
class HTML_Element *find_element_id( const uni_char *id );

/**
 * @overload
 */
class HTML_Element *find_element_id( const char *id );

/**
 * Checks whether two rects are equal, allow some error (typically one),
 * because of rounding issues with scaling and transforms.
 * @param first rect to be compared
 * @param second rect to be compared
 * @param error allow that much difference between the top left and bottom right coordinates
 * @return TRUE when the rectangles are considered equal with the respect to the given error
 */
inline BOOL rects_equal_with_error(const OpRect& first, const OpRect& second, int error = 1)
{
	return op_abs(first.x - second.x) <= error && op_abs(first.y - second.y) <= error &&
		op_abs(first.Right() - second.Right()) <= error && op_abs(first.Bottom() - second.Bottom()) <= error;
}

#define st_rects_equal(first, second) rects_equal_with_error(first, second)

/**
 * Compare the variables in a test, overloaded to support several types of indata.
 * Used by the test_equal() type tests. Report if the test is successful or not.
 *
 * @param file File the test is located in
 * @param line Line the test is located at
 * @param a First test value
 * @param b Second test value
 * @param lhs Left hand side text
 * @param rhs Right hand side text
 * @param one_only The test hasn only one value
 * @param compare_txt Text used in the failed message
 * @param invert The test is inverted. i.e. it should report success only
 *				 if it fails.
 */
int _compare_test( const char *file, int line,
				   const uni_char *a, const uni_char *b,
				   const char *lhs, const char *rhs, int one_only,
				   const char *compare_txt, int invert );

/**
 * @overload
 */
int _compare_test( const char *file, int line, const char *a, const char *b, const char *lhs,
				   const char *rhs, int one_only, const char *compare_txt, int invert );

/**
 * @overload
 */
int _compare_test( const char *file, int line,
				   double a, double b,
				   const char *lhs, const char *rhs, int one_only,
				   const char *compare_txt, int invert );

/**
 * @overload
 */
int _compare_test( const char *file, int line,
				   float a, float b,
				   const char *lhs, const char *rhs, int one_only,
				   const char *compare_txt, int invert );

/**
 * @overload
 */
int _compare_test( const char *file, int line,
				   unsigned int a, unsigned int b,
				   const char *lhs, const char *rhs, int one_only,
				   const char *compare_txt, int invert );

/**
 * @overload
 */
int _compare_test( const char *file, int line,
				   int a, int b,
				   const char *lhs, const char *rhs, int one_only,
				   const char *compare_txt, int invert );

/**
 * @overload
 */
int _compare_test( const char *file, int line,
				   unsigned long a, unsigned long b,
				   const char *lhs, const char *rhs, int one_only,
				   const char *compare_txt, int invert );

/**
 * @overload
 */
int _compare_test( const char *file, int line,
				   long a, long b,
				   const char *lhs, const char *rhs, int one_only,
				   const char *compare_txt, int invert );

/**
 * @overload
 */
int _compare_test( const char *file, int line,
				   void *a, void *b,
				   const char *lhs, const char *rhs, int one_only,
				   const char *compare_txt, int invert );

#endif // SELFTEST
#endif // TESTSUITE_TESTUTILS_H
