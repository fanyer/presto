/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** $Id$
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "WindowsOpPlatformViewers.h"
#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"

OP_STATUS OpPlatformViewers::Create(OpPlatformViewers** new_platformviewers)
{
	OP_ASSERT(new_platformviewers != NULL);
	*new_platformviewers = new WindowsOpPlatformViewers();
	if(*new_platformviewers == NULL)
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

OP_STATUS WindowsOpPlatformViewers::OpenInDefaultBrowser(const uni_char* url) 
{ 
	OP_ASSERT(NULL != url);
	if (NULL == url)
	{
		return OpStatus::OK;
	}

	HINSTANCE result = ShellExecute(
				NULL, UNI_L("open"), url, NULL, NULL, SW_SHOWNORMAL);
	OP_ASSERT(result > (HINSTANCE)32);
	if (result > (HINSTANCE)32)
	{
		return OpStatus::OK;
	}
	else
	{
		return OpStatus::ERR;
	}
}

OP_STATUS WindowsOpPlatformViewers::OpenInOpera(const uni_char* url)
{
	OP_ASSERT(NULL != url);
	if ( !(url && *url) )
	{
		return OpStatus::ERR;
	}

	OpString command;
	OP_ASSERT(g_op_system_info);
	RETURN_IF_ERROR(g_op_system_info->GetBinaryPath(&command));
	return g_op_system_info->ExecuteApplication(command.CStr(), url);
}

