/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef COCOA_POSIX_SELECTOR_H
#define COCOA_POSIX_SELECTOR_H

#if !defined(POSIX_OK_SIMPLE_SELECTOR)
#include "platforms/posix/posix_selector.h"
#define BOOL NSBOOL
#import <Foundation/NSPort.h>
#import <Foundation/NSRunLoop.h>
#undef BOOL

class CocoaPosixSelector : public PosixSelector
{
public:
	CocoaPosixSelector();
	virtual ~CocoaPosixSelector();


	/** Detach a listener, optionally from just one file handle.
	 *
	 * Note that these do not call the listener's OnDetach method; that's
	 * reserved for when PosixSelector initiated the detach.  Caller may call
	 * listener's OnDetach() after Detach()ing it, if appropriate.
	 *
	 * If no file descriptor is specified, all uses of the listener are
	 * detached; in particular, the listener base class's destructor calls this
	 * form of Detach(), so derived classes don't need to pay particular
	 * attention to ensuring they are detached when destroyed.  If a file
	 * descriptor is specified, all uses of the given listener for that file
	 * descriptor shall be detached (this behaviour shall change if someone
	 * proposes a reasonable use case).
	 *
	 * @param whom PosixSelectListener to be detached.
	 * @param fd Optionally only detach from listening to this file descriptor.
	 */
	virtual void Detach(const PosixSelectListener *whom, int fd);
	/**
	 * @overload
	 */
	virtual void Detach(const PosixSelectListener *whom);

	/** Type of listening to perform. */
	enum Type {
		READ = 1,		///< Watch for incoming data.
		WRITE = 2,		///< Watch for readiness to deliver outgoing data.
		CONVERSE = 3	///< Watch for both of the above.
	};

	/** Begin watching a file descriptor.
	 *
	 * All file descriptors being Watch()ed are checked for errors.  Supplying a
	 * socket address shall cause PosixSelector to connect() the given file
	 * descriptor (in fact, to attempt this repeatedly; the POSIX APIs are
	 * ambiguous about the existence of any other way to determine when
	 * connect() has completed, if it fails to complete immediately).  Only
	 * after connect() has completed shall this file descriptor be checked for
	 * read or write readiness (but it still shall be checked for errors).
	 *
	 * The PosixSelector shall, from time to time, check for activity on all
	 * file handles it's Watch()ing.  The time interval between such checks
	 * shall be varied dynamically (see PosixIrregular) subject to an upper
	 * bound.  Each file can specify a suitable upper bound; the shortest shall
	 * be honoured (if shorter than the default upper bound, which is 10
	 * seconds).  Since, in practice, the adaptive timing algorithm shall ensure
	 * that the actual interval between checks shall be shorter, be lenient in
	 * your setting of upper bounds; setting the value too low is apt to have an
	 * adverse impact on Opera's performance without materially affecting your
	 * file access.
	 *
	 * @param fd File descriptor.
	 * @param mode Type of activity to watch for (see enum Type).
	 * @param interval Upper bound on number of milliseconds between checks.
	 * @param listener Entity to notify when activity is noticed.
	 * @param connect Optional OpSocketAddress for connecting, else NULL
	 * (ignored, but asserted NULL, unless API_POSIX_SOCKADDR is imported).
	 * @return See OpStatus.
	 */
	virtual OP_STATUS Watch(int fd, PosixSelector::Type mode, unsigned long interval, PosixSelectListener *listener,
							const class OpSocketAddress *connect);
private:
	OpVector<NSMachPort> m_ports;
};

#endif // !POSIX_OK_SIMPLE_SELECTOR

#endif //COCOA_POSIX_SELECTOR_H
