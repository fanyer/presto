/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef _SCANPASS_TESTMAN_H_
#define _SCANPASS_TESTMAN_H_


#if defined(SELFTEST)
#include "modules/network_selftest/urldoctestman.h"

class ScanPass_SimpleTester : public URL_DocSelfTest_Item
{
protected:
	OpString8	test_string;
	OpString8	extra_test_string;
	BOOL	header_loaded;

public:
	ScanPass_SimpleTester () : header_loaded(FALSE) {};
	ScanPass_SimpleTester (URL &a_url, URL &ref, BOOL unique=TRUE) : URL_DocSelfTest_Item(a_url, ref, unique), header_loaded(FALSE) {};
	virtual ~ScanPass_SimpleTester (){}

	OP_STATUS Construct(const OpStringC8 &pass_string);

	OP_STATUS Construct(URL &a_url, URL &ref, const OpStringC8 &pass_string, BOOL unique=TRUE);
	OP_STATUS Construct(URL &a_url, URL &ref, const OpStringC &pass_string, BOOL unique=TRUE);

	OP_STATUS Construct(const OpStringC8 &source_url, URL &ref, const OpStringC8 &pass_string, BOOL unique=TRUE);
	OP_STATUS Construct(const OpStringC &source_url, URL &ref, const OpStringC &pass_string, BOOL unique=TRUE);

	OP_STATUS AddExtraPassString(const OpStringC8 &expass){return extra_test_string.Set(expass);}

	virtual BOOL Verify_function(URL_DocSelfTest_Event event, Str::LocaleString status_code=Str::NOT_A_STRING);

private:

	BOOL CheckPass(const OpStringC8 &pass_string);
};

class UniScanPass_SimpleTester : public URL_DocSelfTest_Item
{
protected:
	OpString	test_string;
	BOOL	header_loaded;

public:
	UniScanPass_SimpleTester () : header_loaded(FALSE) {};
	UniScanPass_SimpleTester (URL &a_url, URL &ref, BOOL unique=TRUE) : URL_DocSelfTest_Item(a_url, ref, unique), header_loaded(FALSE) {};
	virtual ~UniScanPass_SimpleTester (){}

	OP_STATUS Construct(const OpStringC &pass_string);

	OP_STATUS Construct(URL &a_url, URL &ref, const OpStringC &pass_string, BOOL unique=TRUE);

	OP_STATUS Construct(const OpStringC8 &source_url, URL &ref, const OpStringC &pass_string, BOOL unique=TRUE);
	OP_STATUS Construct(const OpStringC &source_url, URL &ref, const OpStringC &pass_string, BOOL unique=TRUE);

	virtual BOOL Verify_function(URL_DocSelfTest_Event event, Str::LocaleString status_code=Str::NOT_A_STRING);

private:

	BOOL CheckPass();
};

#endif

#endif // _SCANPASS_TESTMAN_H_
