/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef PLUGIN_CRASHLOG_H
#define PLUGIN_CRASHLOG_H

/** @brief Implementations related to crash logging from the plugin wrapper
 */
namespace PluginCrashlog
{
	/** Setup signals to install the crash handler 
	  * @param pathname Path to the executable we're running
	  * @param logfolder Path to log to when crashes happen
	  */
	void InstallHandler(const char* pathname, const char* logfolder);

	/** Handle a crash happening in another process
	  * @param pid Process ID of the crashing process
	  * @param log_location Where to write logs
	  */
	OP_STATUS HandleCrash(pid_t pid, const char* log_location);
};

#endif // PLUGIN_CRASHLOG_H
