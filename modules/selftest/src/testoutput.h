/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4;  -*-
**
** Copyright (C) 2002-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** 
*/

#ifndef SELFTEST_TESTOUTPUT_H_
#define SELFTEST_TESTOUTPUT_H_

#ifdef SELFTEST

class OpFile;

class TestOutput
{
public:
	TestOutput(BOOL force_stdout = FALSE);
	OP_STATUS Construct(char* output_file);

	virtual ~TestOutput();

	virtual void GroupHeader(const char* file_name, unsigned int line, const char* group_name) = 0;
	virtual void TestDescribe(const char* module_name, const char* group_name, const char* test_name) = 0;
	virtual void TestFailed(const char* file_name, int line, const char* fmt, va_list args) = 0;
	virtual void TestFailedWithStatus(const char* file_name, int line, const char* fmt, OP_STATUS status) = 0;
	virtual void TestSkipped(const char* module_name, const char* group_name, const char* test_name, const char* reason) = 0;
	virtual void Leak(unsigned count) = 0;
	virtual void Timer(unsigned ms) = 0;
	virtual void Warning( const char *fmt, ...);

	virtual void Passed() = 0;
	virtual void Failed(const char* reason = NULL, const char* file = NULL, int line = -1, va_list* args = NULL) = 0;
	virtual void FailedWithStatus(const char* reason, const char* file, int line, OP_STATUS status) = 0;
	
	virtual void Summary() = 0;

	virtual void PublicOutputFormatted( const char *file, int line, int verbose, const char *fmt, ...) = 0;
	virtual void PublicOutputFormattedv( const char *file, int line, int verbose, const char *fmt, va_list args) = 0;
	
protected:
	/**
	 * Outputs a formatted string to the selftest output.
	 *
	 * @param file File the test is located in
	 * @param line Line the test is located at
	 * @param verbose Outputs even if always verbose is not set
	 * @param fmt Format string
	 * @param ... Format string parameters
	 */
	void OutputFormatted( const char *file, int line, int verbose, const char *fmt, ... );
	void OutputFormatted( const char *fmt, ... );
	
	/**
	 * Outputs a formatted string to the selftest output.
	 *
	 * @param file File the test is located in
	 * @param line Line the test is located at
	 * @param verbose Outputs even if always verbose is not set
	 * @param fmt Format string
	 * @param args Format string parameters
	 */
	void OutputFormattedv( const char *file, int line, int verbose, const char *fmt, va_list args );

	void Output( const char* txt );

	/**
	 * Converts some known values of OpStatus to their string representation,
	 * else returns the integer stringified
	 */
	const char* OpStatusToDescription(OP_STATUS status);

private:
	OpFile *output_file;
	BOOL 
		force_stdout,
		always_verbose;
	char m_op_status_desc[64];
};


/**
 * Standard human-readable output.
 */
class StandardTestOutput
	: public TestOutput
{	
public:
	StandardTestOutput();

	virtual void GroupHeader(const char* file_name, unsigned int line, const char* group_name);
	virtual void TestDescribe(const char* module_name, const char* group_name, const char* test_name);
	virtual void TestFailed(const char* file_name, int line, const char* fmt, va_list args);
	virtual void TestFailedWithStatus(const char* file_name, int line, const char* fmt, OP_STATUS status);
	virtual void TestSkipped(const char* module_name, const char* group_name, const char* test_name, const char* reason);
	virtual void Leak(unsigned count);
	virtual void Timer(unsigned ms);
	virtual void Passed();	
	virtual void Failed(const char* reason = NULL, const char* file = NULL, int line = -1, va_list* args = NULL);
	virtual void FailedWithStatus(const char* reason, const char* file, int line, OP_STATUS status);
	virtual void Summary();

	virtual void PublicOutputFormatted( const char* file, int line, int verbose, const char* fmt, ...);
	virtual void PublicOutputFormattedv( const char *file, int line, int verbose, const char *fmt, va_list args);

private:
	/**
	 * Used to incrementaly build a report over failed tests and print it to the
	 * selftest output.
	 */
	class FailedReport
	{
		char *report;			///< Pointer to start of report 
		char *insertion_point;	///< Pointer to where 
		unsigned int allocated;	///< Number or bytes currently allocated for report
	public:
		/**
		 * Simple constructor for the FailedReport class.
		 */ 
		FailedReport() : report(NULL), insertion_point(NULL), allocated(0) {}
		~FailedReport() { OP_DELETEA(report); }
		
		/**
		 * Adds a formatted string to the failed report.
		 *
		 * @param file	File the failed test is located in
		 * @param line	Line the failed test is located at
		 * @param format Format string
		 * @param args	Format string parameters
		 */
		OP_STATUS Add( const char *file, int line, const char *format, va_list args );
		
		/**
		 * Adds a string to the failed report.
		 *
		 * @param file	File the failed test is located in
		 * @param line	Line the failed test is located at
		 * @param txt	String to add to the failed report	
		 */
		OP_STATUS Add( const char *file, int line, const char *txt );
		
		/**
		 * Returns a summary over failed tests. May return NULL if the report is empty.
		 */
		const char* GetReport() const;
	};

	FailedReport failed_report;
	
	int num_tests;
	int num_skipped;
	int num_failed_tests;
};


class SpartanTestOutput
	: public TestOutput
{
	virtual void TestDescribe(const char* module_name, const char* group_name, const char* test_name);	
	virtual void TestSkipped(const char* module_name, const char* group_name, const char* test_name, const char* reason);
	virtual void Passed();
	virtual void Failed(const char* reason = NULL, const char* file = NULL, int line = -1, va_list* args = NULL);
	virtual void FailedWithStatus(const char* reason, const char* file, int line, OP_STATUS status);

	// Empty overrides	
	virtual void GroupHeader(const char* /*file_name*/, unsigned int /*line*/, const char* /*group_name*/) { /* do nothing */ }
	virtual void TestFailed(const char* /*file_name*/, int /*line*/, const char* /*fmt*/, va_list /*args*/) { /* do nothing */ }
	virtual void TestFailedWithStatus(const char* file_name, int line, const char* fmt, OP_STATUS status) { /* do nothing */ }
	virtual void Summary();
	virtual void Leak(unsigned /*count*/) { /* nothing to do */ }
	virtual void Timer(unsigned /*ms*/) { }
	virtual void PublicOutputFormatted( const char* /*file*/, int /*line*/, int /*verbose*/, const char* /*fmt*/, ...)
	{ /* do nothing*/ }
	virtual void PublicOutputFormattedv( const char* /*file*/, int /*line*/, int /*verbose*/, const char* /*fmt*/, va_list /*args*/)
	{ /* do nothing*/ }

};

#endif // SELFTEST
#endif // SELFTEST_TESTOUTPUT_H_
