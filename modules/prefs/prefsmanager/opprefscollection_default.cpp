/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright 2002-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

/**
 * @file opprefscollection_default.cpp
 * Implementation of OpPrefsCollectionDefault.
 *
 * This file contains an implementation of all the non-abstract parts of the
 * OpPrefsCollectionDefault interface.
 */
#include "core/pch.h"

#include "modules/prefs/prefsmanager/opprefscollection_default.h"

OpPrefsCollectionDefault::~OpPrefsCollectionDefault()
{
}

#ifdef PREFS_HOSTOVERRIDE
void OpPrefsCollectionDefault::SetOverrideReader(PrefsFile *reader)
{
	/* nothing */
}

HostOverrideStatus OpPrefsCollectionDefault::IsHostOverridden(const uni_char *, BOOL)
{
	return HostNotOverridden;
}

BOOL OpPrefsCollectionDefault::SetHostOverrideActive(const uni_char *, BOOL)
{
	return FALSE;
}

void OpPrefsCollectionDefault::ReadOverridesL(const uni_char *, PrefsSection *, BOOL, BOOL)
{
	/* nothing */
}

# ifdef PREFS_WRITE
void OpPrefsCollectionDefault::RemoveOverridesL(const uni_char *)
{
	/* nothing */
}

#  ifdef PREFS_HAVE_STRING_API
BOOL OpPrefsCollectionDefault::OverridePreferenceL(const uni_char *, IniSection, const char *, const OpStringC &, BOOL)
{
	return FALSE;
}

BOOL OpPrefsCollectionDefault::RemoveOverrideL(const uni_char *, IniSection, const char *, BOOL)
{
	return FALSE;
}
#  endif // PREFS_HAVE_STRING_API
# endif // PREFS_WRITE
#endif // PREFS_HOSTOVERRIDE

#ifdef PREFS_HAVE_STRING_API
BOOL OpPrefsCollectionDefault::GetPreferenceInternalL(IniSection section, const char *key, OpString &target,
	BOOL defval,
	const struct stringprefdefault *strings, int numstrings,
	const struct integerprefdefault *integers, int numintegers)
{
	int i = GetStringPrefIndex(section, key, strings, numstrings);
	if (i >= 0)
	{
#ifdef PREFS_COVERAGE
		++ m_stringprefused[i];
#endif

		if (defval)
			target.SetL(GetDefaultStringInternal(i, &strings[i]));
		else
			target.SetL(m_stringprefs[i]);
		return TRUE;
	}

	int j = GetIntegerPrefIndex(section, key, integers, numintegers);
	if (j >= 0)
	{
#ifdef PREFS_COVERAGE
		++ m_intprefused[j];
#endif
		// We need log(256)/log(10) * sizeof(value) characters to store
		// the number, adding one for the rounding, one for the sign
		// and one for the terminator. 53/22 is a good approximation
		// of this.
		static const int buflen = 3 + sizeof (int) * 53 / 22;
		char buf[buflen]; /* ARRAY OK 2009-04-23 adame */
		op_itoa(defval ? GetDefaultIntegerInternal(j, &integers[j]) : m_intprefs[j], buf, 10);
		target.SetL(buf);
		return TRUE;
	}

	return FALSE;
}
#endif // PREFS_HAVE_STRING_API

int OpPrefsCollectionDefault::GetIntegerPref(int which) const
{
#ifdef PREFS_COVERAGE
	++ m_intprefused[which];
#endif

	return m_intprefs[which];
}

const OpStringC OpPrefsCollectionDefault::GetStringPref(int which) const
{
#ifdef PREFS_COVERAGE
	++ m_stringprefused[which];
#endif

	return m_stringprefs[which];
}
