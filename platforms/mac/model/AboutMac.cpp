/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/model/AboutMac.h"

#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/prefsfile/prefsfile.h"

#include "modules/locale/oplanguagemanager.h"
#include "platforms/mac/util/macutils.h"
#include "platforms/mac/util/systemcapabilities.h"
#include "platforms/mac/File/FileUtils_Mac.h"
#include "platforms/mac/Resources/buildnum.h"

#ifdef USE_ABOUT_FRAMEWORK

OperaAbout *OperaAbout::Create(URL &url)
{
	return new MacOperaAbout(url);
}

MacOperaAbout::MacOperaAbout(URL &url)
	: AboutDesktop(url)
{
}

/** Platform identifier and version */
OP_STATUS MacOperaAbout::GetPlatform(OpString *platform, OpString *version)
{
	RETURN_IF_ERROR(platform->Set("Mac OS X"));

	char temp[512]; 
	int major, minor, bugfix;
	GetDetailedOSVersion(major, minor, bugfix);
	sprintf(temp, "%d.%d.%d", major, minor, bugfix);
	return version->Set(temp);
}

OP_STATUS MacOperaAbout::GetPreferencesFile(OpString *dest)
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

#endif // not USE_ABOUT_FRAMEWORK
