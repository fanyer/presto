/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Karlsson
*/

#if !defined ACCESSORS_INI_H && (defined PREFS_HAS_INI || defined PREFS_HAS_INI_ESCAPED || defined PREFS_HAS_LNG || defined PREFS_HAS_LNGLIGHT)
#define ACCESSORS_INI_H

#include "modules/prefsfile/accessors/prefsaccessor.h"

class PrefsMap;
class PrefsSection;
class PrefsSectionInternal;
class PrefsEntry;
class OpFileDescriptor;
class UnicodeFileInputStream;

/**
 * INI file parser. A class able to parse files containing preferences in
 * the "ini" format, as used in Microsoft Windows. The files are always
 * stored using UTF-8 encoding ("version 2" format). Files written by
 * versions of Opera prior to 6.0 ("version 1" format) are no longer
 * supported, but will be read as if they were version 2 files.
 *
 * <pre>
 *  [food]
 *  fruit = banana
 *  meat = beef
 * </pre>
 *
 */
class IniAccessor : public PrefsAccessor
{
public:

	explicit IniAccessor(BOOL escapes) : m_expandescapes(escapes) {};

	/**
	 * Loads and parses the content in the file and
	 * put the result in the map.
	 * @param file The file to load.
	 * @param map The map to put the result in.
	 */
	virtual OP_STATUS LoadL(OpFileDescriptor *file, PrefsMap *map);

#ifdef PREFSFILE_WRITE
	/**
	 * Saves the content in the map to the file.
	 * @param file The file to save to.
	 * @param map The map to save.
	 */
	virtual OP_STATUS StoreL(OpFileDescriptor *file, const PrefsMap *map);
#endif // PREFSFILE_WRITE

#ifdef UPGRADE_SUPPORT
	/** Description of ini file compatibility. */
	enum CompatibilityStatus
	{
		HeaderNotRecognized,	///< The header was not recognized (or missing).
		HeaderTooOld,			///< Header indicates a too old version.
		HeaderCurrent,			///< Header indicates a recognized version.
		HeaderTooNew,			///< Header indicates a too new version.
		HeaderNull				///< NULL pointer or out of memory while testing.
	};

	/**
	 * Check for valid INI file formats. Checks the first line of the
	 * ini file to see if it is in a compatible format.
	 *
	 * @param header The first line of the INI file.
	 * ®return Information on whether the header is recognized.
	 */
	static CompatibilityStatus IsRecognizedHeader(const uni_char *header);

	/**
	 * @overload
	 * @param header UTF-8 representation of header.
	 * @param len Length of header.
	 */
	static CompatibilityStatus IsRecognizedHeader(const char *header, size_t len);
#endif // UPGRADE_SUPPORT

protected:

	/**
	 * Load data streamed through a UnicodeFileInputStream object.
	 *
	 * @param file Unicode data stream to read.
	 * @param map The map to put the result in.
	 * @param check_header
	 *   Whether to call AllowReadingFile on the first line of the file.
	 * @return Status of the operation.
	 */
	OP_STATUS LoadStreamL(UnicodeFileInputStream *file, PrefsMap *map, BOOL check_header);

#ifdef UPGRADE_SUPPORT
	/**
	 * Check if we should continue reading this file.
	 *
	 * @param header First line of the file we have read.
	 * @return
	 *   Information on whether the header is recognized. A value of
	 *   HeaderNotRecognized is considered as "missing header" and will
	 *   allow continued reading.
	 */
	virtual CompatibilityStatus AllowReadingFile(const uni_char *header);
#endif

	/**
	 * Parses a line in the file.
	 * Sub-task for the Parse function.
	 * @return TRUE if we should stop parsing here (not used in IniAccessor).
	 */
	virtual BOOL ParseLineL(uni_char *, PrefsMap *);

	/**
	 * Cache for the current section pointer, used by the parser code.
	 * Must be cleared before ParseLineL() is called on a new line the
	 * first time (i.e by LoadL()).
	 */
	PrefsSectionInternal *m_current_section;

	/**
	 * Parse a string describing a section e.g. "[personer]".
	 * Creates a new section in the map if successful.
	 * Sub-task for the Parse function.
	 *
	 * @param key_p Pointer to the string to parse.
	 * @param map Map to store to.
	 */
	virtual void ParseSectionL(uni_char *key_p, PrefsMap *map);

	/**
	 * Parse a string describing a key value e.g. "erik = snygg".
	 * Creates a new entry if successful.
	 * Sub-task for the Parse function.
	 * @param key_p Pointer to the string to parse.
	 */
	virtual void ParsePairL(uni_char *key_p);

	/**
	 * Stores a section to a file.
	 * Sub-task for the Store function.
	 * @param f The file to save to.
	 * @param section The section to save.
	 */
	virtual void StoreSectionL(OpFileDescriptor *f, const PrefsSection *section);

	/**
	 * Stores a entry to a file.
	 * Sub-task for the Store function.
	 * @param f The file to save to.
	 * @param entry The entry to save.
	 */
	virtual void StoreEntryL(OpFileDescriptor *f, const PrefsEntry *entry);

private:

	/**
	 * Whether or not escape sequences should be expanded, used
	 * in language files.
	 */
	BOOL m_expandescapes;
};

#endif
