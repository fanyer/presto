/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Karlsson
*/

#if !defined ACCESSORS_LNGLIGHT_H && defined PREFS_HAS_LNGLIGHT
#define ACCESSORS_LNGLIGHT_H

#include "modules/prefsfile/accessors/prefsaccessor.h"
#include "modules/prefsfile/accessors/lng.h"

/**
 * Simplified accessor for 4.0+ language files. This class is only
 * used from the UI to retrieve the language code and the name of
 * the language. It speeds the UI up by only storing those two values.
 */
class LangAccessorLight : public LangAccessor
{
public:
	LangAccessorLight()
		: LangAccessor(), m_hascode(FALSE), m_hasname(FALSE),
		  m_hasbuild(FALSE), m_hasdb(FALSE), m_isinfoblock(FALSE)
		{};

	/**
	 * Retrieve information about this language file.
	 * @param code ISO 639 language code (OUT).
	 * @param name Name of language in its own language (OUT).
	 * @param build Windows build number (OUT).
	 * @return Any error from OpString.
	 */
	OP_STATUS GetFileInfo(OpString &code, OpString &name, OpString &build);

	/**
	 * Retrieve the database version for this language file.
	 * @return Any error from OpString.
	 */
	OP_STATUS GetDatabaseVersion(OpString &db_version);

protected:
	virtual BOOL ParseLineL(uni_char *, PrefsMap *);

private:
	OpString	m_code, m_name, m_build, m_db;
	BOOL		m_hascode, m_hasname, m_hasbuild, m_hasdb, m_isinfoblock;
};

#endif // !ACCESSORS_LNGLIGHT_H && PREFS_HAS_LNGLIGHT
