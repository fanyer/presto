/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

/** @file ua.h
 *
 * Contains functions for handling the User-Agent string.
 */

#ifndef UA_H
#define UA_H

#include "modules/prefs/prefsmanager/opprefslistener.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/url/url_enum.h"
#ifdef COMPONENT_IN_UASTRING_SUPPORT
# include "modules/util/adt/opvector.h"
#endif // COMPONENT_IN_UASTRING_SUPPORT

class ServerName;
class OpUAComponentManager;
class Window;

/** The global UAManager object. */
#define g_uaManager g_opera->url_module.m_ua_manager

/** Class used for handling the User-Agent string. */
class UAManager : public OpPrefsListener
#ifdef UA_CAN_CHANGE_LANGUAGE
	, public PrefsCollectionFilesListener
#endif
{
public:
	/** First-phase constructor. */
	UAManager();
	/** Second-phase constructor. */
	void ConstructL();
	/** Destructor. */
	virtual ~UAManager();

	/**
	 * Retrieve the full User-Agent string.
	 * @param buffer A buffer to write the User-Agent string into.
	 * @param buf_len Length of the buffer in characters.
	 * @param host Host context to retrieve string for.
	 * @param win Window context to retrieve string for.
	 * @param force_ua Force use of a specific identity string.
	 * @param use_ua_utn Insert handset serial number (Imode utn attribute only).
	 * @return Length of string retrieved.
	 */
	inline int GetUserAgentStr(char *buffer, int buf_len, const uni_char *host, Window *win = NULL, UA_BaseStringId force_ua = UA_NotOverridden, BOOL use_ua_utn = FALSE)
	{ return GetUserAgentStr(TRUE, buffer, buf_len, host, win, force_ua, use_ua_utn); }

	/**
	 * Retrieve the User-Agent version. This matches the value that is to
	 * be returned by the navigator.appVersion DOM API.
	 * @param buffer A buffer to write the version string into.
	 * @param buf_len Length of the buffer in characters.
	 * @param host Host context to retrieve string for.
	 * @param win Window context to retrieve string for.
	 * @param force_ua Force use of a specific identity string.
	 * @param use_ua_utn Insert handset serial number (Imode utn attribute only).
	 * @return Length of string retrieved.
	 */
	inline int GetUserAgentVersionStr(char *buffer, int buf_len, const uni_char *host, Window *win = NULL, UA_BaseStringId force_ua = UA_NotOverridden, BOOL use_ua_utn = FALSE)
	{ return GetUserAgentStr(FALSE, buffer, buf_len, host, win, force_ua, use_ua_utn); }

#ifdef UA_NEED_GET_VERSION_CODE
	/**
	 * Retrieve the version code used in the User-Agent string.
	 * @param use_id User-Agent string to get version code for.
	 * @param use_ver Unused.
	 * @param target Output string.
	 */
	static OP_STATUS PickSpoofVersionString(UA_BaseStringId use_id, int use_ver, OpString8 &target);
#endif

	/** Retrieve the default User-Agent type id */
	UA_BaseStringId	GetUABaseId() const	{ return m_UA_SelectedStringId; }

	/**
	 * Retrieve information about User-Agent overrides for a particular host
	 * from those set by preferences.
	 *
	 * @param host Host to check for override.
	 * @return UA to identify as, or UA_NotOverridden if no override is active.
	 */
	static UA_BaseStringId OverrideUserAgent(const ServerName *host);

#ifdef COMPONENT_IN_UASTRING_SUPPORT
	/**
	 * Add a component string to the User-Agent string. GetUserAgentStr()
	 * will incorporate it into the comment parenthesis.
	 *
	 * Several components can be added, but duplicate components will
	 * be ignored. Components added by AddComponent() can be removed
	 * using RemoveComponent().
	 *
	 * Comments must correspond to the RFC 2616 comment syntax, and must
	 * not include any opening or closing parantheses ("(" or ")").
	 *
	 * @param component String to add
	 * @param only_utn Only add this string if use_ua_utn is TRUE in the GetUserAgentStr() call (imode).
	 * @return success status
	 */
	OP_STATUS AddComponent(const char * component, BOOL only_utn = FALSE);

	/**
	 * Remove a User-Agent component string (reverse of the AddComponent()
	 * method).
	 *
	 * @param component String to remove.
	 */
	void	RemoveComponent(const char * component);
#endif // COMPONENT_IN_UASTRING_SUPPORT

#ifdef TOKEN_IN_UASTRING_SUPPORT
	/**
	 * Add a prefix to the User-Agent string. GetUserAgentStr() will
	 * add it before the regular Opera product token. It should conform
	 * to the User-Agent token syntax described in RFC 2616 section
	 * 3.8.
	 *
	 * There can only be one prefix string.
	 *
	 * @param prefix Prefix string, or NULL to clear the prefix.
	 * @return success status
	 */
	OP_STATUS SetPrefix(const char * prefix)
	{ return m_prefix.Set(prefix); }

	/**
	 * Add a suffix to the User-Agent string. GetUserAgentStr() will
	 * add it after the regular Opera product token. It should conform
	 * to the User-Agent token syntax described in RFC 2616 section
	 * 3.8.
	 *
	 * There can only be one suffix string.
	 *
	 * @param suffix Suffix string, or NULL to clear the suffix.
	 * @return success status
	 */
	OP_STATUS SetSuffix(const char * suffix)
	{ return m_suffix.Set(suffix); }
#endif // TOKEN_IN_UASTRING_SUPPORT

	// Interfaces inherited from OpPrefsListener
	virtual void PrefChanged(OpPrefsCollection::Collections id, int pref,
	                         int newvalue);

#ifdef PREFS_HOSTOVERRIDE
	virtual void HostOverrideChanged(OpPrefsCollection::Collections id, const uni_char *);
#endif

#ifdef UA_CAN_CHANGE_LANGUAGE
	// Interfaces inherited from PrefsCollectionFilesListener
	virtual void FileChangedL(PrefsCollectionFiles::filepref pref, const OpFile *newvalue);
#endif

	// End inherited interfaces

private:
	/** Internal helper for GetUserAgentStr() and GetUserAgentVersionStr() */
	const char *GetUserAgent(UA_BaseStringId identify_as, BOOL full);
	/** Internal helper for GetUserAgentStr() and GetUserAgentVersionStr() */
	int GetUserAgentStr(BOOL full, char *buffer, int buf_len, const uni_char *host, Window *win, UA_BaseStringId force_ua, BOOL use_ua_utn);

#ifdef COMPONENT_IN_UASTRING_SUPPORT
	int			FindComponent(const char * component_string);
	OP_STATUS	AssembleComponents();

	struct ua_component
	{
		OpString8 m_componentstring;
#ifdef IMODE_EXTENSIONS
		BOOL m_only_utn;
#endif
	};

	OpAutoVector<ua_component> m_components;	///< List of additional UA comments
	OpString8 m_comment_string;					///< Ready-built UA comment string
# ifdef IMODE_EXTENSIONS
	OpString8 m_comment_string_utn;				///< Ready-built UA comment string for UTN
# endif
#endif // COMPONENT_IN_UASTRING_SUPPORT
#ifdef TOKEN_IN_UASTRING_SUPPORT
	OpString8 m_prefix;							///< Current User-Agent prefix
	OpString8 m_suffix;							///< Current User-Agent suffix
#endif
	OpUAComponentManager *m_component_manager;	///< Active component manager

	UA_BaseStringId m_UA_SelectedStringId;		///< Currently selected UA string in preferences
	OpString8 m_language;						///< Cached UA language string (does not switch)
};

#endif // UA_H
