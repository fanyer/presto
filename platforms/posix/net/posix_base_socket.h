/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef POSIX_BASE_SOCKET_H
#define POSIX_BASE_SOCKET_H __FILE__
#ifdef POSIX_OK_SELECT

#include "platforms/posix/posix_logger.h"		// PosixLogger
#include "platforms/posix/posix_selector.h"		// PosixSelectListener
#ifdef DEBUG_ENABLE_OPASSERT
#include "modules/pi/OpSystemInfo.h" // for IsInMainThread()
#endif
#include <sys/socket.h>

/** Utility base-class for socket-related classes.
 *
 * Presently minimal, but serves to avoid a little code duplication.
 */
class PosixBaseSocket : public PosixLogger
{
protected:
	/** Initialize for logging, defaulting to the SOCKET listener. */
	PosixBaseSocket(LoggerType type=SOCKET) : PosixLogger(type) {}
	/* DecodeError - but OpSocket wants its ::Error, OpUdpSocket wants
	 * OP_NETWORK_STATUS as return value :-( Eventually go for the latter. */

	/** Set close-on-exec and non-blocking flags on fd.
	 *
	 * @param fd File descriptor of a freshly-opened socket.
	 * @param err Pointer to where to store errno value on error.
	 * @return On success, true: else false.
	 */
	bool SetSocketFlags(int fd, int *err);

#ifdef DEBUG_ENABLE_OPASSERT
	/** For use in assertions, to check whether on the core thread.
	 *
	 * @return True if Opera isn't live (in which case we presume there \em is
	 * only one thread) or if OpSystemInfo reports that we're on core's thread.
	 */
	static bool OnCoreThread()
	{
# ifdef THREAD_SUPPORT
		return !g_opera || g_opera->posix_module.OnCoreThread();
# else
		return true;
# endif
	}
#endif // DEBUG_ENABLE_OPASSERT and THREAD_SUPPORT

	/** Wrapper round socket.
	 *
	 * The devices team has seen platforms in which socket() may return 0
	 * instead of -1 on error.  A return of 0 (stdin), 1 (stdout) or 2 (stderr)
	 * is clearly borked, so we should trap these.
	 *
	 * Parameters to and return from this function are as for socket, q.v., but
	 * return (and errno) are sanitised if TWEAK_POSIX_BORKED_SOCKET is enabled.
	 */
	static int OpenSocket(int domain, int type, int protocol)
	{
		int fd = socket(domain, type, protocol);
#ifdef POSIX_BORKED_SOCKET_HACK
		if (fd < 3)
		{
			const int err = errno;
			OP_ASSERT(fd == -1); // but ROCCO-16 has seen 0, stdin, as return !
			OP_ASSERT(err);
			errno = err ? err : ENFILE; // at a guess.
			return -1;
		}
#else
		OP_ASSERT(fd == -1 || fd > 2); // if fd is 0, 1 or 2: you need TWEAK_POSIX_BORKED_SOCKET = YES
#endif // POSIX_BORKED_SOCKET_HACK
		return fd;
	}

	/** Listener for a socket that's been passed to bind().
	 *
	 * Base class from which different socket types can inherit.  Makes
	 * OnWriteReady and OnConnected assert, when possible, so we notice
	 * implausible usage.  Derived classes need to implement OnReadReady() and
	 * OnError().
	 */
	class SocketBindListener : public PosixSelectListener
	{
	public:
# ifdef DEBUG_ENABLE_OPASSERT
		virtual void OnWriteReady(int fd)
			{ OP_ASSERT(!"Write notification not requested, how come one arrived ?"); }
		virtual void OnConnected(int fd)
			{ OP_ASSERT(!"No remote address passed to Watch(), how come we connected ?"); }
# endif
	};
};

# endif // SELECT

// Convenience macro, AGAINCASE, for EAGAIN and/or EWOULDBLOCK as case label(s)
# if EAGAIN == EWOULDBLOCK
#define AGAINCASE EAGAIN
# else
#define AGAINCASE EAGAIN: case EWOULDBLOCK
# endif // EAGAIN == EWOULDBLOCK
#endif // POSIX_BASE_SOCKET_H
