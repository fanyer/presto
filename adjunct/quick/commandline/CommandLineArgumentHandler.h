// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Created by Adam Minchinton on 6/19/07.
//

#ifndef __COMMANDLINEARGUMENTHANDLER_MANAGER_H__
#define __COMMANDLINEARGUMENTHANDLER_MANAGER_H__

#include "adjunct/quick/managers/CommandLineManager.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

enum CLArgType
{
	InvalidCLArg,
	StringCLArg,
	StringListCLArg,
	IntCLArg,
	FlagCLArg,
	OptCLArg
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

enum CLArgPlatform
{
	NoneCLArg	= 0x00, // Invalid value
	MacCLArg	= 0x01,
	UnixCLArg	= 0x02,
	WinCLArg	= 0x04,
	AllCLArg    = 0x07  // Appears on all platforms
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Stucture used to hold and parse the information from the command line and into the
// internal CommandLineArgument format
//
struct CLArgInfo
{
	const char		*arg;
	const char		*parameter;
	const char		*arg_alias; // For options where we have changed name. Old name here
	const char		*arg_help;
	BOOL			arg_restart; // Whether to keep this argument when restarting Opera (usually TRUE)
	CLArgType		arg_type;
	int				arg_platform;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CommandLineArgumentHandler
{
public:
	virtual ~CommandLineArgumentHandler() {}

	/** Create an CommandLineArgumentHandler object. */
	static OP_STATUS Create(CommandLineArgumentHandler** handler);
	
	OP_STATUS			CheckArg(CommandLineManager::CLArgument arg_name, const char* arg);
	OP_STATUS			Handle(CommandLineManager::CLArgument arg_name, CommandLineArgument *arg_value);

	virtual OP_STATUS	CheckURL(OpString& candidate) { return OpStatus::OK; }
	virtual int			OutputText(const char *format, ...);
	virtual int 		OutputHelp(const char *format, ...);
	virtual void        OutputExtraHelp( CommandLineManager::CLArgument arg_name, BOOL before_main_help );

protected:
	int 				OutputHelpEntries( int width );
	virtual OP_STATUS	CheckArgPlatform(CommandLineManager::CLArgument arg_name, const char* arg) = 0;
	virtual OP_STATUS	HandlePlatform(CommandLineManager::CLArgument arg_name, CommandLineArgument *arg_value) = 0;

	virtual CLArgPlatform	GetPlatform() = 0;
	virtual BOOL			HideHelp(CommandLineManager::CLArgument arg_name) = 0;
};

#endif // __COMMANDLINEARGUMENTHANDLER_MANAGER_H__
