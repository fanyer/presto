/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#include "core/pch.h"

#if defined PREFS_HAS_LNGLIGHT

#include "modules/prefsfile/accessors/lnglight.h"

BOOL LangAccessorLight::ParseLineL(uni_char *line, PrefsMap *)
{
    const size_t platformlen = sizeof BUILDNUMSUFFIX - 1;

	if (!m_isinfoblock)
	{
		if (uni_strni_eq(line, "[INFO]", 6))
		{
			m_isinfoblock = TRUE;
		}
	}
	else
	{
		if (!m_hascode && uni_strni_eq(line, "LANGUAGE=", 9))
		{
			size_t namelen = uni_strlen(line);
			if ('"' == line[9] && '"' == line[namelen - 1])
			{
				OpStatus::Ignore(m_code.Set(line + 10, namelen - 11));
			}
			else
			{
				OpStatus::Ignore(m_code.Set(line + 9, namelen - 9));
			}

			m_hascode = TRUE;
		}
		else if (!m_hasname && uni_strni_eq(line, "LANGUAGENAME=", 13))
		{
			size_t namelen = uni_strlen(line);
			if ('"' == line[13] && '"' == line[namelen - 1])
			{
				OpStatus::Ignore(m_name.Set(line + 14, namelen - 15));
			}
			else
			{
				OpStatus::Ignore(m_name.Set(line + 13, namelen - 13));
			}

			m_hasname = TRUE;
		}
		else if (!m_hasbuild && uni_strni_eq(line, "BUILD." BUILDNUMSUFFIX "=", 6 + platformlen))
		{
			size_t namelen = uni_strlen(line);
			if ('"' == line[7 + platformlen] && '"' == line[namelen - 1])
			{
				OpStatus::Ignore(m_build.Set(line + 8 + platformlen, namelen - 9 - platformlen));
			}
			else
			{
				OpStatus::Ignore(m_build.Set(line + 7 + platformlen, namelen - 7 - platformlen));
			}

			m_hasbuild = TRUE;
		}
		else if (!m_hasdb && uni_strni_eq(line, "DB.VERSION=", 11))
		{
			size_t namelen = uni_strlen(line);
			if ('"' == line[11] && '"' == line[namelen - 1])
			{
				OpStatus::Ignore(m_db.Set(line + 12, namelen - 13));
			}
			else
			{
				OpStatus::Ignore(m_db.Set(line + 11, namelen - 11));
			}
			m_hasdb = TRUE;
		}

		else if ('[' == *line)
		{
			m_isinfoblock = FALSE;
		}
	}

	return m_hascode && m_hasname && m_hasbuild && m_hasdb;
}

OP_STATUS LangAccessorLight::GetFileInfo(OpString &code, OpString &name, OpString &build)
{
	OP_STATUS rc = code.Set(m_code);
	if (OpStatus::IsError(rc)) return rc;
	rc = name.Set(m_name);
	if (OpStatus::IsError(rc)) return rc;
	return build.Set(m_build);
}


OP_STATUS  LangAccessorLight::GetDatabaseVersion(OpString &db_version)
{
	return db_version.Set(m_db);
}


#endif // PREFS_HAS_LNGLIGHT
