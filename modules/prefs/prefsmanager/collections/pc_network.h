/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright 2002-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#ifndef PC_NETWORK_H
#define PC_NETWORK_H

#include "modules/prefs/prefsmanager/opprefscollection_override.h"
#include "modules/prefs/prefsmanager/opprefslistener.h"
#include "modules/url/url_sn.h"

class PrefsSectionInternal;

/** Global PrefsCollectionNetwork object (singleton). */
#define g_pcnet (g_opera->prefs_module.PrefsCollectionNetwork())

/**
  * Preferences for the networking code.
  *
  * @author Peter Karlsson
  */
class PrefsCollectionNetwork
	: public OpPrefsCollectionWithHostOverride
	, private OpPrefsListener
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
	static PrefsCollectionNetwork *CreateL(PrefsFile *reader);

	virtual ~PrefsCollectionNetwork();

#include "modules/prefs/prefsmanager/collections/pc_network_h.inl"

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
	inline int GetIntegerPref(integerpref which, URL &host) const
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
	inline void GetStringPrefL(stringpref which, OpString &result, URL &host) const
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
	inline const OpStringC GetStringPref(stringpref which, URL &host) const
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
			m_stringprefdefault, PCNETWORK_NUMBEROFSTRINGPREFS,
			m_integerprefdefault, PCNETWORK_NUMBEROFINTEGERPREFS);
	}
#endif

#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
	/**
	 * Check status for automatic proxy.
	 * @see EnableAutomaticProxy
	 *
	 * @return TRUE if automatic proxy is defined AND it has not been disabled.
	 */
	BOOL IsAutomaticProxyOn();
#endif // SUPPORT_AUTO_PROXY_CONFIGURATION

	// Defaults ------
	/**
	 * Get default value for an integer preference.
	 *
	 * @param which Selects the preference to retrieve.
	 * @return The default value.
	 */
	int GetDefaultIntegerPref(integerpref which) const;

	/**
	 * Get default value for a string preference.
	 *
	 * @param which Selects the preference to retrieve.
	 * @return The default value.
	 */
	const uni_char *GetDefaultStringPref(stringpref which) const;

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

	/** Remove overridden integer preference.
	  *
	  * @param host Host to remove an override from, or NULL to match all hosts.
	  * @param which The preference to override.
	  * @param from_user TRUE if the preference is entered by the user.
	  * @return TRUE if an override was removed.
	  */
	inline BOOL RemoveOverrideL(const uni_char *host, integerpref which,
										   BOOL from_user)
	{
		return OpPrefsCollectionWithHostOverride::RemoveIntegerOverrideL(host, &m_integerprefdefault[which], int(which), from_user);
	}

	/** Remove overridden string preference.
	  *
	  * @param host Host to remove an override from, or NULL to match all hosts.
	  * @param which The preference to override.
	  * @param from_user TRUE if the preference is entered by the user.
	  * @return TRUE if an override was removed.
	  */
	inline BOOL RemoveOverrideL(const uni_char *host, stringpref which,
										   BOOL from_user)
	{
		return OpPrefsCollectionWithHostOverride::RemoveStringOverrideL(host, &m_stringprefdefault[which], int(which), from_user);
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
			m_stringprefdefault, PCNETWORK_NUMBEROFSTRINGPREFS,
			m_integerprefdefault, PCNETWORK_NUMBEROFINTEGERPREFS);
	}

#  ifdef PREFS_HOSTOVERRIDE
	virtual BOOL OverridePreferenceL(const uni_char *host, IniSection section, const char *key, const OpStringC &value, BOOL from_user)
	{
		return OpPrefsCollectionWithHostOverride::OverridePreferenceInternalL(
			host, section, key, value, from_user,
			m_stringprefdefault, PCNETWORK_NUMBEROFSTRINGPREFS,
			m_integerprefdefault, PCNETWORK_NUMBEROFINTEGERPREFS);
	}

	virtual BOOL RemoveOverrideL(const uni_char *host, IniSection section, const char *key, BOOL from_user)
	{
		return OpPrefsCollectionWithHostOverride::RemoveOverrideInternalL(
			host, section, key, from_user,
			m_stringprefdefault, PCNETWORK_NUMBEROFSTRINGPREFS,
			m_integerprefdefault, PCNETWORK_NUMBEROFINTEGERPREFS);
	}
#  endif
# endif
#endif // PREFS_WRITE

	// Helpers ------
	/** Retrieve the Accept header as a character string. Helper function for
	  * the URL code, since protocol data is 8-bit.
	  */
	inline const char *GetAcceptTypes() { return m_accept_types.CStr(); }

	/** Retrieve the Accept-Language header as a character string. Helper
	  *function for the URL code, since protocol data is 8-bit.
	  */
	inline const char *GetAcceptLanguage() { return m_accept_language.CStr(); }

#ifdef PREFS_HOSTOVERRIDE
	/** Check if we force use of 100 continue on a host. We do that for
	  * buggy servers.
	  *
	  * @param host Hostname to check for
	  * @return TRUE if we need to force 100 continue
	  */
	inline BOOL GetUse100Continue(const uni_char *host)
	{
		return static_cast<BOOL>(GetIntegerPref(Use100Continue, host));
	}
#else
	BOOL GetUse100Continue(const uni_char *host);
#endif

#ifdef PREFS_WRITE
# ifdef PREFS_HOSTOVERRIDE
	/** Set the 100 continue force flag for a host.
	  *
	  * @param host Hostname to set the flag for
	  * @param use Whether to force it or not
	  */
	inline void WriteUse100ContinueL(const OpStringC &host, BOOL use)
	{
		OverridePrefL(host.CStr(), Use100Continue, use, FALSE);
	}
# else
	void WriteUse100ContinueL(const OpStringC &host, BOOL use);
# endif
#endif // PREFS_WRITE

#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
	/** Temporarily disable the automatic proxy configuration (or enable it
	  * again). This value is not stored and will be reset when re-starting.
	  *
	  * @param enabled FALSE to disable automatic proxies.
	  */
	inline void EnableAutomaticProxy(BOOL enabled) { m_APCEnabled = enabled; }
#endif // SUPPORT_AUTO_PROXY_CONFIGURATION

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
#endif // PREFS_HOSTOVERRIDE

#ifdef PREFS_ENUMERATE
	// Enumeration helpers ------
	// 100-continue settings not supported
	virtual unsigned int GetNumberOfPreferences() const
	{
		return PCNETWORK_NUMBEROFSTRINGPREFS + PCNETWORK_NUMBEROFINTEGERPREFS;
	}

	virtual unsigned int GetPreferencesL(struct prefssetting *settings) const
	{
		return OpPrefsCollection::GetPreferencesInternalL(settings,
			m_stringprefdefault, PCNETWORK_NUMBEROFSTRINGPREFS,
			m_integerprefdefault, PCNETWORK_NUMBEROFINTEGERPREFS);
	}
#endif

	// Special APIs ------
	// Do not use the following functions in the UI, they are included
	// for the URL classes only
	/** Get address to HTTP proxy server (if enabled) */
	inline const uni_char *GetHTTPProxyServer(const uni_char *host)
	{ return GetProxyServerHelper(host, UseHTTPProxy, HTTPProxy); }
	/** Get address to HTTPS proxy server (if enabled) */
	inline const uni_char *GetHTTPSProxyServer(const uni_char *host)
	{ return GetProxyServerHelper(host, UseHTTPSProxy, HTTPSProxy); }
#ifdef FTP_SUPPORT
	/** Get address to FTP proxy server (if enabled) */
	inline const uni_char *GetFTPProxyServer(const uni_char *host)
	{ return GetProxyServerHelper(host, UseFTPProxy, FTPProxy); }
#endif
#ifdef GOPHER_SUPPORT
	/** Get address to Gopher proxy server (if enabled) */
	inline const uni_char *GetGopherProxyServer(const uni_char *host)
	{ return GetProxyServerHelper(host, UseGopherProxy, GopherProxy); }
#endif
#ifdef WAIS_SUPPORT
	/** Get address to WAIS proxy server (if enabled) */
	inline const uni_char *GetWAISProxyServer(const uni_char *host)
	{ return GetProxyServerHelper(host, UseWAISProxy, WAISProxy); }
#endif
#ifdef SOCKS_SUPPORT
	/** Get address to SOCKS proxy server (if enabled) */
	inline const uni_char *GetSOCKSProxyServer(const uni_char *host)
	{ return GetProxyServerHelper(host, UseSOCKSProxy, SOCKSProxy); }

	/** Get address to SOCKS proxy server (if enabled) */
	inline const uni_char *GetSOCKSUser(const uni_char *host)
	{ return GetProxyServerHelper(host, UseSOCKSProxy, SOCKSUser); }

	/** Get address to SOCKS proxy server (if enabled) */
	inline const uni_char *GetSOCKSPass(const uni_char *host)
	{ return GetProxyServerHelper(host, UseSOCKSProxy, SOCKSPass); }

#endif

	time_t GetFollowedLinksExpireTime();


#ifdef DYNAMIC_PROXY_UPDATE
	/** Only to be called from PrefsNotifier. The ini file must have
	  * been read into memory before calling this, since we check if
	  * the keys are set. */
	inline void ProxyChangedL() { ReadProxiesL(TRUE); }
#endif

private:
	/** Single-phase constructor.
	  *
	  * @param reader Pointer to the PrefsFile object.
	  */
	PrefsCollectionNetwork(PrefsFile *reader)
		: OpPrefsCollectionWithHostOverride(Network, reader)
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
		, m_APCEnabled(TRUE)
#endif
#ifndef PREFS_HOSTOVERRIDE
		, m_use100Continue(NULL)
#endif
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
		, m_default_auto_proxy_enabled(FALSE)
#endif
		, m_default_use_proxy_exceptions(FALSE)
		{
#ifndef HAS_COMPLEX_GLOBALS
			InitStrings();
			InitInts();
#endif
		}

	/** String preference information and default values */
	PREFS_STATIC struct stringprefdefault
		m_stringprefdefault[PCNETWORK_NUMBEROFSTRINGPREFS + 1];
	/** Integer preference information and default values */
	PREFS_STATIC struct integerprefdefault
		m_integerprefdefault[PCNETWORK_NUMBEROFINTEGERPREFS + 1];

#ifdef PREFS_VALIDATE
	virtual void CheckConditionsL(int which, int *value, const uni_char *host);
	virtual BOOL CheckConditionsL(int which, const OpStringC &invalue,
	                              OpString **outvalue, const uni_char *host);
#endif

#if defined PREFS_HAVE_STRING_API || defined PREFS_WRITE
	virtual const uni_char *GetDefaultStringInternal(int, const struct stringprefdefault *);
	virtual int GetDefaultIntegerInternal(int, const struct integerprefdefault *);
#endif

#ifdef PREFS_WRITE
	virtual void BroadcastChange(int pref, const OpStringC &newvalue);
#endif

	OpString8 m_accept_types, m_accept_language;

	BOOL StripServerL(const OpStringC &, OpString **);
	BOOL MassageNoProxyServersL(const OpStringC &, OpString **target);

#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
	BOOL m_APCEnabled; ///< Toggle for enabling/disabling autoproxy
#endif // SUPPORT_AUTO_PROXY_CONFIGURATION

#ifndef PREFS_HOSTOVERRIDE
	void ReadUse100ContinueL();
	PrefsSectionInternal *m_use100Continue;
#endif

	const uni_char *GetProxyServerHelper(const uni_char *, integerpref,
	                                     stringpref);

	OpString m_default_http_proxy, m_default_https_proxy;
#ifdef WAIS_SUPPORT
	OpString m_default_wais_proxy;
#endif
#ifdef SOCKS_SUPPORT
	OpString m_default_socks_proxy;
#endif
#ifdef GOPHER_SUPPORT
	OpString m_default_gopher_proxy;
#endif
#ifdef FTP_SUPPORT
	OpString m_default_ftp_proxy;
#endif

	BOOL m_default_http_proxy_enabled, m_default_https_proxy_enabled;
#ifdef WAIS_SUPPORT
	BOOL m_default_wais_proxy_enabled;
#endif
#ifdef SOCKS_SUPPORT
	BOOL m_default_socks_proxy_enabled;
#endif

#ifdef GOPHER_SUPPORT
	BOOL m_default_gopher_proxy_enabled;
#endif
#ifdef FTP_SUPPORT
	BOOL m_default_ftp_proxy_enabled;
#endif

#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
	OpString m_default_auto_proxy_url;
	BOOL m_default_auto_proxy_enabled;
#endif
	OpString m_default_proxy_exceptions;
	BOOL m_default_use_proxy_exceptions;

	void ReadProxiesL(BOOL);
	void ReadProxyDefaultL(BOOL, class OpSystemInfo *, const uni_char *, stringpref, OpString *, integerpref, BOOL *);

	OpString m_default_accept_language;
#ifdef ADD_PERMUTE_NAME_PARTS
	OpString m_default_hostname_postfix;
#endif

#ifndef HAS_COMPLEX_GLOBALS
	void InitStrings();
	void InitInts();
#endif
};

#endif // PC_NETWORK_H
