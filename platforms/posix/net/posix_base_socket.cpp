/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"
# ifndef POSIX_INTERNAL
#define POSIX_INTERNAL(X) X
# endif // Defined here because this file is first in posix_jumbo.cpp

#ifdef POSIX_OK_SELECT
#include "platforms/posix/net/posix_base_socket.h"

bool PosixBaseSocket::SetSocketFlags(int fd, int *err)
{
	// Set suitable options on a socket:
	errno = 0;
	int flags = fcntl(fd, F_GETFD, 0);
	int res = flags + 1 ? fcntl(fd, F_SETFD, flags | FD_CLOEXEC) : -1;
	if (res != -1)
	{
#if FD_CLOEXEC
		OP_ASSERT(errno == 0 && fcntl(fd, F_GETFD, 0) & FD_CLOEXEC);
#elif defined(__GNUC__)
# warning "FD_CLOEXEC is zero: close-on-exec may be unsupported"
#endif // but might simply be on-by-default
		flags = fcntl(fd, F_GETFL, 0);
		res = flags + 1 ? fcntl(fd, F_SETFL, flags | O_NONBLOCK) : -1;
	}

	if (res == -1)
	{
		*err = errno;
		Log(TERSE, "%010p: Failed to configure socket flags: %s\n",
			this, strerror(*err));

		return false;
	}
#if O_NONBLOCK
	OP_ASSERT(errno == 0 && fcntl(fd, F_GETFL, 0) & O_NONBLOCK);
#elif defined(__GNUC__)
# warning "O_NONBLOCK is zero: non-blocking I/O may be unsupported"
#endif // or it might simply be on by default
	return true;
}

#endif // POSIX_OK_SELECT
