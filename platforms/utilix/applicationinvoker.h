// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//


#ifndef APPLICATIONINVOKER_H
#define APPLICATIONINVOKER_H

/** Runs an application as a child process.
 *
 * An object of this class can be used to start and monitor an
 * external application.  It can be used to retrieve the stdout/stderr
 * output of the application and to keep track of it's state (running,
 * finished, crashed).
 *
 * Deleting the object monitoring an application will let the
 * application run until completion, and the dead process will
 * automatically be wait()ed on.
 */
class ApplicationInvoker
{
public:
	/** Create an empty object that can be used to run an external application.
	 * Call Construct() to initialise the instance. */
	ApplicationInvoker();

	~ApplicationInvoker();

	// ------------------------------------------------------------
	// -- Methods for setting the application's context before Invoke()

	/** Initialises the instance object to run 'filename'.
	 * @param filename The executable file that this object should run.
	 */
	OP_STATUS Construct(const char * filename);

	/** Sets argv[0] of the application.
	 * By default argv[0] is set to the filename of the executable.
	 * This method must be called before the application is started.
	 * There's seldom any reason to call this method.
	 * @param arg The new value of argv[0]
	 */
	OP_STATUS SetArgv0(const char * arg);


	/** Adds another argument to the argument list.
	 * This method must be called before the application is started.
	 * @param arg The new argument to append to the argument list
	 */
	OP_STATUS AppendArgument(const char * arg);


	/** Redirect stdout of the child so that this object can retrieve
	 * it.
	 */
	OP_STATUS CaptureStdout();


	/** Make the child process close a file descriptor before
	 * exec()ing the actual application.
	 * @param fd The file descriptor to close after fork.
	 */
	OP_STATUS CloseInChild(int fd);


	/** Try to make standard input non-blocking unreadable in child.
	 *
	 * It is typically not safe to close stdin, since this means that
	 * other files may be opened with file descriptor 0.  This can be
	 * quite confusing to the invoked program.
	 */
	void MakeStdinUnreadableInChild() { m_stop_stdin = true; }


	/** The program should be executed inside a new terminal.  So
	 * instead of running "<program> <arguments>", we'll execute
	 * something similar to "xterm -e <program> <arguments>".
	 */
	OP_STATUS RunInTerminal();


	// ------------------------------------------------------------
	// -- Method(s) for starting the application


	/** Start the external application.
	 *
	 * Sets LastErrno() if it fails.
	 */
	OP_STATUS Invoke();


	// ------------------------------------------------------------
	// -- Methods used while the application is running or after it has
	// -- stopped.


	/** Obtain data the child has written to stdout.
	 *
	 * Note that this method will only work if CaptureStdout() has
	 * been called (before Invoke()).
	 *
	 * This method will not return before the timeout unless it can
	 * return some data, end of data is reached, or there is an error.
	 *
	 * @param buf The buffer to write the data into.
	 * @param buflen Size of buf.
	 * @param timeout How many milliseconds to wait for data to arrive,
	 *   0 means to not wait, and negative values means wait forever.
	 * @return How many bytes/chars of data have been written to buf.
	 *   0 on timeout, -1 on EOF and -2 on error.
	 */
	int ReadStdout(char * buf, int buflen, int timeout = -1);


	/** Forcefully terminate the process.
	 *
	 * This is not guaranteed to succeed in terminating the process.
	 * It is most likely implemented as kill(SIGTERM), not
	 * kill(SIGKILL).
	 */
	void Terminate();


	/** Check whether application is still running.
	 *
	 * @param active_check If true, this method will make an explicit
	 * test (probably kill(pid, 0)) to see if the process is still
	 * alive.  Otherwise it will only report whether it has been told
	 * about the demise of the process.
	 */
	BOOL IsRunning(BOOL active_check = FALSE);


	/** Returns whether ExitStatus() will return a valid value.
	 */
	BOOL ExitStatusValid() const { return m_has_wait_status; }


	/** Returns the exit status of the process.
	 *
	 * This method only makes sense if ExitStatusValid() returns true.
	 *
	 * @return The 'status' value of the wait() family of functions.
	 */
	int ExitStatus() const { return m_wait_status; }


	// ------------------------------------------------------------
	// -- Methods related to this class itself

	/** Returns an errno-equivalent of the last error that occured.
	 * Not all methods are implemented to set this value when they
	 * fail, though.
	 */
	int LastErrno() const { return m_errno; };

	// ------------------------------------------------------------
	// -- Methods used by other parts of opera to inform this class
	// -- of things it should know about.


	/** This method is called (typically by the SIGCHLD handler) to
	 * inform that a child process has died.
	 *
	 * NOTE: This method must be signal-safe!
	 *
	 * @param pid The pid of the application that died.
	 * @param status The 'status' return parameter of the wait family
	 * of functions.
	 */
	static void ProcessDied(pid_t pid, int status);

	/**
	 * Does a nonblocking waitpid() on all registered pids
	 * If a process has died, @ref ProcessDied will be called 
	 * with the corresponding process id
	 */
	static void WaitAllPids();

private:
	/** Read data from file descriptor 'fd' into 'buf', with a timeout
	 * of 'timeout'.  This method will not return before the timeout
	 * has passed unless it is able to return data, EOF or an error
	 * has occurred.
	 *
	 * @return number of bytes/chars read into buf, 0 on timeout, -1
	 * on EOF and -2 on error.
	 */
	int ReadFd(int fd, char * buf, int buflen, int timeout);


	// The path to the file to be executed.
	char * m_exec_file;

	// The argument list to start the application with (0-terminated)
	char ** m_args;
	// How many elements have been alloced in m_args
	int m_args_allocated;

	// Whether the application is known to be dead
	bool m_is_dead;
	// Whether m_wait_status is valid
	bool m_has_wait_status;
	// The status return value from the wait() call
	int m_wait_status;

	/** If true, we've already added run-in-terminal details (e.g. "xterm", "-e") to start of m_args.
	 */
	bool m_run_in_terminal;

	/** A sorted list of file descriptors that this object should
	 * close in the child process (after the fork) before starting the
	 * external process.
	 */
	int *m_fd_close;

	/** m_fd_close has this many allocated elements.
	 */
	int m_fd_close_allocated;
	/** m_fd_Close has this many used elements.
	 */
	int m_fd_close_used;

	/** If true, stdin should be made inaccessible in child.  In
	 * particular, if the application tries to read from stdin, it
	 * should not block, and preferably return EOF.  It is not
	 * necessarily safe to close stdin outright.  That would make it
	 * likely that other files will be opened with file descriptor 0.
	 * And programs may then accidentally access that file while
	 * trying to read from stdin.
	 */
	bool m_stop_stdin;

	/** Whether to redirect stdout so the output can be retrieved through
	 * this object.
	 */
	bool m_redirect_stdout;

	/** The file descriptor to read stdout from.
	 */
	int m_fd_stdout;


	/** Set to true before fork(), so ProcessDied() can catch
	 * processes that die before we can set m_child_pid.
	 */
	bool m_forked;

	/** List of processes that died while this process started up.
	 * Each dead process adds the pid and the wait-status (in that
	 * order) to the list.
	 */
	int * m_dead_pids;

	/** Number of allocated elements in m_dead_pids.
	 */
	int m_dead_pids_length;

	/** Number of used elements in m_dead_pids.
	 */
	int m_dead_pids_used;

	/** The pid of the child process.
	 */
	pid_t m_child_pid;

	/** errno-equivalent value of last failed method.
	 * Not all methods set this value.
	 */
	int m_errno;

	static ApplicationInvoker * g_application_list;
	ApplicationInvoker * m_next;
};

#endif
