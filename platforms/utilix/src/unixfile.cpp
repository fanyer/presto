/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2004-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand, Edward Welbourne.
 */
#include "core/pch.h"

#ifdef UTILIX

#include "platforms/utilix/unixfile.h"
#include "platforms/posix/posix_native_util.h"

#include <errno.h>
#include <unistd.h>
#include <sys/types.h>

namespace UnixFile
{
	FILE *fopen(const uni_char* filename, const uni_char* mode)
	{
		PosixNativeUtil::NativeString locale_string(filename);
		if( !locale_string.get() )
			return 0;
		PosixNativeUtil::NativeString mode_string(mode);
		if( !mode_string.get() )
			return 0;

		FILE* file = ::fopen(locale_string.get(), mode_string.get());
		return file;
	}

	int access(const uni_char* filename, int mode)
	{
		PosixNativeUtil::NativeString locale_string(filename);
		if( !locale_string.get() )
			return -1;
		return ::access( locale_string.get(), mode );
	}

	int chmod(const uni_char* filename, mode_t mode)
	{
		PosixNativeUtil::NativeString locale_string(filename);
		if( !locale_string.get() )
			return -1;
		return ::chmod( locale_string.get(), mode );
	}

	int mkdir(const uni_char* directory, int mode )
	{
		PosixNativeUtil::NativeString locale_string(directory);
		if( !locale_string.get() )
			return -1;
		else
			return ::mkdir( locale_string.get(), mode );
	}

	int rmdir(const uni_char* directory)
	{
		PosixNativeUtil::NativeString locale_string(directory);
		if( !locale_string.get() )
			return -1;
		else
			return ::rmdir( locale_string.get());
	}

	int rename(const uni_char* from, const uni_char* to)
	{
		PosixNativeUtil::NativeString from_locale_string(from);
		PosixNativeUtil::NativeString to_locale_string(to);
		if (!from_locale_string.get() || !to_locale_string.get())
			return -1;
		else
			return ::rename(from_locale_string.get(), to_locale_string.get());
	}

	int stat(const uni_char* path, struct stat* buf)
	{
		PosixNativeUtil::NativeString locale_string(path);
		if( !locale_string.get() )
			return -1;
		else
			return ::stat( locale_string.get(), buf);
	}

	int lstat(const uni_char* path, struct stat* buf)
	{
		PosixNativeUtil::NativeString locale_string(path);
		if( !locale_string.get() )
			return -1;
		else
			return ::lstat( locale_string.get(), buf);
	}

	int readlink(const uni_char* path, uni_char* buf, size_t bufsize)
	{
		PosixNativeUtil::NativeString locale_string(path);
		if( !locale_string.get() )
			return -1;

		/* The length of a locale string that can just fit into buf is not
		 * something we can easily determine.  Both MB_CUR_MAX * bufsize and
		 * PATH_MAX are safe upper bounds, but they may be gratuitously large.
		 * Tell Eddy if the following assertion fails.
		 */
		const size_t blen = MIN(MB_CUR_MAX * bufsize, PATH_MAX);
		char *b = OP_NEWA(char, blen + 1);
		if (!b)
		{
			OP_ASSERT(!"OOM: we can get away with asking for a smaller buffer."); // see comment above
			errno = ENOMEM;
			return -1;
		}

		int len = ::readlink(locale_string.get(), b, blen);
		if( len >= 0 )
		{
			b[(size_t)len < blen ? (size_t)len : blen] = '\0'; // NB: readlink doesn't '\0'-terminate ! (ever)
			PosixNativeUtil::FromNative(b, buf, bufsize);
			return 0; // should't we mimic readlink and return number of characters written before terminator ?
		}
		OP_DELETEA(b);
		return -1;
	}

	uni_char* getcwd(uni_char* buf, size_t size)
	{
		char b[_MAX_PATH]; // ARRAY OK 2011-04-01 eddy
		const char *ans = ::getcwd(b, _MAX_PATH);
		if (!ans)
			return 0;

		OP_ASSERT(ans == b);

		// We have to fail rather than truncate, so can't pass buf as target:
		if (OpStatus::IsMemoryError(PosixNativeUtil::FromNative(b, buf, size)))
		{
			errno = ENOMEM;
			return 0;
		}

		return buf;
	}

	int mkstemp(uni_char* candidate)
	{
		if (!candidate)
		{
			errno = EINVAL;
			return -1;
		}

		PosixNativeUtil::NativeString locale_string(candidate);
		if( !locale_string.get() )
		{
			errno = ENOMEM; // Not listed as a possible error for mkstemp()
			return -1;
		}

		int fd = ::mkstemp(locale_string.modify());
		if (fd != -1)
			/* We know candidate has enough space for itself plus its trailing
			 * '\0'; and mkstemp merely replaces some X characters in it,
			 * yielding a result no longer than it. */
			PosixNativeUtil::FromNative(locale_string.get(), candidate, uni_strlen(candidate) + 1);

		return fd;
	}

	uni_char* tempnam(const uni_char* directory, const uni_char* prefix)
	{
		/* This implementation deviates significantly from ::tempnam()'s
		 * documented behaviour (it cares about more than five characters of
		 * prefix, it doesn't consult TMPNAM or fall-back on P_tmpdir) and it is
		 * fundamentally an insecure way to get a temporary file; so we should
		 * avoid using it. */

		int plen = (prefix ? uni_strlen(prefix) : 0) + 6; // 6: for the XXXXXX
		int dlen = directory ? uni_strlen(directory) : 0;
		while( dlen > 0 && directory[dlen-1] == '/')
			dlen --;

		uni_char* candidate = OP_NEWA(uni_char, dlen + plen + 2); // 2: one for /, one for '\0'.
		if (!candidate)
		{
			errno = ENOMEM;
			return 0;
		}

		candidate[0] = 0;
		if (dlen)
		{
			uni_strncpy(candidate, directory, dlen);
			uni_strcpy(candidate + dlen, UNI_L("/")); // appends a '\0' for us
		}
		if (prefix)
		{
			uni_strcat(candidate, prefix);
		}
		uni_strcat(candidate, UNI_L("XXXXXX"));

		int fd = mkstemp(candidate);
		if (fd != -1)
		{
			close(fd);
			return candidate;
		}

		OP_DELETEA(candidate);
		return 0;
	}
}

#endif // UTILIX

