/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */

#ifndef __IPC_MESSAGE_PARSER_H__
#define __IPC_MESSAGE_PARSER_H__

class BrowserDesktopWindow;
class DesktopWindow;

namespace IPCMessageParser
{
	struct ParsedData
	{
		ParsedData()
			:new_window(FALSE)
			,new_tab(FALSE)
			,privacy_mode(FALSE)
			,in_background(FALSE)
			,noraise(FALSE)
			{}

		BOOL new_window;
		BOOL new_tab;
		BOOL privacy_mode;
		BOOL in_background;
		BOOL noraise;

		OpString address;
		OpString window_name;
		OpRect window_rect;
	};

	/**
	 * Parses any command and executes actions based on that.
	 */
	BOOL ParseCommand(BrowserDesktopWindow* bw, const OpString& cmd);

	/**
	 * Parses the non-browser specific command and executes
	 * actions based on that.
	 */
	BOOL ParseWindowCommand(DesktopWindow* dw, const OpString& cmd);

	/**
	 * Internal parse functions. Intended to be used by @ref ParseCommand
	 */
	BOOL ParseDestinationCommand(const OpString& cmd, ParsedData& pd);
	BOOL ParseOpenUrlCommand(const OpString& cmd, ParsedData& pd);
}

#endif
