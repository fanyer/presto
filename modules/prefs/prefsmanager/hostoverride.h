/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright 2002-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#if !defined HOSTOVERRIDE_H && defined PREFS_HOSTOVERRIDE
#define HOSTOVERRIDE_H

#include "modules/prefs/prefsmanager/opprefscollection.h"

class OpPrefsCollectionWithHostOverride;

/** Interface describing a single host override. */
class OpHostOverride : public Link
{
public:
	/** Single-phase constructor.
	  *
	  * @param prefidx Index for this preference (integer version of the
	  *                enumeration used in OpPrefsCollection). Used for lookup.
	  */
	OpHostOverride(int prefidx)
		: m_prefidx(prefidx) {};

	virtual ~OpHostOverride() {};

	/** Retrieve the index for this override. */
	inline int Index() { return m_prefidx; }

private:
	int m_prefidx;		///< The index in the array for this preference
};

/** Class describing an overridden integer value. */
class HostOverrideInt : public OpHostOverride
{
public:
	/** Single-phase constructor.
	  *
	  * @param prefidx Index for this preference (integer version of the
	  *                enumeration used in OpPrefsCollection). Used for lookup.
	  * @param value Value for the override.
	  */
	HostOverrideInt(int prefidx, int value)
		: OpHostOverride(prefidx), m_value(value) {};

	virtual ~HostOverrideInt() {};

	/** Retrieve the value for this override. */
	inline int Value() { return m_value; }

private:
	int m_value;		///< The overridden value
};

/** Class describing an overridden string value. */
class HostOverrideString : public OpHostOverride
{
public:
	/** First-phase constructor.
	  *
	  * @param prefidx Index for this preference (integer version of the
	  *                enumeration used in OpPrefsCollection). Used for lookup.
	  */
	HostOverrideString(int prefidx)
		: OpHostOverride(prefidx) {};

	virtual ~HostOverrideString() {};

	/** Second-phace constructor
	  * @param value Value for the override.
	  */
	void ConstructL(const OpStringC &value) { m_value.SetL(value); };

	/** Retrieve the value for this override. */
	const OpStringC *Value() { return &m_value; }

private:
	OpStringS m_value;		///< The overridden value
};

/** Interface describing a particular host that has overridden preferences.
  * There is a list of these in the OpPrefsCollectionWithHostOverride
  * objects.
  *
  * This class should never be instantiated directly, please use one of its
  * decendant classes.
  *
  * @see OpPrefsCollectionWithHostOverride
  * @author Peter Karlsson
  */
class OpOverrideHost : public Link
{
public:
	/** First-phase constructor. */
	OpOverrideHost() : m_host(NULL) {};
	/** Second-phase constructor.
	  *
	  * @param host Host for which this override is valid.
	  * @param from_user Whether override was specified by the user.
	  */
	virtual void ConstructL(const uni_char *host, BOOL from_user);
	virtual ~OpOverrideHost();

	/** Determine if this override describes the requested host or not.
	  *
	  * @param host Host to check against.
	  * @param hostlen Length of host name (in uni_chars).
	  * @param exact Whether to perform an exact match.
	  * @param active Whether to require the override to be active.
	  */
	BOOL IsHost(const uni_char *host, size_t hostlen, BOOL exact, BOOL active) const;

	/** Set active status for this override host. */
	inline void SetActive(BOOL active) { m_active = active; }

	/** Return active status for this override host. */
	inline BOOL IsActive() { return m_active; }

	/** Set entered by user status for this override host. */
	inline void SetFromUser(BOOL from_user)
	{
#ifdef PREFSFILE_WRITE_GLOBAL
		m_from_user = from_user;
#endif
	}

	/** Return entered by user status for this override host. */
	inline BOOL IsFromUser()
	{
#ifdef PREFSFILE_WRITE_GLOBAL
		return m_from_user;
#else
		return TRUE;
#endif
	}

	/** Return the domain name for which this override is valid.
	 */
	inline uni_char *Host() { return m_host; }

	/** Return the length of the host name for this object. Used to be
	  * able to sort the objects by length in the linked list.
	  */
	inline size_t HostNameLength() { return m_host_len; }

	/** Return the number of preferences overridden for this host.
	 */
	virtual size_t OverrideCount() const { return 0; }

protected:
	uni_char *m_host;				///< Domain name for which this override is valid
	size_t m_host_len;				///< Length of m_host
	BOOL m_active;					///< Whether the override is active
#ifdef PREFSFILE_WRITE_GLOBAL
	BOOL m_from_user;				///< Whether the override was set by the user
#endif
	BOOL m_match_left;				///< Match from the left instead of from the right
};

/** Class describing a particular host that has overridden preferences.
  *
  * @author Peter Krefting.
  */
class OverrideHost : public OpOverrideHost
{
public:
	/** First-phase constructor. */
	OverrideHost() {};
	virtual ~OverrideHost();

#ifdef PREFS_WRITE
	/** Write a new override for this host.
	  *
	  * @param reader Reader to write the information to.
	  * @param pref Pointer to the entry in the stringprefdefault structure
	  *             for this preference. Used to get the strings to write to
	  *             to the INI file.
	  * @param prefidx Index for this preference (integer version of the
	  *                enumeration used in OpPrefsCollection). Used for lookup.
	  * @param from_user Whether the override comes from the user or not. If
	  *                  it doesn't, and there is one that does, the current
	  *                  overridden value is not changed.
	  * @param value Value for the override.
	  */
	OP_STATUS WriteOverrideL(
		PrefsFile *reader,
		const struct OpPrefsCollection::integerprefdefault *pref,
		int prefidx,
		int value,
		BOOL from_user);
#endif // PREFS_WRITE

#ifdef PREFS_WRITE
	/** Write a new override for this host.
	  *
	  * @param reader Reader to write the information to.
	  * @param pref Pointer to the entry in the stringprefdefault structure
	  *             for this preference. Used to get the strings to write to
	  *             to the INI file.
	  * @param prefidx Index for this preference (integer version of the
	  *                enumeration used in OpPrefsCollection). Used for lookup.
	  * @param from_user Whether the override comes from the user or not. If
	  *                  it doesn't, and there is one that does, the current
	  *                  overridden value is not changed.
	  * @param value Value for the override.
	  */
	OP_STATUS WriteOverrideL(
		PrefsFile *reader,
		const struct OpPrefsCollection::stringprefdefault *pref,
		int prefidx,
		const OpStringC &value,
		BOOL from_user);
#endif // PREFS_WRITE

#ifdef PREFS_WRITE
	/** Remove an existing integer override for this host.
	 *
	 * @param reader Reader to update the information of.
	 * @param pref Pointer to the entry in the integerprefdefault structure
	 *             for this preference. Used to get the strings to write to
	 *             to the INI file.
	 * @param prefidx Index for this preference (integer version of the
	 *                enumeration used in OpPrefsCollection). Used for lookup.
	 * @param from_user Whether the override comes from the user or not. If
	 *                  it doesn't, and there is one that does, the current
	 *                  overridden value is not changed.
	 */
	BOOL RemoveOverrideL(PrefsFile *reader,
						const struct OpPrefsCollection::integerprefdefault *pref,
						int prefidx,
						BOOL from_user);
#endif // PREFS_WRITE

#ifdef PREFS_WRITE
	/** Remove an existing string override for this host.
	 *
	 * @param reader Reader to update the information of.
	 * @param pref Pointer to the entry in the stringprefdefault structure
	 *             for this preference. Used to get the strings to write to
	 *             to the INI file.
	 * @param prefidx Index for this preference (integer version of the
	 *                enumeration used in OpPrefsCollection). Used for lookup.
	 * @param from_user Whether the override comes from the user or not. If
	 *                  it doesn't, and there is one that does, the current
	 *                  overridden value is not changed.
	 */
	BOOL RemoveOverrideL(PrefsFile *reader,
						const struct OpPrefsCollection::stringprefdefault *pref,
						int prefidx,
						BOOL from_user);
#endif // PREFS_WRITE

	/** Read an overridden integer value, if one exists.
	  *
	  * @param prefidx Index for this preference (integer version of the
	  *                enumeration used in OpPrefsCollection). Used for lookup.
	  * @param value Storage for the value (output).
	  * @return TRUE if there was an override. If FALSE is returned, value is
	  *         not touched.
	  */
	BOOL GetOverridden(int prefidx, int &value) const;

	/** Read an overridden string value, if one exists.
	  *
	  * @param prefidx Index for this preference (integer version of the
	  *                enumeration used in OpPrefsCollection). Used for lookup.
	  * @param value Storage for the value (output).
	  * @return TRUE if there was an override. If FALSE is returned, value is
	  *         not touched.
	  */
	BOOL GetOverridden(int prefidx, const OpStringC **value) const;

	/** Return the number of preferences overridden for this host.
	 */
	virtual size_t OverrideCount() const;

protected:
	Head m_intoverrides;			///< List of integer overrides for this domain
	Head m_stringoverrides;			///< List of string overrides for this domain

	HostOverrideInt *GetIntObjectForIdx(int) const;
	HostOverrideString *GetStringObjectForIdx(int) const;
	static int GetPrefIndex(
		const struct OpPrefsCollection::integerprefdefault *,
		OpPrefsCollection::IniSection, const char *);
	static int GetPrefIndex(
		const struct OpPrefsCollection::stringprefdefault *,
		OpPrefsCollection::IniSection, const char *);
	void InsertOverrideL(int, int);
	void InsertOverrideL(int, const OpStringC &);
	OP_STATUS GetQualifiedKey(OpString& key, int, const char*) const;
	OP_STATUS GetQualifiedKey(OpString& key, const OpPrefsCollection::integerprefdefault *) const;
	OP_STATUS GetQualifiedKey(OpString& key, const OpPrefsCollection::stringprefdefault *) const;

	friend class OpPrefsCollectionWithHostOverride;
	friend class PrefsCollectionFontsAndColors;
};

/** Interface describing something that contains override host objects.
  */
class OpOverrideHostContainer
{
public:
	/** Destructor needs to be virtual due to inheritance. */
	virtual ~OpOverrideHostContainer();

	/** Retrieve the OverrideHost object corresponding to a specified host name.
	  *
	  * @param host Host name to get the object for.
	  * @param exact Whether to perform an exact match.
	  * @param active Whether to require override to be active.
      * @param override_host Where to start searching, NULL means from the beginning.
	  * @return The located object, NULL if none was found.
	  */
	OpOverrideHost *FindOverrideHost(const uni_char *host,
									BOOL exact = FALSE,
									BOOL active = TRUE,
									OpOverrideHost *override_host = NULL) const;

	/** Retrieve the OverrideHost object corresponding to a specified host name,
	  * creating it if none was found.
	  *
	  * @param host Host name to get the object for.
	  * @param from_user Whether the override was entered by the user. If this
	  *                  is TRUE and the host object already exists, it will
	  *                  be "upgraded" to entered-by-user status.
	  * @return The located (or created) object.
	  */
	OpOverrideHost *FindOrCreateOverrideHostL(const uni_char *host, BOOL from_user);

	/** Remove all overrides for a named host.
	  *
	  * @param host Host to delete overrides for.
	  */
	virtual void RemoveOverridesL(const uni_char *host);

#ifdef PREFS_WRITE
	/** Remove an existing integer override for a named host.
	 *
	 * @param reader Reader to update the information of.
	 * @param host Host to delete overrides for, or NULL to match all hosts.
	 * @param pref Pointer to the entry in the integerprefdefault structure
	 *             for this preference. Used to get the strings to write to
	 *             to the INI file.
	 * @param prefidx Index for this preference (integer version of the
	 *                enumeration used in OpPrefsCollection). Used for lookup.
	 * @return TRUE if an override was found and removed.
	 */
	virtual BOOL DoRemoveIntegerOverrideL(PrefsFile *reader,
										  const uni_char *host,
										  const OpPrefsCollection::integerprefdefault *pref,
										  int which,
										  BOOL from_user);
#endif // PREFS_WRITE

#ifdef PREFS_WRITE
	/** Remove an existing string override for a named host.
	 *
	 * @param reader Reader to update the information of.
	 * @param host Host to delete overrides for, or NULL to match all hosts.
	 * @param pref Pointer to the entry in the stringprefdefault structure
	 *             for this preference. Used to get the strings to write to
	 *             to the INI file.
	 * @param prefidx Index for this preference (integer version of the
	 *                enumeration used in OpPrefsCollection). Used for lookup.
	 * @return TRUE if an override was found and removed.
	 */
	virtual BOOL DoRemoveStringOverrideL(PrefsFile *reader,
										 const uni_char *host,
										 const OpPrefsCollection::stringprefdefault *pref,
										 int which,
										 BOOL from_user);
#endif // PREFS_WRITE

	/** Create an OpOverrideHostObject suitable for this container.
	  * The interface only operates on OpOverrideHost interface objects,
	  * the implementing class must implement this to return the proper
	  * implementation class.
	  *
	  * @param host Host name to create an object for.
	  * @param from_user Whether the overrides are entered by the user.
	  * @return The created object.
	  */
	virtual OpOverrideHost *CreateOverrideHostObjectL(const uni_char *host, BOOL from_user) = 0;

	/** Broadcast added, changed or removed host overrides to all listeners.
	  *
	  * @param hostname The host name for which the override was changed.
	  */
	virtual void BroadcastOverride(const uni_char *hostname) = 0;

protected:
	Head m_overrides;			///< List of all host overrides
};

#endif // !HOSTOVERRIDE_H && PREFS_HOSTOVERRIDE
