/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright 2002-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#ifndef PC_DOC_H
#define PC_DOC_H

#include "modules/prefs/prefsmanager/opprefscollection_override.h"
#include "modules/prefs/prefsmanager/prefstypes.h"
#include "modules/prefsfile/prefsfile.h"
#include "modules/url/url_sn.h"
#include "modules/util/adt/opvector.h"

/** Global PrefsCollectionDoc object (singleton). */
#define g_pcdoc (g_opera->prefs_module.PrefsCollectionDoc())

#ifdef PREFS_HAS_PREFSFILE
#define HAVE_HANDLERS_FILE
#endif // PREFS_HAS_PREFSFILE

/**
  * Various uncategorized preferences used in the document code
  * (doc, dochand, logdoc).
  *
  * @author Peter Krefting
  */
class PrefsCollectionDoc : public OpPrefsCollectionWithHostOverride
{
public:
	/**
	  * Create method.
	  *
	  * This method preforms all actions required to construct the object.
	  * PrefsManager calls this method to initialize the object, and will
	  * not call the normal constructor directly.
	  *
	  * @param reader Pointer to the PrefsFile object.
	  * @return Pointer to the created object.
	  */
	static PrefsCollectionDoc *CreateL(PrefsFile *reader);

	virtual ~PrefsCollectionDoc();

#include "modules/prefs/prefsmanager/collections/pc_doc_h.inl"

	// Read ------
	/**
	 * Read an integer preference.
	 *
	 * @param which Selects the preference to retrieve.
	 * @param host Host context to retrieve the preference for. NULL will
	 * retrieve the default context.
	 */
	inline int GetIntegerPref(integerpref which, const uni_char *host = NULL) const
	{
		return OpPrefsCollectionWithHostOverride::GetIntegerPref(int(which), host);
	}

	/** @overload */
	inline int GetIntegerPref(integerpref which, const ServerName *host) const
	{
		return OpPrefsCollectionWithHostOverride::GetIntegerPref(int(which), host ? host->UniName() : NULL);
	}

	/** @overload */
	inline int GetIntegerPref(integerpref which, const URL &host) const
	{
		return OpPrefsCollectionWithHostOverride::GetIntegerPref(int(which), host);
	}

	/**
	 * Read a string preference.
	 *
	 * @param which Selects the preference to retrieve.
	 * @param result Variable where the value will be stored.
	 * @param host Host context to retrieve the preference for. NULL will
	 * retrieve the default context.
	 */
	inline void GetStringPrefL(stringpref which, OpString &result, const uni_char *host = NULL) const
	{
		result.SetL(GetStringPref(which, host));
	}

	/** @overload */
	inline void GetStringPrefL(stringpref which, OpString &result, const ServerName *host) const
	{
		result.SetL(GetStringPref(which, host));
	}

	/** @overload */
	inline void GetStringPrefL(stringpref which, OpString &result, const URL &host) const
	{
		result.SetL(GetStringPref(which, host));
	}

	/**
	 * Read a string preference. This method will return a pointer to internal
	 * data. The object is only guaranteed to live until the next call to any
	 * Write method.
	 *
	 * @param which Selects the preference to retrieve.
	 * @param host Host context to retrieve the preference for. NULL will
	 * retrieve the default context.
	 */
	inline const OpStringC GetStringPref(stringpref which, const uni_char *host = NULL) const
	{
		RETURN_OPSTRINGC(OpPrefsCollectionWithHostOverride::GetStringPref(int(which), host));
	}

	/** @overload */
	inline const OpStringC GetStringPref(stringpref which, const ServerName *host) const
	{
		RETURN_OPSTRINGC(OpPrefsCollectionWithHostOverride::GetStringPref(int(which), host ? host->UniName() : NULL));
	}

	/** @overload */
	inline const OpStringC GetStringPref(stringpref which, const URL &host) const
	{
		RETURN_OPSTRINGC(OpPrefsCollectionWithHostOverride::GetStringPref(int(which), host));
	}

#ifdef PREFS_HOSTOVERRIDE
	/**
	 * Check if integer preference is overridden for this host.
	 *
	 * @param which Selects the preference to check.
	 * @param host Host context to check for override.
	 * @return TRUE if preference is overridden.
	 */
	inline BOOL IsPreferenceOverridden(integerpref which, const uni_char *host) const
	{
		return IsIntegerOverridden(int(which), host);
	}
	inline BOOL IsPreferenceOverridden(integerpref which, URL &host) const
	{
		return IsIntegerOverridden(int(which), host);
	}

	/**
	 * Check if string preference is overridden for this host.
	 *
	 * @param which Selects the preference to check.
	 * @param host Host context to check for override.
	 * @return TRUE if preference is overridden.
	 */
	inline BOOL IsPreferenceOverridden(stringpref which, const uni_char *host) const
	{
		return IsStringOverridden(int(which), host);
	}
	inline BOOL IsPreferenceOverridden(stringpref which, URL &host) const
	{
		return IsStringOverridden(int(which), host);
	}
#endif // PREFS_HOSTOVERRIDE

#ifdef PREFS_HAVE_STRING_API
	virtual BOOL GetPreferenceL(IniSection section, const char *key, OpString &target,
		BOOL defval, const uni_char *host)
	{
		return OpPrefsCollectionWithHostOverride::GetPreferenceInternalL(
			section, key, target, defval, host,
			m_stringprefdefault, PCDOC_NUMBEROFSTRINGPREFS,
			m_integerprefdefault, PCDOC_NUMBEROFINTEGERPREFS);
	}
#endif

	// Defaults ------
	/**
	 * Get default value for an integer preference.
	 *
	 * @param which Selects the preference to retrieve.
	 * @return The default value.
	 */
	inline int GetDefaultIntegerPref(integerpref which) const
	{
		return m_integerprefdefault[which].defval;
	}

	/**
	 * Get default value for a string preference.
	 *
	 * @param which Selects the preference to retrieve.
	 * @return The default value.
	 */
	inline const uni_char *GetDefaultStringPref(stringpref which) const
	{
		return m_stringprefdefault[which].defval;
	}

#ifdef PREFS_WRITE
	// Write ------
# ifdef PREFS_HOSTOVERRIDE
	/** Write overridden integer preferences.
	  *
	  * @param host Host to write an override for
	  * @param which The preference to override.
	  * @param value Value for the override.
	  * @param from_user TRUE if the preference is entered by the user.
	  * @return ERR_NO_ACCESS if override is not allowed, OK otherwise.
	  */
	inline OP_STATUS OverridePrefL(const uni_char *host, integerpref which,
	                               int value, BOOL from_user)
	{
		return OpPrefsCollectionWithHostOverride::OverridePrefL(host, &m_integerprefdefault[which], int(which), value, from_user);
	}
# endif // PREFS_HOSTOVERRIDE

# ifdef PREFS_HOSTOVERRIDE
	/** Write overridden string preferences.
	  *
	  * @param host Host to write an override for
	  * @param which The preference to override.
	  * @param value Value for the override.
	  * @param from_user TRUE if the preference is entered by the user.
	  * @return ERR_NO_ACCESS if override is not allowed, OK otherwise.
	  */
	inline OP_STATUS OverridePrefL(const uni_char *host, stringpref which,
	                               const OpStringC &value, BOOL from_user)
	{
		return OpPrefsCollectionWithHostOverride::OverridePrefL(host, &m_stringprefdefault[which], int(which), value, from_user);
	}
# endif // PREFS_HOSTOVERRIDE

	/** Write a string preference.
	  *
	  * @param which The preference to write.
	  * @param value Value for the write.
	  * @return ERR_NO_ACCESS if override is not allowed, OK otherwise.
	  */
	inline OP_STATUS WriteStringL(stringpref which, const OpStringC &value)
	{
		return OpPrefsCollection::WriteStringL(&m_stringprefdefault[which], int(which), value);
	}

	/** Write an integer preference.
	  *
	  * @param which The preference to write.
	  * @param value Value for the write.
	  * @return ERR_NO_ACCESS if override is not allowed, OK otherwise.
	  */
	inline OP_STATUS WriteIntegerL(integerpref which, int value)
	{
		return OpPrefsCollection::WriteIntegerL(&m_integerprefdefault[which], int(which), value);
	}

# ifdef PREFS_HAVE_STRING_API
	virtual BOOL WritePreferenceL(IniSection section, const char *key, const OpStringC &value)
	{
		return OpPrefsCollection::WritePreferenceInternalL(
			section, key, value,
			m_stringprefdefault, PCDOC_NUMBEROFSTRINGPREFS,
			m_integerprefdefault, PCDOC_NUMBEROFINTEGERPREFS);
	}

#  ifdef PREFS_HOSTOVERRIDE
	virtual BOOL OverridePreferenceL(const uni_char *host, IniSection section, const char *key, const OpStringC &value, BOOL from_user)
	{
		return OpPrefsCollectionWithHostOverride::OverridePreferenceInternalL(
			host, section, key, value, from_user,
			m_stringprefdefault, PCDOC_NUMBEROFSTRINGPREFS,
			m_integerprefdefault, PCDOC_NUMBEROFINTEGERPREFS);
	}

	virtual BOOL RemoveOverrideL(const uni_char *host, IniSection section, const char *key, BOOL from_user)
	{
		return OpPrefsCollectionWithHostOverride::RemoveOverrideInternalL(
			host, section, key, from_user,
			m_stringprefdefault, PCDOC_NUMBEROFSTRINGPREFS,
			m_integerprefdefault, PCDOC_NUMBEROFINTEGERPREFS);
	}
#  endif
# endif
#endif // PREFS_WRITE

#ifdef PREFS_WRITE
	// Reset ------
	/** Reset a string preference. Resets the preference to default by
	  * removing the set value from the preference storage.
	  *
	  * @param which The preference to reset.
	  * @return TRUE if the delete succeeded.
	  */
	inline BOOL ResetStringL(stringpref which)
	{
		return OpPrefsCollection::ResetStringL(&m_stringprefdefault[which], int(which));
	}

	/** Reset an integer preference. Resets the preference to default by
	  * removing the set value from the preference storage.
	  *
	  * @param which The preference to reset.
	  * @return TRUE if the delete succeeded.
	  */
	inline BOOL ResetIntegerL(integerpref which)
	{
		return OpPrefsCollection::ResetIntegerL(&m_integerprefdefault[which], int(which));
	}
#endif // PREFS_WRITE

	// Fetch preferences from file ------
	virtual void ReadAllPrefsL(PrefsModule::PrefsInitInfo *info);
#ifdef PREFS_HOSTOVERRIDE
	virtual void ReadOverridesL(const uni_char *host, PrefsSection *section, BOOL active, BOOL from_user);
#endif

	// Helpers ------
	/** Retrieve total number of trusted external protocols. */
	int GetNumberOfTrustedProtocols()
	{
		return static_cast<int>(m_trusted_applications.GetCount());
	}

	/** Retrieve name of a trusted external protocol. */
	OpStringC GetTrustedProtocol(int i)
	{
		return m_trusted_applications.Get(i)->protocol;
	}

	/**
	 * Store information about a trusted external protocol. The information will
	 * not be saved to disk until WriteTrustedProtocolsL() is called. A new entry
	 * must specify a protocol. Modifying an existing entry will be rejected if
	 * the data flag field includes 'TP_Protocol' and the corresponding string is empty
	 *
	 * @param index An index of the position the protocol info is supposed to be placed at.
	 * @param data Collection of data to be stored. The actual stored data depends datas sate
	 * @return TRUE on success, otherwise FALSE if index is out of range protocol not valid
	 */
	BOOL SetTrustedProtocolInfoL(int index, const TrustedProtocolData& data);

	/**
	 * Retrieve information about a trusted external protocol.
	 *
	 * @param index Number of protocol to retrieve.
	 * @param data protocol data on a valid return
	 * @return TRUE on success, otherwise FALSE if index is out of range
	 */
	BOOL GetTrustedProtocolInfo(int index, TrustedProtocolData& data);

	/**
	 * Retrieve information about a trusted external protocol.
	 *
	 * @param protocol The protocol value to retrieve data from
	 * @param data protocol data on a valid return
	 * @return -1 on failure, otherwise index of matched protocol
	 */
	int GetTrustedProtocolInfo(const OpStringC &protocol, TrustedProtocolData& data);

	/**
	 * Look up the index where the protocol is stored. The index can
	 * be used for @ref SetTrustedProtocolInfoL and @GetTrustedProtocolInfo
	 * and be used to test if an entry for the protocol exists
	 *
	 * @param protocol The protocol to examine
	 * @return -1 if no match exists, otherwise index of matched protocol
	 */
	int GetTrustedProtocolIndex(const OpStringC& protocol);

	/**
	 * Removes information about a trusted external protocol.
	 *
	 * @param index The number of a protocol info to remove.
	 */
	void RemoveTrustedProtocolInfo(int index);

	/**
	 * Removes information about a trusted external protocol.
	 *
	 * @param protocol The name of a protocol the info about which should be removed.
	 * @param save_changes If TRUE the changes are written to the file.
	 */
	void RemoveTrustedProtocolInfoL(const OpStringC& protocol, BOOL save_changes = TRUE);

	/**
	 * Retrieve information about a trusted external protocol.
	 * This function is obsolete. It can not manage web handlers.
	 * Use @ref GetTrustedProtocolInfo(int, TrustedProtocolData&) instead.
	 *
	 * @param index Number of protocol to retrieve.
	 * @param[out] protocol Name of protocol assigned to this slot.
	 * @param[out] filename Application associated with this protocol.
	 * @param[out] description The description of the application associated with this protocol.
	 * @param[out] viewer_mode Value describing how to handle protocol.
	 * @param[out] in_terminal Whether to open protocol in terminal.
	 * @param[out] user_defined Whether the trusted protocol is defined by a user.
	 * 
	 */
	DEPRECATED(BOOL GetTrustedProtocolInfoL(int index, OpString &protocol,
								 OpString &filename,  OpString &description,
								 ViewerMode &viewer_mode, BOOL &in_terminal, BOOL &user_defined));

	/**
	 * Retrieve information about a trusted external protocol.
	 * This function is obsolete. It can not manage web handlers.
	 * Use @ref GetTrustedProtocolInfo(const OpStringC&, TrustedProtocolData&) instead.
	 *
	 * @param protocol Name of protocol to get info about.
	 * @param[out] filename Application associated with this protocol.
	 * @param[out] description The description of the application associated with this protocol.
	 * @param[out] in_terminal Whether to open protocol in terminal.
	 * @param[out] user_defined Whether the trusted protocol is defined by a user.
	 *
	 * @return Index of the protocol, -1 if not found.
	 */
	DEPRECATED(int GetTrustedApplicationL(const OpStringC &protocol, OpString &filename, OpString &description, BOOL &in_terminal, BOOL &user_defined));

	/** Store info about a trusted external protocol. The information will
	 * not be saved to disk until WriteTrustedProtocolsL() is called.
	 * This function is obsolete. It can not manage web handlers.
	 * Use @ref SetTrustedProtocolInfoL(int, const TrustedProtocolData&) instead.
	 *
	 * @param index An index of the position the protocol info is supposed to be placed at.
	 * @param protocol A name of protocol to set.
	 * @param filename An application associated with this protocol.
	 * @param description The description of the application associated with this protocol.
	 * @param viewer_mode A value describing how to handle the protocol.
	 * @param in_terminal Whether to open the protocol in a terminal.
	 * @param user_defined Whether the trusted protocol is defined by a user.
	 */
	DEPRECATED(BOOL SetTrustedProtocolInfoL(int index, const OpStringC &protocol, const OpStringC &filename, const OpStringC &description, ViewerMode handler_mode, BOOL in_terminal, BOOL user_defined = FALSE));

#if defined HAVE_HANDLERS_FILE && defined PREFSFILE_WRITE
	/**
	 * Write stored information about trusted protocols.
	 */
	void WriteTrustedProtocolsL(int num_trusted_protocols);
# endif // defined HAVE_HANDLERS_FILE && defined PREFSFILE_WRITE

	/**
	 * Read stored information about trusted protocols.
	 */
	void ReadTrustedProtocolsL();

#ifdef PREFS_ENUMERATE
	// Enumeration helpers ------
	// Trusted protocols not supported
	virtual unsigned int GetNumberOfPreferences() const
	{
		return PCDOC_NUMBEROFSTRINGPREFS + PCDOC_NUMBEROFINTEGERPREFS;
	}

	virtual unsigned int GetPreferencesL(struct prefssetting *settings) const
	{
		return OpPrefsCollection::GetPreferencesInternalL(settings,
			m_stringprefdefault, PCDOC_NUMBEROFSTRINGPREFS,
			m_integerprefdefault, PCDOC_NUMBEROFINTEGERPREFS);
	}
#endif

#ifdef HAVE_HANDLERS_FILE
	PrefsFile& GetHandlerFile() { return m_handlers_file; }
#endif // HAVE_HANDLERS_FILE

private:
	/** Single-phase constructor.
	  *
	  * @param reader Pointer to the PrefsFile object.
	  */
	PrefsCollectionDoc(PrefsFile *reader)
		: OpPrefsCollectionWithHostOverride(Doc, reader)
#ifdef HAVE_HANDLERS_FILE
		 , m_handlers_file(PREFS_INI)
#endif // HAVE_HANDLERS_FILE
		{
#ifndef HAS_COMPLEX_GLOBALS
			InitStrings();
			InitInts();
#endif
		}

	/** String preference information and default values */
	PREFS_STATIC struct stringprefdefault
		m_stringprefdefault[PCDOC_NUMBEROFSTRINGPREFS + 1];
	/** Integer preference information and default values */
	PREFS_STATIC struct integerprefdefault
		m_integerprefdefault[PCDOC_NUMBEROFINTEGERPREFS + 1];

#ifdef PREFS_VALIDATE
	virtual void CheckConditionsL(int which, int *value, const uni_char *host);
	virtual BOOL CheckConditionsL(int which, const OpStringC &invalue,
	                              OpString **outvalue, const uni_char *host);
#endif

#ifdef PREFS_HAVE_STRING_API
	virtual int GetDefaultIntegerInternal(int, const struct integerprefdefault *);
#endif

	struct trusted_apps
	{
		OpString protocol;		///< Name of protocol.
		OpString filename;		///< Application (if any) associated with protocol.
		OpString webhandler;    ///< Web handler (if any) associated with protocol.
		OpString description;	///< Description of a handler
		int      flags;			///< Bit field describing how to handle protocol.
	};

	OpAutoVector<trusted_apps> m_trusted_applications;

#ifndef HAS_COMPLEX_GLOBALS
	void InitStrings();
	void InitInts();
#endif

#ifdef HAVE_HANDLERS_FILE
	PrefsFile m_handlers_file;
#endif // HAVE_HANDLERS_FILE
};

#endif // PC_CORE_H
