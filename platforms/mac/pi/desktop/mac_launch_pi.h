// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Adam Minchinton
//

#ifndef MAC_LAUNCH_PI_H
#define MAC_LAUNCH_PI_H

#include "adjunct/desktop_pi/launch_pi.h"
#include "adjunct/desktop_util/string/OpBasicString.h"

class MacLaunchPI : public LaunchPI
{
public:
	virtual ~MacLaunchPI() {}

	static LaunchPI* Create();

	/*
	 * Executes a program with the given parameters
	 *
	 * @param exe_name	Full path of file to read in UTF16
	 * @param argc		Number of command line arguments
	 * @param argv		Array of command line arguments in UTF8
	 *
	 * @return BOOL, TRUE if it succeded
	 */
	virtual BOOL Launch(const uni_char *exe_name, int argc, const char* const* argv);

	/**
	 * Verify signature of executable
	 */	
	virtual BOOL VerifyExecutable(const uni_char *executable, const uni_char *vendor_name = NULL);

private:
	void BuildCodesignCmd(OpBasicString8 &cmd_line, const char *bundle_id, char *exe);
};

#endif // MAC_LAUNCH_PI_H
