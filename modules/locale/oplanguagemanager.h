/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#ifndef OPLANGUAGEMANAGER_H
#define OPLANGUAGEMANAGER_H

#include "modules/locale/locale-dbversion.h"
#include "modules/util/opstring.h"
#ifdef LOCALE_CONTEXTS
# include "modules/url/url_id.h"
#endif

/** Global language manager object. */
#define g_languageManager (g_opera->locale_module.OpLanguageManager())

/**
 * Abstract interface for locale string manager.
 *
 * This is the interface to implement for locale string handling. Most platforms
 * would use the LanguageManager implementation, which is a cross-platform
 * implementation, but platforms with special needs can derive directly from
 * this. This interface is the only one used from within the core.
 *
 * @see Str
 * @see Str::LocaleString
 * @author Peter Krefting
 */
class OpLanguageManager
{
public:
	OpLanguageManager() {};
	virtual ~OpLanguageManager() {};

	/** Writing direction. */
	enum WritingDirection
	{
		LTR,		/**< Left-to-right */
		RTL			/**< Right-to-left */
	};

#ifdef LOCALE_CONTEXTS
	/** Set the context for the language manager. The context given
	  * determines which strings language manager will return, in a
	  * implementation-defined way.
	  *
	  * The context is valid until the next call to SetContext().
	  *
	  * @param context The context to switch to.
	  */
	virtual void SetContext(URL_CONTEXT_ID context);
#endif

	/** Retrieve a translation string. This version copies a reference
	  * of the string into the submitted UniString object. In most cases,
	  * this can be done without expensive reallocation.
	  *
	  * The implementation of the API guarantees that the UniString object
	  * returned will remain valid, either by internally using the
	  * UniString framework for reference counting, or by explicitly
	  * copying the data to the passed UniString object.
	  *
	  * Asking for an undefined string is not considered an error, and
	  * will return either an empty string (""), or a fall-back (if
	  * TWEAK_LOC_STRING_NOT_FOUND is enabled). Passing Str::NOT_A_STRING
	  * will always return the empty string.
	  *
	  * @param num The number of the requested string. This MUST be an
	  * instance of Str::LocaleString. You MUST NOT use the numerical
	  * equivalences of the enumeration members in cross-platform code.
	  *
	  * @param[out] s The resulting string.
	  *
	  * @return OK if the string was copied correctly, or an appropriate
	  * error code.
	  */
	virtual OP_STATUS GetString(Str::LocaleString num, UniString &s) = 0;

	/** Retrieve a translation string in an OpString. This version will
	  * LEAVE if encountering a fatal error, such as running out of memory,
	  * or not being able to access the storage of strings. Asking for an
	  * undefined string is not defined as a fatal error, and will return
	  * either an empty string (""), or a fall-back (if
	  * TWEAK_LOC_STRING_NOT_FOUND is enabled). Passing Str::NOT_A_STRING
	  * will always return the empty string.
	  *
	  * Internally this is a wrapper around the UniString version.
	  *
	  * @param num The number of the requested string. This MUST be an
	  * instance of Str::LocaleString. You MUST NOT use the numerical
	  * equivalences of the enumeration members in cross-platform code.
	  *
	  * @param[out] s The resulting string. This string will always be set
	  * to a non-NULL value if the call succeeds.
	  *
	  * @return The length of the returned string, zero if an empty string
	  * is returned.
	  */
	int GetStringL(Str::LocaleString num, OpString &s);

	/** Retrieve a translation string in an OpString. This version will
	  * return an error code if anything goes wrong. Asking for an
	  * undefined string is not considered an error, and will return
	  * either an empty string (""), or a fall-back (if
	  * TWEAK_LOC_STRING_NOT_FOUND is enabled). Passing Str::NOT_A_STRING
	  * will always return the empty string.
	  *
	  * Internally this is a wrapper around the UniString version.
	  *
	  * @param num The number of the requested string. This MUST be an
	  * instance of Str::LocaleString. You MUST NOT use the numerical
	  * equivalences of the enumeration members in cross-platform code.
	  *
	  * @param[out] s The resulting string. This string will always be set
	  * to a non-NULL value if OpStatus::OK is returned.
	  *
	  * @return The status of the operation.
	  */
	OP_STATUS GetString(Str::LocaleString num, OpString &s);

	/** Retrieve the language of the translation. This is the ISO-639-1 (two
	  * letter) code for the language. The returned pointer is owned by the
	  * OpLanguageManager, and is valid until the next call to SetContext(),
	  * or, if LOCALE_CONTEXTS is undefined, for the entire lifetime of
	  * the object.
	  *
	  * @return The translation language. Must be a valid non-empty string.
	  */
	virtual const OpStringC GetLanguage() = 0;

	/** Retrieve the version number of the language database accessed by
	  * OpLanguageManager. This should be compared to the database version
	  * it is built against, which is defined as LANGUAGE_DATABASE_VERSION
	  * in locale-dbversion.h.
	  *
	  * @return The database version of the language file, or zero if none
	  * was found.
	  */
	virtual unsigned int GetDatabaseVersionFromFileL() = 0;

	/** Retrieve the directionality of the language.
	  *
	  * @return The directionality of the language.
	  */
	virtual WritingDirection GetWritingDirection() = 0;
};

#endif // OPLANGUAGEMANAGER_H
