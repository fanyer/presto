/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/locale/oplanguagemanager.h"
#include "modules/url/tools/arrays.h"
#include "modules/locale/locale-enum.h"

void
AuthModule::InitL(const OperaInitInfo& info)
{
#ifdef HTTP_DIGEST_AUTH
	CONST_ARRAY_INIT_L(digest_algs);
	CONST_ARRAY_INIT_L(HTTP_method_texts);
#endif
#ifdef DEBUG
	OpString str;
	int len = g_languageManager->GetStringL(Str::S_BASIC_AUTH_DESCRIPTION, str);
	if( len==0 )
	{
#ifdef DEBUG_ENABLE_PRINTF
		dbg_printf("Language file appears to be missing, basic authentication won't work (no dialog). Fix!\n");
#endif
		exit(-1);
	}

	time_t current_time = op_time(NULL);
	struct tm *time_fields = op_gmtime(&current_time);

	if (time_fields)
	{
		int current_year = time_fields->tm_year+1900;
		if (current_year<2012)
		{
#ifdef DEBUG_ENABLE_PRINTF
			dbg_printf("Your device time is wrong. SSL certificates won't work. Fix!\n");
#endif
			exit(-2);
		}
	}
#endif // DEBUG
}

void
AuthModule::Destroy()
{
#ifdef HTTP_DIGEST_AUTH
	CONST_ARRAY_SHUTDOWN(digest_algs);
	CONST_ARRAY_SHUTDOWN(HTTP_method_texts);
#endif
}
