/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2004-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Edward Welbourne, based on Unix version by Morten Stenshorne.
 */
// #define INSIDE_PI_IMPL // suppress DEPRECATED (and PURE_FUNCTION).
#include "core/pch.h"
#ifdef POSIX_OK_DIR
# ifdef DIRECTORY_SEARCH_SUPPORT
/** \file
 * Implementation of PosixFolderLister.
 *
 * Construct() does all allocation; Reset() does all deletion, and sets members
 * NULL.  There's a live search in progress precisely when m_dir is non-NULL.
 * Other members are non-NULL during a live search or following a failed
 * Construct()
 *
 * TODO: it may make sense to cache some or all of stat()'s information.
 */
# ifndef POSIX_INTERNAL
#define POSIX_INTERNAL(X) X
# endif
#include "modules/pi/system/OpFolderLister.h"
#include "platforms/posix/posix_native_util.h"

#include <dirent.h>
#include <fnmatch.h>

/**
 * Implement OpFolderLister on top of POSIX APIs.
 *
 * Relies on conforming-enough implementations of:
 * \li \c opendir
 * \li \c readdir_r
 * \li \c closedir
 * \li \c fnmatch
 * \li \c realpath
 * \li \c stat
 * \li \c S_ISDIR
 *
 * See base class for method documentation.
 * This class implementats OpFolderLister::Create().
 */
class PosixFolderLister : public OpFolderLister
{
	OpString8 m_pattern;	///< Native form of pattern passed to Construct().
	DIR *m_dir;				///< POSIX directory entity being traversed.
	char *m_syspath;		///< Native form of full path name of current file.
	char *m_sysname;		///< Points to file name within m_syspath.
	uni_char *m_unipath;	///< Unicode form of full path name of current file.
	uni_char *m_uniname;	///< Points to file name within m_unipath.
	mutable enum { UNKNOWN, ISDIR, ISFILE } m_type;

	/**
	 * Delete-and-NULL everything, ready for fresh Construct() or destruction.
	 */
	void Reset()
	{
		if (m_dir)
		{
			closedir(m_dir);
			m_dir = 0;
		}
		m_pattern.Empty();
		OP_DELETEA(m_syspath);
		m_syspath = m_sysname = 0;
		OP_DELETEA(m_unipath);
		m_unipath = m_uniname = 0;
		m_type = UNKNOWN;
	}
public:
	PosixFolderLister()
		: m_dir(0),
		  m_syspath(0), m_sysname(0),
		  m_unipath(0), m_uniname(0),
		  m_type(UNKNOWN)
		{}

	virtual ~PosixFolderLister() { Reset(); }

	// Abort any on-going search, set up ready to begin a new search:
	virtual OP_STATUS Construct(const uni_char* path, const uni_char* pattern);
	// Get a new search result, ready for Get() methods to access:
	virtual BOOL Next();
	virtual const uni_char* GetFileName() const { return m_uniname; }
	virtual const uni_char* GetFullPath() const
		{ return m_uniname && *m_uniname ? m_unipath : 0; }

	/**
	 * Consults stat, as long as m_sysname indicates a live file.
	 */
	virtual OpFileLength GetFileLength() const
	{
		struct stat st;
		return m_sysname && m_sysname[0] && stat(m_syspath, &st) == 0 ? st.st_size : 0;
	}

	/**
	 * Consults stat, as long as m_sysname indicates a live file.
	 */
	virtual BOOL IsFolder() const
	{
		if (m_type == UNKNOWN)
		{
			struct stat st;
			if (m_sysname && m_sysname[0] && stat(m_syspath, &st) == 0)
				m_type = S_ISDIR(st.st_mode) ? ISDIR : ISFILE;
		}
		return m_type == ISDIR;
	}
};

/** Provide implementation of OpFolderLister::Create.
 * Simply creates a new PosixFolderLister, reporting ERR_NO_MEMORY on failure.
 */
// static
OP_STATUS
#ifndef POSIX_DIRLIST_WRAPPABLE
OpFolderLister::Create
#else
PosixModule::CreateRawLister
#endif
(OpFolderLister** new_lister)
{
	*new_lister = OP_NEW(PosixFolderLister, ());
	if (*new_lister) return OpStatus::OK;
	return OpStatus::ERR_NO_MEMORY;
}

/**
 * Loops on readdir until a fnmatch for m_pattern is found, or directory runs
 * out.  Saves match to m_sysname and m_uniname; or, if no match, terminates the
 * strings at position zero.
 */
BOOL PosixFolderLister::Next()
{
	if (!m_dir)
		return FALSE;

#ifdef _PC_NAME_MAX
	m_sysname[0] = '\0'; // so pathconf just sees the directory name
	OP_ASSERT(POSIX_NAME_MAX >= pathconf(m_syspath, _PC_NAME_MAX));
#endif
	struct dirent *ent;
	union
	{
		struct dirent d;
		/* DSK-253570: Solaris's readdir_r will happily copy more data into
		 * d_name than the declared size of the array; so make sure we at least
		 * own the space it'll be using, to prevent it doing any harm when it
		 * does this.  Fortunately, d_name is the last field in the struct ...
		 */
		char pad[sizeof(*ent) - sizeof(ent->d_name) + POSIX_NAME_MAX + 1];
	} buf;

	m_type = UNKNOWN;
	PosixNativeUtil::TransientLocale ignoreme(LC_CTYPE);
	while (readdir_r(m_dir, &buf.d, &ent) == 0 && ent)
		switch (fnmatch(m_pattern.CStr(), ent->d_name, FNM_PATHNAME))
		{
		case 0:
			OP_ASSERT(ent == &buf.d);

			if (POSIX_NAME_MAX < op_strlcpy(m_sysname, ent->d_name, POSIX_NAME_MAX + 1) ||
				OpStatus::IsError(PosixNativeUtil::FromNative(
									  ent->d_name, m_uniname, POSIX_NAME_MAX + 1)))
				break; // skip unrepresentable names :-(

#ifdef _DIRENT_HAVE_D_TYPE
			switch (buf.d.d_type)
			{
			case DT_LNK:
				(void)IsFolder(); // Will set m_type to ISDIR or ISFILE
				break;
			case DT_UNKNOWN:	m_type = UNKNOWN;	break;
			case DT_DIR:		m_type = ISDIR;		break;
			default:			m_type = ISFILE;	break;
			}
#endif
			return TRUE;

		default:
			PosixNativeUtil::Perror(Str::S_ERR_SYSTEM_CALL, "fnmatch", errno);
			OP_ASSERT(!"Error return from fnmatch");
			break;

		case FNM_NOMATCH:
			break;
		}

	Reset();
	return FALSE;
}

/**
 * Massages path and pattern into canonical form suitable for passing to system
 * functions; saves massaged pattern as m_pattern.  Prepares m_syspath big
 * enough to hold the massaged path plus an individual filename up to
 * POSIX_NAME_MAX long; initializes it as path and points m_sysname to the end,
 * where individual filenames are to be recorded.  Likewise prepares m_unipath
 * and m_uniname as Unicode versions of m_syspath and m_sysname.
 *
 * Then uses opendir() to actually prepare for directory traversal, saving the
 * directory handle in m_dir.  Note that no members shall be changed (in
 * particular, we do no malloc/free) until the next Reset().
 */
OP_STATUS PosixFolderLister::Construct(const uni_char* path, const uni_char* pattern)
{
	Reset(); // Abort any search in progress.
	// Canonicalize and check path:
	char normpath[_MAX_PATH + 1], *lp;
	{
		PosixNativeUtil::NativeString loc(path);
		RETURN_IF_ERROR(loc.Ready());
		lp = realpath(loc.get(), normpath);
	}
	OP_ASSERT(lp == 0 || lp == normpath); // lp points to auto memory, no leak-risk.
	{
		struct stat st;
		if (lp == 0 || stat(lp, &st) != 0 || !S_ISDIR(st.st_mode))
		{
			if (errno == ENOMEM)
				return OpStatus::ERR_NO_MEMORY;

			/* No such directory, or alleged directory isn't a directory.
			 * OpFolderLister::Construct() says to return an empty folder lister
			 * in this case; our m_dir is NULL, so that's what this is.
			 */
			return OpStatus::OK;
		}
	}
	// NB: nothing is allocated at this point, though normpath is on stack.
	RETURN_IF_ERROR(PosixNativeUtil::ToNative(pattern, &m_pattern));

	/* We *could* re-use m_dir, using rewinddir(), if lp coincides suitably with
	 * m_syspath; however, restructuring to make that work would complicate
	 * Reset() and all three of its users. */

	// Determine path length:
	int len = op_strlen(lp);
	bool add_slash = len > 0 && lp[len-1] != PATHSEPCHAR;
	if (add_slash)
		len++;

	// Prepare buffer for 8-bit file names:
	m_syspath = OP_NEWA(char, len + POSIX_NAME_MAX + 1);
	if (m_syspath == 0)
		return OpStatus::ERR_NO_MEMORY;

	m_sysname = m_syspath + len;
	op_strcpy(m_syspath, lp);
	if (add_slash)
		op_strcpy(m_sysname - 1, PATHSEP);
	m_sysname[POSIX_NAME_MAX] = '\0'; // Ensure '\0'-termination, once and for all.

	OpString paved;
	RETURN_IF_ERROR(PosixNativeUtil::FromNative(lp, &paved));

	// Determine uni_char path length:
	len = paved.Length();
	OP_ASSERT(add_slash == (len > 0 && paved[len-1] != PATHSEPCHAR));
	if (add_slash)
		len++;

	// Prepare buffer for Unicode file names:
	m_unipath = OP_NEWA(uni_char, len + POSIX_NAME_MAX + 1);
	if (m_unipath)
	{
		m_uniname = m_unipath + len;
		uni_strcpy(m_unipath, paved.CStr());
	}

	if (!m_unipath)
		return OpStatus::ERR_NO_MEMORY;

	if (add_slash)
		uni_strcpy(m_uniname - 1, UNI_L(PATHSEP));

	OP_ASSERT(m_uniname[0] == '\0');

	// Prepare to iterate:
	m_dir = opendir(lp);
	if (!m_dir)
		switch (errno)
		{
		case ENOMEM:
		case ENOBUFS:
			OP_ASSERT(!"Undocumented error code from opendir");
			return OpStatus::ERR_NO_MEMORY;

		/* Otherwise: no such directory, not allowed to list, etc.; PI mandates
		 * that we initialize as an empty iterator and return happy. */
		}

	return OpStatus::OK;
}

# endif // DIRECTORY_SEARCH_SUPPORT
#endif // POSIX_OK_DIR
