/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef DOM_SELFTEST_DOMSELFTESTUTIL_H
#define DOM_SELFTEST_DOMSELFTESTUTIL_H

#ifdef SELFTEST

#include "modules/ecmascript_utils/esasyncif.h"

/** Helper class for testing JS in C++ asynchronously
 *
 * I guess this would fir ecmascipt_utils module better...
*/
class DOM_SelftestUtils
{
public:
	/** Call function asynchronously and test if it returns correct values
	 *
	 * @param function - EcmaScript function object which will be called. MUST NOT be NULL. Must be added to some ES_Runtime.
	 * @param this_obj - will be used as a this in called function. If NULL it will use global object from function's runtime.
	 * @param argc - number of arguments passed to the function.
	 * @param argv - array of arguments passed to the function.
	 * @param expected_status - expected way of function finishing - see ES_AsyncStatus for possible statuses.
	 * @param expected_result - expected result of function if it succeeded. This does't doesn't do any smart comparison if it is of type
	 *		VALUE_OBJECT- just compares pointers to ES_Object (would be niece to implement something better though)
	 * @param line - line to report if the selftest fails
	 */
	static void TestAsyncFunction(ES_Object* function, ES_Object* this_obj, int argc, ES_Value* argv, ES_AsyncStatus expected_status, const ES_Value &expected_result, int line);

	/** Evaluate piece of EcmaScript asynchronously and test if it returns correct values
	 *
	 * @param program - string to evaluate. MUST NOT be NULL,
	 * @param expected_status - expected way of program finishing - see ES_AsyncStatus for possible statuses.
	 * @param expected_result - expected result of program if it succeeded. This does't doesn't do any smart comparison if it is of type
	 *		VALUE_OBJECT- just compares pointers to ES_Object (would be niece to implement something better though)
	 * @param line - line to report if the selftest fails
	 */
	static void TestAsyncEval(const uni_char* program, ES_AsyncStatus expected_status, const ES_Value &expected_result, int line);
private:
	class SelftestAsyncCalback : public ES_AsyncCallback
	{
	public:
		SelftestAsyncCalback(ES_AsyncOperation expected_operation, ES_AsyncStatus expected_status, const ES_Value &expected_result);
		virtual OP_STATUS HandleCallback(ES_AsyncOperation operation, ES_AsyncStatus status, const ES_Value &result);
	private:
		ES_AsyncOperation m_expected_operation;
		ES_AsyncStatus m_expected_status;
		ES_Value m_expected_result;
	};
};
#endif // SELFTEST

#endif // DOM_SELFTEST_DOMSELFTESTUTIL_H
