/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Morten Stenshorne, Espen Sand, Edward Welbourne
 *
 * Platform dependency: UNIX (or, at least, POSIX).
 * Needs fork(), signal() (sigset on Sun), execvp(), waitpid(), _exit()
 */
#include "core/pch.h"

#include "externalapplication.h"

#include "modules/util/opstrlst.h"
#include "modules/util/str.h"

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>

#if defined(__FreeBSD__) || defined(__NetBSD__)
# define sighandler_t sig_t
#elif defined(__CYGWIN__)
# define sighandler_t _sig_func_ptr
#elif !defined(_GNU_SOURCE)
typedef void (*sighandler_t)(int);
#endif

static int uni_execvp( const uni_char* filename, uni_char** const argv )
{
	OP_ASSERT(filename);

	char* fname = uni_down_strdup(filename);
	if( !fname )
	{
		errno = ENOMEM;
		return -1;
	}

	int numElement = 0;
	for( ; argv[numElement]; numElement++) {};

	char** array = OP_NEWA(char *, numElement+1);

	OP_ASSERT(array);

	if( !array )
	{
		op_free(fname);
		errno = ENOMEM;
		return -1;
	}

	int errCode = 0;
	for( int i=0; argv[i]; i++ )
	{
		array[i] = uni_down_strdup( argv[i] );

		OP_ASSERT(array[i]);

		if( !array[i] )
		{
			errCode = ENOMEM;
			break;
		}
	}
	array[numElement]=0;

	if( errCode == 0 )
	{
		int error = execvp( fname, array );
		errCode = error == -1 ? errno : 0;
	}

	op_free(fname);

	for( int i=0; array[i]; i++ )
	{
		op_free(array[i]);
	}
	OP_DELETEA(array);

	if( errCode != 0 )
	{
		errno = errCode;
		return -1;
	}
	else
	{
		errno = 0;
		return 0;
	}
}

static int grandfork(void)
{
	/* Creates a grand-child process to run an external program.
	 *
	 * A process, when complete, sends SIGCHLD to its parent and becomes defunct
	 * (a.k.a. a zombie) until its parent wait()s for it.  If the parent has
	 * died before the child, the child just exits peacefully (in fact because
	 * its parent's death orphaned it, at which point it was adopted by the init
	 * process, pid = 1, which ignores child death).
	 *
	 * This function does a double-fork and waits for the primary child's exit;
	 * the grand-child gets a return of 0 and should exec..() something; the
	 * original process doesn't need to handle SIGCHLD.  If the primary fork
	 * fails, or the primary child is unable to perform the secondary fork, the
	 * parent process sees a negative return value; otherwise, a positive one.
	 */
	pid_t pid = fork();
	if (pid < 0)
		return -1; // failed
	else if (pid)
	{
		// original process
		int stat = 0;
		waitpid(pid, &stat, 0);
		// Decode top-level child's _exit() status (see below):
		return (!WIFEXITED(stat) || WEXITSTATUS(stat)) ? -1 : 1;
	}

	/* Top-level child.
	 *
	 * Note that we use _exit() rather than exit() to avoid calling atexit()
	 * hooks, which we've inherited from our parent, who'd rather *not* have the
	 * child "tidy away" various things it's probably still using ...
	 */
	pid = fork();
	if( pid < 0 )
		_exit( 1 ); // Failure. Caught above

	else if( pid > 0 )
		_exit( 0 ); // Ok. Caught above

	else
		return 0; // grand-child. This will do the job
}

ExternalApplication::ExternalApplication(const uni_char *cmdline, BOOL disown)
	: spawn(disown), m_cmdline(NULL)
{
	OP_ASSERT(cmdline);

	if( cmdline )
	{
		/* The PI-code add a '"' first and last to the command. This
		 * is not, however, compatible with how unix opera used to
		 * handle the commands. You could have multiple arguments.
		 *
		 * Only remove the surrounding '"'s, though. Also, the command
		 * line is parsed wrt. \, ' and " now. This is not really
		 * compatible, however, it can be very useful, since you can
		 * now actually have command names with spaces in them.	*/
		if( cmdline[0] == '"' ) cmdline++;
		int n = uni_strlen(cmdline);
		if( n > 0 && cmdline[n-1] == '"' )
			n--;

		m_cmdline = OP_NEWA(uni_char, n+1);
		if( m_cmdline )
			uni_strlcpy( m_cmdline, cmdline, n + 1 );
	}
}

ExternalApplication::~ExternalApplication()
{
	OP_DELETEA(m_cmdline);
}

int ExternalApplication::run( const uni_char *argument, int fd, const char* encoding )
{
	uni_char **argv = parseCmdLine( m_cmdline, argument );
	if (!argv || !argv[0] || !argv[0][0] )
	{
		errno = ENOENT;
		return -1;
	}

	int result = run(argv, fd, spawn, encoding );
	deleteArray(argv);
	return result;
}

// static
int ExternalApplication::run( const OpVector<OpString>& list, int fd, BOOL disown, const char* encoding )
{
	uni_char **argv = OP_NEWA(uni_char *, list.GetCount()+1);
	if( !argv )
	{
		errno = ENOMEM;
		return -1;
	}

	for( unsigned int i=0; i<list.GetCount(); i++ )
	{
		OpString tmp;
		tmp.Set(*list.Get(i)); // FIXME: OOM
		tmp.Strip();
		int length = tmp.Length();
		if( length > 0 && tmp.CStr()[0] == '"' && tmp.CStr()[length-1] == '"' )
		{
			tmp.CStr()[0] = tmp.CStr()[length-1] = ' ';
			tmp.Strip();
		}

		argv[i] = UniSetNewStr(tmp.CStr()); // FIXME: OOM
	}
	argv[list.GetCount()]=0;

	int result = run(argv, fd, disown, encoding );
	deleteArray(argv);
	return result;
}

// static
int ExternalApplication::run( uni_char **argv, int fd, BOOL disown, const char* encoding )
{
	sighandler_t orig_handler = signal(SIGCHLD, SIG_DFL);
	pid_t pid = disown ? (pid_t) grandfork() : fork();

	if( pid ) // Parent, on success or failure:
		signal(SIGCHLD, orig_handler);

	if (pid == -1)
		// Failure
		return -1;

	if (pid == 0) // Child process
	{
		// Set encoding
		if( encoding && encoding[0] )
			g_env.Set( "OPERA_ENCODING", encoding );

		// (grand-)Child process
		int error = uni_execvp(argv[0], argv);
		if( error == -1 && fd != -1 )
		{
			char *s1 = uni_down_strdup(argv[0]);
			if( s1 )
			{
				char *s2 = strerror(errno);
				char *s3 = OP_NEWA(char,  strlen(s1) + strlen(s2) + 20 );
				if( s3 )
				{
					sprintf( s3, "'%s' %s", s1, s2 );
					::write( fd, s3, strlen(s3)+1 );
					OP_DELETEA(s3);
				}
				op_free(s1);
			} // ? else ?
		}
		else if (error)
		{
			char *cmdname = uni_down_strdup(argv[0]);
			if (cmdname)
			{
				fprintf(stderr, "[opera] '%s' %s\n", cmdname, strerror(errno) );
				free(cmdname);
			}
		}

		_exit(error); // _exit() omits exit()'s running of atexit() hooks.
	}
	else // Parent process
		return 0;
}

/* static */
uni_char **ExternalApplication::parseCmdLine( const uni_char* cmdline,
											  const uni_char* argument )
{
	if (!cmdline)
		return 0;

	uni_char **result = 0;
	// This j-loop is a bogus abstraction !  Un-rolling is terser and clearer ...
	for (int j=0; j<=1; j++)
	{
		int start = -1;
		int strCount = 0;
		int i;
		for (i=0; cmdline[i] != 0; i++)
		{
			if (uni_isspace(cmdline[i]))
			{
				if (start != -1)
				{
					if (j == 1)
					{
						result[strCount] = OP_NEWA(uni_char, i-start+1);
						if (!result[strCount])
						{
							while (strCount-- > 0)
								OP_DELETEA(result[strCount]);

							OP_DELETEA(result);
							return 0;
						}
						uni_strlcpy(result[strCount], cmdline + start, i - start + 1);
					}
					strCount ++;
					start = -1;
				}
			}
			else if (start == -1)
				start = i;
		}

		if (start != -1)
		{
			if (j == 1)
			{
				result[strCount] = OP_NEWA(uni_char, i-start+1);
				if (!result[strCount])
				{
					while (strCount-- > 0)
						OP_DELETEA(result[strCount]);

					OP_DELETEA(result);
					return 0;
				}
				uni_strlcpy(result[strCount], cmdline + start, i - start + 1);
			}
			strCount ++;
		}

		if (argument)
		{
			if (j == 1)
			{
				result[strCount] = OP_NEWA(uni_char, uni_strlen(argument)+1);
				if (!result[strCount])
				{
					while (strCount-- > 0)
						OP_DELETEA(result[strCount]);

					OP_DELETEA(result);
					return 0;
				}
				uni_strcpy(result[strCount], argument);
			}
			strCount ++;
		}

		if (j == 0)
		{
			result = OP_NEWA(uni_char *, strCount+1);
			if (!result)
				return 0;
		}
		else
			result[strCount] = 0;
	}
	return result;
}

/* static */
void ExternalApplication::deleteArray(uni_char **array)
{
	if (array)
	{
		for (int i=0; array[i]; i++)
			OP_DELETEA(array[i]);

		OP_DELETEA(array);
    }
}
