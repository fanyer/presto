/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Marcin Zdun (mzdun)
 */

#include "core/pch.h"

#ifdef AUTO_UPDATE_SUPPORT
#ifdef PLUGIN_AUTO_INSTALL

#include <sys/wait.h>
#include <signal.h>

#include "adjunct/quick/managers/PluginInstallManager.h"
#include "adjunct/desktop_util/string/string_convert.h"
#include "platforms/mac/pi/desktop/MacAsyncProcessRunner.h"

#define BOOL NSBOOL
#import <Foundation/NSArray.h>
#import <Foundation/NSAutoreleasePool.h>
#import <Foundation/NSBundle.h>
#import <Foundation/NSFileManager.h>
#import <Foundation/NSPathUtilities.h>
#import <Foundation/NSString.h>
#import <Foundation/NSTask.h>
#import <Security/Authorization.h>
#undef BOOL

class PosixProcessObserver: public PIM_MacAsyncProcessRunner::ProcessObserver
{
	pid_t m_pid;
	bool m_closed;

protected:

	PosixProcessObserver(): m_pid(0), m_closed(false) {}
	void setPid(pid_t pid) { m_pid = pid; }

private: //ProcessObserver

	virtual PIM_PROCESS_STATE hasFinished(int& return_value)
	{
		if (m_closed) return PIM_PROCESS_FINISHED;
		
		pid_t pid = waitpid(m_pid, &return_value, WNOHANG);
		if (pid == 0) return PIM_PROCESS_RUNNING;
		if (pid == -1) return PIM_PROCESS_STATE_QUERY_ERROR;
		m_closed = true;
		m_pid = 0;
		return PIM_PROCESS_FINISHED;
	}

	virtual int terminate()
	{
		if (!m_pid) return 0;
		int ret = kill(m_pid, SIGTERM);
		if (ret == -1 && errno != ESRCH) return -1;
		if (ret != -1)
		{
			sleep(2);
			int ignore;
			if (hasFinished(ignore) != PIM_PROCESS_FINISHED)
				ret = kill(m_pid, SIGKILL);
		}
		int status;
		(void)waitpid(m_pid, &status, 0);
		m_pid = 0;
		
		return ret;
	}
};

class TaskProcessObserver: public PIM_MacAsyncProcessRunner::ProcessObserver
{
	NSTask* m_task;
	
protected:
	
	TaskProcessObserver(): m_task(nil) {}
	~TaskProcessObserver()
	{
		[m_task release];
	}
	
	void setTask(NSTask* task)
	{
		NSTask* copy = [task retain];
		[m_task release];
		m_task = copy;
	}
	
private: //ProcessObserver
	
	virtual bool start()
	{
		[m_task launch];
		return true;
	}
	
	virtual PIM_PROCESS_STATE hasFinished(int& return_value)
	{
		BOOL finished = ![m_task isRunning];
		if (finished)
			return_value = [m_task terminationStatus];
		return finished ? PIM_PROCESS_FINISHED : PIM_PROCESS_RUNNING;
	}
	
	virtual int terminate()
	{
		[m_task terminate];
		return 0;
	}
};

class ElevatedProcessObserver: public PosixProcessObserver 
{
	char** m_args;
	
protected:
	
	ElevatedProcessObserver(): m_args(NULL) {}
	~ElevatedProcessObserver()
	{
		OP_DELETEA(m_args);
	}
	
	void setArgs(const char** args)
	{
		OP_DELETEA(m_args);
		m_args = NULL;
		if (!args) return;

		size_t size = 0;
		const char** src = args;
		while (*src) ++size, ++src;
		size += 4; //-c "echo..." "" and finalizer

		m_args = OP_NEWA(char*, size);
		if (!m_args) return;

		m_args[0] = "-c";
		m_args[1] = "echo $$; exec \"$@\"";
		m_args[2] = "";

		src = args;
		const char** dst = const_cast<const char**>(m_args + 3);
		while (*src) *dst++ = *src++;
		*dst = NULL;
	}
	
	virtual void setupArgs() = 0;
	
private: //ProcessObserver
	
	virtual bool start()
	{
		setPid(0);
		setupArgs();

		// Create authorization reference
		AuthorizationRef authorizationRef;
		OSStatus status;
		status = AuthorizationCreate(NULL, kAuthorizationEmptyEnvironment,
									 kAuthorizationFlagDefaults, &authorizationRef);
		
		if (status != errAuthorizationSuccess) return false;
		
		// Run the tool using the authorization reference
		FILE *pipe = NULL;
		char* args0[] = { NULL };
		char** args = m_args;
		if (!args) args = args0;
		status = AuthorizationExecuteWithPrivileges(authorizationRef, "/bin/sh",
													kAuthorizationFlagDefaults, args, &pipe);
		
		AuthorizationFree(authorizationRef, kAuthorizationFlagDefaults);

		if (status != errAuthorizationSuccess) return false;
		if (!pipe) return false;
		
		long long i64_pid = 0; //not sure pid_t can be i64, or is it only i32 on darwin, will not take chances...
		int scans = fscanf(pipe, "%lld", &i64_pid);
		fclose(pipe);
	
		if (scans < 1) return false;
		setPid(static_cast<pid_t>(i64_pid));
		return true;
	}
};

class AppInstaller: public TaskProcessObserver
{
public:
	AppInstaller(NSString* path)
	{
		NSBundle* bundle = [[NSBundle alloc] initWithPath:path];
		NSTask* task = [[NSTask alloc] init];
		[task setLaunchPath:[bundle executablePath]];
		setTask(task);
		[task release];
		[bundle release];
	}
};

class ElevatedPackageInstaller: public ElevatedProcessObserver
{
	OpString8 m_pkg;

public:

	ElevatedPackageInstaller(const OpStringC& package)
	{
		m_pkg.Set(package);
	}

private: //ElevatedProcessObserver

	virtual void setupArgs()
	{
		const char *args[] = {"/usr/sbin/installer", "-pkg", m_pkg.CStr(), "-target", "/", NULL};
		setArgs(args);
	}
};

PIM_MacAsyncProcessRunner::PIM_MacAsyncProcessRunner(): m_state(IDLE), m_process(NULL)
{
	m_poll_timer.SetTimerListener(this);
}

PIM_MacAsyncProcessRunner::~PIM_MacAsyncProcessRunner()
{
	if (m_poll_timer.IsStarted())
		m_poll_timer.Stop(); //OpTimer::MessageHandler does not check for valid m_listener
	m_poll_timer.SetTimerListener(NULL);
}

void PIM_MacAsyncProcessRunner::OnTimeOut(OpTimer* timer)
{
	OP_ASSERT(timer == &m_poll_timer);
	if(timer != &m_poll_timer)
		return;

	OP_ASSERT(m_is_running);
	if(!m_is_running)
		return;
	
	int status = 0;
	PIM_PROCESS_STATE result = PIM_PROCESS_STATE_QUERY_ERROR;
	if (m_process.get())
		result = m_process->hasFinished(status);
	if (result == PIM_PROCESS_RUNNING)
	{
		KickTimer();
		return;
	}
	if (result == PIM_PROCESS_STATE_QUERY_ERROR)
	{
		OnProcessFinished(ProcessFailExitCode());
		return;
	}
	OnProcessFinished(status);
}

void PIM_MacAsyncProcessRunner::KickTimer()
{
	m_poll_timer.Start(ProcessPollInterval());
}

int PIM_MacAsyncProcessRunner::Mount(const OpStringC& a_app)
{
	if (m_state != IDLE) return -1;

	int status = 0;

	NSAutoreleasePool* local = [NSAutoreleasePool new];
	NSArray* args = [NSArray arrayWithObjects: 
					 @"attach",
					 @"-nobrowse",
					 @"-noautoopen",
					 @"-mountpoint",
					 [NSString stringWithCharacters:(const unichar*) m_mount_name.CStr() length:m_mount_name.Length()],
					 [NSString stringWithCharacters:(const unichar*) a_app.CStr() length:a_app.Length()],
					 nil];
	
	NSTask* mount = [NSTask launchedTaskWithLaunchPath:@"/usr/bin/hdiutil" arguments:args];
	[mount waitUntilExit];
	status = [mount terminationStatus];
	
	[local release];

	if (status == 0) m_state = MOUNTED;

	return status;
}

void PIM_MacAsyncProcessRunner::Umount()
{
	if (m_state == IDLE) return;

	NSAutoreleasePool* local = [NSAutoreleasePool new];
	NSArray* args = [NSArray arrayWithObjects: 
					 @"detach",
					 [NSString stringWithCharacters:(const unichar*) m_mount_name.CStr() length:m_mount_name.Length()],
					 nil];
	
	NSTask* mount = [NSTask launchedTaskWithLaunchPath:@"/usr/bin/hdiutil" arguments:args];
	[mount waitUntilExit];
	
	[local release];

	m_state = IDLE;
}

void PIM_MacAsyncProcessRunner::InitAppInstaller()
{
	NSAutoreleasePool* local = [NSAutoreleasePool new];
	NSFileManager* filemgr = [[[NSFileManager alloc] init] autorelease];
	NSString* path = [NSString stringWithCharacters:(const unichar*)m_mount_name.CStr() length:m_mount_name.Length()];
	NSDirectoryEnumerator* dir = [filemgr enumeratorAtPath:path];
	
	NSString *file;
	while ((file = [dir nextObject]) != nil)
	{
		if ([[file pathExtension] isEqualToString: @"app"])
		{
			m_process.reset(OP_NEW(AppInstaller, ([path stringByAppendingPathComponent:file])));
			break;
		}
	}
	
	[local release];
}

OP_STATUS PIM_MacAsyncProcessRunner::InitPackageInstaller(const OpStringC& pkg_path)
{
	if (m_state != MOUNTED) return OpStatus::ERR;
	
	OpString pkg_name;
	
	RETURN_IF_ERROR(pkg_name.Set(m_mount_name));
	if (pkg_path[0] != '/')
	{
		RETURN_IF_ERROR(pkg_name.Append(UNI_L("/")));
	}
	RETURN_IF_ERROR(pkg_name.Append(pkg_path));

	int elevation = PackageElevation(pkg_name);
	
	if (elevation)
	{
		m_process.reset(OP_NEW(ElevatedPackageInstaller, (pkg_name)));
	}
	else
	{
		OP_ASSERT(0 && "*\n*\n*\n*\n*\n*\n*\n*\n*\n*\n*\n*\n*\n*\n*\n* PIM_MacAsyncProcessRunner: Installing user packages is not supported...\n*\n*\n*\n*\n*\n*\n*\n*\n*\n*\n*\n*\n*\n");
		m_process.reset();
	}
	return m_process.get() ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

int PIM_MacAsyncProcessRunner::PackageElevation(const OpStringC& pkg_name)
{
	int elevation = 0;
	
	NSAutoreleasePool* local = [NSAutoreleasePool new];
	
	NSBundle* pkg = [NSBundle bundleWithPath:[NSString stringWithCharacters:(const unichar*) pkg_name.CStr() length:pkg_name.Length()]];
	NSString * auth = (NSString*)[pkg objectForInfoDictionaryKey:@"IFPkgFlagAuthorizationAction"];
	if (auth != nil)
	{
		if ([auth caseInsensitiveCompare:@"AdminAuthorization"] == NSOrderedSame) elevation = 1;
		else if ([auth caseInsensitiveCompare:@"RootAuthorization"] == NSOrderedSame) elevation = 2;
	}
	
	[local release];
	
	return elevation;
}

void PIM_MacAsyncProcessRunner::OnProcessFinished(int exit_code)
{
	m_is_running = FALSE;
	m_shell_exit_code = exit_code;
	m_process.reset();
	m_state = FINISHED;
	Umount();
	
	if(m_listener)
		m_listener->OnProcessFinished(m_shell_exit_code);
}

OP_STATUS PIM_MacAsyncProcessRunner::RunProcess(const OpStringC& a_app_name, const OpStringC& a_app_params)
{
	if (m_state == MOUNTED || m_state == INSTALLING)
		return OpStatus::ERR;
	
	m_is_running = FALSE;
	
	Umount();
	
	m_shell_exit_code = PluginInstallManager::PIM_PROCESS_DEFAULT_EXIT_CODE;

	if (a_app_name.Length() < 4) return OpStatus::ERR;
	if (uni_stricmp(a_app_name.CStr() + (a_app_name.Length() - 4), UNI_L(".dmg")) != 0)
		return OpStatus::ERR;
	
	RETURN_IF_ERROR(m_mount_name.Set(a_app_name, a_app_name.Length() - 3));
	RETURN_IF_ERROR(m_mount_name.Append(UNI_L("mount_point")));

	if (a_app_params.IsEmpty()) //non-silent install - search for .app and run it...
	{
		int status = Mount(a_app_name);
		if (status != 0)
		{
			m_shell_exit_code = status;
			return OpStatus::ERR;
		}
		
		InitAppInstaller();
	}
	else //we have param, which means we have pkg file
	{
		if ((a_app_params.Length() < 4) ||
			(uni_stricmp(a_app_params.CStr() + (a_app_params.Length() - 4), UNI_L(".pkg")) != 0))
		{
			return OpStatus::ERR;
		}
		
		int status = Mount(a_app_name);
		if (status != 0)
		{
			m_shell_exit_code = status;
			return OpStatus::ERR;
		}
		
		RETURN_IF_ERROR(InitPackageInstaller(a_app_params));
	}
	
	if (m_process.get() && m_process->start())
	{
		m_is_running = TRUE;
		m_state = INSTALLING;
	}

	if (m_state == MOUNTED) //we didn't move on - installer is not started...
	{
		Umount();
		return OpStatus::ERR;
	}

	KickTimer();
	
	return OpStatus::OK;
}

OP_STATUS PIM_MacAsyncProcessRunner::KillProcess()
{
	if (!m_is_running)
	{
		OP_ASSERT(!"PIM_MacAsyncProcessRunner: Trying to kill a process that is not running");
		return OpStatus::ERR;
	}
	
	int terminate_retcode = -1;
	
	if (m_process.get())
		terminate_retcode = m_process->terminate();
	
	Umount();
	
	if (-1 == terminate_retcode)
		return OpStatus::ERR;
	else
		return OpStatus::OK;
}

#endif // PLUGIN_AUTO_INSTALL
#endif // AUTO_UPDATE_SUPPORT
