/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#include "core/pch.h"

#if defined PREFS_HAS_LNG

#include "modules/prefsfile/accessors/lng.h"
#include "modules/util/opfile/unistream.h"

OP_STATUS LangAccessor::LoadL(OpFileDescriptor *file, PrefsMap *map)
{
	// Clear section cache
	m_current_section = NULL;

	OpStackAutoPtr<UnicodeFileInputStream> languagefile(OP_NEW_L(UnicodeFileInputStream, ()));

	OP_STATUS rc =
		languagefile->Construct(file, UnicodeFileInputStream::LANGUAGE_FILE);
	if (OpStatus::IsError(rc))
	{
		if (OpStatus::ERR_NO_MEMORY == rc) LEAVE(rc);
		return rc;
	}

	LoadStreamL(languagefile.get(), map, FALSE);
	return OpStatus::OK;
}

#ifdef UPGRADE_SUPPORT
/*static*/ BOOL LangAccessor::IsRecognizedLanguageFile(OpFileDescriptor *file)
{
	if (!file)
	{
		return FALSE;
	}

	// Check the header line. Compatible language files start with a line
	// saying "; Opera language file version 2.0". This may, or may not,
	// be prepended by a BOM.
	if (OpStatus::IsError(file->Open(OPFILE_READ)))
	{
		return FALSE;
	}

	BOOL valid = TRUE;

	OpString s;
	OP_STATUS rc = file->ReadUTF8Line(s);
	if (OpStatus::IsSuccess(rc))
	{
		uni_char *p = s.CStr();
		if (0xFEFF == *p)
		{
			// Point past the BOM (which has been converted from its
			// UTF-8 representation here)
			++ p;
		}
#ifdef ENCODINGS_HAVE_UTF7
		else if (s.Compare("+/v8-", 5) == 0)
		{
			// Point past a UTF-7 BOM.
			p += 5;
		}
#endif

		if (!uni_strni_eq(p, "; OPERA LANGUAGE FILE VERSION 2.0", 33))
		{
			valid = FALSE;
		}

#if PF_LNG_CUTOFF > 0
		// Find the database version number
		BOOL has_db_version = FALSE;

		while (valid && !has_db_version && OpStatus::IsSuccess(rc))
		{
			rc = file->ReadUTF8Line(s);
			if (OpStatus::IsSuccess(rc))
			{
				if (uni_strni_eq(s.CStr(), "DB.VERSION=", 11))
				{
					uni_char *dbverstr = s.CStr() + 11;
					if ('"' == *dbverstr)
					{
						++ dbverstr;
					}

					long int dbversion = uni_strtol(dbverstr, NULL, 10);
					has_db_version = TRUE;
					valid = dbversion >= PF_LNG_CUTOFF;
				}
			}
			if (s.IsEmpty() && file->Eof())
			{
				break;
			}
		}
#endif
	}

	OpStatus::Ignore(file->Close());

	return valid;
}
#endif

#endif // PREFS_HAS_LNG
