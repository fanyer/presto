/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2004-2008 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve N. Pettersen
*/

#ifndef SERVERPUSH_DOCTESTMAN_H
#define SERVERPUSH_DOCTESTMAN_H

#ifdef SELFTEST

#include "modules/selftest/src/testutils.h"
#include "modules/url/url2.h"
#include "modules/network_selftest/urldoctestman.h"
#include "modules/network_selftest/serverpush_item.h"

class DataStream_GenericFile;

class ServerPush_DocTester : public URL_DocSelfTest_Item
{
private:
	BOOL started;
	int count;
	BOOL header_loaded;
	ServerPush_Filename_Item *current_item;
	DataStream_GenericFile *current_file;
	URL_DataDescriptor *current_descriptor;
	ServerPush_Filename_List test_items;

public: 
	ServerPush_DocTester(URL &a_url, URL &ref, ServerPush_Filename_List &names);
	virtual ~ServerPush_DocTester();

	virtual BOOL Verify_function(URL_DocSelfTest_Event event, Str::LocaleString status_code);

	BOOL CheckContent(BOOL finished);
};

#endif  // SELFTEST
#endif  // SERVERPUSH_DOCTESTMAN_H

