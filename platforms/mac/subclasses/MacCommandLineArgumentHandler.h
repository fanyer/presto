/*
 *  MacCommandLineArgumentHandler.h
 *  Opera
 *
 *  Created by Adam Minchinton on 6/19/07.
 *  Copyright 2007 Opera. All rights reserved.
 *
 */


#ifndef __MACCOMMANDLINEARGUMENTHANDLER_MANAGER_H__
#define __MACCOMMANDLINEARGUMENTHANDLER_MANAGER_H__

#include "adjunct/quick/commandline/CommandLineArgumentHandler.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

class MacCommandLineArgumentHandler : public CommandLineArgumentHandler
{
public:
	virtual ~MacCommandLineArgumentHandler() {}

	OP_STATUS CheckArgPlatform(CommandLineManager::CLArgument arg_name, const char* arg);
	OP_STATUS HandlePlatform(CommandLineManager::CLArgument arg_name, CommandLineArgument *arg_value);

	CLArgPlatform	GetPlatform() { return MacCLArg; }
	BOOL			HideHelp(CommandLineManager::CLArgument arg_name);
	
	virtual int		OutputText(const char *format, ...);
};

#endif // __MACCOMMANDLINEARGUMENTHANDLER_MANAGER_H__
