/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @file
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
#ifndef ST_CMD_LINE_UTILS_H
#define ST_CMD_LINE_UTILS_H

#ifdef SELFTEST

#include "adjunct/quick/managers/CommandLineManager.h"

struct ST_CmdLineUtils
{
	static OP_STATUS ClearArguments()
	{
		const char* const empty_args[] = {""};
		RETURN_IF_ERROR(CommandLineManager::GetInstance()->Init());
		RETURN_IF_ERROR(CommandLineManager::GetInstance()->ParseArguments(
				1, empty_args));
		return OpStatus::OK;
	}

	static OP_STATUS SetArguments(const char* const* argv, size_t argc = 3)
	{
		RETURN_IF_ERROR(ClearArguments());
		RETURN_IF_ERROR(CommandLineManager::GetInstance()->ParseArguments(
					argc, argv));
		return OpStatus::OK;
	}

	static OP_STATUS SetArguments(const char* const* argv,
			const uni_char* const* wargv, size_t argc = 3)
	{
		RETURN_IF_ERROR(ClearArguments());
		RETURN_IF_ERROR(CommandLineManager::GetInstance()->ParseArguments(
					argc, argv, wargv));
		return OpStatus::OK;
	}
};

#endif // SELFTEST
#endif // ST_CMD_LINE_UTILS_H
