/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef _JUSTLOAD_TESTMAN_H_
#define _JUSTLOAD_TESTMAN_H_

#if defined(SELFTEST)

#include "modules/network_selftest/urldoctestman.h"

class JustLoad_Tester : public URL_DocSelfTest_Item
{
protected:
	BOOL	header_loaded;

public:
	JustLoad_Tester() : header_loaded(FALSE) {};
	JustLoad_Tester(URL &a_url, URL &ref, BOOL unique=TRUE) : URL_DocSelfTest_Item(a_url, ref, unique), header_loaded(FALSE) {};
	virtual ~JustLoad_Tester(){}

	OP_STATUS Construct(){return url.IsValid() ? OpStatus::OK : OpStatus::ERR;}

	OP_STATUS Construct(URL &a_url, URL &ref, BOOL unique=TRUE);
	OP_STATUS Construct(const OpStringC8 &source_url, URL &ref, BOOL unique=TRUE);
	OP_STATUS Construct(const OpStringC &source_url, URL &ref, BOOL unique=TRUE);

	virtual BOOL Verify_function(URL_DocSelfTest_Event event, Str::LocaleString status_code=Str::NOT_A_STRING);
};
#endif

#endif // _JUSTLOAD_TESTMAN_H_
