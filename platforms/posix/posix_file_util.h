/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4; -*-
 *
 * Copyright (C) 2007-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef POSIX_FILE_UTILS_H
#define POSIX_FILE_UTILS_H __FILE__
# ifdef POSIX_OK_FILE_UTIL
#include "platforms/posix/posix_native_util.h"

/* Somewhat perverse location for definition !
 *
 * Because g_opera's type is incomplete until after PosixModule (one of its
 * members) is defined, we can't inline this in PosixModule.  At present, this
 * header is the only client of the method, so is an adequate - if perverse -
 * place to declare the inline.  If we get other clients later, we may need a
 * separate header just for it ...
 */
// static
inline bool PosixModule::Ready()
{ return g_opera != 0 && g_opera->posix_module.m_initialised; }

/** Namespace for utilities relating to the file-system.
 *
 * This packages various POSIX C APIs - adding support for uni_char and OpString
 * where relevant; and returning OP_STATUS rather than setting errno.
 */
class PosixFileUtil
{
	//* This class is really a namespace; don't bother to instantiate it !
	PosixFileUtil() {}
public:
	/**
	 * Determines the file permission level to use when creating files or
	 * directories.  The setting is read from the OPERA_STRICT_FILE_PERMISSIONS
	 * environment variable at start-up; see PosixNativeUtil::AffirmativeEnvVar
	 * for the value it needs to count as affirmative, requesting strict file
	 * modes.
	 *
	 * @return True when we should use a strict setting, otherwise false.
	 */
	static bool UseStrictFilePermission()
	{
		return PosixModule::Ready()
			? g_opera->posix_module.GetStrictFilePermission()
			: PosixNativeUtil::AffirmativeEnvVar("OPERA_STRICT_FILE_PERMISSIONS");
	}

	/**
	 * Creates all path components of a path with filename.  Text after the last
	 * path separator, or the whole text if none appears, is assumed to be the
	 * filename when 'stop_on_last_pathsep' is true and no directory will be
	 * made out of that component.
	 *
	 * @param filename The path to be constucted.
	 * @param stop_on_last_pathsep Everything after the last path separator
	 *        is ignored when true; so no-op unless a separator is present.
	 * @return True on success, otherwise false.
	 */
	static OP_STATUS CreatePath(const char* filename, bool stop_on_last_pathsep);
#ifdef POSIX_OK_NATIVE
	/**
	 * @overload
	 */
	static OP_STATUS CreatePath(const uni_char* filename, bool stop_on_last_pathsep);
#endif // POSIX_OK_NATIVE

	/**
	 * Converts the filename to a canonical name for the same file-system
	 * object, which must exist.
	 *
	 * @param filename The name to convert, as const uni_char* or const char*.
	 * @param out A char[_MAX_PATH+1] array, in which to store the answer.
	 * @return See OpStatus.
	 */
	static OP_STATUS RealPath(const char* filename, char out[_MAX_PATH+1]);
#ifdef POSIX_OK_NATIVE
	/**
	 * @overload
	 */
	static OP_STATUS RealPath(const uni_char* filename, char out[_MAX_PATH+1]);
#endif // POSIX_OK_NATIVE

	/**
	 * Converts the filename to a canonical name for the same file-system
	 * object.  If it doesn't exist, its longest extant prefix ending just
	 * before a / is identified; the canonical name for that is then combined
	 * with the tail that followed it.  If no such prefix exists, "./" is
	 * tacitly prepended; if "." does not exist, ERR_FILE_NOT_FOUND shall be
	 * returned.  The final result is guaranteed to not end in a trailing path
	 * separator, except when it is "/".
	 *
	 * @param filename The name to convert, as const uni_char* or const char*.
	 * @param out A char[_MAX_PATH+1] array, in which to store the answer.
	 * @return See description and OpStatus.
	 */
	static OP_STATUS FullPath(const char* filename, char out[_MAX_PATH+1]);
#ifdef POSIX_OK_NATIVE
	/**
	 * @overload
	 */
	static OP_STATUS FullPath(const uni_char* filename, char out[_MAX_PATH+1]);
#endif // POSIX_OK_NATIVE

#ifdef POSIX_SERIALIZE_FILENAME // In src/posix_file_serialize.cpp
private:
    /** Append environment variable's value to a string.
	 *
     * Appends the specified value of an environment variable to the specified
     * destination string.  The specified value is transformed from native
     * encoding to unicode (see PosixNativeUtil::FromNative()).  If the
     * specified destination string terminates with a PATHSEPCHAR and the
     * specified environment variable starts with a PATHSEPCHAR, then one of
     * these redundant PATHSEPCHARs can be removed.
     *
     * @param dst is the destination string that is appended with the specified
     *  environment variable value.
     * @param env is the value of an environment variable.  The value is
     *  expected in native encoding (e.g. as returned by op_getenv()).
     * @param remove_trailing_sep indicates if a trailing PATHSEPCHAR in the
     *  specified environment variable value should be removed.  If you append
     *  the destination string with some more text that starts with a
     *  PATHSEPCHAR, then you want to set this argument to true, otherwise
     *  false.
     * @return See OpStatus.
     */
    static OP_STATUS AppendEnvironment(OpString* dst, const char* env,
									   bool remove_trailing_sep);

public:
	/** Replace uses of environment variables and escapes with their values.
	 *
	 * This includes replacing escapes with the values they represent; and, when
	 * TWEAK_POSIX_TILDE_EXPANSION is enabled, replacing '~', when used as a
	 * directory name, with the user's home directory.
	 *
	 * @param src Text to be decoded
	 * @param dst String in which to save decoded text
	 * @return See OpStatus.
	 */
	static OP_STATUS DecodeEnvironment(const uni_char* src, OpString* dst);

	/** Produce a text that DecodeEnvironment will map back to the original.
	 *
	 * Note that this includes escaping content in the original that would
	 * otherwise trigger replacement in DecodeEnvironment.  A leading prefix of
	 * the given text, if followed by a directory separator, may also be
	 * replaced with an environment variable reference or, for $HOME when
	 * TWEAK_POSIX_TILDE_EXPANSION is enabled, a '~' as directory.  The set of
	 * environment variables considered is negotiable with the module owner;
	 * it may be replaced using a tweak at a later date.
	 *
	 * @param src Text to be encoded.
	 * @param answer Pointer to where to record the start-address of a
	 * freshly-allocated string; on success, caller is responsible for
	 * OP_DELETEA()ing this start-address (if non-NULL).
	 * @return See OpStatus.
	 */
	static OP_STATUS EncodeEnvironment(const uni_char* src, uni_char** answer);
#endif // POSIX_SERIALIZE_FILENAME

#ifdef POSIX_OK_NATIVE
	/** Reads a symbolic link.
	 *
	 * Dereferences the given symbolic link (once), converts the path to
	 * which it points (assumed to be in native encoding) to UTF-16 and
	 * stores the result in target.
	 *
	 * Note that the result may be relative to the directory containing
	 * the link. The file it references may either not exist or be a
	 * further symbolic link; if what you need is the file at the end of
	 * the chain, see RealPath().
	 *
	 * @param link The symbolic link path (native encoding).
	 * @param target The object to store the contents in.
	 * @return OpStatus::OK on success, otherwise:
	 *         OpStatus::ERR_NO_ACCESS on access denied (EACCES),
	 *         OpStatus::ERR_NO_MEMORY on OOM (ENOMEM),
	 *         OpStatus::ERR_FILE_NO_FOUND on (ENOENT, ENOTDIR),
	 *         OpStatus::ERR for other errors (EIO, ELOOP, ENAMETOOLONG,
	 *                                         EINVAL).
	 */
	static OP_STATUS ReadLink(const char* link, OpString* target);
#endif // POSIX_OK_NATIVE
};

# endif // POSIX_OK_FILE_UTIL
#endif // POSIX_FILE_UTILS_H
