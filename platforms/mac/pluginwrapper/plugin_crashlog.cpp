/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef NO_CORE_COMPONENTS

#include "platforms/mac/pluginwrapper/plugin_crashlog.h"
#include "platforms/crashlog/crashlog.h"

extern const char* g_plugin_crashlogfolder;

namespace PluginCrashlog
{
	OP_STATUS CreateDirectory(const char* log_location);
};

void PluginCrashlog::InstallHandler(const char* logfolder)
{
	g_plugin_crashlogfolder = logfolder;

	InstallCrashSignalHandler();
}

OP_STATUS PluginCrashlog::HandleCrash(pid_t pid, const char* log_location)
{
	OpAutoArray<char> filename (OP_NEWA(char, PATH_MAX));
	RETURN_OOM_IF_NULL(filename.get());

	// Make sure log location exists
	RETURN_IF_ERROR(CreateDirectory(log_location));

	WriteCrashlog(pid, NULL, filename.get(), PATH_MAX, log_location);

	return OpStatus::OK;
}

OP_STATUS PluginCrashlog::CreateDirectory(const char* dirpath)
{
	size_t length = op_strlen(dirpath);
	if (length == 0)
		return OpStatus::ERR;

	OpAutoArray<char> workpath (OP_NEWA(char, length + 1));
	RETURN_OOM_IF_NULL(workpath.get());

	op_strlcpy(workpath.get(), dirpath, length + (dirpath[length - 1] == PATHSEPCHAR ? 0 : 1));

	for (char* nextdir = op_strchr(workpath.get() + 1, PATHSEPCHAR); nextdir; nextdir = op_strchr(nextdir + 1, PATHSEPCHAR))
	{
		*nextdir = '\0';
		int accessible = access(workpath.get(), F_OK);
		if (accessible < 0 && (errno != ENOENT || mkdir(workpath.get(), 0777) < 0))
			return OpStatus::ERR;
		*nextdir = PATHSEPCHAR;
	}

	int accessible = access(workpath.get(), F_OK);
	if (accessible < 0 && (errno != ENOENT || mkdir(workpath.get(), 0777) < 0))
		return OpStatus::ERR;

	return OpStatus::OK;
}

#endif // NO_CORE_COMPONENTS
