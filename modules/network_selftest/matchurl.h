/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef _MATCHURL_TESTMAN_H_
#define _MATCHURL_TESTMAN_H_

#if defined(SELFTEST)

#include "modules/network_selftest/urldoctestman.h"

class MatchWithURL_Tester : public URL_DocSelfTest_Item
{
protected:

	class MatchURLLoader : public URL_DocumentLoader
	{
	private:
		MatchWithURL_Tester *parent;

	public:
		MatchURLLoader(MatchWithURL_Tester *prnt):parent(prnt){}
		virtual void OnAllDocumentsFinished();
	};

	MatchURLLoader loader;
	URL match_url;
	URL_InUse match_url_use;


public:
	MatchWithURL_Tester():loader(this){};
	MatchWithURL_Tester(URL &a_url,  URL &ref, URL &mtch_url,BOOL unique=TRUE) : URL_DocSelfTest_Item(a_url, ref, unique), loader(this),match_url(mtch_url),match_url_use(mtch_url) {};
	virtual ~MatchWithURL_Tester(){}

	OP_STATUS Construct(){return url.IsValid() && match_url.IsValid() ? OpStatus::OK : OpStatus::ERR;}

	OP_STATUS Construct(URL &a_url, URL &ref, URL &match_base, URL &mtch_url, BOOL unique=TRUE);

	OP_STATUS Construct(const OpStringC8 &source_url, URL &ref, URL &match_base, const OpStringC8 &mtch_url, BOOL unique=TRUE);
	OP_STATUS Construct(const OpStringC &source_url, URL &ref, URL &match_base, const OpStringC &mtch_url, BOOL unique=TRUE);

	virtual BOOL Verify_function(URL_DocSelfTest_Event event, Str::LocaleString status_code=Str::NOT_A_STRING);

	virtual void MatchURLs_Loaded();
};
#endif

#endif // _MATCHURL_TESTMAN_H_
