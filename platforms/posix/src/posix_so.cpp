/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Pleasant surprise: OpDLL can be implemented using POSIX.
 * Based on platforms/unix/base/common/unix_opdll.cpp
 */
#include "core/pch.h"
#ifdef POSIX_OK_SO

#include "modules/pi/OpDLL.h"
#include "platforms/posix/posix_native_util.h"
#include "platforms/posix/posix_logger.h"

#ifdef __hpux__
# include <dl.h>
# define RTLD_NOW		BIND_IMMEDIATE
# define RTLD_LAZY		BIND_DEFERRED
# define dlopen(f, r) shl_load(f, r, 0L)
# define dlclose(h) shl_unload((shl_t)h)
#else
# include <dlfcn.h>
typedef void * shl_t;
#endif

#include <stdlib.h>
#include <unistd.h>

/** Implement OpDLL.
 *
 * This implementation uses dlopen(), dlsym(), dlclose() and dlerror().  It's
 * nothing fancy.  It resolves symbols lazily (except in debug, when it does so
 * promptly).
 */
class PosixSharedLibrary : public OpDLL, private PosixLogger
{
	shl_t m_handle;

	friend class OpDLL;
	PosixSharedLibrary() : PosixLogger(STREAM), m_handle(0) {}
public:
	virtual ~PosixSharedLibrary() { Unload(); }

	virtual OP_STATUS Load(const uni_char* dll_name);
	virtual BOOL IsLoaded() const { return m_handle != 0; }
	virtual OP_STATUS Unload();
#ifdef DLL_NAME_LOOKUP_SUPPORT // SYSTEM_DLL_NAME_LOOKUP
	virtual void* GetSymbolAddress(const char* symbol_name) const;
#else
	virtual void* GetSymbolAddress(int symbol_id) const;
#endif // DLL_NAME_LOOKUP_SUPPORT
};

// static
OP_STATUS OpDLL::Create(OpDLL** new_opdll)
{
	*new_opdll = OP_NEW(PosixSharedLibrary, ());
	return (*new_opdll) ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

OP_STATUS PosixSharedLibrary::Load(const uni_char* dll_name)
{
	if (m_handle)
		return OpStatus::ERR;

	PosixNativeUtil::NativeString file(dll_name); // requires POSIX_OK_NATIVE
	RETURN_IF_ERROR(file.Ready());
	dlerror(); // clear error indicator

	/*
	 * RTLD_NODELETE is not needed when using valgrind version with
	 * http://bugs.kde.org/show_bug.cgi?id=79362 fixed. This bug was originally
	 * fixed in 2004, but then regressed, and the proper fix is only on the
	 * development branch at the time of introducing RTLD_NODELETE here.
	 *
	 * The fix in valgrind will probably be included in versions > 3.6.1.
	 */
#if defined(VALGRIND) && defined(RTLD_NODELETE)
	const int mode = RTLD_NOW | RTLD_NODELETE;
#else
	// Used to be RTLD_LAZY (2002-2011) but see DSK-333320
	const int mode = RTLD_NOW;
#endif // VALGRIND && RTLD_NODELETE

	m_handle = dlopen(file.get(), mode);
	if (m_handle == NULL)
	{
# ifndef __hpux__
		if (dll_name[0] == PATHSEPCHAR && access(file.get(), R_OK) != 0)
			return OpStatus::ERR_FILE_NOT_FOUND;

		else if (const char *bad = dlerror())
			Log(NORMAL, "opera (dlopen): %s\n", bad);
# endif // !__hpux__

		return OpStatus::ERR;
	}

	return OpStatus::OK;
}

OP_STATUS PosixSharedLibrary::Unload()
{
	shl_t handle = m_handle;
	m_handle = NULL;
	dlerror();
	if (handle == NULL || dlclose(handle) != 0)
	{
		if (const char *bad = dlerror())
			Log(NORMAL, "opera (dlclose): %s\n", bad);
		return OpStatus::ERR;
	}

	return OpStatus::OK;
}

# ifdef DLL_NAME_LOOKUP_SUPPORT // SYSTEM_DLL_NAME_LOOKUP
void *PosixSharedLibrary::GetSymbolAddress(const char* symbol_name) const
{
#  ifdef __hpux__
	void *symbol;
	if (shl_findsym(&m_handle, symbol_name, TYPE_UNDEFINED, &symbol) != 0)
		return NULL;
#  else
	// Clear unchecked error.
	dlerror();
	void *symbol = dlsym(m_handle, symbol_name);
	if (const char * bad = dlerror())
	{
		Log(NORMAL, "opera (dlsym): %s\n", bad);
		return NULL;
	}
#  endif // __hpux__

	return symbol;
}
# else
void *PosixSharedLibrary::GetSymbolAddress(int symbol_id) const
{
#error "Unimplemented"
}
# endif

#endif // POSIX_OK_SO
