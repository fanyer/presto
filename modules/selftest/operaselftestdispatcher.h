/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Anders Oredsson
*/

#if !defined MODULES_SELFTEST_OPERASELFTESTRUNTIME_H && defined SELFTEST
#define MODULES_SELFTEST_OPERASELFTESTRUNTIME_H

#include "modules/ecmascript/ecmascript.h"

/**
 * Dispatcher class for running tests from opera:selftest page.
 * We need a place to store logs / do callbacks from etc, so
 * we create the operaselftestdispatcher.
 *
 * The operaselftestdispatcher must be part of the
 * selftest_module as a "module global", so that we can
 * remember state from one ecma-call to another.
 *
 * The dispatcher will do its best in trying to clean up
 * after use.
 */
class ES_Value;

class OperaSelftestDispatcher{
protected:
	
	BOOL g_is_selftest_dispatcher_running;
	
	//buffer
	uni_char* g_selftest_output_buffer;
	unsigned g_selftest_output_buffer_used;
	unsigned g_selftest_output_buffer_allocated;

	//es state connection
	ES_ObjectReference g_selftest_window;
	ES_ObjectReference g_selftest_callback;
	BOOL g_selftest_output_cb_called;

	//scope state
	BOOL g_selftests_running_from_scope;
	
	/**
	 * Called from opera:selftest page as ecma opera.selftestrun, and will run tests
	 * specified as commandline values.
	 */
	int _es_selftestRun(ES_Value *argv, int argc, ES_Value* /*return_value*/, DOM_Environment::CallbackSecurityInfo *security_info); 

	/**
	 * Called from scope and will run tests of modules
	 *
	 * modules: comma-separated list of module names without spaces, for example: "url,util"
	 * spartan_readable: TRUE to format the output in a format suitable for further machine processing.
	 */
	void _scope_selftestRun(const char* modules, BOOL spartan_readable);

	/**
	 * Called from opera:selftest page as ecma opera.selftestoutput, and will return whats
	 * currently in the output buffer. The testruns and the output is realy async
	 */
	int _es_selftestReadOutput(ES_Value *argv, int argc, ES_Value* /*return_value*/, DOM_Environment::CallbackSecurityInfo *security_info);
	
	/**
	 * Called from scope when scope is trying to read testcase results
	 *
	 * output_buffer_max_size = number of uni_chars
	 *
	 * returns number of uni_chars written
	 */
	size_t _scope_selftestReadOutput(uni_char* output_buffer, size_t output_buffer_max_size);

	/**
	 * Called from testsuite, and will add output to buffers, so that
	 * webpage can access these with opera.selftestoutput(_selftestReadOutput)
	 */
	BOOL _selftestWriteOutput(const char* str);

	/**
	 * Called from testsuite, and will finish the testrun output.
	 * we cant clean anything here, since there could be more async calls
	 * to opera.selftestoutput(_selftestReadOutput)
	 */
	void _selftestFinishOutput();

	/**
	 * Check if dispatcher is running.
	 */
	BOOL _isSelftestDispatcherRunning();

	/**
	 * Check if dispatcher has detected a window while running
	 * (if not, we are probably running from command-line)
	 */
	BOOL _isSelftestConnectedToWindow();

	/**
	 * Check if dispatcher was called from scope
	 * (if not, we are probably running from command-line)
	 */
	BOOL _isSelftestConnectedToScope();

	/**
	 * static callback wrapper for _selftestRun, which is registered on the JS_Opera DOM object
	 */
	static int es_selftestRun(ES_Value *argv, int argc, ES_Value* /*return_value*/, DOM_Environment::CallbackSecurityInfo *security_info);
	
	/**
	 * static callback wrapper for _selftestReadOutput, which is registered on the JS_Opera DOM object
	 */
	static int es_selftestReadOutput(ES_Value *argv, int argc, ES_Value* /*return_value*/, DOM_Environment::CallbackSecurityInfo *security_info);
public:

	/**
	 * Simple constructor, clears everything
	 */
	OperaSelftestDispatcher();

	/**
	 * Simple destructor
	 */
	~OperaSelftestDispatcher();

	/**
	 * Reset data, but will not clean any buffers because
	 * of async callbacks
	 */
	void Reset();

	/**
	 * Register selftest callbacks at the JS_Opera (opera.*) DOM object
     */
	void registerSelftestCallbacks();

	/**
	 * static wrapper for _selftestWriteOutput
	 */
	static BOOL selftestWriteOutput(const char* str);

	/**
	 * static wrapper for _selftestFinishOutput
	 */
	static void selftestFinishOutput();

	/**
	 * simple test for testsuite, so that it can fall back
	 * to old selftest if someone wants to run test from it
	 *
	 * static wrapper for _isSelftestDispatcherRunning
	 */
	static BOOL isSelftestDispatcherRunning();

	/**
	 * helper to check if we have a window (if not, we are command line or scope)
	 *
	 * static wrapper for __isSelftestConnectedToWindow
	 */
	static BOOL isSelftestConnectedToWindow();

	/**
	 * Check if dispatcher was called from scope
	 * (if not, we are probably running from command-line)
	 */
	static BOOL isSelftestConnectedToScope();

	/**
	 * Called from scope and will run tests of modules
	 *
	 * modules: comma-separated list of module names without spaces, for example: "url,util"
	 * spartan_readable: TRUE to format the output in a format suitable for further machine processing.
	 *
	 * static wrapper for _scope_selftestRun
	 */
	static void selftestRunFromScope(const char* modules, BOOL spartan_readable);
};

#endif
