/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 * 
 * George Refseth, rfz@opera.com
 */

#ifndef MSWINUTIL_H
#define MSWINUTIL_H

class MSWinUtil
{
public:
	static BOOL LookupSid(LPCTSTR lpszMachineName,
						  LPCTSTR lpszAccountName, 
						  LPTSTR lpszSIDStringReceiver, 
						  LPDWORD lpdwBufferLen);

	static BOOL GetTextualSid(PSID pSid, LPTSTR TextualSid, LPDWORD lpdwBufferLen);

	static BOOL IsWindowsNTFamily();
};

#endif//MSWINUTIL_H
