/* Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Tomasz Kupczyk tkupczyk@opera.com
 * Patryk Obara pobara@opera.com
 */

#ifndef UNIX_PROCESS_H
#define UNIX_PROCESS_H

namespace UnixProcess
{
	/**
	 * Set calling process name. Actually this function is cheating
	 * top, some GUI system monitors and simple ps invocations.
	 *
	 * @param[in] new_name New name for the calling process
	 * @return Error status
	 */
	OP_STATUS SetCallingProcessName(const OpStringC8& new_name);

	/**
	 * Get absolute path to binary, that started this process.
	 * Use with caution, usually when you think you can use this, you really
	 * shouldn't (but still may be useful for some quick fixes and testing
	 * purposes).
	 *
	 * @param[out] exec
	 * @return Error status
	 */
	OP_STATUS GetExecPath(OpString8& exec);

};

#endif // !UNIX_PROCESS_H
