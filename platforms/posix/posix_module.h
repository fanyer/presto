/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4; -*-
 *
 * Copyright (C) 2007-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef POSIX_MODULE_H
#define POSIX_MODULE_H __FILE__
# ifdef POSIX_OK_MODULE

/* This module needs to be initialized *after* the encodings module is
 * initialized (for sys wchar encoding) and destroyed *after* various client
 * modules, notably url and probably prefs, which use instances of classes which
 * may be using posix logging infrastructure. */
#define POSIX_MODULE_REQUIRED

#ifdef THREAD_SUPPORT
#include <pthread.h>
#endif

#if defined(POSIX_SETENV_VIA_PUTENV) || defined(POSIX_OK_SELECT)
#include "modules/util/simset.h"
#endif

/** Global data for the posix module.
 *
 * Although all platforms known to use posix can, in fact, use globals freely,
 * this module conforms to core's no-globals policy as a generally sound
 * discipline in its own right.
 */
class PosixModule : public OperaModule
{
	/* Member order: by size of type; within each size, alphabetic on
	 * controlling define.  Compromise as needed if a define controls members of
	 * different sizes.  The aim is to avoid padding (but this class is in
	 * practice a singleton, so don't get too hung up on padding), without
	 * sacrificing readability. */
#ifdef POSIX_OK_ASYNC
	class PosixAsyncManager * m_async_boss;
#endif
#ifdef POSIX_OK_LOG
	class PosixLogManager * m_log_boss;
#endif
#ifdef POSIX_OK_SELECT
	class PosixSelector * m_selector;
	AutoDeleteHead m_select_button_groups;
#endif
#ifdef POSIX_OK_SELECT_CALLBACK
	AutoDeleteHead m_sock_cbs; ///< List used by {add,remove}SocketCallback().
#endif
#ifdef POSIX_OK_SIGNAL
	class PosixSignalWatcher * m_signal_watch;
#endif
#ifdef THREAD_SUPPORT
	const pthread_t m_main_thread;
#endif
#ifdef POSIX_OK_FILE_UTIL
	bool m_strict_file_permission : 1;
#endif
#ifdef POSIX_SUPPORT_IPV6
	const bool m_ipv6_support : 1;
	static bool AskIPv6Support();
	bool m_ipv6_usable : 1;
#endif
	bool m_initialised : 1;

public:
	PosixModule()
		: OperaModule() // only here to make punctuation and #if-ery co-operate
#ifdef POSIX_OK_ASYNC
		, m_async_boss(0)
#endif
#ifdef POSIX_OK_LOG
		, m_log_boss(0)
#endif
#ifdef POSIX_OK_SELECT
		, m_selector(0)
#endif
#ifdef POSIX_OK_SIGNAL
		, m_signal_watch(0)
#endif
#ifdef THREAD_SUPPORT
		, m_main_thread(pthread_self())
#endif
#ifdef POSIX_OK_FILE_UTIL
		, m_strict_file_permission(false) // InitL() may change it.
#endif
#ifdef POSIX_SUPPORT_IPV6
		, m_ipv6_support(AskIPv6Support())
		, m_ipv6_usable(true) // we're optimists; see DSK-267027
#endif
		, m_initialised(false)
		{}

	// OperaModule API:
	virtual void InitL(const class OperaInitInfo& ignored);
	virtual void Destroy();

#if 0 // If we ever have any caches to free, implement this:
	virtual BOOL FreeCachedData(BOOL toplevel_context) { return FALSE; }
#endif

	/* Methods specific to this module.
	 *
	 * Order: alphabetic on the #if-ery.
	 */

	/** Test whether we've come through InitL().
	 *
	 * Note that many contexts can honestly make do with just testing g_opera,
	 * or naturally test the member that matters to them anyway, so have no need
	 * to call this method.
	 */
	static bool Ready(); // See posix_file_util.h for inline implementation.

#ifdef POSIX_DIRLIST_WRAPPABLE
	static OP_STATUS CreateRawLister(class OpFolderLister** new_lister);
#endif

#ifdef POSIX_LOWFILE_WRAPPABLE
	/* See TWEAK_POSIX_LOWFILE_WRAPPABLE; this is a surrogate for
	 * OpLowLevelFile::Create(); keep its signature in sync.
	 * It's implemented in src/posix_low_level_file.cpp */
	static OP_STATUS CreateRawFile(class OpLowLevelFile** new_file,
								   const uni_char* path,
								   BOOL serialized=FALSE);
#endif

	// This module's ways of accessing its global data:

#ifdef POSIX_OK_ASYNC
	OP_STATUS InitAsync(); // Needed by net/posix_interface.cpp's QueryLocalIP().
	class PosixAsyncManager * GetAsyncManager() const { return m_async_boss; }
#endif

#ifdef POSIX_OK_FILE_UTIL
	bool GetStrictFilePermission() const { return m_strict_file_permission; }
#endif

#ifdef POSIX_OK_LOG
	class PosixLogManager * GetLoggingManager() const { return m_log_boss; }
#endif

#ifdef POSIX_OK_LOCALE
	/** Initialize locale sensibly.
	 *
	 * Calling this method may improve the speed of various locale-related
	 * activities.  Platforms are encouraged to call it early (g_opera doesn't
	 * yet need to exist even, let alone be initialized): specifically, before
	 * they do anything that uses any locale-specific functions of this module
	 * (notably the methods of PosixNativeUtil), even indirectly - this includes
	 * opening any files using PosixLowLevelFile, including any use of OpFile.
	 * See comments on PosixNativeUtil::TransientLocale for further details.
	 */
	static void InitLocale();
#endif

#ifdef POSIX_OK_SELECT
	OP_STATUS InitSelector(); // Needed by PosixCoreThread::Create()
	class PosixSelector * GetSelector() const { return m_selector; }
	Head *GetSelectButtonGroups() { return &m_select_button_groups; }
#endif

#ifdef POSIX_OK_SELECT_CALLBACK // See src/posix_plugin_callback.cpp for implementation:
	// Nothing but {add,remove}SocketCallback() implementation should use these:
	static class PluginCallbackListener * FindPluginCB(int fd);
	static bool SocketWatch(int fd, class PluginCallbackListener *what);
#endif // POSIX_OK_SELECT_CALLBACK

#ifdef POSIX_OK_SETENV
	/** Singleton class providing access to the system environment.
	 *
	 * Environment variables are a species of global variable - indeed more
	 * global than usual, since they can be visible even in child processes.
	 * Inconveniently, the implementation of changing the environment is not
	 * entirely consistent among platforms.  In particular, some require us to
	 * keep the changes in memory we allocate, that must remain live as long as
	 * they are relevant.  Use of g_env->Set() and g_env->Unset() can make the
	 * needed modifications; see PosixNativeUtil and op_getenv() for
	 * read-access.
	 */
	class Environment
# ifdef POSIX_SETENV_VIA_PUTENV
		: public Head
# endif // POSIX_SETENV_VIA_PUTENV
	{
# ifdef POSIX_SETENV_VIA_PUTENV
		class Entry : public Link
		{
			char *m_pair;
		public:
			Entry() : m_pair(0) {}
			~Entry() { OP_DELETEA(m_pair); }

			OP_STATUS Set(const char *name, const char *value);
			Entry *Suc() { return static_cast<Entry *>(Link::Suc()); }

			bool Matches(const char *name, size_t len)
			{ return m_pair && op_strncmp(m_pair, name, len) == 0 && m_pair[len] == '='; }
		};

		Entry *First() { return static_cast<Entry *>(Head::First()); }
		Entry *Find(const char *name);

# endif // POSIX_SETENV_VIA_PUTENV
	public:
		/** Set an environment variable.
		 * @param name Which name to set in the environment
		 * @param value What value to associate with this name
		 * @return OK on success, else suitable error code.
		 */
		OP_STATUS Set(const char *name, const char *value);

		/** Set an environment variable from Unicode value.
		 * @param name Which name to set in the environment
		 * @param value Unicode for the value to associate with this name
		 * @return OK on success, else suitable error code.
		 */
		OP_STATUS Set(const char *name, const uni_char *value);

		/** Clear an environment variable, if set.
		 * @param name Which name to clear from the environment
		 * @return OK on success (or not set), else suitable error code.
		 */
		OP_STATUS Unset(const char *name);
	} m_env;
#define g_env (g_opera->posix_module.m_env)
#endif // POSIX_OK_SETENV

#ifdef POSIX_OK_SIGNAL
private:
	friend class PosixSignalWatcher;
	void SetSignalWatcher(class PosixSignalWatcher * watcher) { m_signal_watch = watcher; }
public:
	class PosixSignalWatcher * GetSignalWatcher() const { return m_signal_watch; }
#endif

#ifdef POSIX_SUPPORT_IPV6
	bool GetIPv6Support() const { return m_ipv6_support; }
	bool GetIPv6Usable() const { return m_ipv6_support && m_ipv6_usable; }
	void SetIPv6Usable(bool isit) { m_ipv6_usable = isit; }
#endif

#ifdef THREAD_SUPPORT
	/** Are we on the core thread ?
	 *
	 * Generally, threaded code should *know* whether it's run on the core
	 * thread or not, so shouldn't need to test.  However, this can be useful in
	 * assertions.  Any call to this method dereferences g_opera, so should be
	 * preconditioned on a check that g_opera is not NULL; however, if g_opera
	 * is static memory (helpfully initialized to zero) and hasn't yet been
	 * constructed (much less InitL()ed), m_main_thread shall not have been
	 * initialized (which happens during construction).  So this method checks
	 * for that possibility, assuming that we're on core if it arises (on the
	 * unreliable theory that core starts first, before we spawn any other
	 * threads on which this code @em could be run - however, for this module's
	 * purposes, this theory is valid; this module doesn't create threads until
	 * core is initialized).
	 *
	 * @return True precisely if called on the core thread, or before it's
	 * possible to tell.
	 */
	bool OnCoreThread() const { return !m_main_thread || m_main_thread == pthread_self(); }
#endif
};

#ifdef POSIX_OK_SHARED_MEMORY
#include "platforms/posix/posix_shared_memory_mutex.h"
#endif //POSIX_OK_SHARED_MEMORY

# endif // POSIX_OK_MODULE
#endif // POSIX_MODULE_H
