/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4; -*-
 *
 * Copyright (C) 2007-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Edward Welbourne (loosely based on unix/base/common/unixutils.cpp by Morten
 * Stenshorne, Espen Sand).
 */
#include "core/pch.h"
#ifdef POSIX_OK_FILE_UTIL
// For {En,Dec}codeEnvironment, see posix_file_serialize.cpp

#include "platforms/posix/posix_file_util.h"
#include "platforms/posix/posix_native_util.h"
// Needs <unistd.h>, <sys/stat.h> and <string.h>; should get these from the system system.

static
OP_STATUS CreatePathRecursive(char *const path, size_t done,
							  const char *const tail, size_t stop)
{
	OP_ASSERT(path && tail && stop); // Provably true, 2008/Oct/20 (but upsets prevent).
	size_t i = 0;
	while (i < stop && (done == 0 || tail[i] != PATHSEPCHAR))
		path[done++] = tail[i++];

	path[done] = '\0';
	// path[0:done] contains the name of a directory whose existence we should ensure.

	errno = 0;
	int missing = access(path, F_OK);
	int err = errno;
	while (tail[i] == PATHSEPCHAR)
		i++; // Treat many path separators as one.

	if (missing)
		switch (err)
		{
		case EACCES: return OpStatus::ERR_NO_ACCESS;
		case ELOOP: case ENAMETOOLONG: return OpStatus::ERR;
		}
	else if (i < stop)
	{
		// Recurse; no need to tidy up on failure.
		path[done++] = PATHSEPCHAR;
		return CreatePathRecursive(path, done, tail + i, stop - i);
	}
	else
		return OpStatus::OK; // Nothing left to do :-)

	// Missing dir.  Create, recurse and delete on failure:
	if (PosixFileUtil::UseStrictFilePermission())
	{
		mode_t mask = umask(0077);
		errno = 0;
		missing = mkdir(path, 0777);
		err = errno;
		umask(mask);
	}
	else
	{
		errno = 0;
		missing = mkdir(path, 0777);
		err = errno;
	}

	if (missing) // Failed to create
		switch (err)
		{
		case EMLINK: // would cause inode with too many links
		case EROFS: // creating directory on read-only file-system
		case EACCES: // permission denied
			return OpStatus::ERR_NO_ACCESS;

		case ENOSPC: return OpStatus::ERR_NO_DISK;
		default: return OpStatus::ERR;
		}

	path[done] = PATHSEPCHAR;
	if (i >= stop)
		return OpStatus::OK; // Success :-)

	OP_STATUS res = CreatePathRecursive(path, done + 1, tail + i, stop - i);
	if (OpStatus::IsError(res))
	{
		// Failed.  Tidy away the directory we just created.
		path[done] = '\0';
		rmdir(path);
	}
	return res;
}

// static
OP_STATUS PosixFileUtil::CreatePath(const char* filename, bool stop_on_last_pathsep)
{
	if (filename == 0)
		return OpStatus::ERR_NULL_POINTER;

	size_t length;
	if (stop_on_last_pathsep)
	{
		const char *tail = op_strrchr(filename, PATHSEPCHAR);
		length = tail ? tail - filename : 0; // treat filename[-1] as an implicit pathsep.
	}
	else
		length = op_strlen(filename);

	if (length == 0)
		return OpStatus::OK; // Nothing to do.

	OpString8 buffer; // So stack takes care of releasing the memory we allocate.
	char *path = buffer.Reserve(length + 1);
	if (path == 0)
		return OpStatus::ERR_NO_MEMORY;

	return CreatePathRecursive(path, 0, filename, length);
}

# ifdef POSIX_OK_NATIVE
// static
OP_STATUS PosixFileUtil::CreatePath(const uni_char* filename, bool stop_on_last_pathsep)
{
	PosixNativeUtil::NativeString locale_string(filename);
	RETURN_IF_ERROR(locale_string.Ready());
	return CreatePath(locale_string.get(), stop_on_last_pathsep);
}
# endif // POSIX_OK_NATIVE

// static
OP_STATUS PosixFileUtil::RealPath(const char* filename, char out[_MAX_PATH+1])
{
	if (filename == NULL)
		return OpStatus::ERR_NULL_POINTER;

	errno = 0;
	if (
#ifdef DEBUG_ENABLE_OPASSERT
		char *ans =
#endif
		realpath(filename, out))
	{
		OP_ASSERT(ans == out);
		OP_ASSERT(op_strstr(out, "/../") == 0);
		return OpStatus::OK;
	}

	switch (errno)
	{
		/* Read or search permission was denied for a component of
		 * file_name. */
	case EACCES: return OpStatus::ERR_NO_ACCESS;
		/* Insufficient storage space is available. */
	case ENOMEM: return OpStatus::ERR_NO_MEMORY;

		/* Files must exist: */
	case ENOENT:
		/* A component of file_name does  not  name  an  existing  file  or
		 * file_name points to an empty string. */
	case ENOTDIR:
		/* A component of the path prefix is not a directory. */
		return OpStatus::ERR_FILE_NOT_FOUND;

		/* The file_name argument is a null pointer. */
	case EINVAL: OP_ASSERT(!"But I checked !"); break;

		/* An error occurred while reading from the file system. */
	case EIO: break;

		/* A loop exists in symbolic links encountered during resolution
		 * of the path argument.
		 *
		 * OR
		 *
		 * More than {SYMLOOP_MAX} symbolic links were encountered
		 * during resolution of the path argument.
		 */
	case ELOOP: break;

		/* The length of the file_name argument exceeds {PATH_MAX} or a
		 * pathname component is longer than {NAME_MAX}.
		 *
		 * OR
		 *
		 * Pathname resolution of a symbolic link produced an
		 * intermediate result whose length exceeds {PATH_MAX}.
		 */
	case ENAMETOOLONG: break;

	default: OP_ASSERT(!"Undocumented errno value set by realpath() !");
	}
	return OpStatus::ERR;
}

OP_STATUS PosixFileUtil::FullPath(const char* filename, char out[_MAX_PATH+1])
{
	if (filename == NULL)
		return OpStatus::ERR_NULL_POINTER;

	OP_STATUS err = RealPath(filename, out);
	size_t cut = op_strlen(filename);
	while (err == OpStatus::ERR_FILE_NOT_FOUND && cut > 0)
	{
		while (--cut > 0 && filename[cut] != PATHSEPCHAR)
			/* skip */ ;

		if (cut > 0)
		{
			OpString8 head;
			RETURN_IF_ERROR(head.Set(filename, cut));
			err = RealPath(head.CStr(), out);
		}
		else if (filename[0] == PATHSEPCHAR) // non-existent absolute path
		{
			out[0] = '\0';
			err = OpStatus::OK;
		}
		else if (OpStatus::IsSuccess(err = RealPath(".", out)) &&
				 op_strlcat(out, PATHSEP, _MAX_PATH + 1) > _MAX_PATH)
			err = OpStatus::ERR; // name too long !
	}
	RETURN_IF_ERROR(err);

	if (op_strlcat(out, filename + cut, _MAX_PATH + 1) > _MAX_PATH)
		return OpStatus::ERR;

	// Ensure no trailing path separators:
	cut = op_strlen(out);
	while (cut > 1 && out[cut-1] == PATHSEPCHAR)
		out[--cut] = '\0';

	return OpStatus::OK;
}

# ifdef POSIX_OK_NATIVE
// static
OP_STATUS PosixFileUtil::RealPath(const uni_char* filename, char out[_MAX_PATH+1])
{
	if (filename == NULL)
		return OpStatus::ERR_NULL_POINTER;

	PosixNativeUtil::NativeString name(filename);
	RETURN_IF_ERROR(name.Ready());
	return RealPath(name.get(), out);
}

// static
OP_STATUS PosixFileUtil::FullPath(const uni_char* filename, char out[_MAX_PATH+1])
{
	if (filename == NULL)
		return OpStatus::ERR_NULL_POINTER;

	PosixNativeUtil::NativeString name(filename);
	RETURN_IF_ERROR(name.Ready());
	return FullPath(name.get(), out);
}

// static
OP_STATUS PosixFileUtil::ReadLink(const char* link, OpString* target)
{
	OP_ASSERT(link);
	OP_ASSERT(target);

	// Keep allocating in 256 increments ...
	const size_t STEP = 256;

	// ... at least up to some reasonable limit.
	const size_t MAX = 16384;

	size_t bufsize = STEP;
	OpAutoArray<char> buf;

	while (bufsize < MAX)
	{
		buf.reset(OP_NEWA(char, bufsize));

		if (!buf.get())
			return OpStatus::ERR_NO_MEMORY;

		ssize_t ret = readlink(link, buf.get(), bufsize);

		if (ret < 0)
		{
			switch (errno)
			{
			/* Search permission was denied for a component of the path. */
			case EACCES: return OpStatus::ERR_NO_ACCESS;
			/* Insufficient kernel memory was available. */
			case ENOMEM: return OpStatus::ERR_NO_MEMORY;

			/* Files must exist. */
			case ENOENT:	/* The named files does not exist: */
			case ENOTDIR:	/* A component of the path prefix is not a directory. */
				return OpStatus::ERR_FILE_NOT_FOUND;

			/* The named file is not a symbolic link. */
			case EINVAL:
			/* An error occurred while reading from the file system. */
			case EIO:
			/* Too many symbolic links were encountered in translating the
			* pathname. */
			case ELOOP:
			/* A pathname, or a component of a pathname was too long. */
			case ENAMETOOLONG:
			default:
				return OpStatus::ERR;
			}
		}
		else if (static_cast<size_t>(ret) < bufsize)
		{
			buf.get()[ret] = '\0';
			return PosixNativeUtil::FromNative(buf.get(), target);
		}

		// Keep trying until buffer is big enough.
		bufsize += STEP;
	}

	return OpStatus::ERR; // bufsize >= MAX.
}

# endif // POSIX_OK_NATIVE
#endif // POSIX_OK_FILE_UTIL
