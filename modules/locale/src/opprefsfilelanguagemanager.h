/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#if defined LOC_LNGFILELANGMAN && !defined OPPREFSFILELANGUAGEMANAGER_H
#define OPPREFSFILELANGUAGEMANAGER_H

#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"

class PrefsFile;
class OpFileDescriptor;

/**
 * Platform-independent file-based locale string manager.
 *
 * This class handles the locale strings used in the UI and elsewhere for
 * Opera. This implementation of the OpLanguageManager reads from the
 * supplied translation file. Platforms with special needs can re-implement
 * the OpLanguageManager interface in a manner they see fit. Similarly,
 * extensions to the translation file format can be handled by subclassing
 * this.
 *
 * @author Peter Krefting
 */
class OpPrefsFileLanguageManager : public OpLanguageManager
#ifdef LANGUAGEMANAGER_CAN_RELOAD
	, public PrefsCollectionFilesListener
#endif
{
public:
	OpPrefsFileLanguageManager();
	virtual ~OpPrefsFileLanguageManager();

#ifdef LANGUAGE_FILE_SUPPORT
	/** Load a translation into the language manager. It will fetch the file
	  * to load from the preferences and load it up. Any errors that occur
	  * during load will cause a LEAVE and abort the start-up procedure.
	  *
	  * If LANGUAGEMANAGER_CAN_RELOAD is set and this method is called after
	  * the initial setup (from the listener interface), it will retain the
	  * currently loaded strings on failure.
	  *
	  * Do not use this method on the global language manager interface
	  * (g_languageManager) outside of the locale module. If you are using
	  * the OpPrefsFileLanguageManager as a stand-alone object, you should
	  * probably use LoadTranslationL() instead of LoadL().
	  *
	  * Must only be called from LocaleModule or OpPrefsFileLanguageManager
	  * itself.
	  */
	void LoadL();
#endif

	/** Load a translation into the language manager. The file to load must
	  * be set up before the call to this method. The PrefsFile should use
	  * a PREFS_LNG type file, although that is up to the implementation.
	  * Cascading of language files are handled by the PrefsFile object.
	  *
	  * Do not use this method on the global language manager interface
	  * (g_languageManager) outside of the locale module. If you are using
	  * the OpPrefsFileLanguageManager as a stand-alone object, you should
	  * probably use this API instead of LoadL().
	  *
	  * @param file A pointer to the file to read. The language manager takes
	  * ownership of this object.
	  */
	void LoadTranslationL(PrefsFile *file);

	// Inherited interfaces; see OpLanguageManager for descriptions
	virtual const OpStringC GetLanguage()
	{ return m_language; }

	virtual unsigned int GetDatabaseVersionFromFileL()
	{ return m_db; }

	virtual OP_STATUS GetString(Str::LocaleString num, UniString &s);

	virtual WritingDirection GetWritingDirection()
	{ return m_direction; }

#ifdef LANGUAGEMANAGER_CAN_RELOAD
	// Inherited interfaces; see PrefsCollectionFilesListener for descriptions
	virtual void FileChangedL(PrefsCollectionFiles::filepref pref, const OpFile *newvalue);
#endif

private:
	static int entrycmp(const void *, const void *);

	OpString m_language;
	unsigned int m_db;

	struct stringdb_s
	{
		int id;
		union
		{
			size_t ofs;
			const uni_char *string;
		} s;
	};

	stringdb_s		*m_stringdb;
	UniString		m_strings;
	int				m_number_of_strings;
	WritingDirection m_direction;
};

#endif // LOC_LNGFILELANGMAN && !OPPREFSFILELANGUAGEMANAGER_H
