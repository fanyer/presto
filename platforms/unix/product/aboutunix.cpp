/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "aboutunix.h"
#include "config.h"

#include "modules/locale/oplanguagemanager.h"
#include "modules/util/opfile/opfile.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/prefsfile/prefsfile.h"

#include <unistd.h>
#include <sys/utsname.h>

OperaAbout *OperaAbout::Create(URL &url)
{
	return OP_NEW(AboutUnix, (url));
}

#if defined HAVE_DISK && defined PREFS_READ
OP_STATUS AboutUnix::GetPreferencesFile(OpString *dest)
{
	const PrefsFile *prefsfile = g_prefsManager->GetReader();
	const OpFileDescriptor *inifile = prefsfile->GetFile();

	if (inifile->Type() == OPFILE)
	{
		return dest->Set(reinterpret_cast<const OpFile *>(inifile)->GetFullPath());
	}
	else
	{
		return OpStatus::ERR;
	}
}
#endif

OP_STATUS AboutUnix::GetPlatform(OpString *platform, OpString *version)
{
	struct utsname utsName;
	uname(&utsName);

	OP_STATUS rc = platform->Set(utsName.sysname);
	if (!OpStatus::IsSuccess(rc)) return rc;

	rc = version->Set(utsName.machine);
	if (OpStatus::IsSuccess(rc)) rc = version->Append(", ");
	if (OpStatus::IsSuccess(rc)) rc = version->Append(utsName.release);

	return rc;
}

