// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// 
//

#ifndef UNIX_LAUNCH_PI_H
#define UNIX_LAUNCH_PI_H

#include "adjunct/desktop_pi/launch_pi.h"

class UnixLaunchPI : public LaunchPI
{
public:
	virtual ~UnixLaunchPI() {}

	/*
	 * Executes a program with the given parameters
	 *
	 * @param exe_name	Full path of file to read in UTF16
	 * @param cmd_line	Command line to pass to the application UTF8
	 *
	 * @return BOOL, TRUE if it succeded
	 */
	virtual BOOL Launch(const uni_char *exe_name, int argc, const char* const* argv);

	virtual BOOL VerifyExecutable(const uni_char *executable, const uni_char *vendor_name);

};

#endif // UNIX_LAUNCH_PI_H
