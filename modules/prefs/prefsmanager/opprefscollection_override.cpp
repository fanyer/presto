/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright 2002-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

/**
 * @file opprefscollection_override.cpp
 * Implementation of OpPrefsCollectionWithHostOverride.
 *
 * This file contains an implementation of all the non-abstract parts of the
 * OpPrefsCollectionWithHostOverride interface.
 */
#include "core/pch.h"

#include "modules/prefs/prefsmanager/opprefscollection_override.h"
#include "modules/prefs/prefsmanager/opprefslistener.h"
#include "modules/prefsfile/prefssection.h"
#include "modules/prefsfile/prefsentry.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/prefs/prefsmanager/hostoverride.h"

#include "modules/url/url2.h"

#ifdef PREFS_HOSTOVERRIDE
OpOverrideHost *OpPrefsCollectionWithHostOverride::CreateOverrideHostObjectL(const uni_char *host, BOOL from_user)
{
	OpStackAutoPtr<OpOverrideHost> newobj(OP_NEW_L(OverrideHost, ()));
	newobj->ConstructL(host, from_user);
# ifdef PREFS_WRITE
	if (g_prefsManager->IsInitialized())
	{
		g_prefsManager->OverrideHostAddedL(host);
	}
# endif
	return newobj.release();
}
#endif // PREFS_HOSTOVERRIDE

#if defined PREFS_HOSTOVERRIDE && defined PREFS_WRITE
void OpPrefsCollectionWithHostOverride::RemoveOverridesL(const uni_char *host)
{
	OpOverrideHostContainer::RemoveOverridesL(host);
}
#endif // PREFS_HOSTOVERRIDE && PREFS_WRITE

#ifdef PREFS_HOSTOVERRIDE
void OpPrefsCollectionWithHostOverride::SetOverrideReader(PrefsFile *reader)
{
	m_overridesreader = reader;
}

HostOverrideStatus OpPrefsCollectionWithHostOverride::IsHostOverridden(const uni_char *host, BOOL exact)
{
	OpOverrideHost *p = FindOverrideHost(host, exact, FALSE);
	if (p)
	{
#ifdef PREFSFILE_WRITE_GLOBAL
		BOOL from_user = p->IsFromUser();

		if (p->IsActive())
		{
			return from_user
				? HostOverrideActive
				: HostOverrideDownloadedActive;
		}
		else
		{
			return from_user
				? HostOverrideDisabled
				: HostOverrideDownloadedDisabled;
		}
#else
		return p->IsActive()
			? HostOverrideActive
			: HostOverrideDisabled;
#endif
	}

	return HostNotOverridden;
}

size_t OpPrefsCollectionWithHostOverride::HostOverrideCount(const uni_char *host_name) const
{
	if (OpOverrideHost* host = FindOverrideHost(host_name, TRUE, FALSE))
		return host->OverrideCount();

	return 0;
}
#endif // PREFS_HOSTOVERRIDE

#ifdef PREFS_HOSTOVERRIDE
BOOL OpPrefsCollectionWithHostOverride::IsIntegerOverridden(int which, const uni_char *host) const
{
	BOOL is_overridden;
	GetIntegerPref(which, host, &is_overridden);
	return is_overridden;
}

BOOL OpPrefsCollectionWithHostOverride::IsIntegerOverridden(int which, const URL &host) const
{
	return IsIntegerOverridden(which, host.GetAttribute(URL::KUniHostName).CStr());
}
#endif // PREFS_HOSTOVERRIDE

#ifdef PREFS_HOSTOVERRIDE
BOOL OpPrefsCollectionWithHostOverride::IsStringOverridden(int which, const uni_char *host) const
{
	BOOL is_overridden;
	GetStringPref(which, host, &is_overridden);
	return is_overridden;
}

BOOL OpPrefsCollectionWithHostOverride::IsStringOverridden(int which, const URL &host) const
{
	return IsStringOverridden(which, host.GetAttribute(URL::KUniHostName).CStr());
}
#endif // PREFS_HOSTOVERRIDE

#ifdef PREFS_HOSTOVERRIDE
BOOL OpPrefsCollectionWithHostOverride::SetHostOverrideActive(const uni_char *host, BOOL active)
{
	OpOverrideHost *p = FindOverrideHost(host, TRUE, FALSE);
	if (p)
	{
		p->SetActive(active);
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}
#endif // PREFS_HOSTOVERRIDE

int OpPrefsCollectionWithHostOverride::GetIntegerPref(
	int which, const URL &host, BOOL *overridden) const
{
	return GetIntegerPref(which,  host.GetAttribute(URL::KUniHostName).CStr(), overridden);
}

int OpPrefsCollectionWithHostOverride::GetIntegerPref(
	int which, const uni_char *host, BOOL *overridden) const
{
	return GetIntegerPref(which, host, FALSE, overridden);
}

int OpPrefsCollectionWithHostOverride::GetIntegerPref(
	int which, const uni_char *host, BOOL exact_match, BOOL *overridden) const
{
#ifdef PREFS_COVERAGE
	++ m_intprefused[which];
#endif

#ifdef PREFS_HOSTOVERRIDE
	if (host && *host)
	{
		OverrideHost *override =
			static_cast<OverrideHost *>(FindOverrideHost(host, exact_match));

		while (override)
		{
			// Found a matching override
			int retval;
			if (override->GetOverridden(which, retval))
			{
				// Value is overridden
				if (overridden)
					*overridden = TRUE;

				return retval;
			}
			else if (override->Suc())
			{
				// Search parent domains
				override = static_cast<OverrideHost *>
					(FindOverrideHost(host, FALSE, TRUE, static_cast<OverrideHost *>(override->Suc())));
			}
			else
			{
				override = NULL;
			}
		}
	}
#endif // PREFS_HOSTOVERRIDE

	// No override set, or no host specified; return standard value
	if (overridden)
		*overridden = FALSE;

	return m_intprefs[which];
}

const OpStringC OpPrefsCollectionWithHostOverride::GetStringPref(int which, const URL &host, BOOL *overridden) const
{
	OpStringC res(GetStringPref(which,  host.GetAttribute(URL::KUniHostName).CStr(), overridden));
	return res; // ADS 1.2 crashes if you don't do this via a local variable :-(
}

const OpStringC OpPrefsCollectionWithHostOverride::GetStringPref(
	int which, const uni_char *host, BOOL *overridden) const
{
#ifdef PREFS_COVERAGE
	++ m_stringprefused[which];
#endif

#ifdef PREFS_HOSTOVERRIDE
	if (host)
	{
		OverrideHost *override =
			static_cast<OverrideHost *>(FindOverrideHost(host));

		while (override)
		{
			// Found a matching override
			const OpStringC *retval;
			if (override->GetOverridden(which, &retval))
			{
				// Value is overridden
				if (overridden)
					*overridden = TRUE;

				return *retval;
			}
			else if (override->Suc())
			{
				// Search parent domains
				override = static_cast<OverrideHost *>
					(FindOverrideHost(host, FALSE, TRUE, static_cast<OverrideHost *>(override->Suc())));
			}
			else
			{
				override = NULL;
			}
		}
	}
#endif // PREFS_HOSTOVERRIDE

	// No override set, or no host specified; return standard value
	if (overridden)
		*overridden = FALSE;

	return m_stringprefs[which];
}

#if defined PREFS_HOSTOVERRIDE && defined PREFS_WRITE
OP_STATUS OpPrefsCollectionWithHostOverride::OverridePrefL(
	const uni_char *host, const struct integerprefdefault *pref,
	int which, int value, BOOL from_user)
{
#ifdef PREFS_COVERAGE
	++ m_intprefused[which];
#endif

	if (!host)
	{
		LEAVE(OpStatus::ERR_NULL_POINTER);
	}
	OverrideHost *override =
		static_cast<OverrideHost *>(FindOrCreateOverrideHostL(host, from_user));
# ifdef PREFS_VALIDATE
	CheckConditionsL(which, &value, host);
# endif
	OP_STATUS rc = override->WriteOverrideL(m_overridesreader, pref, which, value, from_user);
	if (OpStatus::IsSuccess(rc))
	{
# ifdef PREFSFILE_WRITE_GLOBAL
		if (from_user)
# endif
		{
			rc = m_overridesreader->WriteStringL(UNI_L("Overrides"), host, NULL);
		}
# ifdef PREFSFILE_WRITE_GLOBAL
		else
		{
			rc = m_overridesreader->WriteStringGlobalL(UNI_L("Overrides"), host, NULL);
		}
# endif
	}

	if (OpStatus::IsSuccess(rc))
	{
		BroadcastOverride(host);
	}
	return rc;
}
#endif // PREFS_HOSTOVERRIDE && PREFS_WRITE

#if defined PREFS_HOSTOVERRIDE && defined PREFS_WRITE
OP_STATUS OpPrefsCollectionWithHostOverride::OverridePrefL(
	const uni_char *host, const struct stringprefdefault *pref,
	int which, const OpStringC &value, BOOL from_user)
{
#ifdef PREFS_COVERAGE
	++ m_stringprefused[which];
#endif

	if (!host)
	{
		LEAVE(OpStatus::ERR_NULL_POINTER);
	}
	OverrideHost *override =
		static_cast<OverrideHost *>(FindOrCreateOverrideHostL(host, from_user));
	OP_STATUS rc;
# ifdef PREFS_VALIDATE
	OpString *newvalue;
	if (CheckConditionsL(which, value, &newvalue, host))
	{
		OpStackAutoPtr<OpString> anchor(newvalue);
		rc = override->WriteOverrideL(m_overridesreader, pref, which, *newvalue, from_user);
		anchor.release();
		OP_DELETE(newvalue);
	}
	else
# endif // PREFS_VALIDATE
	{
		rc = override->WriteOverrideL(m_overridesreader, pref, which, value, from_user);
	}
	if (OpStatus::IsSuccess(rc))
	{
# ifdef PREFSFILE_WRITE_GLOBAL
		if (from_user)
# endif
		{
			rc = m_overridesreader->WriteStringL(UNI_L("Overrides"), host, NULL);
		}
# ifdef PREFSFILE_WRITE_GLOBAL
		else
		{
			rc = m_overridesreader->WriteStringGlobalL(UNI_L("Overrides"), host, NULL);
		}
# endif
	}

	if (OpStatus::IsSuccess(rc))
	{
		BroadcastOverride(host);
	}
	return rc;
}
#endif // PREFS_HOSTOVERRIDE && PREFS_WRITE

#if defined PREFS_HOSTOVERRIDE && defined PREFS_WRITE
BOOL OpPrefsCollectionWithHostOverride::RemoveIntegerOverrideL(const uni_char *host,
															   const integerprefdefault *pref,
															   int which,
															   BOOL from_user)
{
	return OpOverrideHostContainer::DoRemoveIntegerOverrideL(m_overridesreader, host, pref, which, from_user);
}
#endif // PREFS_HOSTOVERRIDE && PREFS_WRITE

#if defined PREFS_HOSTOVERRIDE && defined PREFS_WRITE
BOOL OpPrefsCollectionWithHostOverride::RemoveStringOverrideL(const uni_char *host,
															  const stringprefdefault *pref,
															  int which,
															  BOOL from_user)
{
	return OpOverrideHostContainer::DoRemoveStringOverrideL(m_overridesreader, host, pref, which, from_user);
}
#endif // PREFS_HOSTOVERRIDE && PREFS_WRITE

#ifdef PREFS_HOSTOVERRIDE
void OpPrefsCollectionWithHostOverride::BroadcastOverride(const uni_char *host)
{
	ListenerContainer *p =
		static_cast<ListenerContainer *>(m_listeners.First());
	while (p)
	{
		OP_ASSERT(p->LinkId() != NULL);
		p->GetListener()->HostOverrideChanged(m_identity, host);

		p = static_cast<ListenerContainer *>(p->Suc());
	}
}
#endif // PREFS_HOSTOVERRIDE

#if defined PREFS_HAVE_STRING_API && defined PREFS_WRITE && defined PREFS_HOSTOVERRIDE
BOOL OpPrefsCollectionWithHostOverride::OverridePreferenceInternalL(
	const uni_char *host, IniSection section, const char *key,
	const OpStringC &value, BOOL from_user,
	const struct stringprefdefault *strings, int numstrings,
	const struct integerprefdefault *integers, int numintegers)
{
	int i = GetStringPrefIndex(section, key, strings, numstrings);
	if (i >= 0)
	{
		return OpStatus::IsSuccess(OverridePrefL(host, &strings[i], i, value, from_user));
	}

	int j = GetIntegerPrefIndex(section, key, integers, numintegers);
	if (j >= 0)
	{
		// Need to convert to an integer. API doc says we do a LEAVE if
		// "value" does not contain a proper integer.
		uni_char *endptr;
		if (value.IsEmpty())
		{
			LEAVE(OpStatus::ERR_OUT_OF_RANGE);
		}
		long intval = uni_strtol(value.CStr(), &endptr, 0);
		if (*endptr)
		{
			LEAVE(OpStatus::ERR_OUT_OF_RANGE);
		}

		return OpStatus::IsSuccess(OverridePrefL(host, &integers[j], j,
		                                         int(intval), from_user));
	}

	return FALSE;
}

BOOL OpPrefsCollectionWithHostOverride::RemoveOverrideInternalL(
	const uni_char *host, IniSection section, const char *key, BOOL from_user,
	const struct stringprefdefault *strings, int numstrings,
	const struct integerprefdefault *integers, int numintegers)
{
	int i = GetStringPrefIndex(section, key, strings, numstrings);
	if (i >= 0)
		return RemoveStringOverrideL(host, &strings[i], i, from_user);

	int j = GetIntegerPrefIndex(section, key, integers, numintegers);
	if (j >= 0)
		return RemoveIntegerOverrideL(host, &integers[j], j, from_user);

	return FALSE;
}
#endif // PREFS_HAVE_STRING_API && PREFS_WRITE && PREFS_HOSTOVERRIDE

#ifdef PREFS_HOSTOVERRIDE
void OpPrefsCollectionWithHostOverride::ReadOverridesInternalL(
	const uni_char *host, PrefsSection *section, BOOL active, BOOL from_user,
	const struct OpPrefsCollection::integerprefdefault *intprefs,
	const struct OpPrefsCollection::stringprefdefault *stringprefs)
{
	OverrideHost *override = NULL;

	// The entries for a host are stored in a section which has the
	// same name as the host itself.
	const PrefsEntry *overrideentry = section ? section->Entries() : NULL;
	while (overrideentry)
	{
		OpStringC key(overrideentry->Key());

		// The override is stored as Section|Key=Value, so we need to split
		// it up to lookup in the default values. We assume that no section
		// or key can be longer than 64 characters.
#define MAXLEN 64
		int sectionlen = key.FindFirstOf('|');
		int keylen = key.Length() - sectionlen - 1;
		if (sectionlen != KNotFound && sectionlen <= MAXLEN - 1 && keylen <= MAXLEN - 1)
		{
			char realsection[MAXLEN], realkey[MAXLEN]; /* ARRAY OK 2009-04-23 adame */
			uni_cstrlcpy(realsection, key.CStr(), sectionlen + 1);
			uni_cstrlcpy(realkey, key.CStr() + sectionlen + 1, MAXLEN);

			if (m_reader->AllowedToChangeL(realsection, realkey))
			{
				OpPrefsCollection::IniSection section_id =
					OpPrefsCollection::SectionStringToNumber(realsection);

				// Locate the preference and insert its value if we were allowed
				// to change it and it is ours.
				int idx;
				if ((idx = OverrideHost::GetPrefIndex(intprefs, section_id, realkey)) != -1)
				{
					int intvalue = uni_atoi(overrideentry->Value());
#ifdef PREFS_VALIDATE
					CheckConditionsL(idx, &intvalue, host);
#endif
					if (!override)
					{
						override =
							static_cast<OverrideHost *>
							(FindOrCreateOverrideHostL(host, from_user));
					}
					override->InsertOverrideL(idx, intvalue);
				}
				else if ((idx = OverrideHost::GetPrefIndex(stringprefs, section_id, realkey)) != -1)
				{
					if (!override)
					{
						override =
							static_cast<OverrideHost *>
							(FindOrCreateOverrideHostL(host, from_user));
					}

#ifdef PREFS_VALIDATE
					OpString *newvalue;
					if (CheckConditionsL(idx, overrideentry->Value(), &newvalue, host))
					{
						OpStackAutoPtr<OpString> anchor(newvalue);
						override->InsertOverrideL(idx, *newvalue);
						anchor.release();
						OP_DELETE(newvalue);
					}
					else
#endif // PREFS_VALIDATE
					{
						override->InsertOverrideL(idx, overrideentry->Value());
					}
				}
			}
		}

		overrideentry = overrideentry->Suc();
	}

	// Set flags
	if (override)
	{
		override->SetActive(active);
	}
}
#endif // PREFS_HOSTOVERRIDE

#ifdef PREFS_HAVE_STRING_API
BOOL OpPrefsCollectionWithHostOverride::GetPreferenceInternalL(IniSection section, const char *key, OpString &target,
	BOOL defval, const uni_char *host,
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
			target.SetL(GetStringPref(i, host));
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
		char buf[buflen]; /* ARRAY OK 2009-04-27 adame */
		op_itoa(defval ? GetDefaultIntegerInternal(j, &integers[j]) : GetIntegerPref(j, host), buf, 10);
		target.SetL(buf);
		return TRUE;
	}

	return FALSE;
}
#endif // PREFS_HAVE_STRING_API
