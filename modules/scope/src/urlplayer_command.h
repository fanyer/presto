/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** These mostly abstract classes can be implemented and installed into a
** running opera, such that a 'telnet' program can be used to talk to the
** commands implemented by the shell.
** Several shells, each whith its own command portfolio can be installed.
** The commands from all shells will be available, if named differently.
*/

/**
  * @file urlplayer_commands.h
  * Contains the definition of class UrlPlayerCommand.
  *
  * @author Ole Jørgen Anfindsen <olejorgen@opera.com>
  *
  */

#ifndef _URLPLAYER_COMMAND_H
#define _URLPLAYER_COMMAND_H

#ifdef SCOPE_URL_PLAYER

#ifdef SUPPORT_DEBUGGING_SHELL

class OpShellListener;
class OpProbeShellLoadingListener;
class OpProbeShellErrorListener;
class OpWindowCommander;

#include "modules/probetools/shell-probes.h"

/**
  * The class UrlPlayerCommand implements some commands that are used by URL players
  * and that are invoked through OpProbeShell.
  */
class UrlPlayerCommand : public OpShellListener
{
public:

	UrlPlayerCommand(char* id, OpShell* shell);
	virtual ~UrlPlayerCommand(void);

	virtual void OnClientConnected( uni_char* msg, const int connection_index );
	virtual void OnClientDisconnected( uni_char* msg, const int connection_index );

private:

	// specific methods that implement stuff that the urlplayer supports:
	void OnCreateWindows( char* msg, int connection_index );
	void OnLoadUrl( char* msg, int connection_index );


	//OpProbeShell*						m_probe_shell;
	int									m_num_windows;

	// one listener per potential window:
	OpProbeShellLoadingListener*		m_loading_listener[SHELL_PROBES_MAX_WINDOWS];
	OpProbeShellErrorListener*			m_error_listener[SHELL_PROBES_MAX_WINDOWS];
	OpWindowCommander*					m_windowcommander[SHELL_PROBES_MAX_WINDOWS];
	//OpWindow*							m_opwindow[SHELL_PROBES_MAX_WINDOWS]; //deprecated

	// one listener for the OpProbeShell:
	//OpProbeShellListener*				m_psl;

}; //class UrlPlayerCommand

#else // must be SCOPE_NETWORK

// lth@opera.com

#include "modules/scope/src/scope_service.h"
#include "modules/windowcommander/src/WindowCommanderUrlPlayerListener.h"

#include "modules/scope/src/generated/g_urlplayer_command_interface.h"

class OpUrlPlayerWindow;
class WindowCommander;

class OpScopeUrlPlayer
	: public OpScopeUrlPlayer_SI
	, public WindowCommanderUrlPlayerListener
{
public:
	enum Context
	{
		CREATE_WINDOWS,
		LOAD_URL,
		ILLEGAL
	};

	OpScopeUrlPlayer();
	virtual ~OpScopeUrlPlayer(void);

	/**
	 * WindowCommanderUrlPlayerListener event telling us that a
	 * window has been closed, and that the windowcommander connected
	 * to that window is deleted.
	 *
	 * Method will also notice the url player that the page has
	 * completed so that player dont wait for a window that isn't there.
	 *
	 * @param wc The WindowCommander that will be deleted in 2 sec
	 */
	virtual void OnDeleteWindowCommander(OpWindowCommander* wc);

private:
	int					m_num_windows;
	OpUrlPlayerWindow	*m_windows;

	/**
	 * Setup a single window, return error status if problematic!
	 * This is used at startup, and if a window is closed while playing.
	 * 
	 * @param i The index of m_windows array
	 * @param url_string initial url_string , blank is good.
	 */
	OP_STATUS InitSingleWindow(int i, const OpString& url_string);

public:
	// Request/Response functions
	virtual OP_STATUS DoCreateWindows(const WindowCount &in, WindowCount &out);
	virtual OP_STATUS DoLoadUrl(const Request &in, Window &out);
};

#endif // !SUPPORT_DEBUGGING_SHELL

#endif // SCOPE_URL_PLAYER

#endif // _URLPLAYER_COMMAND_H
