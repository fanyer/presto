/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#if !defined OPBINARYLANGUAGEMANAGER_H && defined LOC_BINFILELANGMAN
#define OPBINARYLANGUAGEMANAGER_H

#include "modules/locale/oplanguagemanager.h"

class OpFileDescriptor;

// Inline readers if opposite-endian support is disabled.
#ifdef LOCALE_BINARY_ENDIAN
# define LOCALE_INLINE
#else
# define LOCALE_INLINE inline
#endif

/**
 * Platform-independent binary file-based locale string manager.
 *
 * This class handles the locale strings used in the UI and elsewhere for
 * Opera. This implementation of the OpLanguageManager reads from the
 * supplied translation file. Platforms with special needs can re-implement
 * the OpLanguageManager interface in a manner they see fit.
 *
 * @author Peter Krefting
 */
class OpBinaryFileLanguageManager : public OpLanguageManager
{
public:
	OpBinaryFileLanguageManager();
	virtual ~OpBinaryFileLanguageManager();

	/** Load a translation into the language manager. The file to load must
	  * be set up before the call to this method. The file to be read has
	  * a binary format described in the module documentation for this
	  * module.
	  *
	  * Only to be called from LocaleModule::InitL().
	  *
	  * @param lngfile A pointer to the file to read.
	  */
	void LoadTranslationL(OpFileDescriptor *lngfile);

public:
	// Inherited interfaces; see OpLanguageManager for descriptions
	virtual const OpStringC GetLanguage()
	{ return m_language; }

	virtual unsigned int GetDatabaseVersionFromFileL()
	{ return m_db; }

	virtual OP_STATUS GetString(Str::LocaleString num, UniString &s);

	virtual WritingDirection GetWritingDirection()
	{ return LTR; /* FIXME: Need to update file format to support this flag. */ }

private:
	static int entrycmp(const void *, const void *);

	OpString m_language;
	unsigned int m_db;

	struct stringdb_s
	{
		int id;
		UniString string;
	};

	stringdb_s		*m_stringdb;
	UINT32			m_number_of_strings;

	LOCALE_INLINE static UINT32 ReadNative32L(OpFileDescriptor *);
	LOCALE_INLINE static void ReadNativeStringL(OpFileDescriptor *, uni_char *, size_t);
#ifdef LOCALE_BINARY_ENDIAN
	static UINT32 ReadOpposite32L(OpFileDescriptor *);
	static void ReadOppositeStringL(OpFileDescriptor *, uni_char *, size_t);
#endif
};

#endif // OPBINARYLANGUAGEMANAGER_H && LOC_BINFILELANGMAN
