/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 */

#include "core/pch.h"

#include "ViewixOpPlatformViewers.h"
#include "modules/pi/OpSystemInfo.h"
#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"
#include "platforms/viewix/src/FileHandlerManagerUtilities.h"

namespace
{
	// Sole purpose of this class is to clear specific environment
	// variables and restore them once function returns.
	// Use it only through STORE_OP_ENV macro.

	struct op_env_val
	{
		op_env_val(const char *name)
		{
			init_status = key.Set(name);
			if (OpStatus::IsSuccess(init_status))
				init_status = val.Set(op_getenv(name));
			if (OpStatus::IsSuccess(init_status))
				unsetenv(key.CStr());
		}

		~op_env_val()
		{
			if (OpStatus::IsSuccess(init_status))
				OpStatus::Ignore(g_env.Set(key.CStr(), val.CStr()));
		}

		OP_STATUS init_status;
		OpString8 key;
		OpString  val;
	};
};

#if defined(STORE_OP_ENV)
	#error
#else
	#define STORE_OP_ENV(x) \
		op_env_val x(#x); \
		if (OpStatus::IsError(x.init_status)) \
			return x.init_status;
#endif

OP_STATUS OpPlatformViewers::Create(OpPlatformViewers** new_platformviewers)
{
	OP_ASSERT(new_platformviewers != NULL);
	*new_platformviewers = new ViewixOpPlatformViewers();
	if(*new_platformviewers == NULL)
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

OP_STATUS ViewixOpPlatformViewers::OpenInDefaultBrowser(const uni_char* url) 
{
	// Now, when we want to open default browser, we need to clear environment.
	// Otherwise, if opera is set as default browser, xdg-open
	// may send request back to origin process.
	// see DSK-310055

	STORE_OP_ENV(OPERA_DIR);
	STORE_OP_ENV(OPERA_PERSONALDIR);
	STORE_OP_ENV(OPERA_BINARYDIR);
	STORE_OP_ENV(OPERA_HOME);

	// xdg-open is recommended by freedesktop.org way of opening pages 
	// in default browser, but DEs prefer to use their own launchers.
	// see DSK-272180

	if (FileHandlerManagerUtilities::isKDERunning())
	{
		return g_op_system_info->ExecuteApplication(UNI_L("kioclient exec"), url);
	}
	else if (FileHandlerManagerUtilities::isGnomeRunning())
	{
		return g_op_system_info->ExecuteApplication(UNI_L("gnome-open"), url);
	}
	else
	{
		return g_op_system_info->ExecuteApplication(UNI_L("xdg-open"), url);
	}
}

OP_STATUS ViewixOpPlatformViewers::OpenInOpera(const uni_char* url)
{
	OP_ASSERT(NULL != url);
	if ( !(url && *url) )
	{
		return OpStatus::ERR;
	}

	OpString command;
	OP_ASSERT(g_op_system_info);
	RETURN_IF_ERROR(g_op_system_info->GetBinaryPath(&command));

	// Opera profile environment variables need to be cleared otherwise call
	// will go back to origin process.

	STORE_OP_ENV(OPERA_PERSONALDIR);
	STORE_OP_ENV(OPERA_HOME);

	return g_op_system_info->ExecuteApplication(command.CStr(), url);
}

#undef STORE_OP_ENV

