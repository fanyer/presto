/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4; -*-
 *
 * Copyright (C) 2007-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"
#ifdef POSIX_MODULE_REQUIRED
// See also posix_plugin_callback.cpp for POSIX_OK_SELECT_CALLBACK portions.

#include "platforms/posix/posix_native_util.h"
#include "platforms/posix/posix_file_util.h"
#include "platforms/posix/src/posix_async.h"
#include "platforms/posix/posix_selector.h"
#include "platforms/posix/posix_signal_watcher.h"
# ifdef POSIX_OK_LOG
#include "platforms/posix/posix_logger.h"
# endif // for m_log_boss

# ifdef POSIX_SUPPORT_IPV6 // static:
#include <sys/socket.h>

bool PosixModule::AskIPv6Support()
{
	int testfd = socket(PF_INET6, SOCK_STREAM, 0);
	if (testfd == -1)
		return false;

	close(testfd);
	return true;
}
# endif // POSIX_SUPPORT_IPV6

#ifdef POSIX_OK_ASYNC
/* May be called by PosixSystemInfo::QueryLocalIP() during pi's InitL();
 * otherwise, called from posix's InitL().  So must cope with being called
 * twice.
 */
OP_STATUS PosixModule::InitAsync()
{
	if (m_async_boss == 0)
	{
		OpAutoPtr<PosixAsyncManager> boss(OP_NEW(PosixAsyncManager, ()));
		if (boss.get() == 0)
			return OpStatus::ERR_NO_MEMORY;

#ifndef THREAD_SUPPORT
		RETURN_IF_ERROR(boss->Init());
#endif
		// Only set m_async_boss on success:
		m_async_boss = boss.release();
	}
	return OpStatus::OK;
}
#endif // POSIX_OK_ASYNC

#ifdef POSIX_OK_SELECT
OP_STATUS PosixModule::InitSelector()
{
	if (m_selector == 0)
	{
# ifdef POSIX_SELECTOR_SELECT_FALLBACK
		RETURN_IF_ERROR(PosixSelector::CreateWithFallback(m_selector));
# else
		RETURN_IF_ERROR(PosixSelector::Create(m_selector));
# endif // POSIX_SELECTOR_SELECT_FALLBACK
		OP_ASSERT(m_selector);
	}
	return OpStatus::OK;
}
#endif // POSIX_OK_SELECT

void PosixModule::InitL(const class OperaInitInfo& ignored)
{
	// Assumed extensively in locale-related code:
	OP_ASSERT(sizeof(uni_char) == 2);

#ifdef POSIX_OK_LOCALE
	InitLocale(); // implemented in src/posix_native_util.cpp
#endif
	// Initialize data members:
#ifdef POSIX_OK_FILE
	/* Done here so that constructor, in class definition, doesn't need to be
	 * able to see the declaration of PosixNativeUtil. */
	m_strict_file_permission = PosixFileUtil::UseStrictFilePermission();
#endif
#ifdef POSIX_OK_LOG
	m_log_boss = OP_NEW(PosixLogManager, ());
	if (m_log_boss == 0)
		LEAVE(OpStatus::ERR_NO_MEMORY);
#endif
#ifdef POSIX_OK_ASYNC
	LEAVE_IF_ERROR(InitAsync());
#endif
#ifdef POSIX_OK_SELECT
	LEAVE_IF_ERROR(InitSelector());
#endif
	m_initialised = true; // *last* in this method.
}

void PosixModule::Destroy()
{
	/* NB: must zero out on deletion.
	 *
	 * After this module is shut down, other modules' shut-down code may call
	 * posix methods that need to check whether a global is still usable; since
	 * g_opera isn't yet NULL (and never will be if we're using the static
	 * global object), such tests have to actually look at the global's value,
	 * which thus needs to have been set to a recognizably dead value.
	 */
	m_initialised = false; // *first* in this method.
#ifdef POSIX_OK_SIGNAL // depends on m_selector
	OP_DELETE(m_signal_watch); m_signal_watch = 0;
#endif
#ifdef POSIX_OK_ASYNC
	OP_DELETE(m_async_boss); m_async_boss = 0;
#endif
#ifdef POSIX_OK_SELECT
	OP_DELETE(m_selector); m_selector = 0;
#endif
#ifdef POSIX_OK_LOG
	OP_DELETE(m_log_boss); m_log_boss = 0;
#endif
#ifdef POSIX_SETENV_VIA_PUTENV
	m_env.Clear();
#endif
	// ... in the reverse of InitL()'s order.
}

# ifdef POSIX_OK_SETENV
OP_STATUS PosixModule::Environment::Set(const char *name, const char *value)
{
#ifdef POSIX_SETENV_VIA_PUTENV
	Entry *ent = Find(name);
	if (ent == 0)
	{
		ent = OP_NEW(Entry, ());
		if (ent == 0)
			return OpStatus::ERR_NO_MEMORY;

		ent->Into(this);
	}
	OP_STATUS res = ent->Set(name, value);
	if (op_strcmp(name, "TZ")) tzset();
	return res;
#else // simply use setenv itself :-)
	errno = 0;
	if (0 == setenv(name, value, 1))
	{
		if (op_strcmp(name, "TZ")) tzset();
		return OpStatus::OK;
	}

	if (errno == ENOMEM)
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::ERR;
#endif // POSIX_SETENV_VIA_PUTENV
}

OP_STATUS PosixModule::Environment::Set(const char *name, const uni_char *value)
{
#ifdef POSIX_OK_NATIVE
	PosixNativeUtil::NativeString val(value);
	RETURN_IF_ERROR(val.Ready());
	return Set(name, val.get());
#else
	return OpStatus::ERR_NOT_SUPPORTED;
#endif
}

OP_STATUS PosixModule::Environment::Unset(const char *name)
{
#ifdef POSIX_SETENV_VIA_PUTENV
	return Set(name, ""); // without unsetenv, setting to empty is as close as we get :-(
#else
#ifdef POSIX_UNSETENV_INT_RETURN
	int ret =
#endif
		unsetenv(name);

	if (op_strcmp(name, "TZ")) tzset();

	return
#ifdef POSIX_UNSETENV_INT_RETURN
		ret ? OpStatus::ERR :
#endif
		OpStatus::OK;
#endif // POSIX_SETENV_VIA_PUTENV
}

#  ifdef POSIX_SETENV_VIA_PUTENV
PosixModule::Environment::Entry *PosixModule::Environment::Find(const char *name)
{
	size_t len = op_strlen(name);
	for (Entry *run = First(); run; run = run->Suc())
		if (run->Matches(name, len))
			return run;

	return 0;
}

OP_STATUS PosixModule::Environment::Entry::Set(const char *name, const char *value)
{
	if (op_strchr(name, '='))
		return OpStatus::ERR;

	// Lengths of strings, plus 2 for the '=' and the '\0':
	char *pair = OP_NEWA(char, op_strlen(name) + op_strlen(value) + 2);
	if (pair == 0)
		return OpStatus::ERR_NO_MEMORY;

	op_strcpy(pair, name);
	op_strcat(pair, "=");
	op_strcat(pair, value);
	if (putenv(pair))
	{
		OP_DELETEA(pair);
		return OpStatus::ERR;
	}
	OP_DELETEA(m_pair);
	m_pair = pair;
	return OpStatus::OK;
}
#  endif // POSIX_SETENV_VIA_PUTENV
# endif // POSIX_OK_SETENV
#endif // POSIX_MODULE_REQUIRED
