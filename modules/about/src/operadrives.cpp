/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#include "core/pch.h"

#if defined SYS_CAP_FILESYSTEM_HAS_MULTIPLE_DRIVES && defined _LOCALHOST_SUPPORT_
#include "modules/about/operadrives.h"
#include "modules/locale/locale-enum.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/pi/OpSystemInfo.h"

OP_STATUS OperaDrives::GenerateData()
{
	RETURN_IF_ERROR(OpenDocument(Str::SI_LOCAL_DRIVES_TEXT, PrefsCollectionFiles::StyleDrivesFile));
	RETURN_IF_ERROR(OpenBody(Str::SI_LOCAL_DRIVES_TEXT));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("<ol>\n")));

	OpString driveletters;
	RETURN_IF_ERROR(g_op_system_info->GetDriveLetters(&driveletters));
	for (uni_char *driveletter = driveletters.CStr(); *driveletter; ++ driveletter)
	{
		m_url.WriteDocumentDataUniSprintf(
				UNI_L(" <li><a href=\"file://localhost/%c:/\">%c:</a></li>\n"),
				*driveletter, *driveletter);
	}

	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</ol>\n")));
	return CloseDocument();
}

#endif
