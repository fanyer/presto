/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright 2002-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#ifndef OPPREFSCOLLECTION_DEFAULT_H
#define OPPREFSCOLLECTION_DEFAULT_H

#include "modules/prefs/prefsmanager/opprefscollection.h"

/**
 * Standard preference collection with outhost override support.
 *
 * This is the standard common implementation of the OpPrefsCollection
 * class without support for overriding based on host name.
 */
class OpPrefsCollectionDefault : public OpPrefsCollection
{
public:
	/** Destructor. */
	virtual ~OpPrefsCollectionDefault();

	// Implementation of OpPrefsCollection interfaces ---
#ifdef PREFS_HOSTOVERRIDE
	virtual void SetOverrideReader(PrefsFile *reader);
	virtual HostOverrideStatus IsHostOverridden(const uni_char *host, BOOL exact = TRUE);
	virtual BOOL SetHostOverrideActive(const uni_char *host, BOOL status = TRUE);
	virtual void ReadOverridesL(const uni_char *host, PrefsSection *section, BOOL active, BOOL from_user);
# ifdef PREFS_WRITE
	virtual void RemoveOverridesL(const uni_char *host);
#  ifdef PREFS_HAVE_STRING_API
	virtual BOOL OverridePreferenceL(const uni_char *, IniSection, const char *, const OpStringC &, BOOL);
	virtual BOOL RemoveOverrideL(const uni_char *, IniSection, const char *, BOOL);
#  endif // PREFS_HAVE_STRING_API
# endif // PREFS_WRITE
#endif // PREFS_HOSTOVERRIDE

#ifdef PREFS_HAVE_STRING_API
	BOOL GetPreferenceInternalL(IniSection section, const char *key,
		OpString &target, BOOL defval,
		const struct stringprefdefault *strings, int numstrings,
		const struct integerprefdefault *integers, int numintegers);
#endif

protected:
	/** Single-phase constructor.
	  *
	  * Inherited classes may have two-phase constructors if they wish.
	  *
	  * @param identity Identity of this collection (communicated by subclass).
	  * @param reader Pointer to the PrefsFile object.
	  */
	OpPrefsCollectionDefault(enum Collections identity, PrefsFile *reader)
		: OpPrefsCollection(identity, reader) {};

	/** Interface used by sub-classes to read integer preferences. */
	int GetIntegerPref(int which) const;

	/** Interface used by sub-classes to read string preferences. */
	const OpStringC GetStringPref(int which) const;
};

#endif // OPPREFSCOLLECTION_DEFAULT_H
