/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#if !defined PREFSFILE_H && defined PREFS_HAS_PREFSFILE
#define PREFSFILE_H

#include "modules/util/opstring.h"
#include "modules/util/tempbuf.h"

class PrefsSection;
class PrefsSectionInternal;
class PrefsAccessor;
class PrefsMap;
class OpFileDescriptor;
class OpString_list;
class OpGenericStringHashTable;

/** Types of support preference file formats */
enum PrefsType
{
#ifdef PREFS_HAS_INI
	  PREFS_INI				///< Standard INI file format
# ifdef PREFS_HAS_INI_ESCAPED
	, PREFS_INI_ESCAPED		///< Identical to INI, but expands \\t \\n \\r
							// (use one backslash, the second is for Doxygen)
# endif
# ifdef PREFS_HAS_LNG
	, PREFS_LNG				///< Like PREFS_INI_ESCAPED, but with encodings support
# endif
#endif
#ifdef PREFS_HAS_XML
	, PREFS_XML				//< XML-based format
#endif
#ifdef PREFS_HAS_NULL
	, PREFS_NULL	///< Does not store settings
#endif
};

/**
  * Interface for preference files used by the PrefsManager. This class
  * does not by itself call anything platform-dependent, but defers that
  * to the PrefsAccessor class and its children. There should be no need
  * to make any changes to this file to introduce new file formats.
  */
class PrefsFile
{
public:

#ifdef PREFSFILE_CASCADE
	/**
	 * A basic constructor for the PrefsFile class
	 *
	 * @param numglobal Number of global (defaults) files in cascade.
	 * @param numfixed Number of fixed (non-overridable) files in cascade.
	 */
	explicit PrefsFile(PrefsType, int numglobal = 1, int numfixed = 1);
#else
	/**
	 * A basic constructor for the PrefsFile class
	 */
	explicit PrefsFile(PrefsType);
#endif

#ifdef PREFSFILE_EXT_ACCESSOR
# ifdef PREFSFILE_CASCADE
	/**
	 * Constructor for using an externally-defined accessor.
	 * Ownership of the accessor object is transferred to the PrefsFile
	 * object.
	 *
	 * @param numglobal Number of global (defaults) files in cascade.
	 * @param numfixed Number of fixed (non-overridable) files in cascade.
	 */
	explicit PrefsFile(PrefsAccessor *, int numglobal = 1, int numfixed = 1);
# else
	/**
	 * Constructor for using an externally-defined accessor.
	 * Ownership of the accessor object is transferred to the PrefsFile
	 * object.
	 */
	explicit PrefsFile(PrefsAccessor *);
# endif
#endif

	/**
	 * Second phase constructor for the PrefsFile class.
	 */
	void ConstructL();

	/**
	 * The destructor for the class, any changes not written to disk
	 * by calling CommitL() will be lost.
	 */
	virtual ~PrefsFile();

# ifdef PREFSFILE_RESET
	/**
	 * Reset all memory data to initial values.
	 * This is typically used when the calling code needs to abort
	 * a PrefsFile operation without having to call CommitL().
	 */
	virtual void Reset();
# endif
	/**
	 * Loads all the values from the specified files. Initially loads
	 * values from the Global file and then follows up by reading values
	 * from the user file in order to override the global options. This
	 * must be called before any Read operations are performed.
	 *
	 * File not found errors are returned, leave is only performed on
	 * fatal (out of memory) errors. Only file not found errors concerning
	 * the local settings file are returned, other are ignored.
	 *
	 * @return Status of the operation.
	 */
	virtual OP_STATUS LoadAllL();

	/**
	 * Flush all values loaded into RAM using the LoadAllL function. Local
	 * changes made by the write functions are not discarded, however. To
	 * access the data again, a LoadAllL must be performed.
	 */
	virtual void Flush();

	/**
	 * Commits all the changes made by the write functions to the user
	 * preferences file. If a flush has occurred since the last commit or
	 * since the LoadAll(), reread all the values from the global and user
	 * files and then flush. No values will be written to the user
	 * preferences file unless there has been changes to make it different
	 * than the global file.
	 *
	 * @param force If force is false, write-expensive systems can ignore it.
	 * @param flush Whether or not to flush internal buffers after the commit.
	 */
	virtual void CommitL(BOOL force = FALSE, BOOL flush = TRUE);

	/**
	 * Set's the user preference file. This under Unix would be for example
	 * ~/.opera/operarc
	 *
	 * @param file The value to set for the user preferences file
	 */
	void SetFileL(const OpFileDescriptor *file);

	/**
	 * Returns the user preference file.
	 *
	 * @return The value previously set for the user preferences file.
	 *         Set using SetFileL().
	 */
	OpFileDescriptor *GetFile();

	/**
	 * Returns the user preference file.
	 *
	 * @return The value previously set for the user preferences file.
	 *         Set using SetFileL().
	 */
	const OpFileDescriptor *GetFile() const;

	/**
	 * Sets the user preference file to read only. Use this to disable
	 * any writing to the user preference file even if it normally is
	 * writable.
	 *
	 * The read-only attribute is reinitialized when SetFileL() is called.
	 */
	inline void SetReadOnly() { m_writable = FALSE; }

#ifdef PREFSFILE_CASCADE
	/**
	 * Sets a system global configuration file. This on Unix systems
	 * would be /etc/operarc
	 *
	 * @param file The new file to use for the Global File
	 * @param priority Priority (0-based) of the global file.
	 */
	void SetGlobalFileL(const OpFileDescriptor *file, int priority = 0);

# ifdef PREFSFILE_GETGLOBALFILE
	/**
	 * Returns the global preferences file.
	 *
	 * @param priority The priority (0-based) of the file to return.
	 * @return The value previously set for the Global file. There
	 *         usually will be a default other than NULL.
	 */
	OpFileDescriptor *GetGlobalFile(int priority = 0);
# endif

	/**
	 * Sets a system global fixed configuration file. This on Unix
	 * systems would be /etc/operarc.fixed
	 *
	 * @param file The new file to use for the fixed file
	 * @param priority Priority (0-based) of the fixed file.
	 */
	void SetGlobalFixedFileL(const OpFileDescriptor *file, int priority = 0);

# ifdef PREFSFILE_GETFIXEDFILE
	/**
	 * Returns the global fixed preferences file.
	 *
	 * @param priority The priority (0-based) of the file to return.
	 * @return The value previously set for the Global fixed file. There
	 *         usually will be a default other than NULL.
	 */
	OpFileDescriptor *GetGlobalFixedFile(int priority = 0);
# endif
#endif // PREFSFILE_CASCADE

#ifdef PREFSFILE_IMPORT
	/**
	 * Import settings from a different preference file. This PrefsFile object
	 * must only have a user file (no global or global fixed file). The
	 * pointer sent to this function will be taken over by the function, and
	 * will be deleted, even if the function call fails.
	 *
	 * You must first load data in the source file by calling LoadAllL() on
	 * it.
	 *
	 * The call will fail with an ERR_OUT_OF_RANGE error if the imported
	 * PrefsFile object has cached or deleted uncommitted settings.
	 *
	 * @param source Preference file to import.
	 */
	void ImportL(PrefsFile *source);
#endif

#ifdef PREFSFILE_WRITE
	/**
	 * Writes an integer value into the preferences file and leaves on out
	 * of memory. The write is cached until CommitL is called.
	 *
	 * @return OK on success, ERR_NO_ACCESS if value is read-only
	 * @param section The section of the preferences
	 * @param key     The key located within the section
	 * @param value   The value to write to the file.
	 */
	virtual OP_STATUS WriteIntL(const OpStringC &section, const OpStringC &key, int value);

	/** @overload */
	virtual OP_STATUS WriteIntL(const OpStringC8 &section, const OpStringC8 &key, int value);
#endif // PREFSFILE_WRITE

#ifdef PREFSFILE_WRITE_GLOBAL
	/**
	 * Writes an integer value into the global preferences file and leaves
	 * on out of memory. The write is cached until CommitL is called.
	 *
	 * @param section The section of the preferences.
	 * @param key	  The key located within the section.
	 * @param value   The value to write to the file.
	 * @param priority Priority of the global file to write to.
	 * @return OK on success, ERR_NO_ACCESS if value is read-only.
	 *         ERR_NULL_POINTER if there is no global file.
	 */
	virtual OP_STATUS WriteIntGlobalL(const OpStringC &section, const OpStringC &key, int value, int priority = 0);

	/** @overload */
	virtual OP_STATUS WriteIntGlobalL(const OpStringC8 &section, const OpStringC8 &key, int value, int priority = 0);
#endif // PREFSFILE_WRITE_GLOBAL

	/**
	 * Read an integer value from the preferences file. If the value can
	 * not be found in the preferences file, then return the default.
	 *
	 * @return The value found in the file, or the default value given if
	 *         it wasn't found.
	 * @param section The section of the preferences
	 * @param key     The key located within the section
	 * @param dfval   A default value to be returned if the specified key is
	 *                not found in the file.
	 */
	virtual int ReadIntL(const OpStringC &section, const OpStringC &key, int dfval = 0);

	/** @overload */
	virtual int ReadIntL(const OpStringC8 &section, const OpStringC8 &key, int dfval = 0);

	/**
	 * Read a boolean value from the preferences file. If the value can
	 * not be found in the preferences file, then return the default. Any
	 * numeric non-zero value is considered true.
	 *
	 * @return The value found in the file, or the default value given if
	 *         it wasn't found.
	 * @param section The section of the preferences
	 * @param key     The key located within the section
	 * @param dfval   A default value to be returned if the specified key is
	 *                not found in the file.
	 */
	inline BOOL ReadBoolL(const OpStringC &section, const OpStringC &key, BOOL dfval = FALSE)
	{
		return ReadIntL(section, key, dfval) != 0;
	}

	/** @overload */
	inline BOOL ReadBoolL(const OpStringC8 &section, const OpStringC8 &key, BOOL dfval = FALSE)
	{
		return ReadIntL(section, key, dfval) != 0;
	}

#ifdef PREFSFILE_WRITE
	/**
	 * Write a string value to the preferences file and leaves on out of
	 * memory. The write is cached until CommitL is called.
	 *
	 * @return OK on success, ERR_NO_ACCESS if read-only
	 * @param section The section of the preferences
	 * @param key     The key located within the section
	 * @param value   The value to write to the file.
	 */
	virtual OP_STATUS WriteStringL(const OpStringC &section, const OpStringC &key,
	                       const OpStringC &value);

	/** @overload */
	virtual OP_STATUS WriteStringL(const OpStringC8 &section, const OpStringC8 &key,
	                       const OpStringC &value);

#endif // PREFSFILE_WRITE

#ifdef PREFSFILE_WRITE_GLOBAL
	/**
	 * Write a string value to the global preferences file and leaves on
	 * out of memory. The write is cached until CommitL is called.
	 *
	 * @param section The section of the preferences.
	 * @param key	  The key located within the section.
	 * @param value   The value to write to the file.
	 * @param priority Priority of the global file to write to.
	 * @return OK on success, ERR_NO_ACCESS if read-only.
	 *         ERR_NULL_POINTER if there is no global file.
	 */
	virtual OP_STATUS WriteStringGlobalL(const OpStringC &section, const OpStringC &key,
						   const OpStringC &value, int priority = 0);

	/** @overload */
	virtual OP_STATUS WriteStringGlobalL(const OpStringC8 &section, const OpStringC8 &key,
	                       const OpStringC &value, int priority = 0);
#endif // PREFSFILE_WRITE_GLOBAL

	/**
	 * Read a string from the preference file. If the key can not be found
	 * the default value is returned. This function will leave if an
	 * out-of-memory condition occurs.
	 *
	 * @return        The resultant string.
	 * @param section The section of the preferences
	 * @param key     The key located within the section
	 * @param dfval   A default value to be returned if the specified key
	 *                is not found in the file.
	 */
	virtual OpStringC ReadStringL(const OpStringC &section, const OpStringC &key,
	                      const OpStringC &dfval=NULL);

	/** @overload */
	virtual OpStringC ReadStringL(const OpStringC8 &section, const OpStringC8 &key,
	                      const OpStringC &dfval=NULL);

	/**
	 * Read a string from the preference file. If the key can not be found
	 * the default value is returned. This function will leave if an
	 * out-of-memory condition occurs.
	 *
	 * @param section The section of the preferences
	 * @param key     The key located within the section
	 * @param result  A string to hold the result.
	 * @param dfval   A default value to be returned if the specified key
	 *                is not found in the file.
	 */
	virtual void ReadStringL(const OpStringC &section, const OpStringC &key,
	                 OpString &result, const OpStringC &dfval=NULL);

	/** @overload */
	virtual void ReadStringL(const OpStringC8 &section, const OpStringC8 &key,
	                 OpString &result, const OpStringC &dfval=NULL);

	/**
	 * Delete a key from the local preference file.
	 *
	 * @param section The section of the preferences.
	 * @param key     The key to be deleted within the section.
	 * @return TRUE if a key was deleted.
	 */
	virtual BOOL DeleteKeyL(const uni_char *section, const uni_char *key);

	/** @overload */
	virtual BOOL DeleteKeyL(const char *section, const char *key);

#ifdef PREFSFILE_WRITE_GLOBAL
	/**
	 * Delete a key from the global preference file.
	 *
	 * @param section The section of the preferences.
	 * @param key	  The key to be deleted within the section.
	 * @param priority Priority of the global file to write to.
	 * @return TRUE if key was deleted.
	 */
	virtual BOOL DeleteKeyGlobalL(const uni_char *section, const uni_char *key, int priority = 0);
#endif

	/**
	 * Delete a section from the local preference file.
	 *
	 * @param section The section of the preferences to be deleted.
	 * @return TRUE if any keys were deleted.
	 */
	virtual BOOL DeleteSectionL(const uni_char *section);

	/** @overload */
	virtual BOOL DeleteSectionL(const char *section8);

#ifdef PREFSFILE_RENAME_SECTION
	/**
	 * Rename a section in the local preference file.
	 *
	 * @param old_section_name The old section name
	 * @param new_section_name The new section name
	 * @return TRUE if the section was renamed
	 */
	virtual BOOL RenameSectionL(const uni_char *old_section_name, const uni_char *new_section_name);

	/** @overload */
	virtual BOOL RenameSectionL(const char *old_section_name8, const char *new_section_name8);

#endif // PREFSFILE_RENAME_SECTION

#ifdef PREFSFILE_WRITE_GLOBAL
	/**
	 * Delete a section from the global preference file.
	 *
	 * @param section The section of the preferences to be deleted.
	 * @param priority Priority of the global file to write to.
	 * @return TRUE if any keys were deleted.
	 */
	virtual BOOL DeleteSectionGlobalL(const uni_char *section, int priority = 0);
#endif

	/**
	 * Clear a section from the local preference file. For file formats
	 * with linear access, this should keep the section in place so that
	 * it can be rewritten with new data. May be implemented as a call
	 * to DeleteSectionL().
	 *
	 * @param section The section of the preferences to be cleared
	 * @return TRUE if any keys were cleread
	 */
	virtual BOOL ClearSectionL(const uni_char *section);
	virtual BOOL ClearSectionL(const char *section8);

#ifdef PREFS_DELETE_ALL_SECTIONS
	/**
	 * Clear the preferences file. All local changes will be discarded and
	 * the contents of the on-disk file will be ignored. The actual file
	 * will not be updated until CommitL() is called.
	 */
	virtual void DeleteAllSectionsL();
#endif

#ifdef PREFS_READ_ALL_SECTIONS
	/**
	 * Retrieve a list of all sections in the preferences file.
	 *
	 * Data must have been loaded using LoadAllL() before calling
	 * this function.
	 *
	 * @param output The list to return.
	 */
	virtual void ReadAllSectionsL(OpString_list &output);
#endif

	/**
	 Read an entire section from the preferences file. The returned
	 * pointer is a copy of the contents of the section, and must be
	 * deleted by the caller. If the section does not exist, or is
	 * empty, an empty PrefsSection will be returned.
	 *
	 * Data must have been loaded using LoadAllL before calling
	 * this function.
	 *
	 * @param section The section of the preferences
	 * @return Pointer to a copy of the section. NULL if section does not
	 *         exist.
	 */
	PrefsSection *ReadSectionL(const OpStringC &section);
	/** @overload */
	PrefsSection *ReadSectionL(const OpStringC8 &section8);

	/**
	 * Check whether there is any reference to a given section.
	 *
	 * @param section The section to check for.
	 * @return TRUE if the section exists.
	 */
	virtual BOOL IsSection(const uni_char *section);
	virtual BOOL IsSection(const char *section8);

#ifdef PREFSFILE_WRITE_GLOBAL
	/**
	 * Check whether there is any reference to a given section in the
	 * preferences given by the user.
	 *
	 * @param section The section to check for.
	 * @return TRUE if the section exists in the preferences given by the
	 *         user. FALSE if it doesn't, or is only set globally.
	 */
	virtual BOOL IsSectionLocal(const uni_char *section);
#endif

	/**
	 * Check whether the key exists anywhere.
	 *
	 * @param section The section the key is in.
	 * @param key The key to check for in the section.
	 * @return TRUE if the key exists (no matter what its value is).
	 */
	virtual BOOL IsKey(const uni_char *section, const uni_char *key);

	/** @overload */
	virtual BOOL IsKey(const char *section, const char *key);

#ifdef PREFSFILE_WRITE_GLOBAL
	/**
	 * Check whether the key exists in the preferences given by the user.
	 *
	 * @param section The section the key is in.
	 * @param key The key to check for in the section.
	 * @return TRUE if the key exists in the preferences given by the user.
	 *         FALSE if it doesn't, or is only set globally.
	 */
	virtual BOOL IsKeyLocal(const uni_char *section, const uni_char *key);

	/** @overload */
	virtual BOOL IsKeyLocal(const char *section, const char *key);
#endif

	/***
	 * Check whether user is allowed to change preference.
	 *
	 * @param section The section the key is in
	 * @param key The key to check for in the section
	 * @return TRUE if the user is allowed to change the value
	 */
#ifdef PREFSFILE_CASCADE
	virtual BOOL AllowedToChangeL(const OpStringC &section, const OpStringC &key);

	/** @overload */
	virtual BOOL AllowedToChangeL(const OpStringC8 &section, const OpStringC8 &key);
#else
	virtual inline BOOL AllowedToChangeL(const OpStringC &, const OpStringC &)
	{ return TRUE; }
	virtual inline BOOL AllowedToChangeL(const OpStringC8 &, const OpStringC8 &)
	{ return TRUE; }
#endif

	/***
	 * Check whether user is allowed to change all the preferences in a
	 * section.
	 *
	 * @param section The section the key is in
	 * @param key The key to check for in the section
	 * @return TRUE if the user is allowed to change all the values in this
	 *         section.
	 */
#ifdef PREFSFILE_CASCADE
	virtual BOOL AllowedToChangeL(const OpStringC &section);
#else
	virtual inline BOOL AllowedToChangeL(const OpStringC &) { return TRUE; }
#endif

	/**
	 * This API is only to be used inside the preferences code. Outside code
	 * must use ReadSectionL().
	 */
	virtual PrefsSectionInternal *ReadSectionInternalL(const OpStringC &section);

protected:

	/**
	 * A reference to the accessor that does the loading
	 * and storing to/from the file
	 */
	PrefsAccessor *m_prefsAccessor;

	/** Cache of non-committed local changes. */
	PrefsMap *m_cacheMap;

	/** Settings from the local preference file. */
	PrefsMap *m_localMap;

#ifdef PREFSFILE_CASCADE
	/** Settings from the global preference file. */
	PrefsMap *m_globalMaps;

# ifdef PREFSFILE_WRITE_GLOBAL
	/** Cache of non-comitted global changes. */
	PrefsMap *m_globalCacheMaps;
# endif

	/** Settings from the (global) fixed preference file. */
	PrefsMap *m_fixedMaps;
#endif // PREFSFILE_CASCADE

	/** The local file. */
	OpFileDescriptor *m_file;

#ifdef PREFSFILE_CASCADE
	/** Number of global files. */
	int m_numglobalfiles;

	/** The global files. */
	OpFileDescriptor **m_globalfiles;

	/** Number of (global) fixed files. */
	int m_numfixedfiles;

	/** The (global) fixed files. */
	OpFileDescriptor **m_fixedfiles;
#endif // PREFSFILE_CASCADE

	/** Flag set if there are deleted keys. */
	BOOL m_hasdeletedkeys;

#if defined PREFSFILE_CASCADE && defined PREFSFILE_WRITE_GLOBAL
	/** Flags set if there are deleted keys in the global maps. */
	BOOL *m_hasdeletedglobalkeys;
#endif

	/** Flag set if the local file is writable. */
	BOOL m_writable;

	/**
	 * Loads preference data from the specific file to
	 * the provided map.
	 * @param file The file to load from
	 * @param map the map to load into
	 * @return Status of the operation
	 */
	OP_STATUS LoadFileL(OpFileDescriptor *file, PrefsMap *map);

	/**
	 * Load preferences from the local file.
	 * @return Status of the operation
	 */
	OP_STATUS LoadLocalL();

#ifdef PREFSFILE_CASCADE
	/**
	 * Load preferences from a global file.
	 * @param num The global file to load.
	 */
	void LoadGlobalL(int num);

	/**
	 * Load preferences from a (global) fixed file.
	 * @param num The (global) fixed file to load.
	 */
	void LoadFixedL(int num);
#endif // PREFSFILE_CASCADE

	/**
	 * Read a value (internal).
	 * @param section The section that contains the key
	 * @param key The requested key
	 * @param value Pointer that will be set to the string, if found
	 * @return Status of the operation
	 */
	OP_STATUS ReadValueL(const uni_char *section, const uni_char *key, const uni_char **value);

	/** Flag for if local file has been loaded. */
	BOOL m_localloaded;
#ifdef PREFSFILE_CASCADE
	/** Flags for if global files has been loaded. */
	BOOL *m_globalsloaded;
	/** Flags for if (global) fixed files has been loaded. */
	BOOL *m_fixedsloaded;
#endif

	/** Type of preference file being used. */
	PrefsType m_type;

	/** Used to store temporary strings when converting from ASCII to UTF-16. */
	TempBuffer m_tmp_buf;

	/** Create temporary UTF-16 strings for ASCII section and key parameters. */
	OpStringC GetTempStringsL(const OpStringC8 &sec, const OpStringC8 &key, const uni_char*& tmp_key);

	/** Create temporary UTF-16 string for ASCII section parameter. */
	OpStringC GetTempStringL(const char *sec);

	/** Helper for ReadAllSectionsL(). */
	unsigned int ReadAllSectionsHelperL(PrefsMap *, OpGenericStringHashTable *);
};

#endif // PREFSFILE_H && PREFS_HAS_PREFSFILE
