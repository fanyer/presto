// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//

#include "core/pch.h"

#ifdef AUTO_UPDATE_SUPPORT

#include "unix_opautoupdate.h"

#include "modules/prefs/prefsmanager/collections/pc_unix.h"
#include "platforms/unix/base/common/unix_opsysteminfo.h"
#include "adjunct/desktop_util/resources/pi/opdesktopproduct.h"

OpAutoUpdatePI* OpAutoUpdatePI::Create()
{
	return new UnixOpAutoUpdate;
}


OP_STATUS UnixOpAutoUpdate::GetOSName(OpString& os)
{
#if defined(__FreeBSD__)
	return os.Set("FreeBSD");
#elif defined(__linux__)
	return os.Set("Linux");
#else
#error "Fix me: unsupported OS name"
	return OpStatus::ERR;
#endif
}


OP_STATUS UnixOpAutoUpdate::GetOSVersion(OpString& version)
{
	version.Empty();
#if defined(__FreeBSD__)
	return version.AppendFormat(UNI_L("%d"), __FreeBSD__); // FreeBSD major version, compile-time
#else
	// Doesn't make sense for Linux
	return OpStatus::OK;
#endif
}


OP_STATUS UnixOpAutoUpdate::GetArchitecture(OpString& arch)
{
#if defined(__x86_64__)
	return arch.Set("x86-64");
#elif defined(i386)
	return arch.Set("i386");
#elif defined(__sparc__)
	return arch.Set("sparc");
#elif defined(__PPC__)
	return arch.Set("ppc");
#else
#error "Fix me: unsupported architecture"
	return OpStatus::ERR;
#endif
}


OP_STATUS UnixOpAutoUpdate::GetPackageType(OpString& package)
{
	return package.Set(g_desktop_product->GetPackageType());
}


OP_STATUS UnixOpAutoUpdate::GetGccVersion(OpString& gcc)
{
#if defined(__GNUC__)
# if __GNUC__ >= 4
	return gcc.Set("gcc4");
# elif __GNUC__ >= 3
	return gcc.Set("gcc3");
# else 
	return gcc.Set("gcc295");
# endif
#else
# error "Fix me: unsupported gcc version"
	return OpStatus::ERR;
#endif
}


OP_STATUS UnixOpAutoUpdate::GetQTVersion(OpString& qt)
{
	return qt.Set(UNI_L(""));
}


#endif
