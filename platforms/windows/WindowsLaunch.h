// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
//
//

#ifndef WINDOWS_LAUNCH_PI_H
#define WINDOWS_LAUNCH_PI_H

#include "adjunct/desktop_pi/launch_pi.h"

class WindowsLaunchPI : public LaunchPI
{
public:

	WindowsLaunchPI();

	virtual ~WindowsLaunchPI() {}
	
	static LaunchPI* Create();

	/*
	 * Executes a program with the given parameters
	 *
	 * @param exe_name	Full path of file to read in UTF16
	 * @param cmd_line	Command line to pass to the application UTF8
	 *
	 * @return BOOL, TRUE if it succeded
	 */
	virtual BOOL Launch(const uni_char *exe_name, int argc, const char* const* argv);

	/*
	 * Executes a program with the given parameters. Function is synchronous.
	 *
	 * @param exe_name	Full path of file to read in UTF16
	 * @param cmd_line	Command line to pass to the application UTF8
	 *
	 * @return BOOL, TRUE if program succeeded with return code as 0
	 */
	BOOL LaunchAndWait(const uni_char *exe_name, int argc, const char* const* argv);

	/**
	 * Verify signature of executable
	 */
	virtual BOOL VerifyExecutable(const uni_char *executable, const uni_char* vendor_name = NULL);
	
	/*
	 * For an argument passed (from the argv array given to Launch),
	 * quote it if it contains a space
	 * and escapes all " characters
	 *
	 * @param arg			Argument to escape and test
	 *
	 * @return char*, Pointer to a string containing the escaped argument
	 */
	char* EscapeAndQuoteArg(const char* arg);


	/*
	 * For a list of arguments passed, builds the corresponding command line
	 * with the help of EscapeAndQuoteArg
	 */
	
	uni_char* ArgCat(int argc, const char* const* argv);
};

#endif // WINDOWS_LAUNCH_PI_H
