/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */

#ifndef _QUIX_COMMANDLINEARGUMENTHANDLER_H_
#define _QUIX_COMMANDLINEARGUMENTHANDLER_H_

#include "adjunct/quick/commandline/CommandLineArgumentHandler.h"
#include "modules/util/adt/opvector.h"

class QuixCommandLineArgumentHandler : public CommandLineArgumentHandler
{
public:
	~QuixCommandLineArgumentHandler();
	int OutputText(const char *format, ...);
	void OutputExtraHelp( CommandLineManager::CLArgument arg_name, BOOL before_main_help );
	OP_STATUS CheckURL(OpString& candidate);

	/**
	 * Tests url argument and resolves it to a local file
	 * if necessary. The url is converted from locale encoding
	 * as well
	 *
	 * @param candidate The url to be tested
	 * 
	 * @return OpStatus::OK on success otherwise an error code
	 */
	static OP_STATUS DecodeURL(OpString& candidate);

protected:
	OP_STATUS CheckArgPlatform(CommandLineManager::CLArgument arg_name, const char* arg);
	OP_STATUS HandlePlatform(CommandLineManager::CLArgument arg_name, CommandLineArgument *arg_value);
	CLArgPlatform GetPlatform() { return UnixCLArg; }
	BOOL HideHelp(CommandLineManager::CLArgument arg_name);

private:
	class PosixLogListener *ReadPosixDebugArg(CommandLineManager::CLArgument);
	BOOL IsHexIntValue(const char* src, long& value);
};

#endif // _QUIX_COMMANDLINEARGUMENTHANDLER_H_
