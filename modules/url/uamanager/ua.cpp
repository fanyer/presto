/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
** Peter Krefting
*/

/** @file ua.cpp
 *
 * Contains functions for handling the User-Agent string.
 */

#include "core/pch.h"

#include "modules/about/operaversion.h"
#include "modules/url/uamanager/ua.h"
#include "modules/pi/OpUAComponentManager.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/hardcore/mem/mem_man.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/url/url2.h"
#include "modules/url/url_man.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/dochand/win.h"
#include "modules/windowcommander/src/WindowCommander.h"

#if defined SC_UA_CONTEXT_COMPONENTS && defined COMPONENT_IN_UASTRING_SUPPORT
# error "Cannot combine TWEAK_URL_UA_COMPONENTS_CONTEXT and TWEAK_URL_UA_COMPONENTS"
#endif
#if defined SC_UA_CONTEXT_COMPONENTS && defined TOKEN_IN_UASTRING_SUPPORT
# error "Cannot combine TWEAK_URL_UA_COMPONENTS_CONTEXT and TWEAK_URL_UA_TOKENS"
#endif

UAManager::UAManager()
  : m_component_manager(NULL)
  , m_UA_SelectedStringId(UA_MSIE)
{}

void UAManager::ConstructL()
{
	// Create the component manager
	LEAVE_IF_ERROR(OpUAComponentManager::Create(&m_component_manager));

	// Read setting from PrefsManager
	PrefChanged(OpPrefsCollection::Network, PrefsCollectionNetwork::UABaseId,
		g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UABaseId));

	// Register as a listener
	g_pcnet->RegisterListenerL(this);
#ifdef UA_CAN_CHANGE_LANGUAGE
	g_pcfiles->RegisterFilesListenerL(this);
#endif
}

UAManager::~UAManager()
{
	// Unregister as a listener
	g_pcnet->UnregisterListener(this);
#ifdef UA_CAN_CHANGE_LANGUAGE
	g_pcfiles->UnregisterFilesListener(this);
#endif

	// Deallocate the component manager
	OP_DELETE(m_component_manager);
}

//	___________________________________________________________________________
//
//	GetUserAgentStr
//
//	___________________________________________________________________________
//
//

#ifndef SC_UA_EXTERNAL
int UAManager::GetUserAgentStr(BOOL full, char *buffer, int buf_len, const uni_char *host, Window *win, UA_BaseStringId force_ua, BOOL use_ua_utn)
{
	//Yes compiler, we declare and possibly do not use these variables..
	(void)use_ua_utn;

	int len = 0;
	UA_BaseStringId identify_as;
	if (force_ua == UA_NotOverridden)
	{
		// Optimization when host override handling is taken care of outside
		// UAManager (as in ServerManager).
		if (host)
		{
			identify_as = static_cast<UA_BaseStringId>
				(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UABaseId, host));
		}
		else
		{
			identify_as = m_UA_SelectedStringId;
		}
	}
	else
	{
		identify_as = force_ua;
	}

	// Set up parameters for OpUAComponentManager.
	OpUAComponentManager::Args ua_args =
	{
		identify_as
		, host
		, win ? win->GetWindowCommander() : NULL
# ifdef IMODE_EXTENSIONS
		, use_ua_utn
# endif
	};

	if (m_language.IsEmpty())
	{
		OP_STATUS rc;
		if (OpStatus::IsError(rc = m_language.Set(g_languageManager->GetLanguage().CStr())))
		{
			m_language.Set("und"); // Undetermined, cf. RFC 5646
			if (OpStatus::IsMemoryError(rc))
				g_memory_manager->RaiseCondition(rc);
		}
	}

#if defined SOFTCORE_CUSTOM_UA && defined PREFS_HAVE_CUSTOM_UA
	OpStringC customua(g_pccore->GetStringPref(PrefsCollectionCore::CustomUAString, host));
	if (!customua.IsEmpty())
	{
		uni_cstrlcpy(buffer, customua.CStr(), buf_len);
		return op_strlen(buffer);
	}
#endif

	if (buf_len > 0)
	{
#ifdef COMPONENT_IN_UASTRING_SUPPORT
		// Get the extended comment string
		if (m_comment_string.IsEmpty()
# ifdef IMODE_EXTENSIONS
			|| m_comment_string_utn.IsEmpty()
# endif
		   )
		{
			// First call or components changed - need to build our
			// comments string.
			OP_STATUS rc = AssembleComponents();
			if (OpStatus::IsMemoryError(rc))
			{
				g_memory_manager->RaiseCondition(rc);
			}
		}

		const char *comment_trailer =
# ifdef IMODE_EXTENSIONS
			use_ua_utn ? m_comment_string_utn.CStr() :
# endif
# ifdef SC_UA_COMPONENT_FOR_OPERA_ONLY
			(identify_as == UA_Opera) ? m_comment_string.CStr() : NULL;
# else
			m_comment_string.CStr();
# endif // SC_UA_COMPONEN_FOR_OPERA_ONLY

#elif defined SC_UA_CONTEXT_COMPONENTS
		// Get the context-dependent comment string
		const char *comment_trailer =
			m_component_manager->GetCommentString(ua_args, m_language.CStr());
#else
		// No comment
		const char *comment_trailer = NULL;
#endif

		char* buf_p = buffer;
		int remaining_buf = buf_len;
#if defined TOKEN_IN_UASTRING_SUPPORT || defined SC_UA_CONTEXT_COMPONENTS
		// Add the prefix
# ifdef SC_UA_CONTEXT_COMPONENTS
		// Get the context-dependent prefix string
		const char *prefix = m_component_manager->GetPrefixString(ua_args);
		if (full && prefix && *prefix)
# else
		if (full && !m_prefix.IsEmpty())
# endif
		{
			// There is a prefix, add it and advance the buffer pointer
# ifdef SC_UA_CONTEXT_COMPONENTS
			len = op_snprintf(buf_p, remaining_buf, "%s ", prefix);
# else
			len = op_snprintf(buf_p, remaining_buf, "%s ", m_prefix.CStr());
# endif
			if (len >= remaining_buf)
				return buf_len - 1;
			buf_p += len;
			remaining_buf -= len;
		}
#endif

		const char *osstr = m_component_manager->GetOSString(ua_args);
		len = op_snprintf(buf_p, remaining_buf, GetUserAgent(identify_as, full),
			osstr, comment_trailer ? "; " : "", comment_trailer ? comment_trailer : "");
		if (len >= remaining_buf)
			return buf_len - 1;
		buf_p += len;
		remaining_buf -= len;

#ifdef SOFTCORE_UA_CORE_VERSION
		// Core version is non-NULL, and we're identifying as Opera
		if (full && SOFTCORE_UA_CORE_VERSION && IsOperaUA(identify_as))
		{
			len = op_snprintf(buf_p, remaining_buf, " %s/%s", SOFTCORE_UA_CORE_VERSION, CORE_VERSION_STRING);
			if (len >= remaining_buf)
				return buf_len - 1;
			buf_p += len;
			remaining_buf -= len;
		}
#endif

#ifdef SC_UA_VERSION_TAG
		// Core version is non-NULL, and we're identifying as Opera
		if (full && IsOperaUA(identify_as))
		{
			len = op_snprintf(buf_p, remaining_buf, " Version/%s", VER_NUM_STR);
			if (len >= remaining_buf)
				return buf_len - 1;
			buf_p += len;
			remaining_buf -= len;
		}
#endif // SC_UA_VERSION_TAG

#if defined TOKEN_IN_UASTRING_SUPPORT || defined SC_UA_CONTEXT_COMPONENTS
		// Add the suffix

# ifdef SC_UA_CONTEXT_COMPONENTS
		// Get the context-dependent suffix string
		const char *suffix = m_component_manager->GetSuffixString(ua_args);
		if (full && suffix && *suffix)
# else
		if (full && !m_suffix.IsEmpty())
# endif
		{
# ifdef SC_UA_CONTEXT_COMPONENTS
			op_snprintf(buf_p, remaining_buf, " %s", suffix);
# else
			op_snprintf(buf_p, remaining_buf, " %s", m_suffix.CStr());
# endif
		}
#endif
		len = op_strlen(buffer);
	}

    return len;
}
#endif // SC_UA_EXTERNAL

void UAManager::PrefChanged(OpPrefsCollection::Collections id, int pref, int newvalue)
{
	if (OpPrefsCollection::Network == id &&
	    PrefsCollectionNetwork::UABaseId == pref)
	{
		m_UA_SelectedStringId =
			(newvalue > 0 && (newvalue < UA_MozillaOnly || IsOperaUA(static_cast<enum UA_BaseStringId>(newvalue)))
				? (UA_BaseStringId) newvalue
				: UA_MSIE);

		if (urlManager)
		{
			urlManager->UpdateUAOverrides();
		}
	}
}

#ifdef PREFS_HOSTOVERRIDE
void UAManager::HostOverrideChanged(OpPrefsCollection::Collections /* id */,
									const uni_char *hostname)
{
	ServerName *sn = g_url_api->GetServerName(hostname);
	if (sn)
		sn->UpdateUAOverrides();
}
#endif

#ifdef UA_CAN_CHANGE_LANGUAGE
void UAManager::FileChangedL(PrefsCollectionFiles::filepref pref, const OpFile *newvalue)
{
	if (pref == PrefsCollectionFiles::LanguageFile)
	{
		// Forget the current language. The new language isn't loaded yet,
		// so we postpone reading the language until the next call.
		m_language.Empty();

# ifdef COMPONENT_IN_UASTRING_SUPPORT
		// We need to re-assemble the comment string next time.
		m_comment_string.Empty();
# endif
	}
}
#endif

const char *UAManager::GetUserAgent(UA_BaseStringId identify_as, BOOL full)
{
	switch (identify_as)
	{
	case UA_Opera:
	default:
#ifdef VER_NUM_FORCE_STR
		return "Opera/" VER_NUM_FORCE_STR " (%s%s%s)" + (full ? 0 : 6);
#else
		return "Opera/" VER_NUM_STR " (%s%s%s)" + (full ? 0 : 6);
#endif // VER_NUM_FORCE_STR

	case UA_Mozilla:
		if (full)
			return "Mozilla/" SOFTCORE_UA_MOZILLA_SPOOF_VERSION " (%s%s%s; rv:"
			       SOFTCORE_UA_MOZILLA_REVISION ") Gecko/" SOFTCORE_UA_MOZILLA_GECKO_DATE
				   " Firefox/" SOFTCORE_UA_FIREFOX_VERSION " Opera " VER_NUM_STR;
		else
			return SOFTCORE_UA_MOZILLA_SPOOF_VERSION " (%s%s%s; rv:"
			       SOFTCORE_UA_MOZILLA_REVISION ")";

	case UA_MSIE:
		if (full)
			return "Mozilla/5.0 (compatible; MSIE " SOFTCORE_UA_MSIE_SPOOF_VERSION "; %s%s%s) Opera " VER_NUM_STR;
		else
			return "5.0 (compatible; MSIE " SOFTCORE_UA_MSIE_SPOOF_VERSION "; %s%s%s)";

#ifdef URL_TVSTORE_UA
	case UA_TVStore:
#ifdef VER_NUM_FORCE_STR
		return "Opera/" VER_NUM_FORCE_STR " (%s; Opera TV Store/" VER_BUILD_IDENTIFIER "%s%s)" + (full ? 0 : 6);
#else
		return "Opera/" VER_NUM_STR " (%s; Opera TV Store/" VER_BUILD_IDENTIFIER "%s%s)" + (full ? 0 : 6);
#endif // VER_NUM_FORCE_STR
#endif // URL_TVSTORE_UA

#ifdef IMODE_EXTENSIONS
	case UA_IMODE:
			return "DoCoMo/1.0/Opera" VER_NUM_STR "/c20/TB/W10H10" + (full ? 0 : 8);
#endif

	case UA_MozillaOnly:
		if (full)
			return "Mozilla/" SOFTCORE_UA_MOZILLA_SPOOF_VERSION " (%s%s%s; rv:"
			       SOFTCORE_UA_MOZILLA_REVISION ") Gecko/" SOFTCORE_UA_MOZILLA_GECKO_DATE
				   " Firefox/" SOFTCORE_UA_FIREFOX_VERSION;
		else
			return SOFTCORE_UA_MOZILLA_SPOOF_VERSION " (%s%s%s; rv:"
			       SOFTCORE_UA_MOZILLA_REVISION ")";

	case UA_MSIE_Only:
		return "Mozilla/5.0 (compatible; MSIE " SOFTCORE_UA_MSIE_SPOOF_VERSION "; %s; "
		       "Trident/" SOFTCORE_UA_MSIE_TRIDENT "%s%s)"
		       + (full ? 0 : 8);

	case UA_Mozilla_478:
		if (full)
			return "Mozilla/4.78 (%s%s%s) Opera " VER_NUM_STR;
		else
			return "4.78 (%s%s%s)";
	}
}

#ifdef UA_NEED_GET_VERSION_CODE
/*static*/
OP_STATUS UAManager::PickSpoofVersionString(UA_BaseStringId use_id, int, OpString8 &target)
{
	const char *verstr;

	switch(use_id)
	{
	case UA_MozillaOnly:
	case UA_Mozilla:
		verstr = SOFTCORE_UA_MOZILLA_SPOOF_VERSION; /* "5.0" */
		break;

	case UA_MSIE:
	case UA_MSIE_Only:
		verstr = SOFTCORE_UA_MSIE_SPOOF_VERSION; /* "6.0" */
		break;

	case UA_Opera:
		verstr = VER_NUM_STR;
		break;

	default:
		verstr = "";
	}

	return target.Set(verstr);
}
#endif // UA_NEED_GET_VERSION_CODE

/* static */
UA_BaseStringId UAManager::OverrideUserAgent(const ServerName *host)
{
	(void)host; //Yes compiler, we declare and possibly do not use this variable..

#ifdef PREFS_HOSTOVERRIDE
	if (host && host->UniName() != NULL)
	{
		UA_BaseStringId current_id = g_uaManager->GetUABaseId();

		// Check if User-Agent string is overridden for this host by the
		// preferences.
		HostOverrideStatus is_overridden =
			g_pcnet->IsHostOverridden(host->UniName(), FALSE);
		if (is_overridden == HostOverrideActive
#ifdef PREFSFILE_WRITE_GLOBAL
		    || is_overridden == HostOverrideDownloadedActive
#endif
			)
		{
			UA_BaseStringId overridden_id =
				UA_BaseStringId(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UABaseId, host));
			if (overridden_id != current_id)
				return overridden_id;
		}
	}
#endif

	return UA_NotOverridden;
}

#ifdef COMPONENT_IN_UASTRING_SUPPORT
OP_STATUS UAManager::AddComponent(const char * component_string, BOOL only_utn)
{
#ifndef IMODE_EXTENSIONS
	OP_ASSERT(!only_utn);
	if (only_utn)
	{
		return OpStatus::ERR_NOT_SUPPORTED;
	}
#endif

	// Recompose comment string next time
	m_comment_string.Empty();

	// Add the component if it isn't already there
	OP_STATUS status = OpStatus::OK;
	if (FindComponent(component_string) == -1)
	{
		ua_component * newitem = OP_NEW(ua_component, ());
		if (newitem)
		{
			status = newitem->m_componentstring.Set(component_string);
			if (OpStatus::IsSuccess(status))
			{
#ifdef IMODE_EXTENSIONS
				newitem->m_only_utn = only_utn;
#endif
				status = m_components.Add(newitem);
			}
		}
		else
			status = OpStatus::ERR_NO_MEMORY;
	} // else,.. duplicates ignored
	return status;
}
#endif // COMPONENT_IN_UASTRING_SUPPORT

#ifdef COMPONENT_IN_UASTRING_SUPPORT
void UAManager::RemoveComponent(const char * component_string)
{
	// Recompose comment string next time
	m_comment_string.Empty();

	// Delete the component if we can find it
	int entry = FindComponent(component_string);
	if (entry >= 0)
		m_components.Delete(entry);
}
#endif // COMPONENT_IN_UASTRING_SUPPORT

#ifdef COMPONENT_IN_UASTRING_SUPPORT
/**
 * Find a component in the comment string.
 *
 * @param component_string String to locat
 * @return Index, or -1 if not found
 */
int UAManager::FindComponent(const char * component_string)
{
	int entry = -1;
	int maxcomponents = m_components.GetCount();
	for (int i = 0; i < maxcomponents; i ++)
	{
		if (op_strcmp(m_components.Get(i)->m_componentstring.CStr(), component_string) == 0)
		{
			entry = i;
			break;
		}
	}
	return entry;
}
#endif // COMPONENT_IN_UASTRING_SUPPORT

#ifdef COMPONENT_IN_UASTRING_SUPPORT
/**
 * Assemble the components for the comment string. The last component
 * is always the language file, so given that we have enough memory
 * to perform our work, m_comment_string should never be empty after
 * this function is called.
 *
 * @return Status of the operation
 */
OP_STATUS UAManager::AssembleComponents()
{
	m_comment_string.Empty();
	BOOL first = TRUE;
#ifdef IMODE_EXTENSIONS
	m_comment_string_utn.Empty();
	BOOL imodeFirst = TRUE;
#endif

	if (g_pccore->GetIntegerPref(PrefsCollectionCore::AllowComponentsInUAStringComment))
	{
		int maxcomponents = m_components.GetCount();

		for (int i = 0; i < maxcomponents; i ++)
		{
			ua_component *p = m_components.Get(i);
#ifdef IMODE_EXTENSIONS
			if (!imodeFirst)
				RETURN_IF_ERROR(m_comment_string_utn.Append("; "));
			imodeFirst = FALSE;
			RETURN_IF_ERROR(m_comment_string_utn.Append(p->m_componentstring));

			if (!p->m_only_utn)
#endif
			{
				if (!first)
					RETURN_IF_ERROR(m_comment_string.Append("; "));
				first = FALSE;
				RETURN_IF_ERROR(m_comment_string.Append(p->m_componentstring));
			}
		}
	}

	// Add the ISP id (dgisp) at the end of the component list, if one
	// is listed in the ini file.
	OpStringC isp_id =
		g_pcnet->GetStringPref(PrefsCollectionNetwork::IspUserAgentId);
	if (isp_id.Length())
	{
		OpString8 isp_id8;
		RETURN_IF_ERROR(isp_id8.SetUTF8FromUTF16(isp_id.CStr()));
		if (!first)
			RETURN_IF_ERROR(m_comment_string.Append("; "));
		RETURN_IF_ERROR(m_comment_string.Append(isp_id8));
#ifdef IMODE_EXTENSIONS
		if (!imodeFirst)
			RETURN_IF_ERROR(m_comment_string_utn.Append("; "));
		RETURN_IF_ERROR(m_comment_string_utn.Append(isp_id8));
#endif
	}

	return OpStatus::OK;
}
#endif // COMPONENT_IN_UASTRING_SUPPORT
