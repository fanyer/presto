// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// 
//

#include "core/pch.h"

#include "platforms/quix/desktop_pi_impl/unix_launch_pi.h"
#include "platforms/unix/base/common/applicationlauncher.h"

/* static */
LaunchPI* LaunchPI::Create()
{
	return OP_NEW(UnixLaunchPI, ());
}

BOOL UnixLaunchPI::Launch(const uni_char *exe_name, int argc, const char* const* argv)
{
	return ApplicationLauncher::ExecuteProgram(exe_name, argc, argv, FALSE);
}

BOOL UnixLaunchPI::VerifyExecutable(const uni_char *, const uni_char *)
{
	return TRUE;
}
