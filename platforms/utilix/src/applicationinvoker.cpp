/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef UTILIX

#include <time.h>
#include <sys/poll.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "platforms/utilix/applicationinvoker.h"
#include "modules/memory/src/memory_debug.h"

ApplicationInvoker * ApplicationInvoker::g_application_list = 0;

ApplicationInvoker::ApplicationInvoker()
	: m_exec_file(NULL)
	, m_args(NULL)
	, m_args_allocated(0)
	, m_is_dead(false)
	, m_has_wait_status(false)
	, m_wait_status(0)
	, m_run_in_terminal(false)
	, m_fd_close(0)
	, m_fd_close_allocated(0)
	, m_fd_close_used(0)
	, m_stop_stdin(false)
	, m_redirect_stdout(false)
	, m_fd_stdout(-1)
	, m_forked(false)
	, m_dead_pids(0)
	, m_dead_pids_length(0)
	, m_dead_pids_used(0)
	, m_child_pid(-1)
	, m_errno(0)
	, m_next(0)
{
	if (g_application_list!=0)
		m_next = g_application_list->m_next;
	g_application_list = this;

};

ApplicationInvoker::~ApplicationInvoker()
{
	OP_ASSERT(g_application_list != 0);
	if (g_application_list == this)
		g_application_list = m_next;
	else
	{
		ApplicationInvoker * p = g_application_list;
		while (p->m_next != 0 && p->m_next != this)
			p=p->m_next;
		OP_ASSERT(p->m_next != 0);
		if (p->m_next != 0)
			p->m_next = m_next;
	};
	OP_DELETEA(m_exec_file);
	if (m_args != 0)
	{
		for (int i = 0; m_args[i] != 0; i++)
			OP_DELETEA(m_args[i]);
		OP_DELETEA(m_args);
	};
	OP_DELETEA(m_fd_close);
};


OP_STATUS ApplicationInvoker::Construct(const char * filename)
{
	OP_ASSERT(m_exec_file == NULL);
	int fnlen = op_strlen(filename) + 1;
	m_exec_file = OP_NEWA(char, fnlen);
	RETURN_OOM_IF_NULL(m_exec_file);
	op_memcpy(m_exec_file, filename, fnlen);

	OP_ASSERT(m_args_allocated == 0 && m_args == NULL);
	m_args = OP_NEWA(char *, 8);
	RETURN_OOM_IF_NULL(m_args);
	m_args_allocated = 8;
	m_args[0] = OP_NEWA(char, fnlen);
	RETURN_OOM_IF_NULL(m_args[0]);
	op_memcpy(m_args[0], filename, fnlen);
	m_args[1] = 0;
	return OpStatus::OK;
}


OP_STATUS ApplicationInvoker::SetArgv0(const char * arg0)
{
	if (m_child_pid != -1)
		return OpStatus::ERR;
	if (arg0 == 0)
		return OpStatus::ERR_NULL_POINTER;
	OP_ASSERT(m_args);
	int len = op_strlen(arg0) + 1;
	char * oldargv0 = m_args[0];
	m_args[0] = OP_NEWA(char, len);
	if (m_args[0] == 0)
	{
		m_args[0] = oldargv0;
		return OpStatus::ERR_NO_MEMORY;
	};
	OP_DELETEA(oldargv0);
	op_memcpy(m_args[0], arg0, len);
	return OpStatus::OK;
};


OP_STATUS ApplicationInvoker::AppendArgument(const char * arg)
{
	if (m_child_pid != -1)
		return OpStatus::ERR;
	if (arg == 0)
		return OpStatus::ERR_NULL_POINTER;
	OP_ASSERT(m_args);
	int arglen = op_strlen(arg) + 1;
	int argno = 0;
	while(m_args[argno] != 0)
		argno++;
	if (argno + 1 >= m_args_allocated)
	{
		int na = argno + 6;
		char ** nb = OP_NEWA(char *, na);
		if (nb == 0)
			return OpStatus::ERR_NO_MEMORY;
		for (int i = 0; i < argno; i++)
			nb[i] = m_args[i];
		OP_DELETEA(m_args);
		m_args = nb;
		m_args_allocated = na;
	};

	m_args[argno] = OP_NEWA(char, arglen);
	if (m_args[argno] == 0)
		return OpStatus::ERR_NO_MEMORY;
	op_memcpy(m_args[argno], arg, arglen);
	m_args[argno+1] = 0;

	return OpStatus::OK;
};


OP_STATUS ApplicationInvoker::CaptureStdout()
{
	if (m_child_pid != -1)
		return OpStatus::ERR;
	m_redirect_stdout = true;
	return OpStatus::OK;
};


OP_STATUS ApplicationInvoker::CloseInChild(int fd)
{
	if (m_child_pid != -1)
		return OpStatus::ERR;
	if (m_fd_close_used + 1 > m_fd_close_allocated)
	{
		int na = m_fd_close_used + 5;
		int * nb = OP_NEWA(int, na);
		if (nb == 0)
			return OpStatus::ERR_NO_MEMORY;

		for (int i = 0; i<m_fd_close_used; i++)
			nb[i] = m_fd_close[i];
		OP_DELETE(m_fd_close);
		m_fd_close = nb;
	};

	int to_use = m_fd_close_used;
	m_fd_close_used ++;
	while (to_use > 0 && m_fd_close[to_use - 1] > fd)
	{
		m_fd_close[to_use] = m_fd_close[to_use - 1];
		to_use --;
	};

	m_fd_close[m_fd_close_used] = fd;
	return OpStatus::OK;
};

OP_STATUS ApplicationInvoker::RunInTerminal()
{
	if (m_child_pid != -1)
		return OpStatus::ERR;

	if (!m_run_in_terminal) // so we don't repeat ourselves
	{
		OP_ASSERT(m_args);

		/* Insert suitable preamble to the command-line, to launch it in a
		 * terminal.  c.f. FileHandlerManagerUtilities::GetTerminalCommand */
		const char *term = op_getenv("TERM");
		enum { XTERM, ETERM, KONSOLE, GNOME } variant;
		int count = 0;
		if (term == 0 ||
			// Anything that accepts -hold -e program [args ...]
			op_strcmp(term, "xterm") == 0 || op_strcmp(term, "aterm") == 0 ||
			op_strcmp(term, "rxvt") == 0 || op_strcmp(term, "urxvt") == 0)
		{
			count = 3;
			variant = XTERM;
		}
		else if (op_strcmp(term, "konsole") == 0)
		{
			count = 3;
			variant = KONSOLE;
		}
		else if (op_strcmp(term, "eterm") == 0)
		{
			count = 2;
			variant = ETERM;
		}
		else if (op_strcmp(term, "gnome-terminal") == 0)
		{
			count = 2;
			variant = GNOME;
		}
		else
		{
			/* Unrecognised.  In principle, it'd be nice to honour whatever we
			 * get: but dumb, screen, emacs, vt100 and a host of other
			 * possibilities make this impractical.  So just fall back on xterm.
			 */
			count = 3;
			term = 0;
			variant = XTERM;
		}

		int argno = 0;
		while (m_args[argno])
			argno++;
		OP_ASSERT(argno < m_args_allocated); // else something's been naughty earlier
		// Prepare to insert count entries at the start of m_args:
		if (argno + count >= m_args_allocated)
		{
			int na = argno + MAX(count, 6);
			char ** nb = OP_NEWA(char *, na);
			if (nb == 0)
				return OpStatus::ERR_NO_MEMORY;

			nb[argno + count] = 0;
			while (argno-- > 0)
				nb[argno + count] = m_args[argno];

			OP_DELETEA(m_args);
			m_args = nb;
			m_args_allocated = na;
		}
		else
		{
			m_args[argno + count] = 0;
			while (argno-- > 0)
				m_args[argno + count] = m_args[argno];
		}

		// first count entries are now free for our use - fill them:
		switch (variant)
		{
		case XTERM: { // includes rxvt variants, aterm and unset or unfamiliar $TERM
			const size_t len = term ? op_strlen(term) + 1 : 6;
			m_args[0] = OP_NEWA(char, len);
			m_args[1] = OP_NEWA(char, 6);
			m_args[2] = OP_NEWA(char, 3);
			if (m_args[0] && m_args[1] && m_args[2])
			{
				op_memcpy(m_args[0], term ? term : "xterm", len);
				op_memcpy(m_args[1], "-hold", 6);
				op_memcpy(m_args[2], "-e", 3);
			}
			else
			{
				OP_DELETEA(m_args[0]);
				OP_DELETEA(m_args[1]);
				OP_DELETEA(m_args[2]);
				m_args[0] = m_args[1] = m_args[2] = 0;
			}
		} break;

		case ETERM:
			m_args[0] = OP_NEWA(char, 6);
			m_args[1] = OP_NEWA(char, 8);
			if (m_args[0] && m_args[1])
			{
				op_memcpy(m_args[0], "eterm", 6);
				op_memcpy(m_args[1], "--pause", 8);
			}
			else
			{
				OP_DELETEA(m_args[0]);
				OP_DELETEA(m_args[1]);
				m_args[0] = m_args[1] = 0;
			}
			break;

		case KONSOLE:
			m_args[0] = OP_NEWA(char, 8);
			m_args[1] = OP_NEWA(char, 10);
			m_args[2] = OP_NEWA(char, 3);
			if (m_args[0] && m_args[1] && m_args[2])
			{
				op_memcpy(m_args[0], "konsole", 8);
				op_memcpy(m_args[1], "--noclose", 10);
				op_memcpy(m_args[2], "-e", 3);
			}
			else
			{
				OP_DELETEA(m_args[0]);
				OP_DELETEA(m_args[1]);
				OP_DELETEA(m_args[2]);
				m_args[0] = m_args[1] = m_args[2] = 0;
			}
			break;

		case GNOME:
			m_args[0] = OP_NEWA(char, 15);
			m_args[1] = OP_NEWA(char, 3);
			if (m_args[0] && m_args[1])
			{
				op_memcpy(m_args[0], "gnome-terminal", 15);
				op_memcpy(m_args[1], "-x", 3);
			}
			else
			{
				OP_DELETEA(m_args[0]);
				OP_DELETEA(m_args[1]);
				m_args[0] = m_args[1] = 0;
			}
			break;
		}

		if (m_args[0])
			m_run_in_terminal = true;
		else // allocating an entry in m_args failed: abort (gracefully)
		{
			// first, shunt args back left
			for (argno = 0; m_args[argno + count]; argno++)
				m_args[argno] = m_args[argno + count];
			m_args[argno] = 0;
			return OpStatus::ERR_NO_MEMORY;
		}
	}
	return OpStatus::OK;
}


OP_STATUS ApplicationInvoker::Invoke()
{
	OP_ASSERT(m_exec_file);
	OP_ASSERT(m_args);

	if (m_child_pid != -1)
	{
		m_errno = EALREADY; // FIXME: Find better errno value
		return OpStatus::ERR;
	};

	int stdout_pipe[2];
	if (m_redirect_stdout)
	{
		// FIXME: If pipe() fails, the error should be reported somehow...
		if (pipe(stdout_pipe)!=0)
			m_redirect_stdout = false;
	};


	// If more than 32 processes die before fork() returns, we may
	// have some trouble here.
	m_dead_pids = OP_NEWA(int, 32 * 2);
	m_dead_pids_length = 32;
	m_dead_pids_used = 0;
	m_forked = true;
	pid_t child = fork();
	if (child < 0)
	{
		m_errno = errno;
		m_forked = false;
		return OpStatus::ERR;
	};

	// TODO: (maybe) Detect that the child failed to exec, and return
	// suitable error.

	if (child > 0)
	{
		// parent
		m_child_pid = child;

		for (int i = 0; i<m_dead_pids_used; i++)
		{
			if (m_dead_pids[i * 2] == m_child_pid)
			{
				m_wait_status = m_dead_pids[i * 2 + 1];
				m_has_wait_status = true;
				m_is_dead = true;
			};
		};

		OP_DELETEA(m_dead_pids);
		m_dead_pids = 0;
		m_dead_pids_length = 0;
		m_dead_pids_used = 0;

		if (m_redirect_stdout)
		{
			close(stdout_pipe[1]);
			m_fd_stdout = stdout_pipe[0];
		};

		return OpStatus::OK;
	};

	// child process.  Exec the application.

#ifdef ENABLE_MEMORY_DEBUGGING
	//
	// When the memory debugger is enabled, we have to tell it that
	// we are now in a new process, or else memory logging would
	// get all garbled.
	//
	g_mem_debug->Action(MEMORY_ACTION_FORKED);
#endif

	if (m_stop_stdin)
	{
		int nullfd = open("/dev/null", O_RDONLY);
		if (nullfd != -1)
			dup2(nullfd, 0);
		close(nullfd);
	};

	if (m_redirect_stdout)
	{
		close(stdout_pipe[0]);
		dup2(stdout_pipe[1], 1);
		close(stdout_pipe[1]);
	};

	for (int i = 0; i<m_fd_close_used; i++)
	{
		if (i>0 && m_fd_close[i-1] != m_fd_close[i])
			close(m_fd_close[i]);
	};
	OP_DELETEA(m_fd_close);
	m_fd_close = 0;
	m_fd_close_used = 0;
	m_fd_close_allocated = 0;

	execv(m_exec_file, m_args);

	// That didn't go too well... Try to end with a signal
	raise(SIGABRT);
	// ... and if we're still alive
	_exit(1);
};



int ApplicationInvoker::ReadStdout(char * buf, int buflen, int timeout)
{
	if (m_child_pid == -1)
		return -2;
	if (m_fd_stdout == -1)
		return -2;

	return ReadFd(m_fd_stdout, buf, buflen, timeout);
};


void ApplicationInvoker::Terminate()
{
	if (m_child_pid == -1)
		return;
	if (m_is_dead)
		return;
	kill(m_child_pid, SIGTERM);
};

BOOL ApplicationInvoker::IsRunning(BOOL active_check /* = FALSE */)
{
	if (m_child_pid <= 0)
		return FALSE;

	if (active_check)
	{
		int err = kill(m_child_pid, 0);
		if (err == -1 && errno == ESRCH)
			m_is_dead = true;
	};

	return !m_is_dead;
};

void ApplicationInvoker::ProcessDied(pid_t pid, int status)
{
	bool foundprocess = false;
	ApplicationInvoker * p = g_application_list;
	while (p != 0)
	{
		if (p->m_child_pid == pid)
		{
			foundprocess = true;
			p->m_wait_status = status;
			p->m_has_wait_status = true;
			p->m_is_dead = true;
		}
		p = p->m_next;
	};

	if (foundprocess)
		return;

	// Maybe fork() haven't returned yet...
	p = g_application_list;
	while (p != 0)
	{
		if (p->m_child_pid == -1 && p->m_forked)
		{
			if (p->m_dead_pids_used < p->m_dead_pids_length)
			{
				p->m_dead_pids[p->m_dead_pids_used * 2] = pid;
				p->m_dead_pids[p->m_dead_pids_used * 2 + 1] = status;
				p->m_dead_pids_used ++;
			};
		};
		p = p->m_next;
	};
};


void ApplicationInvoker::WaitAllPids()
{
	bool match;
	do
	{
		match = false;
		ApplicationInvoker * p = g_application_list;
		while (p != 0)
		{
			if( !p->m_has_wait_status && !p->m_is_dead )
			{
				int status;
				pid_t pid = waitpid(p->m_child_pid, &status, WNOHANG);
				if( pid > 0 )
				{
					ProcessDied(pid, status);
					match = true;
					break;
				};
			};
			p = p->m_next;
		};
	} 
	while( match );
};

int ApplicationInvoker::ReadFd(int fd, char * buf, int buflen, int timeout)
{
	struct timeval stop;
	if (timeout > 0)
	{
		if (gettimeofday(&stop, 0) != 0)
			timeout = 0;

		if (timeout > 0)
		{
			int sec = timeout / 1000;
			int msec = timeout % 1000;
			stop.tv_sec += sec;
			stop.tv_usec += msec * 1000;
			if (stop.tv_usec > 1000000)
			{
				stop.tv_sec ++;
				stop.tv_usec -= 1000000;
			};
		};
	};

	struct pollfd pfd;
	pfd.fd = fd;
	pfd.events = POLLIN;

	bool done = false;
	while (!done)
	{
		int pollret = poll(&pfd, 1, timeout);
		if (pollret == 0)
			return 0;
		if ((pfd.revents & POLLIN) != 0)
		{
			ssize_t got = read(fd, buf, buflen);
			if (got > 0)
				return got;
			else if (got == 0)
				return -1;
			else
			{
				if (errno != EINTR && errno != EAGAIN)
					return -2;
			};
		};

		if (timeout > 0)
		{
			struct timeval now;
			if (gettimeofday(&now, 0) != 0)
				done = true;
			else
			{
				now.tv_sec = stop.tv_sec - now.tv_sec;
				now.tv_usec = stop.tv_usec - now.tv_usec;
				timeout = now.tv_sec * 1000 + now.tv_usec / 1000;
				if (timeout < 0)
					timeout = 0;
			};
		}
		if (timeout == 0)
			done = true;
	};
	return 0;
};

#endif // UTILIX

