/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Karlsson
*/

#include "core/pch.h"
#include "WindowsLanguageManager.h"
#include "modules/locale/locale-enum.h"

extern HINSTANCE hInst;

WindowsLanguageManager::WindowsLanguageManager(): OpPrefsFileLanguageManager()
{
}

int WindowsLanguageManager::GetStringInternalL(int num, OpString &s)
{
	int rc = OpPrefsFileLanguageManager::GetStringL(Str::LocaleString(num), s);
	if (0 == rc && num != 0)
	{
		// Fall back to resources for backwards compatibility
		if (num < 50000)
		{
			uni_char tmpbuf[2048];
			if (LoadString(hInst, UINT(num), tmpbuf, 2048) > 0)
			{
				s.SetL(tmpbuf);
				rc = s.Length();
			}
		}
	}
	return rc;
}
