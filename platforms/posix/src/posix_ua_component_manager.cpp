/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"
#ifdef POSIX_OK_HOST

# include "platforms/posix/posix_ua_component_manager.h"
# include <sys/utsname.h>

PosixUAComponentManager::PosixUAComponentManager()
{
	op_memcpy(m_ms_ie_ver, POSIX_IE_SYSNAME, sizeof(m_ms_ie_ver));
}

/** Initialize cached data. */
OP_STATUS PosixUAComponentManager::Construct()
{
	struct utsname buf;
	int unret = uname(&buf);

	// set m_os_ver
	if (unret >= 0)
	{
#ifdef HAVE_GETENV
		// Use shell-obtained O/S name if available (see DSK-181752):
		const char *systemname = op_getenv("OPERA_SYSTEM_UNAME");
		if (!systemname || systemname[0] == '\0')
			systemname = buf.sysname; // treat empty as unset
#else
		const char *const systemname = buf.sysname;
#endif

		RETURN_IF_ERROR(m_os_ver.Set(POSIX_WINSYS_PREFIX));
		RETURN_IF_ERROR(m_os_ver.AppendFormat("%s ", systemname));
#ifdef __FreeBSD__ // Moose likes to fragment the BSD world as much as possible:
		if (buf.release)
			RETURN_IF_ERROR(m_os_ver.AppendFormat("%s ", buf.release));
#endif
		RETURN_IF_ERROR(m_os_ver.AppendFormat("%s", buf.machine));
	}
	else // Fall-backs:
	{
		RETURN_IF_ERROR(m_os_ver.Set("UNIX"));
	}

#ifdef QWS // It would be nice to banish this, or make it a cross-platform tweak, but
	QSize screen = qApp->desktop()->size(); // qApp and QSize are platform-specific !
	RETURN_IF_ERROR(m_os_ver.AppendFormat("; %dx%d", screen.width(), screen.height()));
#endif
	OP_ASSERT(m_os_ver.CStr());
	return OpStatus::OK;
}

#endif // POSIX_OK_HOST
