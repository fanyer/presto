/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"
#include "modules/locale/locale-enum.h"

#if defined(EXTERNAL_APPLICATIONS_SUPPORT) && defined(DU_REMOTE_URL_HANDLER)
# include "adjunct/desktop_util/handlers/RemoteURLHandler.h"
#endif

#if defined (EXTERNAL_APPLICATIONS_SUPPORT) && defined (DU_REMOTE_URL_HANDLER)
OP_STATUS DesktopOpSystemInfo::ExecuteApplication(const uni_char* application, const uni_char* args, BOOL silent_errors)
{
	if (args)
		RemoteURLHandler::AddURLtoList(args);
	else if (application)
		RemoteURLHandler::AddURLtoList(application);
	
	return PlatformExecuteApplication(application, args, silent_errors);
}

OP_STATUS DesktopOpSystemInfo::OpenFileInApplication(const uni_char* application, const uni_char* filename)
{
	OpString quoted_filename;
	if (filename)
		OpStatus::Ignore(quoted_filename.AppendFormat(UNI_L("\"%s\""), filename));
	if (application)
		return ExecuteApplication(application, quoted_filename.CStr());
	return OpStatus::ERR;
}
#endif // EXTERNAL_APPLICATIONS_SUPPORT && DU_REMOTE_URL_HANDLER

