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
 * @file hostoverride.cpp
 * Implementation of host based overrides.
 *
 * This file contains support for overriding singular preferences for specific
 * hosts.
 */
#include "core/pch.h"

#ifdef PREFS_HOSTOVERRIDE

#include "modules/prefsfile/prefssection.h"
#include "modules/prefsfile/prefsentry.h"
#include "modules/prefs/prefsmanager/opprefscollection.h"
#include "modules/prefs/prefsmanager/opprefscollection_override.h"
#include "modules/prefs/prefsmanager/hostoverride.h"
#include "modules/prefs/prefsmanager/prefsinternal.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/idna/idna.h"

// --- Implementation of OpOverrideHost -----------------------------------

void OpOverrideHost::ConstructL(const uni_char *host, BOOL from_user)
{
	// Decode host name if in IDNA form.
	if (uni_strstr(host, IDNA_PREFIX) != NULL)
	{
		char *asciitmp = static_cast<char *>(g_memory_manager->GetTempBuf2k());
		uni_cstrlcpy(asciitmp, host, g_memory_manager->GetTempBuf2kLen());
		size_t orig_host_len = op_strlen(asciitmp) + 1;
		m_host = OP_NEWA_L(uni_char, orig_host_len); // Never longer
		BOOL is_mailaddress = FALSE;
		IDNA::ConvertFromIDNA_L(asciitmp, m_host, orig_host_len, is_mailaddress);
		OP_ASSERT(!is_mailaddress); // Only operate on regular domain names
	}
	else
	{
		SetStrL(m_host, host);
	}
	m_host_len = uni_strlen(m_host);
	m_active = TRUE;
	SetFromUser(from_user);

	// If we have a trailing asterisk, we are matching the stem from the
	// left (substrings) instead of from the right (subdomains).
	m_match_left = ('*' == m_host[m_host_len - 1]);
	if (m_match_left)
	{
		m_host[-- m_host_len] = 0;
	}
}

OpOverrideHost::~OpOverrideHost()
{
	OP_DELETEA(m_host);
}

BOOL OpOverrideHost::IsHost(const uni_char *host, size_t hostlen, BOOL exact, BOOL active) const
{
	if (!m_active && active)
	{
		return FALSE;
	}

	if (exact)
	{
		return m_host_len == hostlen && uni_strcmp(m_host, host) == 0;
	}

	// The string we are looking for must be at least as long as
	// the stored domain name.
	if (m_host_len <= hostlen)
	{
		if (m_match_left)
		{
			// Match on substrings. "opera." will match "opera.com",
			// "opera.se" and "opera.nta.no", but not "myopera.com"
			// The final asterisk will have been removed by the
			// constructor
			return 0 == uni_strncmp(host, m_host, m_host_len);
		}
		else
		{
			// Match on subdomains. "opera.com" will match "opera.com",
			// "www.opera.com" and "my.opera.com", but not "myopera.com"
			return (m_host_len == hostlen ||
			        '.' == host[hostlen - m_host_len - 1])
			       && (0 == uni_strcmp(host + hostlen - m_host_len, m_host));
		}
	}

	return FALSE;
}

// --- Implementation of OverrideHost -------------------------------------

OverrideHost::~OverrideHost()
{
	m_intoverrides.Clear();
	m_stringoverrides.Clear();
}

/** Get the index of a preference inside the integerprefdefault structure.
  *
  * @param prefs The integerprefdefault structure to look in.
  * @param section Name of the section.
  * @param key Name of the key.
  * @return Index of the prefrence, -1 if not found.
  */
int OverrideHost::GetPrefIndex(const struct OpPrefsCollection::integerprefdefault *prefs,
                               OpPrefsCollection::IniSection section,
                               const char *key)
{
	const struct OpPrefsCollection::integerprefdefault *base = prefs;
	while (prefs->section != OpPrefsCollection::SNone)
	{
		if (prefs->section == section &&
		    !OVERRIDE_STRCMP(prefs->key, key))
		{
			return prefs - base;
		}
		prefs ++;
	}

	// Not found
	return -1;
}

/** Get the index of a preference inside the stringprefdefault structure.
  *
  * @param prefs The stringprefdefault structure to look in.
  * @param section Name of the section.
  * @param key Name of the key.
  * @return Index of the prefrence, -1 if not found.
  */
int OverrideHost::GetPrefIndex(const struct OpPrefsCollection::stringprefdefault *prefs,
                               OpPrefsCollection::IniSection section,
                               const char *key)
{
	const struct OpPrefsCollection::stringprefdefault *base = prefs;
	while (prefs->section != OpPrefsCollection::SNone)
	{
		if (prefs->section == section &&
		    !OVERRIDE_STRCMP(prefs->key, key))
		{
			return prefs - base;
		}
		prefs ++;
	}

	// Not found
	return -1;
}

/** Get the integer override for a certain preference index. */
HostOverrideInt *OverrideHost::GetIntObjectForIdx(int prefidx) const
{
	// Iterate over all integer overrides to see if we can find one that
	// matches
	HostOverrideInt *p =
		static_cast<HostOverrideInt *>(m_intoverrides.First());
	while (p)
	{
		if (p->Index() == prefidx) return p;

		p = static_cast<HostOverrideInt *>(p->Suc());
	}
	return NULL;
}

/** Get the string override for a certain preference index */
HostOverrideString *OverrideHost::GetStringObjectForIdx(int prefidx) const
{
	// Iterate over all integer overrides to see if we can find one that
	// matches
	HostOverrideString *p =
		static_cast<HostOverrideString *>(m_stringoverrides.First());
	while (p)
	{
		if (p->Index() == prefidx) return p;

		p = static_cast<HostOverrideString *>(p->Suc());
	}
	return NULL;
}

/** Insert an integer override in the list of overrides.
  *
  * @param prefidx Index of the override to insert.
  * @param value The overridden value.
  */
void OverrideHost::InsertOverrideL(int prefidx, int value)
{
	// Remove any existing override setting with this index
	HostOverrideInt *old = GetIntObjectForIdx(prefidx);
	if (old)
	{
		old->Out();
		OP_DELETE(old);
	}

	// Create a new override
	HostOverrideInt *newobj = OP_NEW_L(HostOverrideInt, (prefidx, value));
	newobj->Into(&m_intoverrides);
}

/** Insert a string override in the list of overrides.
  *
  * @param prefidx Index of the override to insert.
  * @param value The overridden value.
  */
void OverrideHost::InsertOverrideL(int prefidx, const OpStringC &value)
{
	// Remove any existing override setting with this index
	HostOverrideString *old = GetStringObjectForIdx(prefidx);
	if (old)
	{
		old->Out();
		OP_DELETE(old);
	}

	// Create a new override
	OpStackAutoPtr<HostOverrideString>
		newobj(OP_NEW_L(HostOverrideString, (prefidx)));
	newobj->ConstructL(value);
	newobj.release()->Into(&m_stringoverrides);
}

#ifdef PREFS_READ
/** Convert a <section,key> pair to a "SectionName|Key" string.
 *
 * @param[out] key Variable in which the output parameter is stored.
 * @param section Numeric section to which key belongs.
 * @param name Name of key.
 *
 * @return OpStatus::OK on success, otherwise OpStatus::ERR_NO_MEMORY.
 */
OP_STATUS OverrideHost::GetQualifiedKey(OpString& key, int section, const char* name) const
{
	const char* section_name = OpPrefsCollection::SectionNumberToString(static_cast<OpPrefsCollection::IniSection>(section));
	key.Reserve(op_strlen(section_name) + 1 /* | */ + op_strlen(name) + 1 /* \0 */);

	RETURN_IF_ERROR(key.Append(section_name));
	RETURN_IF_ERROR(key.Append("|"));
	RETURN_IF_ERROR(key.Append(name));

	return OpStatus::OK;
}

OP_STATUS OverrideHost::GetQualifiedKey(OpString& key, const OpPrefsCollection::integerprefdefault *pref) const
{
	return GetQualifiedKey(key, pref->section, pref->key);
}

OP_STATUS OverrideHost::GetQualifiedKey(OpString& key, const OpPrefsCollection::stringprefdefault *pref) const
{
	return GetQualifiedKey(key, pref->section, pref->key);
}
#endif // PREFS_READ

#ifdef PREFS_WRITE
OP_STATUS OverrideHost::WriteOverrideL(
	PrefsFile *reader,
	const struct OpPrefsCollection::integerprefdefault *pref,
	int prefidx, int value, BOOL from_user)
{
	if (reader->AllowedToChangeL(OpPrefsCollection::SectionNumberToString(pref->section), pref->key))
	{
		OpString key;
		RETURN_IF_ERROR(GetQualifiedKey(key, pref));

		OP_STATUS rc;
		BOOL overwrite = from_user;
		OpString host_name_to_write;
		RETURN_IF_ERROR(host_name_to_write.Set(m_host));
		if (m_match_left)
			RETURN_IF_ERROR(host_name_to_write.Append(UNI_L("*")));
#ifdef PREFSFILE_WRITE_GLOBAL
		if (from_user)
#else
		overwrite = TRUE;
#endif
		{
			rc = reader->WriteIntL(host_name_to_write, key, value);
		}
#ifdef PREFSFILE_WRITE_GLOBAL
		else
		{
			rc = reader->WriteIntGlobalL(host_name_to_write, key, value);
			// Check if we should overwrite the current value (i.e it is
			// not overwritten internally).
			overwrite = value == reader->ReadIntL(host_name_to_write, key);
		}
#endif

		// Create a new override
		if (overwrite && OpStatus::IsSuccess(rc))
		{
			InsertOverrideL(prefidx, value);
		}

		return rc;
	}
	else
	{
		return OpStatus::ERR_NO_ACCESS;
	}
}
#endif // PREFS_WRITE

#ifdef PREFS_WRITE
OP_STATUS OverrideHost::WriteOverrideL(
	PrefsFile *reader,
	const struct OpPrefsCollection::stringprefdefault *pref,
	int prefidx, const OpStringC &value, BOOL from_user)
{
	if (reader->AllowedToChangeL(OpPrefsCollection::SectionNumberToString(pref->section), pref->key))
	{
		OpString key;
		RETURN_IF_ERROR(GetQualifiedKey(key, pref));

		OP_STATUS rc;
		BOOL overwrite = from_user;
		OpString host_name_to_write;
		RETURN_IF_ERROR(host_name_to_write.Set(m_host));
		if (m_match_left)
			RETURN_IF_ERROR(host_name_to_write.Append(UNI_L("*")));
#ifdef PREFSFILE_WRITE_GLOBAL
		if (from_user)
#else
		overwrite = TRUE;
#endif
		{
			rc = reader->WriteStringL(host_name_to_write, key, value);
		}
#ifdef PREFSFILE_WRITE_GLOBAL
		else
		{
			rc = reader->WriteStringGlobalL(host_name_to_write, key, value);
			// Check if we should overwrite the current value (i.e it is
			// not overwritten internally).
			overwrite = value.Compare(reader->ReadStringL(host_name_to_write, key)) == 0;
		}
#endif

		// Create a new override
		if (overwrite && OpStatus::IsSuccess(rc))
		{
			InsertOverrideL(prefidx, value);
		}

		return rc;
	}
	else
	{
		return OpStatus::ERR_NO_ACCESS;
	}
}
#endif // PREFS_WRITE

#ifdef PREFS_WRITE
BOOL OverrideHost::RemoveOverrideL(PrefsFile *reader,
								   const struct OpPrefsCollection::integerprefdefault *pref,
								   int prefidx,
								   BOOL from_user)
{
	BOOL rc = FALSE;
#ifndef PREFSFILE_WRITE_GLOBAL
	from_user = TRUE;
#endif // PREFSFILE_WRITE_GLOBAL
	OpString host_name_to_remove;
	RETURN_VALUE_IF_ERROR(host_name_to_remove.Set(m_host), FALSE);
	if (m_match_left)
		RETURN_VALUE_IF_ERROR(host_name_to_remove.Append(UNI_L("*")), FALSE);

	HostOverrideInt *p = GetIntObjectForIdx(prefidx);
	if (p)
	{
		OpString key;
		RETURN_VALUE_IF_ERROR(GetQualifiedKey(key, pref), FALSE);

		if (from_user)
			rc = reader->DeleteKeyL(host_name_to_remove, key);
		else
			rc = reader->DeleteKeyGlobalL(host_name_to_remove, key);

		p->Out();
		OP_DELETE(p);
	}

	return rc;
}
#endif // PREFS_WRITE

#ifdef PREFS_WRITE
BOOL OverrideHost::RemoveOverrideL(PrefsFile *reader,
										const struct OpPrefsCollection::stringprefdefault *pref,
										int prefidx,
										BOOL from_user)
{
	BOOL rc = FALSE;
#ifndef PREFSFILE_WRITE_GLOBAL
	from_user = TRUE;
#endif // PREFSFILE_WRITE_GLOBAL
	OpString host_name_to_remove;
	RETURN_VALUE_IF_ERROR(host_name_to_remove.Set(m_host), FALSE);
	if (m_match_left)
		RETURN_VALUE_IF_ERROR(host_name_to_remove.Append(UNI_L("*")), FALSE);

	HostOverrideString *p = GetStringObjectForIdx(prefidx);
	if (p)
	{
		OpString key;
		RETURN_VALUE_IF_ERROR(GetQualifiedKey(key, pref), FALSE);

		if (from_user)
			rc = reader->DeleteKeyL(host_name_to_remove, key);
		else
			rc = reader->DeleteKeyGlobalL(host_name_to_remove, key);

		p->Out();
		OP_DELETE(p);
	}

	return rc;
}
#endif // PREFS_WRITE

BOOL OverrideHost::GetOverridden(int prefidx, int &value) const
{
	HostOverrideInt *p = GetIntObjectForIdx(prefidx);
	if (p)
	{
		value = p->Value();
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

BOOL OverrideHost::GetOverridden(int prefidx, const OpStringC **value) const
{
	HostOverrideString *p = GetStringObjectForIdx(prefidx);
	if (p)
	{
		*value = p->Value();
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

size_t OverrideHost::OverrideCount() const
{
	return m_intoverrides.Cardinal() + m_stringoverrides.Cardinal();
}

// --- Implementation of OpOverrideHostContainer --------------------------

OpOverrideHostContainer::~OpOverrideHostContainer()
{
	// Clean up
	m_overrides.Clear();
}

OpOverrideHost *OpOverrideHostContainer::FindOverrideHost(
	const uni_char *host, BOOL exact, BOOL active, OpOverrideHost *override_host) const
{
	OP_ASSERT(host && *host); // Should have been caught earlier
	if (!m_overrides.Empty())
	{
		size_t hostlen = uni_strlen(host);
		OpString host_to_find;
		RETURN_VALUE_IF_ERROR(host_to_find.Set(host), NULL);
		if (host_to_find[hostlen - 1] == '*')
			host_to_find[--hostlen] = 0;

		OpOverrideHost *p = override_host ? override_host :
			static_cast<OpOverrideHost *>(m_overrides.First());
		while (p)
		{
			if (p->IsHost(host_to_find, hostlen, exact, active))
			{
				return p;
			}
			p = static_cast<OpOverrideHost *>(p->Suc());
		}
	}

	return NULL;
}

OpOverrideHost *OpOverrideHostContainer::FindOrCreateOverrideHostL(
	const uni_char *host, BOOL from_user)
{
	OpOverrideHost *override = FindOverrideHost(host, TRUE, FALSE);

	if (override)
	{
		// Use the existing object
		if (from_user)
		{
			override->SetFromUser(from_user);
		}
		return override;
	}
	else
	{
		// Create a new one and add it before all objects with a shorter
		// host name
		OpOverrideHost *newobj = CreateOverrideHostObjectL(host, from_user);
		OpOverrideHost *p = static_cast<OpOverrideHost *>(m_overrides.First());
		while (p && p->HostNameLength() > newobj->HostNameLength())
		{
			p = static_cast<OpOverrideHost *>(p->Suc());
		}
		if (p)
		{
			newobj->Precede(p);
		}
		else
		{
			newobj->Into(&m_overrides);
		}
		return newobj;
	}
}

void OpOverrideHostContainer::RemoveOverridesL(const uni_char *host)
{
	if (host)
	{
		OpOverrideHost *override = FindOverrideHost(host, TRUE, FALSE);
		if (override)
		{
			override->Out();
			OP_DELETE(override);
		}

		BroadcastOverride(host);
	}
}

#ifdef PREFS_WRITE
BOOL OpOverrideHostContainer::DoRemoveIntegerOverrideL(PrefsFile *reader,
													   const uni_char *host,
													   const struct OpPrefsCollection::integerprefdefault *pref,
													   int which,
													   BOOL from_user)
{
	BOOL res = FALSE;

	if (host)
	{
		if (OverrideHost *h = static_cast<OverrideHost*>(FindOverrideHost(host, TRUE, FALSE)))
			if ((res = h->RemoveOverrideL(reader, pref, which, from_user)))
			{
				if (h->OverrideCount() == 0 && g_prefsManager->HostOverrideCount(host) == 0)
					// This was the final override, remove all traces of the host.
					g_prefsManager->RemoveOverridesL(host, from_user);
				else
					BroadcastOverride(host);
			}
	}
	else
	{
		OverrideHost *h = static_cast<OverrideHost*>(m_overrides.First());

		while (h)
		{
			OverrideHost* suc = static_cast<OverrideHost*>(h->Suc());
			uni_char* host_copy = NULL;
			if (OpStatus::IsError(UniSetStr(host_copy, h->Host())))
				break;
			ANCHOR_ARRAY(uni_char, host_copy);
			res |= DoRemoveIntegerOverrideL(reader, host_copy, pref, which, from_user);
			h = suc;
		}
	}

	return res;
}
#endif // PREFS_WRITE

#ifdef PREFS_WRITE
BOOL OpOverrideHostContainer::DoRemoveStringOverrideL(PrefsFile *reader,
													  const uni_char *host,
													  const struct OpPrefsCollection::stringprefdefault *pref,
													  int which,
													  BOOL from_user)
{
	BOOL res = FALSE;

	if (host)
	{
		if (OverrideHost *h = static_cast<OverrideHost*>(FindOverrideHost(host, TRUE, FALSE)))
			if ((res = h->RemoveOverrideL(reader, pref, which, from_user)))
			{
				if (h->OverrideCount() == 0 && g_prefsManager->HostOverrideCount(host) == 0)
					// This was the final override, remove all traces of the host.
					g_prefsManager->RemoveOverridesL(host, from_user);
				else
					BroadcastOverride(host);
			}
	}
	else
	{
		OverrideHost *h = static_cast<OverrideHost*>(m_overrides.First());

		for (; h; h = static_cast<OverrideHost*>(h->Suc()))
			res |= DoRemoveStringOverrideL(reader, h->Host(), pref, which, from_user);
	}

	return res;
}
#endif // PREFS_WRITE

#endif // PREFS_HOSTOVERRIDE
