/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2004-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Morten Stenshorne
 */
#include "core/pch.h"

#include "uastring.h"

#include <sys/utsname.h>

OP_STATUS GetUnixUaWindowingSystem(OpString8 &winsys)
{
#if defined(_X11_)
	RETURN_IF_ERROR(winsys.Append("X11"));
#endif
	return OpStatus::OK;
}

OP_STATUS GetUnixUaScreenSize(OpString8 &uastring)
{
	return OpStatus::OK;
}

OP_STATUS GetUnixUaSystemString(OpString8 &uastring)
{
	uastring.Empty();

	struct utsname buf;
	if (uname(&buf) == 0)
	{
#ifdef UA_INCLUDE_WINDOWSYS
		RETURN_IF_ERROR(GetUnixUaWindowingSystem(uastring));
		RETURN_IF_ERROR(uastring.Append("; "));
#endif // UA_INCLUDE_WINDOWSYS
		RETURN_IF_ERROR(uastring.Append(buf.sysname));
#ifdef UA_INCLUDE_ARCHITECTURE
		RETURN_IF_ERROR(uastring.Append(" "));
		RETURN_IF_ERROR(uastring.Append(buf.machine));
#endif // UA_INCLUDE_ARCHITECTURE

#ifdef UA_INCLUDE_DEVICENAME
		RETURN_IF_ERROR(uastring.Append("; "));
		RETURN_IF_ERROR(GetUnixUaPrettyDeviceName(uastring));
#endif // OPERA_DEVICENAME

#ifdef UA_INCLUDE_BUILDNUMBER
		RETURN_IF_ERROR(uastring.Append("; "));
		RETURN_IF_ERROR(uastring.Append(BROWSER_BUILD_NUMBER));
#endif // UA_INCLUDE_BUILDNUMBER

#ifdef UA_INCLUDE_SCREENSIZE
		RETURN_IF_ERROR(uastring.Append("; "));
		RETURN_IF_ERROR(GetUnixUaScreenSize(uastring));
#endif // UA_INCLUDE_SCREENSIZE
	}
	if (uastring.IsEmpty())
	{
		perror("uname");
		RETURN_IF_ERROR(uastring.Set("UNIX"));
	}

	return OpStatus::OK;
}

OP_STATUS GetUnixUaDeviceInfo(OpString8 &uastring)
{
	return OpStatus::OK;
}

#ifdef CUSTOM_UASTRING_SUPPORT
OP_STATUS GetUnixUaPrettyDeviceName(OpString8 &uastring)
{
	return OpStatus::OK;
}
#endif // CUSTOM_UASTRING_SUPPORT

#ifdef CUSTOM_UASTRING_SUPPORT
OP_STATUS GetUnixUaOSName(OpString8 &uastring)
{
	struct utsname buf;
	if (uname(&buf) != 0)
		return OpStatus::ERR;
	return uastring.Append(buf.sysname);
}

OP_STATUS GetUnixUaOSVersion(OpString8 &uastring)
{
	struct utsname buf;
	if (uname(&buf) != 0)
		return OpStatus::ERR;
	return uastring.Append(buf.release);
}

OP_STATUS GetUnixUaMachineArchitecture(OpString8 &uastring)
{
	struct utsname buf;
	if (uname(&buf) != 0)
		return OpStatus::ERR;
	return uastring.Append(buf.machine);
}

OP_STATUS GetUnixUaDeviceIdentifier(OpString8 &uastring)
{
	return OpStatus::OK;
}

OP_STATUS GetUnixUaDeviceSoftwareVersion(OpString8 &uastring)
{
	return OpStatus::OK;
}

#endif // CUSTOM_UASTRING_SUPPORT
