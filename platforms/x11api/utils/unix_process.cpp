/* Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Tomasz Kupczyk tkupczyk@opera.com
 * Patryk Obara pobara@opera.com
 */

#include "core/pch.h"

#ifdef X11API

#include "platforms/x11api/utils/unix_process.h"

#if defined(__linux__)
# include <sys/prctl.h>
#endif // __linux__

#if defined(__FreeBSD__)
# include <sys/types.h>
# include <unistd.h>
#endif // __FreeBSD__


OP_STATUS UnixProcess::SetCallingProcessName(const OpStringC8& new_name)
{
#if defined(__linux__) && defined(PR_SET_NAME)
	// Length of name which is given to prctl

	const int MAX_NAME_LENGTH = 16;

	// Function prctl is supplied with process name that can
	// be up to 16 bytes long, and should be null terminated
	// if it contains fewer bytes, not sure how this will behave
	// with different encodings

	OpString8 truncated_new_name;
	RETURN_OOM_IF_NULL(truncated_new_name.Reserve(MAX_NAME_LENGTH));
	truncated_new_name.Wipe();

	RETURN_IF_ERROR(truncated_new_name.Insert(0, new_name, 16));

	// Set calling process name.
	if (prctl(PR_SET_NAME, truncated_new_name.CStr(), NULL, NULL, NULL) == (-1))
	{
		return OpStatus::ERR;
	}

#elif defined(__FreeBSD__)
	setproctitle("-%s", new_name.CStr());
#else
	OP_ASSERT(!"Not implemented SetCallingProcessName");
#endif // __linux__ && PR_SET_NAME

	return OpStatus::OK;
}


OP_STATUS UnixProcess::GetExecPath(OpString8& exec)
{
	char buf[PATH_MAX]; // ARRAY OK molsson 2011-12-09
	char procfs_link[PATH_MAX]; // ARRAY OK molsson 2011-12-08
	int  endpos;

#if defined(__FreeBSD__) || defined(__NetBSD__)
	// *bsd
	op_sprintf(procfs_link, "/proc/%i/file", getpid());
	endpos = readlink(procfs_link, buf, sizeof(buf));
#else
	// linux
	endpos = readlink("/proc/self/exe", buf, sizeof(buf));
	if (endpos <= 0)
	{
		// solaris
		op_sprintf(procfs_link, "/proc/%i/path/a.out", getpid());
		endpos = readlink(procfs_link, buf, sizeof(buf));
	}
#endif

	if (endpos <= 0)
		return OpStatus::ERR;
	buf[endpos] = '\0';
	return exec.Set(buf);
}

#endif // X11API
