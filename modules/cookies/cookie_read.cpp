/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

// Url_cm.cpp

// URL Cookie Management

#include "core/pch.h"

#include "modules/pi/OpSystemInfo.h"
#include "modules/url/url_man.h"
#include "modules/formats/url_dfr.h"
#include "modules/cookies/url_cm.h"
#include "modules/cookies/cookie_common.h"
#include "modules/util/opfile/opmemfile.h"

void Cookie_Manager::CheckCookiesReadL()
{
#ifdef DISK_COOKIES_SUPPORT
	if (!cookie_file_read)
	{
		ReadCookiesL();
	}
#endif // DISK_COOKIES_SUPPORT
}

#ifdef DISK_COOKIES_SUPPORT
void Cookie_Manager::ReadCookiesL()
{
	DataStream_NonSensitive_ByteArrayL(read_cookies);

	cookie_file_read = TRUE;

#if defined EXTERNAL_COOKIE_STORAGE
	int len;
	extern unsigned char* ReadExternalCookies(URL_CONTEXT_ID, int*);

	unsigned char* cookies = ReadExternalCookies(context_id, &len);
	if (len == 0)
		return;

	OpStackAutoPtr<OpMemFile> cfp(OpMemFile::Create(cookies,len));
	cfp->Open( OPFILE_READ );
	OP_DELETEA(cookies);
#else // EXTERNAL_COOKIE_STORAGE
	if(cookie_file_location == OPFILE_ABSOLUTE_FOLDER)
		return;

	OpStackAutoPtr<OpFile> cfp(OP_NEW_L(OpFile, ()));

	LEAVE_IF_ERROR(cfp->Construct(CookiesFile, cookie_file_location));
	
	if (OpStatus::IsError(cfp->Open(OPFILE_READ)))
	{
		LEAVE_IF_ERROR(cfp->Construct(CookiesFileOld, cookie_file_location));
		if (OpStatus::IsError(cfp->Open(OPFILE_READ)))
		{
			cfp.reset();
		}
	}
#endif // EXTERNAL_COOKIE_STORAGE
	if(cfp.get())
	{
		DataFile c4fp(cfp.release()); ANCHOR(DataFile, c4fp);

		c4fp.InitL();

		TRAPD(op_err, cookie_root->ReadCookiesL(c4fp, c4fp.AppVersion()));
		if(OpStatus::IsMemoryError(op_err))
			LEAVE(op_err); // ignore non-memory problems
		c4fp.Close();
	}
	else
		return;

	Cookie* ck = NULL;
	time_t this_time = (time_t) (g_op_time_info->GetTimeUTC()/1000.0);

	while (cookies_count > max_cookies)
	{
		ck = cookie_root->GetLeastRecentlyUsed(this_time, this_time);
		
		// delete if still necessary
		if (ck && (cookies_count > max_cookies))
		{
			OP_DELETE(ck);
		}
		if(!ck)
			break;
	}

#ifdef _DEBUG_COOKIES_
			FILE* fp = fopen("c:\\klient\\dcookie.txt", "a");
			if (fp)
			{
				cookie_root->DebugWriteCookies(fp);
				fclose(fp);
			}
#endif
}
#endif // DISK_COOKIES_SUPPORT

