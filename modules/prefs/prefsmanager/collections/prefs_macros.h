/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright 2005-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#ifndef PREFS_MACROS_H
#define PREFS_MACROS_H

// Remove the section and key values if we are doing a build that does not
// support reading an ini file.
// Remove the type flag if we are doing a build that does not support
// enumerating preferences.
// Do special initializing if we do not have support for compile-time complex
// structures.

#ifdef HAS_COMPLEX_GLOBALS
/** Internal helper for global initializers. */
# define INITGENERIC(collection, length, type, name) \
	const struct collection::type collection::name[length] =

/** Initialize list of string preference keys.
  * @param collection Class name for the collection object.
  * @param length Length of the enumeration (not including sentinel).
  */
# define INITSTRINGS(collection, length) \
	INITGENERIC(collection, length + 1, stringprefdefault,  m_stringprefdefault)

/** Initialize list of integer preference keys.
  * @param collection Class name for the collection object.
  * @param length Length of the enumeration (not including sentinel).
  */
# define INITINTS(collection, length) \
	INITGENERIC(collection, length + 1, integerprefdefault, m_integerprefdefault)

/** Initialize list of font preference keys.
  * @param collection Class name for the collection object.
  * @param length Length of the enumeration.
  */
# define INITFONTS(collection, length) \
	INITGENERIC(collection, length,     fontsections,       m_fontsettings)

/** Initialize list of color preference keys.
  * @param collection Class name for the collection object.
  * @param length Length of the enumeration.
  */
# define INITCOLORS(collection, length) \
	INITGENERIC(collection, length,     colorsettings,      m_colorsettings)

/** Initialize list of custom color preference keys.
  * @param collection Class name for the collection object.
  * @param length Length of the enumeration (not including sentinel).
  */
# define INITCUSTOMCOLORS(collection, length) \
	INITGENERIC(collection, length + 1, customcolorprefdefault, m_customcolorprefdefault)

/** Initialize list of file preference keys.
  * @param collection Class name for the collection object.
  * @param length Length of the enumeration (not including sentinel).
  */
# define INITFILES(collection, length) \
	INITGENERIC(collection, length + 1, fileprefdefault,    m_fileprefdefault)

/** Initialize list of folder preference keys.
  * @param collection Class name for the collection object.
  * @param length Length of the enumeration (not including sentinel).
  */
# define INITFOLDERS(collection, length) \
	const struct collection::directorykey collection::m_directorykeys[length + 1] =

/** Initialize list of section names.
  * @param length Length of the enumeration.
  */
# define INITSECTIONS(length) \
	const char * const OpPrefsCollection::m_sections[length] =

# define INITSTART		///< Mark start of initializer list.
# define INITEND		///< Mark end of initializer list.

# ifdef PREFS_ENUMERATE
/** Define a string preference key.
  * @param section INI file section for the setting (of type enum IniSection).
  * @param key     INI file key name for the setting.
  * @param value   Default value for the setting.
  */
#  define P(section, key, value)              { section, key, value }

/** Define a integer preference key.
  * @param section INI file section for the setting (of type enum IniSection).
  * @param key     INI file key name for the setting.
  * @param value   Default value for the setting.
  * @param type    Setting subtype (of type prefssetting::prefssettingtypes).
  */
#  define I(section, key, value, type)        { section, key, value, type }

/** Define a file preference key.
  * @param section INI file section for the setting (of type enum IniSection).
  * @param key     INI file key name for the setting.
  * @param folder  Default folder for the file (of type OpFileFolder).
  * @param value   Default file name for the setting.
  * @param use     Use the default file if the specified file is not found.
  */
#  define F(section, key, folder, value, use) { section, key, folder, value, use }

/** Define a folder preference key.
  * @param folderenum Folder identifier (of type OpFileFolder)
  * @param section    INI file section for the setting (of type enum IniSection).
  * @param key        INI file key name for the setting.
  */
#  define D(folderenum, section, key) { folderenum, section, key }

/** Define a font preference key.
  * @param sysenum Font identifier (of type OP_SYSTEM_FONT).
  * @param key     INI file key name for the setting.
  */
#  define S(sysenum, key)                     { sysenum, key }

/** Define a color preference key.
  * @param sysenum Color identifier (of type OP_SYSTEM_COLOR).
  * @param key     INI file key name for the setting.
  */
#  define C(sysenum, key)                     { sysenum, key }

/** Define a custom color preference key.
  * @param section INI file section for the setting (of type enum IniSection).
  * @param key     INI file key name for the setting.
  * @param value   Default value for the setting.
  */
#  define CC(section, key, value)             { section, key, value }
# elif defined PREFS_READ
#  define P(section, key, value)              { section, key, value }
#  define I(section, key, value, type)        { section, key, value }
#  define F(section, key, folder, value, use) { section, key, folder, value, use }
#  define D(folderenum, section, key)         { folderenum, section, key }
#  define S(sysenum, key)                     { sysenum, key }
#  define C(sysenum, key)                     { sysenum, key }
#  define CC(section, key, value)             { section, key, value }
# else
#  define P(section, key, value)              { value }
#  define I(section, key, value, type)        { value }
#  define F(section, key, folder, value, use) { folder, value }
#  define S(sysenum, key)                     { sysenum }
#  define C(sysenum, key)                     { sysenum }
#  define CC(section, key, value)             { value }
# endif

/** Define a section name.
  * @param name INI section name.
  */
# define K(name)	name

#else
# define INITSTRINGS(collection, length) \
	void collection::InitStrings()
# define INITINTS(collection, length) \
	void collection::InitInts()
# define INITFONTS(collection, length) \
	void collection::InitFonts()
# define INITCOLORS(collection, length) \
	void collection::InitColors()
# define INITFILES(collection, length) \
	void collection::InitFiles()
# define INITFOLDERS(collection, length) \
	void collection::InitFolders()
# define INITSECTIONS(length) \
	void OpPrefsCollection::InitSections()
# define INITCUSTOMCOLORS(collection, length) \
	void collection::InitCustomColors()

# define INITSTART \
	int i = 0;
# define INITEND ;

# ifdef PREFS_READ
#  define P(sectionv, keyv, value) \
	m_stringprefdefault[i].section  = sectionv, \
	m_stringprefdefault[i].key      = keyv,     \
	m_stringprefdefault[i ++].defval= value
#  define F(sectionv, keyv, folderv, value, use) \
	m_fileprefdefault[i].section = sectionv, \
	m_fileprefdefault[i].key     = keyv,     \
	m_fileprefdefault[i].folder  = folderv,  \
	m_fileprefdefault[i].filename= value,   \
	m_fileprefdefault[i ++].default_if_not_found = use
#  define D(folderenumv, sectionv, keyv) \
	m_directorykeys[i].folder  = folderenumv, \
	m_directorykeys[i].section = sectionv, \
	m_directorykeys[i ++].key  = keyv
#  define S(sysenum, key) \
	m_fontsettings[i].type = sysenum, \
	m_fontsettings[i ++].setting = key
#  define C(sysenum, key) \
	m_colorsettings[i].type = sysenum, \
	m_colorsettings[i ++].setting = key
#  define CC(sectionv, keyv, value) \
	m_customcolorprefdefault[i].section = sectionv, \
	m_customcolorprefdefault[i].key     = keyv, \
	m_customcolorprefdefault[i ++].defval  = value

#  ifdef PREFS_ENUMERATE
#   define I(sectionv, keyv, value, typev) \
	m_integerprefdefault[i].section = sectionv, \
	m_integerprefdefault[i].key     = keyv, \
	m_integerprefdefault[i].defval  = value, \
	m_integerprefdefault[i ++].type	= typev
#  else
#   define I(sectionv, keyv, value, typev) \
	m_integerprefdefault[i].section  = sectionv, \
	m_integerprefdefault[i].key      = keyv, \
	m_integerprefdefault[i ++].defval= value
#  endif

# else
#  define P(sectionv, keyv, value) \
	m_stringprefdefault[i ++].defval= value
#  define F(sectionv, keyv, folderv, value, use) \
	m_fileprefdefault[i].folder  = folderv,  \
	m_fileprefdefault[i ++].filename= value
#  define S(sysenum, key) \
	m_fontsettings[i ++].type = sysenum
#  define C(sysenum, key) \
	m_colorsettings[i ++].type = sysenum
#  define CC(sectionv, keyv, value) \
	m_customcolorprefdefault[i ++].defval = value
#  define I(sectionv, keyv, value, typev) \
	m_integerprefdefault[i ++].defval = value
# endif

# define K(name) \
	m_sections[i ++] = name
#endif

// Definitions for regular string arrays

#ifdef HAS_COMPLEX_GLOBALS
/** Define a string array.
  * @param T Character type.
  * @param N Length of array.
  * @param name Name of array.
  */
# define PREFSARRAY(T,N,name) static const T * const name[N] =
/** Define a string array entry.
  * @param name Name of array.
  * @param s String to add.
  */
# define PREFSARRAYENTRY(name,s) s
/** Sentinel that signals the end of a string array definition. */
# define PREFSARRAYSENTINEL
#else
# define PREFSARRAY(T,N,name) const T * name[N]; int name ## _init = 0;
# define PREFSARRAYENTRY(name,s) name[name ## _init ++] = s
# define PREFSARRAYSENTINEL ;
#endif


#endif // PREFS_MACROS_H
