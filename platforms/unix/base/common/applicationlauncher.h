// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
//
// Copyright (C) 1995-2003 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Espen Sand
//

#ifndef __APPLICATION_LAUNCHER_H__
#define __APPLICATION_LAUNCHER_H__

#include "modules/util/opstring.h"
#include "modules/util/adt/opvector.h"

class FramesDocument;
class URL;
class Window;
class OpString_list;
//class OpVector;

class ApplicationLauncher
{
public:

	/**
	 * Starts an external application with the specified arguments
	 *
	 * @param application The application to execute
	 * @param arguments A list of arguments. If an argument is enclosed with a set of ""
	 *        then whitespaces will be inluded in the argument, otherwise a whitespace
	 *        will be treated as an argument separator.
	 * @param run_in_terminal
	 *
	 * @return TRUE on success otherwise false
	 */
	static BOOL ExecuteProgram( const OpStringC &application,
								const OpStringC &arguments,
								BOOL run_in_terminal = FALSE );


	/**
	 * Starts an external application with the specified arguments
	 *
	 * @param application The application to execute
	 * @param argc Num of items in argv
	 * @param argv Argument vector as defined in LaunchPI::Launch(...)
	 * @param run_in_terminal
	 *
	 * @return TRUE on success otherwise FALSE
	 */
	static BOOL ExecuteProgram( const OpStringC &application,
								int argc,
								const char* const* argv,
								BOOL run_in_terminal = FALSE );


	/**
	 * Starts an external application that is able to display the
	 * source code of the document.
	 *
	 * @param window. The window where an error message is sent to
	 * @param doc The document that will be inspected
	 *
	 * @return TRUE on success otherwise false
	 */
	static BOOL ViewSource( Window* window, FramesDocument* doc );

	/**
	 * Starts an external application that is able to display the
	 * source code of the document.
	 *
	 * @param filename. File that contains source code
	 *
	 * @return TRUE on success otherwise false
	 */
	static BOOL ViewSource(const OpString& filename, const OpString8& encoding);

	/**
	 * Starts an external email-client program with the parameters
	 * as specified with the mailer command and the function arguments.
	 *
	 * @param to Comma separated list of receivers
	 * @param cc Comma separated list of receivers
	 * @param bcc Comma separated list of receivers
	 * @param subject mail subject
	 * @param message mail message
	 * @param rawUrl raw mailto url (without the mailto specifier)
	 *
	 * @return TRUE on success otherwise false
	 */
	static BOOL GenericMailAction( const uni_char* to,
								   const uni_char* cc,
								   const uni_char* bcc,
								   const uni_char* subject,
								   const uni_char* message,
								   const uni_char* rawurl );

	/**
	 * Starts an external news-client program connecting to the
	 * specified address.
	 *
	 * @param application The application to run
	 * @param address The news address to be parsed.
	 *
	 * @return TRUE on success otherwise false
	 */
	static BOOL GenericNewsAction(const uni_char* application, const uni_char* urlstring);

	/**
	 * Starts an external telnet program connecting to the
	 * specified address.
	 *
	 * @param application The application to run
	 * @param address The telnet address to be parsed.
	 *
	 * @return TRUE on success otherwise false
	 */
	static BOOL GenericTelnetAction(const uni_char* application, const uni_char* urlstring);

	/**
	 * Call this function for protocols that are registered to be trusted
	 * to another program.
	 *
	 * @param window The window where the url was activated
	 * @param url The protocol url
	 *
	 * @return TRUE on success otherwise false
	 */
	static BOOL GenericTrustedAction( Window *window, const URL& url );

	/**
	 * Prepends a terminal program that will execute the application
	 * when started.
	 *
	 * @param application The string will be prepended with the terminal
	 *        on return
	 * @return Opstatus::OK on success, otherwise OpStatus::ERR_NO_MEMORY
	 */
	static OP_STATUS SetTerminalApplication( OpString &application );

	/**
	 * Replaces escaped characters in the taget string with the readable
	 * equivalent
	 *
	 * @param target The string to be modified.
	 * @param start The offset in the string where the scanning will start
	 *        Note that the target string will start at this offset on exit
	 * @param special_character_conversion Convert '&' and '+' when true
	 */
	static void ReplaceEscapedCharacters( OpString &target, UINT32 start, BOOL special_character_conversion );

	static void GetCommandLineOptions(const OpString& protocol, OpString& text);

	static BOOL ParseTelnetAddress( OpVector<OpString>& list,
									const uni_char *application,
									const uni_char *address,
									const uni_char *port,
									const uni_char *user,
									const uni_char *password,
									const uni_char *rawurl );

	static BOOL ParseEmail( OpVector<OpString>& list,
							const uni_char *application,
							const uni_char* to,
							const uni_char* cc,
							const uni_char* bcc,
							const uni_char* subject,
							const uni_char* message,
							const uni_char* rawurl );

	static BOOL ParseNewsAddress( OpVector<OpString>& list,
								  const uni_char *application,
								  const uni_char *url );

	static BOOL ParseTrustedApplication( OpVector<OpString>& list,
										 const uni_char *application,
										 const uni_char *full_address,
										 const uni_char *address );

	/** Conditionally display a dialog with an error message based on the
	 * value of the 'errno' variable.
	 *
	 * A common error handling strategy for C functions is to set the global
	 * 'errno' variable to a value describing the error that occurred and then
	 * return -1.  Calling this method with the return value from such a
	 * function will display a dialog with a proper error message (based on
	 * 'errno') when the function fails.
	 *
	 * @param return_value If 'return_value' is -1, the error message will be
	 * displayed. Otherwise, this method does nothing.
	 */
	static void DisplayDialogOnError(int return_value);
};

#endif
