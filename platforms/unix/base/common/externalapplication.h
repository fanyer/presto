// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2003 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Morten Stenshorne, Espen Sand
//
// Platform dependency: UNIX

#ifndef __EXTERNAL_APPLICATION_H__
#define __EXTERNAL_APPLICATION_H__

#include "modules/util/opstring.h"
#include "modules/util/adt/opvector.h"

/**
 * Utility class for starting/handling external applications (email client,
 * helper applications, source viewer etc.)
 * It is safe to delete an ExternalApp object while the external application
 * is running.
 */
class ExternalApplication
{
private:
	const BOOL spawn;
	uni_char *m_cmdline;

public:
	/** Handler for an external command.
	 * @param cmdline the command line as a single unicode string
	 * @param disown default is TRUE, meaning processes running the
	 * command should die quietly when complete; if set to FALSE, the
	 * process which runs the command will, on exit, send the Opera
	 * process a SIGCHLD and survive as a defunct process until
	 * wait()ed for - this enables the Opera process to garner
	 * information such as its exit status, but obliges the Opera
	 * process to handle SIGCHLD and wait() for such children.
	 */
	ExternalApplication(const uni_char* cmdline, BOOL disown=TRUE);

	/**
	 * Destructor. Releases any allocated resources
	 */
	~ExternalApplication();

    /**
     * Runs the application, with an optional argument
	 *
     * @param (optional) argument. May be NULL.
	 * @param fd Filedescriptor for error reporting
	 *
	 * @return 0 on success, otherwise -1. The global variable errno will
	 *         hold the error code.
     */
	int run( const uni_char* argument, int fd=-1, const char* encoding=0 );

	/**
     * Runs the application, with argument (not optional)
	 *
     * @param list Argument list. 
	 * @param fd Filedescriptor for error reporting
	 * @param disown Whether child running application should detach from
	 * parent.  Default is true.  If given as false, child will not exit until
	 * parent process wait()s for it.
	 *
	 * @return 0 on success, otherwise -1. The global variable errno will
	 *         hold the error code.
     */
	static int run( const OpVector<OpString>& list, int fd=-1, BOOL disown=TRUE, const char* encoding=0 );

	/**
     * Runs the application, with argument (not optional)
	 *
     * @param argv Argument list. 
	 * @param fd Filedescriptor for error reporting
	 * @param disown Whether child running application should detach from
	 * parent.  Default is true.  If given as false, child will not exit until
	 * parent process wait()s for it.
	 *
	 * @return 0 on success, otherwise -1. The global variable errno will
	 *         hold the error code.
     */
	static int run( uni_char **argv, int fd=-1, BOOL disown=TRUE, const char* encoding=0 );

    /**
     * Parse a command line and split it into argument strings.
     * @param cmdline the original command line - e.g. "xterm -e pine"
     * @param argument optional argument to the application. May be NULL.
     * @return an array of ASCIIZ strings that are meant to be passed to 
	 * an exec() function. IMPORTANT: The caller must delete this string 
	 * array when it's done with it!
	 * The last element in the array is a NULL pointer
     */
    static uni_char **parseCmdLine(const uni_char *cmdline, 
								   const uni_char *argument);

    /**
     * Delete a string array and all the strings in that array. 
	 * The last array element must be NULL.
	 *
     * @param array the array. May be NULL.
     */
    static void deleteArray(uni_char **array);
};

#endif
