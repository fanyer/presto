/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"
#include "platforms/mac/pi/MacOpAutoUpdatePI.h"
#include "platforms/mac/util/systemcapabilities.h"
#include "adjunct/autoupdate/autoupdater.h"
#include <sys/sysctl.h>

#ifdef AUTO_UPDATE_SUPPORT

OpAutoUpdatePI* OpAutoUpdatePI::Create()
{
	return OP_NEW(MacOpAutoUpdatePI, ());
}

OP_STATUS MacOpAutoUpdatePI::GetOSName(OpString& os)
{
	// This must match the download page i.e. http://www.opera.com/download/index.dml?custom=yes
	return os.Set(UNI_L("MacOS"));
}

OP_STATUS MacOpAutoUpdatePI::GetOSVersion(OpString& version)
{
	int major, minor, bugfix;
	GetDetailedOSVersion(major, minor, bugfix);
	version.Empty();
	return version.AppendFormat(UNI_L("%d.%d.%d"), major, minor, bugfix);
}

OP_STATUS MacOpAutoUpdatePI::GetArchitecture(OpString& arch)
{
	if (::GetOSVersion() < 0x1060) {
		return arch.Set(UNI_L("i386"));
	}
	else if (::GetOSVersion() >= 0x1060 && ::GetOSVersion() < 0x1070)
	{
		unsigned int cpu64bit_capable = 0;
		size_t size = sizeof(cpu64bit_capable);
		if (sysctlbyname("hw.cpu64bit_capable", &cpu64bit_capable, &size, NULL, 0) == -1)
		{
			OpString log_msg;
			RETURN_IF_ERROR(log_msg.AppendFormat(UNI_L("Error resolving system ctl: %s"), strerror(errno)));
			Console::WriteError(log_msg.CStr());
			log_msg.Wipe();
			RETURN_IF_ERROR(log_msg.Set(UNI_L("Cannot resolve architecture, assuming i386")));
			Console::WriteMessage(log_msg.CStr());

			return arch.Set(UNI_L("i386"));
		}
		return cpu64bit_capable ? arch.Set(UNI_L("amd64")) : arch.Set(UNI_L("i386"));
	}
	return arch.Set(UNI_L("amd64"));
}

OP_STATUS MacOpAutoUpdatePI::GetPackageType(OpString& package)
{
	return package.Set("");
}

OP_STATUS MacOpAutoUpdatePI::GetGccVersion(OpString& gcc)
{
	return gcc.Set("");
}

OP_STATUS MacOpAutoUpdatePI::GetQTVersion(OpString& qt)
{
	return qt.Set("");
}

#endif // AUTO_UPDATE_SUPPORT
