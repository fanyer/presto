/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef OP_SCOPE_SELFTEST_LISTENER_H
#define OP_SCOPE_SELFTEST_LISTENER_H

#ifdef SELFTEST

/**
 * Use this class to report events related to selftests.
 */
class OpScopeSelftestListener
{
public:

	/**
	 * Call this when output from a selftest or group of
	 * selftests is ready.
	 *
	 * @param output The output from the selftest.
	 * @return OpStatus::OK or Opstatus::ERR_NO_MEMORY.
	 */
	static OP_STATUS OnSelftestOutput(const uni_char *output);

	/**
	 * Call this when the running of selftests have finished.
	 *
	 * Don't call this after *each* selftest completes, but after
	 * the *selection* of tests have finished. If only the scope
	 * module is chosen, for instance, call this when the tests
	 * in the scope module are finished.
	 *
	 * @return OpStatus::OK or Opstatus::ERR_NO_MEMORY.
	 */
	static OP_STATUS OnSelftestFinished();

}; // OpScopeSelftestListener

#endif // SELFTEST

#endif // OP_SCOPE_SELFTEST_LISTENER_H
