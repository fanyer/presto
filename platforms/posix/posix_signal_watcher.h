/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4; -*-
 *
 * Copyright (C) 2007-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef POSIX_SIGNAL_WATCHER_H
#define POSIX_SIGNAL_WATCHER_H
# ifdef POSIX_OK_SIGNAL
#include "platforms/posix/posix_selector.h"		// PosixSelector::Button
#include "modules/util/simset.h"				// Head/Link
#include <signal.h>								// sigset_t, sigaction, SIG_ERR

/** Base-class for things that need to know when signals have happened.
 *
 * See PosixSignalWatcher for details.  Registering a listener for a signal
 * implicitly causes that signal to be caught and handled (albeit minimally, by
 * setting a flag).  Note that each listener can only listen for one signal, as
 * a Link can only be in one Head's list at any given time.  The listener shall,
 * furthermore, be deleted when the PosixSignalWatcher is deleted.  Clients are
 * thus encouraged to have dedicated light-weight listener objects associated
 * with their more substantial entities, rather than having substantial entities
 * inherit from PosixSignalListener.
 */
class PosixSignalListener : public Link
{
public:
	/** Destructor.
	 *
	 * To unregister a listener, simply delete it.
	 */
	virtual ~PosixSignalListener() { if (InList()) Out(); }

	/** Call-back when a signal has been noticed.
	 *
	 * This is called in the course of normal execution, on the main thread,
	 * some time after the signal was actually caught.  The catcher just set a
	 * flag and returned; the PosixSignalWatcher has just noticed that flag.
	 */
	virtual void OnSignalled(int signum) = 0; // possibly add a datum ?
};

/** Manager for catching and reporting signals.
 *
 * Each watched signal is given a signal handler which simply records that the
 * signal has been seen; this signal watcher sporadically reviews which signals
 * have been seen, calling all registered listeners for each signal seen since
 * last review.
 *
 * For the full list of signals, see <code><bits/signum.h></code> - but note
 * that any not defined by POSIX should only be used within
 * <code>\#if</code>-ery. On GNU/Linux, <code><bits/signum.h></code> marks the
 * POSIX ones as "(POSIX)" in the descriptions. The more obviously interesting
 * of these are: \c SIGUSR1, \c SIGUSR2 (user-defined; Opera does not,
 * presently, define any meanings for them, but we could), \c SIGPIPE (Broken
 * pipe; but also socket closed), \c SIGCHLD (Child status has changed;
 * typically child process has exited). The following signals from BSD may also
 * be relevant: \c SIGURG (Urgent condition on socket), \c SIGIO (I/O now
 * possible, a.k.a. \c SIGPOLL: Pollable event occurred).
 *
 * Complication not yet addressed in this design (let alone implementation):
 * signals are sent to distinct threads separately, so the signal watcher only
 * watches for signals on the main thread (strictly: the thread that first
 * called Watch, for each signal).
 */
class PosixSignalWatcher : public PosixSelector::Button, public AutoDeleteHead
{
	sigset_t m_mask[2]; ///< Signals we've seen and not yet reported.
	size_t m_count; ///< Number of signals caught since (Init() or) last Report().
	unsigned int m_flip : 1; ///< m_mask[m_flip] is currently active.

	class SingleSignal : public AutoDeleteHead, public Link
	{
		const int m_signum;
#ifdef POSIX_USE_SIGNAL
		void (*m_prior)(int);
#else
		struct sigaction m_prior;
#endif

		// PosixSignalListener *First() { return (PosixSignalListener *)Head::First(); }
	public:
		SingleSignal(int signum, PosixSignalWatcher *boss)
			: m_signum(signum)
#ifdef POSIX_USE_SIGNAL
			, m_prior(SIG_ERR) {}
#else
			{ m_prior.sa_handler = SIG_ERR; m_prior.sa_flags = 0; }
#endif
		int GetSigNum() const { return m_signum; }

		PosixSignalListener* First() const { return (PosixSignalListener*)Head::First(); }
		SingleSignal* Suc() const { return (SingleSignal*)Link::Suc(); }

		OP_STATUS Init();	///< Set up our signal handler, record what it replaces in m_prior
		void SignalAll();	///< Tell all listeners m_signum has been seen
		~SingleSignal();	///< Restore original signal status (if Init() succeeded)
#ifdef POSIX_USE_SIGNAL
		void Relay();
#else
		void Relay(siginfo_t *info, void *data);
#endif
	};

	SingleSignal *First() { return (SingleSignal*)Head::First(); }
	SingleSignal *Find(int signum);
	OP_STATUS Add(int signum, PosixSignalListener* listener);
	void SigClear();

	void Resume()
	{
		if (Empty())
		{
			SigClear();
			m_count = 0;
			// and I don't care which value m_flip has ;-)
		}
		else
			OP_ASSERT(!"Should only Resume() when Empty().");
	}
	PosixSignalWatcher() : m_count(0), m_flip(0) { SigClear(); }

public:
	// PosixSelector::Button API:
	/** Let listeners know about signals we've seen lately. */
	void OnPressed();

	/** Register a listener for a signal.
	 *
	 * This is static so that it can lazily create a signal watcher (looked
	 * after for us by the global PosixAsyncManager) only when it's actually
	 * needed.
	 */
	static OP_STATUS Watch(int signum, PosixSignalListener * listener);

	/// Called by the signal-handler used internally:
#ifdef POSIX_USE_SIGNAL
	void Witness(int signum);
#else
	void Witness(int signum, siginfo_t *info, void *data);
#endif
};

# endif // POSIX_OK_SIGNAL
#endif // POSIX_SIGNAL_WATCHER_H
