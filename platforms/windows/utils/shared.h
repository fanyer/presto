/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef SHARED_H
#define SHARED_H

/**
 * These are utility functions that are shared between the opera
 * desktop starter project and the opera dll project
 */
namespace WindowsUtils
{
	// To prevent DLL hijacking don't directly use LoadLibrary as it also searches the
	// current work directory which might not be safe!(DSK-311377)
	HMODULE SafeLoadLibrary(const wchar_t* dll_name);

#if !defined PLUGIN_WRAPPER && !defined OPERA_MAPI
	DWORD GetOperaProcess(bool look_for_upgrader = false);
#endif //!defined PLUGIN_WRAPPER && !defined OPERA_MAPI
	void ShowError(char *message);
};

#endif // SHARED_H
