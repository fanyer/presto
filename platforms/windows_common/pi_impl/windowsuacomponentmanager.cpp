/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"
#ifdef WIN_OK_UACOMPONENTMANAGER
#include "platforms/windows_common/pi_impl/windowsuacomponentmanager.h"

OP_STATUS WindowsUAComponentManager::Construct()
{
	/* See DSK-61093: Both MSIE and Mozilla use 5.x for all WIN32_NT platforms.
	   Scripts depend on this, so be compatible when it matters.
	   Mostly borrowed from Windows desktop's GetWinVersionStr() implementation */
	OSVERSIONINFO osvi = {0};

	osvi.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);
	if (!GetVersionEx(&osvi))
		return m_os_ver.Set("Windows");

	switch (osvi.dwPlatformId)
	{
	case VER_PLATFORM_WIN32_NT:
		{
#ifdef SIXTY_FOUR_BIT
			/* Native 64-bit. */
			const char architecture[] = "; Win64; x64";
#else // SIXTY_FOUR_BIT
			const char *architecture = "";

			/* Retrieve pointer to the function dynamically for compatibility
			   reasons - it's only available in Windows XP and newer. */
			typedef void (WINAPI *GETNATIVESYSTEMINFOPROC)(SYSTEM_INFO *system_info);
			GETNATIVESYSTEMINFOPROC pGetNativeSystemInfo =
				reinterpret_cast<GETNATIVESYSTEMINFOPROC>(GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetNativeSystemInfo"));

			if (pGetNativeSystemInfo)
			{
				SYSTEM_INFO si = {0};
				pGetNativeSystemInfo(&si);
				if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
					/* 32-bit binary running on 64-bit system. */
					architecture = "; WOW64";
			}
#endif // !SIXTY_FOUR_BIT
			return m_os_ver.AppendFormat("Windows NT %d.%d%s", osvi.dwMajorVersion, osvi.dwMinorVersion, architecture);
		}

	case VER_PLATFORM_WIN32_WINDOWS:
		if (osvi.dwMajorVersion == 4 && osvi.dwMinorVersion < 10)
			return m_os_ver.Set("Windows 95");
		else if (osvi.dwMajorVersion == 4 && osvi.dwMinorVersion >= 10 && osvi.dwMinorVersion < 20)
			return m_os_ver.Set("Windows 98");
		else if (osvi.dwMajorVersion > 4 || (osvi.dwMajorVersion == 4 && osvi.dwMinorVersion >= 90))
			return m_os_ver.Set("Windows ME");
		else
			return m_os_ver.AppendFormat("Windows %d.%d", osvi.dwMajorVersion, osvi.dwMinorVersion);

	case VER_PLATFORM_WIN32s:
		/* Most definitely not supported, but let's just keep it here anyway */
		return m_os_ver.AppendFormat("Win32s %d.%d", osvi.dwMajorVersion, osvi.dwMinorVersion);

#ifdef VER_PLATFORM_WIN32_CE
	case VER_PLATFORM_WIN32_CE:
		/* MSDN says this is a valid response, but not all SDKs define it */
		return m_os_ver.AppendFormat("Windows CE %d.%d", osvi.dwMajorVersion, osvi.dwMinorVersion);
#endif

	default:
		/* This should not happen */
		return m_os_ver.Set("Windows");
	}
}

#endif // WIN_OK_UACOMPONENTMANAGER
