/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "dep.h"

void SetEnableDEP()
{
	if (HASSetProcessDEPPolicy())
	{
		// XP SP3, we don't need to use any hacks on this OS
		// See http://blogs.msdn.com/michael_howard/archive/2008/01/29/new-nx-apis-added-to-windows-vista-sp1-windows-xp-sp3-and-windows-server-2008.aspx

		OPSetProcessDEPPolicy(PROCESS_DEP_ENABLE | PROCESS_DEP_DISABLE_ATL_THUNK_EMULATION);
		return;
	}
	// Only available on Windows XP SP2/Windows Server 2003 SP1 and higher
	// See: http://www.uninformed.org/?v=2&a=4

	ULONG flags = MEM_EXECUTE_OPTION_PERMANENT | MEM_EXECUTE_OPTION_DISABLE;

	OPNtSetInformationProcess(GetCurrentProcess(), NtDll::ProcessExecuteFlags, &flags, sizeof(flags));
}
