/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

/** \file
 * \brief Implement parts of OpCodeLocation and OpCodeLocationManager
 *
 * Implement the platform independent parts of these clases.
 * The platform dependent parts will have to be implemented by the
 * platform.
 *
 * \author Morten Rolland, mortenro@opera.com
 */

#include "core/pch.h"

#if defined(MEMTOOLS_ENABLE_CODELOC) && !defined(MEMTOOLS_NO_DEFAULT_CODELOC)

#if defined(UNIX) || defined(LINGOGI)

#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "modules/memtools/memtools_codeloc.h"

#define CODELOC_MAX_FUNC_LEN		2048
#define CODELOC_MAX_FILE_LEN		2048
#define CODELOC_MAX_BUFFER			4096

extern char* g_executable;

class LinuxCodeLocationManager : public OpCodeLocationManager
{
public:
	LinuxCodeLocationManager(void);
	~LinuxCodeLocationManager(void);

	static void PathCleanup(char* path);

	static void* operator new(size_t size) { return op_debug_malloc(size); }
	static void  operator delete(void* ptr) { op_debug_free(ptr); }

protected:
	virtual OP_STATUS initialize(void);
	virtual OpCodeLocation* create_location(UINTPTR addr);
	virtual void start_lookup(OpCodeLocation* loc);
	virtual int poll(void);
	virtual void do_sync(void);
	virtual void forked(void);

private:
	int read_symbol(void);
	int start_pending_lookup(void);

	int fdr;
	int fdw;
	pid_t child_pid;

	int pending_lookup_count;
	OpCodeLocation* pending_lookup_next;

	int read_buffer_size;
	BOOL fresh_bytes_available;

	BOOL disabled;	// TRUE when symbol lookup is no longer possible

	char read_buffer[CODELOC_MAX_BUFFER+4];	// ARRAY OK 2008-06-06 mortenro
	char function[CODELOC_MAX_FUNC_LEN+4];	// ARRAY OK 2008-06-06 mortenro
	char file[CODELOC_MAX_FUNC_LEN+4];		// ARRAY OK 2008-06-06 mortenro
};

extern "C" int symlookup_verify(const char* function, const char* file)
{
	int line = __LINE__ - 2; // Subtract two to get the func. decl. line above
	const char* thisfile = __FILE__;

	if ( op_strcmp(function, "symlookup_verify") )
	{
		dbg_printf("WARN: Linux Symbol lookup failed, function='%s'\n",
				   function);
		return 0;
	}

	const char* filebase = op_strrchr(thisfile, '/');
	if ( filebase == 0 )
		filebase = thisfile;
	else
		filebase++;

	char buffer[1024];  // ARRAY OK 2008-05-22 mortenro
	op_snprintf(buffer, 1000, "%s:%d", filebase, line);

	if ( op_strstr(file, buffer) == 0 )
		dbg_printf("WARN: Linux file/line lookup, '%s' not in '%s'\n",
				   buffer, file);

	return 1;
}

LinuxCodeLocationManager::LinuxCodeLocationManager(void) :
	fdr(-1),
	fdw(-1),
	child_pid(0),
	pending_lookup_count(0),
	pending_lookup_next(0),
	read_buffer_size(0),
	fresh_bytes_available(FALSE),
	disabled(FALSE)
{
}

OP_STATUS LinuxCodeLocationManager::initialize(void)
{
	int c2p[2];		// ARRAY OK 2008-06-06 mortenro
	int p2c[2];		// ARRAY OK 2008-06-06 mortenro

	pipe(c2p);   // Data from child to parent
	pipe(p2c);   // Data from parent to child

	fdr = c2p[0];  // Parent reads from this one
	fdw = p2c[1];  // Parent writes to this one

	child_pid = fork();

	if ( child_pid < 0 )
	{
		// Oops, something seriously wrong
		return OpStatus::ERR;
	}

	if ( child_pid == 0 )
	{
		// We are the child, close the pipe connections we don't use
		close(c2p[0]);
		close(p2c[1]);

		// Move child-to-parent out of the way if it is at stdin
		if ( c2p[1] == 0 )
		{
			int new_fd = dup(c2p[1]);
			close(c2p[1]);
			c2p[1] = new_fd;
		}

		// Make sure the parent-to-child pipe goes to stdin for child
		if ( p2c[0] != 0 )
		{
			dup2(p2c[0], 0);
			close(p2c[0]);
		}

		// Make sure the child-to-parent pipe goes from stdout for child
		if ( c2p[1] != 1 )
		{
			dup2(c2p[1], 1);
			close(c2p[1]);
		}

		execlp("addr2line", "addr2line", "-f", "-C",
			   "-e", g_executable, (char*)0);

		{
			// Oops, the new process couldn't start 'addr2line'
			fprintf(stderr, "Unable to exec addr2line\n");

			// Close the pipes so reading them will give an error
			close(1);
			close(2);

			// Consume all input until the first new-line character.
			// When we see the newline character, we can safely exit
			// without fear of any more being written to the pipe.
			for(;;)
			{
				char buffer[4];		// ARRAY OK 2008-09-05 mortenro
				buffer[0] = 'a';
				int rc = read(0, buffer, 1);
				if ( buffer[0] == '\n' || rc < 0 )
					break;
			}

			exit(0);
		}
	}
	else
	{
		// We are the parent, close the ones we don't use
		close(c2p[1]);
		close(p2c[0]);

		//
		// Set close on exit flag, so if Opera does fork()+exec(), the
		// pipes to the 'addr2line' process will be broken.
		// This is however not enough since allocations/deallocations
		// may happen after fork(), but before exec(), or after exec()
		// if it fails.
		//
		// For this reason, the 'Forked()' method should be called
		// explicitly from the child process right after the fork()
		// to correctly handle memory operations from that point on.
		//
		fcntl(fdr, F_SETFD, FD_CLOEXEC);
		fcntl(fdw, F_SETFD, FD_CLOEXEC);

		// Perform a simple test to verify that lookup works, by looking
		// up the info for the known function 'symlookup_verify()'.

		char buffer[32];	// ARRAY OK 2008-06-06 mortenro
		op_sprintf(buffer, "%p\n", symlookup_verify);
		int bufsize = op_strlen(buffer);
		int rc = write(fdw, buffer, bufsize);
		if ( rc != bufsize )
		{
			fprintf(stderr, "BUGGER-1 rc=%d errno=%d!!\n", rc, errno);
			return OpStatus::ERR;
		}

		for ( int k = 0; k < 10; k++ )
		{
			if ( read_symbol() )
			{
				if ( symlookup_verify(function, file) )
				{
					// Set the pipe in non-blocking mode, to let us poll
					fcntl(fdr, F_SETFL, O_RDONLY | O_NONBLOCK);

					// Initiate lookup of all OpCodeLocation objects created
					// before initialization
					while ( start_pending_lookup() )
						;

					return OpStatus::OK;
				}
				return OpStatus::ERR;
			}
		}
	}

	return OpStatus::ERR;
}

LinuxCodeLocationManager::~LinuxCodeLocationManager(void)
{
}

int LinuxCodeLocationManager::read_symbol(void)
{
	for (;;)
	{
		if ( fresh_bytes_available )
		{
			// There is a point trying to parse the buffer
			read_buffer[read_buffer_size] = 0;
			char* nl = index(read_buffer, '\n');
			if ( nl != 0 )
			{
				// Locate function name
				char* func_name = read_buffer;
				int func_len = nl - read_buffer;

				// Locate file and line number
				char* file_name = read_buffer + func_len + 1;
				nl = index(file_name, '\n');
				if ( nl != 0 )
				{
					int file_len = nl - file_name;
					int consumed = func_len + file_len + 2;

					if ( func_len > CODELOC_MAX_FUNC_LEN )
						func_len = CODELOC_MAX_FUNC_LEN;
					if ( file_len > CODELOC_MAX_FILE_LEN )
						file_len = CODELOC_MAX_FILE_LEN;

					func_name[func_len] = 0;
					file_name[file_len] = 0;

					op_strcpy(function, func_name);
					op_strcpy(file, file_name);

					read_buffer_size -= consumed;
					if ( read_buffer_size > 0 )
						op_memmove(read_buffer, read_buffer + consumed,
								   read_buffer_size);

					return 1;  // Success, a new symbol is available
				}
			}

			fresh_bytes_available = FALSE;
		}

		if ( read_buffer_size < CODELOC_MAX_BUFFER )
		{
			int rc = read(fdr, read_buffer + read_buffer_size,
						  CODELOC_MAX_BUFFER - read_buffer_size);
			if ( rc > 0 )
			{
				read_buffer_size += rc;
				fresh_bytes_available = TRUE;
				continue;
			}
		}

		return 0;
	}
}

OpCodeLocationManager* OpCodeLocationManager::create(void)
{
	return new LinuxCodeLocationManager();
}

OpCodeLocation* LinuxCodeLocationManager::create_location(UINTPTR addr)
{
	return new OpCodeLocation(addr);
}

void LinuxCodeLocationManager::start_lookup(OpCodeLocation* loc)
{
	if ( ! disabled )
	{
		if ( pending_lookup_next == 0 )
			pending_lookup_next = loc;

		while ( start_pending_lookup() )
			;

		while ( poll() )
			;
	}
}

int LinuxCodeLocationManager::start_pending_lookup(void)
{
	if ( disabled || pending_lookup_next == 0 || pending_lookup_count >= 100 )
		return 0;

	char buffer[32];	// ARRAY OK 2008-06-06 mortenro
	op_sprintf(buffer, "%p\n", (void*)(pending_lookup_next->GetAddr()-1));
	int bufsize = op_strlen(buffer);
	int rc = write(fdw, buffer, bufsize);
	if ( rc != bufsize )
		fprintf(stderr, "BUGGER-2 rc=%d errno=%d!!\n", rc, errno);

	pending_lookup_next = pending_lookup_next->GetInternalNext();
	pending_lookup_count++;

	return 1;
}

int LinuxCodeLocationManager::poll(void)
{
	if ( ! disabled && pending_lookup_count > 0 && read_symbol() )
	{
		// We have a new symbol; fill it with data, and pick it up from
		// the uninitialized list, and move it to the initialized list.

		const char* fn = op_debug_strdup(function);
		if ( fn == 0 )
			fn = "internal-OOM(void)";

		PathCleanup(file);
		const char* fl = op_debug_strdup(file);
		if ( fl == 0 )
			fl = "internal-OOM:1";

		OpCodeLocation* loc = uninitialized_list;
		uninitialized_list = loc->GetInternalNext();

		loc->SetFunction(fn);
		loc->SetFileLine(fl);
		loc->SetInternalNext(0);

		if ( initialized_list == 0 )
		{
			initialized_list = loc;
			initialized_list_last = loc;
		}
		else
		{
			initialized_list_last->SetInternalNext(loc);
			initialized_list_last = loc;
		}

		pending_lookup_count--;

		return 1;
	}

	return 0;
}

void LinuxCodeLocationManager::do_sync(void)
{
	if ( ! disabled && (uninitialized_list != 0 || pending_lookup_count > 0) )
	{
		fcntl(fdr, F_SETFL, O_RDONLY);

		while ( uninitialized_list != 0 || pending_lookup_count > 0 )
		{
			if ( start_pending_lookup() )
				start_pending_lookup();
			poll();
		}

		fcntl(fdr, F_SETFL, O_RDONLY | O_NONBLOCK);
	}
}

void LinuxCodeLocationManager::forked(void)
{
	//
	// Prevent the pipes from being carried over into a new process.
	// This does however render the current process unable to look up
	// any new symbols.
	//
	if ( ! disabled )
	{
		close(fdr);
		close(fdw);
		disabled = TRUE;
	}
}


//
// In place absolute path cleanup. Cleaned up path will allways be shorter
// or same length as the original.
//
void LinuxCodeLocationManager::PathCleanup(char* path)
{
	char* p;

	// Remove '//'
	while ( (p = op_strstr(path, "//")) != 0 )
	{
		int len = op_strlen(p);
		op_memmove(p, p+1, len);
	}

	// Remove '/./'
	while ( (p = op_strstr(path, "/./")) != 0 )
	{
		int len = op_strlen(p);
		op_memmove(p, p+2, len-1);
	}

	// Remove '/../'
	while ( (p = op_strstr(path, "/../")) != 0 )
	{
		int len = op_strlen(p);
		char* pp = p-1;
		while ( pp > path && *pp != '/' )
			pp--;
		if ( pp > path )
			op_memmove(pp, p+3, len-2);
		else
			op_memmove(p, p+3, len-2);
	}
}

#endif // UNIX || LINGOGI

#endif // MEMTOOLS_ENABLE_CODELOC && !MEMTOOLS_NO_DEFAULT_CODELOC
