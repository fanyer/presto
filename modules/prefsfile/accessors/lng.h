/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Karlsson
*/

#if !defined ACCESSORS_LNG_H && defined PREFS_HAS_LNG
#define ACCESSORS_LNG_H

#include "modules/prefsfile/accessors/prefsaccessor.h"
#include "modules/prefsfile/accessors/ini.h"

/**
 * Accessor for 4.0+ language files. This class parses files
 * containing language strings in the 4.0+ language file format
 * and convert them according to their character encoding
 * setting.
 */
class LangAccessor : public IniAccessor
{
public:

	LangAccessor() : IniAccessor(TRUE) {};

	/**
	 * Loads and parses the content in the file and
	 * put the result in the map.
	 * @param file The file to load.
	 * @param map The map to put the result in.
	 */
	virtual OP_STATUS LoadL(OpFileDescriptor *file, PrefsMap *map);

#ifdef UPGRADE_SUPPORT
	/**
	 * Check for valid LNG file formats. Checks the header of the lng
	 * file to see if it is in a compatible format, and then checks
	 * the database version number to see if it has a compatible numbering
	 * scheme.
	 */
	static BOOL IsRecognizedLanguageFile(OpFileDescriptor *file);
#endif // UPGRADE_SUPPORT
};

#endif // !ACCESSORS_LNG_H && PREFS_HAS_LNG
