/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef OP_SCOPE_JSCONSOLE_LISTENER_H
#define OP_SCOPE_JSCONSOLE_LISTENER_H

#ifdef SCOPE_ECMASCRIPT_DEBUGGER

class ES_Value;
class ES_Runtime;

/**
 * An interface for receiving events from JS_Console in the
 * DOM module.
 */
class OpScopeJSConsoleListener
{
public:

	/**
	 * @see OpScopeJSConsoleListener::ConsoleLog
	 */
	enum LogType
	{
		LOG_INVALID = 0,
		LOG_LOG = 1,
		LOG_DEBUG,
		LOG_INFO,
		LOG_WARN,
		LOG_ERROR,
		LOG_ASSERT,
		LOG_DIR,
		LOG_DIRXML,
		LOG_GROUP,
		LOG_GROUP_COLLAPSED,
		LOG_GROUP_END,
		LOG_COUNT,
		LOG_TABLE,
		LOG_CLEAR
	};

	/**
	 * Call this function when any of the following function calls occur in a script.
	 *
	 *  - console.log
	 *  - console.debug
	 *  - console.info
	 *  - console.warn
	 *  - console.error
	 *  - console.assert
	 *  - console.dir
	 *  - console.dirxml
	 *  - console.group
	 *  - console.groupCollapsed
	 *  - console.groupEnd
	 *  - console.count
	 *  - console.table
	 *  - console.clear
	 *
	 * Set the \a type accordingly. In the \a LOG_ASSERT case, you should only call this
	 * function if the assertion failed (i.e. if the first arguments is false).
	 *
	 * @param runtime The runtime which just called one of the above functions.
	 * @param type Which of the possible functions that was called in the script.
	 * @param argv The arguments passed to the function in the script.
	 * @param argc The number of arguments in \a argv.
	 */
	static void OnConsoleLog(ES_Runtime *runtime, LogType type, ES_Value* argv, int argc);

	/**
	 * Call this function from console.trace. Scope will use the ecmascript debugger to
	 * get the current call stack of the specified runtime, and send this information to
	 * the client.
	 *
	 * @param runtime The runtime which just called console.trace.
	 */
	static void OnConsoleTrace(ES_Runtime *runtime);

	/**
	 * Call this function from console.time. A timer will be started with the specified title,
	 * and will keep running until stopped by console.timeEnd. A message will be sent to the
	 * client (if any) that a timer was started.
	 *
	 * @param runtime The runtime which just called console.time.
	 * @param title A name for the timer. When stopping the timer later with
	 *              console.timeEnd, the same name must be used. Can not be NULL.
	 */
	static void OnConsoleTime(ES_Runtime *runtime, const uni_char *title);

	/**
	 * Call this function from console.timeEnd. A timer with the specified will be,
	 * stopped, and a message will be sent to the client with the time difference.
	 *
	 * @param runtime The runtime which just called console.timeEnd.
	 * @param title The name used when creating the timer. If no timer with the
	 *              specified title exists, noting happens. Can not be NULL.
	 */
	static void OnConsoleTimeEnd(ES_Runtime *runtime, const uni_char *title);

	/**
	 * Call this function from console.profile. Scope will start profiling,
	 * and keep doing so until ConsoleProfileEnd is called. A message will be
	 * sent to the client, letting it know that profiling begun.
	 *
	 * @param runtime The runtime which just called console.profile.
	 * @param title A name for the profile. Because just one profiler can be
	 *              run at any time, this is not required, and may be NULL.
	 */
	static void OnConsoleProfile(ES_Runtime *runtime, const uni_char *title);

	/**
	 * Call this function from console.profileEnd. Scope will stop the running
	 * profiler, and sent a report to the client. If no profiler was previously
	 * created, nothing happens.
	 *
	 * @param runtime The runtime which just called console.profileEnd.
	 */
	static void OnConsoleProfileEnd(ES_Runtime *runtime);

	/**
	 * Call this function when a 'console.exception' occurs in a script.
	 *
	 * @param runtime The runtime which just called 'console.exception'.
	 * @param argv The arguments passed to the function in the script.
	 * @param argc The number of arguments in \a argv.
	 */
	static void OnConsoleException(ES_Runtime *runtime, ES_Value* argv, int argc);

}; // OpScopeJSConsoleListener

#endif // SCOPE_ECMASCRIPT_DEBUGGER

#endif // OP_SCOPE_JSCONSOLE_LISTENER_H
