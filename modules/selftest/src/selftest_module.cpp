/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4;  -*-
**
** Copyright (C) 2002-now Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#include "core/pch.h"

#ifdef SELFTEST

#include "modules/dom/domenvironment.h"

#include "modules/selftest/optestsuite.h"
#include "modules/selftest/src/testutils.h"

#include "modules/selftest/generated/testgroups.h"

#include "modules/selftest/operaselftesturlgenerator.h"
#include "modules/selftest/operaselftestdispatcher.h"

#include "modules/url/url_api.h"

// Init and exit code.

SelftestModule::SelftestModule() :
	suite(NULL),
	utils(NULL),
	groups(NULL),
	running(FALSE),
	doc_start_line_number(0),
	selftestDispatcher(NULL),
	selftestUrlGenerator(NULL)
{
}


void SelftestModule::Reset()
{
	running = FALSE;
 	suite->Reset();
	utils->Reset();//Exits app if test-window isnt set
	groups->Reset();
	selftestDispatcher->Reset();
}

void SelftestModule::InitL(const OperaInitInfo& /*info*/)
{
	suite = OP_NEW_L(TestSuite, ());
	LEAVE_IF_ERROR(suite->Construct());
	utils = OP_NEW_L(TestUtils, ());
	groups = OP_NEW_L(TestGroups, ());
	
	//Register opera:selftest url
	selftestUrlGenerator = OP_NEW_L(OperaSelftestURLGenerator, ());
	selftestUrlGenerator->Construct("selftest", FALSE);
	g_url_api->RegisterOperaURL(selftestUrlGenerator);
	
	//Register opera.* selftest callbacks for ecmascript
	selftestDispatcher = OP_NEW_L(OperaSelftestDispatcher, ());
	selftestDispatcher->registerSelftestCallbacks();
}

void SelftestModule::Destroy()
{
	running = FALSE;

	OP_DELETE(suite);
	OP_DELETE(utils);
	OP_DELETE(groups);
	OP_DELETE(selftestUrlGenerator);
	OP_DELETE(selftestDispatcher);

	suite = NULL;
	utils = NULL;
	groups = NULL;
}

#endif // SELFTEST
