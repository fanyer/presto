// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Julien Picalausa
//

#ifndef COMMANDLINEARGUMENTHANDLER_H
#define COMMANDLINEARGUMENTHANDLER_H

#include "adjunct/quick/commandline/CommandLineArgumentHandler.h"

class WindowsOpCommandLineArgumentHandler : public CommandLineArgumentHandler
{
public:
	virtual OP_STATUS	CheckArgPlatform(CommandLineManager::CLArgument arg_name, const char* arg);
	virtual OP_STATUS	HandlePlatform(CommandLineManager::CLArgument arg_name, CommandLineArgument *arg_value);

	virtual CLArgPlatform	GetPlatform() { return WinCLArg; }
	virtual BOOL			HideHelp(CommandLineManager::CLArgument arg_name);
	
};

#endif //COMMANDLINEARGUMENTHANDLER_H
