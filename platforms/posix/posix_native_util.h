/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4; -*-
 *
 * Copyright (C) 2007-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef POSIX_UTIL_H
#define POSIX_UTIL_H
# ifdef POSIX_OK_MISC
#  ifdef POSIX_OK_NATIVE
#include "modules/locale/locale-enum.h"
#  endif
#  ifndef POSIX_INTERNAL
#define POSIX_INTERNAL(X) DEPRECATED(X)
#  endif
#include <locale.h> // for setlocale

/** Utilities used in this module.
 *
 * This module declares (and needs) these utilities if POSIX_NEED_NATIVE is
 * defined (see module.export) but only implements them if POSIX_OK_LOCALE is
 * defined.  When declared but not implemented here, the platform needs to
 * implement the functions below.  At time of writing, that means you can only
 * use the directory API if your locale implementation is adequate.
 *
 * Aside from the native string form of AffirmativeEnvVar, these utilities
 * depend on adequately POSIX-conformant implementations of:
 * \li \c wcstombs
 * \li \c mbstowcs
 * plus, particularly if \c TWEAK_POSIX_OWN_WCMB_LEN (q.v.) is needed,
 * \li \c wctomb
 * \li \c mbtowc
 *
 * This namespace is implemented as a class for the sake of platforms whose
 * compilers do not support namespaces.  Its "methods" are all static.  It
 * provides a convenience class, NativeString (q.v.), which really is a class.
 *
 * See also: TWEAK_POSIX_SYS_WCHAR (which may be problematic with the last two
 * functions, if needed) and the macros UINT_STRSIZE() and INT_STRSIZE().
 */
class PosixNativeUtil
{
	//* Fatuous private constructor: this is just a namespace in disguise.
	PosixNativeUtil() {}
public:
	/** Test whether an environment variable is set to an affirmative value.
	 *
	 * Currently recognised affirmative values are YES, TRUE and any non-zero
	 * number (may add support for some language-specific values some day);
	 * check is case-insensitive, ignores leading spaces and accepts anything
	 * that begins with something it would accept (i.e. it ignores arbitrary
	 * suffixes on values it considers true).
	 *
	 * @param name Name of the environment variable (as native or Unicode string).
	 * @return True if the variable is set to an affirmative value, else false.
	 */
	static bool AffirmativeEnvVar(const char *name);
	// That's used fairly generally, so only conditioned on MISC, not NATIVE.
# ifdef POSIX_OK_NATIVE
	/*
	 * @overload
	 */
	static bool AffirmativeEnvVar(const uni_char *name);

	/** Wrap POSIX getenv so that it copes with and yields uni_char*
	 *
	 * @deprecated Use the non-LEAVE()ing UniGetEnv() variants instead.
	 *
	 * @param name Name of the environment variable (as char or uni_char); non-NULL.
	 * @param namelen Optional length of initial substring of name to use.
	 * @return The expanded environment variable, as a Unicode string; caller is
	 * responsible for OP_DELETEA()ing it subsequently.  NULL if no such variable.
	 */
	DEPRECATED(static uni_char *UniGetEnvL(const char *name, size_t namelen));
	/*
	 * @overload
	 */
	DEPRECATED(static uni_char *UniGetEnvL(const char *name));
	/*
	 * @overload
	 */
	DEPRECATED(static uni_char *UniGetEnvL(const uni_char *name, size_t namelen));
	/*
	 * @overload
	 */
	DEPRECATED(static uni_char *UniGetEnvL(const uni_char *name));

	/** Wrap POSIX getenv to cope with uni_char* and set an OpString.
	 *
	 * If no such environment variable is available, value.Empty() shall be
	 * performed and OK returned.  Note that if getenv() returns a non-NULL
	 * pointer to an empty string, value.CStr() shall be non-NULL (unless OOM).
	 *
	 * @param value Reference to OpString in which to store result.
	 * @param name Name of the environment variable (as char or uni_char); non-NULL.
	 * @param namelen Optional length of initial portion of name to use.
	 * @return See OpStatus.  Check for ERR_NO_MEMORY (at least).
	 */
	static OP_STATUS UniGetEnv(OpString &value, const char *name, size_t namelen);
	/*
	 * @overload
	 */
	static OP_STATUS UniGetEnv(OpString &value, const char *name);
	/*
	 * @overload
	 */
	static OP_STATUS UniGetEnv(OpString &value, const uni_char *name, size_t namelen);
	/*
	 * @overload
	 */
	static OP_STATUS UniGetEnv(OpString &value, const uni_char *name);

	/* FIXME: Perror writes to stderr, not a PosixLogger stream of our chosing.
	 * TODO: re-work, using strerror(), and move to posix_logger.h
	 */

	/** Language-sensitive perror().
	 *
	 * This is a replacement for the POSIX perror function.  It builds a message
	 * (analogous to what one would pass to perror overtly) out of a language
	 * string and a system function (which shall be enclosed in square brackets
	 * at output, if given); if both parts are given, they are separated by a
	 * space.  It emits this message to standard error, followed by a colon and
	 * space if the message was non-empty, ending with strerror()'s
	 * locale-specific text for the given errno value, and a newline.
	 *
	 * @param message LocaleString indicating context of the error.
	 * @param sysfunc Name of a system function implicated in the error.
	 * @param err The value of errno after the failing system function.
	 * @deprecated Use PosixLogger::PError() or call strerror() directly.
	 */
	POSIX_INTERNAL(static void Perror(Str::LocaleString message,
									  const char *sysfunc, int err));

	/* For implementations of {From,To}Native, see src/posix_native.cpp (it's a
	 * lot of code, enough to be worth separating out from the rest of this
	 * namespace's implementation). */

	/** Convert Unicode string to something the operating system should understand.
	 *
	 * In place of target buffer and space, you can pass a (non-NULL) pointer to
	 * an OpString8 to serve as flexible storage; the converted string will be
	 * appended to any prior content.  When buffer and space are passed, a
	 * return of ERR_OUT_OF_RANGE indicates that representing str requires more
	 * space than was provided: but as much has been converted as will fit.
	 *
	 * @param str The string to convert.
	 * @param target Buffer into which to save result.
	 * @param space Limit on how many characters (including '\\0' terminator)
	 * may be written to target.
	 * @param len Optional limit on how much of str to convert; ~0 for unlimited.
	 * @return See OpStatus; ERR_PARSING_FAILED indicates that str contains
	 * characters which cannot be represented in the native encoding.
	 */
	static OP_STATUS ToNative(const uni_char* str,
							  char *target, size_t space,
							  size_t len = ~(size_t)0);
	/**
	 * @overload
	 */
	static OP_STATUS ToNative(const uni_char* str,
							  OpString8 *target,
							  size_t len = ~(size_t)0);
	/**
	 * @overload
	 */
	static OP_STATUS ToNative(const UniString& str,
							  char *target, size_t space,
							  size_t len = ~(size_t)0);
	// len is last, despite being associated with str, because it's optional.

	/** Convert a string from operating system to Unicode.
	 *
	 * In place of target buffer and space, you can pass a (non-NULL) pointer to
	 * an OpString to serve as flexible storage; the converted string will be
	 * appended to any prior content.  When target and space are passed, a
	 * return of ERR_OUT_OF_RANGE indicates that representing str requires more
	 * space than was provided: but as much has been converted as will fit.  If
	 * a truncated value is acceptable, it's available.
	 *
	 * @param str The zero-terminated string to convert.
	 * @param target Buffer into which to save result.
	 * @param space Limit on how many characters (including 0 terminator) may be
	 * written to target.
	 * @return See OpStatus; ERR_PARSING_FAILED indicates that str is not a
	 * valid byte-sequence in the native encoding.
	 */
	static OP_STATUS FromNative(const char* str, uni_char *target, size_t space);
	/**
	 * @overload
	 */
	static OP_STATUS FromNative(const char* str, OpString *target);
	/**
	 * @overload
	 */
	static OP_STATUS FromNative(const char* str, UniString& target);
	// mbtowcs doesn't let us implement a len limit on str sensibly.

	/** Stack-manager for Native packaging of a given Unicode string.
	 *
	 * Creates a native (typically multi-byte-encoded, but encoded using plain
	 * char) string representing the Unicode string passed to its constructor.
	 * Note that the latter is not required to have a life-time beyond the
	 * constructor.  The native string's life-time coincides with that of the
	 * NativeString object; the pointer returned from get() should not be
	 * referenced once the object has passed out of scope.
	 */
	class NativeString : private OpString8
	{
		OP_STATUS m_state;
	public:
		/** Constructor.
		 *
		 * @param str Unicode string whose native form this object is to create,
		 * provide and manage.
		 * @param len Optional length limit on str; if supplied, everything from
		 * str[len] onwards is ignored.
		 */
		NativeString(const uni_char* str, size_t len)
			: m_state(ToNative(str, this, len)) {}
		/**
		 * @overload
		 */
		NativeString(const uni_char* str)
			: m_state(ToNative(str, this)) {}

		/** Post-constructor check.
		 *
		 * Treat this as a second-stage constructor; if there was an problem
		 * converting the string passed to the constructor, this will get you
		 * your report.
		 *
		 * @return See OpStatus.  ERR_OUT_OF_RANGE for string not representable
		 * in the native encoding.
		 */
		OP_STATUS Ready() const { return m_state; }

		/**
		 * Returns locale-encoded string.  The returned string is only valid for
		 * the lifetime of the NativeString object.
		 */
		const char* get() const { OP_ASSERT(OpStatus::IsSuccess(Ready())); return CStr(); }

		/**
		 * As for get(), q.v., but allows you to modify the string.  Always use
		 * get() in preference to this, unless you actually need to modify the
		 * string - that way, we can find modifying uses robustly by searching
		 * for calls to modify.
		 */
		char *modify() { OP_ASSERT(OpStatus::IsSuccess(Ready())); return CStr(); }
	};
# endif // POSIX_OK_NATIVE

	/** Transient locale setting.
	 *
	 * Warning: instantiation (and destruction) of this class may be expensive.
	 * The setlocale function, which it calls, rearranges a potentially large
	 * and complex mess of globals used by assorted libc functions.  If your
	 * libc is well-optimised for such use, this should be no problem: but this
	 * is by no means guaranteed.  It is not uncommon (at least historically)
	 * for libc implementors to assume setlocale only gets called (at most) once
	 * per category, during program start-up, with no later calls so "it can't
	 * matter" if it's a bit expensive.
	 *
	 * Two checks on the locale setting are supported by the instance: Success()
	 * reports true precisely if your requested category has indeed been given
	 * the value you wanted; Happy() reports true if your requested category
	 * does have *some* value, even if it's not the one you asked for.  If
	 * Success() is true, your code must not LEAVE() without ensuring the
	 * instance's destruction, e.g. by ANCHOR()ing it.
	 *
	 * The generally recognised values to which a category can be set (the value
	 * parameter of the constructor) are: "POSIX" and "C" specify a minimal
	 * locale that's compatible with C-language translation (this is the default
	 * value when main() is invoked); the empty string, "", selects an
	 * implementation-defined native environment, typically determined by LC_*
	 * and LANG environment variables; and the NULL pointer makes no change (so
	 * would be pointless here; but it is allowed, as a no-op).  Individual
	 * platforms may define other values.
	 *
	 * Platforms are encouraged to call PosixModule::InitLocale() early in
	 * main(); this will set most locale categories to match the native
	 * environment specified by "", but LC_NUMERIC to the "POSIX" or "C" locale.
	 * Any code that needs to deviate from those settings, should use an
	 * instance of this class to get they change they need while ensuring that
	 * the prior settings are restored.  This class exists principally for use
	 * within this module, where it is used to transiently set LC_NUMERIC to
	 * match the native environment, when needed.  It is also used to set other
	 * categories to the native environment, when needed, but it is expected
	 * that this should be cheap as long as PosixModule::InitLocale() has been
	 * called.
	 *
	 * No instance of this class should have a life-time during which
	 * g_opera->InitL() or g_opera->Destroy() is called (this would possibly
	 * lead to complications).  In general, instances of this class should be
	 * transient: they should have life-times that span just the specific code
	 * for which they are needed.  In particular, during the lifetime of an
	 * instance for category LC_NUMERIC, assorted operations in core code and
	 * elsewhere might not behave as expected.
	 */
	class TransientLocale
	{
		int const m_cat;
		const char * const m_locale;
	public:
		/** Constructor; see class documentation for details.
		 *
		 * @param category One of the POSIX-defined categories of locale
		 * setting, see man setlocale(3posix).
		 */
		TransientLocale(int category)
			: m_cat(category)
			, m_locale(
#ifdef POSIX_FEWER_LOCALE_CHANGES
				category != LC_NUMERIC ?  0 :
#endif
				op_setlocale(category, ""))
			{ OP_ASSERT(Happy()); } // maybe we can get rid of the Happy() check ?

		/** Does the specified category have a value ? */
		bool Happy() const
		{
#ifdef POSIX_FEWER_LOCALE_CHANGES
			if (m_cat != LC_NUMERIC)
				return true;
#endif
			return m_locale || op_setlocale(m_cat, NULL);
		}
		/** Did the specified category get set to the value we wanted ? */
		bool Success() const
		{
#ifdef POSIX_FEWER_LOCALE_CHANGES
			if (m_cat != LC_NUMERIC)
				return true;
#endif
			return m_locale ? true : false;
		}

		/** Destructor: restore prior locale setting. */
		~TransientLocale() { if (m_locale) op_setlocale(m_cat, m_locale); }
	};

# ifdef POSIX_OK_PROCESS
	/** Parse a command-line.
	 *
	 * Split a localized shell command to a null-terminated array of strings
	 * using the given delimiters.  Will un-escape escape sequences and remove
	 * quotation marks.
	 */
	class CommandSplit
	{
		char** m_split;

	public:
		CommandSplit(const char* str, const char* delim = " \t",
					 const char* quote = "\'\"", const char* escape = "\\");
		~CommandSplit();

		/** Access parsed command-line.
		 *
		 * Return the null-terminated array of tokens created by the split, or
		 * NULL if the split failed to allocate sufficient memory.  The returned
		 * array is only valid until this CommandSplit object is released.
		 */
		char** get() { return m_split; } // TODO: apply suitable const-ness
	};
# endif // POSIX_OK_PROCESS
};

# endif // POSIX_OK_MISC
#endif // POSIX_UTIL_H
