/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_SELFTEST_SELFTEST_MODULE_H
#define MODULES_SELFTEST_SELFTEST_MODULE_H
#ifdef SELFTEST

class SelftestModule 
	: public OperaModule
{
public:
	SelftestModule();
	virtual void InitL(const OperaInitInfo& info);
	virtual void Destroy ();
	void Reset();

	void seed_deterministic_rand(unsigned int seed);
	int deterministic_rand();

	SELFTESTAPI class TestSuite *GetTestSuite() { return suite; }

	/**
	 * Returns the selftest document.
	 * Unlike TestUtils::doc, this call reliably
	 * returns the selftest document always.
	 * If NULL, then there is no test document.
	 * */
	FramesDocument *GetTestDoc();

	class TestSuite     *suite;
	class TestUtils     *utils;
	class TestGroups    *groups;
	BOOL				running;
	unsigned			doc_start_line_number; // The line number, relative to its .ot file, of the first line of the current test document.
	class OperaSelftestDispatcher *selftestDispatcher;
protected:
	class OperaSelftestURLGenerator *selftestUrlGenerator;
	int					deterministic_rand_seed;
};

#  define g_selftest g_opera->selftest_module
#  define SELFTEST_MODULE_REQUIRED
#endif // SELFTEST
#endif // !MODULES_SELFTEST_SELFTEST_MODULE_H
