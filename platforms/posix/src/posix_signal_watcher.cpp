/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4; -*-
 *
 * Copyright (C) 2007-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"
#ifdef POSIX_OK_SIGNAL
#include "platforms/posix/posix_signal_watcher.h"

#ifdef sun
/* On sun, actual signal is one-shot, reverting to SIG_DFL, which is terminate;
 * sun's sigset does what signal does elsewhere, so use it instead. */
#define signal sigset
#endif

void PosixSignalWatcher::SigClear()
{
	sigemptyset(m_mask);
	sigemptyset(m_mask + 1);
}

inline PosixSignalWatcher::SingleSignal *PosixSignalWatcher::Find(int signum)
{
	for (SingleSignal* run = First(); run; run = run->Suc())
		if (run->GetSigNum() == signum)
			return run;

	return NULL;
}

inline OP_STATUS PosixSignalWatcher::Add(int signum, PosixSignalListener* listener)
{
	if (SingleSignal* sig = Find(signum))
	{
		listener->Into(sig);
		return OpStatus::OK;
	}

	SingleSignal *fresh = OP_NEW(SingleSignal, (signum, this));
	if (fresh == 0)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS res = fresh->Init();
	if (OpStatus::IsSuccess(res))
	{
		fresh->Into(this);
		listener->Into(fresh);
	}
	else
		OP_DELETE(fresh);

	return res;
}

// static
OP_STATUS PosixSignalWatcher::Watch(int signum, PosixSignalListener * listener)
{
	if (!g_opera)
		return OpStatus::ERR_NULL_POINTER;

	PosixSignalWatcher * vakt = g_opera->posix_module.GetSignalWatcher();
	if (!vakt)
	{
		vakt = OP_NEW(PosixSignalWatcher, ());
		if (vakt == NULL)
			return OpStatus::ERR_NO_MEMORY;

		OP_STATUS res = vakt->Ready();
		if (OpStatus::IsSuccess(res))
			g_opera->posix_module.SetSignalWatcher(vakt);
		else
		{
			OP_DELETE(vakt);
			return res;
		}
	}
	else if (vakt->Empty()) // => paused
		vakt->Resume();

	return vakt->Add(signum, listener);
}

inline void PosixSignalWatcher::SingleSignal::SignalAll()
{
	for (PosixSignalListener *run = First(); run; run = static_cast<PosixSignalListener *>(run->Suc()))
		run->OnSignalled(m_signum);
}

void PosixSignalWatcher::OnPressed()
{
	if (Empty()) return;
	// assert: m_mask[!m_flip] is currently clear
	m_flip = !m_flip;
	m_count = 0;

	/* It's possible for signals to arrive during our calls back to listeners;
	 * and, indeed, while we're scanning a signal mask to see which to have been
	 * relevant lately.  Hence the need for two signal masks; we can now report
	 * what's held in m_mask[!m_flip], which was m_mask[m_flip] until we toggled
	 * m_flip and transcribed m_count.  We only care about the signals for which
	 * we have listeners; and we may as well purge any signals which used to
	 * have listeners but no longer do.
	 */
	SingleSignal* run = First();
	while (run)
	{
		SingleSignal* next = run->Suc();
		if (run->Empty())
		{
			// All its listeners have gone away: kill it !
			run->Out();
			OP_DELETE(run);
		}
		else
			switch (sigismember(m_mask + !m_flip, run->GetSigNum())) // should be 0 or 1
			{
			case 1:
				run->SignalAll();
				break;
			default:
				OP_ASSERT(!"How did a bad signal number get in here ?");
				/* A return of -1 means bad signal number, which I read as "not
				 * a member of our set". */
			case 0:
				break;
			}
		run = next;
	}

	sigemptyset(m_mask + !m_flip);
	if (m_count != 0)
		Press(); // We need to do this again already !
}

PosixSignalWatcher::SingleSignal::~SingleSignal()
{
#ifdef POSIX_USE_SIGNAL
	if (m_prior != SIG_ERR && signal(m_signum, m_prior) == SIG_ERR)
#else
	if (m_prior.sa_handler != SIG_ERR && sigaction(m_signum, &m_prior, NULL))
#endif
		OP_ASSERT(!"Failed to restore prior signal handler !");
}

#ifdef POSIX_USE_SIGNAL
inline void PosixSignalWatcher::SingleSignal::Relay()
{
	if (m_prior != SIG_ERR &&
		m_prior != SIG_DFL &&
		m_prior != SIG_IGN &&
		(UINTPTR) m_prior > 0xff) // in case there are further magic values
		(*m_prior)(m_signum);
}

inline void PosixSignalWatcher::Witness(int signum)
{
	sigaddset(m_mask + m_flip, signum);
	m_count++;
	// FIXME: is it safe to call Find in a signal handler ?
	if (SingleSignal *sig = Find(signum))
		sig->Relay();
	Press();
}

static void generic_handler(int signum)
{
	if (g_opera)
		if (PosixSignalWatcher *eye = g_opera->posix_module.GetSignalWatcher())
			eye->Witness(signum);
}
#else
inline void PosixSignalWatcher::SingleSignal::Relay(siginfo_t *info, void *data)
{
	if (m_prior.sa_flags & SA_SIGINFO)
		m_prior.sa_sigaction(m_signum, info, data);

	else if (m_prior.sa_handler != SIG_ERR &&
			 m_prior.sa_handler != SIG_DFL &&
			 m_prior.sa_handler != SIG_IGN &&
			 (UINTPTR) m_prior.sa_handler > 0xff) // in case there are further magic values
		(*m_prior.sa_handler)(m_signum);
}

inline void PosixSignalWatcher::Witness(int signum, siginfo_t *info, void *data)
{
	sigaddset(m_mask + m_flip, signum);
	m_count++;
	// FIXME: is it safe to call Find in a signal handler ?
	if (SingleSignal *sig = Find(signum))
		sig->Relay(info, data);
	Press();
}

static void generic_handler(int signum, siginfo_t *info, void *data)
{
	if (g_opera)
		if (PosixSignalWatcher *eye = g_opera->posix_module.GetSignalWatcher())
			eye->Witness(signum, info, data);
}

/** Transcribe signal mask.
 *
 * Effectively does *dst = *src; except that sigset_t may be too big to let us
 * do that.  Sadly, there is no sigcopyset() function in POSIX, so we have to
 * roll our own.
 *
 * @param dst Signal mask to populate.
 * @param src Signal mask to duplicate.
 */
static void op_sigcopyset(sigset_t *dst, const sigset_t *src)
{
#if 1
	op_memcpy(dst, src, sizeof(sigset_t));
#else
	const int sig_max = CHAR_BIT * sizeof(sigset_t);
	for (int sig = 0; sig < sig_max; sig++)
		/* FIXME: Mac's sigismember apparently doesn't fail (return -1) when sig
		 * is out of range - it just merrily returns 0.  For that matter, do we
		 * know Linux does any better ?  Apparently POSIX 1988 specified only 0
		 * and 1 as returns; a later spec has added the option of failing. */
		switch (sigismember(src, sig))
		{
		case 1: sigaddset(dst, sig); break;
		case 0: break;
		default: return;
		}
#endif
}
#endif // POSIX_USE_SIGNAL

OP_STATUS PosixSignalWatcher::SingleSignal::Init()
{
	/* An alternative strategy would be to set the process's signal mask, using
	 * pthread_sigmask (or just sigmask if no THREAD_SUPPORT), and have
	 * OnPressed call sigpending() to find out what signals have happened since
	 * last time.  It could open up the signal mask with sigsuspend (to let
	 * these signals all happen) then resume blocking them and talk to the
	 * call-backs about them.
	 *
	 * This would have the advantage of not changing the handlers for signals
	 * (which we don't, really, need to do), thereby reducing the scope for
	 * collisions with other things frobbing the signal handlers.
	 */

	errno = 0;
#ifdef POSIX_USE_SIGNAL
	m_prior = signal(m_signum, &generic_handler);
	if (m_prior != SIG_ERR)
		return OpStatus::OK;
#else
	struct sigaction fresh;
	fresh.sa_sigaction = &generic_handler;
	sigemptyset(&(fresh.sa_mask)); // but see below, if run.
	fresh.sa_flags = SA_SIGINFO;

	if (sigaction(m_signum, &fresh, &m_prior) == 0)
	{
		bool run = false;
		if (m_prior.sa_flags & SA_SIGINFO)
			run = true;
		else if (m_prior.sa_handler != SIG_ERR &&
				 m_prior.sa_handler != SIG_DFL &&
				 m_prior.sa_handler != SIG_IGN &&
				 (UINTPTR) m_prior.sa_handler > 0xff) // in case there are further magic values
			run = true;

		if (run)
		{
			// We'll recurse into the prior handler, so set mask and flags the way it likes:
			op_sigcopyset(&fresh.sa_mask, &m_prior.sa_mask);
			fresh.sa_flags = SA_SIGINFO | m_prior.sa_flags;
			sigaction(m_signum, &fresh, NULL);
		}
		return OpStatus::OK;
	}

	// Make sure we know not to ignore m_prior hereafter:
	m_prior.sa_handler = SIG_ERR;
	m_prior.sa_flags = 0;
#endif
	switch (errno)
	{
	case EINVAL: return OpStatus::ERR_OUT_OF_RANGE;
	case ENOTSUP:
		// SA_SIGINFO requires us to set sa_sigaction, not sa_handler.
		OP_ASSERT(!"This should only happen if we set the SA_SIGINFO flag - which we shouldn't !");
		return OpStatus::ERR_NOT_SUPPORTED;
	}
	return OpStatus::ERR;
}

#endif // POSIX_OK_SIGNAL
